#include "tabletfx/effects/dimension_chorus.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {

DimensionChorus::DimensionChorus(float sample_rate, std::uint16_t channels,
                                 DimensionChorusParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument(
            "DimensionChorus requires 8–384 kHz and mono or stereo channels");
    }

    const double frames = std::ceil(static_cast<double>(sample_rate_) *
                                    static_cast<double>(kStorageDelayMs) / 1000.0);
    if (frames > static_cast<double>(std::numeric_limits<std::size_t>::max() - 2)) {
        throw std::length_error("DimensionChorus delay buffer is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(frames) + 2;
    storage_.assign(capacity_frames_ * channels_, 0.0F);
    set_parameters(parameters);
    retarget(true);
}

void DimensionChorus::set_parameters(
    DimensionChorusParameters parameters) noexcept {
    parameters.version = sanitize_version(parameters.version);
    parameters.mode = sanitize_mode(parameters.mode);
    parameters.mix = bounded(parameters.mix, 0.5F, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, 0.0F, -24.0F, 12.0F);
    parameters_ = parameters;

    const ModeProfile profile = profile_for(parameters_.mode);
    target_base_delay_ms_ = profile.base_delay_ms;
    target_depth_ms_ = profile.depth_ms;
    target_mix_ = parameters_.mix;
    target_level_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    target_version_blend_ = parameters_.version == DimensionChorusVersion::D
                                ? 1.0F
                                : 0.0F;

    const float step = kTwoPi * profile.rate_hz / sample_rate_;
    target_sine_increment_ = std::sin(step);
    target_cosine_increment_ = std::cos(step);
    retarget(false);
}

void DimensionChorus::process(std::span<float> interleaved,
                              std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    const float samples_per_ms = sample_rate_ * 0.001F;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (ramp_remaining_ > 0) {
            current_base_delay_ms_ += base_delay_step_;
            current_depth_ms_ += depth_step_;
            current_mix_ += mix_step_;
            current_level_gain_ += level_gain_step_;
            current_version_blend_ += version_blend_step_;
            current_sine_increment_ += sine_increment_step_;
            current_cosine_increment_ += cosine_increment_step_;
            --ramp_remaining_;
            if (ramp_remaining_ == 0) {
                current_base_delay_ms_ = target_base_delay_ms_;
                current_depth_ms_ = target_depth_ms_;
                current_mix_ = target_mix_;
                current_level_gain_ = target_level_gain_;
                current_version_blend_ = target_version_blend_;
                current_sine_increment_ = target_sine_increment_;
                current_cosine_increment_ = target_cosine_increment_;
                base_delay_step_ = 0.0F;
                depth_step_ = 0.0F;
                mix_step_ = 0.0F;
                level_gain_step_ = 0.0F;
                version_blend_step_ = 0.0F;
                sine_increment_step_ = 0.0F;
                cosine_increment_step_ = 0.0F;
            }
        }

        const std::size_t write_offset = write_frame_ * channels_;
        const std::size_t input_offset = frame * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = input_offset + channel;
            const float input = std::isfinite(interleaved[index])
                                    ? interleaved[index]
                                    : 0.0F;
            storage_[write_offset + channel] = input;

            float modulation = sine_phase_ * (1.0F - current_version_blend_) +
                               cosine_phase_ * current_version_blend_;
            if (channel == 1) modulation = -modulation;
            const float delay = std::clamp(
                (current_base_delay_ms_ + current_depth_ms_ * modulation) *
                    samples_per_ms,
                1.0F, static_cast<float>(capacity_frames_ - 2));
            const std::size_t whole_delay = static_cast<std::size_t>(delay);
            const float fraction = delay - static_cast<float>(whole_delay);
            std::size_t read_frame = write_frame_ + capacity_frames_ - whole_delay;
            if (read_frame >= capacity_frames_) read_frame -= capacity_frames_;
            const std::size_t older_frame = read_frame == 0
                                                ? capacity_frames_ - 1
                                                : read_frame - 1;
            const float newer = storage_[read_frame * channels_ + channel];
            const float older = storage_[older_frame * channels_ + channel];
            const float wet = newer * (1.0F - fraction) + older * fraction;
            const float mixed = input * (1.0F - current_mix_) + wet * current_mix_;
            const float output = mixed * current_level_gain_;
            interleaved[index] = std::isfinite(output) ? output : 0.0F;
        }

        ++write_frame_;
        if (write_frame_ == capacity_frames_) write_frame_ = 0;
        advance_modulator();
    }
}

void DimensionChorus::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    normalization_counter_ = 0;
    retarget(true);
}

float DimensionChorus::bounded(float value, float fallback,
                               float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

DimensionChorusVersion DimensionChorus::sanitize_version(
    DimensionChorusVersion version) noexcept {
    switch (version) {
    case DimensionChorusVersion::C:
    case DimensionChorusVersion::D:
        return version;
    }
    return DimensionChorusVersion::C;
}

DimensionChorusMode DimensionChorus::sanitize_mode(
    DimensionChorusMode mode) noexcept {
    switch (mode) {
    case DimensionChorusMode::Mode1:
    case DimensionChorusMode::Mode2:
    case DimensionChorusMode::Mode3:
    case DimensionChorusMode::Mode4:
    case DimensionChorusMode::Modes12:
    case DimensionChorusMode::Modes13:
    case DimensionChorusMode::Modes14:
    case DimensionChorusMode::Modes23:
    case DimensionChorusMode::Modes24:
    case DimensionChorusMode::Modes34:
        return mode;
    }
    return DimensionChorusMode::Mode1;
}

DimensionChorus::ModeProfile DimensionChorus::profile_for(
    DimensionChorusMode mode) noexcept {
    // Rates, delay centers, depths, and the meanings of these profiles are
    // project-authored; the manufacturer documents labels, not DSP settings.
    switch (mode) {
    case DimensionChorusMode::Mode1:
        return {12.0F, 0.5F, 0.16F};
    case DimensionChorusMode::Mode2:
        return {14.0F, 1.2F, 0.24F};
    case DimensionChorusMode::Mode3:
        return {16.0F, 2.1F, 0.32F};
    case DimensionChorusMode::Mode4:
        return {18.0F, 3.0F, 0.42F};
    case DimensionChorusMode::Modes12:
        return {13.0F, 1.8F, 0.25F};
    case DimensionChorusMode::Modes13:
        return {15.0F, 2.2F, 0.35F};
    case DimensionChorusMode::Modes14:
        return {16.0F, 2.8F, 0.42F};
    case DimensionChorusMode::Modes23:
        return {16.0F, 2.5F, 0.38F};
    case DimensionChorusMode::Modes24:
        return {17.0F, 3.1F, 0.48F};
    case DimensionChorusMode::Modes34:
        return {18.0F, 3.8F, 0.60F};
    }
    return {12.0F, 0.5F, 0.16F};
}

void DimensionChorus::retarget(bool immediate) noexcept {
    if (immediate) {
        current_base_delay_ms_ = target_base_delay_ms_;
        current_depth_ms_ = target_depth_ms_;
        current_mix_ = target_mix_;
        current_level_gain_ = target_level_gain_;
        current_version_blend_ = target_version_blend_;
        current_sine_increment_ = target_sine_increment_;
        current_cosine_increment_ = target_cosine_increment_;
        base_delay_step_ = 0.0F;
        depth_step_ = 0.0F;
        mix_step_ = 0.0F;
        level_gain_step_ = 0.0F;
        version_blend_step_ = 0.0F;
        sine_increment_step_ = 0.0F;
        cosine_increment_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }

    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(kParameterSmoothingMs) / 1000.0;
    ramp_remaining_ = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_remaining_ == 0) {
        retarget(true);
        return;
    }
    const float denominator = static_cast<float>(ramp_remaining_);
    base_delay_step_ = (target_base_delay_ms_ - current_base_delay_ms_) / denominator;
    depth_step_ = (target_depth_ms_ - current_depth_ms_) / denominator;
    mix_step_ = (target_mix_ - current_mix_) / denominator;
    level_gain_step_ = (target_level_gain_ - current_level_gain_) / denominator;
    version_blend_step_ =
        (target_version_blend_ - current_version_blend_) / denominator;
    sine_increment_step_ =
        (target_sine_increment_ - current_sine_increment_) / denominator;
    cosine_increment_step_ =
        (target_cosine_increment_ - current_cosine_increment_) / denominator;
}

void DimensionChorus::advance_modulator() noexcept {
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
