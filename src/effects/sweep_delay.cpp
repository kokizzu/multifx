#include "tabletfx/effects.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kTwoPi = 6.2831853071795864769F;
constexpr float kMaximumFloat = static_cast<float>(
    static_cast<double>(std::numeric_limits<float>::max()));
constexpr float kParameterRampMs = 10.0F;

float checked_sample_rate(float sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate < 8000.0F ||
        sample_rate > 384000.0F) {
        throw std::invalid_argument(
            "Sweep Delay requires a sample rate between 8 kHz and 384 kHz");
    }
    return sample_rate;
}

std::uint16_t checked_channels(std::uint16_t channels) {
    if (channels == 0 || channels > 2) {
        throw std::invalid_argument("Sweep Delay supports mono or stereo audio");
    }
    return channels;
}

float checked_max_delay(float max_delay_ms) {
    if (!std::isfinite(max_delay_ms) || max_delay_ms < 1.0F ||
        max_delay_ms > 6000.0F) {
        throw std::invalid_argument(
            "Sweep Delay maximum time must be between 1 and 6000 ms");
    }
    return max_delay_ms;
}

float bounded(float value, float fallback, float minimum,
              float maximum) noexcept {
    return std::isfinite(value)
        ? std::clamp(value, minimum, maximum)
        : std::clamp(fallback, minimum, maximum);
}

float bounded_sample(double value) noexcept {
    return std::isfinite(value)
        ? static_cast<float>(std::clamp(value,
              -static_cast<double>(kMaximumFloat),
              static_cast<double>(kMaximumFloat)))
        : 0.0F;
}

float beats_for_division(DelayDivision division) noexcept {
    switch (division) {
    case DelayDivision::whole: return 4.0F;
    case DelayDivision::dotted_half: return 3.0F;
    case DelayDivision::half: return 2.0F;
    case DelayDivision::dotted_quarter: return 1.5F;
    case DelayDivision::quarter: return 1.0F;
    case DelayDivision::quarter_triplet: return 2.0F / 3.0F;
    case DelayDivision::dotted_eighth: return 0.75F;
    case DelayDivision::eighth: return 0.5F;
    case DelayDivision::eighth_triplet: return 1.0F / 3.0F;
    case DelayDivision::dotted_sixteenth: return 0.375F;
    case DelayDivision::sixteenth: return 0.25F;
    case DelayDivision::sixteenth_triplet: return 1.0F / 6.0F;
    case DelayDivision::thirty_second: return 0.125F;
    case DelayDivision::sixty_fourth: return 0.0625F;
    }
    return 1.0F;
}

bool valid_division(DelayDivision division) noexcept {
    switch (division) {
    case DelayDivision::whole:
    case DelayDivision::dotted_half:
    case DelayDivision::half:
    case DelayDivision::dotted_quarter:
    case DelayDivision::quarter:
    case DelayDivision::quarter_triplet:
    case DelayDivision::dotted_eighth:
    case DelayDivision::eighth:
    case DelayDivision::eighth_triplet:
    case DelayDivision::dotted_sixteenth:
    case DelayDivision::sixteenth:
    case DelayDivision::sixteenth_triplet:
    case DelayDivision::thirty_second:
    case DelayDivision::sixty_fourth:
        return true;
    }
    return false;
}

} // namespace

SweepDelay::SweepDelay(float sample_rate, std::uint16_t channels,
                       float max_delay_ms, SweepDelayParameters parameters)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)),
      max_delay_ms_(checked_max_delay(max_delay_ms)),
      delay_(sample_rate_, channels_, max_delay_ms_, 350.0F, 0.25F, 1.0F) {
    const double ramp_frames = std::ceil(
        static_cast<double>(sample_rate_) * kParameterRampMs / 1000.0);
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(ramp_frames));
    configure(parameters);
    reset();
}

std::size_t SweepDelay::memory_bytes() const noexcept {
    return sizeof(*this) + delay_.memory_bytes() - sizeof(Delay);
}

float SweepDelay::rate_hz_for(
    const SweepDelayParameters& parameters) noexcept {
    if (parameters.sweep_rate_mode == DelayTimeMode::milliseconds) {
        return parameters.rate_hz;
    }
    return parameters.sweep_tempo_bpm /
           (60.0F * beats_for_division(parameters.sweep_tempo_division));
}

void SweepDelay::configure(SweepDelayParameters parameters) noexcept {
    const float maximum_time = std::min(2000.0F, max_delay_ms_);
    const float maximum_cutoff = std::min(18000.0F, sample_rate_ * 0.45F);
    parameters.time_ms = bounded(parameters.time_ms, 350.0F,
                                 1.0F, maximum_time);
    parameters.feedback = bounded(parameters.feedback, 0.25F,
                                  0.0F, 0.95F);
    parameters.mix = bounded(parameters.mix, 0.25F, 0.0F, 1.0F);
    parameters.start_hz = bounded(parameters.start_hz, 250.0F,
                                  20.0F, maximum_cutoff);
    parameters.stop_hz = bounded(parameters.stop_hz, 6000.0F,
                                 20.0F, maximum_cutoff);
    parameters.resonance_q = bounded(parameters.resonance_q, 1.2F,
                                      0.5F, 12.0F);
    parameters.rate_hz = bounded(parameters.rate_hz, 0.5F,
                                 0.05F, 10.0F);
    parameters.sweep_tempo_bpm = bounded(parameters.sweep_tempo_bpm,
                                         120.0F, 20.0F, 300.0F);
    parameters.phase_degrees = bounded(parameters.phase_degrees, 90.0F,
                                        0.0F, 360.0F);
    switch (parameters.waveform) {
    case SweepDelayWaveform::sine:
    case SweepDelayWaveform::triangle:
    case SweepDelayWaveform::square:
        break;
    default:
        parameters.waveform = SweepDelayWaveform::sine;
        break;
    }
    switch (parameters.filter_type) {
    case SweepDelayFilterType::lowpass:
    case SweepDelayFilterType::bandpass:
    case SweepDelayFilterType::highpass:
        break;
    default:
        parameters.filter_type = SweepDelayFilterType::bandpass;
        break;
    }
    switch (parameters.sweep_rate_mode) {
    case DelayTimeMode::milliseconds:
    case DelayTimeMode::tempo_sync:
        break;
    default:
        parameters.sweep_rate_mode = DelayTimeMode::milliseconds;
        break;
    }
    if (!valid_division(parameters.sweep_tempo_division)) {
        parameters.sweep_tempo_division = DelayDivision::quarter;
    }

    if (parameters.filter_type != parameters_.filter_type) {
        filter_state_ = {};
    }
    parameters_ = parameters;
    delay_.configure(parameters_.time_ms, parameters_.feedback, 1.0F);
    retarget_ramps();
}

void SweepDelay::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    std::array<float, 2> dry{};
    std::array<float, 2> wet{};
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const float input = interleaved[offset + channel];
            dry[channel] = std::isfinite(input) ? input : 0.0F;
            wet[channel] = dry[channel];
        }

        delay_.process(
            std::span<float>(wet.data(), static_cast<std::size_t>(channels_)),
            channels_);

        const float stereo_phase = current_phase_degrees_ / 360.0F;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            float filter_phase = phase_;
            if (channel == 1) {
                filter_phase += stereo_phase;
                filter_phase -= std::floor(filter_phase);
            }
            const float sweep = lfo_value(filter_phase);
            const float cutoff = current_start_hz_ +
                (current_stop_hz_ - current_start_hz_) * sweep;
            const float filtered = filter_wet_sample(
                wet[channel], channel, cutoff, current_resonance_q_,
                parameters_.filter_type);
            const double output =
                static_cast<double>(dry[channel]) *
                    static_cast<double>(1.0F - current_mix_) +
                static_cast<double>(filtered) *
                    static_cast<double>(current_mix_);
            interleaved[offset + channel] = bounded_sample(output);
        }

        phase_ += current_rate_hz_ / sample_rate_;
        phase_ -= std::floor(phase_);
        advance_ramps();
    }
}

void SweepDelay::reset() noexcept {
    delay_.reset();
    filter_state_ = {};
    phase_ = 0.0F;
    current_mix_ = parameters_.mix;
    mix_step_ = 0.0F;
    current_start_hz_ = parameters_.start_hz;
    start_step_ = 0.0F;
    current_stop_hz_ = parameters_.stop_hz;
    stop_step_ = 0.0F;
    current_resonance_q_ = parameters_.resonance_q;
    resonance_step_ = 0.0F;
    current_rate_hz_ = rate_hz_for(parameters_);
    rate_step_ = 0.0F;
    current_phase_degrees_ = parameters_.phase_degrees;
    phase_step_ = 0.0F;
    ramp_remaining_ = 0;
}

void SweepDelay::retarget_ramps() noexcept {
    if (ramp_total_ <= 1) {
        current_mix_ = parameters_.mix;
        current_start_hz_ = parameters_.start_hz;
        current_stop_hz_ = parameters_.stop_hz;
        current_resonance_q_ = parameters_.resonance_q;
        current_rate_hz_ = rate_hz_for(parameters_);
        current_phase_degrees_ = parameters_.phase_degrees;
        mix_step_ = 0.0F;
        start_step_ = 0.0F;
        stop_step_ = 0.0F;
        resonance_step_ = 0.0F;
        rate_step_ = 0.0F;
        phase_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }

    const float count = static_cast<float>(ramp_total_);
    mix_step_ = (parameters_.mix - current_mix_) / count;
    start_step_ = (parameters_.start_hz - current_start_hz_) / count;
    stop_step_ = (parameters_.stop_hz - current_stop_hz_) / count;
    resonance_step_ =
        (parameters_.resonance_q - current_resonance_q_) / count;
    rate_step_ = (rate_hz_for(parameters_) - current_rate_hz_) / count;
    phase_step_ =
        (parameters_.phase_degrees - current_phase_degrees_) / count;
    ramp_remaining_ = ramp_total_;
}

void SweepDelay::advance_ramps() noexcept {
    if (ramp_remaining_ == 0) return;
    if (ramp_remaining_ == 1) {
        current_mix_ = parameters_.mix;
        current_start_hz_ = parameters_.start_hz;
        current_stop_hz_ = parameters_.stop_hz;
        current_resonance_q_ = parameters_.resonance_q;
        current_rate_hz_ = rate_hz_for(parameters_);
        current_phase_degrees_ = parameters_.phase_degrees;
        mix_step_ = 0.0F;
        start_step_ = 0.0F;
        stop_step_ = 0.0F;
        resonance_step_ = 0.0F;
        rate_step_ = 0.0F;
        phase_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }
    current_mix_ += mix_step_;
    current_start_hz_ += start_step_;
    current_stop_hz_ += stop_step_;
    current_resonance_q_ += resonance_step_;
    current_rate_hz_ += rate_step_;
    current_phase_degrees_ += phase_step_;
    --ramp_remaining_;
}

float SweepDelay::lfo_value(float phase) const noexcept {
    switch (parameters_.waveform) {
    case SweepDelayWaveform::sine:
        return 0.5F + 0.5F * std::sin(kTwoPi * phase);
    case SweepDelayWaveform::triangle:
        return phase < 0.5F ? phase * 2.0F : 2.0F - phase * 2.0F;
    case SweepDelayWaveform::square:
        return phase < 0.5F ? 0.0F : 1.0F;
    }
    return 0.0F;
}

float SweepDelay::filter_wet_sample(float input, std::uint16_t channel,
                                    float cutoff_hz,
                                    float resonance_q,
                                    SweepDelayFilterType filter_type) noexcept {
    const float cutoff = std::clamp(
        cutoff_hz, 20.0F, std::min(18000.0F, sample_rate_ * 0.45F));
    const float omega = kTwoPi * cutoff / sample_rate_;
    const float sine = std::sin(omega);
    const float cosine = std::cos(omega);
    const float alpha = sine / (2.0F * resonance_q);
    const float inverse_a0 = 1.0F / (1.0F + alpha);
    float b0 = 0.0F;
    float b1 = 0.0F;
    float b2 = 0.0F;
    switch (filter_type) {
    case SweepDelayFilterType::lowpass: {
        const float one_minus_cosine = 1.0F - cosine;
        b0 = 0.5F * one_minus_cosine * inverse_a0;
        b1 = one_minus_cosine * inverse_a0;
        b2 = b0;
        break;
    }
    case SweepDelayFilterType::bandpass:
        b0 = alpha * inverse_a0;
        b2 = -b0;
        break;
    case SweepDelayFilterType::highpass: {
        const float one_plus_cosine = 1.0F + cosine;
        b0 = 0.5F * one_plus_cosine * inverse_a0;
        b1 = -one_plus_cosine * inverse_a0;
        b2 = b0;
        break;
    }
    }
    const float a1 = -2.0F * cosine * inverse_a0;
    const float a2 = (1.0F - alpha) * inverse_a0;

    BiquadState& state = filter_state_[channel];
    const double output = static_cast<double>(b0) * input +
        static_cast<double>(b1) * state.input_1 +
        static_cast<double>(b2) * state.input_2 -
        static_cast<double>(a1) * state.output_1 -
        static_cast<double>(a2) * state.output_2;
    const float bounded_output = bounded_sample(output);
    state.input_2 = state.input_1;
    state.input_1 = input;
    state.output_2 = state.output_1;
    state.output_1 = bounded_output;
    return bounded_output;
}

} // namespace tabletfx
