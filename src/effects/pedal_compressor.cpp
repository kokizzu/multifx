#include "tabletfx/effects/pedal_compressor.hpp"

#include <cmath>

namespace tabletfx {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 192000.0F;
constexpr float kDetectorAttackMs = 0.5F;
constexpr float kSoftKneeDb = 4.0F;
constexpr float kDenormalGuard = 1.0e-20F;
// Protect only against pathological finite float inputs. This is far beyond
// audio full scale and is not an audible output limiter.
constexpr float kMaximumInputMagnitude = 1.0e20F;

} // namespace

void PedalCompressor::configure(float sample_rate,
                                PedalCompressorParameters parameters) noexcept {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        sample_rate = 48000.0F;
    }
    sample_rate_ = sample_rate;

    parameters_.sensitivity = bounded(parameters.sensitivity, 0.45F, 0.0F, 1.0F);
    parameters_.sustain = bounded(parameters.sustain, 0.45F, 0.0F, 1.0F);
    parameters_.attack = bounded(parameters.attack, 0.25F, 0.0F, 1.0F);
    parameters_.level_db = bounded(parameters.level_db, 0.0F, -12.0F, 12.0F);

    // The sensitivity control sweeps the detector threshold from -8 dBFS to
    // -48 dBFS. Sustain controls both the ratio (1:1..10:1) and a natural pedal-
    // style release (60..560 ms); no release knob is exposed by this model.
    threshold_db_ = -8.0F - 40.0F * parameters_.sensitivity;
    const float sustain_curve = parameters_.sustain * parameters_.sustain;
    const float ratio = 1.0F + 9.0F * sustain_curve;
    compression_slope_ = 1.0F - 1.0F / ratio;
    release_ms_ = 60.0F + 500.0F * sustain_curve;

    // A squared taper gives useful fine control near fast attack settings.
    const float attack_ms = 0.5F + 79.5F * parameters_.attack * parameters_.attack;
    detector_attack_coefficient_ = time_coefficient(kDetectorAttackMs, sample_rate_);
    detector_release_coefficient_ = time_coefficient(release_ms_, sample_rate_);
    gain_attack_coefficient_ = time_coefficient(attack_ms, sample_rate_);
    gain_release_coefficient_ = time_coefficient(release_ms_, sample_rate_);
    makeup_gain_ = decibels_to_linear(parameters_.level_db);

    if (!state_initialized_) {
        gain_ = makeup_gain_;
        state_initialized_ = true;
    }
}

void PedalCompressor::set_parameters(PedalCompressorParameters parameters) noexcept {
    configure(sample_rate_, parameters);
}

void PedalCompressor::process(std::span<float> interleaved,
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

        const float detector_coefficient = linked_peak > envelope_
            ? detector_attack_coefficient_
            : detector_release_coefficient_;
        envelope_ = detector_coefficient * envelope_ +
                    (1.0F - detector_coefficient) * linked_peak;
        if (envelope_ < kDenormalGuard) {
            envelope_ = 0.0F;
        }

        const float target_gain = makeup_gain_ * compression_gain(envelope_);
        const float gain_coefficient = target_gain < gain_
            ? gain_attack_coefficient_
            : gain_release_coefficient_;
        gain_ = gain_coefficient * gain_ + (1.0F - gain_coefficient) * target_gain;
        if (gain_ < kDenormalGuard) {
            gain_ = 0.0F;
        }

        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            interleaved[offset + channel] *= gain_;
        }
    }
}

void PedalCompressor::reset() noexcept {
    envelope_ = 0.0F;
    gain_ = makeup_gain_;
}

float PedalCompressor::bounded(float value, float fallback,
                               float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float PedalCompressor::decibels_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float PedalCompressor::time_coefficient(float milliseconds,
                                        float sample_rate) noexcept {
    return std::exp(-1.0F / (milliseconds * 0.001F * sample_rate));
}

float PedalCompressor::compression_gain(float detector_level) const noexcept {
    if (detector_level <= 0.0F || compression_slope_ <= 0.0F) {
        return 1.0F;
    }

    const float level_db = 20.0F * std::log10(detector_level);
    const float lower_knee = threshold_db_ - 0.5F * kSoftKneeDb;
    const float upper_knee = threshold_db_ + 0.5F * kSoftKneeDb;
    float gain_reduction_db = 0.0F;

    if (level_db >= upper_knee) {
        gain_reduction_db = -compression_slope_ * (level_db - threshold_db_);
    } else if (level_db > lower_knee) {
        const float knee_position = level_db - lower_knee;
        gain_reduction_db = -compression_slope_ * knee_position * knee_position /
                            (2.0F * kSoftKneeDb);
    }
    return decibels_to_linear(gain_reduction_db);
}

} // namespace tabletfx
