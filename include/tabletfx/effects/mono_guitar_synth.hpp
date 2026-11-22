#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/diatonic_harmonizer.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx::effects {

enum class MonoGuitarSynthWaveform : std::uint8_t {
    Sine,
    Triangle,
    Sawtooth,
    Square,
};

// Project-authored controls and bounds. They are not settings copied from a
// Fractal Audio or Fender synthesizer model.
struct MonoGuitarSynthConfig {
    MonoGuitarSynthWaveform waveform = MonoGuitarSynthWaveform::Sawtooth;
    float mix = 0.75F;                 // Linear dry/synth blend, [0, 1].
    float level = 0.6F;                // Linear synth level, [0, 2].
    float gate_threshold_db = -54.0F;  // Peak threshold in dBFS, [-90, 0].
    float attack_ms = 8.0F;            // Synth gate attack, [0, 2000] ms.
    float release_ms = 180.0F;         // Synth gate release, [0, 5000] ms.
    float portamento_ms = 8.0F;        // Pitch smoothing, [0, 250] ms.
};

// Project-authored, one-oscillator guitar synth. The input detector is strictly
// monophonic and uses DiatonicHarmonizer's raw detected-input-frequency API;
// it does not infer polyphony from the detector's polyphonic audio shifter.
//
// The detector accepts fundamentals from about 55 to 1100 Hz and can miss or
// octave-misread chords, distorted input, noisy attacks, and fast note changes.
// At 48 kHz, a complete detector frame spans 1024 input samples / 21.3 ms. The
// oscillator adds no separate block delay, but the generated note waits for a
// confident pitch estimate. Waveforms, envelope behavior, and bounds here are
// project choices and do not claim Fractal or Fender DSP/parameter parity.
// The oscillator is deliberately small: no oversampling or measured aliasing
// quality is claimed, especially for the brighter waveforms.
// The input gate uses a linked channel-peak follower with fixed 1 ms attack,
// 25 ms release, and 6 dB hysteresis; synth attack/release are separate.
// The tracker must process the input to update its estimate; its audio output
// is discarded, so this currently retains the tracker's extra DSP cost/state.
class MonoGuitarSynth final : public tabletfx::Effect {
public:
    explicit MonoGuitarSynth(
        float sample_rate = 48000.0F,
        MonoGuitarSynthConfig config = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "mono-guitar-synth";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;

    void configure(const MonoGuitarSynthConfig& config) noexcept;
    [[nodiscard]] const MonoGuitarSynthConfig& config() const noexcept {
        return config_;
    }

    // Pitch-control response is the detector's complete analysis-frame span;
    // the harmonizer shifter latency is excluded because its audio is discarded.
    [[nodiscard]] std::size_t pitch_response_latency_samples() const noexcept {
        const std::size_t control_latency =
            tracker_.pitch_control_latency_samples();
        const std::size_t shifter_latency = tracker_.latency_samples();
        return control_latency >= shifter_latency
                   ? control_latency - shifter_latency
                   : 0U;
    }
    [[nodiscard]] float pitch_response_latency_ms() const noexcept;
    [[nodiscard]] float detected_input_frequency_hz() const noexcept {
        return tracker_.detected_input_frequency_hz();
    }
    [[nodiscard]] float envelope() const noexcept { return synth_envelope_; }

    // Clears detector, gate, oscillator phase, and envelope state while
    // retaining configured controls.
    void reset() noexcept override;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 96000.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static bool valid_waveform(
        MonoGuitarSynthWaveform waveform) noexcept;
    [[nodiscard]] float time_coefficient(float milliseconds) const noexcept;
    [[nodiscard]] float oscillator_sample() noexcept;
    [[nodiscard]] static float poly_blep(float phase,
                                         float phase_increment) noexcept;
    [[nodiscard]] static float finite_output(double sample) noexcept;

    float sample_rate_;
    DiatonicHarmonizer tracker_;
    MonoGuitarSynthConfig config_{};
    float input_level_ = 0.0F;
    float synth_envelope_ = 0.0F;
    float current_frequency_hz_ = 0.0F;
    float gate_open_threshold_ = 0.0019952623F;
    float gate_close_threshold_ = 0.0009976312F;
    float input_attack_coefficient_ = 0.0F;
    float input_release_coefficient_ = 0.0F;
    float synth_attack_coefficient_ = 0.0F;
    float synth_release_coefficient_ = 0.0F;
    float portamento_coefficient_ = 1.0F;
    double phase_ = 0.0;
    bool gate_open_ = false;
};

}  // namespace tabletfx::effects
