#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx::effects {

// Project-authored branch levels inspired only by the control roles listed in
// Fender's Tone Master Pro Model Guide v1.1.100. The manual does not publish
// numeric ranges or defaults; these normalized levels and their defaults are
// project choices, not Fender settings.
struct OctobotParameters {
    float direct = 0.65F;  // Project-authored linear level, [0, 1].
    float down = 0.20F;    // Project-authored linear level, [0, 1].
    float sizzle = 0.25F;  // Project-authored linear level, [0, 1].
};

/*
 * Project-authored Octobot effect. Direct is an unshifted branch, Down is
 * a clean project pitch-shifted branch at -12 semitones, and Sizzle is a
 * project pitch-shifted +12-semitone branch followed by a fixed tanh shaper.
 * The dry branch is delayed to align with the shifter's reported latency.
 *
 * This uses the project's STFT polyphonic audio transposer; it does not claim
 * that Fender's Octobot is polyphonic, uses this topology, or has matching
 * tracking, controls, settings, or sound. The manual establishes only the
 * octave-down and octave-up-fuzz roles and the three level controls. Shifter
 * latency is 256 samples (5.333 ms at 48 kHz), plus any host buffering.
 *
 * All storage is allocated by the constructor. process() does not allocate.
 * Configure parameters on the audio thread or while processing is stopped.
 */
class Octobot final : public tabletfx::Effect {
public:
    explicit Octobot(float sample_rate = 48000.0F,
                     std::uint16_t channels = 1,
                     OctobotParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "octobot";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] const OctobotParameters& parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float algorithmic_latency_ms() const noexcept;

    void configure(OctobotParameters parameters) noexcept;
    void reset() noexcept override;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

private:
    static constexpr std::size_t kMaximumChunkFrames = 128U;
    static constexpr std::size_t kAlignmentDelayFrames = 256U;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kParameterSmoothingMs = 10.0F;
    static constexpr float kSizzleDrive = 6.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float finite_input(float sample) noexcept;
    void advance_parameters() noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    tabletfx::PolyPitchShifter down_shifter_;
    tabletfx::PolyPitchShifter up_shifter_;
    OctobotParameters parameters_{};
    float direct_ = 0.65F;
    float target_direct_ = 0.65F;
    float direct_step_ = 0.0F;
    float down_ = 0.20F;
    float target_down_ = 0.20F;
    float down_step_ = 0.0F;
    float sizzle_ = 0.25F;
    float target_sizzle_ = 0.25F;
    float sizzle_step_ = 0.0F;
    std::size_t parameter_ramp_remaining_ = 0U;
    std::size_t parameter_ramp_total_ = 1U;
    std::size_t alignment_write_frame_ = 0U;
    std::vector<float> alignment_delay_;
    std::vector<float> direct_buffer_;
    std::vector<float> down_buffer_;
    std::vector<float> sizzle_buffer_;
};

}  // namespace tabletfx::effects
