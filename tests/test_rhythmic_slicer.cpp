#include "tabletfx/effects/rhythmic_slicer.hpp"

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
#include <string>
#include <vector>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

tabletfx::RhythmicSlicerParameters hard_gate_parameters() {
    tabletfx::RhythmicSlicerParameters parameters;
    parameters.tempo_sync = true;
    parameters.tempo_bpm = 120.0F;
    parameters.division = tabletfx::RhythmicDivision::Sixteenth;
    parameters.depth = 1.0F;
    parameters.shape = 0.0F;
    parameters.attack_ms = 0.0F;
    parameters.release_ms = 0.0F;
    parameters.steps.fill(0.0F);
    parameters.steps[0] = 1.0F;
    return parameters;
}

void test_identity_defaults_bounds_and_fixed_memory() {
    tabletfx::RhythmicSlicer slicer;
    const auto defaults = slicer.parameters();
    require(slicer.id() == "mod-16", "MOD-16 stable ID changed");
    require(defaults.tempo_sync && defaults.tempo_bpm == 120.0F &&
                defaults.division == tabletfx::RhythmicDivision::Sixteenth &&
                defaults.depth == 1.0F && defaults.steps.size() == 16U &&
                std::abs(slicer.effective_step_rate_hz() - 8.0F) < 1.0e-6F,
            "project-authored MOD-16 defaults changed");
    require(slicer.memory_bytes() == sizeof(slicer),
            "MOD-16 memory report is inaccurate");

    auto parameters = hard_gate_parameters();
    parameters.tempo_bpm = 900.0F;
    parameters.division = tabletfx::RhythmicDivision::ThirtySecondTriplet;
    parameters.depth = -2.0F;
    parameters.shape = 3.0F;
    parameters.attack_ms = -1.0F;
    parameters.release_ms = 200.0F;
    parameters.steps[0] = std::numeric_limits<float>::quiet_NaN();
    parameters.steps[1] = 2.0F;
    tabletfx::RhythmicSlicer bounded(0.0F, parameters);
    const auto bounded_parameters = bounded.parameters();
    require(bounded.parameters().tempo_bpm == 300.0F &&
                bounded.effective_step_rate_hz() == 60.0F,
            "tempo-derived step rate did not respect its project safety limits");
    require(bounded_parameters.depth == 0.0F && bounded_parameters.shape == 1.0F &&
                bounded_parameters.attack_ms == 0.0F &&
                bounded_parameters.release_ms == 100.0F &&
                bounded_parameters.steps[0] == 1.0F &&
                bounded_parameters.steps[1] == 1.0F,
            "out-of-range or non-finite controls were not bounded safely");
    require(bounded.memory_bytes() == sizeof(bounded),
            "MOD-16 storage size changed after configuration");

    bounded.set_depth(std::numeric_limits<float>::infinity());
    bounded.set_shape(std::numeric_limits<float>::quiet_NaN());
    bounded.set_edge_times_ms(std::numeric_limits<float>::quiet_NaN(), 2.0F);
    bounded.set_rate_hz(100.0F);
    require(bounded.parameters().depth == 0.0F &&
                bounded.parameters().shape == 1.0F &&
                bounded.parameters().attack_ms == 0.0F &&
                bounded.parameters().release_ms == 2.0F,
            "non-finite setters did not preserve the last valid controls");
    bounded.set_tempo_sync(false, 120.0F,
                           static_cast<tabletfx::RhythmicDivision>(255U));
    require(bounded.parameters().division ==
                tabletfx::RhythmicDivision::Sixteenth &&
                bounded.effective_step_rate_hz() == 60.0F,
            "invalid division or free-rate upper bound was not sanitized");
}

void test_tempo_divisions_and_sixteenth_gate_timing() {
    auto parameters = hard_gate_parameters();
    parameters.steps[1] = 0.5F;
    tabletfx::RhythmicSlicer slicer(8000.0F, parameters);
    require(std::abs(slicer.effective_step_rate_hz() - 8.0F) < 1.0e-6F,
            "120 BPM sixteenth division should advance at 8 steps per second");

    std::vector<float> audio(2002U, 1.0F);
    slicer.process(audio, 1);
    require(audio[0] == 1.0F && audio[999] == 1.0F,
            "first sixteenth-note gate step did not stay open for 1000 frames");
    require(std::abs(audio[1000] - 0.5F) < 1.0e-6F &&
                std::abs(audio[1999] - 0.5F) < 1.0e-5F,
            "second gate step did not use its configured half-level value");
    require(audio[2000] == 0.0F,
            "third gate step did not close at the expected tempo boundary");

    slicer.set_tempo_sync(true, 60.0F,
                          tabletfx::RhythmicDivision::Eighth);
    require(std::abs(slicer.effective_step_rate_hz() - 2.0F) < 1.0e-6F,
            "60 BPM eighth-note steps should advance at 2 steps per second");
    slicer.set_tempo_sync(true, 120.0F,
                          tabletfx::RhythmicDivision::DottedEighth);
    require(std::abs(slicer.effective_step_rate_hz() - (120.0F / 45.0F)) <
                1.0e-6F,
            "dotted-eighth division conversion is incorrect");
    slicer.set_tempo_sync(true, 120.0F,
                          tabletfx::RhythmicDivision::SixteenthTriplet);
    require(std::abs(slicer.effective_step_rate_hz() - 12.0F) < 1.0e-5F,
            "sixteenth-triplet division conversion is incorrect");

    slicer.set_tempo_sync(false, 120.0F,
                          tabletfx::RhythmicDivision::Quarter);
    slicer.set_rate_hz(3.25F);
    require(std::abs(slicer.effective_step_rate_hz() - 3.25F) < 1.0e-6F,
            "free-running step-rate selection was not applied");
}

void test_depth_pattern_shape_and_reset_phase() {
    auto parameters = hard_gate_parameters();
    parameters.steps.fill(0.0F);
    parameters.steps[1] = 1.0F;
    parameters.steps[2] = 0.0F;
    parameters.shape = 1.0F;
    tabletfx::RhythmicSlicer shaped(8000.0F, parameters);
    std::array<float, 1002> transition{};
    transition.fill(1.0F);
    shaped.process(transition, 1);
    require(transition[0] == 0.0F &&
                std::abs(transition[500] - 0.5F) < 0.01F &&
                transition[999] > transition[500] &&
                std::abs(transition[1000] - 1.0F) < 1.0e-5F,
            "full-step smooth transition did not ease between adjacent values");
    require(transition[1001] > 0.9999F,
            "new step did not begin at its configured open level");

    shaped.set_depth(0.0F);
    shaped.reset();
    std::array<float, 8> dry{0.25F, -0.5F, 0.75F, -1.0F,
                             0.125F, -0.25F, 0.5F, -0.75F};
    const auto original = dry;
    shaped.process(dry, 2);
    require(dry == original,
            "zero depth did not preserve the input signal exactly");

    auto phased_parameters = hard_gate_parameters();
    phased_parameters.steps.fill(0.0F);
    phased_parameters.steps[1] = 1.0F;
    phased_parameters.phase = 1.0F / 16.0F;
    tabletfx::RhythmicSlicer phased(8000.0F, phased_parameters);
    std::array<float, 2> first{1.0F, 1.0F};
    phased.process(first, 1);
    require(first[0] == 1.0F,
            "configured cycle phase did not start at the selected pattern step");
    phased.set_phase(0.0F);
    std::array<float, 1> reset_to_start{1.0F};
    phased.process(reset_to_start, 1);
    require(reset_to_start[0] == 0.0F,
            "phase setter did not restore the initial pattern position");
    phased.set_phase(0.0625F);
    std::array<float, 1> reset_to_second{1.0F};
    phased.process(reset_to_second, 1);
    require(reset_to_second[0] == 1.0F,
            "phase setter did not select the second pattern step");
}

void test_edge_smoothing_reduces_click_step() {
    auto parameters = hard_gate_parameters();
    parameters.attack_ms = 0.0F;
    parameters.release_ms = 5.0F;
    tabletfx::RhythmicSlicer slicer(8000.0F, parameters);
    std::vector<float> audio(1042U, 1.0F);
    slicer.process(audio, 1);
    require(audio[999] == 1.0F && audio[1000] < audio[999] &&
                audio[1000] > 0.9F,
            "release smoothing did not prevent an abrupt step-boundary drop");
    require(audio[1040] < audio[1000] && audio[1040] > 0.0F,
            "release smoothing did not decay smoothly toward the closed gate");

    auto open_parameters = hard_gate_parameters();
    open_parameters.steps.fill(0.0F);
    open_parameters.steps[1] = 1.0F;
    open_parameters.attack_ms = 5.0F;
    open_parameters.release_ms = 0.0F;
    tabletfx::RhythmicSlicer opening(8000.0F, open_parameters);
    std::vector<float> open_audio(1042U, 1.0F);
    opening.process(open_audio, 1);
    require(open_audio[1000] > 0.0F && open_audio[1000] < 0.1F &&
                open_audio[1040] > open_audio[1000],
            "attack smoothing did not avoid a click when opening the gate");
}

void test_mono_stereo_link_and_unaligned_buffer() {
    auto parameters = hard_gate_parameters();
    tabletfx::RhythmicSlicer mono(8000.0F, parameters);
    std::array<float, 1001> mono_audio{};
    mono_audio.fill(1.0F);
    mono.process(mono_audio, 1);
    require(mono_audio[999] == 1.0F && mono_audio[1000] == 0.0F,
            "mono path did not apply the pattern's shared amplitude gate");

    tabletfx::RhythmicSlicer stereo(8000.0F, parameters);
    std::array<float, 2002> stereo_audio{};
    for (std::size_t frame = 0; frame < stereo_audio.size() / 2U; ++frame) {
        stereo_audio[frame * 2U] = 0.8F;
        stereo_audio[frame * 2U + 1U] = -0.4F;
    }
    stereo.process(stereo_audio, 2);
    require(stereo_audio[1998] == 0.8F && stereo_audio[1999] == -0.4F &&
                stereo_audio[2000] == 0.0F && stereo_audio[2001] == 0.0F,
            "stereo channels did not share gate phase at step transitions");

    tabletfx::RhythmicSlicer partial(8000.0F, parameters);
    std::array<float, 4> trailing{1.0F, 1.0F, 0.25F, -0.5F};
    partial.process(trailing, 3);
    require(trailing[0] == 1.0F && trailing[1] == 1.0F &&
                trailing[2] == 0.25F && trailing[3] == -0.5F,
            "incomplete trailing channel frame was modified");

    std::array<float, 4> no_channels{0.1F, -0.2F, 0.3F, -0.4F};
    const auto original = no_channels;
    partial.process(no_channels, 0);
    require(no_channels == original,
            "zero-channel processing should leave the buffer untouched");
}

void test_variable_block_partition_invariance_and_reset() {
    auto parameters = hard_gate_parameters();
    parameters.tempo_bpm = 137.0F;
    parameters.division = tabletfx::RhythmicDivision::EighthTriplet;
    parameters.shape = 0.43F;
    parameters.attack_ms = 0.8F;
    parameters.release_ms = 2.3F;
    parameters.phase = 0.17F;
    parameters.steps = {1.0F, 0.0F, 0.3F, 1.0F, 0.0F, 0.0F, 0.7F, 1.0F,
                        0.2F, 0.0F, 1.0F, 0.0F, 0.5F, 0.0F, 1.0F, 0.0F};
    tabletfx::RhythmicSlicer whole(44100.0F, parameters);
    tabletfx::RhythmicSlicer chunked(44100.0F, parameters);
    std::vector<float> expected(16004U);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        expected[index] = 0.6F * std::sin(static_cast<float>(index) * 0.013F);
    }
    auto actual = expected;
    whole.process(expected, 2);
    const std::array<std::size_t, 6> block_sizes{1U, 127U, 53U, 300U, 17U, 89U};
    std::size_t frame = 0;
    std::size_t block = 0;
    while (frame < actual.size() / 2U) {
        const std::size_t frames = std::min(
            block_sizes[block % block_sizes.size()], actual.size() / 2U - frame);
        chunked.process(std::span<float>(actual).subspan(frame * 2U, frames * 2U), 2);
        frame += frames;
        ++block;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(std::abs(expected[index] - actual[index]) < 1.0e-7F,
                "variable block partitions changed pattern timing or envelope output");
    }

    whole.reset();
    chunked.reset();
    std::array<float, 128> first{};
    std::array<float, 128> second{};
    first.fill(1.0F);
    second.fill(1.0F);
    whole.process(first, 1);
    chunked.process(second, 1);
    require(first == second,
            "reset did not reproduce the same starting phase and envelope");
}

void test_finite_input_and_no_process_allocations() {
    auto parameters = hard_gate_parameters();
    tabletfx::RhythmicSlicer slicer(48000.0F, parameters);
    std::array<float, 5> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.25F,
        -0.5F,
    };
    slicer.process(invalid, 1);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F && invalid[2] == 0.0F,
            "non-finite input did not produce finite silence");
    for (float sample : invalid) {
        require(std::isfinite(sample), "MOD-16 emitted a non-finite sample");
    }

    std::array<float, 1024> block{};
    block.fill(0.5F);
    slicer.reset();
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        slicer.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "MOD-16 allocated memory during process()");
    require(slicer.memory_bytes() == sizeof(slicer),
            "MOD-16 memory grew during repeated processing");
    for (float sample : block) {
        require(std::isfinite(sample), "MOD-16 emitted non-finite audio");
    }
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_identity_defaults_bounds_and_fixed_memory();
        test_tempo_divisions_and_sixteenth_gate_timing();
        test_depth_pattern_shape_and_reset_phase();
        test_edge_smoothing_reduces_click_step();
        test_mono_stereo_link_and_unaligned_buffer();
        test_variable_block_partition_invariance_and_reset();
        test_finite_input_and_no_process_allocations();
        std::cout << "All TabletFX rhythmic slicer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
