#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/rotary_speaker.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

struct VibratoneParameters {
    // Project-selected ranges; the Tone Master Pro guide lists the controls but
    // does not publish numerical ranges or defaults.
    float drive = 0.0F;
    float tone = 0.8F;
    float level_db = 0.0F;
    float fast_speed_hz = 6.5F;
    float slow_speed_hz = 0.85F;
    bool speed_fast = false;
    float ramp_ms = 650.0F;
    float sensitivity = 0.5F;
    bool dynamics_switch = false;
};

// Project-authored rotating-baffle effect inspired by the Tone Master Pro
// Vibratone control list. Rotor motion reuses the existing project rotary
// engine; Tone, envelope-controlled speed switching, ranges, and defaults are
// project choices, not Fender settings, DSP, or sound parity. It allocates its
// bounded rotor buffer during construction and performs no process-time
// allocation. The tone filter has no fixed sample delay.
class Vibratone final : public Effect {
public:
    explicit Vibratone(float sample_rate = 48000.0F,
                       std::uint16_t channels = 2,
                       VibratoneParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "vibratone";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;

    void set_parameters(VibratoneParameters parameters) noexcept;
    [[nodiscard]] VibratoneParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float target_speed_hz() const noexcept {
        return rotary_.target_hi_rate_hz();
    }
    [[nodiscard]] float current_speed_hz() const noexcept {
        return rotary_.current_hi_rate_hz();
    }
    [[nodiscard]] float input_envelope() const noexcept { return envelope_; }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumSpeedHz = 0.1F;
    static constexpr float kMaximumSpeedHz = 10.0F;
    static constexpr float kMinimumRampMs = 50.0F;
    static constexpr float kMaximumRampMs = 3000.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void prepare_tone_filter() noexcept;
    void apply_rotor_speed(bool fast) noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    VibratoneParameters parameters_{};
    RotarySpeaker rotary_;
    std::array<float, 2> tone_state_{};
    float tone_alpha_ = 1.0F;
    float attack_coefficient_ = 0.0F;
    float release_coefficient_ = 0.0F;
    float envelope_ = 0.0F;
    float sensitivity_threshold_ = 0.05623413F;
    float output_gain_ = 1.0F;
    bool applied_speed_fast_ = false;
};

} // namespace tabletfx
