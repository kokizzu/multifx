#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

inline constexpr std::size_t kStepTremoloMaximumSteps = 16;

struct StepTremoloParameters {
    float rate_hz = 4.0F;
    float depth = 0.75F;
    std::uint8_t step_count = 8;
    // Per-step output level, normalized to [0, 1]. The levels are deliberately
    // distinct from Rhythmic Slicer's on/off gate patterns.
    std::array<float, kStepTremoloMaximumSteps> levels{
        1.0F, 0.75F, 0.5F, 0.75F,
        1.0F, 0.75F, 0.5F, 0.75F,
        1.0F, 0.75F, 0.5F, 0.75F,
        1.0F, 0.75F, 0.5F, 0.75F,
    };
    // A short crossfade at step boundaries avoids hard clicks. This is a
    // project-authored control; Fender does not publish Step Tremolo controls.
    float transition_ms = 4.0F;
};

// Project-authored stepped amplitude modulation. A cycle contains up to 16
// independently adjustable levels; depth blends those levels toward unity,
// and a bounded boundary ramp avoids a hard on/off gate. This is not a claim of
// Fender algorithm, control, or sound parity and remains distinct from the
// project's Rhythmic Slicer pattern gate.
class StepTremolo final : public Effect {
public:
    explicit StepTremolo(float sample_rate = 48000.0F,
                         StepTremoloParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "step-tremolo";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   StepTremoloParameters parameters) noexcept;
    void set_rate_hz(float rate_hz) noexcept;
    void set_depth(float depth) noexcept;
    void set_step_count(std::uint8_t step_count) noexcept;
    void set_step_level(std::uint8_t step, float level) noexcept;
    void set_transition_ms(float transition_ms) noexcept;

    [[nodiscard]] StepTremoloParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] std::uint8_t current_step() const noexcept {
        return current_step_;
    }
    [[nodiscard]] float current_gain() const noexcept { return current_gain_; }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumRateHz = 0.05F;
    static constexpr float kMaximumRateHz = 20.0F;
    static constexpr float kMaximumTransitionMs = 100.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] std::uint8_t step_at_phase(float phase) const noexcept;
    [[nodiscard]] float gain_for_step(std::uint8_t step) const noexcept;
    void prepare_transition() noexcept;
    void begin_transition(std::uint8_t step) noexcept;

    float sample_rate_ = 48000.0F;
    StepTremoloParameters parameters_{};
    float phase_increment_ = 4.0F / 48000.0F;
    float phase_ = 0.0F;
    float current_gain_ = 1.0F;
    float target_gain_ = 1.0F;
    float ramp_step_ = 0.0F;
    std::size_t transition_frames_ = 0;
    std::size_t ramp_remaining_ = 0;
    std::uint8_t current_step_ = 0;
};

} // namespace tabletfx
