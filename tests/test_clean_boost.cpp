#include "tabletfx/effects/clean_boost.hpp"

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
    if (!condition) throw std::runtime_error(message);
}

float linear_gain(float db) { return std::pow(10.0F, db / 20.0F); }

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

void test_clean_transfer_and_bounds() {
    tabletfx::CleanBoost boost;
    require(boost.id() == "clean-boost", "clean boost stable ID changed");
    require(boost.gain_db() == 0.0F && boost.memory_bytes() == sizeof(boost),
            "clean boost defaults or memory report changed");
    std::array<float, 4> unity{0.25F, -0.5F, 0.75F, -1.0F};
    boost.process(unity, 2);
    require(unity == std::array<float, 4>{0.25F, -0.5F, 0.75F, -1.0F},
            "zero-dB boost changed the input");

    boost.set_gain_db_immediate(6.0F);
    std::array<float, 2> samples{0.25F, -0.25F};
    boost.process(samples, 2);
    require(std::abs(samples[0] - 0.25F * linear_gain(6.0F)) < 1.0e-6F &&
                std::abs(samples[1] + 0.25F * linear_gain(6.0F)) < 1.0e-6F,
            "clean boost did not preserve polarity and linear gain");

    boost.set_gain_db(100.0F);
    require(boost.gain_db() == 24.0F, "clean boost upper bound failed");
    boost.set_gain_db(-100.0F);
    require(boost.gain_db() == 0.0F, "clean boost lower bound failed");
    boost.set_gain_db(std::numeric_limits<float>::quiet_NaN());
    require(boost.gain_db() == 0.0F, "non-finite setting did not fall back to unity");
}

void test_smooth_ramp_and_reset() {
    tabletfx::CleanBoost boost(0.0F, 48000.0F, 10.0F);
    boost.set_gain_db(12.0F);
    std::vector<float> samples(480, 0.5F);
    boost.process(samples, 1);
    require(samples.front() > 0.5F && samples.front() < samples.back(),
            "clean boost did not begin a smooth gain ramp");
    for (std::size_t i = 1; i < samples.size(); ++i) {
        require(samples[i] >= samples[i - 1], "clean boost ramp was not monotonic");
    }
    require(std::abs(samples.back() - 0.5F * linear_gain(12.0F)) < 1.0e-6F,
            "clean boost ramp did not reach the target");

    boost.set_gain_db(3.0F);
    boost.reset();
    std::array<float, 1> after_reset{1.0F};
    boost.process(after_reset, 1);
    require(std::abs(after_reset[0] - linear_gain(3.0F)) < 1.0e-6F,
            "reset did not settle to the selected boost level");
}

void test_variable_blocks_and_no_allocations() {
    tabletfx::CleanBoost whole_boost(0.0F, 48000.0F, 10.0F);
    tabletfx::CleanBoost chunked_boost(0.0F, 48000.0F, 10.0F);
    whole_boost.set_gain_db(9.0F);
    chunked_boost.set_gain_db(9.0F);
    std::vector<float> whole(997, 0.25F);
    std::vector<float> chunked = whole;
    whole_boost.process(whole, 1);
    constexpr std::array<std::size_t, 5> blocks{1, 127, 53, 300, 17};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < chunked.size()) {
        const std::size_t count = std::min(blocks[block % blocks.size()],
                                           chunked.size() - offset);
        chunked_boost.process(std::span<float>(chunked).subspan(offset, count), 1);
        offset += count;
        ++block;
    }
    for (std::size_t i = 0; i < whole.size(); ++i) {
        require(std::abs(whole[i] - chunked[i]) < 1.0e-6F,
                "variable blocks changed clean boost output");
    }

    std::vector<float> rt_buffer(256 * 2, 0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int block_index = 0; block_index < 100; ++block_index) {
        chunked_boost.process(rt_buffer, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before,
            "clean boost allocated in the audio process method");
}

} // namespace

int main() {
    try {
        test_clean_transfer_and_bounds();
        test_smooth_ramp_and_reset();
        test_variable_blocks_and_no_allocations();
        std::cout << "All clean-boost tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Clean-boost test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
