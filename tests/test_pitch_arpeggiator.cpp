#include "tabletfx/effects/pitch_arpeggiator.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};
constexpr double kPi = 3.14159265358979323846264338327950288;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Keep the large fixed-state pitch trackers out of the test stack.
tabletfx::effects::PitchArpeggiator timing_effect(8000.0F);
tabletfx::effects::PitchArpeggiator shape_effect(8000.0F);
tabletfx::effects::PitchArpeggiator partition_whole(8000.0F);
tabletfx::effects::PitchArpeggiator partition_split(8000.0F);
tabletfx::effects::PitchArpeggiator threshold_effect(8000.0F);
tabletfx::effects::PitchArpeggiator threshold_whole(8000.0F);
tabletfx::effects::PitchArpeggiator threshold_split(8000.0F);
tabletfx::effects::PitchArpeggiator allocation_effect(8000.0F);
tabletfx::effects::PitchArpeggiator pitch_effect(48000.0F);
tabletfx::effects::PitchArpeggiator wide_pitch_effect(16000.0F);

void configure_free_run(tabletfx::effects::PitchArpeggiator& effect,
                        std::uint8_t steps,
                        float step_ms,
                        std::uint8_t repeats = 0U) {
    tabletfx::effects::PitchArpeggiatorParameters parameters;
    parameters.key = 0;
    parameters.scale = tabletfx::effects::DiatonicScale::Major;
    parameters.steps = steps;
    parameters.repeats = repeats;
    parameters.step_division =
        tabletfx::effects::PitchArpeggiatorDivision::Free;
    parameters.free_step_ms = step_ms;
    parameters.run = true;
    parameters.mix = 1.0F;
    effect.configure(parameters);
}

double tone_amplitude(const std::vector<float>& samples,
                      std::size_t start,
                      std::size_t length,
                      double frequency,
                      double sample_rate) {
    double cosine = 0.0;
    double sine = 0.0;
    double window_sum = 0.0;
    for (std::size_t i = 0; i < length; ++i) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * kPi * static_cast<double>(i) /
            static_cast<double>(length - 1U));
        const double angle = 2.0 * kPi * frequency *
                             static_cast<double>(i) / sample_rate;
        const double value = static_cast<double>(samples[start + i]) * window;
        cosine += value * std::cos(angle);
        sine += value * std::sin(angle);
        window_sum += window;
    }
    return 2.0 * std::hypot(cosine, sine) / window_sum;
}

void test_scale_mapping_and_pitch_sequence() {
    using tabletfx::effects::DiatonicScale;
    using tabletfx::effects::PitchArpeggiator;

    require(PitchArpeggiator::sequence_midi_note(
                57, 0, DiatonicScale::Major, 0) == 57,
            "zero scale-degree shift must preserve the tracked note");
    require(PitchArpeggiator::sequence_midi_note(
                57, 0, DiatonicScale::Major, 2) == 60,
            "A in C major shifted by two scale degrees must become C");
    require(PitchArpeggiator::sequence_midi_note(
                57, 0, DiatonicScale::Major, 4) == 64,
            "A in C major shifted by four scale degrees must become E");
    require(PitchArpeggiator::sequence_midi_note(
                62, 7, DiatonicScale::Major, 2) == 66,
            "G-major arpeggio steps must follow the selected scale");
    require(PitchArpeggiator::sequence_midi_note(
                60, 0, DiatonicScale::Major, 24) == 101,
            "a positive 24-degree C-major shift must reach F7");
    require(PitchArpeggiator::sequence_midi_note(
                60, 0, DiatonicScale::Major, -24) == 19,
            "a negative 24-degree C-major shift must reach G0");
    require(PitchArpeggiator::sequence_midi_note(
                57, 0, DiatonicScale::MajorPentatonic, 24) == 115,
            "a positive 24-degree pentatonic shift must use the selected scale");
    require(PitchArpeggiator::sequence_midi_note(
                81, 0, DiatonicScale::MajorPentatonic, -24) == 24,
            "a negative 24-degree pentatonic shift must use the selected scale");

    configure_free_run(pitch_effect, 2U, 250.0F);
    auto parameters = pitch_effect.parameters();
    parameters.stage_shift_scale_degrees[0] = 0;
    parameters.stage_shift_scale_degrees[1] = 2;
    pitch_effect.configure(parameters);

    constexpr std::size_t kFrames = 18000;
    std::vector<float> rendered(kFrames);
    double phase = 0.0;
    for (float& sample : rendered) {
        sample = static_cast<float>(0.25 * std::sin(phase));
        phase += 2.0 * kPi * 220.0 / 48000.0;
        if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
    }
    pitch_effect.process(rendered, 1U);

    const double unshifted = tone_amplitude(
        rendered, 4096U, 5904U, 220.0, 48000.0);
    const double shifted = tone_amplitude(
        rendered, 13000U, 5000U, 261.625565, 48000.0);
    require(unshifted > 0.02,
            "the zero-shift first stage must preserve the tracked input pitch");
    require(shifted > 0.02 && shifted > unshifted * 0.5,
            "the second scale-degree stage must produce its shifted pitch");
    require(pitch_effect.current_scale_shift() == 2,
            "the active step must expose its configured scale-degree shift");
}

void test_wide_scale_degree_extremes_reach_audio_output() {
    using tabletfx::effects::DiatonicScale;
    using tabletfx::effects::PitchArpeggiator;
    using tabletfx::effects::PitchArpeggiatorParameters;

    struct Case {
        int input_midi;
        DiatonicScale scale;
        int scale_degrees;
    };
    constexpr std::array<Case, 4> cases{{
        {60, DiatonicScale::Major, 24},
        {60, DiatonicScale::Major, -24},
        {57, DiatonicScale::MajorPentatonic, 24},
        {81, DiatonicScale::MajorPentatonic, -24},
    }};
    constexpr float sample_rate = 16000.0F;
    constexpr std::size_t frames = 32000U;
    constexpr std::size_t measure_start = 12000U;
    constexpr std::size_t measure_frames = 16000U;
    std::vector<float> audio(frames);

    for (const Case& test_case : cases) {
        PitchArpeggiatorParameters parameters;
        parameters.key = 0;
        parameters.scale = test_case.scale;
        parameters.steps = 1U;
        parameters.stage_shift_scale_degrees[0] = test_case.scale_degrees;
        parameters.run = true;
        parameters.step_division =
            tabletfx::effects::PitchArpeggiatorDivision::Free;
        parameters.free_step_ms = 2000.0F;
        parameters.mix = 1.0F;
        wide_pitch_effect.configure(parameters);
        wide_pitch_effect.reset();

        const double input_hz = 440.0 * std::exp2(
            static_cast<double>(test_case.input_midi - 69) / 12.0);
        const int target_midi = PitchArpeggiator::sequence_midi_note(
            test_case.input_midi, parameters.key, parameters.scale,
            test_case.scale_degrees);
        const double target_hz = 440.0 * std::exp2(
            static_cast<double>(target_midi - 69) / 12.0);
        require(target_hz >= 20.0 && target_hz < 0.45 * sample_rate,
                "extreme degree test tones must remain in the wide shifter's safe output band");

        for (std::size_t frame = 0; frame < frames; ++frame) {
            audio[frame] = static_cast<float>(0.25 * std::sin(
                2.0 * kPi * input_hz * static_cast<double>(frame) /
                sample_rate));
        }
        wide_pitch_effect.process(audio, 1U);
        const double amplitude = tone_amplitude(
            audio, measure_start, measure_frames, target_hz, sample_rate);
        require(std::isfinite(amplitude) && amplitude > 0.005,
                "the ±24-degree arpeggiator steps must produce their mapped output pitches");
    }
}

void test_step_timing_finite_repeats_and_trigger() {
    configure_free_run(timing_effect, 2U, 10.0F, 2U);
    std::array<float, 320> audio{};

    timing_effect.process(std::span<float>(audio.data(), 79U), 1U);
    require(timing_effect.current_step() == 0U,
            "the first step must remain active before its sample deadline");
    timing_effect.process(std::span<float>(audio.data() + 79U, 1U), 1U);
    require(timing_effect.current_step() == 1U &&
                timing_effect.completed_repeats() == 0U,
            "step zero must advance on the exact 10 ms boundary at 8 kHz");

    timing_effect.process(std::span<float>(audio.data(), 80U), 1U);
    require(timing_effect.current_step() == 0U &&
                timing_effect.completed_repeats() == 1U,
            "finishing all configured steps must count one completed repeat");
    timing_effect.process(std::span<float>(audio.data(), 160U), 1U);
    require(!timing_effect.running() &&
                timing_effect.current_step() == 0U &&
                timing_effect.completed_repeats() == 2U,
            "a finite sequence must stop and reset after the requested repeats");

    timing_effect.trigger();
    require(timing_effect.running() && timing_effect.current_step() == 0U &&
                timing_effect.completed_repeats() == 0U,
            "trigger must restart a completed sequence from step zero");
    timing_effect.process(std::span<float>(audio.data(), 100U), 1U);
    require(timing_effect.current_step() == 1U,
            "the triggered sequence must continue on its step clock");
    timing_effect.stop();
    require(!timing_effect.running() && timing_effect.current_step() == 0U &&
                timing_effect.completed_repeats() == 0U,
            "stop must reset the sequence position and repeat count");
    timing_effect.process(std::span<float>(audio.data(), 160U), 1U);
    require(timing_effect.current_step() == 0U && !timing_effect.running(),
            "a stopped sequence must not continue advancing");

    configure_free_run(timing_effect, 3U, 10.0F);
    timing_effect.process(std::span<float>(audio.data(), 200U), 1U);
    timing_effect.reset();
    require(timing_effect.running() && timing_effect.current_step() == 0U &&
                timing_effect.completed_repeats() == 0U,
            "reset must clear phase/cycles while retaining Run state");
}

void test_threshold_trigger_manual_mode_release_and_retrigger() {
    using tabletfx::effects::PitchArpeggiatorDivision;
    using tabletfx::effects::PitchArpeggiatorParameters;

    PitchArpeggiatorParameters parameters;
    parameters.steps = 4U;
    parameters.run = false;
    parameters.trigger_threshold_dbfs = -20.0F;
    parameters.step_division = PitchArpeggiatorDivision::Free;
    parameters.free_step_ms = 10.0F;
    parameters.repeats = 0U;
    threshold_effect.configure(parameters);

    const std::array<float, 1> playing_sample{0.5F};
    bool started = false;
    for (std::size_t frame = 0; frame < 1000U && !started; ++frame) {
        auto sample = playing_sample;
        threshold_effect.process(sample, 1U);
        started = threshold_effect.running();
    }
    require(started && threshold_effect.current_step() == 0U,
            "a rising input envelope must auto-start at step zero");
    std::array<float, 120> held_audio{};
    held_audio.fill(0.5F);
    threshold_effect.process(held_audio, 1U);
    require(threshold_effect.running() && threshold_effect.current_step() > 0U,
            "the automatic trigger must advance while the input stays above threshold");

    std::array<float, 2400> silence{};
    threshold_effect.process(silence, 1U);
    require(!threshold_effect.running() &&
                threshold_effect.current_step() == 0U &&
                threshold_effect.completed_repeats() == 0U,
            "release below threshold must stop and reset the sequence");

    started = false;
    for (std::size_t frame = 0; frame < 1000U && !started; ++frame) {
        auto sample = playing_sample;
        threshold_effect.process(sample, 1U);
        started = threshold_effect.running();
    }
    require(started && threshold_effect.current_step() == 0U &&
                threshold_effect.completed_repeats() == 0U,
            "a new rising envelope must retrigger from step one");

    parameters.trigger_threshold_dbfs = -90.0F;
    parameters.run = false;
    threshold_effect.configure(parameters);
    threshold_effect.reset();
    std::array<float, 160> loud_audio{};
    loud_audio.fill(0.5F);
    threshold_effect.process(loud_audio, 1U);
    require(!threshold_effect.running(),
            "the minimum threshold must disable auto-trigger while Run is off");
    parameters.run = true;
    threshold_effect.configure(parameters);
    threshold_effect.process(std::span<float>(loud_audio.data(), 80U), 1U);
    require(threshold_effect.running() && threshold_effect.current_step() == 1U,
            "at the minimum threshold, manual Run must control the sequence");
}

void test_threshold_trigger_partition_invariance() {
    using tabletfx::effects::PitchArpeggiatorDivision;
    using tabletfx::effects::PitchArpeggiatorParameters;

    PitchArpeggiatorParameters parameters;
    parameters.steps = 3U;
    parameters.stage_shift_scale_degrees = {0, 2, -1, 0, 0, 0, 0, 0,
                                             0, 0, 0, 0, 0, 0, 0, 0};
    parameters.run = false;
    parameters.trigger_threshold_dbfs = -24.0F;
    parameters.step_division = PitchArpeggiatorDivision::Free;
    parameters.free_step_ms = 17.0F;
    parameters.repeats = 0U;
    threshold_whole.configure(parameters);
    threshold_split.configure(parameters);

    constexpr std::size_t kFrames = 7000U;
    std::array<float, kFrames> whole{};
    std::array<float, kFrames> split{};
    for (std::size_t frame = 0; frame < kFrames; ++frame) {
        float sample = 0.0F;
        if ((frame < 1200U) || (frame >= 3200U && frame < 4600U)) {
            const float amplitude = frame < 1200U ? 0.4F : 0.7F;
            sample = amplitude * static_cast<float>(std::sin(
                2.0 * kPi * 220.0 * static_cast<double>(frame) / 8000.0));
        }
        whole[frame] = split[frame] = sample;
    }
    threshold_whole.process(whole, 1U);

    constexpr std::array<std::size_t, 9> chunks{
        1U, 13U, 5U, 127U, 2U, 61U, 257U, 3U, 41U};
    std::size_t offset = 0;
    std::size_t chunk = 0;
    while (offset < kFrames) {
        const std::size_t count = std::min(
            chunks[chunk++ % chunks.size()], kFrames - offset);
        threshold_split.process(
            std::span<float>(split.data() + offset, count), 1U);
        offset += count;
    }
    for (std::size_t frame = 0; frame < kFrames; ++frame) {
        require(std::isfinite(whole[frame]) && std::isfinite(split[frame]),
                "automatic-trigger output must remain finite");
        require(std::abs(whole[frame] - split[frame]) < 1.0e-5F,
                "automatic-trigger output must not depend on block partitioning");
    }
    require(threshold_whole.running() == threshold_split.running() &&
                threshold_whole.current_step() == threshold_split.current_step() &&
                threshold_whole.completed_repeats() ==
                    threshold_split.completed_repeats(),
            "automatic-trigger state must be invariant to caller block sizes");
}

void test_tempo_division_and_tempo_change() {
    using tabletfx::effects::PitchArpeggiatorDivision;
    using tabletfx::effects::PitchArpeggiatorParameters;

    PitchArpeggiatorParameters parameters;
    parameters.steps = 2U;
    parameters.run = true;
    parameters.step_division = PitchArpeggiatorDivision::Quarter;
    parameters.tempo_bpm = 120.0F;
    timing_effect.configure(parameters);
    timing_effect.reset();

    std::array<float, 4096> audio{};
    timing_effect.process(std::span<float>(audio.data(), 3999U), 1U);
    require(timing_effect.current_step() == 0U,
            "a quarter-note step at 120 BPM must last 4000 frames at 8 kHz");
    timing_effect.process(std::span<float>(audio.data(), 1U), 1U);
    require(timing_effect.current_step() == 1U,
            "the tempo-synchronized quarter-note step must advance exactly");

    timing_effect.trigger();
    timing_effect.process(std::span<float>(audio.data(), 2000U), 1U);
    auto faster = timing_effect.parameters();
    faster.tempo_bpm = 240.0F;
    timing_effect.configure(faster);
    timing_effect.process(std::span<float>(audio.data(), 999U), 1U);
    require(timing_effect.current_step() == 0U,
            "changing tempo must preserve the elapsed fraction of the step");
    timing_effect.process(std::span<float>(audio.data(), 1U), 1U);
    require(timing_effect.current_step() == 1U,
            "the preserved half-step must complete at the new tempo boundary");
}

void test_amplitude_pan_shapes_and_bounds() {
    using tabletfx::effects::PitchArpeggiatorAmplitudeShape;
    using tabletfx::effects::PitchArpeggiatorPanShape;

    configure_free_run(shape_effect, 4U, 10.0F);
    auto parameters = shape_effect.parameters();
    parameters.amplitude_shape = PitchArpeggiatorAmplitudeShape::RampUp;
    parameters.pan_shape = PitchArpeggiatorPanShape::LeftToRight;
    parameters.amplitude_alpha = 0.0F;
    parameters.pan_alpha = 0.0F;
    shape_effect.configure(parameters);
    std::array<float, 320> audio{};
    shape_effect.process(std::span<float>(audio.data(), 160U), 1U);
    require(std::abs(shape_effect.current_amplitude() - 0.5F) < 0.03F,
            "amplitude ramp must rise with sequence phase");
    require(std::abs(shape_effect.current_pan()) < 0.03F,
            "left-to-right pan must be near center halfway through the pattern");
    shape_effect.process(std::span<float>(audio.data() + 160U, 159U), 1U);
    require(shape_effect.current_amplitude() > 0.95F &&
                shape_effect.current_pan() > 0.95F,
            "amplitude and pan must approach their end-of-cycle targets");

    tabletfx::effects::PitchArpeggiatorParameters ramp_parameters;
    ramp_parameters.steps = 4U;
    ramp_parameters.run = true;
    ramp_parameters.step_division =
        tabletfx::effects::PitchArpeggiatorDivision::Free;
    ramp_parameters.free_step_ms = 100.0F;
    ramp_parameters.mix = 1.0F;
    ramp_parameters.amplitude_shape = PitchArpeggiatorAmplitudeShape::RampUp;
    ramp_parameters.pan_shape = PitchArpeggiatorPanShape::Center;
    shape_effect.configure(ramp_parameters);
    auto constant_parameters = ramp_parameters;
    constant_parameters.amplitude_shape =
        PitchArpeggiatorAmplitudeShape::Constant;
    partition_whole.configure(constant_parameters);
    shape_effect.reset();
    partition_whole.reset();

    constexpr std::size_t kCycleFrames = 3200U;
    std::vector<float> ramp_audio(kCycleFrames);
    std::vector<float> constant_audio(kCycleFrames);
    for (std::size_t i = 0; i < kCycleFrames; ++i) {
        const float sample = static_cast<float>(0.2 * std::sin(
            2.0 * kPi * 220.0 * static_cast<double>(i) / 8000.0));
        ramp_audio[i] = sample;
        constant_audio[i] = sample;
    }
    shape_effect.process(ramp_audio, 1U);
    partition_whole.process(constant_audio, 1U);
    constexpr std::size_t kMeasureStart = 1400U;
    constexpr std::size_t kMeasureFrames = 400U;
    double constant_energy = 0.0;
    double ramp_energy = 0.0;
    for (std::size_t frame = kMeasureStart;
         frame < kMeasureStart + kMeasureFrames; ++frame) {
        constant_energy += static_cast<double>(constant_audio[frame]) *
                           constant_audio[frame];
        ramp_energy += static_cast<double>(ramp_audio[frame]) * ramp_audio[frame];
    }
    const double measured_ramp = std::sqrt(ramp_energy / constant_energy);
    require(constant_energy > 1.0 && std::isfinite(measured_ramp) &&
                std::abs(measured_ramp - 0.5) < 0.08,
            "amplitude shape must change the rendered signal level over its cycle");

    auto pan_parameters = ramp_parameters;
    pan_parameters.amplitude_shape = PitchArpeggiatorAmplitudeShape::Constant;
    pan_parameters.pan_shape = PitchArpeggiatorPanShape::LeftToRight;
    shape_effect.configure(pan_parameters);
    shape_effect.reset();
    std::vector<float> stereo_audio(kCycleFrames * 2U);
    for (std::size_t i = 0; i < kCycleFrames; ++i) {
        const float sample = static_cast<float>(0.2 * std::sin(
            2.0 * kPi * 220.0 * static_cast<double>(i) / 8000.0));
        stereo_audio[i * 2U] = sample;
        stereo_audio[i * 2U + 1U] = sample;
    }
    shape_effect.process(stereo_audio, 2U);
    constexpr std::size_t kPanStart = 2900U;
    constexpr std::size_t kPanFrames = 200U;
    double left_energy = 0.0;
    double right_energy = 0.0;
    for (std::size_t frame = kPanStart; frame < kPanStart + kPanFrames; ++frame) {
        left_energy += static_cast<double>(stereo_audio[frame * 2U]) *
                       stereo_audio[frame * 2U];
        right_energy += static_cast<double>(stereo_audio[frame * 2U + 1U]) *
                        stereo_audio[frame * 2U + 1U];
    }
    require(right_energy > left_energy * 4.0,
            "pan shape must move the rendered mono-tracked voice across stereo");

    auto supported_range = shape_effect.parameters();
    supported_range.stage_shift_scale_degrees[0] = 24;
    supported_range.stage_shift_scale_degrees[1] = -24;
    shape_effect.configure(supported_range);
    require(shape_effect.parameters().stage_shift_scale_degrees[0] == 24 &&
                shape_effect.parameters().stage_shift_scale_degrees[1] == -24,
            "the project sequence must clamp to ±24 scale degrees");

    parameters.key = 99;
    parameters.scale = static_cast<tabletfx::effects::DiatonicScale>(255U);
    parameters.steps = 0U;
    parameters.repeats = 255U;
    parameters.stage_shift_scale_degrees[0] = 99;
    parameters.stage_shift_scale_degrees[1] = -99;
    parameters.trigger_threshold_dbfs =
        std::numeric_limits<float>::quiet_NaN();
    parameters.step_division =
        static_cast<tabletfx::effects::PitchArpeggiatorDivision>(255U);
    parameters.tempo_bpm = std::numeric_limits<float>::quiet_NaN();
    parameters.free_step_ms = std::numeric_limits<float>::infinity();
    parameters.mix = -2.0F;
    parameters.output_level = 5.0F;
    parameters.amplitude_alpha = 4.0F;
    parameters.pan_alpha = -1.0F;
    parameters.amplitude_shape =
        static_cast<PitchArpeggiatorAmplitudeShape>(255U);
    parameters.pan_shape = static_cast<PitchArpeggiatorPanShape>(255U);
    shape_effect.configure(parameters);
    const auto& safe = shape_effect.parameters();
    require(safe.key == 11 &&
                safe.scale == tabletfx::effects::DiatonicScale::Major &&
                safe.steps == 1U && safe.repeats == 64U,
            "key, scale, step-count, and repeat controls must be bounded");
    require(safe.stage_shift_scale_degrees[0] == 24 &&
                safe.stage_shift_scale_degrees[1] == -24,
            "per-step scale shifts must clamp to the supported ±24 range");
    require(safe.trigger_threshold_dbfs == -90.0F,
            "a non-finite threshold must recover to the disabled project default");
    require(safe.step_division ==
                tabletfx::effects::PitchArpeggiatorDivision::Eighth &&
                safe.tempo_bpm == 120.0F && safe.free_step_ms == 250.0F,
            "invalid timing controls must recover to safe project defaults");
    require(safe.mix == 0.0F && safe.output_level == 2.0F &&
                safe.amplitude_alpha == 1.0F && safe.pan_alpha == 0.0F &&
                safe.amplitude_shape == PitchArpeggiatorAmplitudeShape::Constant &&
                safe.pan_shape == PitchArpeggiatorPanShape::Center,
            "mix, level, alpha, and shape controls must be sanitized");

    auto threshold_bounds = safe;
    threshold_bounds.trigger_threshold_dbfs = 12.0F;
    shape_effect.configure(threshold_bounds);
    require(shape_effect.parameters().trigger_threshold_dbfs == 0.0F,
            "the project trigger threshold must clamp at 0 dBFS");
}

void test_irregular_blocks_and_finite_recovery() {
    using tabletfx::effects::PitchArpeggiatorParameters;

    PitchArpeggiatorParameters parameters;
    parameters.steps = 3U;
    parameters.stage_shift_scale_degrees = {0, 1, -2, 0, 0, 0, 0, 0,
                                             0, 0, 0, 0, 0, 0, 0, 0};
    parameters.run = true;
    parameters.step_division =
        tabletfx::effects::PitchArpeggiatorDivision::Free;
    parameters.free_step_ms = 10.0F;
    parameters.tempo_bpm = 120.0F;
    parameters.mix = 1.0F;
    partition_whole.configure(parameters);
    partition_split.configure(parameters);
    partition_whole.reset();
    partition_split.reset();

    constexpr std::size_t kFrames = 777U;
    std::array<float, kFrames> whole{};
    std::array<float, kFrames> split{};
    for (std::size_t i = 0; i < kFrames; ++i) {
        whole[i] = split[i] = static_cast<float>(0.2 * std::sin(
            2.0 * kPi * 220.0 * static_cast<double>(i) / 8000.0));
    }
    partition_whole.process(whole, 1U);

    constexpr std::array<std::size_t, 7> chunks{1U, 17U, 3U, 64U,
                                                9U, 127U, 5U};
    std::size_t offset = 0;
    std::size_t chunk = 0;
    while (offset < kFrames) {
        const std::size_t count = std::min(
            chunks[chunk++ % chunks.size()], kFrames - offset);
        partition_split.process(
            std::span<float>(split.data() + offset, count), 1U);
        offset += count;
    }
    for (std::size_t i = 0; i < kFrames; ++i) {
        require(std::isfinite(whole[i]) && std::isfinite(split[i]),
                "irregular-block output must remain finite");
        require(std::abs(whole[i] - split[i]) < 1.0e-5F,
                "output must not depend on caller block partitioning");
    }
    require(partition_whole.current_step() == partition_split.current_step() &&
                partition_whole.completed_repeats() ==
                    partition_split.completed_repeats(),
            "sequencer state must be invariant to caller block sizes");

    std::array<float, 96> exceptional{};
    exceptional[5] = std::numeric_limits<float>::quiet_NaN();
    exceptional[21] = std::numeric_limits<float>::infinity();
    exceptional[39] = -std::numeric_limits<float>::infinity();
    partition_whole.process(exceptional, 1U);
    require(std::all_of(exceptional.begin(), exceptional.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "non-finite input must not escape the processor");
    exceptional.fill(0.1F);
    partition_whole.process(exceptional, 1U);
    require(std::all_of(exceptional.begin(), exceptional.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "the processor must recover to finite output after invalid input");
}

void test_process_performs_no_allocations() {
    configure_free_run(allocation_effect, 4U, 10.0F);
    auto parameters = allocation_effect.parameters();
    parameters.run = false;
    parameters.trigger_threshold_dbfs = -30.0F;
    allocation_effect.configure(parameters);
    std::array<float, 512> audio{};
    for (std::size_t i = 0; i < audio.size(); ++i) {
        audio[i] = static_cast<float>(0.1 * std::sin(
            2.0 * kPi * 110.0 * static_cast<double>(i) / 8000.0));
    }

    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    allocation_effect.process(audio, 1U);
    count_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "audio processing must not allocate after construction");
}

}  // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* allocation = std::malloc(size == 0U ? 1U : size)) {
        return allocation;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* allocation = std::malloc(size == 0U ? 1U : size)) {
        return allocation;
    }
    throw std::bad_alloc();
}

void operator delete(void* allocation) noexcept { std::free(allocation); }
void operator delete[](void* allocation) noexcept { std::free(allocation); }
void operator delete(void* allocation, std::size_t) noexcept {
    std::free(allocation);
}
void operator delete[](void* allocation, std::size_t) noexcept {
    std::free(allocation);
}

int main() {
    try {
        test_scale_mapping_and_pitch_sequence();
        test_wide_scale_degree_extremes_reach_audio_output();
        test_step_timing_finite_repeats_and_trigger();
        test_threshold_trigger_manual_mode_release_and_retrigger();
        test_threshold_trigger_partition_invariance();
        test_tempo_division_and_tempo_change();
        test_amplitude_pan_shapes_and_bounds();
        test_irregular_blocks_and_finite_recovery();
        test_process_performs_no_allocations();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "TEST FAILURE: %s\n", error.what());
        return 1;
    }
    return 0;
}
