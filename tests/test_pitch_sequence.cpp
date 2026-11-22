#include "tabletfx/effects/pitch_sequence.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

using tabletfx::effects::PitchSequence;
using tabletfx::effects::PitchSequenceParameters;
using tabletfx::effects::PitchSequenceStepMode;

void process_zeros(PitchSequence& effect, std::size_t frames) {
    std::vector<float> audio(frames, 0.0F);
    effect.process(audio, 1U);
}

void test_defaults_and_parameter_bounds() {
    PitchSequence effect(8000.0F, 1U);
    const auto defaults = effect.parameters();
    require(effect.id() == "pitch-sequence",
            "Pitch Sequence stable identifier changed");
    require(defaults.steps == 4U && defaults.semitones[0] == 0 &&
                defaults.semitones[1] == 7 && defaults.semitones[2] == 12 &&
                !defaults.run && defaults.glide_ms == 15.0F,
            "Pitch Sequence project defaults changed");
    require(effect.pitch_shifter_latency_ms() > 0.0F &&
                effect.memory_bytes() > sizeof(effect),
            "Pitch Sequence latency or memory reporting omitted owned state");

    auto parameters = defaults;
    parameters.steps = 0U;
    parameters.semitones[0] = 99;
    parameters.mode[0] = static_cast<PitchSequenceStepMode>(255U);
    parameters.time_mode = static_cast<tabletfx::DelayTimeMode>(255U);
    parameters.division = static_cast<tabletfx::DelayDivision>(255U);
    parameters.free_step_ms = std::numeric_limits<float>::quiet_NaN();
    parameters.tempo_bpm = std::numeric_limits<float>::infinity();
    parameters.glide_ms = -1.0F;
    parameters.glitch_depth_semitones = 255U;
    parameters.glitch_interval_ms = 0.0F;
    parameters.freeze_window_ms = 0.0F;
    parameters.mix = 2.0F;
    parameters.level = -1.0F;
    effect.configure(parameters);
    const auto bounded = effect.parameters();
    require(bounded.steps == 1U && bounded.semitones[0] == 12 &&
                bounded.mode[0] == PitchSequenceStepMode::Pitch,
            "invalid step count, shift, or mode was not bounded");
    require(bounded.time_mode == tabletfx::DelayTimeMode::milliseconds &&
                bounded.division == tabletfx::DelayDivision::quarter &&
                bounded.free_step_ms == 250.0F &&
                bounded.tempo_bpm == 120.0F,
            "invalid timing controls did not use safe project defaults");
    require(bounded.glide_ms == 0.0F &&
                bounded.glitch_depth_semitones == 12U &&
                bounded.glitch_interval_ms == 10.0F &&
                bounded.freeze_window_ms == 10.0F && bounded.mix == 1.0F &&
                bounded.level == 0.0F,
            "pitch, glitch, freeze, mix, or level bounds failed");

    bool rejected_rate = false;
    try {
        PitchSequence invalid_rate(7999.0F, 1U);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    require(rejected_rate, "unsupported sample rate was accepted");
    bool rejected_channels = false;
    try {
        PitchSequence invalid_channels(48000.0F, 3U);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    require(rejected_channels, "unsupported channel count was accepted");
}

void test_pitch_steps_glide_and_sequence_boundaries() {
    PitchSequenceParameters parameters;
    parameters.steps = 3U;
    parameters.run = true;
    parameters.free_step_ms = 20.0F;
    parameters.glide_ms = 100.0F;
    parameters.semitones[0] = 0;
    parameters.semitones[1] = 12;
    parameters.semitones[2] = -7;
    PitchSequence effect(8000.0F, 1U, parameters);
    effect.reset();

    process_zeros(effect, 159U);
    require(effect.current_step() == 0U,
            "step zero advanced before its 20 ms boundary");
    process_zeros(effect, 1U);
    require(effect.current_step() == 1U,
            "step zero did not advance at its exact sample boundary");
    require(effect.current_pitch_semitones() == 0.0F,
            "a new pitch step changed before processing its first sample");

    process_zeros(effect, 80U);
    require(effect.current_pitch_semitones() > 0.0F &&
                effect.current_pitch_semitones() < 12.0F,
            "the pitch glide did not move smoothly toward the new target");
    process_zeros(effect, 80U);
    require(effect.current_step() == 2U,
            "the second programmed step did not advance on time");
    process_zeros(effect, 80U);
    require(effect.current_pitch_semitones() > -7.0F &&
                effect.current_pitch_semitones() < 12.0F,
            "the reverse pitch glide jumped directly to its target");

    effect.stop();
    require(!effect.running() && effect.current_step() == 0U,
            "stop did not reset the sequence position");
    process_zeros(effect, 400U);
    require(effect.current_step() == 0U && !effect.running(),
            "a stopped sequence continued advancing");
    effect.trigger();
    require(effect.running() && effect.current_step() == 0U,
            "trigger did not restart from step zero");
}

void test_glitch_offsets_are_deterministic_and_bounded() {
    PitchSequenceParameters parameters;
    parameters.steps = 1U;
    parameters.run = true;
    parameters.free_step_ms = 20.0F;
    parameters.mode[0] = PitchSequenceStepMode::Glitch;
    parameters.glitch_depth_semitones = 12U;
    parameters.glitch_interval_ms = 10.0F;
    PitchSequence first(8000.0F, 1U, parameters);
    PitchSequence second(8000.0F, 1U, parameters);
    first.reset();
    second.reset();

    bool observed_offset = false;
    for (std::size_t frame = 0; frame < 800U; ++frame) {
        std::array<float, 1> a{};
        std::array<float, 1> b{};
        first.process(a, 1U);
        second.process(b, 1U);
        const float first_pitch = first.current_pitch_semitones();
        const float second_pitch = second.current_pitch_semitones();
        require(first_pitch >= -12.0F && first_pitch <= 12.0F,
                "glitch offset exceeded the supported pitch range");
        require(first_pitch == second_pitch,
                "identically seeded glitch sequences were not deterministic");
        observed_offset = observed_offset || first_pitch != 0.0F;
    }
    require(observed_offset,
            "glitch mode did not generate any pitch offsets");
}

void test_freeze_capture_loop_release_and_reset() {
    PitchSequenceParameters parameters;
    parameters.steps = 2U;
    parameters.run = true;
    parameters.free_step_ms = 20.0F;
    parameters.freeze_window_ms = 80.0F;
    parameters.mode[1] = PitchSequenceStepMode::Freeze;
    PitchSequence effect(8000.0F, 1U, parameters);
    effect.reset();

    std::array<float, 320> audio{};
    for (std::size_t frame = 0; frame < audio.size(); ++frame) {
        audio[frame] = 0.25F * std::sin(
            2.0F * 3.14159265358979323846F * 220.0F *
            static_cast<float>(frame) / 8000.0F);
    }
    effect.process(std::span<float>(audio).first(160U), 1U);
    require(effect.current_step() == 1U && effect.freeze_active(),
            "freeze step did not capture the recent input window");
    const std::size_t loop_length = effect.frozen_loop_length_frames();
    require(loop_length > 0U && loop_length <= 160U,
            "freeze loop length is outside captured history");

    effect.process(std::span<float>(audio).subspan(160U, 159U), 1U);
    require(effect.freeze_active() &&
                effect.frozen_loop_length_frames() == loop_length,
            "freeze loop changed while it was playing");
    effect.process(std::span<float>(audio).subspan(319U, 1U), 1U);
    require(effect.current_step() == 0U && !effect.freeze_active(),
            "leaving the freeze step did not release the captured loop");
    require(std::all_of(audio.begin(), audio.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "freeze processing produced a non-finite sample");

    effect.process(std::span<float>(audio).first(160U), 1U);
    require(effect.freeze_active(), "freeze step did not retrigger");
    effect.reset();
    require(!effect.freeze_active() && effect.running() &&
                effect.current_step() == 0U &&
                effect.frozen_loop_length_frames() == 0U,
            "reset did not clear freeze state while retaining Run");
}

void test_nonfinite_recovery_channel_guard_and_process_allocations() {
    PitchSequenceParameters parameters;
    parameters.steps = 1U;
    parameters.run = true;
    parameters.mix = 0.5F;
    PitchSequence effect(8000.0F, 1U, parameters);
    effect.reset();

    std::array<float, 4> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.25F};
    effect.process(invalid, 1U);
    require(std::all_of(invalid.begin(), invalid.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "non-finite input was not sanitized to finite output");

    std::array<float, 4> wrong_channels{0.1F, 0.2F, 0.3F, 0.4F};
    const auto original = wrong_channels;
    const auto step_before = effect.current_step();
    effect.process(wrong_channels, 2U);
    require(wrong_channels == original && effect.current_step() == step_before,
            "mismatched channel layout changed audio or sequence state");

    std::array<float, 256> audio{};
    audio.fill(0.1F);
    allocation_count = 0U;
    count_allocations = true;
    effect.process(audio, 1U);
    count_allocations = false;
    require(allocation_count == 0U,
            "Pitch Sequence allocated memory from process()");
}

void test_irregular_block_partition_is_sample_stable() {
    PitchSequenceParameters parameters;
    parameters.steps = 3U;
    parameters.run = true;
    parameters.free_step_ms = 23.0F;
    parameters.glide_ms = 7.0F;
    parameters.semitones[0] = 0;
    parameters.semitones[1] = 5;
    parameters.semitones[2] = -3;
    parameters.mode[2] = PitchSequenceStepMode::Glitch;

    PitchSequence whole(8000.0F, 1U, parameters);
    PitchSequence split(8000.0F, 1U, parameters);
    whole.reset();
    split.reset();
    std::vector<float> input(4097U);
    for (std::size_t frame = 0; frame < input.size(); ++frame) {
        input[frame] = 0.2F * std::sin(
            2.0F * 3.14159265358979323846F * 330.0F *
            static_cast<float>(frame) / 8000.0F);
    }
    auto whole_audio = input;
    auto split_audio = input;
    whole.process(whole_audio, 1U);

    constexpr std::array<std::size_t, 7> chunks{1U, 13U, 127U, 3U,
                                                 256U, 79U, 511U};
    std::size_t offset = 0U;
    std::size_t chunk_index = 0U;
    while (offset < split_audio.size()) {
        const std::size_t count = std::min(
            chunks[chunk_index % chunks.size()], split_audio.size() - offset);
        split.process(std::span<float>(split_audio).subspan(offset, count), 1U);
        offset += count;
        ++chunk_index;
    }
    require(whole_audio == split_audio &&
                whole.current_step() == split.current_step() &&
                whole.current_pitch_semitones() ==
                    split.current_pitch_semitones(),
            "processing block boundaries changed sequence output or state");
}

}  // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_and_parameter_bounds();
        test_pitch_steps_glide_and_sequence_boundaries();
        test_glitch_offsets_are_deterministic_and_bounded();
        test_freeze_capture_loop_release_and_reset();
        test_nonfinite_recovery_channel_guard_and_process_allocations();
        test_irregular_block_partition_is_sample_stable();
        std::cout << "Pitch Sequence tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        count_allocations = false;
        std::cerr << "Pitch Sequence test failure: " << error.what() << '\n';
        return 1;
    }
}
