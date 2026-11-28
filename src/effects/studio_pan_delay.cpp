#include "tabletfx/effects/studio_pan_delay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kSqrtTwo = 1.41421356237309504880F;

} // namespace

StudioPanDelay::StudioPanDelay(float sample_rate,
                               StudioPanDelayParameters parameters)
    : StudioPanDelay(sample_rate, kMaximumDelayMs, parameters) {}

StudioPanDelay::StudioPanDelay(float sample_rate, float max_delay_ms,
                               StudioPanDelayParameters parameters)
    : sample_rate_(sample_rate), requested_max_delay_ms_(max_delay_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate ||
        !std::isfinite(requested_max_delay_ms_) ||
        requested_max_delay_ms_ < kMinimumDelayMs ||
        requested_max_delay_ms_ > kMaximumDelayMs) {
        throw std::invalid_argument(
            "Studio Pan Delay requires 8-384 kHz and a 30-2000 ms buffer request");
    }
    configure(parameters);
    reset();
}

float StudioPanDelay::maximum_delay_ms() const noexcept {
    if (storage_.empty()) return requested_max_delay_ms_;
    if (capacity_frames_ <= 2U) return 0.0F;
    const float storage_limit_ms =
        static_cast<float>(capacity_frames_ - 2U) * 1000.0F / sample_rate_;
    return std::min(requested_max_delay_ms_, storage_limit_ms);
}

float StudioPanDelay::effective_delay_ms() const noexcept {
    return current_delay_frames_ * 1000.0F / sample_rate_;
}

void StudioPanDelay::configure(StudioPanDelayParameters parameters) noexcept {
    const float maximum_time = maximum_delay_ms();
    parameters.delay_time_ms = bounded(parameters.delay_time_ms,
                                       kDefaultDelayMs,
                                       kMinimumDelayMs, maximum_time);
    parameters.feedback = bounded(parameters.feedback, kDefaultFeedback,
                                  kMinimumFeedback, kMaximumFeedback);
    parameters.mix = bounded(parameters.mix, kDefaultMix, 0.0F, 1.0F);
    parameters.pan_rate_hz = bounded(parameters.pan_rate_hz, 0.5F,
                                     kMinimumRateHz, kMaximumRateHz);
    parameters.pan_depth = bounded(parameters.pan_depth, 0.75F, 0.0F, 1.0F);
    parameters.pan_mode = sanitize_mode(parameters.pan_mode);
    parameters_ = parameters;

    if (storage_.empty()) {
        if (!parameters_.bypass) {
            if (!prepare()) {
                parameters_.bypass = true;
                return;
            }
        } else {
            initialized_ = false;
            return;
        }
    }

    update_envelope_coefficients();
    prepare_oscillator();

    const float delay_target = delay_frames_for_ms(parameters_.delay_time_ms);
    if (!initialized_) {
        current_delay_frames_ = delay_target;
        target_delay_frames_ = delay_target;
        current_feedback_ = parameters_.feedback;
        target_feedback_ = parameters_.feedback;
        current_mix_ = parameters_.mix;
        target_mix_ = parameters_.mix;
        current_depth_ = parameters_.pan_depth;
        depth_target_ = parameters_.pan_depth;
        mode_weights_ = mode_weights(parameters_.pan_mode);
        mode_targets_ = mode_weights_;
        initialized_ = true;
        return;
    }

    start_delay_ramp(delay_target);
    start_feedback_ramp(parameters_.feedback);
    start_mix_ramp(parameters_.mix);
    start_depth_ramp(parameters_.pan_depth);
    start_mode_ramp(parameters_.pan_mode);
}

bool StudioPanDelay::prepare() noexcept {
    if (!storage_.empty()) return true;

    constexpr std::size_t maximum_frames =
        kMaximumStorageBytes / (2U * sizeof(float));
    const double requested_frames = std::ceil(
        static_cast<double>(sample_rate_) *
        static_cast<double>(requested_max_delay_ms_) / 1000.0);
    if (!std::isfinite(requested_frames) || requested_frames < 1.0 ||
        requested_frames >=
            static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        return false;
    }

    const std::size_t requested_capacity =
        static_cast<std::size_t>(requested_frames) + 2U;
    const std::size_t capacity = std::min(requested_capacity, maximum_frames);
    if (capacity < 4U || capacity > storage_.max_size() / 2U) return false;

    try {
        storage_.assign(capacity * 2U, 0.0F);
    } catch (...) {
        storage_.clear();
        capacity_frames_ = 0U;
        return false;
    }
    capacity_frames_ = capacity;
    if (storage_.capacity() > kMaximumStorageBytes / sizeof(float)) {
        std::vector<float>().swap(storage_);
        capacity_frames_ = 0U;
        return false;
    }

    initialized_ = false;
    configure(parameters_);
    reset();
    return true;
}

void StudioPanDelay::set_delay_time_ms(float delay_time_ms) noexcept {
    parameters_.delay_time_ms = bounded(delay_time_ms,
                                        parameters_.delay_time_ms,
                                        kMinimumDelayMs, maximum_delay_ms());
    start_delay_ramp(delay_frames_for_ms(parameters_.delay_time_ms));
}

void StudioPanDelay::set_feedback(float feedback) noexcept {
    parameters_.feedback = bounded(feedback, parameters_.feedback,
                                   kMinimumFeedback, kMaximumFeedback);
    start_feedback_ramp(parameters_.feedback);
}

void StudioPanDelay::set_mix(float mix) noexcept {
    parameters_.mix = bounded(mix, parameters_.mix, 0.0F, 1.0F);
    start_mix_ramp(parameters_.mix);
}

void StudioPanDelay::set_pan_rate_hz(float rate_hz) noexcept {
    parameters_.pan_rate_hz = bounded(rate_hz, parameters_.pan_rate_hz,
                                     kMinimumRateHz, kMaximumRateHz);
    prepare_oscillator();
}

void StudioPanDelay::set_pan_depth(float depth) noexcept {
    parameters_.pan_depth = bounded(depth, parameters_.pan_depth, 0.0F, 1.0F);
    start_depth_ramp(parameters_.pan_depth);
}

void StudioPanDelay::set_pan_mode(StudioDelayPanMode mode) noexcept {
    parameters_.pan_mode = sanitize_mode(mode);
    start_mode_ramp(parameters_.pan_mode);
}

void StudioPanDelay::set_bypass(bool bypass) noexcept {
    if (!bypass && storage_.empty() && !prepare()) return;
    parameters_.bypass = bypass;
}

void StudioPanDelay::process(std::span<float> interleaved,
                             std::uint16_t channels) noexcept {
    if ((channels != 1U && channels != 2U) || interleaved.empty()) return;

    if (storage_.empty()) return;

    const std::size_t channel_count = static_cast<std::size_t>(channels);
    const std::size_t frames = interleaved.size() / channel_count;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        advance_controls();

        const std::size_t offset = frame * channel_count;
        const float dry_left = safe_signal(interleaved[offset]);
        const float dry_right = channels == 2U
            ? safe_signal(interleaved[offset + 1U])
            : dry_left;
        const float delayed_left = read_delayed(current_delay_frames_, 0U);
        const float delayed_right = channels == 2U
            ? read_delayed(current_delay_frames_, 1U)
            : delayed_left;

        const float detector = channels == 2U
            ? std::sqrt(0.5F * (dry_left * dry_left + dry_right * dry_right))
            : std::abs(dry_left);
        const float envelope_coefficient = detector > envelope_level_
            ? attack_coefficient_
            : release_coefficient_;
        envelope_level_ += envelope_coefficient * (detector - envelope_level_);
        if (!std::isfinite(envelope_level_)) envelope_level_ = 0.0F;

        float wet_left = delayed_left;
        float wet_right = delayed_right;
        if (channels == 2U) {
            const float envelope_width = std::clamp(
                1.0F - envelope_level_ / kEnvelopeFullScale, 0.0F, 1.0F);
            const float envelope_weight = mode_weights_[2U];
            const float mode_width =
                1.0F - envelope_weight * (1.0F - envelope_width);
            const float excursion = current_depth_ * mode_width;
            const float pan_position = std::clamp(
                0.5F + 0.5F * waveform_value(), 0.0F, 1.0F);
            const float mid = 0.5F * (delayed_left + delayed_right);
            const float panned_left =
                mid * (kSqrtTwo * std::sqrt(1.0F - pan_position));
            const float panned_right =
                mid * (kSqrtTwo * std::sqrt(pan_position));
            wet_left += excursion * (panned_left - wet_left);
            wet_right += excursion * (panned_right - wet_right);
        }

        if (parameters_.bypass) {
            interleaved[offset] = dry_left;
            if (channels == 2U) interleaved[offset + 1U] = dry_right;
        } else {
            interleaved[offset] = safe_signal(
                dry_left * (1.0F - current_mix_) + wet_left * current_mix_);
            if (channels == 2U) {
                interleaved[offset + 1U] = safe_signal(
                    dry_right * (1.0F - current_mix_) +
                    wet_right * current_mix_);
            }
        }

        const float input_left = parameters_.bypass ? 0.0F : dry_left;
        const float input_right = parameters_.bypass ? 0.0F : dry_right;
        const std::size_t write_offset = write_frame_ * 2U;
        storage_[write_offset] = safe_signal(
            input_left + current_feedback_ * delayed_left);
        storage_[write_offset + 1U] = safe_signal(
            input_right + current_feedback_ * delayed_right);
        if (++write_frame_ == capacity_frames_) write_frame_ = 0U;
        advance_oscillator();
    }

    const std::size_t processed_values = frames * channel_count;
    for (std::size_t index = processed_values; index < interleaved.size(); ++index) {
        interleaved[index] = safe_signal(interleaved[index]);
    }
}

void StudioPanDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0U;
    current_delay_frames_ = delay_frames_for_ms(parameters_.delay_time_ms);
    target_delay_frames_ = current_delay_frames_;
    delay_frames_step_ = 0.0F;
    delay_ramp_remaining_ = 0U;
    current_feedback_ = parameters_.feedback;
    target_feedback_ = current_feedback_;
    feedback_step_ = 0.0F;
    feedback_ramp_remaining_ = 0U;
    current_mix_ = parameters_.mix;
    target_mix_ = current_mix_;
    mix_step_ = 0.0F;
    mix_ramp_remaining_ = 0U;
    phase_ = 0.0F;
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    envelope_level_ = 0.0F;
    current_depth_ = parameters_.pan_depth;
    depth_target_ = current_depth_;
    depth_step_ = 0.0F;
    depth_ramp_remaining_ = 0U;
    mode_weights_ = mode_weights(parameters_.pan_mode);
    mode_targets_ = mode_weights_;
    mode_steps_.fill(0.0F);
    mode_ramp_remaining_ = 0U;
    normalization_counter_ = 0U;
}

float StudioPanDelay::bounded(float value, float fallback,
                              float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

StudioDelayPanMode StudioPanDelay::sanitize_mode(
    StudioDelayPanMode mode) noexcept {
    switch (mode) {
    case StudioDelayPanMode::Sine:
    case StudioDelayPanMode::Triangle:
    case StudioDelayPanMode::Envelope:
        return mode;
    }
    return StudioDelayPanMode::Sine;
}

float StudioPanDelay::safe_signal(float sample) noexcept {
    if (!std::isfinite(sample)) return 0.0F;
    return std::clamp(sample, -kMaximumSignal, kMaximumSignal);
}

std::array<float, 3U> StudioPanDelay::mode_weights(
    StudioDelayPanMode mode) noexcept {
    switch (sanitize_mode(mode)) {
    case StudioDelayPanMode::Sine:
        return {1.0F, 0.0F, 0.0F};
    case StudioDelayPanMode::Triangle:
        return {0.0F, 1.0F, 0.0F};
    case StudioDelayPanMode::Envelope:
        return {0.0F, 0.0F, 1.0F};
    }
    return {1.0F, 0.0F, 0.0F};
}

float StudioPanDelay::waveform_value() const noexcept {
    const float triangle = phase_ < 0.5F ? -1.0F + 4.0F * phase_
                                         : 3.0F - 4.0F * phase_;
    return (mode_weights_[0U] + mode_weights_[2U]) * sine_phase_ +
           mode_weights_[1U] * triangle;
}

float StudioPanDelay::delay_frames_for_ms(float milliseconds) const noexcept {
    const double requested = static_cast<double>(sample_rate_) *
                             static_cast<double>(milliseconds) / 1000.0;
    const float maximum_frames = static_cast<float>(capacity_frames_ - 2U);
    if (!std::isfinite(requested)) return maximum_frames;
    return std::clamp(static_cast<float>(requested), 1.0F, maximum_frames);
}

std::size_t StudioPanDelay::control_ramp_frames() const noexcept {
    const double requested = static_cast<double>(sample_rate_) *
                             static_cast<double>(kParameterRampMs) / 1000.0;
    return std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(requested)));
}

float StudioPanDelay::read_delayed(float delay_frames,
                                   std::size_t channel) const noexcept {
    const float maximum_delay = static_cast<float>(capacity_frames_ - 2U);
    const float bounded_delay = std::clamp(delay_frames, 1.0F, maximum_delay);
    const std::size_t whole_delay = static_cast<std::size_t>(bounded_delay);
    const float fraction = bounded_delay - static_cast<float>(whole_delay);
    const std::size_t near_frame = write_frame_ >= whole_delay
        ? write_frame_ - whole_delay
        : write_frame_ + capacity_frames_ - whole_delay;
    const std::size_t farther_delay = whole_delay + 1U;
    const std::size_t far_frame = write_frame_ >= farther_delay
        ? write_frame_ - farther_delay
        : write_frame_ + capacity_frames_ - farther_delay;
    const float nearer = storage_[near_frame * 2U + channel];
    const float farther = storage_[far_frame * 2U + channel];
    return nearer + fraction * (farther - nearer);
}

void StudioPanDelay::update_envelope_coefficients() noexcept {
    const double attack_frames =
        static_cast<double>(sample_rate_) * kEnvelopeAttackMs / 1000.0;
    const double release_frames =
        static_cast<double>(sample_rate_) * kEnvelopeReleaseMs / 1000.0;
    attack_coefficient_ = static_cast<float>(
        1.0 - std::exp(-1.0 / std::max(attack_frames, 1.0)));
    release_coefficient_ = static_cast<float>(
        1.0 - std::exp(-1.0 / std::max(release_frames, 1.0)));
}

void StudioPanDelay::prepare_oscillator() noexcept {
    effective_pan_rate_hz_ = std::clamp(parameters_.pan_rate_hz,
                                        kMinimumRateHz, kMaximumRateHz);
    phase_increment_ = effective_pan_rate_hz_ / sample_rate_;
    const float radians = kTwoPi * phase_increment_;
    sine_increment_ = std::sin(radians);
    cosine_increment_ = std::cos(radians);
}

void StudioPanDelay::start_delay_ramp(float target) noexcept {
    if (target == target_delay_frames_) return;
    target_delay_frames_ = target;
    delay_ramp_remaining_ = control_ramp_frames();
    delay_frames_step_ = (target - current_delay_frames_) /
                         static_cast<float>(delay_ramp_remaining_);
}

void StudioPanDelay::start_feedback_ramp(float target) noexcept {
    if (target == target_feedback_) return;
    target_feedback_ = target;
    feedback_ramp_remaining_ = control_ramp_frames();
    feedback_step_ = (target - current_feedback_) /
                     static_cast<float>(feedback_ramp_remaining_);
}

void StudioPanDelay::start_mix_ramp(float target) noexcept {
    if (target == target_mix_) return;
    target_mix_ = target;
    mix_ramp_remaining_ = control_ramp_frames();
    mix_step_ = (target - current_mix_) /
                static_cast<float>(mix_ramp_remaining_);
}

void StudioPanDelay::start_depth_ramp(float target) noexcept {
    if (target == depth_target_) return;
    depth_target_ = target;
    depth_ramp_remaining_ = control_ramp_frames();
    depth_step_ = (target - current_depth_) /
                  static_cast<float>(depth_ramp_remaining_);
}

void StudioPanDelay::start_mode_ramp(StudioDelayPanMode mode) noexcept {
    const auto target = mode_weights(mode);
    if (target == mode_targets_) return;
    mode_targets_ = target;
    mode_ramp_remaining_ = control_ramp_frames();
    for (std::size_t index = 0U; index < mode_weights_.size(); ++index) {
        mode_steps_[index] = (mode_targets_[index] - mode_weights_[index]) /
                             static_cast<float>(mode_ramp_remaining_);
    }
}

void StudioPanDelay::advance_controls() noexcept {
    if (delay_ramp_remaining_ > 0U) {
        current_delay_frames_ += delay_frames_step_;
        if (--delay_ramp_remaining_ == 0U) {
            current_delay_frames_ = target_delay_frames_;
            delay_frames_step_ = 0.0F;
        }
    }
    if (feedback_ramp_remaining_ > 0U) {
        current_feedback_ += feedback_step_;
        if (--feedback_ramp_remaining_ == 0U) {
            current_feedback_ = target_feedback_;
            feedback_step_ = 0.0F;
        }
    }
    if (mix_ramp_remaining_ > 0U) {
        current_mix_ += mix_step_;
        if (--mix_ramp_remaining_ == 0U) {
            current_mix_ = target_mix_;
            mix_step_ = 0.0F;
        }
    }
    if (depth_ramp_remaining_ > 0U) {
        current_depth_ += depth_step_;
        if (--depth_ramp_remaining_ == 0U) {
            current_depth_ = depth_target_;
            depth_step_ = 0.0F;
        }
    }
    if (mode_ramp_remaining_ > 0U) {
        for (std::size_t index = 0U; index < mode_weights_.size(); ++index) {
            mode_weights_[index] += mode_steps_[index];
        }
        if (--mode_ramp_remaining_ == 0U) {
            mode_weights_ = mode_targets_;
            mode_steps_.fill(0.0F);
        }
    }
}

void StudioPanDelay::advance_oscillator() noexcept {
    phase_ += phase_increment_;
    if (phase_ >= 1.0F) phase_ -= 1.0F;

    const float next_sine = sine_phase_ * cosine_increment_ +
                            cosine_phase_ * sine_increment_;
    const float next_cosine = cosine_phase_ * cosine_increment_ -
                              sine_phase_ * sine_increment_;
    sine_phase_ = next_sine;
    cosine_phase_ = next_cosine;
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
        normalization_counter_ = 0U;
    }
}

} // namespace tabletfx
