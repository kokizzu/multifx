#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/plate_reverb.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tabletfx {

struct ShimmerReverbParameters {
    // Project-authored controls and bounds; these are not copied from any
    // manufacturer's algorithm or parameter ranges.
    float decay_seconds = 3.0F;
    float damping_hz = 6500.0F;
    float diffusion = 0.85F;
    float feedback = 0.25F;
    float high_pass_hz = 80.0F;
    float pitch_semitones = 12.0F;
    float shimmer_intensity = 1.0F;
    float mix = 0.25F;
    float level_db = 0.0F;
};

// Project-authored pitch-fed shimmer reverb. A plate-style reverb is kept
// fully wet; a pitch-shifted copy of its wet output is high-pass filtered and
// returned to its input. The unshifted/pitched wet blend is mixed with the dry
// input. This is an original topology using the project's existing DSP blocks,
// not a reproduction of any commercial algorithm. Its block enable/bypass is
// controlled by the containing chain; bypass does not preserve audible tails.
// All storage is allocated during construction; process() does not allocate.
class ShimmerReverb final : public Effect {
public:
    explicit ShimmerReverb(float sample_rate = 48000.0F,
                           std::uint16_t channels = 2,
                           ShimmerReverbParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "shimmer-reverb";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] ShimmerReverbParameters parameters() const noexcept {
        return parameters_;
    }

    // Configure on the audio thread or while processing is stopped.
    void configure(ShimmerReverbParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kFeedbackDelayFrames = 128;
    static constexpr std::size_t kMaxProcessFrames = 128;
    static constexpr float kMaximumFeedback = 0.65F;
    static constexpr float kMaximumInternalSample = 16.0F;
    static constexpr float kControlSmoothingMs = 10.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void advance_controls() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    ShimmerReverbParameters parameters_{};
    PlateReverb reverb_;
    PolyPitchShifter pitch_shifter_;
    std::array<float, 2 * kFeedbackDelayFrames> feedback_ring_{};
    std::array<float, 2 * kMaxProcessFrames> dry_buffer_{};
    std::array<float, 2 * kMaxProcessFrames> wet_buffer_{};
    std::array<float, 2 * kMaxProcessFrames> pitched_buffer_{};
    std::array<float, kMaxProcessFrames> high_pass_alpha_buffer_{};
    std::array<float, kMaxProcessFrames> shimmer_intensity_buffer_{};
    std::array<float, kMaxProcessFrames> mix_buffer_{};
    std::array<float, kMaxProcessFrames> level_buffer_{};
    std::size_t feedback_frame_ = 0;
    std::array<float, 2> feedback_input_previous_{};
    std::array<float, 2> feedback_output_previous_{};
    float feedback_ = 0.25F;
    float feedback_step_ = 0.0F;
    float high_pass_alpha_ = 0.0F;
    float target_high_pass_alpha_ = 0.0F;
    float high_pass_step_ = 0.0F;
    float shimmer_intensity_ = 1.0F;
    float shimmer_intensity_step_ = 0.0F;
    float mix_ = 0.25F;
    float mix_step_ = 0.0F;
    float level_ = 1.0F;
    float target_level_ = 1.0F;
    float level_step_ = 0.0F;
    std::size_t ramp_total_ = 1;
    std::size_t ramp_remaining_ = 0;
};

} // namespace tabletfx
