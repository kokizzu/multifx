#include "tabletfx/effects/notch_filter.hpp"

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

std::vector<float> make_sine(float sample_rate, float frequency,
                             std::size_t frames, std::uint16_t channels = 1,
                             std::uint16_t active_channel = 0) {
    std::vector<float> audio(frames * channels, 0.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float phase = 2.0F * 3.14159265358979323846F * frequency *
                            static_cast<float>(frame) / sample_rate;
        audio[frame * channels + active_channel] = 0.5F * std::sin(phase);
    }
    return audio;
}

float rms(std::span<const float> audio, std::uint16_t channels,
          std::uint16_t channel = 0) {
    const std::size_t frames = audio.size() / channels;
    const std::size_t begin = frames / 2U;
    double sum = 0.0;
    for (std::size_t frame = begin; frame < frames; ++frame) {
        const double sample = audio[frame * channels + channel];
        sum += sample * sample;
    }
    return frames == begin ? 0.0F
                           : static_cast<float>(std::sqrt(
                                 sum / static_cast<double>(frames - begin)));
}

float render_tone(float frequency_hz, float q, float mix = 1.0F) {
    constexpr float sample_rate = 48000.0F;
    std::vector<float> audio = make_sine(sample_rate, frequency_hz, 24000U);
    tabletfx::NotchFilterParameters parameters;
    parameters.frequency_hz = 1000.0F;
    parameters.q = q;
    parameters.mix = mix;
    tabletfx::NotchFilter filter(sample_rate, parameters);
    filter.process(audio, 1);
    return rms(audio, 1);
}

void test_defaults_bounds_and_fixed_memory() {
    tabletfx::NotchFilter filter;
    const auto defaults = filter.parameters();
    require(filter.id() == "notch-filter", "stable effect ID changed");
    require(defaults.frequency_hz == 1000.0F && defaults.q == 4.0F &&
                defaults.mix == 1.0F,
            "project-authored default controls changed");
    require(filter.memory_bytes() == sizeof(filter),
            "fixed-state memory report is inaccurate");

    tabletfx::NotchFilterParameters parameters;
    parameters.frequency_hz = 90000.0F;
    parameters.q = 100.0F;
    parameters.mix = -2.0F;
    tabletfx::NotchFilter bounded(8000.0F, parameters);
    auto sanitized = bounded.parameters();
    require(sanitized.frequency_hz == 3200.0F && sanitized.q == 12.0F &&
                sanitized.mix == 0.0F,
            "controls did not respect the configured upper bounds");

    parameters.frequency_hz = -100.0F;
    parameters.q = 0.0F;
    parameters.mix = 2.0F;
    bounded.set_parameters(parameters);
    sanitized = bounded.parameters();
    require(sanitized.frequency_hz == 20.0F && sanitized.q == 0.5F &&
                sanitized.mix == 1.0F,
            "controls did not respect the configured lower bounds");

    parameters.frequency_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.q = std::numeric_limits<float>::infinity();
    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    bounded.set_parameters(parameters);
    sanitized = bounded.parameters();
    require(sanitized.frequency_hz == 1000.0F && sanitized.q == 4.0F &&
                sanitized.mix == 1.0F,
            "non-finite controls did not use project defaults");

    parameters.frequency_hz = 90000.0F;
    tabletfx::NotchFilter low_rate(8000.0F, parameters);
    require(low_rate.parameters().frequency_hz == 3200.0F,
            "cutoff did not respect the sample-rate-aware upper bound");
    tabletfx::NotchFilter invalid_rate(1000.0F, parameters);
    require(invalid_rate.parameters().frequency_hz == 18000.0F,
            "invalid sample rate did not use the safe 48 kHz fallback");
}

void test_notch_response_and_q() {
    const float center = render_tone(1000.0F, 4.0F);
    const float away = render_tone(1800.0F, 4.0F);
    require(center < 0.002F,
            "center frequency was not substantially rejected");
    require(away > 0.25F,
            "frequencies away from the notch were not preserved");

    const float broad = render_tone(1150.0F, 0.5F);
    const float narrow = render_tone(1150.0F, 12.0F);
    require(narrow > broad,
            "higher Q did not make the notch narrower");
}

void test_mix_and_dry_path() {
    tabletfx::NotchFilterParameters parameters;
    parameters.frequency_hz = 1000.0F;
    parameters.q = 8.0F;
    parameters.mix = 0.0F;
    tabletfx::NotchFilter dry_filter(48000.0F, parameters);
    std::array<float, 10> dry{0.1F, -0.2F, 0.4F, -0.8F, 1.2F,
                              0.3F, -0.1F, 0.7F, -0.5F, 0.2F};
    const auto original = dry;
    dry_filter.process(dry, 1);
    require(dry == original, "zero mix changed the exact dry signal");

    const float half_mix = render_tone(1000.0F, 8.0F, 0.5F);
    const float full_mix = render_tone(1000.0F, 8.0F, 1.0F);
    require(half_mix > full_mix * 10.0F,
            "wet/dry mix did not retain the expected dry component");
}

void test_stereo_isolation_finite_recovery_and_reset() {
    constexpr float sample_rate = 48000.0F;
    tabletfx::NotchFilter filter(sample_rate);
    auto stereo = make_sine(sample_rate, 600.0F, 4096U, 2U, 1U);
    filter.process(stereo, 2);
    for (std::size_t frame = 0; frame < stereo.size() / 2U; ++frame) {
        require(stereo[frame * 2U] == 0.0F,
                "right-channel history leaked into the left channel");
    }
    require(rms(stereo, 2U, 1U) > 0.25F,
            "active right-channel signal was unexpectedly lost");

    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.1F,
        100000.0F, -100000.0F, 0.2F, -0.2F,
    };
    filter.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(), [](float value) {
                return std::isfinite(value);
            }),
            "invalid/extreme samples poisoned the filter state");

    tabletfx::NotchFilterParameters parameters;
    parameters.frequency_hz = 1700.0F;
    parameters.q = 2.5F;
    parameters.mix = 0.75F;
    filter.set_parameters(parameters);
    filter.reset();
    auto impulse_a = std::vector<float>(1200U, 0.0F);
    impulse_a[0] = 0.5F;
    auto impulse_b = impulse_a;
    filter.process(impulse_a, 1);
    require(filter.parameters().frequency_hz == 1700.0F &&
                filter.parameters().q == 2.5F && filter.parameters().mix == 0.75F,
            "reset/configuration test did not retain requested parameters");
    filter.reset();
    filter.process(impulse_b, 1);
    require(std::equal(impulse_a.begin(), impulse_a.end(), impulse_b.begin(),
                       [](float left, float right) {
                           return std::abs(left - right) < 1.0e-6F;
                       }),
            "reset did not clear the filter history");
}

void test_parameter_smoothing_is_block_invariant() {
    constexpr std::size_t frames = 1600U;
    tabletfx::NotchFilter one_block;
    tabletfx::NotchFilter split_blocks;
    tabletfx::NotchFilterParameters parameters;
    parameters.frequency_hz = 4500.0F;
    parameters.q = 1.25F;
    parameters.mix = 0.35F;
    one_block.set_parameters(parameters);
    split_blocks.set_parameters(parameters);

    auto whole = make_sine(48000.0F, 700.0F, frames);
    auto divided = whole;
    one_block.process(whole, 1);
    const std::array<std::size_t, 5> blocks{7U, 113U, 2U, 512U, 966U};
    std::size_t offset = 0;
    for (const std::size_t block_frames : blocks) {
        split_blocks.process(
            std::span<float>(divided.data() + offset, block_frames), 1);
        offset += block_frames;
    }
    require(offset == frames, "irregular block fixture does not cover all frames");
    for (std::size_t index = 0; index < frames; ++index) {
        require(std::abs(whole[index] - divided[index]) < 1.0e-7F,
                "parameter smoothing depends on block partition");
    }
}

void test_no_process_allocations() {
    tabletfx::NotchFilter filter;
    std::array<float, 512> audio{};
    audio.fill(0.25F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        filter.process(audio, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "Notch Filter allocated during process()");
    require(filter.memory_bytes() == sizeof(filter),
            "Notch Filter memory changed while processing");
    require(std::all_of(audio.begin(), audio.end(), [](float value) {
                return std::isfinite(value);
            }),
            "Notch Filter produced non-finite output");
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
        test_notch_response_and_q();
        test_mix_and_dry_path();
        test_stereo_isolation_finite_recovery_and_reset();
        test_parameter_smoothing_is_block_invariant();
        test_no_process_allocations();
        std::cout << "All TabletFX Notch Filter tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
