#include "tabletfx/channel_conversion.hpp"

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

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_near(float actual, float expected, float tolerance, const char* message) {
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
}

void test_mono_to_stereo_duplicates_each_frame() {
    const std::vector<float> input{0.75F, -0.25F, 0.0F};
    std::vector<float> output(6, 99.0F);
    const auto result = tabletfx::convert_channels(input, 1, output, 2);
    require(result.succeeded() && result.frames == 3, "mono-to-stereo conversion failed");
    require(output == std::vector<float>({0.75F, 0.75F, -0.25F, -0.25F, 0.0F, 0.0F}),
            "mono input was not duplicated to left and right");
}

void test_stereo_to_mono_policies() {
    const std::vector<float> input{0.5F, 0.25F, -0.5F, 0.5F};
    std::vector<float> average(2);
    auto result = tabletfx::convert_channels(input, 2, average, 1);
    require(result.succeeded() && result.frames == 2, "average downmix failed");
    require_near(average[0], 0.375F, 1.0e-7F, "average downmix did not average channels");
    require_near(average[1], 0.0F, 1.0e-7F, "average downmix did not cancel opposite phase");

    std::vector<float> equal_power(2);
    result = tabletfx::convert_channels(input, 2, equal_power, 1,
                                        tabletfx::MonoSumPolicy::EqualPower);
    require(result.succeeded(), "equal-power downmix failed");
    require_near(equal_power[0], 0.53033006F, 1.0e-6F,
                 "equal-power downmix used the wrong coefficient");
    require_near(equal_power[1], 0.0F, 1.0e-7F,
                 "equal-power downmix did not preserve opposite-phase cancellation");
}

void test_same_layout_copy_and_in_place() {
    const std::vector<float> mono{0.125F, -0.75F};
    std::vector<float> copy(2);
    auto result = tabletfx::convert_channels(mono, 1, copy, 1);
    require(result.succeeded() && copy == mono, "mono copy changed samples");

    std::vector<float> stereo_in_place{0.25F, -0.5F, 0.75F, -1.0F};
    result = tabletfx::convert_channels(stereo_in_place, 2, stereo_in_place, 2);
    require(result.succeeded() && stereo_in_place == std::vector<float>({0.25F, -0.5F, 0.75F, -1.0F}),
            "same-layout in-place conversion changed samples");

    std::vector<float> mono_to_stereo_in_place{0.5F, -0.25F, 0.0F, 0.0F};
    result = tabletfx::convert_channels(
        std::span<const float>(mono_to_stereo_in_place.data(), 2), 1,
        mono_to_stereo_in_place, 2);
    require(result.succeeded() && mono_to_stereo_in_place ==
                std::vector<float>({0.5F, 0.5F, -0.25F, -0.25F}),
            "in-place upmix overwrote an unread mono frame");

    std::vector<float> stereo_to_mono_in_place{0.5F, 0.25F, -0.5F, 0.5F};
    result = tabletfx::convert_channels(stereo_to_mono_in_place, 2,
                                        stereo_to_mono_in_place, 1);
    require(result.succeeded(), "in-place downmix failed");
    require_near(stereo_to_mono_in_place[0], 0.375F, 1.0e-7F,
                 "in-place downmix corrupted the first frame");
    require_near(stereo_to_mono_in_place[1], 0.0F, 1.0e-7F,
                 "in-place downmix corrupted the second frame");
}

void test_invalid_layouts_and_buffers_are_rejected_without_writes() {
    const std::vector<float> input{0.25F, -0.25F};
    std::vector<float> output{8.0F, 8.0F, 8.0F};
    auto result = tabletfx::convert_channels(input, 3, output, 2);
    require(result.status == tabletfx::ChannelConversionStatus::UnsupportedChannelCount,
            "unsupported input layout was accepted");
    require(output == std::vector<float>({8.0F, 8.0F, 8.0F}),
            "unsupported layout modified output");

    const std::vector<float> partial_frame{0.25F, -0.25F, 0.5F};
    result = tabletfx::convert_channels(partial_frame, 2, output, 1);
    require(result.status == tabletfx::ChannelConversionStatus::InvalidInputLength,
            "partial stereo frame was accepted");

    const std::vector<float> two_frames{0.25F, -0.25F, 0.5F, 0.75F};
    std::vector<float> short_output{7.0F};
    result = tabletfx::convert_channels(two_frames, 2, short_output, 1);
    require(result.status == tabletfx::ChannelConversionStatus::OutputTooSmall,
            "undersized output was accepted");
    require(short_output[0] == 7.0F, "undersized output was partially modified");

    result = tabletfx::convert_channels(input, 2, output, 1,
                                        static_cast<tabletfx::MonoSumPolicy>(99));
    require(result.status == tabletfx::ChannelConversionStatus::InvalidSummingPolicy,
            "unknown summing policy was accepted");

    std::vector<float> overlap{0.25F, 0.5F, 0.75F, 1.0F, 0.0F};
    result = tabletfx::convert_channels(
        std::span<const float>(overlap.data(), 4), 2,
        std::span<float>(overlap.data() + 1, 2), 1);
    require(result.status == tabletfx::ChannelConversionStatus::OverlappingBuffers,
            "partially overlapping buffers were accepted");
}

void test_empty_and_no_allocation() {
    std::vector<float> empty;
    std::vector<float> output(512, 0.125F);
    auto result = tabletfx::convert_channels(empty, 1, output, 2);
    require(result.succeeded() && result.frames == 0,
            "empty input did not produce an empty successful result");

    const std::vector<float> input(256, 0.125F);
    std::vector<float> stereo(512);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int i = 0; i < 64; ++i) {
        result = tabletfx::convert_channels(input, 1, stereo, 2);
        require(result.succeeded(), "repeated real-time conversion failed");
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "channel conversion allocated memory");
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
        test_mono_to_stereo_duplicates_each_frame();
        test_stereo_to_mono_policies();
        test_same_layout_copy_and_in_place();
        test_invalid_layouts_and_buffers_are_rejected_without_writes();
        test_empty_and_no_allocation();
        std::cout << "Mono/stereo conversion tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
