#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx::effects {

// Numeric limits, the two preset profiles, and the DSP topology below are
// project-authored. The Tone Master Pro Model Guide v1.1.100 names controls
// (Preset Switch, Manual Switch, Rate, L/C/R Depth, Feedback, Sparkle, Stereo
// Switch) but does not publish their ranges or the underlying DSP.
struct TripleDoubleChorusConfig {
    // PRESET SWITCH: false selects the project-authored Classic profile;
    // true selects Wide. MANUAL SWITCH selects the numeric controls below.
    bool preset_switch = false;
    bool manual_switch = false;

    // Project-authored manual defaults/bounds: Rate [0.05, 8] Hz, each L/C/R
    // Depth [0, 20] ms, Feedback [-0.8, 0.8], Sparkle [0, 1].
    float rate_hz = 0.8F;
    float left_depth_ms = 7.0F;
    float center_depth_ms = 5.0F;
    float right_depth_ms = 8.0F;
    float feedback = 0.1F;
    float sparkle = 0.2F;

    // STEREO SWITCH: true produces a project-authored L/C/R stereo spread;
    // false produces a dual-mono output. The guide does not publish its range
    // or the exact switching behavior.
    bool stereo_switch = true;
};

struct TripleDoubleChorusParameters {
    float rate_hz = 0.8F;
    float left_depth_ms = 7.0F;
    float center_depth_ms = 5.0F;
    float right_depth_ms = 8.0F;
    float feedback = 0.1F;
    float sparkle = 0.2F;
};

// Project-authored fixed-memory tri-stereo chorus. It uses three modulated
// delay voices with 120-degree LFO offsets, linked across stereo input
// channels. A fixed 72% dry / 28% wet blend is used because no Mix control was
// included in the cited Fender control list. Sparkle is a simple wet-path
// high-shelf lift; it is not a recreation of Fender's undocumented DSP.
//
// The delay lines use linear interpolation and bounded feedback. This is a
// generic chorus for experimentation, not a claim of Fender algorithm, preset,
// or sound parity. It performs no heap allocation after construction.
class TripleDoubleChorus final : public tabletfx::Effect {
public:
    explicit TripleDoubleChorus(float sample_rate = 48000.0F) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "triple-double-chorus";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(const TripleDoubleChorusConfig& config) noexcept;
    [[nodiscard]] const TripleDoubleChorusConfig& config() const noexcept {
        return config_;
    }
    [[nodiscard]] const TripleDoubleChorusParameters& effective_parameters() const noexcept {
        return target_;
    }

    // Clears delay and filter state while retaining switches and parameters.
    void reset() noexcept override;

    // Effect-chain adapter. Mono input is centered back to mono after the
    // project's stereo spread; stereo input keeps the two output channels.
    // Processing remains allocation-free.
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

    // Mono is selected when input_right is null. If output_right is provided,
    // mono input can be spread to stereo when Stereo Switch is on. Stereo
    // input uses linked modulation with separate per-channel delay histories.
    // Input and output buffers may not alias.
    void process(const float* input_left,
                 const float* input_right,
                 float* output_left,
                 float* output_right,
                 std::size_t frames) noexcept;

private:
    static constexpr std::size_t kDelaySize = 4096;
    static constexpr std::size_t kVoiceCount = 3;
    static constexpr float kBaseDelayMs = 6.0F;
    static constexpr float kMaxDepthMs = 20.0F;

    void resolve_parameters() noexcept;
    void clear_state() noexcept;
    [[nodiscard]] float read_delay(std::size_t channel,
                                  std::size_t voice,
                                  float delay_samples) const noexcept;

    float sample_rate_;
    float phase_ = 0.0F;
    float sparkle_alpha_ = 1.0F;
    float smoothing_ = 1.0F;
    std::size_t write_position_ = 0;
    TripleDoubleChorusConfig config_{};
    TripleDoubleChorusParameters target_{};
    TripleDoubleChorusParameters current_{};
    bool parameters_initialized_ = false;
    std::array<std::array<std::array<float, kDelaySize>, kVoiceCount>, 2> delay_{};
    std::array<std::array<float, kVoiceCount>, 2> sparkle_lowpass_{};
};

}  // namespace tabletfx::effects
