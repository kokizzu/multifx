#include "tabletfx/effects/envelope_wah.hpp"

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

void test_sensitivity_direction_and_sweep() {
    tabletfx::EnvelopeWahParameters parameters;
    parameters.sensitivity = 1.0F;
    parameters.attack_ms = 0.0F;
    parameters.release_ms = 0.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::EnvelopeWah up(48000.0F, 1, parameters);
    require(up.id() == "envelope-wah" && up.memory_bytes() == sizeof(up),
            "envelope wah stable ID or memory reporting changed");
    require(std::abs(up.current_frequency_hz() - 350.0F) < 0.1F,
            "up-sweep reset did not start at the heel frequency");

    std::array<float, 1> loud{0.5F};
    up.process(loud, 1);
    require(std::abs(up.current_frequency_hz() - 2200.0F) < 0.2F &&
                std::abs(up.target_frequency_hz() - 2200.0F) < 0.2F,
            "up-sweep did not follow a strong guitar envelope to the toe");

    parameters.direction = tabletfx::EnvelopeWahDirection::Down;
    tabletfx::EnvelopeWah down(48000.0F, 1, parameters);
    require(std::abs(down.current_frequency_hz() - 2200.0F) < 0.2F,
            "down-sweep reset did not start at the high-frequency end");
    std::array<float, 1> down_loud{0.5F};
    down.process(down_loud, 1);
    require(std::abs(down.current_frequency_hz() - 350.0F) < 0.1F,
            "down-sweep did not move toward the heel on a strong envelope");

    parameters.direction = tabletfx::EnvelopeWahDirection::Up;
    parameters.sensitivity = 0.0F;
    tabletfx::EnvelopeWah less_sensitive(48000.0F, 1, parameters);
    std::array<float, 1> quiet{0.1F};
    less_sensitive.process(quiet, 1);
    require(std::abs(less_sensitive.current_frequency_hz() - 350.0F) < 0.1F,
            "low sensitivity opened below its envelope threshold");
}

void test_parameter_bounds_and_finite_recovery() {
    tabletfx::EnvelopeWahParameters parameters;
    parameters.sensitivity = 2.0F;
    parameters.minimum_hz = 9000.0F;
    parameters.maximum_hz = 100.0F;
    parameters.resonance_q = 100.0F;
    parameters.level_db = 100.0F;
    parameters.attack_ms = -1.0F;
    parameters.release_ms = 6000.0F;
    parameters.smoothing_ms = std::numeric_limits<float>::quiet_NaN();
    tabletfx::EnvelopeWah wah(48000.0F, 1, parameters);
    const auto bounded = wah.parameters();
    require(bounded.sensitivity == 1.0F && bounded.minimum_hz == 100.0F &&
                bounded.maximum_hz == 9000.0F && bounded.resonance_q == 10.0F &&
                bounded.level_db == 12.0F && bounded.attack_ms == 0.0F &&
                bounded.release_ms == 5000.0F && bounded.smoothing_ms == 5.0F,
            "envelope-wah parameter bounds or non-finite fallback failed");

    std::array<float, 4> invalid{std::numeric_limits<float>::quiet_NaN(),
                                 std::numeric_limits<float>::infinity(), 0.0F, 0.25F};
    wah.process(invalid, 1);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F,
            "non-finite input did not recover as silence");
    for (float sample : invalid) {
        require(std::isfinite(sample), "invalid input contaminated envelope-wah output");
    }
}

void test_block_invariance_stereo_and_no_allocations() {
    tabletfx::EnvelopeWahParameters parameters;
    parameters.sensitivity = 0.8F;
    tabletfx::EnvelopeWah whole_wah(48000.0F, 2, parameters);
    tabletfx::EnvelopeWah chunked_wah(48000.0F, 2, parameters);
    std::vector<float> whole(4096, 0.0F);
    for (std::size_t frame = 0; frame < whole.size() / 2; ++frame) {
        const float envelope = frame < 1000 ? 0.02F : 0.4F;
        whole[frame * 2] = envelope * std::sin(static_cast<float>(frame) * 0.13F);
        whole[frame * 2 + 1] = envelope * std::sin(static_cast<float>(frame) * 0.13F);
    }
    std::vector<float> chunked = whole;
    whole_wah.process(whole, 2);
    constexpr std::array<std::size_t, 5> blocks{1, 127, 53, 300, 17};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < chunked.size()) {
        const std::size_t count_frames = std::min(blocks[block % blocks.size()],
                                                  chunked.size() / 2 - offset / 2);
        const std::size_t count = count_frames * 2;
        chunked_wah.process(std::span<float>(chunked).subspan(offset, count), 2);
        offset += count;
        ++block;
    }
    for (std::size_t i = 0; i < whole.size(); ++i) {
        require(std::abs(whole[i] - chunked[i]) < 1.0e-6F,
                "variable block sizes changed the envelope-wah response");
    }
    for (std::size_t frame = 0; frame < whole.size() / 2; ++frame) {
        require(std::abs(whole[frame * 2] - whole[frame * 2 + 1]) < 1.0e-7F,
                "stereo-linked envelope or filter state diverged between channels");
    }

    std::vector<float> rt_buffer(256 * 2, 0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int i = 0; i < 100; ++i) chunked_wah.process(rt_buffer, 2);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "envelope wah allocated during audio processing");
}

} // namespace

int main() {
    try {
        test_sensitivity_direction_and_sweep();
        test_parameter_bounds_and_finite_recovery();
        test_block_invariance_stereo_and_no_allocations();
        std::cout << "All envelope-wah tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Envelope-wah test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
