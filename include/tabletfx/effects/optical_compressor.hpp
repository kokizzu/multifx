#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

// Project-authored optical-style compressor. It uses a linked RMS detector and
// a program-dependent release envelope; it does not model a specific optical
// component, circuit, or commercial compressor.
struct OpticalCompressorParameters {
    float threshold_db = -24.0F;
    float ratio = 3.0F;
    float knee_db = 6.0F;
    float attack_ms = 10.0F;
    float release_ms = 80.0F;
    float program_release_ms = 600.0F;
    float rms_window_ms = 10.0F;
    float makeup_db = 0.0F;
};

class OpticalCompressor final : public Effect {
public:
    explicit OpticalCompressor(float sample_rate = 48000.0F,
                               OpticalCompressorParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "optical-compressor";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    // Configure while preparing the graph. State is preserved when parameters
    // change; reset() clears the RMS, program, and gain envelopes.
    void configure(float sample_rate, OpticalCompressorParameters parameters) noexcept;
    void set_parameters(OpticalCompressorParameters parameters) noexcept;
    [[nodiscard]] OpticalCompressorParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kReleaseTableSteps = 33;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float decibels_to_linear(float db) noexcept;
    [[nodiscard]] static float time_coefficient(float milliseconds,
                                                float sample_rate) noexcept;
    [[nodiscard]] float gain_reduction_db(float detector_level) const noexcept;

    float sample_rate_ = 48000.0F;
    OpticalCompressorParameters parameters_{};
    float threshold_db_ = -24.0F;
    float makeup_gain_ = 1.0F;
    float compression_slope_ = 2.0F / 3.0F;
    float rms_coefficient_ = 0.0F;
    float attack_coefficient_ = 0.0F;
    float program_release_coefficient_ = 0.0F;
    float program_attack_coefficient_ = 0.0F;
    std::array<float, kReleaseTableSteps> gain_release_coefficients_{};
    double rms_power_ = 0.0;
    float program_reduction_ = 0.0F;
    float gain_ = 1.0F;
    bool state_initialized_ = false;
};

} // namespace tabletfx
