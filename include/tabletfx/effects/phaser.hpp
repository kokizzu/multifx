#pragma once

#include "tabletfx/effects/delay_timing.hpp"
#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tabletfx {

struct PhaserParameters {
    float rate_hz = 0.5F;
    bool tempo_sync = false;
    float tempo_bpm = 120.0F;
    DelayDivision tap_division = DelayDivision::quarter;
    float minimum_hz = 250.0F;
    float maximum_hz = 1600.0F;
    float feedback = 0.2F;
    std::uint8_t stages = 4;
    float mix = 0.5F;
    float smoothing_ms = 10.0F;
    // Project-authored stereo LFO phase spread. Kept last so callers using the
    // previous aggregate fields retain the default stereo behavior.
    bool stereo = true;
};

// Modulated first-order all-pass stages create the moving phaser notches.
// Coefficients and state are prepared in fixed storage; process() allocates
// nothing and adds no fixed delay to the dry path.
class Phaser final : public Effect {
public:
    explicit Phaser(float sample_rate = 48000.0F, std::uint16_t channels = 2,
                    PhaserParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override { return "phaser"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }

    void set_parameters(PhaserParameters parameters) noexcept;
    [[nodiscard]] PhaserParameters parameters() const noexcept { return parameters_; }
    [[nodiscard]] float effective_rate_hz() const noexcept;

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct StageState {
        float previous_input = 0.0F;
        float previous_output = 0.0F;
    };

    static constexpr std::size_t kMaximumStages = 8;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumFrequencyHz = 40.0F;
    static constexpr float kMaximumFeedback = 0.7F;
    static constexpr float kMinimumTempoBpm = 20.0F;
    static constexpr float kMaximumTempoBpm = 300.0F;
    static constexpr float kMaximumSynchronizedRateHz = 80.0F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static bool valid_tap_division(DelayDivision division) noexcept;
    [[nodiscard]] static float beats_for_tap_division(DelayDivision division) noexcept;
    [[nodiscard]] float coefficient_for_frequency(float frequency_hz) const noexcept;
    void prepare_lfo() noexcept;
    void retarget(bool immediate) noexcept;
    void advance_lfo() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    PhaserParameters parameters_{};
    std::array<std::array<StageState, kMaximumStages>, 2> state_{};
    std::array<float, 2> previous_wet_{};
    float current_minimum_coefficient_ = 0.0F;
    float current_maximum_coefficient_ = 0.0F;
    float current_feedback_ = 0.2F;
    float current_mix_ = 0.5F;
    float current_sine_increment_ = 0.0F;
    float current_cosine_increment_ = 1.0F;
    float target_minimum_coefficient_ = 0.0F;
    float target_maximum_coefficient_ = 0.0F;
    float target_feedback_ = 0.2F;
    float target_mix_ = 0.5F;
    float target_sine_increment_ = 0.0F;
    float target_cosine_increment_ = 1.0F;
    float minimum_coefficient_step_ = 0.0F;
    float maximum_coefficient_step_ = 0.0F;
    float feedback_step_ = 0.0F;
    float mix_step_ = 0.0F;
    float sine_increment_step_ = 0.0F;
    float cosine_increment_step_ = 0.0F;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    std::size_t ramp_remaining_ = 0;
    std::uint32_t normalization_counter_ = 0;
};

} // namespace tabletfx
