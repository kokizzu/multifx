#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class AutoPanWaveform : std::uint8_t {
    Square,
    Sine,
    Triangle,
};

enum class AutoPanDivision : std::uint8_t {
    Free,
    Whole,
    Half,
    DottedHalf,
    HalfTriplet,
    Quarter,
    DottedQuarter,
    QuarterTriplet,
    Eighth,
    DottedEighth,
    EighthTriplet,
    Sixteenth,
    DottedSixteenth,
    SixteenthTriplet,
    ThirtySecond,
    ThirtySecondTriplet,
};

struct AutoPanParameters {
    float rate_hz = 0.5F;
    float depth = 0.75F;
    AutoPanWaveform waveform = AutoPanWaveform::Sine;
    bool tempo_sync = false;
    float tempo_bpm = 120.0F;
    AutoPanDivision division = AutoPanDivision::Quarter;
    float depth_smoothing_ms = 10.0F;
};

/*
 * Project-authored stereo auto-panner. Fender's Tone Master Pro Model Guide
 * v1.1.100 names the analogous Orbit Stereo Panner controls Rate, Depth, Tap
 * Division, and a Type switch with square, sine, and triangle choices, but
 * publishes no numeric bounds or defaults. This implementation's bounds,
 * defaults, tempo subdivisions, equal-power law, and stereo-to-mid behavior
 * are project choices; it does not claim Fender DSP, parameter, or sound
 * parity.
 *
 * At zero depth the stereo input is preserved. As depth increases, the
 * centered mid signal is blended toward an equal-power pan trajectory. Mono
 * input is left unchanged because this processor cannot create a second
 * channel in-place. Processing allocates no memory and calls no trigonometric
 * functions per sample.
 */
class AutoPan final : public Effect {
public:
    explicit AutoPan(float sample_rate = 48000.0F,
                     AutoPanParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "auto-pan";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate, AutoPanParameters parameters) noexcept;
    void set_rate_hz(float rate_hz) noexcept;
    void set_depth(float depth) noexcept;
    void set_depth_smoothing_ms(float smoothing_ms) noexcept;
    void set_waveform(AutoPanWaveform waveform) noexcept;
    void set_tempo_sync(bool enabled, float bpm,
                        AutoPanDivision division) noexcept;

    [[nodiscard]] AutoPanParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_rate_hz() const noexcept {
        return effective_rate_hz_;
    }
    [[nodiscard]] float current_depth() const noexcept { return current_depth_; }

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
    void prepare_oscillator() noexcept;
    [[nodiscard]] float waveform_value() const noexcept;
    void advance_oscillator() noexcept;

    float sample_rate_ = 48000.0F;
    AutoPanParameters parameters_{};
    float effective_rate_hz_ = 0.5F;
    float phase_increment_ = 0.0F;
    float phase_ = 0.0F;
    float sine_increment_ = 0.0F;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    float current_depth_ = 0.75F;
    float target_depth_ = 0.75F;
    float depth_ramp_step_ = 0.0F;
    std::size_t depth_ramp_remaining_ = 0;
    std::uint32_t normalization_counter_ = 0;
};

} // namespace tabletfx
