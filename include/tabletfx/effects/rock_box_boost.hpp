#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

struct RockBoxBoostParameters {
    // Project-authored bounds. Line 6 does not publish ranges or defaults.
    float boost_db = 0.0F;
    float level_db = 0.0F;
    float smoothing_ms = 10.0F;
};

/*
 * Project-authored gain stage guided by the Line 6 Helix Stadium Rock Box
 * Boost control descriptions: Boost sets the boost-circuit output and Level
 * sets the block output. Stadium firmware 1.4 lists it as a mono/stereo model
 * based on the Z.Vex Box of Rock boost circuit. Line 6 publishes no numeric
 * bounds, defaults, or DSP topology. This implementation uses two cascaded
 * linear gains with independently smoothed controls; all values and processing
 * are project choices and do not claim Line 6 or Z.Vex settings or sound parity.
 *
 * State is fixed-size and process() performs no allocation.
 * Source: https://kb.line6.com/helix-stadium-1-4-release-notes
 */
class RockBoxBoost final : public Effect {
public:
    explicit RockBoxBoost(float sample_rate = 48000.0F,
                          RockBoxBoostParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "rock-box-boost";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   RockBoxBoostParameters parameters) noexcept;
    void set_parameters(RockBoxBoostParameters parameters) noexcept;
    [[nodiscard]] RockBoxBoostParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    [[nodiscard]] static RockBoxBoostParameters sanitize(
        RockBoxBoostParameters parameters) noexcept;
    [[nodiscard]] static float db_to_gain(float db) noexcept;
    [[nodiscard]] std::size_t ramp_length() const noexcept;
    void retarget() noexcept;
    void advance_ramp() noexcept;
    void snap_to_targets() noexcept;

    float sample_rate_ = 48000.0F;
    RockBoxBoostParameters parameters_{};
    float smoothing_ms_ = 10.0F;
    float target_boost_gain_ = 1.0F;
    float target_level_gain_ = 1.0F;
    float boost_gain_ = 1.0F;
    float level_gain_ = 1.0F;
    float boost_gain_step_ = 0.0F;
    float level_gain_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0U;
};

} // namespace tabletfx
