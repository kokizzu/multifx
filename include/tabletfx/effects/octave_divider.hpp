#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx::effects {

// Project-authored controls and ranges. The FM9-family Blocks Guide identifies
// two fixed octave-divider voices (one and two octaves down), with independent
// voice pan and level controls. It does not publish numeric bounds or defaults.
struct OctaveDividerParameters {
    float input_gain = 1.0F;       // Linear input gain, [0, 4].
    float voice1_level = 0.45F;    // Fixed -12 semitone voice, [0, 1].
    float voice1_pan = -0.25F;     // Fixed -12 semitone voice, [-1, 1].
    float voice2_level = 0.20F;    // Fixed -24 semitone voice, [0, 1].
    float voice2_pan = 0.25F;      // Fixed -24 semitone voice, [-1, 1].
    float mix = 0.65F;             // Dry/wet balance, [0, 1].
    float output_level = 1.0F;     // Linear output gain, [0, 2].
};

/*
 * Project-authored, fixed-state octave divider. It uses positive-going input
 * crossings to toggle divide-by-two and divide-by-four square-wave voices.
 * This is a lightweight divider, not a pitch-shift or Fractal DSP emulation.
 *
 * The June 2023 Fractal Audio Blocks Guide (Pitch block, printed p.73) says
 * the Octave Divider models a flip-flop style divider, is intended for single
 * notes, and is usually placed before distortion. That is source guidance, not
 * a validator: this effect accepts any audio buffer and makes no promise about
 * chords, tracking quality, settings, or sound parity. The guide documents
 * fixed -1/-2 octave voices with individual Pan and Level; all numeric ranges,
 * defaults, detector details, and DSP here are project-authored.
 *
 * Stereo input is preserved on the dry path and averaged to mono for the
 * divider, then the generated voices are panned to stereo. Processing uses no
 * lookahead, fixed delay, dynamic storage, or per-call allocations.
 */
class OctaveDivider final : public tabletfx::Effect {
public:
    explicit OctaveDivider(float sample_rate = 48000.0F,
                           std::uint16_t channels = 1U,
                           OctaveDividerParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "octave-divider";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    [[nodiscard]] const OctaveDividerParameters& parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] constexpr std::uint32_t algorithmic_latency_frames() const noexcept {
        return 0U;
    }
    [[nodiscard]] constexpr float algorithmic_latency_ms() const noexcept {
        return 0.0F;
    }

    void configure(OctaveDividerParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct Ramp {
        float current = 0.0F;
        float target = 0.0F;
        float step = 0.0F;
        std::uint32_t remaining = 0U;

        void initialize(float value) noexcept;
        void set(float value, std::uint32_t frames) noexcept;
        [[nodiscard]] float advance() noexcept;
        void snap_to_target() noexcept;
    };

    struct DividerState {
        bool edge_armed = true;
        bool voice1_high = false;
        bool voice2_high = false;
        bool active = false;
        std::uint32_t quiet_frames = 0U;
        float gate_gain = 0.0F;
        float gate_target = 0.0F;
        float gate_step = 0.0F;
        std::uint32_t gate_ramp_remaining = 0U;
    };

    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kEdgeThreshold = 0.0015F;
    static constexpr float kSilenceThreshold = 0.0005F;
    static constexpr float kSilenceTimeoutMs = 80.0F;
    static constexpr float kGateRampMs = 3.0F;
    static constexpr float kParameterRampMs = 5.0F;
    static constexpr float kMaximumInputMagnitude = 8.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float finite_input(float sample) noexcept;
    [[nodiscard]] static float pan_left(float pan) noexcept;
    [[nodiscard]] static float pan_right(float pan) noexcept;
    void set_gate_target(float target) noexcept;
    void advance_gate() noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    OctaveDividerParameters parameters_{};
    DividerState divider_{};
    Ramp input_gain_{};
    Ramp voice1_level_{};
    Ramp voice1_pan_{};
    Ramp voice2_level_{};
    Ramp voice2_pan_{};
    Ramp mix_{};
    Ramp output_level_{};
    std::uint32_t silence_timeout_frames_ = 0U;
    std::uint32_t gate_ramp_frames_ = 0U;
    bool parameters_initialized_ = false;
};

}  // namespace tabletfx::effects
