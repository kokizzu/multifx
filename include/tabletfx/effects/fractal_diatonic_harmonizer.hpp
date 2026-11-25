#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/diatonic_harmonizer.hpp"
#include "tabletfx/effects/fractal_chromatic_harmonizer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tabletfx::effects {

struct FractalDiatonicVoiceParameters {
    // Project mapping: 0 is unison; positive/negative values move by that many
    // scale steps relative to the detected input note. This intentionally
    // differs from Fractal's Harmony target-degree display, where 1 is unison
    // and 0 is invalid.
    int harmony_steps = 0;
    float detune_cents = 0.0F;
    float level = 0.0F;
    float pan = 0.0F;
    float delay_ms = 0.0F;
    float feedback = 0.0F;
};

struct FractalDiatonicHarmonizerConfig {
    // The Fractal Audio Blocks Guide (November 2026, printed pp. 76 and 80;
    // https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf)
    // lists Dual Diatonic (2 voices), Quad Diatonic (4), and Quad Diatonic
    // Delay (4). It documents Key, Scale, Learn, Harmony 1–4, Glide Time,
    // per-voice Detune ±50 cents, per-voice Delay 0–2000 ms, and 4–8-note
    // custom scales whose degrees are at least a half-step apart. The guide
    // does not publish all control ranges/defaults, tracker quality, latency,
    // or DSP cost. Firmware v12.0 lists FM9 Original/Turbo, not Mark II Turbo
    // by name; exact applicability and installed firmware are unknown.
    //
    // This class is a project-authored monophonic tracker plus the separate
    // project STFT shifter. It does not claim Fractal DSP, settings, tracking,
    // firmware, or sound parity. The input tracker follows one note at a time;
    // it does not perform polyphonic input-note tracking.
    std::uint8_t voice_count = 2;
    int key = 0;
    DiatonicScale scale = DiatonicScale::Major;
    bool learn = false;
    float mix = 0.5F;
    // Project-authored glide bounds/default; the Fractal guide names Glide
    // Time but does not publish its numeric range or default.
    float glide_ms = 0.0F;
    std::array<std::uint8_t, 8> custom_scale_notes{
        0U, 2U, 4U, 5U, 7U, 9U, 11U, 0U};
    std::uint8_t custom_scale_note_count = 7U;
    // Project-authored starter harmony is a third and fifth above the input.
    // Levels, pans, mix, and 0=unison mapping are project choices.
    std::array<FractalDiatonicVoiceParameters, 4> voices{{
        {2, 0.0F, 0.4F, -0.3F, 0.0F, 0.0F},
        {4, 0.0F, 0.4F, 0.3F, 0.0F, 0.0F},
        {0, 0.0F, 0.0F, -0.7F, 0.0F, 0.0F},
        {0, 0.0F, 0.0F, 0.7F, 0.0F, 0.0F},
    }};
};

// Project-authored Dual/Quad intelligent harmony experiment. A short YIN-like
// detector handles notes above roughly 125 Hz using 512 samples at 48 kHz
// equivalent; a 768-sample path targets low-E and nearby notes from about
// 80 Hz, while a 1024-sample fallback covers the lowest guitar range to 55 Hz. The
// audio shifter has 256 samples (5.333 ms at 48 kHz) of fixed latency. The
// tracker waits for two consistent observations on its fast path and three
// for lower notes; control response also depends on the detector window and hop.
//
// The four-voice shifter is allocated once. In the default two-voice mode the
// unused voices stay silent and are skipped by its STFT cores. Delay buffers
// are allocated only when a nonzero per-voice delay is configured. Audio
// processing itself is in-place, fixed-memory, and performs no heap allocation.
class FractalDiatonicHarmonizer final : public tabletfx::Effect {
public:
    explicit FractalDiatonicHarmonizer(float sample_rate = 48000.0F,
                                       std::uint16_t channels = 2);
    ~FractalDiatonicHarmonizer() override = default;

    FractalDiatonicHarmonizer(const FractalDiatonicHarmonizer&) = delete;
    FractalDiatonicHarmonizer& operator=(const FractalDiatonicHarmonizer&) = delete;
    FractalDiatonicHarmonizer(FractalDiatonicHarmonizer&&) = delete;
    FractalDiatonicHarmonizer& operator=(FractalDiatonicHarmonizer&&) = delete;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "fractal-diatonic-harmonizer";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] std::uint32_t latency_samples() const noexcept;
    [[nodiscard]] float algorithmic_latency_ms() const noexcept;
    // Returns the selected detector window plus the required confirmation
    // update hops and shifter latency. This is conservative, not an end-to-end
    // device measurement.
    [[nodiscard]] std::uint32_t pitch_control_latency_samples() const noexcept;
    [[nodiscard]] float detected_input_frequency_hz() const noexcept;
    // Most recent raw tracker candidate before the two-update pitch-confirmation
    // gate. Useful for distinguishing attack transients from the accepted pitch.
    [[nodiscard]] float candidate_input_frequency_hz() const noexcept;
    [[nodiscard]] const FractalDiatonicHarmonizerConfig& config() const noexcept {
        return config_;
    }

    void configure(const FractalDiatonicHarmonizerConfig& config) noexcept;
    [[nodiscard]] bool set_voice_parameters(
        std::uint8_t voice,
        const FractalDiatonicVoiceParameters& parameters) noexcept;
    [[nodiscard]] bool set_custom_scale(
        std::uint8_t note_count,
        const std::array<std::uint8_t, 8>& notes) noexcept;

    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    void reset() noexcept override;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

private:
    class PitchTracker {
    public:
        explicit PitchTracker(float sample_rate) noexcept;
        void reset() noexcept;
        [[nodiscard]] bool process_sample(float left, float right,
                                          float& frequency_hz,
                                          std::uint32_t& window_samples) noexcept;

    private:
        [[nodiscard]] float estimate(std::size_t window_samples,
                                     float minimum_hz,
                                     float maximum_hz) noexcept;

        float sample_rate_;
        std::array<float, 4096> ring_{};
        std::array<float, 4096> frame_{};
        std::array<float, 4096> difference_{};
        std::size_t write_ = 0;
        std::size_t count_ = 0;
        std::size_t hop_count_ = 0;
    };

    [[nodiscard]] static FractalDiatonicHarmonizerConfig sanitize(
        const FractalDiatonicHarmonizerConfig& requested) noexcept;
    void apply_voice(std::uint8_t voice) noexcept;
    void update_harmonies(float input_frequency_hz) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    FractalChromaticHarmonizer shifter_;
    PitchTracker tracker_;
    FractalDiatonicHarmonizerConfig config_{};
    std::array<float, 4> current_shift_semitones_{};
    std::array<float, 4> target_shift_semitones_{};
    std::array<std::array<float, 257>, 2> dry_delay_{};
    std::array<float, 2 * 128> source_block_{};
    std::array<float, 2 * 128> dry_block_{};
    std::size_t dry_write_ = 0;
    float dry_gain_ = 0.5F;
    float wet_gain_ = 0.5F;
    float detected_input_frequency_hz_ = 0.0F;
    float candidate_input_frequency_hz_ = 0.0F;
    float pending_input_frequency_hz_ = 0.0F;
    std::uint8_t pending_pitch_observations_ = 0U;
    std::uint32_t current_control_latency_samples_ = 1536U;
    bool has_valid_pitch_ = false;
    bool enabled_ = true;
};

}  // namespace tabletfx::effects
