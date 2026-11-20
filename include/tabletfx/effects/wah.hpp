#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tabletfx {

struct WahParameters {
    float position = 0.5F;        // 0 = heel/minimum, 1 = toe/maximum
    float minimum_hz = 350.0F;
    float maximum_hz = 2200.0F;
    float resonance_q = 4.0F;
    float level_db = 0.0F;
    float smoothing_ms = 10.0F;
};

// Generic expression-controlled resonant band-pass wah. It uses a topology-
// preserving state-variable filter, prepared control coefficients, fixed
// per-channel state, and no lookahead. This is not a commercial circuit model.
class Wah final : public Effect {
public:
    explicit Wah(float sample_rate = 48000.0F,
                 std::uint16_t channels = 2,
                 WahParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override { return "wah"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    // Full preset configuration is immediate and belongs on the control/setup
    // thread. Individual setters prepare their targets off process() and ramp
    // filter coefficients/level over smoothing_ms.
    void configure(float sample_rate, std::uint16_t channels,
                   WahParameters parameters) noexcept;
    void set_position(float position) noexcept;
    void set_sweep_range_hz(float minimum_hz, float maximum_hz) noexcept;
    void set_resonance_q(float resonance_q) noexcept;
    void set_level_db(float level_db) noexcept;
    void set_smoothing_ms(float smoothing_ms) noexcept;

    [[nodiscard]] WahParameters parameters() const noexcept { return parameters_; }
    [[nodiscard]] float current_frequency_hz() const noexcept;
    [[nodiscard]] float target_frequency_hz() const noexcept { return target_frequency_hz_; }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct State {
        float integrator1 = 0.0F;
        float integrator2 = 0.0F;
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] float frequency_for_position(float position) const noexcept;
    void retarget(bool immediate) noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    WahParameters parameters_{};
    float current_g_ = 0.0F;
    float target_g_ = 0.0F;
    float g_step_ = 0.0F;
    float current_k_ = 0.25F;
    float target_k_ = 0.25F;
    float k_step_ = 0.0F;
    float current_level_ = 1.0F;
    float target_level_ = 1.0F;
    float level_step_ = 0.0F;
    float target_frequency_hz_ = 1000.0F;
    std::size_t ramp_remaining_ = 0;
    std::array<State, 2> states_{};
};

} // namespace tabletfx
