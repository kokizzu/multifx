#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

struct RockBoxDistortionParameters {
    // Project-authored normalized controls; Line 6 publishes no ranges/defaults.
    float drive = 0.5F;
    float tone = 0.5F;
    float level_db = 0.0F;
    float smoothing_ms = 10.0F;
};

/*
 * Project-authored mono/stereo distortion stage guided by the Line 6 Helix
 * Stadium Rock Box Dist control descriptions: Drive controls distortion,
 * Tone controls tonal balance (lower is darker, higher is brighter), and
 * Level controls overall block output. Line 6 introduced this model in
 * Stadium firmware 1.4 and identifies it as based on the Z.Vex Box of Rock
 * distortion circuit. The published notes do not give numeric ranges,
 * defaults, or DSP topology. All values and processing here are project
 * choices; this does not claim Line 6 settings, DSP, or sound parity.
 *
 * Processing is a project-authored driven soft clipper followed by a
 * one-pole low-pass Tone control. Parameters ramp over a configurable period.
 * State is fixed-size and process() performs no allocation.
 *
 * Source: https://kb.line6.com/helix-stadium-1-4-release-notes
 */
class RockBoxDistortion final : public Effect {
public:
    explicit RockBoxDistortion(
        float sample_rate = 48000.0F,
        RockBoxDistortionParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "rock-box-distortion";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   RockBoxDistortionParameters parameters) noexcept;
    void set_parameters(RockBoxDistortionParameters parameters) noexcept;
    [[nodiscard]] RockBoxDistortionParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct ChannelState {
        float tone_low = 0.0F;
    };

    [[nodiscard]] static RockBoxDistortionParameters sanitize(
        RockBoxDistortionParameters parameters) noexcept;
    [[nodiscard]] static float db_to_gain(float db) noexcept;
    [[nodiscard]] static float soft_clip(float sample) noexcept;
    [[nodiscard]] static float low_pass_alpha(float cutoff_hz,
                                              float sample_rate) noexcept;
    [[nodiscard]] float tone_alpha(float tone) const noexcept;
    [[nodiscard]] std::size_t ramp_length() const noexcept;
    void retarget() noexcept;
    void advance_ramp() noexcept;
    void snap_to_targets() noexcept;

    float sample_rate_ = 48000.0F;
    RockBoxDistortionParameters parameters_{};
    float smoothing_ms_ = 10.0F;

    float target_drive_gain_ = 6.5F;
    float target_tone_alpha_ = 0.374F;
    float target_level_gain_ = 1.0F;
    float drive_gain_ = 6.5F;
    float drive_gain_step_ = 0.0F;
    float tone_alpha_ = 0.374F;
    float tone_alpha_step_ = 0.0F;
    float level_gain_ = 1.0F;
    float level_gain_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0U;
    std::array<ChannelState, 2> channel_state_{};
};

} // namespace tabletfx
