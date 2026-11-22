#include "tabletfx/effects/mono_guitar_synth.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx::effects {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTau = 2.0F * kPi;
constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kMinimumTrackedHz = 55.0F;
constexpr float kMaximumTrackedHz = 1100.0F;
constexpr float kInputFollowerAttackMs = 1.0F;
constexpr float kInputFollowerReleaseMs = 25.0F;
constexpr float kMaximumOutputMagnitude = 1000000.0F;

}  // namespace

MonoGuitarSynth::MonoGuitarSynth(float sample_rate,
                                 MonoGuitarSynthConfig config)
    : sample_rate_(bounded(sample_rate, kDefaultSampleRate,
                           kMinimumSampleRate, kMaximumSampleRate)),
      tracker_(sample_rate_) {
    DiatonicHarmonizerConfig tracker_config;
    tracker_config.mix = 0.0F;
    tracker_config.pitch_interval = 0;
    tracker_config.delay_ms = 0.0F;
    tracker_config.feedback = 0.0F;
    tracker_config.spillover = false;
    tracker_.configure(tracker_config);
    // Detection continues while disabled; discarding the tracker's audio output
    // avoids feeding its shifted signal back into this effect's dry/synth mix.
    tracker_.set_enabled(false);

    input_attack_coefficient_ = time_coefficient(kInputFollowerAttackMs);
    input_release_coefficient_ = time_coefficient(kInputFollowerReleaseMs);
    configure(config);
}

std::size_t MonoGuitarSynth::memory_bytes() const noexcept {
    return sizeof(*this) + tracker_.memory_bytes() - sizeof(tracker_);
}

float MonoGuitarSynth::bounded(float value, float fallback,
                               float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

bool MonoGuitarSynth::valid_waveform(
    MonoGuitarSynthWaveform waveform) noexcept {
    return static_cast<unsigned>(waveform) <=
           static_cast<unsigned>(MonoGuitarSynthWaveform::Square);
}

float MonoGuitarSynth::time_coefficient(float milliseconds) const noexcept {
    if (!(milliseconds > 0.0F)) {
        return 1.0F;
    }
    const float exponent = -1000.0F / (milliseconds * sample_rate_);
    return std::clamp(1.0F - std::exp(exponent), 0.0F, 1.0F);
}

void MonoGuitarSynth::configure(
    const MonoGuitarSynthConfig& requested) noexcept {
    config_ = requested;
    if (!valid_waveform(config_.waveform)) {
        config_.waveform = MonoGuitarSynthWaveform::Sawtooth;
    }
    config_.mix = bounded(config_.mix, 0.75F, 0.0F, 1.0F);
    config_.level = bounded(config_.level, 0.6F, 0.0F, 2.0F);
    config_.gate_threshold_db = bounded(
        config_.gate_threshold_db, -54.0F, -90.0F, 0.0F);
    config_.attack_ms = bounded(config_.attack_ms, 8.0F, 0.0F, 2000.0F);
    config_.release_ms = bounded(config_.release_ms, 180.0F, 0.0F, 5000.0F);
    config_.portamento_ms = bounded(
        config_.portamento_ms, 8.0F, 0.0F, 250.0F);

    gate_open_threshold_ = std::pow(
        10.0F, config_.gate_threshold_db / 20.0F);
    gate_close_threshold_ = gate_open_threshold_ * 0.5F;
    synth_attack_coefficient_ = time_coefficient(config_.attack_ms);
    synth_release_coefficient_ = time_coefficient(config_.release_ms);
    portamento_coefficient_ = time_coefficient(config_.portamento_ms);
}

float MonoGuitarSynth::pitch_response_latency_ms() const noexcept {
    return static_cast<float>(pitch_response_latency_samples()) *
           1000.0F / sample_rate_;
}

void MonoGuitarSynth::reset() noexcept {
    tracker_.reset();
    input_level_ = 0.0F;
    synth_envelope_ = 0.0F;
    current_frequency_hz_ = 0.0F;
    phase_ = 0.0;
    gate_open_ = false;
}

float MonoGuitarSynth::poly_blep(float phase,
                                float phase_increment) noexcept {
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

float MonoGuitarSynth::oscillator_sample() noexcept {
    if (!(current_frequency_hz_ > 0.0F) ||
        !std::isfinite(current_frequency_hz_)) {
        return 0.0F;
    }

    const float phase = static_cast<float>(phase_);
    const float increment = std::clamp(
        current_frequency_hz_ / sample_rate_, 0.0F, 0.49F);
    float sample = 0.0F;
    switch (config_.waveform) {
        case MonoGuitarSynthWaveform::Sine:
            sample = std::sin(kTau * phase);
            break;
        case MonoGuitarSynthWaveform::Triangle:
            sample = 1.0F - 4.0F * std::abs(phase - 0.5F);
            break;
        case MonoGuitarSynthWaveform::Sawtooth:
            sample = 2.0F * phase - 1.0F - poly_blep(phase, increment);
            break;
        case MonoGuitarSynthWaveform::Square: {
            float shifted_phase = phase + 0.5F;
            if (shifted_phase >= 1.0F) {
                shifted_phase -= 1.0F;
            }
            sample = (phase < 0.5F ? 1.0F : -1.0F) +
                     poly_blep(phase, increment) -
                     poly_blep(shifted_phase, increment);
            break;
        }
    }

    phase_ += static_cast<double>(increment);
    phase_ -= std::floor(phase_);
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return std::clamp(sample, -1.0F, 1.0F);
}

float MonoGuitarSynth::finite_output(double sample) noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return static_cast<float>(std::clamp(
        sample, -static_cast<double>(kMaximumOutputMagnitude),
        static_cast<double>(kMaximumOutputMagnitude)));
}

void MonoGuitarSynth::process(std::span<float> interleaved,
                              std::uint16_t channels) noexcept {
    if (channels == 0U || channels > 2U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        const float input_left = std::isfinite(interleaved[offset])
                                     ? interleaved[offset]
                                     : 0.0F;
        const float input_right = channels == 2U &&
                                          std::isfinite(interleaved[offset + 1U])
                                      ? interleaved[offset + 1U]
                                      : 0.0F;

        // A linked peak follower avoids cancellation when stereo pickups or
        // effects produce opposite-polarity channels.
        const float input_magnitude = channels == 1U
            ? std::abs(input_left)
            : std::max(std::abs(input_left), std::abs(input_right));
        const float level_coefficient = input_magnitude > input_level_
            ? input_attack_coefficient_
            : input_release_coefficient_;
        input_level_ += level_coefficient * (input_magnitude - input_level_);
        if (!std::isfinite(input_level_)) {
            input_level_ = 0.0F;
        }
        input_level_ = std::clamp(input_level_, 0.0F, 1.0e6F);

        // Feed the original guitar sample to the monophonic detector. Its
        // processed audio is intentionally discarded; only its raw frequency
        // estimate controls this oscillator. One-frame calls keep estimator
        // timing independent of the host's arbitrary block partitioning.
        float discarded_left = 0.0F;
        float discarded_right = 0.0F;
        const float* tracker_input_right = channels == 2U
            ? &input_right
            : nullptr;
        float* tracker_output_right = channels == 2U
            ? &discarded_right
            : nullptr;
        tracker_.process(&input_left, tracker_input_right,
                         &discarded_left, tracker_output_right, 1U);

        float detected_hz = tracker_.detected_input_frequency_hz();
        if (!std::isfinite(detected_hz) || detected_hz < 0.0F) {
            detected_hz = 0.0F;
        }
        const bool has_pitch = detected_hz > 0.0F;
        if (has_pitch) {
            detected_hz = std::clamp(
                detected_hz, kMinimumTrackedHz, kMaximumTrackedHz);
            if (!(current_frequency_hz_ > 0.0F)) {
                current_frequency_hz_ = detected_hz;
            } else {
                current_frequency_hz_ += portamento_coefficient_ *
                    (detected_hz - current_frequency_hz_);
            }
            if (!std::isfinite(current_frequency_hz_)) {
                current_frequency_hz_ = detected_hz;
            }
        }

        if (gate_open_) {
            if (!has_pitch || input_level_ <= gate_close_threshold_) {
                gate_open_ = false;
            }
        } else if (has_pitch && input_level_ >= gate_open_threshold_) {
            gate_open_ = true;
        }

        const float envelope_target = gate_open_ && has_pitch ? 1.0F : 0.0F;
        const float envelope_coefficient = envelope_target > synth_envelope_
            ? synth_attack_coefficient_
            : synth_release_coefficient_;
        synth_envelope_ += envelope_coefficient *
                           (envelope_target - synth_envelope_);
        if (!std::isfinite(synth_envelope_)) {
            synth_envelope_ = 0.0F;
        }
        synth_envelope_ = std::clamp(synth_envelope_, 0.0F, 1.0F);

        const float oscillator = oscillator_sample();
        const double synth = static_cast<double>(oscillator) *
                             synth_envelope_ * config_.level * config_.mix;
        const double dry_gain = 1.0 - static_cast<double>(config_.mix);
        interleaved[offset] = finite_output(
            static_cast<double>(input_left) * dry_gain + synth);
        if (channels == 2U) {
            interleaved[offset + 1U] = finite_output(
                static_cast<double>(input_right) * dry_gain + synth);
        }
    }
}

}  // namespace tabletfx::effects
