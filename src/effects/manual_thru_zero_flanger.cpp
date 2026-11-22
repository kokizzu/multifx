#include "tabletfx/effects/manual_thru_zero_flanger.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {

ManualThruZeroFlanger::ManualThruZeroFlanger(
    float sample_rate, std::uint16_t channels,
    ManualThruZeroFlangerParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument(
            "Manual through-zero flanger requires 8–384 kHz and mono or stereo channels");
    }

    const double frames = std::ceil(
        static_cast<double>(sample_rate_) * static_cast<double>(kMaximumTimeMs) /
        1000.0);
    if (frames > static_cast<double>(std::numeric_limits<std::size_t>::max() - 2)) {
        throw std::length_error("Manual through-zero flanger delay buffer is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(frames) + 2;
    wet_storage_.assign(capacity_frames_ * channels_, 0.0F);
    dry_storage_.assign(capacity_frames_ * channels_, 0.0F);
    set_parameters(parameters);
    retarget(true);
}

void ManualThruZeroFlanger::set_parameters(
    ManualThruZeroFlangerParameters parameters) noexcept {
    parameters.minimum_time_ms = bounded(
        parameters.minimum_time_ms, 0.34F, kMinimumTimeMs,
        kMaximumTimeMs - kMinimumTimeSpanMs);
    parameters.maximum_time_ms = bounded(
        parameters.maximum_time_ms, 8.0F,
        parameters.minimum_time_ms + kMinimumTimeSpanMs, kMaximumTimeMs);
    parameters.manual = bounded(parameters.manual, 5.0F, 0.0F, 10.0F);
    parameters.dry_delay_position = bounded(
        parameters.dry_delay_position, 0.5F, 0.0F, 1.0F);
    parameters.feedback_percent = bounded(
        parameters.feedback_percent, 0.0F, -100.0F, 100.0F);
    switch (parameters.phase_reverse) {
    case FlangerPhaseReverse::None:
    case FlangerPhaseReverse::Left:
    case FlangerPhaseReverse::Right:
    case FlangerPhaseReverse::Both:
        break;
    default:
        parameters.phase_reverse = FlangerPhaseReverse::Both;
        break;
    }
    parameters.mix = bounded(parameters.mix, 0.5F, 0.0F, 1.0F);
    parameters.smoothing_ms = bounded(parameters.smoothing_ms, 10.0F, 0.0F, 1000.0F);

    parameters_ = parameters;
    target_minimum_time_ms_ = parameters_.minimum_time_ms;
    target_maximum_time_ms_ = parameters_.maximum_time_ms;
    target_manual_ = parameters_.manual;
    target_dry_delay_position_ = parameters_.dry_delay_position;
    target_feedback_percent_ = parameters_.feedback_percent;
    target_mix_ = parameters_.mix;
    retarget(false);
}

float ManualThruZeroFlanger::current_wet_time_ms() const noexcept {
    const float minimum = std::clamp(current_minimum_time_ms_, kMinimumTimeMs,
                                     kMaximumTimeMs - kMinimumTimeSpanMs);
    const float maximum = std::clamp(current_maximum_time_ms_,
                                     minimum + kMinimumTimeSpanMs,
                                     kMaximumTimeMs);
    const float position = std::clamp(current_manual_ / 10.0F, 0.0F, 1.0F);
    return minimum + (maximum - minimum) * position;
}

float ManualThruZeroFlanger::current_dry_time_ms() const noexcept {
    const float minimum = std::clamp(current_minimum_time_ms_, kMinimumTimeMs,
                                     kMaximumTimeMs - kMinimumTimeSpanMs);
    const float maximum = std::clamp(current_maximum_time_ms_,
                                     minimum + kMinimumTimeSpanMs,
                                     kMaximumTimeMs);
    const float position = std::clamp(current_dry_delay_position_, 0.0F, 1.0F);
    return minimum + (maximum - minimum) * position;
}

std::size_t ManualThruZeroFlanger::maximum_path_latency_frames() const noexcept {
    const double frames = std::ceil(static_cast<double>(sample_rate_) *
                                    static_cast<double>(parameters_.maximum_time_ms) /
                                    1000.0);
    if (!std::isfinite(frames) || frames <= 0.0 ||
        frames >= static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        return 0;
    }
    return static_cast<std::size_t>(frames);
}

void ManualThruZeroFlanger::process(std::span<float> interleaved,
                                    std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    const float samples_per_ms = sample_rate_ * 0.001F;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        advance_ramps();

        const float minimum_time = std::clamp(
            current_minimum_time_ms_, kMinimumTimeMs,
            kMaximumTimeMs - kMinimumTimeSpanMs);
        const float maximum_time = std::clamp(
            current_maximum_time_ms_, minimum_time + kMinimumTimeSpanMs,
            kMaximumTimeMs);
        const float span = maximum_time - minimum_time;
        const float wet_position = std::clamp(current_manual_ / 10.0F, 0.0F, 1.0F);
        const float dry_position = std::clamp(current_dry_delay_position_, 0.0F, 1.0F);
        const float wet_delay_frames =
            (minimum_time + span * wet_position) * samples_per_ms;
        const float dry_delay_frames =
            (minimum_time + span * dry_position) * samples_per_ms;
        const float feedback_gain =
            std::clamp(current_feedback_percent_ / 100.0F, -1.0F, 1.0F) *
            kMaximumFeedbackGain;
        const float dry_gain = 1.0F - current_mix_;
        const std::size_t input_offset = frame * channels_;
        const std::size_t write_offset = write_frame_ * channels_;

        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = input_offset + channel;
            const float input = std::isfinite(interleaved[index])
                                    ? interleaved[index]
                                    : 0.0F;
            const float wet = read_delay(wet_storage_, write_frame_, channel,
                                         wet_delay_frames);
            const float dry = read_delay(dry_storage_, write_frame_, channel,
                                         dry_delay_frames);

            float feedback_sample = input + wet * feedback_gain;
            if (!std::isfinite(feedback_sample)) {
                feedback_sample = 0.0F;
            }
            wet_storage_[write_offset + channel] = feedback_sample;
            dry_storage_[write_offset + channel] = input;

            bool reverse = false;
            switch (parameters_.phase_reverse) {
            case FlangerPhaseReverse::None:
                break;
            case FlangerPhaseReverse::Left:
                reverse = channel == 0;
                break;
            case FlangerPhaseReverse::Right:
                reverse = channel == 1;
                break;
            case FlangerPhaseReverse::Both:
                reverse = true;
                break;
            }
            const float wet_sign = reverse ? -1.0F : 1.0F;
            const float output = dry_gain * dry + current_mix_ * wet_sign * wet;
            interleaved[index] = std::isfinite(output) ? output : 0.0F;
        }

        ++write_frame_;
        if (write_frame_ == capacity_frames_) {
            write_frame_ = 0;
        }
    }
}

void ManualThruZeroFlanger::reset() noexcept {
    std::fill(wet_storage_.begin(), wet_storage_.end(), 0.0F);
    std::fill(dry_storage_.begin(), dry_storage_.end(), 0.0F);
    write_frame_ = 0;
    retarget(true);
}

float ManualThruZeroFlanger::bounded(float value, float fallback,
                                     float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float ManualThruZeroFlanger::read_delay(const std::vector<float>& storage,
                                        std::size_t write_frame,
                                        std::uint16_t channel,
                                        float delay_frames) const noexcept {
    const float bounded_delay = std::clamp(
        delay_frames, 1.0F, static_cast<float>(capacity_frames_ - 2));
    const std::size_t whole_delay = static_cast<std::size_t>(bounded_delay);
    const float fraction = bounded_delay - static_cast<float>(whole_delay);
    std::size_t newer_frame = write_frame + capacity_frames_ - whole_delay;
    if (newer_frame >= capacity_frames_) {
        newer_frame -= capacity_frames_;
    }
    const std::size_t older_frame = newer_frame == 0
                                        ? capacity_frames_ - 1
                                        : newer_frame - 1;
    const float newer = storage[newer_frame * channels_ + channel];
    const float older = storage[older_frame * channels_ + channel];
    const float value = newer * (1.0F - fraction) + older * fraction;
    return std::isfinite(value) ? value : 0.0F;
}

void ManualThruZeroFlanger::retarget(bool immediate) noexcept {
    if (immediate) {
        current_minimum_time_ms_ = target_minimum_time_ms_;
        current_maximum_time_ms_ = target_maximum_time_ms_;
        current_manual_ = target_manual_;
        current_dry_delay_position_ = target_dry_delay_position_;
        current_feedback_percent_ = target_feedback_percent_;
        current_mix_ = target_mix_;
        minimum_time_step_ = 0.0F;
        maximum_time_step_ = 0.0F;
        manual_step_ = 0.0F;
        dry_delay_position_step_ = 0.0F;
        feedback_percent_step_ = 0.0F;
        mix_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }

    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(parameters_.smoothing_ms) /
                                    1000.0;
    ramp_remaining_ = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_remaining_ == 0) {
        retarget(true);
        return;
    }

    const float denominator = static_cast<float>(ramp_remaining_);
    minimum_time_step_ =
        (target_minimum_time_ms_ - current_minimum_time_ms_) / denominator;
    maximum_time_step_ =
        (target_maximum_time_ms_ - current_maximum_time_ms_) / denominator;
    manual_step_ = (target_manual_ - current_manual_) / denominator;
    dry_delay_position_step_ =
        (target_dry_delay_position_ - current_dry_delay_position_) / denominator;
    feedback_percent_step_ =
        (target_feedback_percent_ - current_feedback_percent_) / denominator;
    mix_step_ = (target_mix_ - current_mix_) / denominator;
}

void ManualThruZeroFlanger::advance_ramps() noexcept {
    if (ramp_remaining_ == 0) {
        return;
    }

    current_minimum_time_ms_ += minimum_time_step_;
    current_maximum_time_ms_ += maximum_time_step_;
    current_manual_ += manual_step_;
    current_dry_delay_position_ += dry_delay_position_step_;
    current_feedback_percent_ += feedback_percent_step_;
    current_mix_ += mix_step_;
    --ramp_remaining_;
    if (ramp_remaining_ == 0) {
        current_minimum_time_ms_ = target_minimum_time_ms_;
        current_maximum_time_ms_ = target_maximum_time_ms_;
        current_manual_ = target_manual_;
        current_dry_delay_position_ = target_dry_delay_position_;
        current_feedback_percent_ = target_feedback_percent_;
        current_mix_ = target_mix_;
        minimum_time_step_ = 0.0F;
        maximum_time_step_ = 0.0F;
        manual_step_ = 0.0F;
        dry_delay_position_step_ = 0.0F;
        feedback_percent_step_ = 0.0F;
        mix_step_ = 0.0F;
    }
}

} // namespace tabletfx
