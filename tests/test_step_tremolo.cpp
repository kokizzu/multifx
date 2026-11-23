#include "tabletfx/effects/step_tremolo.hpp"

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

tabletfx::StepTremoloParameters pattern_parameters() {
    tabletfx::StepTremoloParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.step_count = 4;
    parameters.levels.fill(1.0F);
    parameters.levels[0] = 1.0F;
    parameters.levels[1] = 0.75F;
    parameters.levels[2] = 0.5F;
    parameters.levels[3] = 0.25F;
    parameters.transition_ms = 0.0F;
    return parameters;
}

void test_defaults_bounds_and_memory() {
    tabletfx::StepTremolo step;
    const auto parameters = step.parameters();
    require(step.id() == "step-tremolo", "step tremolo stable ID changed");
    require(parameters.rate_hz == 4.0F && parameters.depth == 0.75F &&
                parameters.step_count == 8 && parameters.transition_ms == 4.0F,
            "project-authored step tremolo defaults changed");
    require(step.memory_bytes() == sizeof(step),
            "step tremolo memory report is inaccurate");

    step.set_rate_hz(-1.0F);
    require(step.parameters().rate_hz == 0.05F,
            "minimum free-running rate was not enforced");
    step.set_rate_hz(50.0F);
    require(step.parameters().rate_hz == 20.0F,
            "maximum free-running rate was not enforced");
    step.set_rate_hz(std::numeric_limits<float>::quiet_NaN());
    require(step.parameters().rate_hz == 20.0F,
            "non-finite rate did not preserve the last valid value");

    step.set_depth(-1.0F);
    require(step.parameters().depth == 0.0F,
            "depth below zero was not clamped");
    step.set_depth(2.0F);
    require(step.parameters().depth == 1.0F,
            "depth above one was not clamped");
    step.set_depth(std::numeric_limits<float>::infinity());
    require(step.parameters().depth == 1.0F,
            "non-finite depth did not preserve the last valid value");

    step.set_step_count(0);
    require(step.parameters().step_count == 1,
            "step count below one was not clamped");
    step.set_step_count(255);
    require(step.parameters().step_count == 16,
            "step count above the fixed pattern length was not clamped");
    step.set_step_level(16, 0.1F);
    require(step.parameters().levels[15] == 0.75F,
            "out-of-range step index changed the pattern");
    step.set_step_level(15, -1.0F);
    require(step.parameters().levels[15] == 0.0F,
            "step level below zero was not clamped");
    step.set_step_level(15, std::numeric_limits<float>::quiet_NaN());
    require(step.parameters().levels[15] == 0.0F,
            "non-finite step level did not preserve the previous value");
    step.set_transition_ms(1000.0F);
    require(step.parameters().transition_ms == 100.0F,
            "transition safety maximum was not enforced");
}

void test_step_pattern_and_depth_blend() {
    const auto parameters = pattern_parameters();
    tabletfx::StepTremolo step(8000.0F, parameters);
    std::vector<float> audio(8000, 1.0F);
    step.process(audio, 1);
    require(std::abs(audio[100] - 1.0F) < 1.0e-6F &&
                std::abs(audio[2100] - 0.75F) < 1.0e-5F &&
                std::abs(audio[4100] - 0.5F) < 1.0e-5F &&
                std::abs(audio[6100] - 0.25F) < 1.0e-5F,
            "four-step pattern did not apply the selected levels");

    auto blended_parameters = pattern_parameters();
    blended_parameters.step_count = 2;
    blended_parameters.levels[0] = 0.0F;
    blended_parameters.levels[1] = 1.0F;
    blended_parameters.depth = 0.5F;
    tabletfx::StepTremolo blended(8000.0F, blended_parameters);
    std::vector<float> blended_audio(8000, 1.0F);
    blended.process(blended_audio, 1);
    require(std::abs(blended_audio[100] - 0.5F) < 1.0e-6F &&
                std::abs(blended_audio[4100] - 1.0F) < 1.0e-6F,
            "depth did not blend a low step back toward unity");
}

void test_smooth_boundaries_and_nonzero_default_levels() {
    tabletfx::StepTremoloParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.step_count = 2;
    parameters.levels.fill(1.0F);
    parameters.levels[0] = 0.2F;
    parameters.levels[1] = 1.0F;
    parameters.transition_ms = 10.0F;
    tabletfx::StepTremolo smooth(8000.0F, parameters);
    std::vector<float> audio(4300, 1.0F);
    smooth.process(audio, 1);

    float largest_adjacent_change = 0.0F;
    for (std::size_t i = 1; i < audio.size(); ++i) {
        largest_adjacent_change = std::max(
            largest_adjacent_change, std::abs(audio[i] - audio[i - 1]));
    }
    require(largest_adjacent_change < 0.02F,
            "step transition was not smoothed across its configured ramp");
    require(std::abs(audio[100] - 0.2F) < 1.0e-6F &&
                std::abs(audio[4200] - 1.0F) < 1.0e-6F,
            "smooth steps did not settle at their requested levels");

    tabletfx::StepTremolo defaults;
    std::array<float, 2> first_frame{1.0F, 1.0F};
    defaults.process(first_frame, 2);
    require(first_frame[0] > 0.0F && first_frame[1] > 0.0F &&
                first_frame[0] == first_frame[1],
            "default pattern should be linked stereo and avoid hard-gate silence");
}

void test_variable_blocks_stereo_link_and_reset() {
    auto parameters = pattern_parameters();
    parameters.rate_hz = 7.3F;
    parameters.depth = 0.82F;
    parameters.step_count = 7;
    parameters.transition_ms = 1.7F;

    tabletfx::StepTremolo whole(44100.0F, parameters);
    tabletfx::StepTremolo chunked(44100.0F, parameters);
    std::vector<float> expected(8194);
    for (std::size_t i = 0; i < expected.size(); i += 2) {
        const float sample = 0.6F * std::sin(static_cast<float>(i) * 0.013F);
        expected[i] = sample;
        expected[i + 1] = sample * 0.5F;
    }
    auto actual = expected;
    whole.process(expected, 2);

    const std::array<std::size_t, 5> block_sizes{1, 127, 53, 300, 17};
    std::size_t frame = 0;
    std::size_t block = 0;
    while (frame < actual.size() / 2) {
        const std::size_t frames = std::min(block_sizes[block % block_sizes.size()],
                                            actual.size() / 2 - frame);
        chunked.process(std::span<float>(actual).subspan(frame * 2, frames * 2), 2);
        frame += frames;
        ++block;
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        require(std::abs(expected[i] - actual[i]) < 1.0e-7F,
                "variable block sizes changed step phase or output");
    }

    for (std::size_t i = 0; i < actual.size(); i += 2) {
        require(std::abs(actual[i + 1] * 2.0F - actual[i]) < 1.0e-6F,
                "stereo channels did not share the same amplitude envelope");
    }

    whole.reset();
    chunked.reset();
    std::array<float, 128> whole_reset{};
    std::array<float, 128> chunked_reset{};
    whole_reset.fill(1.0F);
    chunked_reset.fill(1.0F);
    whole.process(whole_reset, 2);
    chunked.process(chunked_reset, 2);
    require(whole_reset == chunked_reset && whole.current_step() == 0 &&
                chunked.current_step() == 0,
            "reset did not restore the deterministic first step");
}

void test_finite_output_zero_channels_and_no_process_allocation() {
    tabletfx::StepTremolo step;
    std::array<float, 6> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.25F, -0.5F, 0.75F,
    };
    step.process(invalid, 1);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F && invalid[2] == 0.0F,
            "non-finite input did not produce finite silence");

    std::array<float, 4> untouched{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before = untouched;
    step.process(untouched, 0);
    require(untouched == before, "zero-channel processing changed the buffer");

    std::array<float, 512> block{};
    block.fill(0.5F);
    step.reset();
    const std::size_t allocations_before =
        allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        step.process(block, 2);
    }
    const std::size_t allocations_after =
        allocation_count.load(std::memory_order_relaxed);
    require(allocations_after == allocations_before,
            "step tremolo allocated memory during process()");
    for (float sample : block) {
        require(std::isfinite(sample), "step tremolo emitted non-finite audio");
    }
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
        test_defaults_bounds_and_memory();
        test_step_pattern_and_depth_blend();
        test_smooth_boundaries_and_nonzero_default_levels();
        test_variable_blocks_stereo_link_and_reset();
        test_finite_output_zero_channels_and_no_process_allocation();
        std::cout << "All TabletFX step tremolo tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
