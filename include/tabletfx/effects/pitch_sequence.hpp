#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/delay_timing.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace tabletfx::effects {

// Project-authored steps for a pitch sequence inspired by the documented
// feature description of Fender Tone Master Pro firmware v1.8.45 (2026-06-03):
// pitch sequences, glides, glitch effects, and freeze-like sounds. Fender's
// release notes do not publish this model's controls, ranges, defaults, or DSP.
// None of the controls or processing below claim Fender algorithm, parameter,
// tracking, latency, or sound parity.
//
// The processor applies programmed semitone shifts to the incoming audio; it
// does not detect the played note or quantize to a scale. Glitch steps add
// deterministic pseudo-random pitch jumps around their programmed interval.
// Freeze steps repeatedly play a short captured input loop with a small
// crossfade at the loop seam. This is a bounded sample-loop approximation, not
// a spectral freeze or an implementation of Fender's effect.
enum class PitchSequenceStepMode : std::uint8_t {
    Pitch,
    Glitch,
    Freeze,
};

struct PitchSequenceParameters {
    // Project-authored bounds: 1–16 steps, each shift −12…+12 semitones.
    std::array<int, 16> semitones{
        0, 7, 12, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<PitchSequenceStepMode, 16> mode{
        PitchSequenceStepMode::Pitch, PitchSequenceStepMode::Pitch,
        PitchSequenceStepMode::Pitch, PitchSequenceStepMode::Pitch,
        PitchSequenceStepMode::Pitch, PitchSequenceStepMode::Pitch,
        PitchSequenceStepMode::Pitch, PitchSequenceStepMode::Pitch,
        PitchSequenceStepMode::Pitch, PitchSequenceStepMode::Pitch,
        PitchSequenceStepMode::Pitch, PitchSequenceStepMode::Pitch,
        PitchSequenceStepMode::Pitch, PitchSequenceStepMode::Pitch,
        PitchSequenceStepMode::Pitch, PitchSequenceStepMode::Pitch};
    std::uint8_t steps = 4;
    bool run = false;

    // Project-authored timing: free steps use 20–2000 ms; synchronized steps
    // use the selected division at 30–300 BPM.
    tabletfx::DelayTimeMode time_mode = tabletfx::DelayTimeMode::milliseconds;
    tabletfx::DelayDivision division = tabletfx::DelayDivision::quarter;
    float free_step_ms = 250.0F;
    float tempo_bpm = 120.0F;

    // Project-authored smoothing and effect controls.
    float glide_ms = 15.0F;                 // 0–250 ms.
    std::uint8_t glitch_depth_semitones = 7; // 0–12 semitones.
    float glitch_interval_ms = 40.0F;        // 10–250 ms.
    float freeze_window_ms = 80.0F;          // 10–120 ms.
    float mix = 1.0F;                        // Linear, 0–1.
    float level = 1.0F;                      // Linear, 0–2.
};

class PitchSequence final : public tabletfx::Effect {
public:
    explicit PitchSequence(float sample_rate = 48000.0F,
                            std::uint16_t channels = 2,
                            PitchSequenceParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "pitch-sequence";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] const PitchSequenceParameters& parameters() const noexcept {
        return parameters_;
    }
    void configure(const PitchSequenceParameters& parameters) noexcept;

    // Starts at step zero. The sequence wraps continuously while running.
    void trigger() noexcept;
    void stop() noexcept;
    void reset() noexcept override;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

    [[nodiscard]] bool running() const noexcept { return running_; }
    [[nodiscard]] std::uint8_t current_step() const noexcept {
        return current_step_;
    }
    [[nodiscard]] float current_pitch_semitones() const noexcept {
        return current_pitch_semitones_;
    }
    [[nodiscard]] bool freeze_active() const noexcept { return freeze_active_; }
    [[nodiscard]] std::size_t frozen_loop_length_frames() const noexcept {
        return freeze_loop_period_;
    }
    [[nodiscard]] float pitch_shifter_latency_ms() const noexcept {
        return pitch_shifter_.algorithmic_latency_ms();
    }

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 96000.0F;
    static constexpr float kMinimumTempoBpm = 30.0F;
    static constexpr float kMaximumTempoBpm = 300.0F;
    static constexpr float kMinimumStepMs = 20.0F;
    static constexpr float kMaximumStepMs = 2000.0F;
    static constexpr float kMaximumOutputMagnitude = 1000000.0F;
    static constexpr std::uint32_t kGlitchSeed = 0x9E3779B9U;

    [[nodiscard]] static float checked_sample_rate(float sample_rate);
    [[nodiscard]] static std::uint16_t checked_channels(std::uint16_t channels);
    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static bool valid_time_mode(
        tabletfx::DelayTimeMode mode) noexcept;
    [[nodiscard]] static bool valid_division(
        tabletfx::DelayDivision division) noexcept;
    [[nodiscard]] static bool valid_step_mode(
        PitchSequenceStepMode mode) noexcept;
    [[nodiscard]] static double division_beats(
        tabletfx::DelayDivision division) noexcept;
    [[nodiscard]] static float finite_output(double sample) noexcept;

    void update_step_duration(bool preserve_fraction,
                              double old_step_duration) noexcept;
    void record_history(const std::array<float, 2>& input) noexcept;
    void synchronize_freeze_mode() noexcept;
    void begin_freeze() noexcept;
    void read_frozen_frame(std::array<float, 2>& frame) noexcept;
    [[nodiscard]] int step_pitch_target() noexcept;
    [[nodiscard]] int next_glitch_offset() noexcept;
    void advance_sequence() noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    tabletfx::PolyPitchShifter pitch_shifter_;
    PitchSequenceParameters parameters_{};
    std::vector<float> history_;
    std::vector<float> dry_delay_;
    std::size_t history_frames_ = 0;
    std::size_t history_write_ = 0;
    std::size_t history_filled_ = 0;
    std::size_t dry_delay_frames_ = 0;
    std::size_t dry_write_ = 0;
    std::size_t freeze_window_frames_ = 0;
    std::size_t freeze_start_ = 0;
    std::size_t freeze_length_ = 0;
    std::size_t freeze_crossfade_frames_ = 0;
    std::size_t freeze_loop_period_ = 0;
    std::size_t freeze_read_ = 0;
    std::size_t glitch_interval_frames_ = 1;
    std::size_t glitch_countdown_ = 0;
    std::uint32_t random_state_ = kGlitchSeed;
    std::int32_t glitch_offset_ = 0;
    double step_duration_samples_ = 12000.0;
    double step_elapsed_samples_ = 0.0;
    float glide_coefficient_ = 1.0F;
    float current_pitch_semitones_ = 0.0F;
    std::uint8_t current_step_ = 0;
    bool running_ = false;
    bool freeze_active_ = false;
};

}  // namespace tabletfx::effects
