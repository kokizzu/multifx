#include "tabletfx/effects/fractal_surround_delay.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kFeedbackSafetyLimit = 0.98F;

float sanitized_sample_rate(float sample_rate) noexcept {
    if (!std::isfinite(sample_rate) ||
        sample_rate < 8000.0F || sample_rate > 192000.0F) {
        return 48000.0F;
    }
    return sample_rate;
}

} // namespace

FractalSurroundDelay::FractalSurroundDelay(
    float sample_rate, FractalSurroundDelayParameters parameters)
    : sample_rate_(sanitized_sample_rate(sample_rate)) {
    // Initialize the selected time without a startup ramp. All later control
    // changes grow storage only when needed, ramp the read heads, and retain
    // existing history when the ring grows.
    parameters_ = {};
    set_parameters(parameters);
    current_delay_left_frames_ = static_cast<float>(target_delay_left_frames_);
    current_delay_right_frames_ = static_cast<float>(target_delay_right_frames_);
    delay_left_step_ = 0.0F;
    delay_right_step_ = 0.0F;
    delay_ramp_remaining_ = 0U;
}

float FractalSurroundDelay::bounded(float value, float fallback,
                                    float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

float FractalSurroundDelay::finite_audio(float sample) noexcept {
    if (!std::isfinite(sample)) return 0.0F;
    return std::clamp(sample, -8.0F, 8.0F);
}

std::size_t FractalSurroundDelay::delay_frames(float time_ms) const noexcept {
    const double requested = static_cast<double>(time_ms) *
                             static_cast<double>(sample_rate_) / 1000.0;
    const auto rounded = static_cast<std::size_t>(std::llround(requested));
    const std::size_t maximum_supported = static_cast<std::size_t>(std::llround(
        static_cast<double>(sample_rate_) *
        static_cast<double>(kMaximumDelayMs) / 1000.0));
    return std::clamp<std::size_t>(rounded, 1U, maximum_supported);
}

void FractalSurroundDelay::grow_storage(std::size_t frames) noexcept {
    if (frames <= maximum_delay_frames_) return;

    std::vector<float> expanded;
    try {
        expanded.assign(frames * 2U, 0.0F);
    } catch (...) {
        // Keep the previous ring on memory pressure. update_targets() clamps
        // the active read heads to its available history; the audio callback
        // remains allocation-free and valid.
        return;
    }

    if (maximum_delay_frames_ > 0U && !storage_.empty()) {
        const std::size_t history_frames = maximum_delay_frames_;
        for (std::size_t age = 1U; age <= history_frames; ++age) {
            const std::size_t old_frame =
                (write_frame_ + maximum_delay_frames_ - age) %
                maximum_delay_frames_;
            const std::size_t new_frame = frames - age;
            expanded[new_frame * 2U] = storage_[old_frame * 2U];
            expanded[new_frame * 2U + 1U] = storage_[old_frame * 2U + 1U];
        }
    }

    storage_.swap(expanded);
    maximum_delay_frames_ = frames;
    write_frame_ = 0U;
}

void FractalSurroundDelay::set_parameters(
    FractalSurroundDelayParameters parameters) noexcept {
    const bool initialized = current_delay_left_frames_ > 0.0F &&
                             current_delay_right_frames_ > 0.0F;
    parameters.time_ms = bounded(parameters.time_ms, 375.0F,
                                  kMinimumTimeMs, kMaximumBaseTimeMs);
    parameters.master_time_percent = bounded(
        parameters.master_time_percent, 100.0F,
        kMinimumMasterTimePercent, kMaximumMasterTimePercent);
    parameters.right_time_ratio_percent = bounded(
        parameters.right_time_ratio_percent, 100.0F, 25.0F, 200.0F);
    parameters.feedback_left_percent = bounded(
        parameters.feedback_left_percent, 35.0F, 0.0F, 100.0F);
    parameters.feedback_right_percent = bounded(
        parameters.feedback_right_percent, 35.0F, 0.0F, 100.0F);
    parameters.master_feedback_percent = bounded(
        parameters.master_feedback_percent, 100.0F, 0.0F, 200.0F);
    parameters.rotation_percent = bounded(
        parameters.rotation_percent, 50.0F, -200.0F, 200.0F);
    parameters.width_percent = bounded(parameters.width_percent,
                                        100.0F, -100.0F, 100.0F);
    parameters.mix_percent = bounded(parameters.mix_percent,
                                      25.0F, 0.0F, 100.0F);
    parameters.level_db = bounded(parameters.level_db,
                                  0.0F, -24.0F, 12.0F);
    parameters.global_mix_offset = bounded(
        parameters.global_mix_offset, 0.0F, kMinimumGlobalMixOffset,
        kMaximumGlobalMixOffset);
    parameters_ = parameters;
    update_targets(initialized);
}

void FractalSurroundDelay::update_targets(bool ramp_delay) noexcept {
    const float scaled_left_ms = parameters_.time_ms *
                                 parameters_.master_time_percent / 100.0F;
    const float scaled_right_ms = scaled_left_ms *
                                  parameters_.right_time_ratio_percent / 100.0F;
    const std::size_t requested_left_frames = delay_frames(scaled_left_ms);
    const std::size_t requested_right_frames = delay_frames(scaled_right_ms);
    grow_storage(std::max(requested_left_frames, requested_right_frames));
    if (maximum_delay_frames_ > 0U) {
        target_delay_left_frames_ =
            std::min(requested_left_frames, maximum_delay_frames_);
        target_delay_right_frames_ =
            std::min(requested_right_frames, maximum_delay_frames_);
    } else {
        target_delay_left_frames_ = 1U;
        target_delay_right_frames_ = 1U;
    }

    const float master_feedback = parameters_.master_feedback_percent / 100.0F;
    feedback_left_ = std::min(
        kFeedbackSafetyLimit,
        parameters_.feedback_left_percent / 100.0F * master_feedback);
    feedback_right_ = std::min(
        kFeedbackSafetyLimit,
        parameters_.feedback_right_percent / 100.0F * master_feedback);

    // The map is a project-authored linear angle scale. The FM9 guide states
    // that Rotation moves each echo and negative values reverse its direction,
    // but it does not publish numerical bounds or a transfer curve.
    const float radians = parameters_.rotation_percent * 1.8F * kPi / 180.0F;
    rotation_cos_ = std::cos(radians);
    rotation_sin_ = std::sin(radians);
    width_scale_ = parameters_.width_percent / 100.0F;
    const float base_mix = parameters_.mix_percent / 100.0F;
    const float effective_mix = parameters_.global_mix_enabled
        ? std::clamp(base_mix + parameters_.global_mix_offset, 0.0F, 1.0F)
        : base_mix;
    mix_ = effective_mix;
    level_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);

    if (!ramp_delay || current_delay_left_frames_ <= 0.0F ||
        current_delay_right_frames_ <= 0.0F) {
        current_delay_left_frames_ =
            static_cast<float>(target_delay_left_frames_);
        current_delay_right_frames_ =
            static_cast<float>(target_delay_right_frames_);
        delay_left_step_ = 0.0F;
        delay_right_step_ = 0.0F;
        delay_ramp_remaining_ = 0U;
        return;
    }

    const double ramp_frames = static_cast<double>(sample_rate_) *
                               static_cast<double>(kParameterRampMs) / 1000.0;
    delay_ramp_remaining_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(ramp_frames)));
    delay_left_step_ =
        (static_cast<float>(target_delay_left_frames_) -
         current_delay_left_frames_) /
        static_cast<float>(delay_ramp_remaining_);
    delay_right_step_ =
        (static_cast<float>(target_delay_right_frames_) -
         current_delay_right_frames_) /
        static_cast<float>(delay_ramp_remaining_);
}

float FractalSurroundDelay::read_delay(std::uint16_t channel,
                                       float requested_delay_frames) const noexcept {
    const float maximum = static_cast<float>(maximum_delay_frames_);
    const float delay = std::clamp(requested_delay_frames, 1.0F, maximum);
    const std::size_t whole = static_cast<std::size_t>(delay);
    const float fraction = delay - static_cast<float>(whole);
    const std::size_t first_frame =
        (write_frame_ + maximum_delay_frames_ - whole) % maximum_delay_frames_;
    const std::size_t second_frame =
        (first_frame + maximum_delay_frames_ - 1U) % maximum_delay_frames_;
    const float first = storage_[first_frame * 2U + channel];
    const float second = storage_[second_frame * 2U + channel];
    return first + (second - first) * fraction;
}

void FractalSurroundDelay::advance_delay_ramp() noexcept {
    if (delay_ramp_remaining_ == 0U) return;
    current_delay_left_frames_ += delay_left_step_;
    current_delay_right_frames_ += delay_right_step_;
    --delay_ramp_remaining_;
    if (delay_ramp_remaining_ == 0U) {
        current_delay_left_frames_ =
            static_cast<float>(target_delay_left_frames_);
        current_delay_right_frames_ =
            static_cast<float>(target_delay_right_frames_);
        delay_left_step_ = 0.0F;
        delay_right_step_ = 0.0F;
    }
}

void FractalSurroundDelay::process(std::span<float> interleaved,
                                   std::uint16_t channels) noexcept {
    if (channels == 0U || interleaved.empty()) return;
    if (storage_.empty() || maximum_delay_frames_ == 0U) {
        for (float& sample : interleaved) {
            sample = finite_audio(finite_audio(sample) * level_gain_);
        }
        return;
    }
    if (channels > 2U) {
        for (float& sample : interleaved) sample = finite_audio(sample);
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        advance_delay_ramp();
        const float dry_left = finite_audio(interleaved[frame * channels]);
        if (channels == 1U) {
            const float delayed = read_delay(0U, current_delay_left_frames_);
            const float output = dry_left * (1.0F - mix_) + delayed * mix_;
            interleaved[frame] = finite_audio(output * level_gain_);
            const float write = finite_audio(dry_left + delayed * feedback_left_);
            const std::size_t offset = write_frame_ * 2U;
            storage_[offset] = write;
            storage_[offset + 1U] = write;
        } else {
            const float dry_right = finite_audio(interleaved[frame * 2U + 1U]);
            const float delayed_left =
                read_delay(0U, current_delay_left_frames_);
            const float delayed_right =
                read_delay(1U, current_delay_right_frames_);

            const float mid = 0.5F * (delayed_left + delayed_right);
            const float side = 0.5F * (delayed_left - delayed_right) * width_scale_;
            const float wet_left = mid + side;
            const float wet_right = mid - side;
            interleaved[frame * 2U] = finite_audio(
                (dry_left * (1.0F - mix_) + wet_left * mix_) * level_gain_);
            interleaved[frame * 2U + 1U] = finite_audio(
                (dry_right * (1.0F - mix_) + wet_right * mix_) * level_gain_);

            const float rotated_left = rotation_cos_ * delayed_left -
                                       rotation_sin_ * delayed_right;
            const float rotated_right = rotation_sin_ * delayed_left +
                                        rotation_cos_ * delayed_right;
            const std::size_t offset = write_frame_ * 2U;
            storage_[offset] = finite_audio(
                dry_left + rotated_left * feedback_left_);
            storage_[offset + 1U] = finite_audio(
                dry_right + rotated_right * feedback_right_);
        }
        ++write_frame_;
        if (write_frame_ == maximum_delay_frames_) write_frame_ = 0U;
    }
}

void FractalSurroundDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0U;
    current_delay_left_frames_ =
        static_cast<float>(target_delay_left_frames_);
    current_delay_right_frames_ =
        static_cast<float>(target_delay_right_frames_);
    delay_left_step_ = 0.0F;
    delay_right_step_ = 0.0F;
    delay_ramp_remaining_ = 0U;
}

} // namespace tabletfx
