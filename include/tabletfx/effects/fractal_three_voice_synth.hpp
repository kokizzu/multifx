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

// The Blocks Guide describes GUITAR/BASS as an input pitch-tracking range,
// separately from each voice's OFF/ENV ONLY/PITCH+ENV/QUANTIZE tracking mode.
enum class FractalSynthTrackingMode : std::uint8_t {
    Guitar,
    Bass,
};

// Project-authored bounds and defaults for a three-voice monophonic synth.
// The November 2026 Fractal Blocks Guide documents the control names and
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
    // Project default is Guitar to preserve the existing 55–1100 Hz detector.
    // Bass adds a project-authored low-frequency detector to about F#0.
    FractalSynthTrackingMode tracking_mode = FractalSynthTrackingMode::Guitar;
    // Fractal documents a block output Level but gives no range or default.
    // TabletFX uses -24..+12 dB, default 0 dB, with a 10 ms ramp.
    float output_level_db = 0.0F;
};

// Project-authored three-oscillator guitar synth. All three voices share one
// monophonic input pitch detector but have independent oscillator, tracking
// mode, pitch shift/tuning, envelope attack, filter, level and pan. It does not
// track chords; three oscillators do not imply three-note input polyphony.
// Guitar mode retains the reused project tracker at approximately 55–1100 Hz.
// Bass mode adds a project-authored low-frequency detector reaching about F#0
// (23.1 Hz); its analysis window, YIN-style estimation, and confidence bound
// are project choices, not Fractal settings or tracking-quality parity. It uses
// a 1024-sample window plus a 256-sample hop after decimation near 8 kHz, so
// bass-mode onset response can take roughly 0.1–0.22 seconds before buffering.
// Waveforms, most ranges, envelopes, filtering, panning and DSP are project
// choices, not Fractal settings or sound parity. The project-authored output
// Level affects the complete block output; Mix, Balance, Bypass Mode, Global
// Mix and spillover controls remain outside this processor.
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
        return detected_input_frequency_hz_;
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
    static constexpr float kMinimumBassTrackedHz = 23.125F;
    static constexpr float kMaximumBassFallbackHz = 55.0F;
    static constexpr float kBassDetectorTargetRate = 8000.0F;
    static constexpr std::size_t kBassDetectorFrame = 1024U;
    static constexpr std::size_t kBassDetectorHop = 256U;
    static constexpr float kMinimumOutputLevelDb = -24.0F;
    static constexpr float kMaximumOutputLevelDb = 12.0F;
    static constexpr float kOutputLevelSmoothingMs = 10.0F;
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
    [[nodiscard]] static bool valid_tracking_mode(
        FractalSynthTrackingMode mode) noexcept;
    [[nodiscard]] float time_coefficient(float milliseconds) const noexcept;
    [[nodiscard]] float random_bipolar(VoiceState& state) noexcept;
    [[nodiscard]] static float poly_blep(float phase,
                                         float phase_increment) noexcept;
    [[nodiscard]] float oscillator_sample(std::size_t voice) noexcept;
    [[nodiscard]] float filter_sample(std::size_t voice, float sample) noexcept;
    [[nodiscard]] float finite_output(double sample) const noexcept;
    void configure_filter(std::size_t voice) noexcept;
    void process_bass_detector(float mono_sample) noexcept;
    [[nodiscard]] float detect_bass_pitch() const noexcept;
    void reset_bass_detector() noexcept;
    void update_frequency(std::size_t voice,
                          float detected_frequency_hz) noexcept;

    float sample_rate_;
    DiatonicHarmonizer tracker_;
    FractalThreeVoiceSynthConfig config_{};
    std::array<VoiceState, kVoiceCount> state_{};
    std::uint32_t seed_;
    // Fixed-storage project-local low-frequency detector; processing allocates
    // nothing. Numerical detector thresholds are project-authored.
    std::array<float, kBassDetectorFrame> bass_detector_frame_{};
    float output_level_gain_ = 1.0F;
    float output_level_target_gain_ = 1.0F;
    float output_level_gain_step_ = 0.0F;
    std::uint32_t output_level_ramp_frames_remaining_ = 0U;
    std::size_t bass_detector_decimation_ = 1U;
    std::size_t bass_decimation_count_ = 0U;
    std::size_t bass_detector_write_ = 0U;
    std::size_t bass_detector_count_ = 0U;
    std::size_t bass_detector_hop_count_ = 0U;
    float bass_detector_rate_ = 8000.0F;
    float bass_decimation_sum_ = 0.0F;
    float bass_detected_frequency_hz_ = 0.0F;
    float detected_input_frequency_hz_ = 0.0F;
};

}  // namespace tabletfx::effects
