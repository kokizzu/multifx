#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

enum class DoubleTakeSource : std::uint8_t {
    mono = 0,
    true_stereo = 1,
    left_only = 2,
    right_only = 3,
};

struct DoubleTakeParameters {
    // The roles mirror the controls documented for Stadium Double Take.
    // Bounds, units, defaults, DSP, and routing below are project-authored.
    std::uint8_t doubles = 2;
    float slop = 0.25F;
    float sensitivity = 0.55F;
    float dry_level = 0.85F;
    float wet_level = 0.15F;
    // Source is meaningful only when the processor is constructed for stereo.
    DoubleTakeSource source = DoubleTakeSource::mono;
};

/*
 * Project-authored dynamic double tracker inspired by the Line 6 Helix
 * Stadium Double Take model. Stadium documentation lists the effect as mono
 * and stereo. A Line 6 sound designer describes 1–4 generated double voices,
 * Slop as timing/pitch variation that responds to playing dynamics, and
 * Sensitivity as the input response for that variation. The stereo version
 * documents Source choices: Mono, True Stereo, Left Only, and Right Only.
 * Dry and Wet level controls are also documented.
 *
 * The vendor does not publish the algorithm, numerical ranges/defaults,
 * detailed routing behavior, latency, or DSP cost. This implementation's
 * normalized Slop/Sensitivity/levels, voice delays, pan placement, envelope
 * detector, deterministic smooth-random timing drift, and mixing are all
 * project choices. They do not claim Line 6 DSP or sound parity.
 *
 * The dry path is immediate. Wet voices use a bounded 7–10.75 ms base delay
 * with up to ±2.5 ms of project-authored movement; no lookahead is used. The
 * fixed-capacity delay ring is allocated in the constructor, and process()
 * performs no allocation.
 *
 * Sources:
 * https://manuals.line6.com/en/helix-stadium/live/effect-blocks
 * https://line6.com/helix-stadium-models/
 * https://line6.com/support/topic/34949-helix-models-%E2%80%9Cbased-on%E2%80%9D-full-listing-pdf-as-of-fw-260/
 */
class DoubleTake final : public Effect {
public:
    explicit DoubleTake(float sample_rate = 48000.0F,
                        std::uint16_t channels = 2,
                        DoubleTakeParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "double-take";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    void set_parameters(DoubleTakeParameters parameters) noexcept;
    [[nodiscard]] DoubleTakeParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kVoiceCount = 4;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMaximumDelayMs = 24.0F;
    static constexpr float kBaseDelayMs = 7.0F;
    static constexpr float kVoiceSpacingMs = 1.25F;
    static constexpr float kMaximumSlopMs = 2.5F;
    static constexpr float kControlRampMs = 10.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float random_value(std::uint32_t& state) noexcept;
    [[nodiscard]] static float equal_power_left(float pan) noexcept;
    [[nodiscard]] static float equal_power_right(float pan) noexcept;
    [[nodiscard]] float read_delay(std::size_t frame, float delay_samples,
                                   std::uint16_t channel) const noexcept;
    [[nodiscard]] float dynamic_activity() const noexcept;
    [[nodiscard]] float noise_value(std::size_t voice) const noexcept;
    void retarget_controls() noexcept;
    void advance_controls() noexcept;
    void advance_noise() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;
    DoubleTakeParameters parameters_{};

    std::array<float, kVoiceCount> voice_weight_{};
    std::array<float, kVoiceCount> voice_weight_step_{};
    std::array<float, kVoiceCount> pan_left_{};
    std::array<float, kVoiceCount> pan_right_{};
    std::array<float, kVoiceCount> noise_current_{};
    std::array<float, kVoiceCount> noise_target_{};
    std::array<float, kVoiceCount> noise_phase_{};
    std::array<std::uint32_t, kVoiceCount> random_state_{};

    float current_slop_ = 0.25F;
    float current_sensitivity_ = 0.55F;
    float current_dry_level_ = 0.85F;
    float current_wet_level_ = 0.15F;
    float slop_step_ = 0.0F;
    float sensitivity_step_ = 0.0F;
    float dry_level_step_ = 0.0F;
    float wet_level_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
    std::size_t ramp_total_ = 1;

    float envelope_ = 0.0F;
    float attack_coefficient_ = 0.0F;
    float release_coefficient_ = 0.0F;
    float noise_phase_step_ = 0.0F;
};

} // namespace tabletfx
