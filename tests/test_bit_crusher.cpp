#include "tabletfx/effects/bit_crusher.hpp"

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

void test_defaults_bounds_and_fixed_memory() {
    tabletfx::BitCrusher crusher;
    const auto defaults = crusher.parameters();
    require(crusher.id() == "bit-crusher", "bit crusher stable ID changed");
    require(defaults.bit_depth == 16 && defaults.sample_rate_divisor == 1 &&
                defaults.mix == 1.0F &&
                crusher.effective_sample_rate_hz() == 48000.0F,
            "project-authored bit crusher defaults changed");
    require(crusher.memory_bytes() == sizeof(crusher),
            "bit crusher memory report is inaccurate");

    auto parameters = defaults;
    parameters.bit_depth = 0;
    parameters.sample_rate_divisor = 0;
    parameters.mix = -2.0F;
    crusher.set_parameters(parameters);
    require(crusher.parameters().bit_depth == 2 &&
                crusher.parameters().sample_rate_divisor == 1 &&
                crusher.parameters().mix == 0.0F,
            "lower project-authored bounds were not enforced");

    parameters.bit_depth = 255;
    parameters.sample_rate_divisor = 65535;
    parameters.mix = 2.0F;
    crusher.set_parameters(parameters);
    require(crusher.parameters().bit_depth == 24 &&
                crusher.parameters().sample_rate_divisor == 128 &&
                crusher.parameters().mix == 1.0F &&
                crusher.effective_sample_rate_hz() == 375.0F,
            "upper project-authored bounds were not enforced");

    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    crusher.set_parameters(parameters);
    require(crusher.parameters().mix == 1.0F,
            "non-finite mix did not retain the last valid setting");
    require(crusher.memory_bytes() == sizeof(crusher),
            "bit crusher storage grew after parameter changes");
}

void test_bit_depth_quantization_and_mix() {
    tabletfx::BitCrusherParameters parameters;
    parameters.bit_depth = 3;
    parameters.sample_rate_divisor = 1;
    parameters.mix = 1.0F;
    tabletfx::BitCrusher crusher(48000.0F, 1, parameters);
    std::array<float, 6> audio{
        0.30F, 0.62F, -0.30F, -0.62F, 1.5F, -1.5F};
    crusher.process(audio, 1);
    const std::array<float, 6> expected{
        0.25F, 0.5F, -0.25F, -0.5F, 0.75F, -1.0F};
    require(audio == expected,
            "bit-depth reduction did not quantize and clamp to the expected levels");

    parameters.mix = 0.0F;
    tabletfx::BitCrusher dry(48000.0F, 1, parameters);
    std::array<float, 4> unchanged{0.13F, -0.37F, 1.25F, -1.5F};
    const auto original = unchanged;
    dry.process(unchanged, 1);
    require(unchanged == original,
            "zero mix did not preserve the dry signal exactly");

    parameters.mix = 0.5F;
    tabletfx::BitCrusher blended(48000.0F, 1, parameters);
    std::array<float, 1> half_wet{0.30F};
    blended.process(half_wet, 1);
    require(std::abs(half_wet[0] - 0.275F) < 1.0e-6F,
            "mix did not blend dry input with the quantized signal");
}

void test_sample_rate_reduction_and_stereo_isolation() {
    tabletfx::BitCrusherParameters parameters;
    parameters.bit_depth = 24;
    parameters.sample_rate_divisor = 4;
    parameters.mix = 1.0F;
    tabletfx::BitCrusher mono(48000.0F, 1, parameters);
    std::array<float, 8> mono_audio{0.1F, 0.2F, 0.3F, 0.4F,
                                   0.5F, 0.6F, 0.7F, 0.8F};
    mono.process(mono_audio, 1);
    const std::array<float, 8> expected_mono{0.1F, 0.1F, 0.1F, 0.1F,
                                             0.5F, 0.5F, 0.5F, 0.5F};
    for (std::size_t index = 0; index < mono_audio.size(); ++index) {
        require(std::abs(mono_audio[index] - expected_mono[index]) < 2.0e-7F,
                "sample-rate reduction did not hold one sample per division");
    }
    require(mono.effective_sample_rate_hz() == 12000.0F,
            "effective sample rate does not reflect the integer divisor");

    tabletfx::BitCrusher stereo(48000.0F, 2, parameters);
    std::array<float, 16> stereo_audio{};
    for (std::size_t frame = 0; frame < stereo_audio.size() / 2U; ++frame) {
        stereo_audio[frame * 2U] = 0.1F * static_cast<float>(frame + 1U);
        stereo_audio[frame * 2U + 1U] = -0.05F * static_cast<float>(frame + 1U);
    }
    stereo.process(stereo_audio, 2);
    for (std::size_t frame = 0; frame < stereo_audio.size() / 2U; frame += 4U) {
        const float left = stereo_audio[frame * 2U];
        const float right = stereo_audio[frame * 2U + 1U];
        require(std::abs(left + 2.0F * right) < 2.0e-7F,
                "left/right sample-and-hold state leaked across channels");
    }
}

void test_variable_blocks_reset_and_channel_validation() {
    tabletfx::BitCrusherParameters parameters;
    parameters.bit_depth = 9;
    parameters.sample_rate_divisor = 7;
    parameters.mix = 0.83F;
    tabletfx::BitCrusher whole(44100.0F, 2, parameters);
    tabletfx::BitCrusher chunked(44100.0F, 2, parameters);
    std::vector<float> expected(4096U);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        expected[index] = 0.9F * std::sin(static_cast<float>(index) * 0.019F);
    }
    auto actual = expected;
    whole.process(expected, 2);

    const std::array<std::size_t, 5> block_frames{1U, 53U, 127U, 211U, 17U};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < actual.size() / 2U) {
        const std::size_t frames = std::min(
            block_frames[block % block_frames.size()], actual.size() / 2U - offset);
        chunked.process(std::span<float>(actual).subspan(offset * 2U, frames * 2U), 2);
        offset += frames;
        ++block;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(std::abs(expected[index] - actual[index]) < 1.0e-7F,
                "output changed across variable block partitions");
    }

    whole.reset();
    chunked.reset();
    std::array<float, 32> reset_a{};
    reset_a.fill(0.375F);
    auto reset_b = reset_a;
    whole.process(reset_a, 2);
    chunked.process(reset_b, 2);
    require(reset_a == reset_b,
            "reset did not restore deterministic sample-hold state");

    std::array<float, 3> mismatch{0.25F, -0.5F, 0.75F};
    const auto before = mismatch;
    whole.process(mismatch, 1);
    require(mismatch == before,
            "mismatched channel layout should leave audio unchanged");
}

void test_non_finite_recovery_and_no_process_allocations() {
    tabletfx::BitCrusherParameters parameters;
    parameters.bit_depth = 8;
    parameters.sample_rate_divisor = 3;
    tabletfx::BitCrusher crusher(48000.0F, 2, parameters);

    std::array<float, 10> invalid{
        0.5F, -0.25F,
        std::numeric_limits<float>::quiet_NaN(), 0.4F,
        0.5F, std::numeric_limits<float>::infinity(),
        0.6F, -0.25F,
        0.7F, -0.25F,
    };
    crusher.process(invalid, 2);
    require(invalid[2] == 0.0F && invalid[5] == 0.0F &&
                std::all_of(invalid.begin(), invalid.end(),
                            [](float sample) { return std::isfinite(sample); }),
            "non-finite samples did not recover to finite output");

    std::array<float, 512> block{};
    block.fill(0.25F);
    crusher.reset();
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        crusher.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before,
            "bit crusher allocated memory during process()");
    require(crusher.memory_bytes() == sizeof(crusher),
            "bit crusher memory changed during processing");
    require(std::all_of(block.begin(), block.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "bit crusher produced non-finite output");
}

void test_invalid_construction() {
    bool bad_rate = false;
    try {
        tabletfx::BitCrusher invalid(100.0F, 1);
    } catch (const std::invalid_argument&) {
        bad_rate = true;
    }
    require(bad_rate, "bit crusher accepted an unsupported sample rate");

    bool bad_channels = false;
    try {
        tabletfx::BitCrusher invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        bad_channels = true;
    }
    require(bad_channels, "bit crusher accepted more than stereo channels");
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
        test_defaults_bounds_and_fixed_memory();
        test_bit_depth_quantization_and_mix();
        test_sample_rate_reduction_and_stereo_isolation();
        test_variable_blocks_reset_and_channel_validation();
        test_non_finite_recovery_and_no_process_allocations();
        test_invalid_construction();
        std::cout << "All TabletFX bit crusher tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
