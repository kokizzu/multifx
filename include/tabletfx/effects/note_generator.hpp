#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx::effects {

enum class NoteGeneratorWaveform : std::uint8_t {
    Sine,
    Triangle,
    Sawtooth,
    Square,
};

struct NoteGeneratorVoiceParameters {
    NoteGeneratorWaveform waveform = NoteGeneratorWaveform::Sine;
    int note_number = 60;  // MIDI note numbering, project range [0, 127].
    int octave = 0;        // Additional octave shift, project range [-2, 2].
    float level = 0.5F;    // Linear voice gain, project range [0, 1]; zero mutes.
};

struct NoteGeneratorParameters {
    std::uint8_t voice_count = 1;  // Project range [1, 4].
    std::array<NoteGeneratorVoiceParameters, 4> voices{};
    float output_level = 0.5F;     // Linear summed-output gain, project range [0, 1].
};

// Project-authored, free-running oscillator note generator. It overwrites the
// input buffer with generated audio and never reads input samples, tracks guitar
// pitch, or consumes MIDI notes. The note_number field is a static control, not
// a MIDI input. One to four fixed oscillators can run at once; a voice's level
// of zero mutes it. Stereo output is dual mono.
//
// Defaults and bounds are project choices, not copied manufacturer settings.
// The control surface takes inspiration from documented free-running/manual
// oscillator and static-note generator concepts; this implementation does not
// claim parity with a Line 6 or Fractal effect. Saw and square use a small
// polyBLEP edge correction; the triangle is a compact piecewise waveform, so
// high notes can still contain aliasing. No oversampling or vendor DSP parity
// is claimed. All audio state is fixed-size and process() allocates nothing.
class NoteGenerator final : public tabletfx::Effect {
public:
    explicit NoteGenerator(float sample_rate = 48000.0F,
                           NoteGeneratorParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "note-generator";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    // Apply controls without resetting oscillator phase. Call between audio
    // blocks or under the host's normal parameter synchronization.
    void configure(const NoteGeneratorParameters& parameters) noexcept;
    [[nodiscard]] const NoteGeneratorParameters& parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float voice_frequency_hz(std::size_t voice) const noexcept {
        return voice < kMaximumVoices ? voice_frequency_hz_[voice] : 0.0F;
    }

    // Rewinds all oscillators to phase zero while retaining configured controls.
    void reset() noexcept override;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

private:
    static constexpr std::size_t kMaximumVoices = 4U;
    static constexpr float kDefaultSampleRate = 48000.0F;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 96000.0F;
    static constexpr float kMaximumFundamentalFraction = 0.45F;
    static constexpr float kMaximumFundamentalHz = 20000.0F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;

    [[nodiscard]] static bool valid_waveform(
        NoteGeneratorWaveform waveform) noexcept;
    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float waveform_sample(NoteGeneratorWaveform waveform,
                                               float phase,
                                               float phase_increment) noexcept;
    [[nodiscard]] static float poly_blep(float phase,
                                         float phase_increment) noexcept;
    void prepare_frequencies() noexcept;

    float sample_rate_ = kDefaultSampleRate;
    NoteGeneratorParameters parameters_{};
    std::array<double, kMaximumVoices> phase_{};
    std::array<double, kMaximumVoices> phase_increment_{};
    std::array<float, kMaximumVoices> voice_frequency_hz_{};
};

inline NoteGenerator::NoteGenerator(
    const float sample_rate,
    const NoteGeneratorParameters parameters) noexcept
    : sample_rate_(
          sample_rate >= kMinimumSampleRate && sample_rate <= kMaximumSampleRate
                  ? sample_rate
                  : kDefaultSampleRate) {
    configure(parameters);
}

inline bool NoteGenerator::valid_waveform(
    const NoteGeneratorWaveform waveform) noexcept {
    switch (waveform) {
        case NoteGeneratorWaveform::Sine:
        case NoteGeneratorWaveform::Triangle:
        case NoteGeneratorWaveform::Sawtooth:
        case NoteGeneratorWaveform::Square:
            return true;
    }
    return false;
}

inline float NoteGenerator::bounded(const float value, const float fallback,
                                    const float minimum,
                                    const float maximum) noexcept {
    if (!(value == value)) {
        return fallback;
    }
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

inline void NoteGenerator::configure(
    const NoteGeneratorParameters& parameters) noexcept {
    NoteGeneratorParameters sanitized = parameters;
    if (sanitized.voice_count < 1U) {
        sanitized.voice_count = 1U;
    } else if (sanitized.voice_count > kMaximumVoices) {
        sanitized.voice_count = static_cast<std::uint8_t>(kMaximumVoices);
    }

    for (std::size_t voice = 0; voice < kMaximumVoices; ++voice) {
        auto& current = sanitized.voices[voice];
        const auto& previous = parameters_.voices[voice];
        if (!valid_waveform(current.waveform)) {
            current.waveform = previous.waveform;
        }
        if (current.note_number < 0) {
            current.note_number = 0;
        } else if (current.note_number > 127) {
            current.note_number = 127;
        }
        if (current.octave < -2) {
            current.octave = -2;
        } else if (current.octave > 2) {
            current.octave = 2;
        }
        current.level = bounded(current.level, previous.level, 0.0F, 1.0F);
    }
    sanitized.output_level = bounded(
        sanitized.output_level, parameters_.output_level, 0.0F, 1.0F);
    parameters_ = sanitized;
    prepare_frequencies();
}

inline void NoteGenerator::prepare_frequencies() noexcept {
    const float maximum_frequency =
        sample_rate_ * kMaximumFundamentalFraction < kMaximumFundamentalHz
            ? sample_rate_ * kMaximumFundamentalFraction
            : kMaximumFundamentalHz;
    for (std::size_t voice = 0; voice < kMaximumVoices; ++voice) {
        const int semitones = parameters_.voices[voice].note_number +
                              12 * parameters_.voices[voice].octave;
        const float frequency = 440.0F *
            std::exp2(static_cast<float>(semitones - 69) / 12.0F);
        voice_frequency_hz_[voice] =
            frequency < maximum_frequency ? frequency : maximum_frequency;
        phase_increment_[voice] =
            static_cast<double>(voice_frequency_hz_[voice]) / sample_rate_;
    }
}

inline float NoteGenerator::poly_blep(const float phase,
                                      const float phase_increment) noexcept {
    if (phase_increment <= 0.0F) {
        return 0.0F;
    }
    if (phase < phase_increment) {
        const float t = phase / phase_increment;
        return t + t - t * t - 1.0F;
    }
    if (phase > 1.0F - phase_increment) {
        const float t = (phase - 1.0F) / phase_increment;
        return t * t + t + t + 1.0F;
    }
    return 0.0F;
}

inline float NoteGenerator::waveform_sample(
    const NoteGeneratorWaveform waveform, const float phase,
    const float phase_increment) noexcept {
    switch (waveform) {
        case NoteGeneratorWaveform::Sine:
            return std::sin(kTwoPi * phase);
        case NoteGeneratorWaveform::Triangle:
            return 1.0F - 4.0F *
                (phase < 0.5F ? 0.5F - phase : phase - 0.5F);
        case NoteGeneratorWaveform::Sawtooth:
            return 2.0F * phase - 1.0F - poly_blep(phase, phase_increment);
        case NoteGeneratorWaveform::Square: {
            const float opposite_edge = phase < 0.5F
                                            ? phase + 0.5F
                                            : phase - 0.5F;
            const float raw = phase < 0.5F ? 1.0F : -1.0F;
            return raw + poly_blep(phase, phase_increment) -
                   poly_blep(opposite_edge, phase_increment);
        }
    }
    return 0.0F;
}

inline void NoteGenerator::reset() noexcept {
    phase_.fill(0.0);
}

inline void NoteGenerator::process(std::span<float> interleaved,
                                   const std::uint16_t channels) noexcept {
    if (channels != 1U && channels != 2U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    const float output_level = parameters_.output_level;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        double mixed = 0.0;
        for (std::size_t voice = 0; voice < parameters_.voice_count; ++voice) {
            const float phase = static_cast<float>(phase_[voice]);
            const float increment = static_cast<float>(phase_increment_[voice]);
            mixed += static_cast<double>(waveform_sample(
                          parameters_.voices[voice].waveform, phase, increment)) *
                     parameters_.voices[voice].level;
            phase_[voice] += phase_increment_[voice];
            if (phase_[voice] >= 1.0) {
                phase_[voice] -= 1.0;
            }
        }

        const float output = static_cast<float>(mixed) * output_level;
        const std::size_t offset = frame * channels;
        interleaved[offset] = output;
        if (channels == 2U) {
            interleaved[offset + 1U] = output;
        }
    }
}

// A separate four-oscillator source for the documented Helix Stadium 4 OSC
// Generator capability. Line 6 does not publish this model's parameter table;
// these note, waveform, level, and safe-output defaults are project-authored.
// It generates audio without reading or tracking the instrument input. Keep it
// disabled in the host until deliberately enabled; the default summed tone is
// bounded below normal line level. It shares the fixed-size oscillator core.
class FourOscGenerator final : public tabletfx::Effect {
public:
    explicit FourOscGenerator(
        const float sample_rate = 48000.0F) noexcept
        : generator_(sample_rate, safe_defaults()) {}

    [[nodiscard]] std::string_view id() const noexcept override {
        return "four-osc-free-generator";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(const NoteGeneratorParameters& parameters) noexcept {
        NoteGeneratorParameters four_voices = parameters;
        four_voices.voice_count = 4U;
        generator_.configure(four_voices);
    }
    [[nodiscard]] const NoteGeneratorParameters& parameters() const noexcept {
        return generator_.parameters();
    }
    [[nodiscard]] float voice_frequency_hz(
        const std::size_t voice) const noexcept {
        return generator_.voice_frequency_hz(voice);
    }

    void reset() noexcept override { generator_.reset(); }
    void process(std::span<float> interleaved,
                 const std::uint16_t channels) noexcept override {
        generator_.process(interleaved, channels);
    }

private:
    [[nodiscard]] static NoteGeneratorParameters safe_defaults() noexcept {
        NoteGeneratorParameters parameters;
        parameters.voice_count = 4U;
        parameters.voices[0] = {
            NoteGeneratorWaveform::Sine, 48, 0, 0.15F}; // C3
        parameters.voices[1] = {
            NoteGeneratorWaveform::Sine, 55, 0, 0.15F}; // G3
        parameters.voices[2] = {
            NoteGeneratorWaveform::Sine, 60, 0, 0.15F}; // C4
        parameters.voices[3] = {
            NoteGeneratorWaveform::Sine, 64, 0, 0.15F}; // E4
        parameters.output_level = 0.35F;
        return parameters;
    }

    NoteGenerator generator_;
};

}  // namespace tabletfx::effects
