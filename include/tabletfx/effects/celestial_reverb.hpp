#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/plate_reverb.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tabletfx {

struct CelestialReverbParameters {
    // Project-authored controls. The names follow the Fender Model Guide,
    // while these bounds, defaults, and DSP behavior are TabletFX choices.
    float mix = 0.25F;
    float decay_seconds = 3.0F;
    float pitch_1_semitones = 12.0F;
    float pitch_2_semitones = 7.0F;
    float pitch_decay_seconds = 2.0F;
    float high_cut_hz = 9000.0F;
    float blend = 0.5F;
    bool spillover = false;
};

// Project-authored dual-pitch feedback reverb. A plate-style network feeds
// two independently transposed copies of its wet signal back into the network.
// This uses the project's PlateReverb and PolyPitchShifter; it is not Fender's
// Celestial Reverb DSP or a claim of matching its sound. Processing is
// allocation-free after construction. Spillover preserves this block's wet
// tail on bypass; it does not implement rig switching or cross-preset tails.
class CelestialReverb final : public Effect {
public:
    explicit CelestialReverb(float sample_rate = 48000.0F,
                             std::uint16_t channels = 2,
                             CelestialReverbParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "celestial-reverb";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] CelestialReverbParameters parameters() const noexcept {
        return parameters_;
    }

    // Configure outside the audio callback or on the audio thread.
    void configure(CelestialReverbParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kFeedbackDelayFrames = 128;
    static constexpr std::size_t kMaxProcessFrames = 128;
    static constexpr float kControlSmoothingMs = 10.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void process_block(std::span<float> interleaved, std::size_t frames,
                       bool bypassed) noexcept;
    void clear_tail_state() noexcept;
    void advance_controls() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    CelestialReverbParameters parameters_{};
    PlateReverb reverb_;
    PolyPitchShifter pitch_1_;
    PolyPitchShifter pitch_2_;
    std::array<float, kMaxProcessFrames * 2> dry_buffer_{};
    std::array<float, kMaxProcessFrames * 2> wet_buffer_{};
    std::array<float, kMaxProcessFrames * 2> pitch_1_buffer_{};
    std::array<float, kMaxProcessFrames * 2> pitch_2_buffer_{};
    // Two 128-frame mono/stereo pitch-return rings. The extra channel slot
    // keeps indexing identical for mono and stereo instances.
    std::array<float, 2 * kFeedbackDelayFrames * 2> feedback_ring_{};
    // Low-pass states: unshifted wet, pitch 1, pitch 2; two channels each.
    std::array<float, 6> high_cut_state_{};
    std::size_t feedback_frame_ = 0;
    float feedback_gain_ = 0.0F;
    float high_cut_alpha_ = 0.0F;
    float target_high_cut_alpha_ = 0.0F;
    float high_cut_alpha_step_ = 0.0F;
    float mix_ = 0.25F;
    float mix_step_ = 0.0F;
    float blend_ = 0.5F;
    float blend_step_ = 0.0F;
    std::size_t ramp_total_ = 1;
    std::size_t ramp_remaining_ = 0;
    bool bypass_cleared_ = false;
};

} // namespace tabletfx
