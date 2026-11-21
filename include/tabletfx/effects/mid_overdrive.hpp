#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
#include <string_view>

namespace tabletfx {

struct MidFocusedOverdriveParameters {
    float drive = 0.18F;
    float mid_boost_db = 6.0F;
    float level_db = -4.0F;
    float mix = 0.4F;
    float smoothing_ms = 10.0F;
};

// A generic overdrive with a broad pre-clipping mid emphasis.
class MidFocusedOverdrive final : public Effect {
public:
    explicit MidFocusedOverdrive(float sample_rate = 48000.0F,
                                 MidFocusedOverdriveParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "mid-focused-overdrive";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }
    [[nodiscard]] float drive() const noexcept { return target_drive_; }
    [[nodiscard]] float mid_boost_db() const noexcept { return target_mid_boost_db_; }
    [[nodiscard]] float level_db() const noexcept { return target_level_db_; }
    [[nodiscard]] float mix() const noexcept { return target_mix_; }

    void configure(MidFocusedOverdriveParameters parameters) noexcept;
    void set_drive(float drive) noexcept;
    void set_mid_boost_db(float boost_db) noexcept;
    void set_level_db(float level_db) noexcept;
    void set_mix(float mix) noexcept;
    void configure_smoothing(float sample_rate, float smoothing_ms) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    void retarget() noexcept;
    void advance_ramp() noexcept;
    [[nodiscard]] static float bounded_drive(float drive) noexcept;
    [[nodiscard]] static float bounded_boost(float boost_db) noexcept;
    [[nodiscard]] static float bounded_level(float level_db) noexcept;
    [[nodiscard]] static float bounded_mix(float mix) noexcept;
    [[nodiscard]] static float db_to_gain(float level_db) noexcept;
    [[nodiscard]] static float soft_clip(float sample) noexcept;

    float sample_rate_ = 48000.0F;
    float smoothing_ms_ = 10.0F;
    float low_alpha_ = 0.0383F;
    float high_alpha_ = 0.2094F;
    float target_drive_ = 0.18F;
    float target_mid_boost_db_ = 6.0F;
    float target_level_db_ = -4.0F;
    float target_mix_ = 0.4F;
    float drive_ = 0.18F;
    float drive_step_ = 0.0F;
    float mid_gain_ = 1.9952623F;
    float target_mid_gain_ = 1.9952623F;
    float mid_gain_step_ = 0.0F;
    float level_gain_ = 0.63095737F;
    float target_level_gain_ = 0.63095737F;
    float level_step_ = 0.0F;
    float mix_ = 0.4F;
    float mix_step_ = 0.0F;
    std::array<float, 2> low_state_{};
    std::array<float, 2> high_state_{};
    std::size_t ramp_remaining_ = 0;
};

} // namespace tabletfx
