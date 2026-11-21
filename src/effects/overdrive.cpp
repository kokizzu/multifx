#include "tabletfx/effects/overdrive.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {

TransparentOverdrive::TransparentOverdrive(float sample_rate,
                                           TransparentOverdriveParameters parameters) noexcept {
    configure_smoothing(sample_rate, parameters.smoothing_ms);
    target_drive_ = bounded_drive(parameters.drive);
    target_level_db_ = bounded_level(parameters.level_db);
    target_level_gain_ = db_to_gain(target_level_db_);
    target_mix_ = bounded_mix(parameters.mix);
    reset();
}

void TransparentOverdrive::configure(TransparentOverdriveParameters parameters) noexcept {
    configure_smoothing(sample_rate_, parameters.smoothing_ms);
    target_drive_ = bounded_drive(parameters.drive);
    target_level_db_ = bounded_level(parameters.level_db);
    target_level_gain_ = db_to_gain(target_level_db_);
    target_mix_ = bounded_mix(parameters.mix);
    retarget();
}

void TransparentOverdrive::set_drive(float drive) noexcept {
    target_drive_ = bounded_drive(drive);
    retarget();
}

void TransparentOverdrive::set_level_db(float level_db) noexcept {
    target_level_db_ = bounded_level(level_db);
    target_level_gain_ = db_to_gain(target_level_db_);
    retarget();
}

void TransparentOverdrive::set_mix(float mix) noexcept {
    target_mix_ = bounded_mix(mix);
    retarget();
}

void TransparentOverdrive::configure_smoothing(float sample_rate, float smoothing_ms) noexcept {
    sample_rate_ = std::isfinite(sample_rate) && sample_rate >= 8000.0F &&
                           sample_rate <= 384000.0F
                       ? sample_rate
                       : 48000.0F;
    smoothing_ms_ = std::isfinite(smoothing_ms)
                        ? std::clamp(smoothing_ms, 0.0F, 1000.0F)
                        : 10.0F;
}

void TransparentOverdrive::process(std::span<float> interleaved,
                                   std::uint16_t channels) noexcept {
    if (channels == 0) return;
    const std::size_t channel_count = channels;
    for (std::size_t offset = 0; offset < interleaved.size(); offset += channel_count) {
        advance_ramp();
        const float pre_gain = 1.0F + 6.0F * drive_;
        const std::size_t frame_channels = std::min(channel_count, interleaved.size() - offset);
        for (std::size_t channel = 0; channel < frame_channels; ++channel) {
            float& sample = interleaved[offset + channel];
            if (!std::isfinite(sample)) {
                sample = 0.0F;
                continue;
            }
            const float saturated = soft_clip(sample * pre_gain);
            sample = (sample + (saturated - sample) * mix_) * level_gain_;
        }
    }
}

void TransparentOverdrive::reset() noexcept {
    drive_ = target_drive_;
    level_gain_ = target_level_gain_;
    mix_ = target_mix_;
    drive_step_ = 0.0F;
    level_step_ = 0.0F;
    mix_step_ = 0.0F;
    ramp_remaining_ = 0;
}

void TransparentOverdrive::retarget() noexcept {
    const double requested = std::ceil(static_cast<double>(sample_rate_) *
                                       static_cast<double>(smoothing_ms_) / 1000.0);
    const std::size_t samples = requested >= 1.0 ? static_cast<std::size_t>(requested) : 0;
    if (samples == 0) {
        reset();
        return;
    }
    ramp_remaining_ = samples;
    const float count = static_cast<float>(samples);
    drive_step_ = (target_drive_ - drive_) / count;
    level_step_ = (target_level_gain_ - level_gain_) / count;
    mix_step_ = (target_mix_ - mix_) / count;
}

void TransparentOverdrive::advance_ramp() noexcept {
    if (ramp_remaining_ == 0) return;
    drive_ += drive_step_;
    level_gain_ += level_step_;
    mix_ += mix_step_;
    --ramp_remaining_;
    if (ramp_remaining_ == 0) {
        drive_ = target_drive_;
        level_gain_ = target_level_gain_;
        mix_ = target_mix_;
        drive_step_ = 0.0F;
        level_step_ = 0.0F;
        mix_step_ = 0.0F;
    }
}

float TransparentOverdrive::bounded_drive(float drive) noexcept {
    return std::isfinite(drive) ? std::clamp(drive, 0.0F, 0.5F) : 0.15F;
}

float TransparentOverdrive::bounded_level(float level_db) noexcept {
    return std::isfinite(level_db) ? std::clamp(level_db, -24.0F, 12.0F) : -3.0F;
}

float TransparentOverdrive::bounded_mix(float mix) noexcept {
    return std::isfinite(mix) ? std::clamp(mix, 0.0F, 1.0F) : 0.35F;
}

float TransparentOverdrive::db_to_gain(float level_db) noexcept {
    return std::pow(10.0F, level_db / 20.0F);
}

float TransparentOverdrive::soft_clip(float sample) noexcept {
    const float magnitude = std::abs(sample);
    if (magnitude >= 3.0F) return std::copysign(1.0F, sample);
    const float squared = sample * sample;
    return sample * (27.0F + squared) / (27.0F + 9.0F * squared);
}

} // namespace tabletfx
