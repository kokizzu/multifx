#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class CrossoverFrequencyMultiplier : std::uint8_t {
    x1,
    x10,
};

// Project-authored stereo two-way crossover. The FM9-family Blocks Guide
// documents an LR4 high/low split, a frequency multiplier, per-band/channel
// levels, and per-band/channel pans. It does not provide these numeric bounds,
// defaults, or a parameter-smoothing policy; those are selected by TabletFX.
struct StereoCrossoverParameters {
    float frequency_hz = 500.0F;
    CrossoverFrequencyMultiplier frequency_multiplier =
        CrossoverFrequencyMultiplier::x1;
    float low_level_left_db = 0.0F;
    float low_level_right_db = 0.0F;
    float high_level_left_db = 0.0F;
    float high_level_right_db = 0.0F;
    float low_pan_left = -1.0F;
    float high_pan_left = -1.0F;
    float low_pan_right = 1.0F;
    float high_pan_right = 1.0F;
    float level_db = 0.0F;
    float balance = 0.0F;
};

class StereoCrossover final : public Effect {
public:
    explicit StereoCrossover(
        float sample_rate = 48000.0F,
        StereoCrossoverParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "stereo-crossover";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   StereoCrossoverParameters parameters) noexcept;
    void configure(StereoCrossoverParameters parameters) noexcept;

    [[nodiscard]] const StereoCrossoverParameters& parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_frequency_hz() const noexcept {
        return effective_frequency_hz_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumFrequencyHz = 20.0F;
    static constexpr float kMaximumBaseFrequencyHz = 2000.0F;
    static constexpr float kMinimumBandLevelDb = -60.0F;
    static constexpr float kMaximumBandLevelDb = 12.0F;
    static constexpr float kMaximumSample = 1000000.0F;

    struct BiquadCoefficients {
        float b0 = 1.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    struct BiquadState {
        float z1 = 0.0F;
        float z2 = 0.0F;

        [[nodiscard]] float process(float input,
                                    const BiquadCoefficients& coefficients) noexcept;
        void reset() noexcept {
            z1 = 0.0F;
            z2 = 0.0F;
        }
    };

    struct ChannelState {
        BiquadState low_first;
        BiquadState low_second;
        BiquadState high_first;
        BiquadState high_second;

        void reset() noexcept;
    };

    struct PanGains {
        float left = 1.0F;
        float right = 0.0F;
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float decibels_to_gain(float decibels) noexcept;
    [[nodiscard]] static PanGains pan_gains(float pan) noexcept;
    [[nodiscard]] static float multiplier_value(
        CrossoverFrequencyMultiplier multiplier) noexcept;
    [[nodiscard]] float maximum_effective_frequency_hz() const noexcept;
    void sanitize_parameters(StereoCrossoverParameters& parameters) const noexcept;
    void prepare_filters() noexcept;
    void prepare_gains() noexcept;

    float sample_rate_ = 48000.0F;
    float effective_frequency_hz_ = 500.0F;
    StereoCrossoverParameters parameters_{};
    BiquadCoefficients lowpass_{};
    BiquadCoefficients highpass_{};
    std::array<ChannelState, 2> state_{};
    std::array<float, 2> low_level_gain_{};
    std::array<float, 2> high_level_gain_{};
    std::array<PanGains, 2> low_pan_gain_{};
    std::array<PanGains, 2> high_pan_gain_{};
    float output_level_gain_ = 1.0F;
    float balance_left_gain_ = 1.0F;
    float balance_right_gain_ = 1.0F;
};

} // namespace tabletfx
