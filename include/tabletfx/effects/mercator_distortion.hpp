#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

struct MercatorDistParameters {
    // Project-authored normalized gains between the three distortion stages.
    float drive = 0.45F;
    float drive_2 = 0.45F;
    float bass_db = 0.0F;
    float mid_db = 0.0F;
    float treble_db = 0.0F;
    bool bright_1 = false;
    bool bright_2 = false;
    float level_db = 0.0F;
    float smoothing_ms = 10.0F;
};

/*
 * Project-authored three-stage distortion inspired by the controls in the
 * Line 6 Helix Stadium v1.4 Mercator description. That document identifies
 * Drive and Drive 2 as interstage gains, Bass/Mid/Treble as active tone
 * controls, Bright 1/2 as switched high boosts after stages one/two, and
 * Level as output level. It does not publish numeric ranges/defaults or DSP
 * topology. Every numeric range/default, gain mapping, filter frequency,
 * clipping curve, and topology below is a project choice; this is not Line 6
 * DSP, parameter, or sound parity.
 *
 * Processing uses three cascaded soft-clipping stages. Drive and Drive 2
 * scale the signals between stages; the two Bright switches add fixed high
 * shelves after stages one and two. Bass, Mid, and Treble form a complementary
 * three-band post-stage tone control. Controls ramp over a configurable
 * interval. State is fixed-size and process() allocates no memory.
 *
 * Source: https://kb.line6.com/helix-stadium-1-4-release-notes
 */
class MercatorDistortion final : public Effect {
public:
    explicit MercatorDistortion(
        float sample_rate = 48000.0F,
        MercatorDistParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "mercator-distortion";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   MercatorDistParameters parameters) noexcept;
    void set_parameters(MercatorDistParameters parameters) noexcept;

    [[nodiscard]] MercatorDistParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct ChannelState {
        float bright_1_low = 0.0F;
        float bright_2_low = 0.0F;
        float tone_low = 0.0F;
        float tone_high = 0.0F;
    };

    [[nodiscard]] static MercatorDistParameters sanitize(
        MercatorDistParameters parameters) noexcept;
    [[nodiscard]] static float db_to_gain(float db) noexcept;
    [[nodiscard]] static float soft_clip(float sample) noexcept;
    [[nodiscard]] static float low_pass_alpha(float cutoff_hz,
                                              float sample_rate) noexcept;
    [[nodiscard]] std::size_t ramp_length() const noexcept;
    void retarget() noexcept;
    void advance_ramp() noexcept;
    void snap_to_targets() noexcept;

    float sample_rate_ = 48000.0F;
    MercatorDistParameters parameters_{};
    float smoothing_ms_ = 10.0F;
    float bright_alpha_ = 0.2266F;
    float tone_low_alpha_ = 0.0233F;
    float tone_high_alpha_ = 0.1770F;

    float target_drive_gain_ = 3.7F;
    float target_drive_2_gain_ = 3.7F;
    float target_bass_gain_ = 1.0F;
    float target_mid_gain_ = 1.0F;
    float target_treble_gain_ = 1.0F;
    float target_bright_1_gain_ = 1.0F;
    float target_bright_2_gain_ = 1.0F;
    float target_level_gain_ = 1.0F;

    float drive_gain_ = 3.7F;
    float drive_gain_step_ = 0.0F;
    float drive_2_gain_ = 3.7F;
    float drive_2_gain_step_ = 0.0F;
    float bass_gain_ = 1.0F;
    float bass_gain_step_ = 0.0F;
    float mid_gain_ = 1.0F;
    float mid_gain_step_ = 0.0F;
    float treble_gain_ = 1.0F;
    float treble_gain_step_ = 0.0F;
    float bright_1_gain_ = 1.0F;
    float bright_1_gain_step_ = 0.0F;
    float bright_2_gain_ = 1.0F;
    float bright_2_gain_step_ = 0.0F;
    float level_gain_ = 1.0F;
    float level_gain_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0U;
    std::array<ChannelState, 2> channel_state_{};
};

} // namespace tabletfx
