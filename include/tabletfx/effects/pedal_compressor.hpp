#pragma once

#include "tabletfx/effect.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

// Four-control guitar pedal compressor. Sensitivity sets the input threshold;
// Sustain sets compression strength and the automatic release time; Attack sets
// gain-reduction response time; Level is the post-compressor output level.
struct PedalCompressorParameters {
    float sensitivity = 0.45F; // normalized 0..1; higher means more compression
    float sustain = 0.45F;     // normalized 0..1; higher means more compression
    float attack = 0.25F;      // normalized 0..1; higher means slower attack
    float level_db = 0.0F;     // -12..+12 dB
};

class PedalCompressor final : public Effect {
public:
    explicit PedalCompressor(float sample_rate = 48000.0F,
                              PedalCompressorParameters parameters = {}) noexcept {
        configure(sample_rate, parameters);
    }

    [[nodiscard]] std::string_view id() const noexcept override {
        return "pedal-compressor";
    }

    // Call during graph preparation, outside the audio callback. This processor
    // has no heap-owned state, so each instance's complete storage is sizeof(*this).
    void configure(float sample_rate, PedalCompressorParameters parameters) noexcept;
    void set_parameters(PedalCompressorParameters parameters) noexcept;
    [[nodiscard]] PedalCompressorParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float decibels_to_linear(float db) noexcept;
    [[nodiscard]] static float time_coefficient(float milliseconds,
                                                float sample_rate) noexcept;
    [[nodiscard]] float compression_gain(float detector_level) const noexcept;

    float sample_rate_ = 48000.0F;
    PedalCompressorParameters parameters_{};
    float threshold_db_ = -26.0F;
    float release_ms_ = 161.25F;
    float makeup_gain_ = 1.0F;
    float detector_attack_coefficient_ = 0.0F;
    float detector_release_coefficient_ = 0.0F;
    float gain_attack_coefficient_ = 0.0F;
    float gain_release_coefficient_ = 0.0F;
    float compression_slope_ = 0.0F;
    float envelope_ = 0.0F;
    float gain_ = 1.0F;
    bool state_initialized_ = false;
};

} // namespace tabletfx
