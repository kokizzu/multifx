#pragma once

#include "tabletfx/effects.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

// Transparent 0..24 dB level boost. It deliberately reuses the shared Gain
// ramp so the effect adds no separate DSP implementation or audio-path state.
class CleanBoost final : public Effect {
public:
    explicit CleanBoost(float gain_db = 0.0F, float sample_rate = 48000.0F,
                        float smoothing_ms = 10.0F) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override { return "clean-boost"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }
    [[nodiscard]] float gain_db() const noexcept { return gain_.gain_db(); }

    void set_gain_db(float gain_db) noexcept;
    void set_gain_db_immediate(float gain_db) noexcept;
    void configure_smoothing(float sample_rate, float smoothing_ms) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    [[nodiscard]] static float bounded_gain(float gain_db) noexcept;

    Gain gain_;
};

} // namespace tabletfx
