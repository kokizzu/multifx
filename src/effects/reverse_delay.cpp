#include "tabletfx/effects/reverse_delay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr double kMaximumFloat =
    static_cast<double>(std::numeric_limits<float>::max());
constexpr float kPiOverTwo = 1.57079632679489661923F;

float bounded_sample(double value) noexcept {
    return static_cast<float>(std::clamp(value, -kMaximumFloat, kMaximumFloat));
}

} // namespace

ReverseDelay::ReverseDelay(float sample_rate, std::uint16_t channels,
                           float max_delay_ms,
                           ReverseDelayParameters parameters)
    : sample_rate_(sample_rate), channels_(channels), max_delay_ms_(max_delay_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2 ||
        !std::isfinite(max_delay_ms_) || max_delay_ms_ <= 0.0F ||
        max_delay_ms_ > 6000.0F) {
        throw std::invalid_argument(
            "Reverse Delay requires a supported sample rate, mono/stereo layout, and a maximum time in (0, 6000] ms");
    }

    const double requested_frames = std::ceil(
        static_cast<double>(sample_rate_) * static_cast<double>(max_delay_ms_) /
        1000.0);
    const double maximum_frames = static_cast<double>(
        (std::numeric_limits<std::size_t>::max() - 1U) / 2U);
    if (!std::isfinite(requested_frames) || requested_frames < 1.0 ||
        requested_frames > maximum_frames) {
        throw std::length_error("Reverse Delay buffer size is too large");
    }

    const std::size_t maximum = static_cast<std::size_t>(requested_frames);
    capacity_frames_ = maximum * 2U + 1U;
    if (capacity_frames_ > storage_.max_size() / channels_) {
        throw std::length_error("Reverse Delay buffer size is too large");
    }
    storage_.assign(capacity_frames_ * channels_, 0.0F);

    const double ramp_frames = std::ceil(
        static_cast<double>(sample_rate_) *
        static_cast<double>(kParameterSmoothingMs) / 1000.0);
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(ramp_frames));
    set_parameters(parameters);
    feedback_ = parameters_.feedback;
    mix_ = parameters_.mix;
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    ramp_remaining_ = 0;
    reset();
}

void ReverseDelay::set_parameters(ReverseDelayParameters parameters) noexcept {
    parameters.time_ms = bounded(parameters.time_ms, kDefaultTimeMs,
                                 1.0F, max_delay_ms_);
    delay_samples_ = samples_for_ms(parameters.time_ms);

    const float maximum_crossfade = std::min(
        kMaximumCrossfadeMs, parameters.time_ms * 0.5F);
    parameters.crossfade_ms = bounded(parameters.crossfade_ms,
                                      kDefaultCrossfadeMs, 0.0F,
                                      maximum_crossfade);
    const double requested_crossfade_samples = std::round(
        static_cast<double>(sample_rate_) *
        static_cast<double>(parameters.crossfade_ms) / 1000.0);
    crossfade_samples_ = parameters.crossfade_ms <= 0.0F
        ? 0U
        : static_cast<std::size_t>(std::max(1.0, requested_crossfade_samples));
    crossfade_samples_ = std::min(
        crossfade_samples_, (delay_samples_ - 1U) / 2U);
    parameters.crossfade_ms = static_cast<float>(
        static_cast<double>(crossfade_samples_) * 1000.0 /
        static_cast<double>(sample_rate_));

    parameters.feedback = bounded(parameters.feedback, kDefaultFeedback,
                                  0.0F, 0.95F);
    parameters.mix = bounded(parameters.mix, kDefaultMix, 0.0F, 1.0F);
    parameters_ = parameters;

    feedback_step_ = (parameters_.feedback - feedback_) /
                     static_cast<float>(ramp_total_);
    mix_step_ = (parameters_.mix - mix_) / static_cast<float>(ramp_total_);
    ramp_remaining_ = ramp_total_;
}

void ReverseDelay::trigger_restart() noexcept {
    capture_start_frame_ = write_count_;
}

void ReverseDelay::process(std::span<float> interleaved,
                           std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    const std::size_t hop = std::max<std::size_t>(
        1U, delay_samples_ - crossfade_samples_);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::uint64_t relative_frame = write_count_ - capture_start_frame_;
        const bool has_reverse_output =
            relative_frame >= static_cast<std::uint64_t>(delay_samples_);
        const std::uint64_t elapsed = has_reverse_output
            ? relative_frame - static_cast<std::uint64_t>(delay_samples_)
            : 0U;
        const std::uint64_t hop_u64 = static_cast<std::uint64_t>(hop);
        const std::uint64_t grain = has_reverse_output ? elapsed / hop_u64 : 0U;
        const std::size_t phase = has_reverse_output
            ? static_cast<std::size_t>(elapsed % hop_u64)
            : 0U;
        const std::size_t write_offset = write_frame_ * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t sample_index = frame * channels_ + channel;
            const float original = std::isfinite(interleaved[sample_index])
                ? interleaved[sample_index]
                : 0.0F;
            float reverse_sample = 0.0F;
            if (has_reverse_output) {
                const std::size_t current_age = phase * 2U + 1U;
                reverse_sample = read_age(current_age, channel);
                if (crossfade_samples_ > 0U && grain > 0U &&
                    phase < crossfade_samples_) {
                    const std::size_t previous_phase = hop + phase;
                    const std::size_t previous_age = previous_phase * 2U + 1U;
                    const float progress = static_cast<float>(phase + 1U) /
                        static_cast<float>(crossfade_samples_ + 1U);
                    const float angle = progress * kPiOverTwo;
                    const float previous_weight = std::cos(angle);
                    const float current_weight = std::sin(angle);
                    reverse_sample =
                        read_age(previous_age, channel) * previous_weight +
                        reverse_sample * current_weight;
                }
            }

            const float audible_reverse = running_ ? reverse_sample : 0.0F;
            const double output = static_cast<double>(original) *
                                      static_cast<double>(1.0F - mix_) +
                                  static_cast<double>(audible_reverse) *
                                      static_cast<double>(mix_);
            interleaved[sample_index] = bounded_sample(output);

            const double recorded = static_cast<double>(original) +
                static_cast<double>(reverse_sample) *
                    static_cast<double>(feedback_);
            storage_[write_offset + channel] = bounded_sample(recorded);
        }

        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
        ++write_count_;
        advance_parameters();
    }
}

void ReverseDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
    write_count_ = 0;
    capture_start_frame_ = 0;
    feedback_ = parameters_.feedback;
    mix_ = parameters_.mix;
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    ramp_remaining_ = 0;
}

float ReverseDelay::bounded(float value, float fallback,
                            float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

std::size_t ReverseDelay::samples_for_ms(float milliseconds) const noexcept {
    const double samples = std::round(
        static_cast<double>(sample_rate_) *
        static_cast<double>(milliseconds) / 1000.0);
    const double maximum = static_cast<double>(capacity_frames_ / 2U);
    const double bounded_samples = std::clamp(samples, 1.0, maximum);
    return static_cast<std::size_t>(bounded_samples);
}

float ReverseDelay::read_age(std::size_t age,
                             std::uint16_t channel) const noexcept {
    if (age == 0U || age >= capacity_frames_ || channel >= channels_) return 0.0F;
    const std::size_t read_frame =
        (write_frame_ + capacity_frames_ - age) % capacity_frames_;
    return storage_[read_frame * channels_ + channel];
}

void ReverseDelay::advance_parameters() noexcept {
    if (ramp_remaining_ == 0U) return;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) {
        feedback_ = parameters_.feedback;
        mix_ = parameters_.mix;
        return;
    }
    feedback_ += feedback_step_;
    mix_ += mix_step_;
}

} // namespace tabletfx
