#include "tabletfx/effects/rock_box_dist.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kMinimumToneHz = 300.0F;
constexpr float kMaximumToneHz = 12000.0F;
constexpr float kDriveGainRange = 11.0F;

} // namespace

RockBoxDistortion::RockBoxDistortion(
    float sample_rate, RockBoxDistortionParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void RockBoxDistortion::configure(
    float sample_rate, RockBoxDistortionParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    parameters_ = sanitize(parameters);
    smoothing_ms_ = parameters_.smoothing_ms;
    target_drive_gain_ = 1.0F + kDriveGainRange * parameters_.drive;
    target_tone_alpha_ = tone_alpha(parameters_.tone);
    target_level_gain_ = db_to_gain(parameters_.level_db);
    snap_to_targets();
    reset();
}

void RockBoxDistortion::set_parameters(
    RockBoxDistortionParameters parameters) noexcept {
    parameters_ = sanitize(parameters);
    smoothing_ms_ = parameters_.smoothing_ms;
    target_drive_gain_ = 1.0F + kDriveGainRange * parameters_.drive;
    target_tone_alpha_ = tone_alpha(parameters_.tone);
    target_level_gain_ = db_to_gain(parameters_.level_db);
    retarget();
}

void RockBoxDistortion::process(std::span<float> interleaved,
                                std::uint16_t channels) noexcept {
    if (channels == 0U || channels > channel_state_.size()) return;

    const std::size_t channel_count = channels;
    for (std::size_t offset = 0; offset < interleaved.size();
         offset += channel_count) {
        advance_ramp();
        const std::size_t frame_channels =
            std::min(channel_count, interleaved.size() - offset);
        for (std::size_t channel = 0; channel < frame_channels; ++channel) {
            float& sample = interleaved[offset + channel];
            ChannelState& state = channel_state_[channel];
            if (!std::isfinite(sample)) {
                sample = 0.0F;
                state = {};
                continue;
            }

            const float input = std::clamp(sample, -16.0F, 16.0F);
            const float driven = soft_clip(input * drive_gain_);
            state.tone_low += tone_alpha_ * (driven - state.tone_low);
            sample = std::clamp(state.tone_low * level_gain_, -8.0F, 8.0F);
        }
    }
}

void RockBoxDistortion::reset() noexcept {
    channel_state_.fill({});
    snap_to_targets();
    ramp_remaining_ = 0U;
}

RockBoxDistortionParameters RockBoxDistortion::sanitize(
    RockBoxDistortionParameters parameters) noexcept {
    const auto bounded = [](float value, float fallback, float minimum,
                            float maximum) noexcept {
        if (!std::isfinite(value)) value = fallback;
        return std::clamp(value, minimum, maximum);
    };
    parameters.drive = bounded(parameters.drive, 0.5F, 0.0F, 1.0F);
    parameters.tone = bounded(parameters.tone, 0.5F, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, 0.0F, -24.0F, 12.0F);
    parameters.smoothing_ms =
        bounded(parameters.smoothing_ms, 10.0F, 0.0F, 1000.0F);
    return parameters;
}

float RockBoxDistortion::db_to_gain(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float RockBoxDistortion::soft_clip(float sample) noexcept {
    const float magnitude = std::abs(sample);
    if (magnitude >= 3.0F) return std::copysign(1.0F, sample);
    const float squared = sample * sample;
    return sample * (27.0F + squared) / (27.0F + 9.0F * squared);
}

float RockBoxDistortion::low_pass_alpha(float cutoff_hz,
                                        float sample_rate) noexcept {
    const float cutoff = std::clamp(cutoff_hz, 10.0F,
                                    std::min(18000.0F, sample_rate * 0.45F));
    return 1.0F - std::exp(-2.0F * kPi * cutoff / sample_rate);
}

float RockBoxDistortion::tone_alpha(float tone) const noexcept {
    const float cutoff_hz = kMinimumToneHz +
                            (kMaximumToneHz - kMinimumToneHz) * tone;
    return low_pass_alpha(cutoff_hz, sample_rate_);
}

std::size_t RockBoxDistortion::ramp_length() const noexcept {
    const double samples = std::ceil(static_cast<double>(sample_rate_) *
                                     static_cast<double>(smoothing_ms_) / 1000.0);
    return samples <= 0.0 ? 0U : static_cast<std::size_t>(samples);
}

void RockBoxDistortion::retarget() noexcept {
    const std::size_t samples = ramp_length();
    if (samples == 0U) {
        snap_to_targets();
        return;
    }

    const float divisor = static_cast<float>(samples);
    drive_gain_step_ = (target_drive_gain_ - drive_gain_) / divisor;
    tone_alpha_step_ = (target_tone_alpha_ - tone_alpha_) / divisor;
    level_gain_step_ = (target_level_gain_ - level_gain_) / divisor;
    ramp_remaining_ = samples;
}

void RockBoxDistortion::advance_ramp() noexcept {
    if (ramp_remaining_ == 0U) return;
    drive_gain_ += drive_gain_step_;
    tone_alpha_ += tone_alpha_step_;
    level_gain_ += level_gain_step_;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) snap_to_targets();
}

void RockBoxDistortion::snap_to_targets() noexcept {
    drive_gain_ = target_drive_gain_;
    drive_gain_step_ = 0.0F;
    tone_alpha_ = target_tone_alpha_;
    tone_alpha_step_ = 0.0F;
    level_gain_ = target_level_gain_;
    level_gain_step_ = 0.0F;
    ramp_remaining_ = 0U;
}

} // namespace tabletfx
