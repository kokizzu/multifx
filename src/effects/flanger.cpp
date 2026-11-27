#include "tabletfx/effects/flanger.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {

Flanger::Flanger(float sample_rate, std::uint16_t channels,
                 FlangerParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument("Flanger requires 8–384 kHz and mono or stereo channels");
    }
    const double frames = std::ceil(static_cast<double>(sample_rate_) *
                                    static_cast<double>(kStorageDelayMs) / 1000.0);
    if (frames > static_cast<double>(std::numeric_limits<std::size_t>::max() - 2)) {
        throw std::length_error("Flanger delay buffer is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(frames) + 2;
    storage_.assign(capacity_frames_ * channels_, 0.0F);
    set_parameters(parameters);
    retarget(true);
}

void Flanger::set_parameters(FlangerParameters parameters) noexcept {
    parameters.rate_hz = bounded(parameters.rate_hz, 0.25F, 0.05F, 8.0F);
    parameters.mode = sanitize_mode(parameters.mode);
    parameters.color = bounded(parameters.color, 0.35F, 0.0F,
                               kFilterMaximumColor);
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, 120.0F,
                                   kMinimumTempoBpm, kMaximumTempoBpm);
    if (!valid_tap_division(parameters.tap_division)) {
        parameters.tap_division = DelayDivision::quarter;
    }
    parameters.base_delay_ms = bounded(parameters.base_delay_ms, 2.0F,
                                       kMinimumDelayMs, kMaximumDelayMs);
    const float maximum_depth = std::max(
        0.0F, std::min({4.5F, parameters.base_delay_ms - 0.25F,
                        kStorageDelayMs - parameters.base_delay_ms}));
    parameters.depth_ms = bounded(parameters.depth_ms,
                                   std::min(1.0F, maximum_depth), 0.0F,
                                   maximum_depth);
    parameters.feedback = bounded(parameters.feedback, 0.35F, 0.0F, 0.9F);
    switch (parameters.feedback_polarity) {
    case FlangerFeedbackPolarity::Positive:
    case FlangerFeedbackPolarity::Negative:
        break;
    default:
        parameters.feedback_polarity = FlangerFeedbackPolarity::Positive;
        break;
    }
    parameters.mix = bounded(parameters.mix, 0.35F, 0.0F, 1.0F);
    parameters.right_channel_phase_degrees = bounded(
        parameters.right_channel_phase_degrees, 180.0F, 0.0F, 360.0F);
    parameters.smoothing_ms = bounded(parameters.smoothing_ms, 10.0F, 0.0F,
                                      1000.0F);
    parameters_ = parameters;
    if (parameters_.right_channel_phase_degrees == 180.0F) {
        target_right_phase_sine_ = 0.0F;
        target_right_phase_cosine_ = -1.0F;
    } else if (parameters_.right_channel_phase_degrees == 0.0F ||
               parameters_.right_channel_phase_degrees == 360.0F) {
        target_right_phase_sine_ = 0.0F;
        target_right_phase_cosine_ = 1.0F;
    } else {
        const float phase_radians = parameters_.right_channel_phase_degrees *
                                    (kPi / 180.0F);
        target_right_phase_sine_ = std::sin(phase_radians);
        target_right_phase_cosine_ = std::cos(phase_radians);
    }
    const bool filter_mode = parameters_.mode == FlangerMode::Filter;
    target_base_delay_ms_ = filter_mode
                                ? filter_delay_ms(parameters_.rate_hz)
                                : parameters_.base_delay_ms;
    target_depth_ms_ = filter_mode ? 0.0F : parameters_.depth_ms;
    if (filter_mode) {
        // The project-authored static comb interprets Color as positive
        // feedback/intensity. Tap Division and tempo sync intentionally do
        // not affect this fixed spectral response.
        target_feedback_ = parameters_.color;
    } else {
        const float polarity = parameters_.feedback_polarity == FlangerFeedbackPolarity::Negative
                                   ? -1.0F
                                   : 1.0F;
        target_feedback_ = parameters_.feedback * polarity;
    }
    target_mix_ = parameters_.mix;
    if (parameters_.global_mix_enabled &&
        parameters_.mode == FlangerMode::Flanger) {
        // Fractal documents a global ±50% offset but not its equation. This
        // project uses an additive percentage-point interpretation.
        target_mix_ = std::clamp(target_mix_ + parameters_.global_mix_offset,
                                 0.0F, 1.0F);
    }
    prepare_rate();
    retarget(false);
}

bool Flanger::valid_tap_division(DelayDivision division) noexcept {
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

FlangerMode Flanger::sanitize_mode(FlangerMode mode) noexcept {
    switch (mode) {
    case FlangerMode::Flanger:
    case FlangerMode::Filter:
        return mode;
    }
    return FlangerMode::Flanger;
}

float Flanger::filter_delay_ms(float rate_setting) noexcept {
    // The guide describes different static tonalities at different Rate
    // positions but publishes no mapping. This linear 0.5..10 ms mapping is a
    // project choice that reuses the existing short delay line.
    const float rate = bounded(rate_setting, 0.25F, kFilterMinimumRate,
                               kFilterMaximumRate);
    const float position = (rate - kFilterMinimumRate) /
                           (kFilterMaximumRate - kFilterMinimumRate);
    return kMinimumDelayMs + position * (kMaximumDelayMs - kMinimumDelayMs);
}

float Flanger::beats_for_tap_division(DelayDivision division) noexcept {
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

float Flanger::effective_rate_hz() const noexcept {
    if (!parameters_.tempo_sync) {
        return parameters_.rate_hz;
    }
    const float rate_hz = (parameters_.tempo_bpm / 60.0F) /
                          beats_for_tap_division(parameters_.tap_division);
    return bounded(rate_hz, parameters_.rate_hz, 0.05F,
                   kMaximumSynchronizedRateHz);
}

void Flanger::process(std::span<float> interleaved,
                      std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    const float samples_per_ms = sample_rate_ * 0.001F;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (ramp_remaining_ > 0) {
            current_base_delay_ms_ += base_delay_step_;
            current_depth_ms_ += depth_step_;
            current_feedback_ += feedback_step_;
            current_mix_ += mix_step_;
            const float old_right_phase_sine = right_phase_sine_;
            right_phase_sine_ = old_right_phase_sine * right_phase_cosine_step_ +
                                right_phase_cosine_ * right_phase_sine_step_;
            right_phase_cosine_ = right_phase_cosine_ * right_phase_cosine_step_ -
                                  old_right_phase_sine * right_phase_sine_step_;
            --ramp_remaining_;
            if (ramp_remaining_ == 0) {
                current_base_delay_ms_ = target_base_delay_ms_;
                current_depth_ms_ = target_depth_ms_;
                current_feedback_ = target_feedback_;
                current_mix_ = target_mix_;
                right_phase_sine_ = target_right_phase_sine_;
                right_phase_cosine_ = target_right_phase_cosine_;
                base_delay_step_ = 0.0F;
                depth_step_ = 0.0F;
                feedback_step_ = 0.0F;
                mix_step_ = 0.0F;
                right_phase_sine_step_ = 0.0F;
                right_phase_cosine_step_ = 1.0F;
            }
        }

        const float base_delay = current_base_delay_ms_ * samples_per_ms;
        const float modulation_depth = current_depth_ms_ * samples_per_ms;
        float right_modulation = sine_phase_;
        if (parameters_.mode == FlangerMode::Flanger && channels_ > 1) {
            right_modulation = sine_phase_ * right_phase_cosine_ +
                               cosine_phase_ * right_phase_sine_;
        }
        const std::size_t write_offset = write_frame_ * channels_;
        const std::size_t input_offset = frame * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = input_offset + channel;
            const float input = std::isfinite(interleaved[index])
                                    ? interleaved[index]
                                    : 0.0F;
            float delay = base_delay;
            if (parameters_.mode == FlangerMode::Flanger) {
                const float modulation = channel == 0 ? sine_phase_ : right_modulation;
                delay = std::clamp(base_delay + modulation_depth * modulation,
                                   1.0F,
                                   static_cast<float>(capacity_frames_ - 2));
            } else {
                delay = std::clamp(base_delay, 1.0F,
                                   static_cast<float>(capacity_frames_ - 2));
            }
            const std::size_t whole_delay = static_cast<std::size_t>(delay);
            const float fraction = delay - static_cast<float>(whole_delay);
            std::size_t read_frame = write_frame_ + capacity_frames_ - whole_delay;
            if (read_frame >= capacity_frames_) {
                read_frame -= capacity_frames_;
            }
            const std::size_t older_frame = read_frame == 0
                                                ? capacity_frames_ - 1
                                                : read_frame - 1;
            const float newer = storage_[read_frame * channels_ + channel];
            const float older = storage_[older_frame * channels_ + channel];
            const float wet = newer * (1.0F - fraction) + older * fraction;
            float feedback_sample = input + wet * current_feedback_;
            if (!std::isfinite(feedback_sample)) {
                feedback_sample = 0.0F;
            }
            storage_[write_offset + channel] = feedback_sample;
            const float output = input * (1.0F - current_mix_) + wet * current_mix_;
            interleaved[index] = std::isfinite(output) ? output : 0.0F;
        }

        ++write_frame_;
        if (write_frame_ == capacity_frames_) {
            write_frame_ = 0;
        }
        if (parameters_.mode == FlangerMode::Flanger) {
            advance_modulator();
        }
    }
}

void Flanger::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    normalization_counter_ = 0;
    retarget(true);
}

float Flanger::bounded(float value, float fallback,
                       float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

void Flanger::prepare_rate() noexcept {
    const float step = kTwoPi * effective_rate_hz() / sample_rate_;
    sine_increment_ = std::sin(step);
    cosine_increment_ = std::cos(step);
}

void Flanger::retarget(bool immediate) noexcept {
    if (immediate) {
        current_base_delay_ms_ = target_base_delay_ms_;
        current_depth_ms_ = target_depth_ms_;
        current_feedback_ = target_feedback_;
        current_mix_ = target_mix_;
        right_phase_sine_ = target_right_phase_sine_;
        right_phase_cosine_ = target_right_phase_cosine_;
        base_delay_step_ = 0.0F;
        depth_step_ = 0.0F;
        feedback_step_ = 0.0F;
        mix_step_ = 0.0F;
        right_phase_sine_step_ = 0.0F;
        right_phase_cosine_step_ = 1.0F;
        ramp_remaining_ = 0;
        return;
    }
    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(parameters_.smoothing_ms) / 1000.0;
    ramp_remaining_ = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_remaining_ == 0) {
        retarget(true);
        return;
    }
    const float denominator = static_cast<float>(ramp_remaining_);
    base_delay_step_ = (target_base_delay_ms_ - current_base_delay_ms_) / denominator;
    depth_step_ = (target_depth_ms_ - current_depth_ms_) / denominator;
    feedback_step_ = (target_feedback_ - current_feedback_) / denominator;
    mix_step_ = (target_mix_ - current_mix_) / denominator;
    const float phase_cross =
        target_right_phase_sine_ * right_phase_cosine_ -
        target_right_phase_cosine_ * right_phase_sine_;
    const float phase_dot =
        target_right_phase_cosine_ * right_phase_cosine_ +
        target_right_phase_sine_ * right_phase_sine_;
    const float phase_step = std::atan2(phase_cross, phase_dot) / denominator;
    right_phase_sine_step_ = std::sin(phase_step);
    right_phase_cosine_step_ = std::cos(phase_step);
}

void Flanger::advance_modulator() noexcept {
    const float old_sine = sine_phase_;
    sine_phase_ = old_sine * cosine_increment_ +
                  cosine_phase_ * sine_increment_;
    cosine_phase_ = cosine_phase_ * cosine_increment_ -
                    old_sine * sine_increment_;
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
