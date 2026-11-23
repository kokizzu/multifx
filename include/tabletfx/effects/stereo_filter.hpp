#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

enum class StereoFilterType : std::uint8_t {
    Null,
    Lowpass,
    Bandpass,
    Highpass,
    LowShelf,
    HighShelf,
    Peaking,
    Notch,
    TiltEq,
    FeedForwardComb,
    FeedbackComb,
    Allpass,
    LowShelf2,
    HighShelf2,
    Peaking2,
};

enum class StereoFilterLfoShape : std::uint8_t {
    Sine,
    Triangle,
    Square,
    SawUp,
    SawDown,
    Random,
};

enum class StereoFilterPhaseInvert : std::uint8_t {
    None,
    Left,
    Right,
    Both,
};

struct StereoFilterParameters {
    StereoFilterType type = StereoFilterType::Lowpass;
    float frequency_hz = 1000.0F;
    std::uint8_t order = 2;
    float q = 0.707F;
    float gain_db = 0.0F;
    float low_cut_hz = 0.0F;
    float high_cut_hz = 0.0F;
    float pan_left_percent = -100.0F;
    float pan_right_percent = 100.0F;
    StereoFilterPhaseInvert phase_invert = StereoFilterPhaseInvert::None;
    float delay_time_ms = 5.0F;
    float depth = 0.5F;
    float feedback = 0.5F;
    float level_db = 0.0F;
    float balance_percent = 0.0F;
    bool lfo_enabled = false;
    StereoFilterLfoShape lfo_shape = StereoFilterLfoShape::Sine;
    float lfo_rate_hz = 0.5F;
    float lfo_duty_percent = 50.0F;
    float mod_frequency_hz = 4000.0F;
    std::uint8_t lfo_quantize_steps = 0;
};

/*
 * Project-authored stereo filter inspired by control roles in the Fractal
 * Audio Blocks Guide. Its filter equations, ranges, defaults, and LFO shapes
 * are project choices, not Fractal Audio DSP or preset parity. The guide names
 * filter types and controls but does not specify numeric limits/defaults for
 * every control. In particular, comb and allpass implementations here are
 * bounded feedback/feed-forward delay filters chosen to expose those controls.
 *
 * Supports mono/stereo interleaved float audio. Delay-line storage is allocated
 * during construction; process() performs no allocation. Four-pole modes are
 * formed by cascading two identical project biquads. LFO coefficient updates
 * occur every 16 frames to bound work while retaining smooth slow sweeps.
 */
class StereoFilter final : public Effect {
public:
    explicit StereoFilter(float sample_rate = 48000.0F,
                          StereoFilterParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "stereo-filter";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] StereoFilterParameters parameters() const noexcept {
        return parameters_;
    }

    void set_parameters(StereoFilterParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kMaximumPasses = 4U;
    static constexpr std::size_t kCoefficientUpdateFrames = 16U;

    struct BiquadCoefficients {
        float b0 = 1.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    struct BiquadState {
        float s1 = 0.0F;
        float s2 = 0.0F;
    };

    struct FirstOrderState {
        float previous_input = 0.0F;
        float previous_output = 0.0F;
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static StereoFilterType sanitize_type(
        StereoFilterType type) noexcept;
    [[nodiscard]] static StereoFilterLfoShape sanitize_shape(
        StereoFilterLfoShape shape) noexcept;
    [[nodiscard]] static StereoFilterPhaseInvert sanitize_phase(
        StereoFilterPhaseInvert phase) noexcept;
    [[nodiscard]] float nyquist_limit() const noexcept;
    void rebuild_coefficients(float frequency_hz) noexcept;
    [[nodiscard]] std::size_t make_coefficients(
        float frequency_hz,
        std::array<BiquadCoefficients, kMaximumPasses>& coefficients) const noexcept;
    [[nodiscard]] float process_biquad(float input, std::uint16_t channel,
                                       std::size_t pass) noexcept;
    [[nodiscard]] float process_comb(float input, std::uint16_t channel,
                                     bool feedback) noexcept;
    [[nodiscard]] float process_allpass(float input,
                                        std::uint16_t channel) noexcept;
    [[nodiscard]] float process_first_order(float input,
                                            std::uint16_t channel) noexcept;
    [[nodiscard]] float lfo_position() const noexcept;
    void advance_lfo() noexcept;
    [[nodiscard]] static float pan_left_gain(float percent) noexcept;
    [[nodiscard]] static float pan_right_gain(float percent) noexcept;

    float sample_rate_ = 48000.0F;
    StereoFilterParameters parameters_{};
    std::array<BiquadCoefficients, kMaximumPasses> coefficients_{};
    std::size_t pass_count_ = 1U;
    std::array<std::array<BiquadState, kMaximumPasses>, 2> biquad_states_{};
    std::array<FirstOrderState, 2> low_cut_states_{};
    std::array<FirstOrderState, 2> high_cut_states_{};
    std::vector<float> delayed_input_;
    std::vector<float> delayed_output_;
    std::size_t delay_capacity_frames_ = 1U;
    std::size_t write_frame_ = 0U;
    std::size_t delay_frames_ = 1U;
    float lfo_phase_ = 0.0F;
    std::uint32_t lfo_cycle_ = 0U;
    float level_gain_ = 1.0F;
    float balance_left_gain_ = 1.0F;
    float balance_right_gain_ = 1.0F;
    float low_cut_alpha_ = 1.0F;
    float high_cut_alpha_ = 0.0F;
};

} // namespace tabletfx
