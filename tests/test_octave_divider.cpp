#include "tabletfx/effects/octave_divider.hpp"

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
std::atomic<std::size_t> allocation_count{0U};
constexpr float kSampleRate = 48000.0F;
constexpr double kPi = 3.14159265358979323846;

void fill_sine(std::span<float> samples, double frequency_hz,
               double amplitude, std::uint16_t channels = 1U) {
    const std::size_t frames = samples.size() / channels;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const float value = static_cast<float>(amplitude * std::sin(
            2.0 * kPi * frequency_hz * static_cast<double>(frame) /
            static_cast<double>(kSampleRate)));
        for (std::uint16_t channel = 0U; channel < channels; ++channel) {
            samples[frame * channels + channel] = value;
        }
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

std::vector<float> render_voice(bool voice1) {
    constexpr std::size_t kFrames = 48000U;
    tabletfx::effects::OctaveDividerParameters parameters;
    parameters.voice1_level = voice1 ? 1.0F : 0.0F;
    parameters.voice2_level = voice1 ? 0.0F : 1.0F;
    parameters.mix = 1.0F;
    std::vector<float> samples(kFrames, 0.0F);
    fill_sine(samples, 220.0, 0.25);
    tabletfx::effects::OctaveDivider effect(kSampleRate, 1U, parameters);
    effect.process(samples, 1U);
    return samples;
}

void test_startup_controls_and_bounds() {
    using namespace tabletfx::effects;
    OctaveDivider effect;
    assert(effect.id() == "octave-divider");
    assert(effect.sample_rate() == kSampleRate);
    assert(effect.channels() == 1U);
    assert(effect.memory_bytes() == sizeof(effect));
    assert(effect.algorithmic_latency_frames() == 0U);
    assert(effect.algorithmic_latency_ms() == 0.0F);
    assert(effect.parameters().input_gain == 1.0F);
    assert(effect.parameters().voice1_level == 0.45F);
    assert(effect.parameters().voice2_level == 0.20F);
    assert(effect.parameters().mix == 0.65F);

    std::array<float, 32U> silence{};
    effect.process(silence, 1U);
    assert(std::all_of(silence.begin(), silence.end(), [](float value) {
        return value == 0.0F;
    }));

    OctaveDividerParameters parameters;
    parameters.input_gain = -1.0F;
    parameters.voice1_level = 2.0F;
    parameters.voice1_pan = -2.0F;
    parameters.voice2_level = std::numeric_limits<float>::quiet_NaN();
    parameters.voice2_pan = 2.0F;
    parameters.mix = -1.0F;
    parameters.output_level = std::numeric_limits<float>::infinity();
    effect.configure(parameters);
    assert(effect.parameters().input_gain == 0.0F);
    assert(effect.parameters().voice1_level == 1.0F);
    assert(effect.parameters().voice1_pan == -1.0F);
    assert(effect.parameters().voice2_level == 0.20F);
    assert(effect.parameters().voice2_pan == 1.0F);
    assert(effect.parameters().mix == 0.0F);
    assert(effect.parameters().output_level == 1.0F);

    bool rejected_rate = false;
    try {
        OctaveDivider invalid(7999.0F, 1U);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    assert(rejected_rate);

    bool rejected_channels = false;
    try {
        OctaveDivider invalid(kSampleRate, 3U);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    assert(rejected_channels);
}

void test_fixed_divider_voice_frequencies() {
    const auto voice1 = render_voice(true);
    const auto voice2 = render_voice(false);
    constexpr std::size_t kStart = 12000U;
    constexpr std::size_t kLength = 24000U;
    assert(amplitude_near(voice1, 110.0, kStart, kLength) > 0.35);
    assert(amplitude_near(voice2, 55.0, kStart, kLength) > 0.35);
    assert(amplitude_near(voice1, 220.0, kStart, kLength) < 0.08);
    assert(amplitude_near(voice2, 110.0, kStart, kLength) < 0.08);
}

void test_stereo_voice_pan_and_dry_path() {
    using namespace tabletfx::effects;
    constexpr std::size_t kFrames = 4096U;
    std::vector<float> stereo(kFrames * 2U, 0.0F);
    fill_sine(stereo, 220.0, 0.25, 2U);

    OctaveDividerParameters parameters;
    parameters.voice1_level = 1.0F;
    parameters.voice1_pan = -1.0F;
    parameters.voice2_level = 0.0F;
    parameters.mix = 1.0F;
    OctaveDivider left_effect(kSampleRate, 2U, parameters);
    left_effect.process(stereo, 2U);
    for (std::size_t frame = 0U; frame < kFrames; ++frame) {
        assert(stereo[frame * 2U + 1U] == 0.0F);
    }
    assert(std::any_of(stereo.begin(), stereo.end(), [](float value) {
        return std::abs(value) > 0.1F;
    }));

    parameters.voice1_pan = 1.0F;
    OctaveDivider right_effect(kSampleRate, 2U, parameters);
    std::fill(stereo.begin(), stereo.end(), 0.0F);
    fill_sine(stereo, 220.0, 0.25, 2U);
    right_effect.process(stereo, 2U);
    for (std::size_t frame = 0U; frame < kFrames; ++frame) {
        assert(stereo[frame * 2U] == 0.0F);
    }
    assert(std::any_of(stereo.begin() + 1, stereo.end(), [](float value) {
        return std::abs(value) > 0.1F;
    }));

    parameters.mix = 0.0F;
    parameters.input_gain = 1.0F;
    parameters.output_level = 1.0F;
    OctaveDivider dry_effect(kSampleRate, 2U, parameters);
    std::array<float, 8U> impulse{0.5F, -0.25F, 0.0F, 0.0F,
                                  0.0F, 0.0F, 0.0F, 0.0F};
    dry_effect.process(impulse, 2U);
    assert(impulse[0] == 0.5F && impulse[1] == -0.25F);
}

void test_reset_and_irregular_block_invariance() {
    using namespace tabletfx::effects;
    constexpr std::size_t kFrames = 5003U;
    std::vector<float> source(kFrames, 0.0F);
    fill_sine(source, 110.0, 0.3);

    auto after_reset = source;
    OctaveDivider reused(kSampleRate, 1U);
    std::array<float, 512U> warmup{};
    fill_sine(warmup, 330.0, 0.4);
    reused.process(warmup, 1U);
    reused.reset();
    reused.process(after_reset, 1U);

    auto fresh_output = source;
    OctaveDivider fresh(kSampleRate, 1U);
    fresh.process(fresh_output, 1U);
    assert(after_reset == fresh_output);

    auto whole = source;
    OctaveDivider whole_effect(kSampleRate, 1U);
    whole_effect.process(whole, 1U);

    auto partitioned = source;
    OctaveDivider partitioned_effect(kSampleRate, 1U);
    constexpr std::array<std::size_t, 9U> kChunks{
        1U, 3U, 127U, 2U, 257U, 64U, 511U, 17U, 1021U};
    std::size_t frame = 0U;
    std::size_t chunk_index = 0U;
    while (frame < kFrames) {
        const std::size_t chunk = std::min(
            kChunks[chunk_index % kChunks.size()], kFrames - frame);
        partitioned_effect.process(
            std::span<float>(partitioned.data() + frame, chunk), 1U);
        frame += chunk;
        ++chunk_index;
    }
    assert(partitioned == whole);
}

void test_finite_recovery_no_allocation_and_zero_latency() {
    using namespace tabletfx::effects;
    OctaveDivider effect;
    std::array<float, 16U> samples{
        1.0F, -1.0F, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 1.0e30F, -1.0e30F,
        0.0F, 0.5F, -0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
    effect.process(samples, 1U);
    assert(std::all_of(samples.begin(), samples.end(), [](float value) {
        return std::isfinite(value) && std::abs(value) <= 8.0F;
    }));

    OctaveDividerParameters parameters;
    parameters.mix = 0.25F;
    OctaveDivider immediate(kSampleRate, 1U, parameters);
    std::array<float, 8U> impulse{1.0F, 0.0F, 0.0F, 0.0F,
                                  0.0F, 0.0F, 0.0F, 0.0F};
    immediate.process(impulse, 1U);
    assert(immediate.algorithmic_latency_frames() == 0U);
    assert(impulse[0] > 0.70F);

    std::array<float, 2048U> allocation_probe{};
    fill_sine(allocation_probe, 110.0, 0.2);
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    effect.process(allocation_probe, 1U);
    count_allocations.store(false, std::memory_order_release);
    assert(allocation_count.load(std::memory_order_relaxed) == 0U);
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
    test_startup_controls_and_bounds();
    test_fixed_divider_voice_frequencies();
    test_stereo_voice_pan_and_dry_path();
    test_reset_and_irregular_block_invariance();
    test_finite_recovery_no_allocation_and_zero_latency();
    return 0;
}
