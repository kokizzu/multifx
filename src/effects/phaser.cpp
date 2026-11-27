#include "tabletfx/effects/phaser.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx {

Phaser::Phaser(float sample_rate, std::uint16_t channels,
               PhaserParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument("Phaser requires 8–384 kHz and mono or stereo channels");
    }
    set_parameters(parameters);
    retarget(true);
}

void Phaser::set_parameters(PhaserParameters parameters) noexcept {
    const std::uint8_t previous_stages = parameters_.stages;
    const float maximum_frequency = sample_rate_ * 0.45F;
    parameters.rate_hz = bounded(parameters.rate_hz, 0.5F, 0.05F, 8.0F);
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, 120.0F,
                                   kMinimumTempoBpm, kMaximumTempoBpm);
    if (!valid_tap_division(parameters.tap_division)) {
        parameters.tap_division = DelayDivision::quarter;
    }
    parameters.minimum_hz = bounded(parameters.minimum_hz, 250.0F,
                                     kMinimumFrequencyHz, maximum_frequency);
    parameters.maximum_hz = bounded(parameters.maximum_hz, 1600.0F,
                                     kMinimumFrequencyHz, maximum_frequency);
    if (parameters.minimum_hz > parameters.maximum_hz) {
        std::swap(parameters.minimum_hz, parameters.maximum_hz);
    }
    if (parameters.stages != 4 && parameters.stages != 6 && parameters.stages != 8 &&
        parameters.stages != 10 && parameters.stages != 12) {
        parameters.stages = 4;
    }
    if (parameters.stages != previous_stages) {
        state_ = {};
        previous_wet_ = {};
    }
    parameters.feedback = bounded(parameters.feedback, 0.2F,
                                   -kMaximumFeedback, kMaximumFeedback);
    parameters.mix = bounded(parameters.mix, 0.5F, 0.0F, 1.0F);
    parameters.smoothing_ms = bounded(parameters.smoothing_ms, 10.0F, 0.0F, 1000.0F);
    parameters_ = parameters;

    target_minimum_coefficient_ = coefficient_for_frequency(parameters_.minimum_hz);
    target_maximum_coefficient_ = coefficient_for_frequency(parameters_.maximum_hz);
    target_feedback_ = parameters_.feedback;
    target_mix_ = parameters_.mix;
    prepare_lfo();
    retarget(false);
    if (parameters_.global_mix_enabled) {
        target_mix_ = std::clamp(target_mix_ + parameters_.global_mix_offset * 0.01F,
                                 0.0F, 1.0F);
    }
}

bool Phaser::valid_tap_division(DelayDivision division) noexcept {
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
    default:
        return false;
    }
}

float Phaser::beats_for_tap_division(DelayDivision division) noexcept {
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
    default: return 1.0F;
    }
}

float Phaser::effective_rate_hz() const noexcept {
    if (!parameters_.tempo_sync) return parameters_.rate_hz;
    const float rate_hz = (parameters_.tempo_bpm / 60.0F) /
                          beats_for_tap_division(parameters_.tap_division);
    return bounded(rate_hz, parameters_.rate_hz, 0.05F,
                   kMaximumSynchronizedRateHz);
}

void Phaser::process(std::span<float> interleaved,
                     std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (ramp_remaining_ > 0) {
            current_minimum_coefficient_ += minimum_coefficient_step_;
            current_maximum_coefficient_ += maximum_coefficient_step_;
            current_feedback_ += feedback_step_;
            current_mix_ += mix_step_;
            current_sine_increment_ += sine_increment_step_;
            current_cosine_increment_ += cosine_increment_step_;
            --ramp_remaining_;
            if (ramp_remaining_ == 0) {
                current_minimum_coefficient_ = target_minimum_coefficient_;
                current_maximum_coefficient_ = target_maximum_coefficient_;
                current_feedback_ = target_feedback_;
                current_mix_ = target_mix_;
                current_sine_increment_ = target_sine_increment_;
                current_cosine_increment_ = target_cosine_increment_;
                minimum_coefficient_step_ = 0.0F;
                maximum_coefficient_step_ = 0.0F;
                feedback_step_ = 0.0F;
                mix_step_ = 0.0F;
                sine_increment_step_ = 0.0F;
                cosine_increment_step_ = 0.0F;
            }
        }

        const std::size_t input_offset = frame * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = input_offset + channel;
            const float dry = std::isfinite(interleaved[index]) ? interleaved[index] : 0.0F;
            const float phase = channel == 0 || !parameters_.stereo
                ? sine_phase_ : -sine_phase_;
            const float sweep = 0.5F * (phase + 1.0F);
            const float coefficient = current_minimum_coefficient_ +
                (current_maximum_coefficient_ - current_minimum_coefficient_) * sweep;
            float value = dry + previous_wet_[channel] * current_feedback_;
            for (std::size_t stage = 0; stage < parameters_.stages; ++stage) {
                StageState& state = state_[channel][stage];
                const float output = coefficient * value + state.previous_input -
                                     coefficient * state.previous_output;
                state.previous_input = value;
                state.previous_output = std::isfinite(output) ? output : 0.0F;
                value = state.previous_output;
            }
            previous_wet_[channel] = std::isfinite(value) ? value : 0.0F;
            const float output = dry * (1.0F - current_mix_) +
                                 previous_wet_[channel] * current_mix_;
            interleaved[index] = std::isfinite(output) ? output : 0.0F;
        }
        advance_lfo();
    }
}

void Phaser::reset() noexcept {
    state_ = {};
    previous_wet_ = {};
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    normalization_counter_ = 0;
    retarget(true);
}

float Phaser::bounded(float value, float fallback,
                      float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float Phaser::coefficient_for_frequency(float frequency_hz) const noexcept {
    const float tangent = std::tan(kPi * frequency_hz / sample_rate_);
    return (tangent - 1.0F) / (tangent + 1.0F);
}

void Phaser::prepare_lfo() noexcept {
    const float step = kTwoPi * effective_rate_hz() / sample_rate_;
    target_sine_increment_ = std::sin(step);
    target_cosine_increment_ = std::cos(step);
}

void Phaser::retarget(bool immediate) noexcept {
    if (immediate) {
        current_minimum_coefficient_ = target_minimum_coefficient_;
        current_maximum_coefficient_ = target_maximum_coefficient_;
        current_feedback_ = target_feedback_;
        current_mix_ = target_mix_;
        current_sine_increment_ = target_sine_increment_;
        current_cosine_increment_ = target_cosine_increment_;
        minimum_coefficient_step_ = 0.0F;
        maximum_coefficient_step_ = 0.0F;
        feedback_step_ = 0.0F;
        mix_step_ = 0.0F;
        sine_increment_step_ = 0.0F;
        cosine_increment_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }

    const double frames = static_cast<double>(sample_rate_) *
                          static_cast<double>(parameters_.smoothing_ms) / 1000.0;
    ramp_remaining_ = static_cast<std::size_t>(std::llround(frames));
    if (ramp_remaining_ == 0) {
        retarget(true);
        return;
    }
    const float denominator = static_cast<float>(ramp_remaining_);
    minimum_coefficient_step_ =
        (target_minimum_coefficient_ - current_minimum_coefficient_) / denominator;
    maximum_coefficient_step_ =
        (target_maximum_coefficient_ - current_maximum_coefficient_) / denominator;
    feedback_step_ = (target_feedback_ - current_feedback_) / denominator;
    mix_step_ = (target_mix_ - current_mix_) / denominator;
    sine_increment_step_ =
        (target_sine_increment_ - current_sine_increment_) / denominator;
    cosine_increment_step_ =
        (target_cosine_increment_ - current_cosine_increment_) / denominator;
}

void Phaser::advance_lfo() noexcept {
    const float old_sine = sine_phase_;
    sine_phase_ = old_sine * current_cosine_increment_ +
                  cosine_phase_ * current_sine_increment_;
    cosine_phase_ = cosine_phase_ * current_cosine_increment_ -
                    old_sine * current_sine_increment_;
    if (++normalization_counter_ == 4096U) {
        const float magnitude_squared = sine_phase_ * sine_phase_ +
                                        cosine_phase_ * cosine_phase_;
        if (std::isfinite(magnitude_squared) && magnitude_squared > 0.0F) {
            const float inverse_magnitude = 1.0F / std::sqrt(magnitude_squared);
            sine_phase_ *= inverse_magnitude;
            cosine_phase_ *= inverse_magnitude;
        } else {
            sine_phase_ = 0.0F;
            cosine_phase_ = 1.0F;
        }
        normalization_counter_ = 0;
    }
}

} // namespace tabletfx
