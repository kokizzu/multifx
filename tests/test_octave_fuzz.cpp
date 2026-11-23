#include "tabletfx/effects/octave_fuzz.hpp"

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

std::vector<float> render_sine(tabletfx::effects::OctaveFuzzParameters params) {
    constexpr std::size_t frames = 72000U;
    std::vector<float> output(frames, 0.0F);
    fill_sine(output, 220.0, 0.25);
    tabletfx::effects::OctaveFuzz effect(kSampleRate, 1U, params);
    effect.process(output, 1U);
    return output;
}

void test_defaults_bounds_latency_and_unsupported_layout() {
    using namespace tabletfx::effects;
    OctaveFuzz effect;
    assert(effect.id() == "octave-fuzz");
    assert(effect.parameters().boost == 0.55F);
    assert(effect.parameters().volume == 0.75F);
    assert(effect.parameters().octave_switch);
    assert(std::abs(effect.algorithmic_latency_ms() -
                    1000.0F * 256.0F / kSampleRate) < 0.01F);
    assert(effect.memory_bytes() > sizeof(effect));

    auto parameters = effect.parameters();
    parameters.boost = -1.0F;
    parameters.volume = 9.0F;
    effect.configure(parameters);
    assert(effect.parameters().boost == 0.0F);
    assert(effect.parameters().volume == 2.0F);

    std::array<float, 64U> mismatch{};
    mismatch.fill(0.25F);
    const auto before = mismatch;
    effect.process(mismatch, 2U);
    assert(mismatch == before);

    bool rejected_channels = false;
    try {
        OctaveFuzz unsupported(kSampleRate, 3U);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    assert(rejected_channels);

    bool rejected_rate = false;
    try {
        OctaveFuzz unsupported(7999.0F, 1U);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    assert(rejected_rate);
}

void test_octave_switch_adds_upper_octave_and_boost_changes_tone() {
    using namespace tabletfx::effects;
    OctaveFuzzParameters parameters;
    parameters.boost = 0.3F;
    parameters.volume = 0.8F;
    parameters.octave_switch = false;
    const auto base = render_sine(parameters);
    const double base_fundamental = amplitude_near(base, 220.0, 36000U, 24000U);
    const double base_upper = amplitude_near(base, 440.0, 36000U, 24000U);

    parameters.octave_switch = true;
    const auto octave = render_sine(parameters);
    const double octave_upper = amplitude_near(octave, 440.0, 36000U, 24000U);
    assert(octave_upper > base_upper * 1.5);
    assert(base_fundamental > 0.02);

    parameters.octave_switch = false;
    parameters.boost = 0.9F;
    const auto high_boost = render_sine(parameters);
    double difference = 0.0;
    for (std::size_t index = 0U; index < base.size(); ++index) {
        const double delta = static_cast<double>(high_boost[index]) - base[index];
        difference += delta * delta;
    }
    assert(std::sqrt(difference / static_cast<double>(base.size())) > 0.01);
}

void test_volume_stereo_isolation_and_reset() {
    using namespace tabletfx::effects;
    constexpr std::size_t frames = 8192U;
    std::vector<float> left(frames, 0.0F);
    std::vector<float> right(frames, 0.0F);
    fill_sine(left, 220.0, 0.25);
    fill_sine(right, 220.0, 0.25);
    OctaveFuzzParameters quiet_params;
    quiet_params.octave_switch = false;
    quiet_params.volume = 0.5F;
    OctaveFuzzParameters loud_params = quiet_params;
    loud_params.volume = 1.0F;
    OctaveFuzz quiet(kSampleRate, 1U, quiet_params);
    OctaveFuzz loud(kSampleRate, 1U, loud_params);
    quiet.process(left, 1U);
    loud.process(right, 1U);
    for (std::size_t frame = 256U; frame < frames; ++frame) {
        assert(std::abs(right[frame] - 2.0F * left[frame]) < 1.0e-5F);
    }

    std::vector<float> stereo(frames * 2U, 0.0F);
    fill_sine(stereo, 220.0, 0.25, 2U, 0U);
    OctaveFuzz stereo_effect(kSampleRate, 2U, quiet_params);
    stereo_effect.process(stereo, 2U);
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        assert(stereo[frame * 2U + 1U] == 0.0F);
    }

    stereo_effect.reset();
    std::array<float, 256U> zeros{};
    stereo_effect.process(zeros, 2U);
    assert(std::all_of(zeros.begin(), zeros.end(), [](float value) {
        return std::isfinite(value);
    }));
}

void test_non_finite_recovery_and_no_process_allocations() {
    using namespace tabletfx::effects;
    OctaveFuzz effect;
    std::array<float, 512U> block{};
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
    test_defaults_bounds_latency_and_unsupported_layout();
    test_octave_switch_adds_upper_octave_and_boost_changes_tone();
    test_volume_stereo_isolation_and_reset();
    test_non_finite_recovery_and_no_process_allocations();
}
