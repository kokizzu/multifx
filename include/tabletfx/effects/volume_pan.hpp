#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class VolumePanTaper : std::uint8_t {
    Linear,
    Log30A,
    Log20A,
    Log15A,
    Log10A,
    Log5A,
    STaper,
};

enum class VolumePanInputSelect : std::uint8_t {
    Stereo,
    LeftOnly,
    RightOnly,
};

enum class VolumePanBypassMode : std::uint8_t {
    Pass,
    Mute,
};

struct VolumePanParameters {
    // Project-authored normalized expression position: 0 is heel, 1 is toe.
    float volume = 1.0F;
    // The checked Fractal guide does not publish numeric bounds or defaults.
    // These endpoints and the following ranges are project-authored.
    float minimum_db = -96.0F;
    float maximum_db = 0.0F;
    VolumePanTaper taper = VolumePanTaper::Linear;
    float balance_percent = 0.0F;
    VolumePanInputSelect input_select = VolumePanInputSelect::Stereo;
    float pan_left_percent = -100.0F;
    float pan_right_percent = 100.0F;
    // Kept independent from the Volume control, as the Fractal block guide
    // lists a separate Output Level control.
    float output_level_db = 0.0F;
    bool bypassed = false;
    VolumePanBypassMode bypass_mode = VolumePanBypassMode::Pass;
};

/*
 * Project-authored stereo volume/pan utility based on the controls named in
 * Fractal Audio's FM9-family Blocks Guide: Volume, Balance, Taper, Input
 * Select, independent Pan Left/Pan Right, Bypass Mode, and Output Level.
 * The guide does not provide numeric bounds/defaults or the internal pan/taper
 * laws. The ranges, taper approximations, equal-power pan law, and input
 * routing below are therefore project choices, not Fractal settings or DSP
 * parity. The log taper labels select curves whose midpoint is approximately
 * the named attenuation; the exact Fractal curves remain unknown.
 *
 * Supports mono and stereo in-place buffers without allocating in process().
 * For more than two channels, the first pair receives stereo processing and
 * remaining channels receive the shared Volume and Output Level gains.
 */
class VolumePan final : public Effect {
public:
    explicit VolumePan(VolumePanParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "volume-pan";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void set_parameters(VolumePanParameters parameters) noexcept;
    [[nodiscard]] VolumePanParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static VolumePanTaper sanitize_taper(
        VolumePanTaper taper) noexcept;
    [[nodiscard]] static VolumePanInputSelect sanitize_input_select(
        VolumePanInputSelect input_select) noexcept;
    [[nodiscard]] static VolumePanBypassMode sanitize_bypass_mode(
        VolumePanBypassMode bypass_mode) noexcept;
    [[nodiscard]] static float volume_curve(float position,
                                           VolumePanTaper taper) noexcept;
    static void pan_gains(float position, float& left_gain,
                          float& right_gain) noexcept;
    void prepare_gains() noexcept;

    VolumePanParameters parameters_{};
    float volume_gain_ = 1.0F;
    float output_gain_ = 1.0F;
    float left_input_left_gain_ = 1.0F;
    float left_input_right_gain_ = 0.0F;
    float right_input_left_gain_ = 0.0F;
    float right_input_right_gain_ = 1.0F;
    float left_balance_gain_ = 1.0F;
    float right_balance_gain_ = 1.0F;
};

} // namespace tabletfx
