#include "tabletfx/effects/round_fuzz.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tabletfx {

RoundFuzz::RoundFuzz(float sample_rate, RoundFuzzParameters parameters) noexcept {
    configure_smoothing(sample_rate, parameters.smoothing_ms);
    target_fuzz_ = bounded_fuzz(parameters.fuzz);
    target_level_db_ = bounded_level(parameters.level_db);
    target_level_gain_ = db_to_gain(target_level_db_);
    target_mix_ = bounded_mix(parameters.mix);
    reset();
}

void RoundFuzz::configure(RoundFuzzParameters parameters) noexcept {
    configure_smoothing(sample_rate_, parameters.smoothing_ms);
    target_fuzz_ = bounded_fuzz(parameters.fuzz);
    target_level_db_ = bounded_level(parameters.level_db);
    target_level_gain_ = db_to_gain(target_level_db_);
    target_mix_ = bounded_mix(parameters.mix);
    retarget();
}

void RoundFuzz::set_fuzz(float fuzz) noexcept {
    target_fuzz_ = bounded_fuzz(fuzz);
    retarget();
}

void RoundFuzz::set_level_db(float level_db) noexcept {
    target_level_db_ = bounded_level(level_db);
    target_level_gain_ = db_to_gain(target_level_db_);
    retarget();
}

void RoundFuzz::set_mix(float mix) noexcept {
    target_mix_ = bounded_mix(mix);
    retarget();
}

void RoundFuzz::configure_smoothing(float sample_rate, float smoothing_ms) noexcept {
    sample_rate_ = std::isfinite(sample_rate) && sample_rate >= 8000.0F &&
                           sample_rate <= 384000.0F
                       ? sample_rate
                       : 48000.0F;
    smoothing_ms_ = std::isfinite(smoothing_ms)
                        ? std::clamp(smoothing_ms, 0.0F, 1000.0F)
                        : 10.0F;
}

void RoundFuzz::process(std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels == 0) return;

    const std::size_t channel_count = channels;
    const float maximum = std::numeric_limits<float>::max();
    for (std::size_t offset = 0; offset < interleaved.size(); offset += channel_count) {
        advance_ramp();
        const float pre_gain = 1.0F + 31.0F * fuzz_;
        const float saturation_input = 3.0F / pre_gain;
        const std::size_t frame_channels =
            std::min(channel_count, interleaved.size() - offset);
        for (std::size_t channel = 0; channel < frame_channels; ++channel) {
            float& sample = interleaved[offset + channel];
            if (!std::isfinite(sample)) {
                sample = 0.0F;
                continue;
            }

            const float magnitude = std::abs(sample);
            const float saturated = magnitude >= saturation_input
                                       ? std::copysign(1.0F, sample)
                                       : soft_clip(sample * pre_gain);
            float blended = mix_ <= 0.0F
                                ? sample
                                : (mix_ >= 1.0F
                                       ? saturated
                                       : sample + (saturated - sample) * mix_);
            if (!std::isfinite(blended)) {
                sample = 0.0F;
                continue;
            }

            if (level_gain_ > 1.0F) {
                const float largest_safe_input = maximum / level_gain_;
                if (std::abs(blended) > largest_safe_input) {
                    blended = std::copysign(largest_safe_input, blended);
                }
            }
            float output = blended * level_gain_;
            if (!std::isfinite(output)) {
                output = std::copysign(maximum, blended);
            }
            sample = output;
        }
    }
}

void RoundFuzz::reset() noexcept {
    fuzz_ = target_fuzz_;
    level_gain_ = target_level_gain_;
    mix_ = target_mix_;
    fuzz_step_ = 0.0F;
    level_step_ = 0.0F;
    mix_step_ = 0.0F;
    ramp_remaining_ = 0;
}

void RoundFuzz::retarget() noexcept {
    const double requested = std::ceil(static_cast<double>(sample_rate_) *
                                       static_cast<double>(smoothing_ms_) / 1000.0);
    const std::size_t samples = requested >= 1.0 ? static_cast<std::size_t>(requested) : 0;
    if (samples == 0) {
        reset();
        return;
    }

    ramp_remaining_ = samples;
    const float count = static_cast<float>(samples);
    fuzz_step_ = (target_fuzz_ - fuzz_) / count;
    level_step_ = (target_level_gain_ - level_gain_) / count;
    mix_step_ = (target_mix_ - mix_) / count;
}

void RoundFuzz::advance_ramp() noexcept {
    if (ramp_remaining_ == 0) return;
    fuzz_ += fuzz_step_;
    level_gain_ += level_step_;
    mix_ += mix_step_;
    --ramp_remaining_;
    if (ramp_remaining_ == 0) {
        fuzz_ = target_fuzz_;
        level_gain_ = target_level_gain_;
        mix_ = target_mix_;
        fuzz_step_ = 0.0F;
        level_step_ = 0.0F;
        mix_step_ = 0.0F;
    }
}

float RoundFuzz::bounded_fuzz(float fuzz) noexcept {
    return std::isfinite(fuzz) ? std::clamp(fuzz, 0.0F, 1.0F) : 0.55F;
}

float RoundFuzz::bounded_level(float level_db) noexcept {
    return std::isfinite(level_db) ? std::clamp(level_db, -24.0F, 12.0F) : -6.0F;
}

float RoundFuzz::bounded_mix(float mix) noexcept {
    return std::isfinite(mix) ? std::clamp(mix, 0.0F, 1.0F) : 0.85F;
}

float RoundFuzz::db_to_gain(float level_db) noexcept {
    return std::pow(10.0F, level_db / 20.0F);
}

float RoundFuzz::soft_clip(float sample) noexcept {
    const float magnitude = std::abs(sample);
    if (magnitude >= 3.0F) return std::copysign(1.0F, sample);
    const float squared = sample * sample;
    return sample * (27.0F + squared) / (27.0F + 9.0F * squared);
}

} // namespace tabletfx
