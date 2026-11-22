#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/diatonic_harmonizer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx::effects {

// The June 2023 Fractal Audio Blocks Guide (Pitch block, pp. 76–82;
// Arpeggiator p. 81 and scale table p. 82)
// describes an intelligent, scale-aware 16-step Arpeggiator with Key, Scale,
// Run, Trigger Thresh, Steps, Repeats, tempo-synchronized step duration,
// per-stage scale-degree shifts, and amplitude/pan Shape and Alpha controls.
// The Arpeggiator inherits
// Harmony controls except feedback. The guide does not give numeric bounds for
// most Arpeggiator controls. Fractal's FM9 download page lists v12.0 for "FM9
// Original and FM9 Turbo"; it does not name "FM9 Mark II Turbo" separately.
//
// This module is a project-authored analog, not Fractal DSP, tracking, timing,
// parameter-range, or sound parity. The guide's p. 76 Pitch type table lists
// the Fractal Arpeggiator as one voice, with no voice delay and a ±24 scale-
// degree range. This implementation currently clamps steps to ±7 because its
// shared DiatonicHarmonizer processing path supports no wider interval; that is
// a known range gap. The input tracker is monophonic. It reuses
// the project DiatonicHarmonizer pitch tracker and scale mapping; its nominal
// pitch-control response is above the project's provisional 20 ms live gate.
// Notes outside the selected scale are snapped to its nearest tone by that
// shared mapper, with ties choosing the lower note. Project-authored
// bounds/defaults are documented on PitchArpeggiatorParameters.
// Primary references: https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
// and https://www.fractalaudio.com/fm9-downloads/ (firmware listing checked 2026-11-22).
enum class PitchArpeggiatorDivision : std::uint8_t {
    Free,
    Whole,
    Half,
    DottedHalf,
    Quarter,
    DottedQuarter,
    QuarterTriplet,
    Eighth,
    DottedEighth,
    EighthTriplet,
    Sixteenth,
    DottedSixteenth,
    SixteenthTriplet,
    ThirtySecond,
};

enum class PitchArpeggiatorAmplitudeShape : std::uint8_t {
    Constant,
    RampUp,
    RampDown,
    Triangle,
};

enum class PitchArpeggiatorPanShape : std::uint8_t {
    Center,
    LeftToRight,
    RightToLeft,
    PingPong,
};

struct PitchArpeggiatorParameters {
    int key = 0;  // Project-authored MIDI pitch class, 0=C ... 11=B.
    DiatonicScale scale = DiatonicScale::Major;
    std::array<int, 16> stage_shift_scale_degrees{
        0, 2, 4, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Project-authored bounds: Steps 1–16; each implemented shift −7…+7 scale
    // degrees. Fractal documents ±24 degrees; its wider range remains a gap.
    // Repeats 0…64 where 0 means infinite; tempo 30…300 BPM; Free step time
    // 10…2000 ms. A tempo division sets one step relative to the supplied tempo.
    std::uint8_t steps = 4;
    std::uint8_t repeats = 0;
    bool run = false;
    // Project-authored Trigger Thresh control: −90…0 dBFS. −90 dBFS disables
    // automatic triggering and leaves Run as the manual start/stop control.
    // Above that sentinel, a linked mono/stereo peak envelope starts the
    // sequence when it reaches the threshold and resets it after falling below.
    // Envelope attack (5 ms) and release (80 ms) are fixed project behavior;
    // the guide does not specify numeric bounds or detector timing.
    float trigger_threshold_dbfs = -90.0F;
    PitchArpeggiatorDivision step_division = PitchArpeggiatorDivision::Eighth;
    float tempo_bpm = 120.0F;
    float free_step_ms = 250.0F;

    // Project-authored bounds: Mix 0…1, linear output Level 0…2, and each
    // amplitude/pan Alpha 0…1. The shape choices and transfer curves are also
    // project-authored; they are not copied from Fractal's undocumented shapes.
    float mix = 1.0F;
    float output_level = 1.0F;
    PitchArpeggiatorAmplitudeShape amplitude_shape =
        PitchArpeggiatorAmplitudeShape::Constant;
    float amplitude_alpha = 0.0F;
    PitchArpeggiatorPanShape pan_shape = PitchArpeggiatorPanShape::Center;
    float pan_alpha = 0.0F;
};

class PitchArpeggiator final : public Effect {
public:
    explicit PitchArpeggiator(
        float sample_rate = 48000.0F,
        PitchArpeggiatorParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "pitch-arpeggiator";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] const PitchArpeggiatorParameters& parameters() const noexcept {
        return parameters_;
    }
    void configure(const PitchArpeggiatorParameters& parameters) noexcept;

    // Starts or restarts at step zero in manual mode. Automatic threshold mode
    // is driven by the input envelope during process().
    void trigger() noexcept;
    // Stops and resets the sequence at step zero. A stopped module keeps its
    // pitch tracker warm and produces the unshifted, latency-matched signal.
    void stop() noexcept;
    void reset() noexcept override;

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

    [[nodiscard]] bool running() const noexcept { return running_; }
    [[nodiscard]] std::uint8_t current_step() const noexcept {
        return current_step_;
    }
    [[nodiscard]] std::uint32_t completed_repeats() const noexcept {
        return completed_repeats_;
    }
    [[nodiscard]] int current_scale_shift() const noexcept;
    [[nodiscard]] float current_amplitude() const noexcept {
        return current_amplitude_;
    }
    [[nodiscard]] float current_pan() const noexcept { return current_pan_; }
    [[nodiscard]] float pitch_control_latency_ms() const noexcept {
        return tracker_.latency_ms() +
               static_cast<float>(tracker_.pitch_control_latency_samples() -
                                  tracker_.latency_samples()) *
                   1000.0F / sample_rate_;
    }
    [[nodiscard]] std::size_t pitch_control_latency_samples() const noexcept {
        return tracker_.pitch_control_latency_samples();
    }

    // Deterministic project scale mapping helper used by tests and inspectors.
    [[nodiscard]] static int sequence_midi_note(int input_midi_note,
                                                int key,
                                                DiatonicScale scale,
                                                int scale_degrees) noexcept;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 96000.0F;
    static constexpr float kMinimumTempoBpm = 30.0F;
    static constexpr float kMaximumTempoBpm = 300.0F;
    static constexpr float kMinimumStepMs = 10.0F;
    static constexpr float kMaximumStepMs = 2000.0F;
    static constexpr float kMaximumOutputMagnitude = 1000000.0F;

    [[nodiscard]] static float bounded(float value,
                                       float fallback,
                                       float minimum,
                                       float maximum) noexcept;
    [[nodiscard]] static bool valid_division(
        PitchArpeggiatorDivision division) noexcept;
    [[nodiscard]] static bool valid_amplitude_shape(
        PitchArpeggiatorAmplitudeShape shape) noexcept;
    [[nodiscard]] static bool valid_pan_shape(
        PitchArpeggiatorPanShape shape) noexcept;
    [[nodiscard]] static double division_multiplier(
        PitchArpeggiatorDivision division) noexcept;
    [[nodiscard]] static float eased(float phase, float alpha) noexcept;
    [[nodiscard]] static float finite_output(double sample) noexcept;

    void update_tracker() noexcept;
    void update_trigger_gate(std::span<const float> frame) noexcept;
    void update_step_duration(bool preserve_fraction) noexcept;
    void update_shape_controls() noexcept;
    void advance_sequence() noexcept;
    [[nodiscard]] double normalized_cycle_phase() const noexcept;

    float sample_rate_ = 48000.0F;
    DiatonicHarmonizer tracker_;
    DiatonicHarmonizerConfig tracker_config_{};
    PitchArpeggiatorParameters parameters_{};
    double step_duration_samples_ = 12000.0;
    double step_elapsed_samples_ = 0.0;
    float trigger_threshold_linear_ = 0.0F;
    float trigger_envelope_ = 0.0F;
    float trigger_attack_coefficient_ = 0.0F;
    float trigger_release_coefficient_ = 0.0F;
    std::uint8_t current_step_ = 0;
    std::uint32_t completed_repeats_ = 0;
    float current_amplitude_ = 1.0F;
    float current_pan_ = 0.0F;
    bool running_ = false;
    bool trigger_gate_open_ = false;
};

}  // namespace tabletfx::effects
