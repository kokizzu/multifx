#include "tabletfx/effects/peak_limiter.hpp"

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

void require_below_ceiling(const std::vector<float>& samples, float ceiling,
                           const std::string& message) {
    for (float sample : samples) {
        require(std::isfinite(sample) && std::abs(sample) <= ceiling + 2.0e-6F,
                message);
    }
}

void test_default_and_bounds() {
    tabletfx::PeakLimiter limiter;
    require(limiter.id() == "peak-limiter", "limiter ID changed");
    require(std::abs(limiter.ceiling_db() + 1.0F) < 1.0e-6F &&
            std::abs(limiter.release_ms() - 80.0F) < 1.0e-6F,
            "sane limiter defaults changed unexpectedly");
    require(limiter.memory_bytes() == sizeof(limiter),
            "limiter memory report is inaccurate");

    limiter.configure(1.0F, -100.0F, -1.0F);
    require(std::abs(limiter.ceiling_db() + 24.0F) < 1.0e-6F &&
            std::abs(limiter.release_ms() - 5.0F) < 1.0e-6F,
            "limiter did not bound finite controls or fall back on sample rate");
    limiter.set_ceiling_db(std::numeric_limits<float>::quiet_NaN());
    limiter.set_release_ms(std::numeric_limits<float>::infinity());
    require(std::abs(limiter.ceiling_db() + 1.0F) < 1.0e-6F &&
            std::abs(limiter.release_ms() - 80.0F) < 1.0e-6F,
            "non-finite controls did not return to safe defaults");
}

void test_impulse_step_and_zero_lookahead() {
    tabletfx::PeakLimiter limiter(48000.0F, -1.0F, 80.0F);
    const float ceiling = limiter.ceiling_linear();
    std::vector<float> mono{2.0F, 0.5F, -3.0F, -0.5F, 1.0F};
    limiter.process(mono, 1);
    require(std::abs(mono[0] - ceiling) < 2.0e-6F,
            "positive impulse was not limited on the same sample");
    require(std::abs(mono[2] + ceiling) < 2.0e-6F,
            "negative impulse was not limited on the same sample");
    require(std::abs(mono[4]) <= ceiling + 2.0e-6F,
            "unity-level sample exceeded configured ceiling");
    require_below_ceiling(mono, ceiling,
                          "limiter output exceeded configured ceiling");

    limiter.reset();
    std::vector<float> step(128, 2.0F);
    limiter.process(step, 1);
    require_below_ceiling(step, ceiling,
                          "over-ceiling step was not limited sample by sample");
}

void test_linked_stereo_and_release() {
    tabletfx::PeakLimiter limiter(48000.0F, -1.0F, 80.0F);
    const float ceiling = limiter.ceiling_linear();
    std::array<float, 2> stereo_frame{2.0F, 0.5F};
    limiter.process(stereo_frame, 2);
    const float shared_gain = stereo_frame[1] / 0.5F;
    require(std::abs(stereo_frame[0] - ceiling) < 2.0e-6F &&
            shared_gain < 1.0F &&
            std::abs(shared_gain - stereo_frame[0] / 2.0F) < 2.0e-6F,
            "stereo channels did not share linked peak gain");

    const float gain_before_release = shared_gain;
    std::vector<float> quiet(4800, 0.25F);
    limiter.process(quiet, 1);
    const float gain_after_100_ms = quiet.back() / 0.25F;
    require(gain_after_100_ms > gain_before_release && gain_after_100_ms < 1.0F,
            "gain did not recover smoothly after a peak");

    std::vector<float> longer_quiet(48000, 0.25F);
    limiter.process(longer_quiet, 1);
    require(longer_quiet.back() / 0.25F > gain_after_100_ms,
            "release gain did not continue returning toward unity");
}

void test_variable_blocks_nonfinite_and_reset() {
    tabletfx::PeakLimiter whole(44100.0F, -3.0F, 120.0F);
    tabletfx::PeakLimiter chunked(44100.0F, -3.0F, 120.0F);
    std::vector<float> input(2051);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = (i % 17 == 0) ? 1.5F : 0.2F * std::sin(static_cast<float>(i) * 0.17F);
    }
    auto expected = input;
    auto actual = input;
    whole.process(expected, 1);

    const std::array<std::size_t, 5> block_sizes{1, 127, 53, 300, 17};
    std::size_t frame = 0;
    std::size_t block = 0;
    while (frame < actual.size()) {
        const auto count = std::min(block_sizes[block % block_sizes.size()],
                                    actual.size() - frame);
        chunked.process(std::span<float>(actual).subspan(frame, count), 1);
        frame += count;
        ++block;
    }
    for (std::size_t i = 0; i < actual.size(); ++i) {
        require(std::abs(expected[i] - actual[i]) < 1.0e-6F,
                "variable block sizes changed limiter output");
    }

    std::array<float, 4> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max()};
    whole.reset();
    whole.process(invalid, 1);
    for (float sample : invalid) {
        require(std::isfinite(sample) && std::abs(sample) <= whole.ceiling_linear() + 2.0e-6F,
                "non-finite or extreme sample escaped limiter protection");
    }

    whole.reset();
    std::array<float, 1> after_reset{0.25F};
    whole.process(after_reset, 1);
    require(std::abs(after_reset[0] - 0.25F) < 1.0e-6F,
            "reset did not restore unity gain state");
}

void test_zero_process_allocations() {
    tabletfx::PeakLimiter limiter;
    std::array<float, 256 * 2> block{};
    block.fill(0.75F);
    limiter.process(block, 2);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        limiter.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "limiter allocated memory during process()");
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
        test_default_and_bounds();
        test_impulse_step_and_zero_lookahead();
        test_linked_stereo_and_release();
        test_variable_blocks_nonfinite_and_reset();
        test_zero_process_allocations();
        std::cout << "All TabletFX peak limiter tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
