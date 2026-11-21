#include "tabletfx/effects/fet_compressor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tabletfx {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 192000.0F;
constexpr float kMinimumThresholdDb = -60.0F;
constexpr float kMaximumThresholdDb = 0.0F;
constexpr float kMinimumRatio = 1.0F;
constexpr float kMaximumRatio = 20.0F;
constexpr float kMinimumKneeDb = 0.0F;
constexpr float kMaximumKneeDb = 18.0F;
constexpr float kMinimumAttackMs = 0.01F;
constexpr float kMaximumAttackMs = 20.0F;
constexpr float kMinimumReleaseMs = 1.0F;
constexpr float kMaximumReleaseMs = 1000.0F;
constexpr float kMinimumMakeupDb = -12.0F;
constexpr float kMaximumMakeupDb = 18.0F;
constexpr float kDenormalGuard = 1.0e-20F;
// Only pathological finite float samples are capped; this is not an audio
// limiter and is far above any useful full-scale input.
constexpr float kMaximumInputMagnitude = 1.0e20F;

} // namespace

FetCompressor::FetCompressor(float sample_rate,
                             FetCompressorParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void FetCompressor::configure(float sample_rate,
                              FetCompressorParameters parameters) noexcept {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        sample_rate = 48000.0F;
    }
    sample_rate_ = sample_rate;

    parameters_.threshold_db = bounded(parameters.threshold_db, -24.0F,
                                       kMinimumThresholdDb, kMaximumThresholdDb);
    parameters_.ratio = bounded(parameters.ratio, 8.0F,
                                kMinimumRatio, kMaximumRatio);
    parameters_.knee_db = bounded(parameters.knee_db, 2.0F,
                                  kMinimumKneeDb, kMaximumKneeDb);
    parameters_.attack_ms = bounded(parameters.attack_ms, 0.1F,
                                    kMinimumAttackMs, kMaximumAttackMs);
    parameters_.release_ms = bounded(parameters.release_ms, 50.0F,
                                      kMinimumReleaseMs, kMaximumReleaseMs);
    parameters_.makeup_db = bounded(parameters.makeup_db, 0.0F,
                                    kMinimumMakeupDb, kMaximumMakeupDb);

    makeup_gain_ = decibels_to_linear(parameters_.makeup_db);
    compression_slope_ = 1.0F - 1.0F / parameters_.ratio;
    detector_attack_coefficient_ = time_coefficient(parameters_.attack_ms, sample_rate_);
    detector_release_coefficient_ = time_coefficient(parameters_.release_ms, sample_rate_);
    gain_attack_coefficient_ = detector_attack_coefficient_;
    gain_release_coefficient_ = detector_release_coefficient_;

    if (!state_initialized_) {
        gain_ = makeup_gain_;
        state_initialized_ = true;
    }
}

void FetCompressor::set_parameters(FetCompressorParameters parameters) noexcept {
    configure(sample_rate_, parameters);
}

void FetCompressor::process(std::span<float> interleaved,
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
            } else if (sample > kMaximumInputMagnitude) {
                sample = kMaximumInputMagnitude;
            } else if (sample < -kMaximumInputMagnitude) {
                sample = -kMaximumInputMagnitude;
            }
            linked_peak = std::max(linked_peak, std::abs(sample));
        }

        const float detector_coefficient = linked_peak > detector_envelope_
            ? detector_attack_coefficient_
            : detector_release_coefficient_;
        detector_envelope_ = detector_coefficient * detector_envelope_ +
                             (1.0F - detector_coefficient) * linked_peak;
        if (detector_envelope_ < kDenormalGuard) {
            detector_envelope_ = 0.0F;
        }

        const float target_gain = makeup_gain_ *
            decibels_to_linear(-gain_reduction_db(detector_envelope_));
        const float gain_coefficient = target_gain < gain_
            ? gain_attack_coefficient_
            : gain_release_coefficient_;
        gain_ = gain_coefficient * gain_ + (1.0F - gain_coefficient) * target_gain;
        if (gain_ < kDenormalGuard) {
            gain_ = 0.0F;
        }

        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const std::size_t index = offset + channel;
            const double output = static_cast<double>(interleaved[index]) *
                                  static_cast<double>(gain_);
            interleaved[index] = static_cast<float>(std::clamp(
                output,
                -static_cast<double>(std::numeric_limits<float>::max()),
                static_cast<double>(std::numeric_limits<float>::max())));
        }
    }
}

void FetCompressor::reset() noexcept {
    detector_envelope_ = 0.0F;
    gain_ = makeup_gain_;
}

float FetCompressor::bounded(float value, float fallback,
                             float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float FetCompressor::decibels_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float FetCompressor::time_coefficient(float milliseconds,
                                      float sample_rate) noexcept {
    return std::exp(-1.0F / (milliseconds * 0.001F * sample_rate));
}

float FetCompressor::gain_reduction_db(float detector_level) const noexcept {
    if (detector_level <= 0.0F || compression_slope_ <= 0.0F) {
        return 0.0F;
    }

    const float level_db = 20.0F * std::log10(detector_level);
    const float knee = parameters_.knee_db;
    const float threshold = parameters_.threshold_db;
    float reduction_db = 0.0F;

    if (knee <= 0.0F) {
        if (level_db > threshold) {
            reduction_db = compression_slope_ * (level_db - threshold);
        }
    } else {
        const float lower_knee = threshold - 0.5F * knee;
        const float upper_knee = threshold + 0.5F * knee;
        if (level_db >= upper_knee) {
            reduction_db = compression_slope_ * (level_db - threshold);
        } else if (level_db > lower_knee) {
            const float knee_position = level_db - lower_knee;
            reduction_db = compression_slope_ * knee_position * knee_position /
                           (2.0F * knee);
        }
    }
    return reduction_db;
}

} // namespace tabletfx
