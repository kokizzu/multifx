#include "tabletfx/effects/auto_swell.hpp"

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

tabletfx::AutoSwellParameters test_parameters() {
    tabletfx::AutoSwellParameters parameters;
    parameters.threshold_db = -20.0F;
    parameters.attack_ms = 50.0F;
    parameters.release_ms = 50.0F;
    parameters.hysteresis_db = 6.0F;
    parameters.taper = tabletfx::AutoSwellTaper::Linear;
    parameters.volume = 1.0F;
    return parameters;
}

void test_defaults_identity_and_reset() {
    tabletfx::AutoSwell swell;
    const auto parameters = swell.parameters();
    require(parameters.threshold_db == -36.0F &&
                parameters.attack_ms == 250.0F &&
                parameters.release_ms == 250.0F &&
                parameters.hysteresis_db == 6.0F &&
                parameters.taper == tabletfx::AutoSwellTaper::SwellS &&
                parameters.volume == 1.0F,
            "project defaults changed unexpectedly");
    require(swell.id() == "auto-swell", "Auto Swell stable id changed");
    require(swell.memory_bytes() == sizeof(swell),
            "Auto Swell memory report is inaccurate");
    require(swell.current_gain_linear() == 0.0F && !swell.is_swell_active(),
            "a reset Auto Swell should start silent and armed");
}

void test_threshold_attack_and_stereo_link() {
    tabletfx::AutoSwell swell(8000.0F, test_parameters());
    std::array<float, 2> below_threshold{0.05F, -0.025F};
    swell.process(below_threshold, 2);
    require(below_threshold[0] == 0.0F && below_threshold[1] == 0.0F,
            "signal below threshold should not open the swell");

    std::array<float, 800> stereo{};
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        stereo[frame * 2] = 0.5F;
        stereo[frame * 2 + 1] = -0.25F;
    }
    swell.process(stereo, 2);
    require(stereo[0] == 0.0F && stereo[2] > 0.0F && stereo[2] < 0.5F,
            "the detector should suppress the first transient and ramp the following onset");
    require(stereo.back() < -0.24F,
            "the attack ramp did not approach the configured full volume");
    require(std::abs(stereo[2] / stereo[3] + 2.0F) < 1.0e-4F,
            "linked stereo channels did not retain their balance");
    require(swell.is_swell_active(), "input above threshold did not trigger");
}

void test_hysteresis_and_release() {
    tabletfx::AutoSwell swell(8000.0F, test_parameters());
    std::array<float, 400> onset{};
    onset.fill(0.5F);
    swell.process(onset, 1);
    require(swell.current_gain_linear() > 0.9F,
            "attack did not reach near full gain");

    std::array<float, 800> middle_level{};
    middle_level.fill(0.07F);
    swell.process(middle_level, 1);
    require(swell.is_swell_active(),
            "hysteresis rearmed before the detector fell far enough");

    std::array<float, 8000> silence{};
    swell.process(silence, 1);
    require(!swell.is_swell_active(),
            "the envelope did not rearm after dropping below hysteresis");
    require(swell.current_gain_linear() == 0.0F,
            "release did not return the gain to silence");

    std::array<float, 400> next_onset{};
    next_onset.fill(0.5F);
    swell.process(next_onset, 1);
    require(swell.is_swell_active() && swell.current_gain_linear() > 0.9F,
            "a later note did not retrigger after rearming");
}

void test_short_gap_retrigger_restarts_attack() {
    auto parameters = test_parameters();
    parameters.attack_ms = 50.0F;
    parameters.release_ms = 200.0F;
    tabletfx::AutoSwell swell(8000.0F, parameters);

    std::array<float, 400> first_note{};
    first_note.fill(0.5F);
    swell.process(first_note, 1);
    require(swell.current_gain_linear() > 0.9F,
            "the first note did not reach the configured swell level");

    std::array<float, 960> short_silence{};
    swell.process(short_silence, 1);
    require(!swell.is_swell_active() && swell.current_gain_linear() > 0.2F,
            "the short gap did not rearm while the release was still active");

    std::array<float, 2> retrigger{0.5F, 0.5F};
    swell.process(retrigger, 1);
    require(swell.is_swell_active() && swell.current_gain_linear() < 0.01F,
            "a retrigger during release must begin a fresh slow attack");
}

void test_tapers_and_volume() {
    auto parameters = test_parameters();
    parameters.attack_ms = 100.0F;

    tabletfx::AutoSwell linear(8000.0F, parameters);
    std::vector<float> signal(400, 0.5F);
    linear.process(signal, 1);
    require(std::abs(linear.current_gain_linear() - 0.5F) < 0.02F,
            "linear taper did not produce a linear halfway point");

    parameters.taper = tabletfx::AutoSwellTaper::Log10A;
    tabletfx::AutoSwell logarithmic(8000.0F, parameters);
    signal.assign(400, 0.5F);
    logarithmic.process(signal, 1);
    require(std::abs(logarithmic.current_gain_linear() - 0.10F) < 0.02F,
            "project-authored 10A taper approximation changed");

    parameters.taper = tabletfx::AutoSwellTaper::Linear;
    parameters.volume = 0.5F;
    tabletfx::AutoSwell half_volume(8000.0F, parameters);
    signal.assign(400, 0.5F);
    half_volume.process(signal, 1);
    require(std::abs(half_volume.current_gain_linear() - 0.25F) < 0.01F,
            "Volume did not scale the swell output");
}

void test_reconfiguration_keeps_active_envelope() {
    auto parameters = test_parameters();
    parameters.attack_ms = 1000.0F;
    tabletfx::AutoSwell swell(8000.0F, parameters);
    std::array<float, 128> onset{};
    onset.fill(0.5F);
    swell.process(onset, 1);
    require(swell.is_swell_active() && swell.current_gain_linear() > 0.0F,
            "test onset did not start an active swell");

    parameters.volume = 0.5F;
    swell.configure(8000.0F, parameters);
    require(swell.is_swell_active() && swell.current_gain_linear() > 0.0F,
            "changing a control should not reset an in-progress swell");
}

void test_parameter_bounds_and_nonfinite_values() {
    auto parameters = test_parameters();
    parameters.threshold_db = -200.0F;
    parameters.attack_ms = -1.0F;
    parameters.release_ms = std::numeric_limits<float>::infinity();
    parameters.hysteresis_db = 80.0F;
    parameters.volume = std::numeric_limits<float>::quiet_NaN();
    parameters.taper = static_cast<tabletfx::AutoSwellTaper>(255);
    tabletfx::AutoSwell swell(8000.0F, parameters);
    const auto bounded = swell.parameters();
    require(bounded.threshold_db == -90.0F && bounded.attack_ms == 1.0F &&
                bounded.release_ms == 250.0F && bounded.hysteresis_db == 36.0F &&
                bounded.volume == 1.0F &&
                bounded.taper == tabletfx::AutoSwellTaper::SwellS,
            "non-finite or out-of-range parameters were not normalized");

    std::array<float, 4> input{0.1F, -0.2F, 0.3F, -0.4F};
    swell.process(input, 0);
    require(input == std::array<float, 4>{0.1F, -0.2F, 0.3F, -0.4F},
            "zero-channel input should be ignored");

    tabletfx::AutoSwell sanitizer(8000.0F, test_parameters());
    std::array<float, 3> nonfinite{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
    };
    sanitizer.process(nonfinite, 3);
    require(nonfinite == std::array<float, 3>{0.0F, 0.0F, 0.0F},
            "non-finite input samples were not replaced with silence");
}

void test_variable_blocks_and_no_audio_thread_allocations() {
    auto parameters = test_parameters();
    tabletfx::AutoSwell whole(8000.0F, parameters);
    tabletfx::AutoSwell chunked(8000.0F, parameters);
    std::vector<float> whole_audio(800, 0.0F);
    whole_audio[0] = 0.5F;
    std::vector<float> chunked_audio = whole_audio;
    whole.process(whole_audio, 1);

    const std::array<std::size_t, 5> block_sizes{1, 127, 53, 211, 17};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < chunked_audio.size()) {
        const std::size_t frames = std::min(
            block_sizes[block % block_sizes.size()], chunked_audio.size() - offset);
        chunked.process(std::span<float>(chunked_audio).subspan(offset, frames), 1);
        offset += frames;
        ++block;
    }
    for (std::size_t index = 0; index < whole_audio.size(); ++index) {
        require(std::abs(whole_audio[index] - chunked_audio[index]) < 1.0e-7F,
                "variable block sizes changed the swell output");
    }

    std::array<float, 512> audio{};
    audio.fill(0.25F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        whole.process(audio, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "Auto Swell allocated memory during process()");
    for (float sample : audio) {
        require(std::isfinite(sample), "Auto Swell emitted a non-finite sample");
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
        test_defaults_identity_and_reset();
        test_threshold_attack_and_stereo_link();
        test_hysteresis_and_release();
        test_short_gap_retrigger_restarts_attack();
        test_tapers_and_volume();
        test_reconfiguration_keeps_active_envelope();
        test_parameter_bounds_and_nonfinite_values();
        test_variable_blocks_and_no_audio_thread_allocations();
        std::cout << "All TabletFX Auto Swell tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
