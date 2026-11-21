#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

// Project-authored fast, peak-detect compressor characterization. It is a
// feed-forward DSP design, not an emulation of a particular FET circuit or
// commercial compressor.
struct FetCompressorParameters {
    float threshold_db = -24.0F;
    float ratio = 8.0F;
    float knee_db = 2.0F;
    float attack_ms = 0.1F;
    float release_ms = 50.0F;
    float makeup_db = 0.0F;
};

class FetCompressor final : public Effect {
public:
    explicit FetCompressor(float sample_rate = 48000.0F,
                           FetCompressorParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "fet-compressor";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    // Configure while preparing the graph. Parameter changes preserve envelope
    // state; reset() clears the peak detector and gain envelope.
    void configure(float sample_rate, FetCompressorParameters parameters) noexcept;
    void set_parameters(FetCompressorParameters parameters) noexcept;
    [[nodiscard]] FetCompressorParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float decibels_to_linear(float db) noexcept;
    [[nodiscard]] static float time_coefficient(float milliseconds,
                                                float sample_rate) noexcept;
    [[nodiscard]] float gain_reduction_db(float detector_level) const noexcept;

    float sample_rate_ = 48000.0F;
    FetCompressorParameters parameters_{};
    float makeup_gain_ = 1.0F;
    float compression_slope_ = 0.875F;
    float detector_attack_coefficient_ = 0.0F;
    float detector_release_coefficient_ = 0.0F;
    float gain_attack_coefficient_ = 0.0F;
    float gain_release_coefficient_ = 0.0F;
    float detector_envelope_ = 0.0F;
    float gain_ = 1.0F;
    bool state_initialized_ = false;
};

} // namespace tabletfx
