#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

struct PitchFeedbackDelayParameters {
    // Project-authored controls and defaults; these are not copied from a
    // manufacturer's named delay algorithm.
    float time_ms = 350.0F;
    float feedback = 0.35F;
    float mix = 0.25F;
    float pitch_semitones = 7.0F;
};

/*
 * Project-authored pitch-shifted feedback delay. Each delayed pass is
 * transposed before it is mixed and fed back, so successive repeats accumulate
 * the selected pitch interval. Time is the intended interval between repeat
 * peaks; the ring delay compensates for PolyPitchShifter's reported latency.
 * Its onset and transient shape still reflect that shifter's processing.
 *
 * Numeric bounds, defaults, and the feedback topology are project choices, not
 * claims of parity with any commercial delay algorithm. Configure parameters
 * on the audio thread or while processing is stopped.
 */
class PitchFeedbackDelay final : public Effect {
public:
    explicit PitchFeedbackDelay(float sample_rate = 48000.0F,
                                std::uint16_t channels = 1,
                                float max_delay_ms = 2000.0F,
                                PitchFeedbackDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "pitch-feedback-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] PitchFeedbackDelayParameters parameters() const noexcept {
        return parameters_;
    }

    void configure(PitchFeedbackDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumTimeMs = 40.0F;
    static constexpr float kMaximumTimeMs = 6000.0F;
    static constexpr float kMaximumFeedback = 0.95F;
    static constexpr float kControlRampMs = 10.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] std::size_t samples_for_ms(float time_ms) const noexcept;
    [[nodiscard]] float read_fractional_delay(float delay_samples,
                                               std::uint16_t channel) const noexcept;
    [[nodiscard]] float read_age(std::size_t age,
                                 std::uint16_t channel) const noexcept;
    void advance_ramps() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 1;
    float max_delay_ms_ = 2000.0F;
    std::size_t shifter_latency_samples_ = 0;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;
    PolyPitchShifter pitch_shifter_;
    PitchFeedbackDelayParameters parameters_{};
    float current_time_samples_ = 0.0F;
    float target_time_samples_ = 0.0F;
    float time_step_ = 0.0F;
    float current_feedback_ = 0.35F;
    float feedback_step_ = 0.0F;
    float current_mix_ = 0.25F;
    float mix_step_ = 0.0F;
    std::size_t ramp_total_ = 1;
    std::size_t ramp_remaining_ = 0;
};

} // namespace tabletfx
