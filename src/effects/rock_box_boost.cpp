#include "tabletfx/effects/rock_box_boost.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;

} // namespace

RockBoxBoost::RockBoxBoost(float sample_rate,
                           RockBoxBoostParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void RockBoxBoost::configure(float sample_rate,
                             RockBoxBoostParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    parameters_ = sanitize(parameters);
    smoothing_ms_ = parameters_.smoothing_ms;
    target_boost_gain_ = db_to_gain(parameters_.boost_db);
    target_level_gain_ = db_to_gain(parameters_.level_db);
    snap_to_targets();
}

void RockBoxBoost::set_parameters(
    RockBoxBoostParameters parameters) noexcept {
    parameters_ = sanitize(parameters);
    smoothing_ms_ = parameters_.smoothing_ms;
    target_boost_gain_ = db_to_gain(parameters_.boost_db);
    target_level_gain_ = db_to_gain(parameters_.level_db);
    retarget();
}

void RockBoxBoost::process(std::span<float> interleaved,
                           std::uint16_t channels) noexcept {
    if (channels == 0U || channels > 2U) return;

    const std::size_t channel_count = channels;
    for (std::size_t offset = 0; offset < interleaved.size();
         offset += channel_count) {
        advance_ramp();
        const std::size_t frame_channels =
            std::min(channel_count, interleaved.size() - offset);
        for (std::size_t channel = 0; channel < frame_channels; ++channel) {
            float& sample = interleaved[offset + channel];
            if (!std::isfinite(sample)) {
                sample = 0.0F;
                continue;
            }
            const float input = std::clamp(sample, -16.0F, 16.0F);
            sample = std::clamp(input * boost_gain_ * level_gain_, -8.0F, 8.0F);
        }
    }
}

void RockBoxBoost::reset() noexcept {
    snap_to_targets();
}

RockBoxBoostParameters RockBoxBoost::sanitize(
    RockBoxBoostParameters parameters) noexcept {
    const auto bounded = [](float value, float fallback, float minimum,
                            float maximum) noexcept {
        if (!std::isfinite(value)) value = fallback;
        return std::clamp(value, minimum, maximum);
    };
    parameters.boost_db = bounded(parameters.boost_db, 0.0F, 0.0F, 24.0F);
    parameters.level_db = bounded(parameters.level_db, 0.0F, -24.0F, 12.0F);
    parameters.smoothing_ms =
        bounded(parameters.smoothing_ms, 10.0F, 0.0F, 1000.0F);
    return parameters;
}

float RockBoxBoost::db_to_gain(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

std::size_t RockBoxBoost::ramp_length() const noexcept {
    const double samples = std::ceil(static_cast<double>(sample_rate_) *
                                     static_cast<double>(smoothing_ms_) / 1000.0);
    return samples <= 0.0 ? 0U : static_cast<std::size_t>(samples);
}

void RockBoxBoost::retarget() noexcept {
    const std::size_t samples = ramp_length();
    if (samples == 0U) {
        snap_to_targets();
        return;
    }

    const float divisor = static_cast<float>(samples);
    boost_gain_step_ = (target_boost_gain_ - boost_gain_) / divisor;
    level_gain_step_ = (target_level_gain_ - level_gain_) / divisor;
    ramp_remaining_ = samples;
}

void RockBoxBoost::advance_ramp() noexcept {
    if (ramp_remaining_ == 0U) return;
    boost_gain_ += boost_gain_step_;
    level_gain_ += level_gain_step_;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) snap_to_targets();
}

void RockBoxBoost::snap_to_targets() noexcept {
    boost_gain_ = target_boost_gain_;
    boost_gain_step_ = 0.0F;
    level_gain_ = target_level_gain_;
    level_gain_step_ = 0.0F;
    ramp_remaining_ = 0U;
}

} // namespace tabletfx
