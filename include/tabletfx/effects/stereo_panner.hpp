#pragma once

#include "tabletfx/effects/auto_pan.hpp"
#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class StereoPannerWaveform : std::uint8_t {
    Sine,
    Triangle,
    Square,
    SawUp,
    SawDown,
    Random,
    Logarithmic,
    Exponential,
    Trapezoid,
    Astable,
};

struct StereoPannerParameters {
    float rate_hz = 0.5F;
    float width_percent = 100.0F;
    StereoPannerWaveform waveform = StereoPannerWaveform::Sine;
    bool tempo_sync = false;
    float tempo_bpm = 120.0F;
    AutoPanDivision division = AutoPanDivision::Quarter;
    float duty_cycle_percent = 50.0F;
    float shape_percent = 50.0F;
    float phase_degrees = 180.0F;
    float pan_center_percent = 0.0F;
    float ducking_percent = 0.0F;
    float level_db = 0.0F;
    float balance_percent = 0.0F;
    float smoothing_ms = 10.0F;
};

/*
 * Project-authored stereo LFO panner. The November 2026 Fractal Audio Blocks
 * Guide lists a Panner type in the FM9-family Tremolo/Panner block and names
 * Rate, Tempo, Depth/Width, LFO Type, Duty Cycle, Shape, Phase, Pan Center,
 * Ducking, Level, Balance, Bypass, and Bypass Mode. It describes width above
 * 100% as psychoacoustic widening, but does not publish numeric bounds or
 * defaults for these controls. This processor chooses its own ranges, defaults,
 * waveform mappings, stereo math, envelope detector, and smoothing; it makes
 * no Fractal DSP, settings, or sound-parity claim. The guide identifies the
 * FM9 family, while published v12.0 firmware compatibility lists FM9 Original
 * and FM9 Turbo without separately naming Mark II Turbo.
 *
 * Processing is stereo-only in-place and performs no allocations. Mono input
 * is sanitized and passed through because a one-channel buffer cannot carry
 * stereo panning.
 */
class StereoPanner final : public Effect {
public:
    explicit StereoPanner(float sample_rate = 48000.0F,
                          StereoPannerParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "stereo-panner";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   StereoPannerParameters parameters) noexcept;
    void set_parameters(StereoPannerParameters parameters) noexcept;
    [[nodiscard]] StereoPannerParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_rate_hz() const noexcept {
        return effective_rate_hz_;
    }
    [[nodiscard]] float envelope_level() const noexcept { return envelope_; }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumRateHz = 0.05F;
    static constexpr float kMaximumRateHz = 20.0F;
    static constexpr float kMinimumBpm = 30.0F;
    static constexpr float kMaximumBpm = 300.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static StereoPannerWaveform sanitize_waveform(
        StereoPannerWaveform waveform) noexcept;
    void prepare_oscillator() noexcept;
    [[nodiscard]] float waveform_value(float phase) const noexcept;
    [[nodiscard]] float shape_waveform(float value) const noexcept;
    void advance_oscillator() noexcept;

    float sample_rate_ = 48000.0F;
    StereoPannerParameters parameters_{};
    float effective_rate_hz_ = 0.5F;
    float phase_increment_ = 0.0F;
    float phase_ = 0.0F;
    float smoothing_coefficient_ = 0.0F;
    float envelope_attack_coefficient_ = 0.0F;
    float envelope_release_coefficient_ = 0.0F;
    float envelope_ = 0.0F;
    float current_width_percent_ = 100.0F;
    float current_pan_center_percent_ = 0.0F;
    float current_ducking_percent_ = 0.0F;
    float current_level_gain_ = 1.0F;
    float target_level_gain_ = 1.0F;
    float current_balance_percent_ = 0.0F;
    std::uint32_t random_cycle_ = 0U;
};

} // namespace tabletfx
