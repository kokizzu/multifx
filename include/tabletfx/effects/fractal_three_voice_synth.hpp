#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/diatonic_harmonizer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx::effects {

enum class FractalSynthWaveform : std::uint8_t {
    Sine,
    Triangle,
    Square,
    Sawtooth,
    Random,
    WhiteNoise,
    PinkNoise,
    Off,
};

enum class FractalSynthTracking : std::uint8_t {
    Off,
    EnvelopeOnly,
    PitchAndEnvelope,
    Quantize,
};

// Project-authored bounds and defaults for a three-voice monophonic synth.
// The June 2023 Fractal Blocks Guide documents the control names and
// behavior but does not give most numeric limits, defaults, or DSP details.
struct FractalSynthVoiceParameters {
    FractalSynthWaveform waveform = FractalSynthWaveform::Sawtooth;
    FractalSynthTracking tracking = FractalSynthTracking::PitchAndEnvelope;
    float frequency_hz = 440.0F;       // [20, 2000] when tracking is Off/EnvOnly.
    int shift_semitones = 0;           // [-24, 24], project range.
    float tune_cents = 0.0F;           // [-50, 50], guide documents +/-50 cents.
    float duty_cycle = 0.5F;           // [0.05, 0.95].
    float pan = 0.0F;                   // [-1, 1], equal-power stereo.
    float filter_frequency_hz = 12000.0F; // [20, min(20000, 0.45*sample rate)].
    float filter_q = 0.707F;            // [0.25, 12], project range.
    float attack_ms = 5.0F;             // [0, 2000], project range.
    float level = 0.25F;                // [0, 1], project range.
};

struct FractalThreeVoiceSynthConfig {
    std::array<FractalSynthVoiceParameters, 3> voices{};
};

// Project-authored three-oscillator guitar synth. All three voices share one
// monophonic input pitch detector but have independent oscillator, tracking
// mode, pitch shift/tuning, envelope attack, filter, level and pan. It does not
// track chords; three oscillators do not imply three-note input polyphony.
// The reused project tracker is limited to approximately 55–1100 Hz; the
// guide's GUITAR/BASS range selector (BASS reaches about F#0) is not implemented.
// Waveforms, most ranges, envelopes, filtering, panning and DSP are project
// choices, not Fractal settings or sound parity. Global Mix/Level/Balance/Bypass
// and spillover controls are intentionally outside this processor.
class FractalThreeVoiceSynth final : public tabletfx::Effect {
public:
    explicit FractalThreeVoiceSynth(
        float sample_rate = 48000.0F,
        FractalThreeVoiceSynthConfig config = {},
        std::uint32_t random_seed = 0x9e3779b9U);

    [[nodiscard]] std::string_view id() const noexcept override {
        return "fractal-three-voice-synth";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;

    void configure(const FractalThreeVoiceSynthConfig& config) noexcept;
    [[nodiscard]] const FractalThreeVoiceSynthConfig& config() const noexcept {
        return config_;
    }
    [[nodiscard]] float detected_input_frequency_hz() const noexcept {
        return tracker_.detected_input_frequency_hz();
    }
    [[nodiscard]] float voice_frequency_hz(std::size_t voice) const noexcept;
    [[nodiscard]] float voice_envelope(std::size_t voice) const noexcept;

    void reset() noexcept override;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

private:
    static constexpr std::size_t kVoiceCount = 3U;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 96000.0F;
    static constexpr float kMinimumTrackedHz = 55.0F;
    static constexpr float kMaximumTrackedHz = 1100.0F;
    static constexpr float kMaximumOutputMagnitude = 1000000.0F;

    struct VoiceState {
        double phase = 0.0;
        float envelope = 0.0F;
        float frequency_hz = 0.0F;
        float attack_coefficient = 0.0F;
        float release_coefficient = 0.0F;
        float pitch_smoothing_coefficient = 0.0F;
        float b0 = 1.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
        float filter_state_1 = 0.0F;
        float filter_state_2 = 0.0F;
        float pan_left = 0.70710678F;
        float pan_right = 0.70710678F;
        std::uint32_t random_state = 1U;
        float random_value = 0.0F;
        std::array<float, 7> pink_state{};
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static bool valid_waveform(FractalSynthWaveform waveform) noexcept;
    [[nodiscard]] static bool valid_tracking(FractalSynthTracking tracking) noexcept;
    [[nodiscard]] float time_coefficient(float milliseconds) const noexcept;
    [[nodiscard]] float random_bipolar(VoiceState& state) noexcept;
    [[nodiscard]] static float poly_blep(float phase,
                                         float phase_increment) noexcept;
    [[nodiscard]] float oscillator_sample(std::size_t voice) noexcept;
    [[nodiscard]] float filter_sample(std::size_t voice, float sample) noexcept;
    [[nodiscard]] float finite_output(double sample) const noexcept;
    void configure_filter(std::size_t voice) noexcept;
    void update_frequency(std::size_t voice,
                          float detected_frequency_hz) noexcept;

    float sample_rate_;
    DiatonicHarmonizer tracker_;
    FractalThreeVoiceSynthConfig config_{};
    std::array<VoiceState, kVoiceCount> state_{};
    std::uint32_t seed_;
};

}  // namespace tabletfx::effects
