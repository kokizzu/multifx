#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx::effects {

// Project-authored control ranges. Fender's Tone Master Pro Model Guide
// names Boost, Volume, and Octave Switch for Octave Fuzz but publishes no
// numeric ranges, defaults, or processing topology.
struct OctaveFuzzParameters {
    float boost = 0.55F;   // Project pre-fuzz gain amount, [0, 1].
    float volume = 0.75F;  // Project linear output level, [0, 2].
    bool octave_switch = true;
};

/*
 * Project-authored upper-octave fuzz with a polyphonic +12-semitone branch.
 * The octave path uses the project's STFT transposer, then project fuzz
 * shaping. A fixed 256-frame delay aligns the non-shifted fuzz branch with
 * the transposer. This does not claim Fender's Octave Fuzz topology, tracking,
 * settings, or sound; the manufacturer's guide documents control roles only.
 *
 * Construction allocates all buffers. process() does not allocate.
 */
class OctaveFuzz final : public tabletfx::Effect {
public:
    explicit OctaveFuzz(float sample_rate = 48000.0F,
                        std::uint16_t channels = 1,
                        OctaveFuzzParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "octave-fuzz";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] const OctaveFuzzParameters& parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float algorithmic_latency_ms() const noexcept;

    void configure(OctaveFuzzParameters parameters) noexcept;
    void reset() noexcept override;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

private:
    static constexpr std::size_t kMaximumChunkFrames = 128U;
    static constexpr std::size_t kAlignmentDelayFrames = 256U;
    static constexpr float kParameterSmoothingMs = 10.0F;
    static constexpr float kMaximumInputMagnitude = 1000000.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float finite_input(float sample) noexcept;
    void advance_parameters() noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    tabletfx::PolyPitchShifter octave_shifter_;
    OctaveFuzzParameters parameters_{};
    float boost_ = 0.55F;
    float target_boost_ = 0.55F;
    float boost_step_ = 0.0F;
    float volume_ = 0.75F;
    float target_volume_ = 0.75F;
    float volume_step_ = 0.0F;
    float octave_mix_ = 0.35F;
    float target_octave_mix_ = 0.35F;
    float octave_mix_step_ = 0.0F;
    std::size_t parameter_ramp_remaining_ = 0U;
    std::size_t parameter_ramp_total_ = 1U;
    std::size_t alignment_write_frame_ = 0U;
    std::vector<float> alignment_delay_;
    std::vector<float> octave_buffer_;
    std::vector<float> base_buffer_;
};

}  // namespace tabletfx::effects
