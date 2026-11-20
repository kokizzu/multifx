#include "tabletfx/effects/peak_limiter.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 192000.0F;
constexpr float kMinimumCeilingDb = -24.0F;
constexpr float kMaximumCeilingDb = 0.0F;
constexpr float kMinimumReleaseMs = 5.0F;
constexpr float kMaximumReleaseMs = 1000.0F;

float bounded(float value, float fallback, float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float decibels_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

} // namespace

PeakLimiter::PeakLimiter(float sample_rate, float ceiling_db,
                         float release_ms) noexcept {
    configure(sample_rate, ceiling_db, release_ms);
}

void PeakLimiter::configure(float sample_rate, float ceiling_db,
                            float release_ms) noexcept {
    sample_rate_ = bounded(sample_rate, kDefaultSampleRate,
                           kMinimumSampleRate, kMaximumSampleRate);
    ceiling_db_ = bounded(ceiling_db, kDefaultCeilingDb,
                          kMinimumCeilingDb, kMaximumCeilingDb);
    release_ms_ = bounded(release_ms, kDefaultReleaseMs,
                          kMinimumReleaseMs, kMaximumReleaseMs);
    ceiling_linear_ = decibels_to_linear(ceiling_db_);
    release_coefficient_ = std::exp(-1.0F /
        (release_ms_ * 0.001F * sample_rate_));
    gain_ = 1.0F;
}

void PeakLimiter::set_ceiling_db(float ceiling_db) noexcept {
    ceiling_db_ = bounded(ceiling_db, kDefaultCeilingDb,
                          kMinimumCeilingDb, kMaximumCeilingDb);
    ceiling_linear_ = decibels_to_linear(ceiling_db_);
}

void PeakLimiter::set_release_ms(float release_ms) noexcept {
    release_ms_ = bounded(release_ms, kDefaultReleaseMs,
                          kMinimumReleaseMs, kMaximumReleaseMs);
    release_coefficient_ = std::exp(-1.0F /
        (release_ms_ * 0.001F * sample_rate_));
}

void PeakLimiter::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        float linked_peak = 0.0F;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            if (!std::isfinite(sample)) {
                sample = 0.0F;
            }
            linked_peak = std::max(linked_peak, std::abs(sample));
        }

        const float target_gain = linked_peak > ceiling_linear_
            ? ceiling_linear_ / linked_peak
            : 1.0F;

        // Attack is immediate, keeping this processor at zero lookahead latency.
        // Recovery is smoothed to avoid pumping after isolated peaks.
        if (target_gain < gain_) {
            gain_ = target_gain;
        } else {
            gain_ = release_coefficient_ * gain_ +
                    (1.0F - release_coefficient_) * target_gain;
        }

        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            sample = std::clamp(sample * gain_, -ceiling_linear_, ceiling_linear_);
        }
    }
}

void PeakLimiter::reset() noexcept {
    gain_ = 1.0F;
}

} // namespace tabletfx
