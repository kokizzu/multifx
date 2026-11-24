#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tabletfx::effects {

// Project-authored scales. The Fender Tone Master Pro Model Guide v1.1.100
// names a "Diatonic Harmonizer" and lists Mix, Pitch, Key, Tone, Scale, Delay,
// Feedback, Tap Division, and Spillover controls. It does not publish numeric
// ranges or this scale implementation. Fender says its Diatonic Harmonizer
// typically works best with single notes; this does not establish reliable
// chord tracking.
enum class DiatonicScale : std::uint8_t {
    Major,
    NaturalMinor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian,
    HarmonicMinor,
    MelodicMinor,
    MajorPentatonic,
    MinorPentatonic,
    Blues,
    Chromatic,
    Custom,
};

// Project-authored tempo divisions for the effect's delay control.
enum class TapDivision : std::uint8_t {
    Free,
    Half,
    Quarter,
    DottedQuarter,
    QuarterTriplet,
    Eighth,
    DottedEighth,
    EighthTriplet,
    Sixteenth,
};

struct DiatonicHarmonizerConfig {
    // Project-authored defaults and bounds: linear Mix [0, 1], interval [-7,
    // +7] scale degrees in the normal profile or [-24, +24] in the explicit
    // wide profile, key [0, 11] (C..B), tone cutoff [500, 12000] Hz, delay
    // [0, 600] ms, feedback [0, 0.85], tempo [30, 300] BPM. The Fender guide
    // does not specify numeric ranges.
    float mix = 0.35F;
    int pitch_interval = 2;
    int key = 0;
    DiatonicScale scale = DiatonicScale::Major;
    float tone_hz = 8000.0F;
    float delay_ms = 0.0F;
    float feedback = 0.2F;
    TapDivision tap_division = TapDivision::Free;
    float tempo_bpm = 120.0F;
    bool spillover = false;
    // Project-authored custom scale. Entries are ascending semitone offsets
    // [0, 11]; Key transposes these offsets for harmony. The Fractal guide's
    // separate Tonic control is a display reference, not another DSP root.
    // Configuration sanitizes the active count to 4–8 and enforces strictly
    // ascending notes at least one semitone apart.
    std::array<std::uint8_t, 8> custom_scale_notes = {0U, 2U, 4U, 5U,
                                                      7U, 9U, 11U, 0U};
    std::uint8_t custom_scale_note_count = 7U;
};

// A project-authored experimental/practice-only monophonic guitar harmonizer.
// The tracker handles one input pitch at a time; the existing STFT shifter
// supplies the audio transposition. Polyphonic shifting therefore does not
// imply polyphonic note tracking. It does not claim Fender tracking or sound
// parity. At 48 kHz the shifter's nominal stream latency is 256 samples / 5.33
// ms, while the tracker needs a 1,024-sample analysis frame; first-note control
// response is therefore about 26.7 ms before host/device buffering. That
// exceeds the project's provisional <=20 ms live-transposition response gate.
// These are algorithm estimates, not measured end-to-end latency.
//
// Pitch is detected from a fixed 512-sample, decimated frame using a YIN-style
// difference estimator. At 44.1/48/96 kHz, the analysis frame spans about
// 23.2/21.3/21.3 ms. Notes outside the selected scale are mapped to the nearest
// scale tone (ties choose the lower pitch). This is intended for clean, mostly
// single-note guitar input. Chords, distorted/polyphonic input, noisy attacks,
// and fast note changes can produce missed notes or octave errors. It does not
// claim Fender tracking or sound parity.
//
// Stereo uses a linked detector that sums the two channels' analysis energies,
// then applies one shared pitch ratio to independent left/right delay lines.
// Mono is selected by passing null for both right-channel pointers. Processing
// is fixed-memory and performs no heap allocation after construction.
class DiatonicHarmonizer final : public tabletfx::Effect {
public:
    // The normal profile retains its ±7-degree config clamp and ±12-semitone
    // shifter. The explicit 24-degree profile is used by PitchArpeggiator and
    // supports up to ±60 semitones for the project's current scale set.
    explicit DiatonicHarmonizer(
        float sample_rate = 48000.0F,
        int max_pitch_interval_scale_degrees = 7);

    [[nodiscard]] std::string_view id() const noexcept override {
        return "diatonic-harmonizer";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;

    void configure(const DiatonicHarmonizerConfig& config) noexcept;
    [[nodiscard]] const DiatonicHarmonizerConfig& config() const noexcept {
        return config_;
    }

    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }

    // Clears pitch, delay, filter, and crossfade state. The current config and
    // enabled state are retained.
    void reset() noexcept override;

    // Process mono when input_right/output_right are both null. For stereo,
    // provide all four channel pointers. A missing output_right discards the
    // right output; a missing input_right selects mono detection/processing.
    // Input and output buffers may not alias each other.
    void process(const float* input_left,
                 const float* input_right,
                 float* output_left,
                 float* output_right,
                 std::size_t frames) noexcept;

    // Interleaved, in-place Effect API. Per-frame scratch keeps processing
    // allocation-free and safe when the caller's input/output buffers alias.
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

    [[nodiscard]] std::size_t latency_samples() const noexcept;
    [[nodiscard]] float latency_ms() const noexcept;
    [[nodiscard]] std::size_t pitch_control_latency_samples() const noexcept {
        return analysis_latency_samples_ + latency_samples();
    }
    // Raw fundamental estimate from the latest confident monophonic input
    // measurement, in Hz. Returns 0 when pitch validity/confidence is lost.
    // This does not apply key/scale snapping or the configured harmony interval.
    [[nodiscard]] float detected_input_frequency_hz() const noexcept;
    [[nodiscard]] std::size_t effective_delay_samples() const noexcept {
        return delay_samples_;
    }

    // Deterministic project-authored scale mapping helpers, useful to the UI
    // and tests. midi_note is rounded to the nearest equal-tempered note;
    // ties round upward here, while nearest-scale-note ties choose lower.
    [[nodiscard]] static int harmony_midi_note(int midi_note,
                                               int key,
                                               DiatonicScale scale,
                                               int interval_steps) noexcept;
    [[nodiscard]] static int harmony_midi_note_custom(
        int midi_note, int key, int interval_steps,
        const std::array<std::uint8_t, 8>& custom_scale_notes,
        std::uint8_t custom_scale_note_count) noexcept;
    [[nodiscard]] static const char* scale_name(DiatonicScale scale) noexcept;

private:
    static constexpr std::size_t kDetectorFrame = 512;
    static constexpr std::size_t kDetectorHop = 128;
    static constexpr std::size_t kPitchLatencySamples = 256;
    static constexpr std::size_t kDryDelaySize = kPitchLatencySamples + 1;
    static constexpr std::size_t kDelaySize = 65536;

    void detect_pitch() noexcept;
    void pitch_ratio_bounds(float input_frequency_hz,
                            float& minimum,
                            float& maximum) const noexcept;
    [[nodiscard]] float delayed_sample(std::size_t channel,
                                       std::size_t age) const noexcept;
    void clear_delay() noexcept;
    void update_derived_parameters() noexcept;

    float sample_rate_;
    int max_pitch_interval_scale_degrees_;
    tabletfx::PolyPitchShifter pitch_shifter_;
    float detector_rate_;
    std::size_t decimation_;
    std::size_t analysis_latency_samples_;
    std::size_t delay_samples_;
    std::size_t dry_write_ = 0;
    std::size_t delay_write_ = 0;
    std::size_t detector_write_ = 0;
    std::size_t detector_count_ = 0;
    std::size_t detector_hop_count_ = 0;
    std::size_t decimation_count_ = 0;
    std::size_t lost_pitch_samples_ = 0;
    float decimation_sum_[2] = {0.0F, 0.0F};
    float target_ratio_ = 1.0F;
    float smoothed_ratio_ = 1.0F;
    float tone_alpha_ = 1.0F;
    float mix_dry_gain_ = 1.0F;
    float mix_wet_gain_ = 0.0F;
    float feedback_ = 0.0F;
    float confidence_ = 0.0F;
    float detected_input_frequency_hz_ = 0.0F;
    float filter_state_[2] = {0.0F, 0.0F};
    bool enabled_ = true;
    bool had_valid_pitch_ = false;
    DiatonicHarmonizerConfig config_{};

    std::array<std::array<float, kDryDelaySize>, 2> dry_delay_{};
    std::array<std::array<float, kDelaySize>, 2> delay_line_{};
    std::array<std::array<float, kDetectorFrame>, 2> detector_frame_{};
};

}  // namespace tabletfx::effects
