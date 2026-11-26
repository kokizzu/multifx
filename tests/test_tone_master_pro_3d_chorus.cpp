#include "tabletfx/effects/tone_master_pro_3d_chorus.hpp"

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
std::atomic<bool> count_allocations{false};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> guitar_tone(std::size_t frames,
                               std::uint16_t channels = 2U) {
    std::vector<float> samples(frames * channels);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float time = static_cast<float>(frame) / 48000.0F;
        const float value = 0.22F * std::sin(2.0F * 3.14159265358979323846F *
                                             110.0F * time) +
                            0.08F * std::sin(2.0F * 3.14159265358979323846F *
                                             220.0F * time);
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            samples[frame * channels + channel] = value;
        }
    }
    return samples;
}

double rms_difference(std::span<const float> left,
                      std::span<const float> right) {
    double energy = 0.0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        const double delta = static_cast<double>(left[i]) - right[i];
        energy += delta * delta;
    }
    return std::sqrt(energy / static_cast<double>(left.size()));
}

void test_defaults_mode_bounds_and_memory() {
    tabletfx::ToneMasterPro3DChorus chorus;
    require(chorus.id() == "tone-master-pro-3d-chorus",
            "Tone Master Pro 3D Chorus ID changed");
    require(chorus.mode() == tabletfx::ToneMasterPro3DChorusMode::Mode1,
            "safe project default must select the documented subtle-first mode");
    require(chorus.memory_bytes() > sizeof(chorus),
            "memory estimate must include the preallocated chorus delay ring");

    chorus.set_mode(tabletfx::ToneMasterPro3DChorusMode::Mode4);
    require(chorus.mode() == tabletfx::ToneMasterPro3DChorusMode::Mode4,
            "Mode 4 selection was not retained");
    chorus.set_mode(static_cast<tabletfx::ToneMasterPro3DChorusMode>(255U));
    require(chorus.mode() == tabletfx::ToneMasterPro3DChorusMode::Mode1,
            "invalid mode must safely fall back to Mode 1");
}

void test_all_modes_are_distinct_and_finite() {
    const auto input = guitar_tone(48000U);
    std::array<std::vector<float>, 4U> outputs;
    for (std::size_t mode = 0; mode < outputs.size(); ++mode) {
        outputs[mode] = input;
        tabletfx::ToneMasterPro3DChorus chorus;
        chorus.set_mode(static_cast<tabletfx::ToneMasterPro3DChorusMode>(mode));
        chorus.process(outputs[mode], 2U);
        require(std::all_of(outputs[mode].begin(), outputs[mode].end(),
                            [](float value) { return std::isfinite(value); }),
                "a 3D Chorus mode produced non-finite audio");
        require(rms_difference(input, outputs[mode]) > 1.0e-4,
                "3D Chorus mode failed to produce a wet modulation signal");
    }
    for (std::size_t mode = 1; mode < outputs.size(); ++mode) {
        require(rms_difference(outputs[mode - 1U], outputs[mode]) > 1.0e-4,
                "adjacent project-authored modes must produce distinct audio");
    }
}

void test_partition_invariance_and_reset() {
    auto whole = guitar_tone(24000U, 1U);
    auto split = whole;
    tabletfx::ToneMasterPro3DChorus one_block(48000.0F, 1U);
    tabletfx::ToneMasterPro3DChorus two_blocks(48000.0F, 1U);
    one_block.set_mode(tabletfx::ToneMasterPro3DChorusMode::Mode3);
    two_blocks.set_mode(tabletfx::ToneMasterPro3DChorusMode::Mode3);
    one_block.process(whole, 1U);
    two_blocks.process(std::span<float>(split.data(), split.size() / 2U), 1U);
    two_blocks.process(std::span<float>(split.data() + split.size() / 2U,
                                        split.size() / 2U), 1U);
    require(rms_difference(whole, split) < 1.0e-6,
            "processor output changed when the same input was partitioned");

    one_block.reset();
    two_blocks.reset();
    auto reset_a = guitar_tone(4096U, 1U);
    auto reset_b = reset_a;
    one_block.process(reset_a, 1U);
    two_blocks.process(reset_b, 1U);
    require(rms_difference(reset_a, reset_b) < 1.0e-7,
            "reset did not return the processor to a reproducible state");
}

void test_process_has_no_heap_allocations() {
    tabletfx::ToneMasterPro3DChorus chorus;
    chorus.set_mode(tabletfx::ToneMasterPro3DChorusMode::Mode4);
    auto samples = guitar_tone(4096U, 2U);
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    chorus.process(samples, 2U);
    count_allocations.store(false, std::memory_order_release);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "3D Chorus allocated memory while processing audio");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_acquire)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* pointer = std::malloc(size)) return pointer;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_acquire)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* pointer = std::malloc(size)) return pointer;
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
    try {
        test_defaults_mode_bounds_and_memory();
        test_all_modes_are_distinct_and_finite();
        test_partition_invariance_and_reset();
        test_process_has_no_heap_allocations();
    } catch (const std::exception& error) {
        std::cerr << "Tone Master Pro 3D Chorus test failed: " << error.what()
                  << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "Tone Master Pro 3D Chorus tests passed\n";
    return EXIT_SUCCESS;
}
