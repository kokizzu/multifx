#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/delay_timing.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

struct TapeEchoParameters {
    DelayTimeMode time_mode = DelayTimeMode::milliseconds;
    float time_ms = 350.0F;
    float tempo_bpm = 120.0F;
    DelayDivision tap_division = DelayDivision::quarter;
    bool independent_stereo_times = false;
    float left_time_ms = 350.0F;
    float right_time_ms = 350.0F;
    DelayDivision left_tap_division = DelayDivision::quarter;
    DelayDivision right_tap_division = DelayDivision::quarter;
    float feedback = 0.35F;
    float mix = 0.25F;
    float record_level = 0.25F;
    float tape_bias = 0.35F;
    float tape_noise = 0.0F;
    float wow_flutter = 0.15F;
    bool spillover = false;
};

/*
 * Project-authored tape-echo delay core. The Fender Tone Master Pro Model
 * Guide names Time, Tap Division, Feedback, Mix, Record Level, Tape Bias,
 * Tape Noise, Wow and Flutter, and a Spillover switch for its Tape Echo.
 * The guide supplies no numeric ranges/defaults or DSP topology. All bounds,
 * defaults, tape saturation, noise, modulation, and stereo behavior here are
 * project choices; this is not Fender, Echoplex, or Space Echo parity.
 *
 * `process()` allocates no memory. Configure parameters outside the audio
 * callback. `process_bypassed()` passes dry audio and emits the stored tail
 * only when the project Spillover option is enabled.
 */
class TapeEcho final : public Effect {
public:
    explicit TapeEcho(float sample_rate = 48000.0F,
                      std::uint16_t channels = 1U,
                      float max_delay_ms = 2000.0F,
                      TapeEchoParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "tape-echo";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }
    [[nodiscard]] const TapeEchoParameters& parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float selected_time_ms() const noexcept;
    [[nodiscard]] float selected_time_ms(std::uint16_t channel) const noexcept;

    void configure(TapeEchoParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumTimeMs = 1.0F;
    static constexpr float kMaximumTimeMs = 2000.0F;
    static constexpr float kMaximumFeedback = 0.95F;
    static constexpr float kMinimumTempoBpm = 20.0F;
    static constexpr float kMaximumTempoBpm = 300.0F;
    static constexpr float kControlRampMs = 10.0F;
    static constexpr float kTau = 6.2831853071795864769F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float finite_input(float sample) noexcept;
    [[nodiscard]] static bool valid_division(DelayDivision division) noexcept;
    [[nodiscard]] static float beats_for_division(DelayDivision division) noexcept;
    [[nodiscard]] static float signed_random(std::uint32_t& state) noexcept;
    [[nodiscard]] float read_delay(float delay_samples,
                                   std::uint16_t channel) const noexcept;
    [[nodiscard]] float record_to_tape(float sample) const noexcept;
    [[nodiscard]] float modulated_delay_samples(std::uint16_t channel) const noexcept;
    void advance_parameters() noexcept;
    void process_audio(std::span<float> interleaved, bool bypassed) noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 1U;
    float max_delay_ms_ = 2000.0F;
    std::size_t capacity_frames_ = 0U;
    std::size_t write_frame_ = 0U;
    bool storage_cleared_ = true;
    std::vector<float> storage_;
    TapeEchoParameters parameters_{};
    std::array<float, 2> current_time_samples_{{16800.0F, 16800.0F}};
    std::array<float, 2> target_time_samples_{{16800.0F, 16800.0F}};
    std::array<float, 2> time_step_{};
    float current_feedback_ = 0.35F;
    float feedback_step_ = 0.0F;
    float current_mix_ = 0.25F;
    float mix_step_ = 0.0F;
    float current_record_level_ = 0.25F;
    float record_level_step_ = 0.0F;
    float current_tape_bias_ = 0.35F;
    float tape_bias_step_ = 0.0F;
    float current_tape_noise_ = 0.0F;
    float tape_noise_step_ = 0.0F;
    float current_wow_flutter_ = 0.15F;
    float wow_flutter_step_ = 0.0F;
    std::size_t ramp_total_ = 1U;
    std::size_t ramp_remaining_ = 0U;
    float wow_phase_ = 0.0F;
    float flutter_phase_ = 0.0F;
    std::uint32_t random_state_ = 0x9e3779b9U;
};

}  // namespace tabletfx
