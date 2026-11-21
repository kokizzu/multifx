#include "tabletfx/effects/triple_double_chorus.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <span>

using tabletfx::effects::TripleDoubleChorus;
using tabletfx::effects::TripleDoubleChorusConfig;
using tabletfx::effects::TripleDoubleChorusParameters;

namespace {

std::atomic<bool> track_allocations{false};
std::atomic<std::size_t> allocation_count{0};

TripleDoubleChorus whole_block(48000.0F);
TripleDoubleChorus split_blocks(48000.0F);
TripleDoubleChorus allocation_test(48000.0F);

void assert_parameters(const TripleDoubleChorusParameters& actual,
                       const TripleDoubleChorusParameters& expected) {
    assert(std::abs(actual.rate_hz - expected.rate_hz) < 1.0e-6F);
    assert(std::abs(actual.left_depth_ms - expected.left_depth_ms) < 1.0e-6F);
    assert(std::abs(actual.center_depth_ms - expected.center_depth_ms) < 1.0e-6F);
    assert(std::abs(actual.right_depth_ms - expected.right_depth_ms) < 1.0e-6F);
    assert(std::abs(actual.feedback - expected.feedback) < 1.0e-6F);
    assert(std::abs(actual.sparkle - expected.sparkle) < 1.0e-6F);
}

void test_project_authored_presets_and_manual_switch() {
    TripleDoubleChorusConfig config;
    whole_block.configure(config);
    assert_parameters(whole_block.effective_parameters(),
                      {0.8F, 7.0F, 5.0F, 8.0F, 0.1F, 0.2F});

    config.preset_switch = true;
    whole_block.configure(config);
    assert_parameters(whole_block.effective_parameters(),
                      {0.55F, 18.0F, 12.0F, 18.0F, 0.25F, 0.45F});

    config.manual_switch = true;
    config.rate_hz = 2.0F;
    config.left_depth_ms = 1.0F;
    config.center_depth_ms = 2.0F;
    config.right_depth_ms = 3.0F;
    config.feedback = -0.3F;
    config.sparkle = 0.8F;
    whole_block.configure(config);
    assert_parameters(whole_block.effective_parameters(),
                      {2.0F, 1.0F, 2.0F, 3.0F, -0.3F, 0.8F});
}

void test_control_bounds() {
    TripleDoubleChorusConfig config;
    config.manual_switch = true;
    config.rate_hz = std::numeric_limits<float>::infinity();
    config.left_depth_ms = 99.0F;
    config.center_depth_ms = -1.0F;
    config.right_depth_ms = std::numeric_limits<float>::quiet_NaN();
    config.feedback = -5.0F;
    config.sparkle = 2.0F;
    whole_block.configure(config);

    assert(std::abs(whole_block.config().rate_hz - 0.8F) < 1.0e-6F);
    assert(std::abs(whole_block.config().left_depth_ms - 20.0F) < 1.0e-6F);
    assert(std::abs(whole_block.config().center_depth_ms - 0.0F) < 1.0e-6F);
    assert(std::abs(whole_block.config().right_depth_ms - 8.0F) < 1.0e-6F);
    assert(std::abs(whole_block.config().feedback + 0.8F) < 1.0e-6F);
    assert(std::abs(whole_block.config().sparkle - 1.0F) < 1.0e-6F);
}

void test_chunk_partition_and_stereo_policy() {
    TripleDoubleChorusConfig config;
    config.manual_switch = true;
    config.rate_hz = 0.7F;
    config.left_depth_ms = 12.0F;
    config.center_depth_ms = 7.0F;
    config.right_depth_ms = 16.0F;
    config.feedback = 0.2F;
    config.sparkle = 0.5F;
    config.stereo_switch = true;
    whole_block.configure(config);
    split_blocks.configure(config);
    whole_block.reset();
    split_blocks.reset();

    constexpr std::size_t kFrames = 8192;
    std::array<float, kFrames> input{};
    std::array<float, kFrames> left_whole{};
    std::array<float, kFrames> right_whole{};
    std::array<float, kFrames> left_split{};
    std::array<float, kFrames> right_split{};
    for (std::size_t i = 0; i < kFrames; ++i) {
        input[i] = 0.3F * std::sin(
            2.0F * 3.14159265358979323846F * 220.0F *
            static_cast<float>(i) / 48000.0F);
    }

    whole_block.process(input.data(), nullptr,
                        left_whole.data(), right_whole.data(), kFrames);
    constexpr std::array<std::size_t, 4> chunks = {1, 63, 512, 7};
    std::size_t offset = 0;
    std::size_t chunk = 0;
    while (offset < kFrames) {
        const std::size_t count = std::min(
            chunks[chunk++ % chunks.size()], kFrames - offset);
        split_blocks.process(input.data() + offset, nullptr,
                             left_split.data() + offset,
                             right_split.data() + offset, count);
        offset += count;
    }

    bool stereo_width_observed = false;
    for (std::size_t i = 0; i < kFrames; ++i) {
        assert(std::isfinite(left_whole[i]));
        assert(std::isfinite(right_whole[i]));
        assert(std::abs(left_whole[i] - left_split[i]) < 1.0e-6F);
        assert(std::abs(right_whole[i] - right_split[i]) < 1.0e-6F);
        if (std::abs(left_whole[i] - right_whole[i]) > 1.0e-5F) {
            stereo_width_observed = true;
        }
    }
    assert(stereo_width_observed);

    config.stereo_switch = false;
    whole_block.configure(config);
    whole_block.reset();
    whole_block.process(input.data(), nullptr,
                        left_whole.data(), right_whole.data(), kFrames);
    for (std::size_t i = 0; i < kFrames; ++i) {
        assert(std::abs(left_whole[i] - right_whole[i]) < 1.0e-6F);
    }
}

void test_finite_recovery_reset_and_no_allocation() {
    TripleDoubleChorusConfig config;
    config.manual_switch = true;
    config.feedback = 0.7F;
    allocation_test.configure(config);
    allocation_test.reset();

    std::array<float, 256> input{};
    std::array<float, 256> left{};
    std::array<float, 256> right{};
    input[10] = 0.5F;
    input[40] = std::numeric_limits<float>::quiet_NaN();
    input[80] = std::numeric_limits<float>::infinity();
    input[120] = -std::numeric_limits<float>::infinity();

    allocation_count.store(0, std::memory_order_relaxed);
    track_allocations.store(true, std::memory_order_relaxed);
    allocation_test.process(input.data(), nullptr, left.data(), right.data(),
                            input.size());
    track_allocations.store(false, std::memory_order_relaxed);
    assert(allocation_count.load(std::memory_order_relaxed) == 0);
    for (std::size_t i = 0; i < input.size(); ++i) {
        assert(std::isfinite(left[i]));
        assert(std::isfinite(right[i]));
    }

    allocation_test.reset();
    std::array<float, 128> silence{};
    std::array<float, 128> silent_left{};
    std::array<float, 128> silent_right{};
    allocation_test.process(silence.data(), nullptr,
                            silent_left.data(), silent_right.data(), silence.size());
    for (std::size_t i = 0; i < silence.size(); ++i) {
        assert(std::abs(silent_left[i]) < 1.0e-7F);
        assert(std::abs(silent_right[i]) < 1.0e-7F);
    }
}

void test_effect_interface_and_interleaved_processing() {
    TripleDoubleChorus effect(48000.0F);
    TripleDoubleChorusConfig config;
    config.manual_switch = true;
    config.rate_hz = 1.2F;
    config.left_depth_ms = 10.0F;
    config.center_depth_ms = 6.0F;
    config.right_depth_ms = 14.0F;
    effect.configure(config);

    std::array<float, 1024> stereo{};
    stereo[0] = 0.5F;
    effect.process(std::span<float>(stereo), 2);
    assert(effect.id() == "triple-double-chorus");
    assert(effect.memory_bytes() == sizeof(effect));
    assert(std::any_of(stereo.begin(), stereo.end(),
                       [](float sample) { return std::abs(sample) > 1.0e-5F; }));
    for (float sample : stereo) assert(std::isfinite(sample));

    effect.reset();
    std::array<float, 512> mono{};
    mono[0] = 0.5F;
    effect.process(std::span<float>(mono), 1);
    for (float sample : mono) assert(std::isfinite(sample));

    effect.reset();
    allocation_count.store(0, std::memory_order_relaxed);
    track_allocations.store(true, std::memory_order_relaxed);
    effect.process(std::span<float>(stereo), 2);
    track_allocations.store(false, std::memory_order_relaxed);
    assert(allocation_count.load(std::memory_order_relaxed) == 0);
}

}  // namespace

void* operator new(std::size_t size) {
    if (track_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (track_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    test_project_authored_presets_and_manual_switch();
    test_control_bounds();
    test_chunk_partition_and_stereo_policy();
    test_finite_recovery_reset_and_no_allocation();
    test_effect_interface_and_interleaved_processing();
    return 0;
}
