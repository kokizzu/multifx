#include "tabletfx/effects.hpp"

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

float linear_gain(float decibels) {
    return std::pow(10.0F, decibels / 20.0F);
}

void test_default_transfer_and_bounds() {
    tabletfx::Gain gain;
    std::array<float, 4> samples{0.25F, -0.5F, 0.75F, -1.0F};
    gain.process(samples, 2);
    require(samples == std::array<float, 4>{0.25F, -0.5F, 0.75F, -1.0F},
            "unity gain changed the signal");

    gain.set_gain_db(6.0F);
    gain.reset();
    std::array<float, 2> boosted{0.25F, -0.25F};
    gain.process(boosted, 2);
    require(std::abs(boosted[0] - 0.25F * linear_gain(6.0F)) < 1.0e-6F &&
            std::abs(boosted[1] + 0.25F * linear_gain(6.0F)) < 1.0e-6F,
            "gain dB conversion is incorrect");

    gain.set_gain_db(100.0F);
    require(gain.gain_db() == 24.0F, "positive gain bound failed");
    gain.set_gain_db(-100.0F);
    require(gain.gain_db() == -96.0F, "negative gain bound failed");
    gain.set_gain_db(std::numeric_limits<float>::quiet_NaN());
    require(gain.gain_db() == -96.0F, "non-finite gain did not preserve the last value");
    require(gain.memory_bytes() == sizeof(gain), "gain memory_bytes is inaccurate");
}

void test_smoothing_reaches_target_and_is_monotonic() {
    tabletfx::Gain gain(0.0F, 48000.0F, 10.0F);
    gain.set_gain_db(-12.0F);
    std::vector<float> samples(480, 1.0F);
    gain.process(samples, 1);
    require(samples.front() < 1.0F, "gain ramp did not begin on the first sample");
    for (std::size_t i = 1; i < samples.size(); ++i) {
        require(samples[i] <= samples[i - 1], "gain ramp was not monotonic");
    }
    require(std::abs(samples.back() - linear_gain(-12.0F)) < 1.0e-6F,
            "gain ramp did not reach its target after the configured duration");
}

void test_retarget_variable_blocks_and_reset() {
    tabletfx::Gain whole_gain(0.0F, 48000.0F, 10.0F);
    tabletfx::Gain chunked_gain(0.0F, 48000.0F, 10.0F);
    whole_gain.set_gain_db(-6.0F);
    chunked_gain.set_gain_db(-6.0F);
    std::vector<float> whole(997, 0.5F);
    std::vector<float> chunked = whole;
    whole_gain.process(whole, 1);

    const std::array<std::size_t, 5> blocks{1, 127, 53, 300, 17};
    std::size_t frame = 0;
    std::size_t block = 0;
    while (frame < chunked.size()) {
        const std::size_t count = std::min(blocks[block % blocks.size()],
                                           chunked.size() - frame);
        chunked_gain.process(std::span<float>(chunked).subspan(frame, count), 1);
        frame += count;
        ++block;
    }
    for (std::size_t i = 0; i < whole.size(); ++i) {
        require(std::abs(whole[i] - chunked[i]) < 1.0e-6F,
                "variable block sizes changed gain-ramp output");
    }

    whole_gain.set_gain_db(-12.0F);
    std::array<float, 240> first_half{};
    first_half.fill(1.0F);
    whole_gain.process(first_half, 1);
    const float before_retarget = first_half.back();
    whole_gain.set_gain_db(6.0F);
    std::array<float, 480> second_half{};
    second_half.fill(1.0F);
    whole_gain.process(second_half, 1);
    require(second_half.front() > before_retarget &&
            std::abs(second_half.back() - linear_gain(6.0F)) < 1.0e-6F,
            "retargeting did not smoothly reach the new gain");

    whole_gain.set_gain_db(-3.0F);
    whole_gain.reset();
    std::array<float, 1> after_reset{1.0F};
    whole_gain.process(after_reset, 1);
    require(std::abs(after_reset[0] - linear_gain(-3.0F)) < 1.0e-6F,
            "reset did not settle the gain to its target");
}

void test_immediate_mode_and_no_processing_allocations() {
    tabletfx::Gain prepared_gain(0.0F, 48000.0F, 10.0F);
    prepared_gain.set_gain_db_immediate(-3.0F);
    std::array<float, 1> prepared_sample{1.0F};
    prepared_gain.process(prepared_sample, 1);
    require(std::abs(prepared_sample[0] - linear_gain(-3.0F)) < 1.0e-6F,
            "prepared preset gain incorrectly ramped from unity");

    tabletfx::Gain gain(0.0F, 100.0F, 0.0F);
    gain.set_gain_db(-6.0F);
    std::array<float, 2> samples{1.0F, 1.0F};
    gain.process(samples, 1);
    require(std::abs(samples.back() - linear_gain(-6.0F)) < 1.0e-6F,
            "zero-duration smoothing did not apply immediately");

    gain.configure_smoothing(48000.0F, 10.0F);
    gain.set_gain_db(0.0F);
    std::array<float, 256> block{};
    block.fill(0.5F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        gain.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "gain processor allocated memory during process()");
    for (float sample : block) {
        require(std::isfinite(sample), "gain processor emitted a non-finite sample");
    }
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_default_transfer_and_bounds();
        test_smoothing_reaches_target_and_is_monotonic();
        test_retarget_variable_blocks_and_reset();
        test_immediate_mode_and_no_processing_allocations();
        std::cout << "All TabletFX gain tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
