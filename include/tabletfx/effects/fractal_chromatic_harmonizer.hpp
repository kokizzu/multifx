#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace tabletfx::effects {

// Project-authored fixed-interval, multi-voice pitch effect. The Fractal Audio
// Blocks Guide, November 2026 edition (Pitch Block, printed pp. 76, 79;
// https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf)
// documents Dual Chromatic, Quad Chromatic, and Quad Chromatic Delay with per-voice shifts
// through ±24 semitones, detune through ±50 cents, level, pan, and delays up to
// 2000 ms. This implementation uses the same broad controls, not Fractal DSP,
// sound, or firmware parity. It uses one shared input analysis and one
// phase-vocoder pitch core per voice. MonoSum (the default) sums a stereo input
// to mono. StereoRoute sends left to voices 1/3 and right to voices 2/4 (or L/R
// to voices 1/2 in the two-voice profile), following the guide's routing summary.
// The guide names FM9 as a family and does not specify Mark II Turbo behavior.
// The FM9 downloads page (https://www.fractalaudio.com/fm9-downloads/) lists
// v12.0 dated 2026-07-07 for Original/Turbo, not Mark II Turbo; exact firmware
// applicability to Mark II Turbo remains unverified.
//
// Delay is post-shift, per-voice, with project-authored feedback [0, 0.9].
// This resembles the documented Quad Chromatic Delay signal ordering, but the
// guide does not specify numeric feedback bounds and this effect does not
// implement tempo sync, cross-feedback, the Dual Chromatic pre-shift feedback
// topology, or the manufacturer's routing modes beyond the routing summary.
//
// Control units/bounds are project-authored: shift [-24, 24] semitones,
// detune [-50, 50] cents, linear level [0, 2], pan [-1, 1], delay [0, 2000]
// ms, and feedback [0, 0.9]. Updates are allocation-free and intended for the
// audio thread or while processing is stopped. The processor accepts mono or
// stereo in-place buffers matching its constructor configuration.
class FractalChromaticHarmonizer final : public tabletfx::Effect {
public:
    enum class InputMode : std::uint8_t {
        MonoSum,
        StereoRoute,
    };

    struct VoiceParameters {
        float shift_semitones = 0.0F;
        float detune_cents = 0.0F;
        float level = 0.0F;
        float pan = 0.0F;
        float delay_ms = 0.0F;
        float feedback = 0.0F;
    };

    explicit FractalChromaticHarmonizer(float sample_rate = 48000.0F,
                                        std::uint16_t channels = 2,
                                        std::uint8_t voices = 4,
                                        InputMode input_mode = InputMode::MonoSum);
    ~FractalChromaticHarmonizer() override;

    FractalChromaticHarmonizer(const FractalChromaticHarmonizer&) = delete;
    FractalChromaticHarmonizer& operator=(const FractalChromaticHarmonizer&) = delete;
    FractalChromaticHarmonizer(FractalChromaticHarmonizer&&) = delete;
    FractalChromaticHarmonizer& operator=(FractalChromaticHarmonizer&&) = delete;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "fractal-chromatic-harmonizer";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] std::uint8_t voice_count() const noexcept { return voices_; }
    [[nodiscard]] InputMode input_mode() const noexcept { return input_mode_; }
    [[nodiscard]] float algorithmic_latency_ms() const noexcept;
    [[nodiscard]] std::uint32_t latency_samples() const noexcept;
    [[nodiscard]] VoiceParameters voice_parameters(std::uint8_t voice) const noexcept;

    // Returns false for an invalid voice index or non-finite parameter. Finite
    // values are clamped to the project-authored ranges documented above.
    [[nodiscard]] bool set_voice_parameters(
        std::uint8_t voice, const VoiceParameters& parameters) noexcept;

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    float sample_rate_;
    std::uint16_t channels_;
    std::uint8_t voices_;
    InputMode input_mode_;
};

} // namespace tabletfx::effects
