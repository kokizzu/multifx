#include "tabletfx/effects/overdrive.hpp"

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

void test_defaults_bounds_and_identity() {
    tabletfx::TransparentOverdrive overdrive;
    require(overdrive.id() == "transparent-overdrive", "stable overdrive ID changed");
    require(overdrive.drive() == 0.15F && overdrive.level_db() == -3.0F &&
                overdrive.mix() == 0.35F,
            "overdrive defaults changed");
    require(overdrive.memory_bytes() == sizeof(overdrive),
            "overdrive memory report omitted or invented owned storage");

    tabletfx::TransparentOverdriveParameters bypass{};
    bypass.drive = 0.5F;
    bypass.level_db = 0.0F;
    bypass.mix = 0.0F;
    bypass.smoothing_ms = 0.0F;
    overdrive.configure(bypass);
    std::array<float, 4> input{0.25F, -0.5F, 0.75F, -1.0F};
    const auto original = input;
    overdrive.process(input, 2);
    require(input == original, "zero mix and zero level must be exact dry passthrough");

    overdrive.set_drive(9.0F);
    require(overdrive.drive() == 0.5F, "drive upper bound failed");
    overdrive.set_drive(-1.0F);
    require(overdrive.drive() == 0.0F, "drive lower bound failed");
    overdrive.set_drive(std::numeric_limits<float>::quiet_NaN());
    require(overdrive.drive() == 0.15F, "invalid drive must return to its sane default");
    overdrive.set_level_db(-100.0F);
    require(overdrive.level_db() == -24.0F, "level lower bound failed");
    overdrive.set_level_db(100.0F);
    require(overdrive.level_db() == 12.0F, "level upper bound failed");
    overdrive.set_mix(-1.0F);
    require(overdrive.mix() == 0.0F, "mix lower bound failed");
    overdrive.set_mix(2.0F);
    require(overdrive.mix() == 1.0F, "mix upper bound failed");
}

void test_soft_clip_symmetry_and_stereo_independence() {
    tabletfx::TransparentOverdriveParameters parameters{};
    parameters.drive = 0.35F;
    parameters.level_db = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::TransparentOverdrive overdrive(48000.0F, parameters);
    std::array<float, 6> stereo{0.5F, -0.5F, 0.2F, 0.7F, -2.0F, 2.0F};
    overdrive.process(stereo, 2);
    require(stereo[0] > 0.5F && stereo[1] < -0.5F,
            "drive did not produce soft-clipped overdrive");
    require(std::abs(stereo[0] + stereo[1]) < 1.0e-6F,
            "soft clipping must preserve odd symmetry");
    require(stereo[4] == -1.0F && stereo[5] == 1.0F,
            "large finite values must enter the bounded saturation region");

    tabletfx::TransparentOverdrive mono(48000.0F, parameters);
    std::array<float, 1> left{0.2F};
    std::array<float, 1> right{0.7F};
    mono.process(left, 1);
    mono.reset();
    mono.process(right, 1);
    require(std::abs(stereo[2] - left[0]) < 1.0e-6F &&
                std::abs(stereo[3] - right[0]) < 1.0e-6F,
            "stereo samples were coupled or channel order changed");
}

void test_smoothing_reset_and_block_invariance() {
    tabletfx::TransparentOverdriveParameters initial{};
    initial.drive = 0.0F;
    initial.level_db = 0.0F;
    initial.mix = 1.0F;
    tabletfx::TransparentOverdrive whole(48000.0F, initial);
    tabletfx::TransparentOverdrive split(48000.0F, initial);
    whole.set_drive(0.5F);
    split.set_drive(0.5F);
    std::vector<float> whole_buffer(997, 0.5F);
    std::vector<float> split_buffer = whole_buffer;
    whole.process(whole_buffer, 1);
    constexpr std::array<std::size_t, 5> blocks{1, 127, 53, 300, 17};
    std::size_t offset = 0;
    std::size_t block_index = 0;
    while (offset < split_buffer.size()) {
        const std::size_t count = std::min(blocks[block_index % blocks.size()],
                                           split_buffer.size() - offset);
        split.process(std::span<float>(split_buffer).subspan(offset, count), 1);
        offset += count;
        ++block_index;
    }
    for (std::size_t i = 0; i < whole_buffer.size(); ++i) {
        require(std::abs(whole_buffer[i] - split_buffer[i]) < 1.0e-6F,
                "smoothing depends on callback partitioning");
    }
    require(whole_buffer.front() < whole_buffer.back(),
            "drive retarget did not produce a smooth increasing response");

    tabletfx::TransparentOverdrive stereo(48000.0F, initial);
    stereo.set_drive(0.5F);
    std::array<float, 512> stereo_buffer{};
    stereo_buffer.fill(0.5F);
    stereo.process(stereo_buffer, 2);
    for (std::size_t frame = 0; frame < stereo_buffer.size() / 2; ++frame) {
        require(stereo_buffer[frame * 2] == stereo_buffer[frame * 2 + 1],
                "stereo parameter ramps must advance once per frame and stay linked");
    }

    whole.reset();
    tabletfx::TransparentOverdrive settled(48000.0F, initial);
    settled.set_drive(0.5F);
    settled.reset();
    std::array<float, 1> after_reset{0.5F};
    std::array<float, 1> settled_sample{0.5F};
    whole.process(after_reset, 1);
    settled.process(settled_sample, 1);
    require(std::abs(after_reset[0] - settled_sample[0]) < 1.0e-6F,
            "reset did not settle immediately to the configured target");
}

void test_invalid_layout_non_finite_and_no_allocations() {
    tabletfx::TransparentOverdrive overdrive;
    std::array<float, 2> untouched{0.25F, -0.5F};
    const auto original = untouched;
    overdrive.set_drive(0.5F);
    overdrive.process(untouched, 0);
    require(untouched == original, "zero-channel layout must not touch samples");

    std::array<float, 3> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()};
    overdrive.process(invalid, 1);
    require(invalid == std::array<float, 3>{0.0F, 0.0F, 0.0F},
            "non-finite audio must recover to silence");

    std::vector<float> buffer(256 * 2, 0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int i = 0; i < 100; ++i) overdrive.process(buffer, 2);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "process allocated memory");
}

} // namespace

int main() {
    try {
        test_defaults_bounds_and_identity();
        test_soft_clip_symmetry_and_stereo_independence();
        test_smoothing_reset_and_block_invariance();
        test_invalid_layout_non_finite_and_no_allocations();
        std::cout << "All transparent-overdrive tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Transparent-overdrive test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
