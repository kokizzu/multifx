#include "tabletfx/effects/double_take.hpp"

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

void test_defaults_bounds_and_memory() {
    tabletfx::DoubleTake effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "double-take", "Double Take stable ID changed");
    require(defaults.doubles == 2U && defaults.slop == 0.25F &&
                defaults.sensitivity == 0.55F && defaults.dry_level == 0.85F &&
                defaults.wet_level == 0.15F &&
                defaults.source == tabletfx::DoubleTakeSource::mono,
            "project-authored Double Take defaults changed");
    require(effect.memory_bytes() == sizeof(effect) + 1154U * 2U * sizeof(float),
            "Double Take delay memory estimate is inaccurate at 48 kHz stereo");

    tabletfx::DoubleTakeParameters parameters;
    parameters.doubles = 0;
    parameters.slop = -1.0F;
    parameters.sensitivity = 2.0F;
    parameters.dry_level = -3.0F;
    parameters.wet_level = 9.0F;
    effect.set_parameters(parameters);
    const auto bounded = effect.parameters();
    require(bounded.doubles == 1U && bounded.slop == 0.0F &&
                bounded.sensitivity == 1.0F && bounded.dry_level == 0.0F &&
                bounded.wet_level == 1.0F,
            "Double Take project parameter bounds were not enforced");

    parameters.doubles = 9;
    parameters.slop = std::numeric_limits<float>::quiet_NaN();
    parameters.sensitivity = std::numeric_limits<float>::infinity();
    parameters.dry_level = std::numeric_limits<float>::quiet_NaN();
    parameters.wet_level = std::numeric_limits<float>::infinity();
    parameters.source = static_cast<tabletfx::DoubleTakeSource>(255U);
    effect.set_parameters(parameters);
    const auto finite = effect.parameters();
    require(finite.doubles == 4U && finite.slop == 0.25F &&
                finite.sensitivity == 0.55F && finite.dry_level == 0.85F &&
                finite.wet_level == 0.15F &&
                finite.source == tabletfx::DoubleTakeSource::mono,
            "non-finite or invalid Double Take controls did not use safe values");

    tabletfx::DoubleTake mono(48000.0F, 1);
    tabletfx::DoubleTakeParameters stereo_only;
    stereo_only.source = tabletfx::DoubleTakeSource::right_only;
    mono.set_parameters(stereo_only);
    require(mono.parameters().source == tabletfx::DoubleTakeSource::mono,
            "mono Double Take exposed a stereo-only Source mode");
}

void test_dry_path_is_immediate_and_wet_path_is_bounded() {
    tabletfx::DoubleTakeParameters parameters;
    parameters.doubles = 1;
    parameters.slop = 0.0F;
    parameters.dry_level = 1.0F;
    parameters.wet_level = 0.0F;
    tabletfx::DoubleTake dry(8000.0F, 1, parameters);
    std::array<float, 80> dry_impulse{};
    dry_impulse[0] = 0.75F;
    dry.process(dry_impulse, 1);
    require(std::abs(dry_impulse[0] - 0.75F) < 1.0e-7F,
            "Double Take added delay or lookahead to its direct path");
    require(std::all_of(dry_impulse.begin() + 1, dry_impulse.end(),
                        [](float value) { return std::abs(value) < 1.0e-7F; }),
            "zero-wet Double Take changed the dry impulse tail");

    parameters.dry_level = 0.0F;
    parameters.wet_level = 1.0F;
    tabletfx::DoubleTake wet(8000.0F, 1, parameters);
    std::array<float, 96> wet_impulse{};
    wet_impulse[0] = 1.0F;
    wet.process(wet_impulse, 1);
    const auto first_wet = std::find_if(
        wet_impulse.begin(), wet_impulse.end(),
        [](float value) { return std::abs(value) > 1.0e-6F; });
    require(first_wet == wet_impulse.begin() + 56,
            "project-authored wet voice delay changed from 7 ms at 8 kHz");
    require(std::all_of(wet_impulse.begin(), first_wet,
                        [](float value) { return std::abs(value) < 1.0e-7F; }),
            "wet voice produced output before its configured delay");
}

void test_stereo_source_routing() {
    tabletfx::DoubleTakeParameters parameters;
    parameters.doubles = 1;
    parameters.slop = 0.0F;
    parameters.dry_level = 0.0F;
    parameters.wet_level = 1.0F;

    parameters.source = tabletfx::DoubleTakeSource::true_stereo;
    tabletfx::DoubleTake true_stereo(8000.0F, 2, parameters);
    std::array<float, 128> left_impulse{};
    left_impulse[0] = 1.0F;
    true_stereo.process(left_impulse, 2);
    require(left_impulse[56U * 2U] > 0.9F &&
                std::abs(left_impulse[56U * 2U + 1U]) < 1.0e-7F,
            "project True Stereo routing did not preserve channel separation");

    parameters.source = tabletfx::DoubleTakeSource::mono;
    tabletfx::DoubleTake mono_source(8000.0F, 2, parameters);
    std::array<float, 128> mono_impulse{};
    mono_impulse[0] = 1.0F;
    mono_source.process(mono_impulse, 2);
    require(mono_impulse[56U * 2U] > 0.2F &&
                mono_impulse[56U * 2U + 1U] > 0.2F,
            "project Mono source routing did not create a centered stereo voice");

    parameters.source = tabletfx::DoubleTakeSource::left_only;
    tabletfx::DoubleTake left_only(8000.0F, 2, parameters);
    std::array<float, 128> right_input{};
    right_input[1] = 1.0F;
    left_only.process(right_input, 2);
    require(std::abs(right_input[56U * 2U]) < 1.0e-7F &&
                std::abs(right_input[56U * 2U + 1U]) < 1.0e-7F,
            "project Left Only source consumed right-channel input");

    parameters.source = tabletfx::DoubleTakeSource::right_only;
    tabletfx::DoubleTake right_only(8000.0F, 2, parameters);
    std::array<float, 128> right_impulse{};
    right_impulse[1] = 1.0F;
    right_only.process(right_impulse, 2);
    require(right_impulse[56U * 2U] > 0.2F &&
                right_impulse[56U * 2U + 1U] > 0.2F,
            "project Right Only source did not feed the generated voice");
}

void test_block_invariance_parameter_ramps_and_reset() {
    tabletfx::DoubleTake whole(48000.0F, 2);
    tabletfx::DoubleTake chunked(48000.0F, 2);
    constexpr std::size_t frames = 4099;
    std::vector<float> input(frames * 2U);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float phase = 2.0F * 3.14159265358979323846F * 220.0F *
                            static_cast<float>(frame) / 48000.0F;
        input[frame * 2U] = 0.4F * std::sin(phase);
        input[frame * 2U + 1U] = 0.3F * std::cos(phase * 0.997F);
    }
    auto a = input;
    auto b = input;

    constexpr std::size_t prefix_frames = 113;
    whole.process(std::span<float>(a.data(), prefix_frames * 2U), 2);
    std::size_t offset = 0;
    const std::array<std::size_t, 5> initial_blocks{1U, 7U, 31U, 64U, 10U};
    for (const std::size_t block_frames : initial_blocks) {
        chunked.process(std::span<float>(b.data() + offset * 2U,
                                         block_frames * 2U), 2);
        offset += block_frames;
    }
    require(offset == prefix_frames,
            "Double Take test prefix does not align with its block sequence");

    tabletfx::DoubleTakeParameters changed;
    changed.doubles = 4;
    changed.slop = 0.9F;
    changed.sensitivity = 0.8F;
    changed.dry_level = 0.65F;
    changed.wet_level = 0.3F;
    changed.source = tabletfx::DoubleTakeSource::true_stereo;
    whole.set_parameters(changed);
    chunked.set_parameters(changed);
    whole.process(std::span<float>(a.data() + prefix_frames * 2U,
                                   a.size() - prefix_frames * 2U), 2);

    offset = prefix_frames;
    const std::array<std::size_t, 6> blocks{17U, 1U, 127U, 63U, 255U, 32U};
    std::size_t block_index = 0;
    while (offset < frames) {
        const std::size_t count =
            std::min(blocks[block_index++ % blocks.size()], frames - offset);
        chunked.process(std::span<float>(b.data() + offset * 2U, count * 2U), 2);
        offset += count;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        require(std::abs(a[i] - b[i]) < 1.0e-6F,
                "Double Take output depends on callback block boundaries");
    }

    whole.reset();
    tabletfx::DoubleTake fresh(48000.0F, 2, changed);
    std::array<float, 512> after_reset{};
    for (std::size_t i = 0; i < after_reset.size(); ++i) {
        after_reset[i] = input[i];
    }
    auto from_fresh = after_reset;
    whole.process(after_reset, 2);
    fresh.process(from_fresh, 2);
    require(after_reset == from_fresh,
            "Double Take reset did not restore the deterministic initial state");
}

void test_invalid_layout_nonfinite_samples_and_no_allocations() {
    tabletfx::DoubleTake effect(48000.0F, 2);
    std::array<float, 8> wrong_layout{0.1F, -0.2F, 0.3F, -0.4F,
                                      0.5F, -0.6F, 0.7F, -0.8F};
    const auto before = wrong_layout;
    effect.process(wrong_layout, 1);
    require(wrong_layout == before,
            "Double Take modified a buffer with a mismatched channel layout");

    std::array<float, 16> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        0.25F, -0.25F, 0.5F,
        0.0F, 0.0F, 0.0F, 0.0F,
        0.1F, -0.1F, 0.2F, -0.2F};
    effect.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float value) { return std::isfinite(value); }),
            "Double Take did not recover to finite output after invalid samples");

    std::array<float, 512> block{};
    block.fill(0.2F);
    const std::size_t before_allocations =
        allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        effect.process(block, 2);
    }
    const std::size_t after_allocations =
        allocation_count.load(std::memory_order_relaxed);
    require(after_allocations == before_allocations,
            "Double Take allocated memory during process()");
    require(std::all_of(block.begin(), block.end(),
                        [](float value) { return std::isfinite(value); }),
            "Double Take emitted non-finite output after repeated processing");
}

void test_invalid_construction() {
    bool rejected_rate = false;
    try {
        tabletfx::DoubleTake invalid(1000.0F, 1);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    require(rejected_rate, "Double Take accepted an unsupported sample rate");

    bool rejected_channels = false;
    try {
        tabletfx::DoubleTake invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    require(rejected_channels, "Double Take accepted more than stereo channels");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
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
    try {
        test_defaults_bounds_and_memory();
        test_dry_path_is_immediate_and_wet_path_is_bounded();
        test_stereo_source_routing();
        test_block_invariance_parameter_ramps_and_reset();
        test_invalid_layout_nonfinite_samples_and_no_allocations();
        test_invalid_construction();
        std::cout << "All Double Take tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Double Take test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
