#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

// Project-authored control mapping for a lightweight Essex Vibe-inspired
// modulation effect. The official Helix Stadium 1.4 notes document Rate,
// Depth, Trem, Stages (1 or 2), stereo-only Spread, and Level, but do not
// publish numeric bounds/defaults or the DSP topology. This implementation
// uses a modulated first-order all-pass network mixed with dry signal; it is
// not a Line 6 sound/DSP replica.
struct EssexVibeParameters {
    float rate_hz = 0.8F;
    float depth = 0.65F;
    float trem = 0.0F;
    std::uint8_t stages = 2U;
    float spread = 0.0F;
    // Project-authored linear output gain; 1.0 is unity.
    float level = 1.0F;
};

class EssexVibe final : public Effect {
public:
    explicit EssexVibe(float sample_rate = 48000.0F,
                       EssexVibeParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "essex-vibe";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    // Reconfiguration rebuilds the bounded coefficient table and clears its
    // small filter history. Invoke before audio processing or at a safe block
    // boundary; process() itself performs no allocation.
    void configure(float sample_rate, EssexVibeParameters parameters) noexcept;

    [[nodiscard]] EssexVibeParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset_modulation_phase() noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kCoefficientCount = 256U;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumRateHz = 0.05F;
    static constexpr float kMaximumRateHz = 12.0F;
    static constexpr float kMinimumCenterHz = 120.0F;
    static constexpr float kMaximumSweepHz = 6000.0F;
    static constexpr float kMaximumLevel = 2.0F;

    struct AllPassState {
        float previous_input = 0.0F;
        float previous_output = 0.0F;
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] float coefficient_for(float lfo_value) const noexcept;
    void prepare_coefficients() noexcept;
    void prepare_oscillator() noexcept;

    float sample_rate_ = 48000.0F;
    EssexVibeParameters parameters_{};
    std::array<float, kCoefficientCount> coefficient_table_{};
    std::array<std::array<AllPassState, 2U>, 2U> state_{};
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    float sine_increment_ = 0.0F;
    float cosine_increment_ = 1.0F;
    float spread_sine_ = 0.0F;
    float spread_cosine_ = 1.0F;
    std::uint32_t normalization_counter_ = 0U;
};

} // namespace tabletfx
