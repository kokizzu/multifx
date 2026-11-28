#include "tabletfx/effects/essex_vibe.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

std::atomic<std::size_t> allocation_count{0U};
std::atomic<bool> count_allocations{false};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <std::size_t Size>
float rms(const std::array<float, Size>& samples) {
    double sum = 0.0;
    for (const float sample : samples) {
        sum += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(Size)));
}

template <std::size_t Size>
bool all_finite(const std::array<float, Size>& samples) {
    return std::all_of(samples.begin(), samples.end(),
                       [](float value) { return std::isfinite(value); });
}

std::array<float, 512U> make_tone(float frequency_hz = 440.0F) {
    std::array<float, 512U> samples{};
    constexpr float sample_rate = 48000.0F;
    constexpr float pi = 3.14159265358979323846F;
    for (std::size_t index = 0U; index < samples.size(); ++index) {
        samples[index] = std::sin(2.0F * pi * frequency_hz *
                                  static_cast<float>(index) / sample_rate);
    }
    return samples;
}

void test_defaults_bounds_and_memory() {
    tabletfx::EssexVibe effect;
    const auto parameters = effect.parameters();
    require(effect.id() == "essex-vibe", "effect ID changed");
    require(parameters.rate_hz == 0.8F && parameters.depth == 0.65F &&
                parameters.trem == 0.0F && parameters.stages == 2U &&
                parameters.spread == 0.0F && parameters.level == 1.0F,
            "project-authored default controls changed unexpectedly");
    require(effect.memory_bytes() == sizeof(effect),
            "effect memory report is inaccurate");

    tabletfx::EssexVibeParameters extremes;
    extremes.rate_hz = 1000.0F;
    extremes.depth = -1.0F;
    extremes.trem = 4.0F;
    extremes.stages = 1U;
    extremes.spread = 3.0F;
    extremes.level = 99.0F;
    effect.configure(48000.0F, extremes);
    const auto bounded = effect.parameters();
    require(bounded.rate_hz == 12.0F && bounded.depth == 0.0F &&
                bounded.trem == 1.0F && bounded.stages == 1U &&
                bounded.spread == 1.0F && bounded.level == 2.0F,
            "project-authored bounds were not enforced");

    tabletfx::EssexVibeParameters invalid;
    invalid.rate_hz = std::numeric_limits<float>::quiet_NaN();
    invalid.depth = std::numeric_limits<float>::infinity();
    invalid.trem = std::numeric_limits<float>::quiet_NaN();
    invalid.stages = 99U;
    invalid.spread = std::numeric_limits<float>::infinity();
    invalid.level = std::numeric_limits<float>::quiet_NaN();
    effect.configure(std::numeric_limits<float>::quiet_NaN(), invalid);
    const auto safe = effect.parameters();
    require(effect.sample_rate() == 48000.0F && safe.rate_hz == 0.8F &&
                safe.depth == 0.65F && safe.trem == 0.0F &&
                safe.stages == 2U && safe.spread == 0.0F &&
                safe.level == 1.0F,
            "invalid configuration did not return to safe project defaults");
}

void test_silence_finite_output_and_channel_guards() {
    tabletfx::EssexVibeParameters parameters;
    parameters.depth = 1.0F;
    parameters.trem = 0.8F;
    parameters.spread = 1.0F;
    tabletfx::EssexVibe effect(48000.0F, parameters);

    std::array<float, 1024U> silence{};
    effect.process(silence, 2U);
    require(std::all_of(silence.begin(), silence.end(),
                        [](float value) { return value == 0.0F; }),
            "silence should remain silent");

    auto invalid_input = make_tone();
    invalid_input[7U] = std::numeric_limits<float>::quiet_NaN();
    invalid_input[11U] = std::numeric_limits<float>::infinity();
    effect.process(invalid_input, 1U);
    require(all_finite(invalid_input), "non-finite input escaped processing");

    std::array<float, 6U> unsupported{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F};
    effect.process(unsupported, 3U);
    require(unsupported == std::array<float, 6U>{1.0F, 2.0F, 3.0F,
                                                4.0F, 5.0F, 6.0F},
            "unsupported channel counts must remain untouched");
}

void test_stage_rate_depth_and_trem_controls() {
    const auto source = make_tone();
    auto render = [&source](tabletfx::EssexVibeParameters parameters) {
        tabletfx::EssexVibe effect(48000.0F, parameters);
        auto output = source;
        for (unsigned block = 0U; block < 40U; ++block) {
            effect.process(output, 1U);
        }
        return output;
    };

    tabletfx::EssexVibeParameters one_stage;
    one_stage.stages = 1U;
    const auto one = render(one_stage);
    auto two_stage = one_stage;
    two_stage.stages = 2U;
    const auto two = render(two_stage);
    auto different_rate = two_stage;
    different_rate.rate_hz = 3.0F;
    const auto faster = render(different_rate);
    auto static_sweep = two_stage;
    static_sweep.depth = 0.0F;
    const auto static_output = render(static_sweep);
    auto tremolo_only = static_sweep;
    tremolo_only.trem = 1.0F;
    const auto tremolo = render(tremolo_only);

    auto difference = [](const auto& left, const auto& right) {
        double sum = 0.0;
        for (std::size_t index = 0U; index < left.size(); ++index) {
            const double delta = static_cast<double>(left[index] - right[index]);
            sum += delta * delta;
        }
        return std::sqrt(sum / static_cast<double>(left.size()));
    };

    require(difference(one, two) > 1.0e-3,
            "one and two all-pass stage choices should differ");
    require(difference(two, faster) > 1.0e-3,
            "Rate should alter the modulation trajectory");
    require(difference(two, static_output) > 1.0e-3,
            "Depth should alter the filter sweep");
    require(difference(static_output, tremolo) > 1.0e-3,
            "Trem should alter the amplitude modulation");
    require(all_finite(two) && rms(two) > 0.05F && rms(two) < 2.0F,
            "modulated output should remain finite and bounded");
}

void test_level_and_zero_latency_response() {
    tabletfx::EssexVibeParameters parameters;
    parameters.depth = 0.0F;
    parameters.trem = 0.0F;
    parameters.stages = 1U;
    tabletfx::EssexVibe unity(48000.0F, parameters);
    tabletfx::EssexVibeParameters doubled_parameters = parameters;
    doubled_parameters.level = 2.0F;
    tabletfx::EssexVibe doubled(48000.0F, doubled_parameters);

    std::array<float, 64U> impulse{};
    impulse[0U] = 1.0F;
    auto doubled_impulse = impulse;
    unity.process(impulse, 1U);
    doubled.process(doubled_impulse, 1U);
    require(std::abs(impulse[0U]) > 1.0e-3F,
            "first output frame should respond immediately without fixed delay");
    for (std::size_t index = 0U; index < impulse.size(); ++index) {
        require(std::abs(doubled_impulse[index] - 2.0F * impulse[index]) <
                    1.0e-5F,
                "Level should scale the processed output");
    }

    std::array<float, 8U> bypass_input{0.1F, -0.2F, 0.3F, -0.4F,
                                       0.5F, -0.6F, 0.7F, -0.8F};
    const auto original = bypass_input;
    unity.process_bypassed(bypass_input, 2U);
    require(bypass_input == original,
            "default bypass processing should remain transparent");
}

void test_stereo_spread_and_independent_filter_states() {
    std::array<float, 1024U> same_input{};
    for (std::size_t frame = 0U; frame < same_input.size() / 2U; ++frame) {
        const float sample = std::sin(0.071F * static_cast<float>(frame));
        same_input[frame * 2U] = sample;
        same_input[frame * 2U + 1U] = sample;
    }

    tabletfx::EssexVibeParameters mono_spread;
    mono_spread.depth = 1.0F;
    mono_spread.trem = 0.5F;
    mono_spread.spread = 0.0F;
    tabletfx::EssexVibe centered(48000.0F, mono_spread);
    auto centered_output = same_input;
    centered.process(centered_output, 2U);
    for (std::size_t frame = 0U; frame < centered_output.size() / 2U; ++frame) {
        require(std::abs(centered_output[frame * 2U] -
                         centered_output[frame * 2U + 1U]) < 1.0e-6F,
                "zero Spread should preserve centered stereo equality");
    }

    mono_spread.spread = 1.0F;
    tabletfx::EssexVibe wide(48000.0F, mono_spread);
    auto wide_output = same_input;
    wide.process(wide_output, 2U);
    double separation = 0.0;
    for (std::size_t frame = 0U; frame < wide_output.size() / 2U; ++frame) {
        const double delta = static_cast<double>(wide_output[frame * 2U] -
                                                 wide_output[frame * 2U + 1U]);
        separation += delta * delta;
    }
    require(std::sqrt(separation / static_cast<double>(wide_output.size() / 2U)) >
                1.0e-3,
            "full stereo Spread should produce distinct left/right motion");
    require(all_finite(wide_output), "stereo output must remain finite");
}

void test_process_has_no_heap_allocation() {
    tabletfx::EssexVibe effect;
    auto samples = make_tone();
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    effect.process(samples, 1U);
    count_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "process() allocated memory");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) {
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
        test_silence_finite_output_and_channel_guards();
        test_stage_rate_depth_and_trem_controls();
        test_level_and_zero_latency_response();
        test_stereo_spread_and_independent_filter_states();
        test_process_has_no_heap_allocation();
        std::cout << "Essex Vibe DSP tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Essex Vibe DSP test failed: " << error.what() << '\n';
        return 1;
    }
}
