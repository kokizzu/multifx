#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx::effects {

// Project-authored controls for a pitch-tracked ring modulator. Line 6's
// current Stadium catalog verifies the model name, Modulation category,
// Stereo format, and first appearance 1.0 only. It does not publish this
// model's parameter table, bounds, defaults, or DSP. These controls are
// project choices, not Line 6 settings or a claim of algorithm parity.
struct PitchRingModParameters {
    float carrier_ratio = 1.0F;        // Project range: 0.25..4.0.
    float fallback_pitch_hz = 110.0F;  // Project range: 55..880 Hz.
    float tracking_confidence = 0.55F; // Project range: 0.2..0.98.
    float mix = 0.5F;                  // Project range: 0..1.
    float level_db = 0.0F;             // Project range: -24..+12 dB.
};

/*
 * Project-authored pitch-tracked ring modulation. A bounded, monophonic
 * autocorrelation tracker estimates one shared fundamental from the mono sum
 * of the input channels; a sine carrier follows that estimate times
 * carrier_ratio. The input is multiplied by the carrier and blended with the
 * dry path. The moderate default Mix retains some of the original guitar
 * signal.
 *
 * This is an explicitly monophonic tracking design. Chords, weak fundamentals,
 * noisy signals, and rapid note changes can produce inaccurate carrier pitch.
 * When confidence or input level is insufficient, the project-selected
 * fallback pitch is used. The current Stadium catalog verifies the model name,
 * Modulation category, Stereo format, and first appearance 1.0; the manual
 * does not publish this model's algorithm or controls, so this project kernel
 * does not claim Stadium parity.
 *
 * The tracker and oscillator use fixed state; process() allocates nothing.
 */
class PitchRingMod final : public tabletfx::Effect {
public:
    explicit PitchRingMod(float sample_rate = 48000.0F,
                          std::uint16_t channels = 1U,
                          PitchRingModParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "pitch-ring-mod";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    [[nodiscard]] const PitchRingModParameters& parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float tracked_pitch_hz() const noexcept {
        return tracked_pitch_hz_;
    }
    [[nodiscard]] bool has_tracked_pitch() const noexcept {
        return has_tracked_pitch_;
    }
    [[nodiscard]] float pitch_confidence() const noexcept {
        return pitch_confidence_;
    }
    [[nodiscard]] float carrier_frequency_hz() const noexcept {
        return carrier_frequency_hz_;
    }

    void configure(PitchRingModParameters parameters) noexcept;
    void reset() noexcept override;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr std::size_t kTrackerWindowFrames = 512U;
    static constexpr std::size_t kMinimumAnalysisFrames = 192U;
    static constexpr std::size_t kTrackerUpdateSamples = 64U;
    static constexpr float kMinimumPitchHz = 55.0F;
    static constexpr float kMaximumPitchHz = 1100.0F;
    static constexpr float kMinimumRmsSquared = 1.0e-6F;
    static constexpr float kMaximumStableInput = 1000000.0F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float finite_input(float value) noexcept;
    void add_tracker_sample(float sample) noexcept;
    void estimate_pitch() noexcept;
    [[nodiscard]] std::size_t analysis_window_frames() const noexcept;
    void use_fallback_pitch() noexcept;
    void update_carrier() noexcept;
    void advance_carrier() noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    PitchRingModParameters parameters_{};
    std::array<float, kTrackerWindowFrames> tracker_history_{};
    std::size_t tracker_write_index_ = 0U;
    std::size_t tracker_samples_seen_ = 0U;
    std::size_t tracker_update_counter_ = 0U;
    std::size_t decimation_factor_ = 1U;
    std::size_t decimation_counter_ = 0U;
    float decimation_sum_ = 0.0F;
    float tracker_sample_rate_ = 48000.0F;
    float tracked_pitch_hz_ = 110.0F;
    float pitch_confidence_ = 0.0F;
    bool has_tracked_pitch_ = false;
    float carrier_frequency_hz_ = 110.0F;
    float output_gain_ = 1.0F;
    float phase_sine_ = 0.0F;
    float phase_cosine_ = 1.0F;
    float increment_sine_ = 0.0F;
    float increment_cosine_ = 1.0F;
    std::uint32_t normalization_counter_ = 0U;
};

}  // namespace tabletfx::effects
