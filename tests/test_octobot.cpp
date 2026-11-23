#include "tabletfx/effects/octobot.hpp"

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
#include <stdexcept>
#include <vector>

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};
constexpr float kSampleRate = 48000.0F;
constexpr double kPi = 3.14159265358979323846;

void fill_sine(std::span<float> samples, double frequency_hz,
               double amplitude, std::uint16_t channels = 1U,
               std::uint16_t selected_channel = 0U) {
    const std::size_t frames = samples.size() / channels;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const float value = static_cast<float>(amplitude * std::sin(
            2.0 * kPi * frequency_hz * static_cast<double>(frame) /
            static_cast<double>(kSampleRate)));
        samples[frame * channels + selected_channel] = value;
    }
}

double amplitude_near(std::span<const float> samples, double frequency_hz,
                      std::size_t start_frame, std::size_t length,
                      std::uint16_t channel = 0U,
                      std::uint16_t channels = 1U) {
    double cosine = 0.0;
    double sine = 0.0;
    double window_sum = 0.0;
    for (std::size_t index = 0U; index < length; ++index) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * kPi * static_cast<double>(index) /
            static_cast<double>(length - 1U));
        const double angle = 2.0 * kPi * frequency_hz *
                             static_cast<double>(start_frame + index) /
                             static_cast<double>(kSampleRate);
        const double value = static_cast<double>(
            samples[(start_frame + index) * channels + channel]) * window;
        cosine += value * std::cos(angle);
        sine += value * std::sin(angle);
        window_sum += window;
    }
    return 2.0 * std::hypot(cosine, sine) / window_sum;
}

void test_octobot_defaults_bounds_and_invalid_values() {
    using namespace tabletfx::effects;
    Octobot effect;
    assert(effect.id() == "octobot");
    assert(effect.parameters().direct == 0.65F);
    assert(effect.parameters().down == 0.20F);
    assert(effect.parameters().sizzle == 0.25F);
    assert(std::abs(effect.algorithmic_latency_ms() -
                    1000.0F * 256.0F / kSampleRate) < 0.01F);
    assert(effect.memory_bytes() > sizeof(effect));

    auto parameters = effect.parameters();
    parameters.direct = -1.0F;
    parameters.down = 2.0F;
    parameters.sizzle = std::numeric_limits<float>::quiet_NaN();
    effect.configure(parameters);
    assert(effect.parameters().direct == 0.0F);
    assert(effect.parameters().down == 1.0F);
    assert(effect.parameters().sizzle == 0.25F);

    bool rejected_channels = false;
    try {
        Octobot unsupported(kSampleRate, 3U);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    assert(rejected_channels);

    bool rejected_rate = false;
    try {
        Octobot unsupported(7999.0F, 1U);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    assert(rejected_rate);
}

void test_octobot_direct_branch_has_aligned_fixed_delay() {
    using namespace tabletfx::effects;
    OctobotParameters parameters;
    parameters.direct = 1.0F;
    parameters.down = 0.0F;
    parameters.sizzle = 0.0F;
    Octobot effect(kSampleRate, 1U, parameters);
    std::vector<float> audio(4096U, 0.0F);
    audio[1000U] = 0.5F;
    effect.process(audio, 1U);

    assert(audio[1000U + 255U] == 0.0F);
    assert(std::abs(audio[1000U + 256U] - 0.5F) < 1.0e-7F);
    assert(audio[1000U + 257U] == 0.0F);
}

void test_octobot_down_and_sizzle_branches_are_distinct() {
    using namespace tabletfx::effects;
    constexpr std::size_t frames = 72000U;
    constexpr std::size_t analysis_start = 36000U;
    constexpr std::size_t analysis_length = 24000U;
    std::vector<float> input(frames, 0.0F);
    fill_sine(input, 220.0, 0.25);

    auto down_parameters = OctobotParameters{};
    down_parameters.direct = 0.0F;
    down_parameters.down = 1.0F;
    down_parameters.sizzle = 0.0F;
    auto down = input;
    Octobot down_effect(kSampleRate, 1U, down_parameters);
    down_effect.process(down, 1U);
    const double sub_octave = amplitude_near(
        down, 110.0, analysis_start, analysis_length);
    const double unshifted = amplitude_near(
        down, 220.0, analysis_start, analysis_length);
    assert(sub_octave > unshifted * 1.5);

    auto sizzle_parameters = OctobotParameters{};
    sizzle_parameters.direct = 0.0F;
    sizzle_parameters.down = 0.0F;
    sizzle_parameters.sizzle = 1.0F;
    auto sizzle = input;
    Octobot sizzle_effect(kSampleRate, 1U, sizzle_parameters);
    sizzle_effect.process(sizzle, 1U);
    const double upper_octave = amplitude_near(
        sizzle, 440.0, analysis_start, analysis_length);
    assert(upper_octave > 0.1);
}

void test_octobot_stereo_channels_do_not_leak() {
    using namespace tabletfx::effects;
    constexpr std::size_t frames = 8192U;
    std::vector<float> stereo(frames * 2U, 0.0F);
    fill_sine(stereo, 220.0, 0.25, 2U, 0U);
    OctobotParameters parameters;
    parameters.direct = 1.0F;
    parameters.down = 0.0F;
    parameters.sizzle = 0.0F;
    Octobot effect(kSampleRate, 2U, parameters);
    effect.process(stereo, 2U);
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        assert(stereo[frame * 2U + 1U] == 0.0F);
    }

    std::array<float, 64U> mismatch{};
    mismatch.fill(0.25F);
    const auto before = mismatch;
    effect.process(mismatch, 1U);
    assert(mismatch == before);
}

void test_octobot_non_finite_recovery_and_no_process_allocations() {
    using namespace tabletfx::effects;
    Octobot effect;
    std::array<float, 256U> block{};
    block[4U] = std::numeric_limits<float>::quiet_NaN();
    block[9U] = std::numeric_limits<float>::infinity();
    block[13U] = -std::numeric_limits<float>::infinity();
    count_allocations.store(true, std::memory_order_release);
    effect.process(block, 1U);
    count_allocations.store(false, std::memory_order_release);
    assert(allocation_count.load(std::memory_order_relaxed) == 0U);
    assert(std::all_of(block.begin(), block.end(), [](float sample) {
        return std::isfinite(sample);
    }));
}

}  // namespace

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
    test_octobot_defaults_bounds_and_invalid_values();
    test_octobot_direct_branch_has_aligned_fixed_delay();
    test_octobot_down_and_sizzle_branches_are_distinct();
    test_octobot_stereo_channels_do_not_leak();
    test_octobot_non_finite_recovery_and_no_process_allocations();
}
