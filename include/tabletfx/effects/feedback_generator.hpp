#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/mono_guitar_synth.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx::effects {

enum class FeedbackGeneratorType : std::uint8_t {
    OctaveDown,
    Unison,
    OctaveUp,
    OctaveAndFifth,
    TwoOctaves,
    TwoOctavesAndThird,
    TwoOctavesAndFifth,
    TwoOctavesAndSeventh,
};

// These are project-authored single-oscillator interval choices. The names
// describe the selected interval; they do not represent stacked voices or
// Line 6's proprietary Feedbacker mode behavior.

enum class FeedbackGeneratorDryKill : std::uint8_t {
    Off,
    On,
};

// Project-authored controls and bounds for a mono-tracked generated-tone
// effect. They are not copied manufacturer settings or a Line 6 DSP model.
struct FeedbackGeneratorParameters {
    FeedbackGeneratorType type = FeedbackGeneratorType::Unison;
    float feedback_gain = 0.35F;           // [0, 1].
    float attack_ms = 15.0F;               // [0, 2000] ms.
    float release_ms = 600.0F;             // [0, 5000] ms.
    FeedbackGeneratorDryKill dry_kill = FeedbackGeneratorDryKill::Off;
    float dry_level = 1.0F;                // Linear gain, [0, 1].
    float silence_threshold_db = -48.0F;    // [−90, 0] dBFS.
};

/*
 * Project-authored, mono-tracked feedback-tone generator. It detects one
 * fundamental with the existing MonoGuitarSynth tracker and generates a sine
 * tone at a selected project-authored fixed interval from −12 to +35
 * semitones. Each mode generates one oscillator only. This is a bounded
 * synthesized sustain effect, not physical speaker feedback and not a
 * polyphonic chord/reference detector. The tracker covers approximately
 * 55–1,100 Hz and can miss or octave-misread chords, distorted input, noisy
 * attacks, and fast note changes. Its 1,024-sample analysis frame is about
 * 21.3 ms at 48 kHz. The generated oscillator adds no pitch-shifter latency;
 * pitch can only respond after the tracker provides an estimate. Pitch is
 * smoothed with a fixed 8 ms one-pole glide.
 *
 * Control bounds/defaults, sine generation, and output limiting are project
 * choices. The effect intentionally omits changing/random harmonic modes,
 * lowest-vs-loudest chord reference, onset/offset thresholds, Retrigger, and
 * Trails. Configure parameters on the audio thread or while processing is
 * stopped. The effect is expected to be bypassed by default in a host.
 */
class FeedbackGenerator final : public tabletfx::Effect {
public:
    explicit FeedbackGenerator(float sample_rate = 48000.0F,
                               std::uint16_t channels = 1,
                               FeedbackGeneratorParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "feedback-generator";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] const FeedbackGeneratorParameters& parameters() const noexcept {
        return parameters_;
    }
    // Detector-control response estimate; the generated oscillator itself
    // adds no block or pitch-shifter delay.
    [[nodiscard]] std::size_t pitch_response_latency_samples() const noexcept {
        return synth_.pitch_response_latency_samples();
    }
    [[nodiscard]] float pitch_response_latency_ms() const noexcept {
        return synth_.pitch_response_latency_ms();
    }

    void configure(const FeedbackGeneratorParameters& parameters) noexcept;
    void reset() noexcept override;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

private:
    static constexpr std::size_t kMaximumChunkFrames = 256U;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 96000.0F;
    static constexpr float kSafetyCeiling = 1.0F;
    static constexpr float kPitchGlideMs = 8.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static bool valid_type(FeedbackGeneratorType type) noexcept;
    [[nodiscard]] static bool valid_dry_kill(
        FeedbackGeneratorDryKill dry_kill) noexcept;
    [[nodiscard]] static float finite_input(float sample) noexcept;
    [[nodiscard]] float interval_for_type() const noexcept;
    [[nodiscard]] float pitch_glide_coefficient() const noexcept;
    [[nodiscard]] float oscillator_sample(float frequency_hz) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    MonoGuitarSynth synth_;
    FeedbackGeneratorParameters parameters_{};
    float pitch_interval_semitones_ = 0.0F;
    float pitch_glide_coefficient_ = 0.0F;
    float current_frequency_hz_ = 0.0F;
    float target_frequency_hz_ = 0.0F;
    double phase_ = 0.0;
    std::vector<float> dry_buffer_;
};

}  // namespace tabletfx::effects
