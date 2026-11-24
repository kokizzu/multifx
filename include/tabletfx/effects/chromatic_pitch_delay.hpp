#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

enum class ChromaticPitchDelayTimeMode : std::uint8_t {
    Milliseconds = 0,
    TempoSync = 1,
};

enum class ChromaticPitchDelayTapDivision : std::uint8_t {
    Whole = 0,
    DottedHalf,
    Half,
    DottedQuarter,
    Quarter,
    QuarterTriplet,
    DottedEighth,
    Eighth,
    EighthTriplet,
    DottedSixteenth,
    Sixteenth,
    SixteenthTriplet,
    ThirtySecond,
    SixtyFourth,
};

struct ChromaticPitchDelayParameters {
    // Project-authored defaults/ranges. The Tone Master Pro Model Guide names
    // these control roles but does not publish general numeric defaults/ranges.
    float pitch_semitones = 0.0F;
    float time_ms = 350.0F;
    ChromaticPitchDelayTimeMode time_mode = ChromaticPitchDelayTimeMode::Milliseconds;
    float tempo_bpm = 120.0F;
    ChromaticPitchDelayTapDivision tap_division = ChromaticPitchDelayTapDivision::Quarter;
    float feedback = 0.25F;
    float tone = 0.65F;
    float mix = 0.25F;
    bool spillover = false;
};

// Project-authored chromatic pitch delay with an opt-in ±24-semitone range.
// The existing PolyPitchShifter is reused once per channel; no second FFT
// engine is created. Delay storage and all pitch state are prepared in the
// constructor. process() performs no allocation.
class ChromaticPitchDelay final : public Effect {
public:
    explicit ChromaticPitchDelay(
        float sample_rate = 48000.0F, std::uint16_t channels = 2,
        float max_delay_ms = 2000.0F,
        ChromaticPitchDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "chromatic-pitch-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] ChromaticPitchDelayParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float max_pitch_semitones() const noexcept { return 24.0F; }
    [[nodiscard]] float effective_time_ms() const noexcept;
    [[nodiscard]] float pitch_engine_latency_ms() const noexcept {
        return pitch_shifter_.algorithmic_latency_ms();
    }
    [[nodiscard]] float dry_path_latency_ms() const noexcept { return 0.0F; }

    void configure(ChromaticPitchDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumTimeMs = 10.0F;
    static constexpr float kMaximumTimeMs = 2000.0F;
    static constexpr float kMaximumFeedback = 0.95F;
    static constexpr float kControlRampMs = 10.0F;
    static constexpr float kMinimumToneCutoffHz = 600.0F;
    static constexpr float kMaximumToneCutoffHz = 18000.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float division_beats(
        ChromaticPitchDelayTapDivision division) noexcept;
    [[nodiscard]] float tone_coefficient(float tone) const noexcept;
    [[nodiscard]] std::size_t samples_for_ms(float time_ms) const noexcept;
    [[nodiscard]] float read_fractional_delay(float delay_samples,
                                               std::uint16_t channel) const noexcept;
    [[nodiscard]] float read_age(std::size_t age,
                                 std::uint16_t channel) const noexcept;
    void process_internal(std::span<float> interleaved, bool record_input,
                          bool bypassed) noexcept;
    void advance_ramps() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    float max_delay_ms_ = 2000.0F;
    std::size_t shifter_latency_samples_ = 0;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;
    PolyPitchShifter pitch_shifter_;
    ChromaticPitchDelayParameters parameters_{};
    float current_time_samples_ = 0.0F;
    float target_time_samples_ = 0.0F;
    float time_step_ = 0.0F;
    float current_feedback_ = 0.25F;
    float feedback_step_ = 0.0F;
    float current_mix_ = 0.25F;
    float mix_step_ = 0.0F;
    float current_tone_coefficient_ = 0.0F;
    float target_tone_coefficient_ = 0.0F;
    float tone_coefficient_step_ = 0.0F;
    std::size_t ramp_total_ = 1;
    std::size_t ramp_remaining_ = 0;
    float filter_state_[2]{};
    bool bypassed_ = false;
};

} // namespace tabletfx
