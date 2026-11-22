#include "tabletfx/effects/fractal_three_voice_synth.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx::effects {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTau = 2.0F * kPi;
constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kEnvelopeReleaseMs = 80.0F;
constexpr float kPitchSmoothingMs = 4.0F;
constexpr float kMaximumInputMagnitude = 1000000.0F;

}  // namespace

FractalThreeVoiceSynth::FractalThreeVoiceSynth(
    const float sample_rate,
    const FractalThreeVoiceSynthConfig config,
    const std::uint32_t random_seed)
    : sample_rate_(bounded(sample_rate, kDefaultSampleRate,
                           kMinimumSampleRate, kMaximumSampleRate)),
      tracker_(sample_rate_),
      seed_(random_seed == 0U ? 0x6d2b79f5U : random_seed) {
    DiatonicHarmonizerConfig tracker_config;
    tracker_config.mix = 0.0F;
    tracker_config.pitch_interval = 0;
    tracker_config.delay_ms = 0.0F;
    tracker_config.feedback = 0.0F;
    tracker_config.spillover = false;
    tracker_.configure(tracker_config);
    // The tracker remains active for its raw monophonic pitch estimate, but its
    // harmonized output is discarded. This avoids a second audible pitch path.
    tracker_.set_enabled(false);

    for (std::size_t voice = 0U; voice < kVoiceCount; ++voice) {
        state_[voice].random_state = seed_ ^
            (0x9e3779b9U * static_cast<std::uint32_t>(voice + 1U));
        if (state_[voice].random_state == 0U) {
            state_[voice].random_state =
                0x6d2b79f5U + static_cast<std::uint32_t>(voice);
        }
    }
    configure(config);
}

std::size_t FractalThreeVoiceSynth::memory_bytes() const noexcept {
    return sizeof(*this) + tracker_.memory_bytes() - sizeof(tracker_);
}

float FractalThreeVoiceSynth::bounded(const float value, const float fallback,
                                      const float minimum,
                                      const float maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

bool FractalThreeVoiceSynth::valid_waveform(
    const FractalSynthWaveform waveform) noexcept {
    return static_cast<unsigned>(waveform) <=
           static_cast<unsigned>(FractalSynthWaveform::Off);
}

bool FractalThreeVoiceSynth::valid_tracking(
    const FractalSynthTracking tracking) noexcept {
    return static_cast<unsigned>(tracking) <=
           static_cast<unsigned>(FractalSynthTracking::Quantize);
}

float FractalThreeVoiceSynth::time_coefficient(
    const float milliseconds) const noexcept {
    if (!(milliseconds > 0.0F)) {
        return 1.0F;
    }
    const float exponent = -1000.0F / (milliseconds * sample_rate_);
    return std::clamp(1.0F - std::exp(exponent), 0.0F, 1.0F);
}

void FractalThreeVoiceSynth::configure(
    const FractalThreeVoiceSynthConfig& requested) noexcept {
    config_ = requested;
    for (std::size_t voice = 0U; voice < kVoiceCount; ++voice) {
        auto& parameters = config_.voices[voice];
        if (!valid_waveform(parameters.waveform)) {
            parameters.waveform = FractalSynthWaveform::Off;
        }
        if (!valid_tracking(parameters.tracking)) {
            parameters.tracking = FractalSynthTracking::Off;
        }
        parameters.frequency_hz = bounded(
            parameters.frequency_hz, 440.0F, 20.0F, 2000.0F);
        parameters.shift_semitones =
            std::clamp(parameters.shift_semitones, -24, 24);
        parameters.tune_cents = bounded(
            parameters.tune_cents, 0.0F, -50.0F, 50.0F);
        parameters.duty_cycle = bounded(
            parameters.duty_cycle, 0.5F, 0.05F, 0.95F);
        parameters.pan = bounded(parameters.pan, 0.0F, -1.0F, 1.0F);
        const float maximum_filter_hz =
            std::min(20000.0F, sample_rate_ * 0.45F);
        parameters.filter_frequency_hz = bounded(
            parameters.filter_frequency_hz, 12000.0F, 20.0F,
            maximum_filter_hz);
        parameters.filter_q = bounded(
            parameters.filter_q, 0.707F, 0.25F, 12.0F);
        parameters.attack_ms = bounded(
            parameters.attack_ms, 5.0F, 0.0F, 2000.0F);
        parameters.level = bounded(parameters.level, 0.25F, 0.0F, 1.0F);

        auto& state = state_[voice];
        state.attack_coefficient = time_coefficient(parameters.attack_ms);
        state.release_coefficient = time_coefficient(kEnvelopeReleaseMs);
        state.pitch_smoothing_coefficient = time_coefficient(kPitchSmoothingMs);
        const float angle = (parameters.pan + 1.0F) * kPi * 0.25F;
        state.pan_left = std::cos(angle);
        state.pan_right = std::sin(angle);
        configure_filter(voice);
        if (parameters.tracking == FractalSynthTracking::Off ||
            parameters.tracking == FractalSynthTracking::EnvelopeOnly) {
            const float shift_ratio = std::exp2(
                (static_cast<float>(parameters.shift_semitones) +
                 parameters.tune_cents * 0.01F) / 12.0F);
            state.frequency_hz = std::clamp(
                parameters.frequency_hz * shift_ratio, 10.0F,
                std::min(20000.0F, sample_rate_ * 0.45F));
        }
    }
}

void FractalThreeVoiceSynth::configure_filter(
    const std::size_t voice) noexcept {
    const auto& parameters = config_.voices[voice];
    auto& state = state_[voice];
    const double omega = static_cast<double>(kTau) *
        static_cast<double>(parameters.filter_frequency_hz) /
        static_cast<double>(sample_rate_);
    const double cosine = std::cos(omega);
    const double sine = std::sin(omega);
    const double alpha = sine /
        (2.0 * static_cast<double>(parameters.filter_q));
    const double a0 = 1.0 + alpha;
    if (!(a0 > 0.0) || !std::isfinite(a0)) {
        state.b0 = 1.0F;
        state.b1 = 0.0F;
        state.b2 = 0.0F;
        state.a1 = 0.0F;
        state.a2 = 0.0F;
        return;
    }
    state.b0 = static_cast<float>((1.0 - cosine) * 0.5 / a0);
    state.b1 = static_cast<float>((1.0 - cosine) / a0);
    state.b2 = state.b0;
    state.a1 = static_cast<float>((-2.0 * cosine) / a0);
    state.a2 = static_cast<float>((1.0 - alpha) / a0);
}

float FractalThreeVoiceSynth::random_bipolar(VoiceState& state) noexcept {
    std::uint32_t x = state.random_state;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    state.random_state = x == 0U ? 0x6d2b79f5U : x;
    return static_cast<float>(state.random_state >> 8U) *
               (2.0F / 16777215.0F) - 1.0F;
}

float FractalThreeVoiceSynth::poly_blep(
    const float phase, const float phase_increment) noexcept {
    if (!(phase_increment > 0.0F) || phase_increment >= 0.5F) {
        return 0.0F;
    }
    if (phase < phase_increment) {
        const float t = phase / phase_increment;
        return t + t - t * t - 1.0F;
    }
    if (phase > 1.0F - phase_increment) {
        const float t = (phase - 1.0F) / phase_increment;
        return t * t + t + t + 1.0F;
    }
    return 0.0F;
}

float FractalThreeVoiceSynth::oscillator_sample(
    const std::size_t voice) noexcept {
    const auto& parameters = config_.voices[voice];
    auto& state = state_[voice];
    if (parameters.waveform == FractalSynthWaveform::Off ||
        !(state.frequency_hz > 0.0F) ||
        !std::isfinite(state.frequency_hz)) {
        return 0.0F;
    }

    const float phase = static_cast<float>(state.phase);
    const float increment = std::clamp(
        state.frequency_hz / sample_rate_, 0.0F, 0.45F);
    float sample = 0.0F;
    switch (parameters.waveform) {
        case FractalSynthWaveform::Sine:
            sample = std::sin(kTau * phase);
            break;
        case FractalSynthWaveform::Triangle:
            sample = phase < parameters.duty_cycle
                ? -1.0F + 2.0F * phase / parameters.duty_cycle
                : 1.0F - 2.0F * (phase - parameters.duty_cycle) /
                    (1.0F - parameters.duty_cycle);
            break;
        case FractalSynthWaveform::Square: {
            const float edge = phase >= parameters.duty_cycle
                ? phase - parameters.duty_cycle
                : phase + (1.0F - parameters.duty_cycle);
            sample = (phase < parameters.duty_cycle ? 1.0F : -1.0F) +
                poly_blep(phase, increment) - poly_blep(edge, increment);
            break;
        }
        case FractalSynthWaveform::Sawtooth:
            sample = 2.0F * phase - 1.0F - poly_blep(phase, increment);
            break;
        case FractalSynthWaveform::Random:
            sample = state.random_value;
            break;
        case FractalSynthWaveform::WhiteNoise:
            sample = random_bipolar(state);
            break;
        case FractalSynthWaveform::PinkNoise: {
            const float white = random_bipolar(state);
            auto& b = state.pink_state;
            b[0] = 0.99886F * b[0] + white * 0.0555179F;
            b[1] = 0.99332F * b[1] + white * 0.0750759F;
            b[2] = 0.96900F * b[2] + white * 0.1538520F;
            b[3] = 0.86650F * b[3] + white * 0.3104856F;
            b[4] = 0.55000F * b[4] + white * 0.5329522F;
            b[5] = -0.7616F * b[5] - white * 0.0168980F;
            sample = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] +
                      b[6] + white * 0.5362F) * 0.11F;
            b[6] = white * 0.115926F;
            break;
        }
        case FractalSynthWaveform::Off:
            return 0.0F;
    }

    state.phase += static_cast<double>(increment);
    if (state.phase >= 1.0) {
        state.phase -= std::floor(state.phase);
        if (parameters.waveform == FractalSynthWaveform::Random) {
            state.random_value = random_bipolar(state);
        }
    }
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return std::clamp(sample, -1.0F, 1.0F);
}

float FractalThreeVoiceSynth::filter_sample(
    const std::size_t voice, const float sample) noexcept {
    auto& state = state_[voice];
    const float output = state.b0 * sample + state.filter_state_1;
    state.filter_state_1 = state.b1 * sample - state.a1 * output +
                            state.filter_state_2;
    state.filter_state_2 = state.b2 * sample - state.a2 * output;
    if (!std::isfinite(output) || !std::isfinite(state.filter_state_1) ||
        !std::isfinite(state.filter_state_2)) {
        state.filter_state_1 = 0.0F;
        state.filter_state_2 = 0.0F;
        return 0.0F;
    }
    state.filter_state_1 = std::clamp(
        state.filter_state_1, -kMaximumInputMagnitude, kMaximumInputMagnitude);
    state.filter_state_2 = std::clamp(
        state.filter_state_2, -kMaximumInputMagnitude, kMaximumInputMagnitude);
    return std::clamp(output, -kMaximumInputMagnitude, kMaximumInputMagnitude);
}

float FractalThreeVoiceSynth::finite_output(const double sample) const noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return static_cast<float>(std::clamp(
        sample, -static_cast<double>(kMaximumOutputMagnitude),
        static_cast<double>(kMaximumOutputMagnitude)));
}

void FractalThreeVoiceSynth::update_frequency(
    const std::size_t voice, const float detected_frequency_hz) noexcept {
    const auto& parameters = config_.voices[voice];
    auto& state = state_[voice];
    float base_frequency = parameters.frequency_hz;
    if (parameters.tracking == FractalSynthTracking::PitchAndEnvelope ||
        parameters.tracking == FractalSynthTracking::Quantize) {
        if (!(detected_frequency_hz >= kMinimumTrackedHz &&
              detected_frequency_hz <= kMaximumTrackedHz)) {
            return;
        }
        base_frequency = detected_frequency_hz;
        if (parameters.tracking == FractalSynthTracking::Quantize) {
            const float midi = 69.0F +
                12.0F * std::log2(base_frequency / 440.0F);
            const float quantized_note = std::round(midi);
            base_frequency = 440.0F * std::exp2((quantized_note - 69.0F) / 12.0F);
        }
    }

    const float shift_ratio = std::exp2(
        (static_cast<float>(parameters.shift_semitones) +
         parameters.tune_cents * 0.01F) / 12.0F);
    const float maximum_frequency = std::min(20000.0F, sample_rate_ * 0.45F);
    const float target = std::clamp(
        base_frequency * shift_ratio, 10.0F, maximum_frequency);
    if (!(state.frequency_hz > 0.0F) ||
        parameters.tracking == FractalSynthTracking::Off ||
        parameters.tracking == FractalSynthTracking::EnvelopeOnly) {
        state.frequency_hz = target;
    } else {
        state.frequency_hz += state.pitch_smoothing_coefficient *
                              (target - state.frequency_hz);
    }
    if (!std::isfinite(state.frequency_hz)) {
        state.frequency_hz = target;
    }
}

float FractalThreeVoiceSynth::voice_frequency_hz(
    const std::size_t voice) const noexcept {
    return voice < kVoiceCount ? state_[voice].frequency_hz : 0.0F;
}

float FractalThreeVoiceSynth::voice_envelope(
    const std::size_t voice) const noexcept {
    return voice < kVoiceCount ? state_[voice].envelope : 0.0F;
}

void FractalThreeVoiceSynth::reset() noexcept {
    tracker_.reset();
    for (std::size_t voice = 0U; voice < kVoiceCount; ++voice) {
        auto& state = state_[voice];
        state.phase = 0.0;
        state.envelope = 0.0F;
        state.filter_state_1 = 0.0F;
        state.filter_state_2 = 0.0F;
        state.random_state = seed_ ^
            (0x9e3779b9U * static_cast<std::uint32_t>(voice + 1U));
        if (state.random_state == 0U) {
            state.random_state = 0x6d2b79f5U +
                                 static_cast<std::uint32_t>(voice);
        }
        state.random_value = 0.0F;
        state.pink_state.fill(0.0F);
        update_frequency(voice, 0.0F);
    }
}

void FractalThreeVoiceSynth::process(
    std::span<float> interleaved, const std::uint16_t channels) noexcept {
    if (channels != 1U && channels != 2U) {
        return;
    }
    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        const float input_left = std::isfinite(interleaved[offset])
            ? std::clamp(interleaved[offset], -kMaximumInputMagnitude,
                         kMaximumInputMagnitude)
            : 0.0F;
        const float input_right = channels == 2U &&
                std::isfinite(interleaved[offset + 1U])
            ? std::clamp(interleaved[offset + 1U], -kMaximumInputMagnitude,
                         kMaximumInputMagnitude)
            : 0.0F;

        float discarded_left = 0.0F;
        float discarded_right = 0.0F;
        const float* tracker_right_input = channels == 2U
            ? &input_right
            : nullptr;
        float* tracker_right_output = channels == 2U
            ? &discarded_right
            : nullptr;
        tracker_.process(&input_left, tracker_right_input,
                         &discarded_left, tracker_right_output, 1U);
        float detected = tracker_.detected_input_frequency_hz();
        if (!std::isfinite(detected)) {
            detected = 0.0F;
        }

        const float input_magnitude = channels == 1U
            ? std::abs(input_left)
            : std::max(std::abs(input_left), std::abs(input_right));
        float wet_left = 0.0F;
        float wet_right = 0.0F;
        for (std::size_t voice = 0U; voice < kVoiceCount; ++voice) {
            const auto& parameters = config_.voices[voice];
            auto& state = state_[voice];
            update_frequency(voice, detected);

            float envelope_target = 0.0F;
            switch (parameters.tracking) {
                case FractalSynthTracking::Off:
                    envelope_target = 1.0F;
                    break;
                case FractalSynthTracking::EnvelopeOnly:
                    envelope_target = std::clamp(input_magnitude, 0.0F, 1.0F);
                    break;
                case FractalSynthTracking::PitchAndEnvelope:
                case FractalSynthTracking::Quantize:
                    envelope_target = detected > 0.0F
                        ? std::clamp(input_magnitude, 0.0F, 1.0F)
                        : 0.0F;
                    break;
            }
            const float envelope_coefficient = envelope_target > state.envelope
                ? state.attack_coefficient
                : state.release_coefficient;
            state.envelope += envelope_coefficient *
                              (envelope_target - state.envelope);
            if (!std::isfinite(state.envelope)) {
                state.envelope = 0.0F;
            }
            state.envelope = std::clamp(state.envelope, 0.0F, 1.0F);

            const float amplitude = parameters.tracking == FractalSynthTracking::Off
                ? parameters.level
                : parameters.level * state.envelope;
            const float oscillator = filter_sample(
                voice, oscillator_sample(voice));
            const float sample = oscillator * amplitude;
            wet_left += sample * state.pan_left;
            wet_right += sample * state.pan_right;
        }

        if (channels == 1U) {
            interleaved[offset] = finite_output(
                static_cast<double>(input_left) +
                static_cast<double>(wet_left + wet_right) * 0.70710678);
        } else {
            interleaved[offset] = finite_output(
                static_cast<double>(input_left) + wet_left);
            interleaved[offset + 1U] = finite_output(
                static_cast<double>(input_right) + wet_right);
        }
    }
}

}  // namespace tabletfx::effects
