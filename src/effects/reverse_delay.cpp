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
constexpr float kTwoPi = 6.28318530717958647692F;

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
    forward_level_ = parameters_.forward_level;
    highpass_alpha_ = parameters_.low_cut_hz <= 0.0F
        ? 1.0F
        : std::exp(-kTwoPi * parameters_.low_cut_hz / sample_rate_);
    lowpass_alpha_ = parameters_.high_cut_hz <= 0.0F
        ? 0.0F
        : std::exp(-kTwoPi * parameters_.high_cut_hz / sample_rate_);
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    forward_level_step_ = 0.0F;
    highpass_alpha_step_ = 0.0F;
    lowpass_alpha_step_ = 0.0F;
    ramp_remaining_ = 0;
    reset();
}

void ReverseDelay::set_parameters(ReverseDelayParameters parameters) noexcept {
    const float minimum_time_ms = std::min(1.0F, max_delay_ms_);
    parameters.time_ms = bounded(parameters.time_ms, kDefaultTimeMs,
                                 minimum_time_ms, max_delay_ms_);
    if (parameters.time_mode != DelayTimeMode::milliseconds &&
        parameters.time_mode != DelayTimeMode::tempo_sync) {
        parameters.time_mode = DelayTimeMode::milliseconds;
    }
    if (static_cast<std::uint8_t>(parameters.division) >
        static_cast<std::uint8_t>(DelayDivision::sixteenth_triplet)) {
        parameters.division = DelayDivision::quarter;
    }
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, kDefaultTempoBpm,
                                   20.0F, 300.0F);
    const float beats = beats_for_division(parameters.division);
    const float requested_time_ms = parameters.time_mode ==
            DelayTimeMode::tempo_sync
        ? 60000.0F * beats / parameters.tempo_bpm
        : parameters.time_ms;
    const float effective_time_ms = bounded(requested_time_ms, kDefaultTimeMs,
                                            minimum_time_ms, max_delay_ms_);
    delay_samples_ = samples_for_ms(effective_time_ms);

    const float maximum_crossfade = std::min(
        kMaximumCrossfadeMs, effective_time_ms * 0.5F);
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

    const float max_low_cut = std::min(1000.0F, sample_rate_ * 0.15F);
    const float max_high_cut = std::min(20000.0F, sample_rate_ * 0.45F);
    parameters.low_cut_hz = bounded(parameters.low_cut_hz,
                                    kDefaultLowCutHz, 0.0F, max_low_cut);
    parameters.high_cut_hz = bounded(parameters.high_cut_hz,
                                     kDefaultHighCutHz, 0.0F, max_high_cut);
    parameters.forward_level = bounded(parameters.forward_level,
                                        kDefaultForwardLevel, 0.0F, 1.0F);
    parameters_ = parameters;

    feedback_step_ = (parameters_.feedback - feedback_) /
                     static_cast<float>(ramp_total_);
    mix_step_ = (parameters_.mix - mix_) / static_cast<float>(ramp_total_);
    forward_level_step_ = (parameters_.forward_level - forward_level_) /
                          static_cast<float>(ramp_total_);
    const float target_highpass_alpha = parameters_.low_cut_hz <= 0.0F
        ? 1.0F
        : std::exp(-kTwoPi * parameters_.low_cut_hz / sample_rate_);
    const float target_lowpass_alpha = parameters_.high_cut_hz <= 0.0F
        ? 0.0F
        : std::exp(-kTwoPi * parameters_.high_cut_hz / sample_rate_);
    highpass_alpha_step_ = (target_highpass_alpha - highpass_alpha_) /
                           static_cast<float>(ramp_total_);
    lowpass_alpha_step_ = (target_lowpass_alpha - lowpass_alpha_) /
                          static_cast<float>(ramp_total_);
    ramp_remaining_ = ramp_total_;
}

void ReverseDelay::set_enabled(bool enabled) noexcept {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    if (!enabled_) return;

    // A fresh enabled pass must not read old ring contents as its first snippet.
    capture_start_frame_ = write_count_;
    valid_frames_ = 0;
    reverse_filter_state_ = {};
    forward_filter_state_ = {};
}

void ReverseDelay::trigger_restart() noexcept {
    capture_start_frame_ = write_count_;
    valid_frames_ = 0;
    reverse_filter_state_ = {};
    forward_filter_state_ = {};
}

void ReverseDelay::process(std::span<float> interleaved,
                           std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    const std::size_t hop = std::max<std::size_t>(
        1U, delay_samples_ - crossfade_samples_);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t write_offset = write_frame_ * channels_;

        if (!enabled_ && !parameters_.spillover) {
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                const std::size_t sample_index = frame * channels_ + channel;
                if (!std::isfinite(interleaved[sample_index])) {
                    interleaved[sample_index] = 0.0F;
                }
            }
            advance_parameters();
            continue;
        }

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
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t sample_index = frame * channels_ + channel;
            const float original = std::isfinite(interleaved[sample_index])
                ? interleaved[sample_index]
                : 0.0F;
            float reverse_sample = 0.0F;
            float forward_sample = 0.0F;
            if (has_reverse_output) {
                const std::size_t current_age = phase * 2U + 1U;
                reverse_sample = read_age(current_age, channel);
                forward_sample = read_age(delay_samples_, channel);
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

            reverse_sample = filter_repeat_sample(
                reverse_sample, channel, reverse_filter_state_);
            forward_sample = filter_repeat_sample(
                forward_sample, channel, forward_filter_state_);
            const float audible_wet = running_
                ? reverse_sample + forward_sample * forward_level_
                : 0.0F;
            const double output = enabled_
                ? static_cast<double>(original) *
                      static_cast<double>(1.0F - mix_) +
                      static_cast<double>(audible_wet) *
                      static_cast<double>(mix_)
                : static_cast<double>(original) +
                      static_cast<double>(audible_wet) *
                      static_cast<double>(mix_);
            interleaved[sample_index] = bounded_sample(output);

            const double capture_input = enabled_ ? original : 0.0;
            const double recorded = capture_input +
                static_cast<double>(reverse_sample) *
                    static_cast<double>(feedback_);
            storage_[write_offset + channel] = bounded_sample(recorded);
        }

        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
        ++write_count_;
        valid_frames_ = std::min(valid_frames_ + 1U, capacity_frames_ - 1U);
        advance_parameters();
    }
}

void ReverseDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
    write_count_ = 0;
    capture_start_frame_ = 0;
    valid_frames_ = 0;
    feedback_ = parameters_.feedback;
    mix_ = parameters_.mix;
    forward_level_ = parameters_.forward_level;
    highpass_alpha_ = parameters_.low_cut_hz <= 0.0F
        ? 1.0F
        : std::exp(-kTwoPi * parameters_.low_cut_hz / sample_rate_);
    lowpass_alpha_ = parameters_.high_cut_hz <= 0.0F
        ? 0.0F
        : std::exp(-kTwoPi * parameters_.high_cut_hz / sample_rate_);
    reverse_filter_state_ = {};
    forward_filter_state_ = {};
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    forward_level_step_ = 0.0F;
    highpass_alpha_step_ = 0.0F;
    lowpass_alpha_step_ = 0.0F;
    ramp_remaining_ = 0;
}

float ReverseDelay::bounded(float value, float fallback,
                            float minimum, float maximum) noexcept {
    return std::clamp(std::isfinite(value) ? value : fallback,
                      minimum, maximum);
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
    if (age == 0U || age >= capacity_frames_ || age > valid_frames_ ||
        channel >= channels_) {
        return 0.0F;
    }
    const std::size_t read_frame =
        (write_frame_ + capacity_frames_ - age) % capacity_frames_;
    return storage_[read_frame * channels_ + channel];
}

float ReverseDelay::beats_for_division(DelayDivision division) noexcept {
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
    }
    return 1.0F;
}

float ReverseDelay::filter_repeat_sample(
    float input, std::uint16_t channel,
    std::array<RepeatFilterState, 2>& state) noexcept {
    if (channel >= channels_) return 0.0F;
    auto& filter = state[channel];
    const float highpass = highpass_alpha_ *
        (filter.highpass_previous_output + input -
         filter.highpass_previous_input);
    filter.highpass_previous_input = input;
    filter.highpass_previous_output = std::isfinite(highpass)
        ? highpass
        : 0.0F;
    const float lowpass = (1.0F - lowpass_alpha_) *
                              filter.highpass_previous_output +
                          lowpass_alpha_ * filter.lowpass_output;
    filter.lowpass_output = std::isfinite(lowpass) ? lowpass : 0.0F;
    return filter.lowpass_output;
}

void ReverseDelay::advance_parameters() noexcept {
    if (ramp_remaining_ == 0U) return;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) {
        feedback_ = parameters_.feedback;
        mix_ = parameters_.mix;
        forward_level_ = parameters_.forward_level;
        highpass_alpha_ = parameters_.low_cut_hz <= 0.0F
            ? 1.0F
            : std::exp(-kTwoPi * parameters_.low_cut_hz / sample_rate_);
        lowpass_alpha_ = parameters_.high_cut_hz <= 0.0F
            ? 0.0F
            : std::exp(-kTwoPi * parameters_.high_cut_hz / sample_rate_);
        return;
    }
    feedback_ += feedback_step_;
    mix_ += mix_step_;
    forward_level_ += forward_level_step_;
    highpass_alpha_ += highpass_alpha_step_;
    lowpass_alpha_ += lowpass_alpha_step_;
}

} // namespace tabletfx
