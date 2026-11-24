#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

enum class DuckingDelayTimeMode : std::uint8_t {
    milliseconds,
    tempo_sync,
};

enum class DuckingDelayTapDivision : std::uint8_t {
    whole,
    dotted_half,
    half,
    dotted_quarter,
    quarter,
    quarter_triplet,
    dotted_eighth,
    eighth,
    eighth_triplet,
    dotted_sixteenth,
    sixteenth,
    sixteenth_triplet,
    thirty_second,
    sixty_fourth,
};

struct DuckingDelayParameters {
    // Free Time is retained while tempo sync is selected.
    float time_ms = 350.0F;
    float feedback = 0.25F;
    float mix = 0.25F;
    float threshold_db = -30.0F;
    float attenuation_db = 12.0F;
    float release_ms = 250.0F;
    bool spillover = false;
    DuckingDelayTimeMode time_mode = DuckingDelayTimeMode::milliseconds;
    float tempo_bpm = 120.0F;
    DuckingDelayTapDivision tap_division = DuckingDelayTapDivision::quarter;
    // Zero disables that feedback-path filter, preserving the original default.
    float low_cut_hz = 0.0F;
    float high_cut_hz = 0.0F;
};

/*
 * Project-authored ducking delay. A linked input peak detector lowers only the
 * delayed signal while the input exceeds Threshold; Feedback remains in the
 * internal delay loop and the dry signal stays immediate. The source documents
 * Threshold, Attenuation, Release, tempo sync, and repeat cuts but publishes no
 * defaults. Numeric ranges, defaults, and the filter response below are
 * project-authored rather than copied from a unit.
 */
class DuckingDelay final : public Effect {
public:
    explicit DuckingDelay(float sample_rate = 48000.0F,
                          std::uint16_t channels = 1,
                          float max_delay_ms = 2000.0F,
                          DuckingDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "ducking-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    [[nodiscard]] DuckingDelayParameters parameters() const noexcept {
        return parameters_;
    }
    void configure(DuckingDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 1.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumTimeMs = 1.0F;
    static constexpr float kMaximumTimeMs = 2000.0F;
    static constexpr float kMaximumFeedback = 0.95F;
    static constexpr float kMinimumThresholdDb = -72.0F;
    static constexpr float kMaximumAttenuationDb = 60.0F;
    static constexpr float kMinimumReleaseMs = 1.0F;
    static constexpr float kMaximumReleaseMs = 2000.0F;
    static constexpr float kMinimumTempoBpm = 20.0F;
    static constexpr float kMaximumTempoBpm = 300.0F;
    static constexpr float kControlRampMs = 10.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float beats_for_division(
        DuckingDelayTapDivision division) noexcept;
    [[nodiscard]] std::size_t samples_for_ms(float time_ms) const noexcept;
    [[nodiscard]] float current_delay_samples() const noexcept;
    [[nodiscard]] float filter_repeat_sample(float sample,
                                             std::uint16_t channel) noexcept;
    [[nodiscard]] float read_fractional_delay(float delay_samples,
                                              std::uint16_t channel) const noexcept;
    [[nodiscard]] float read_age(std::size_t age,
                                 std::uint16_t channel) const noexcept;
    void process_audio(std::span<float> interleaved, bool bypassed) noexcept;
    void advance_ramps() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 1;
    float max_delay_ms_ = kMaximumTimeMs;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    bool storage_cleared_ = true;
    std::vector<float> storage_;
    DuckingDelayParameters parameters_{};
    float threshold_amplitude_ = 0.0316228F;
    float attenuation_gain_ = 0.2511886F;
    float release_coefficient_ = 0.0000833F;
    float duck_gain_ = 1.0F;
    float duck_attack_coefficient_ = 0.0F;
    float current_feedback_ = 0.25F;
    float feedback_step_ = 0.0F;
    float current_mix_ = 0.25F;
    float mix_step_ = 0.0F;
    std::size_t parameter_ramp_total_ = 1;
    std::size_t parameter_ramp_remaining_ = 0;
    float target_delay_samples_ = 0.0F;
    float delay_transition_from_samples_ = 0.0F;
    std::size_t delay_transition_total_ = 1;
    std::size_t delay_transition_remaining_ = 0;
    float repeat_highpass_alpha_ = 1.0F;
    float repeat_lowpass_alpha_ = 1.0F;
    std::array<float, 2> repeat_highpass_previous_input_{};
    std::array<float, 2> repeat_highpass_previous_output_{};
    std::array<float, 2> repeat_lowpass_state_{};
};

} // namespace tabletfx
