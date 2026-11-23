#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class FormantVowel : std::uint8_t {
    aaa,
    eee,
    iii,
    ohh,
    ooo,
    ehh,
    ahh,
    aww,
    uhh,
    err,
};

struct FormantFilterParameters {
    // Continuous path through five project-defined vowel profiles: A, E, I, O, U.
    // Integer positions select a profile; intermediate positions crossfade the
    // formant frequencies and gains between adjacent profiles.
    float vowel_position = 0.0F;
    float resonance_q = 4.0F;
    float mix = 1.0F;
    float level_db = 0.0F;
    bool three_vowel_morph = false;
    FormantVowel start_vowel = FormantVowel::aaa;
    FormantVowel middle_vowel = FormantVowel::eee;
    FormantVowel end_vowel = FormantVowel::ooo;
    float control_percent = 0.0F;
    float balance_percent = 0.0F;
};

/*
 * Project-authored four-band formant-style filter. It sweeps a fixed bank of
 * peaking filters through five generic vowel profiles or project-authored
 * Start/Middle/End profiles. The profile data, control ranges, defaults, and
 * signal path are project choices and do not reproduce any named
 * manufacturer's model or claim speech-like pitch tracking.
 *
 * The processor supports mono/stereo interleaved float audio, uses fixed
 * per-channel state, smooths parameter changes over 10 ms, and performs no
 * allocation in process().
 */
class FormantFilter final : public Effect {
public:
    explicit FormantFilter(float sample_rate = 48000.0F,
                           FormantFilterParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "formant-filter";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void set_parameters(FormantFilterParameters parameters) noexcept;
    [[nodiscard]] FormantFilterParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumQ = 0.5F;
    static constexpr float kMaximumQ = 12.0F;
    static constexpr float kMaximumInputMagnitude = 16.0F;
    static constexpr float kMaximumStateMagnitude = 1000000.0F;
    static constexpr float kParameterSmoothingMs = 10.0F;
    static constexpr std::size_t kFormantCount = 4U;
    static constexpr std::size_t kProfileCount = 5U;

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

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] BiquadCoefficients make_coefficients(
        float frequency_hz, float q, float gain_db) const noexcept;
    [[nodiscard]] float filter_sample(float input, std::uint16_t channel) noexcept;
    void advance_parameters() noexcept;

    float sample_rate_ = 48000.0F;
    FormantFilterParameters parameters_{};
    std::array<std::array<BiquadCoefficients, kFormantCount>, 2> coefficients_{};
    std::array<std::array<BiquadCoefficients, kFormantCount>, 2> target_coefficients_{};
    std::array<std::array<BiquadCoefficients, kFormantCount>, 2> coefficient_steps_{};
    std::array<std::array<BiquadState, kFormantCount>, 2> states_{};
    float mix_ = 1.0F;
    float target_mix_ = 1.0F;
    float mix_step_ = 0.0F;
    float level_gain_ = 1.0F;
    float target_level_gain_ = 1.0F;
    float level_gain_step_ = 0.0F;
    float balance_left_gain_ = 1.0F;
    float target_balance_left_gain_ = 1.0F;
    float balance_left_gain_step_ = 0.0F;
    float balance_right_gain_ = 1.0F;
    float target_balance_right_gain_ = 1.0F;
    float balance_right_gain_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
    std::size_t ramp_total_ = 1;
};

} // namespace tabletfx
