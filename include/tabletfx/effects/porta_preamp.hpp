#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

struct PortaPreampParameters {
    // All units, ranges, and defaults below are project-authored.
    float gain_1_db = 6.0F;
    float gain_2_db = 3.0F;
    float bass_db = 0.0F;
    float treble_db = 0.0F;
    float level_db = 0.0F;
    float smoothing_ms = 10.0F;
};

/*
 * Project-authored dual-gain preamp processor. Line 6's Helix Stadium 1.4
 * release notes list a PortaBox 424 model and describe Gain 1 as preamp input
 * trim, Gain 2 as second-stage level, plus Bass, Treble, and overall Level.
 * The notes do not publish numeric bounds, defaults, units, or DSP topology.
 * This processor assigns its own dB controls, ranges, defaults, two smooth
 * saturation stages, and fixed-frequency low/high tone shelves. It does not
 * claim Line 6 DSP, control-range, default, or sound parity.
 *
 * Source: https://kb.line6.com/helix-stadium-1-4-release-notes
 * Effect introduction: Helix Stadium firmware 1.4 (October 2026).
 *
 * Processing supports mono and stereo, uses fixed per-channel state, and does
 * not allocate in process(). No oversampling or aliasing-quality claim is
 * made.
 */
class PortaPreamp final : public Effect {
public:
    explicit PortaPreamp(float sample_rate = 48000.0F,
                         PortaPreampParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "porta-preamp";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   PortaPreampParameters parameters) noexcept;
    void set_parameters(PortaPreampParameters parameters) noexcept;
    [[nodiscard]] PortaPreampParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct ChannelState {
        float bass_low = 0.0F;
        float treble_low = 0.0F;
    };

    [[nodiscard]] static PortaPreampParameters sanitize(
        PortaPreampParameters parameters) noexcept;
    [[nodiscard]] static float db_to_gain(float db) noexcept;
    [[nodiscard]] static float one_pole_alpha(float cutoff_hz,
                                               float sample_rate) noexcept;
    [[nodiscard]] static float soft_clip(float sample) noexcept;
    [[nodiscard]] std::size_t ramp_length() const noexcept;
    void retarget() noexcept;
    void advance_ramp() noexcept;
    void snap_to_targets() noexcept;

    float sample_rate_ = 48000.0F;
    PortaPreampParameters parameters_{};
    float bass_alpha_ = 0.0285F;
    float treble_alpha_ = 0.3243F;

    float target_gain_1_ = 1.9952623F;
    float target_gain_2_ = 1.4125376F;
    float target_bass_gain_ = 1.0F;
    float target_treble_gain_ = 1.0F;
    float target_level_gain_ = 1.0F;

    float gain_1_ = 1.9952623F;
    float gain_1_step_ = 0.0F;
    float gain_2_ = 1.4125376F;
    float gain_2_step_ = 0.0F;
    float bass_gain_ = 1.0F;
    float bass_gain_step_ = 0.0F;
    float treble_gain_ = 1.0F;
    float treble_gain_step_ = 0.0F;
    float level_gain_ = 1.0F;
    float level_gain_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0U;
    std::array<ChannelState, 2> channel_state_{};
};

} // namespace tabletfx
