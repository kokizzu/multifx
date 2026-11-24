#include "tabletfx/effects/mutant_filter.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
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

double squared_difference(std::span<const float> a,
                          std::span<const float> b) {
    double sum = 0.0;
    for (std::size_t index = 0; index < a.size(); ++index) {
        const double difference = static_cast<double>(a[index]) - b[index];
        sum += difference * difference;
    }
    return sum;
}

void test_defaults_mode_outputs_and_zero_buffering() {
    tabletfx::EnvelopeWahParameters parameters;
    parameters.sensitivity = 1.0F;
    parameters.minimum_hz = 350.0F;
    parameters.maximum_hz = 2200.0F;
    parameters.attack_ms = 0.0F;
    parameters.release_ms = 0.0F;
    parameters.smoothing_ms = 0.0F;

    tabletfx::MutantFilter bandpass(48000.0F, 1U, parameters);
    require(bandpass.id() == "mutant-filter" &&
                bandpass.memory_bytes() == sizeof(bandpass),
            "Mutant Filter stable ID or memory report changed");
    require(bandpass.parameters().filter_mode ==
                tabletfx::EnvelopeWahFilterMode::Bandpass,
            "project default should remain band-pass");

    auto low_parameters = parameters;
    low_parameters.filter_mode = tabletfx::EnvelopeWahFilterMode::Lowpass;
    tabletfx::MutantFilter lowpass(48000.0F, 1U, low_parameters);
    auto high_parameters = parameters;
    high_parameters.filter_mode = tabletfx::EnvelopeWahFilterMode::Highpass;
    tabletfx::MutantFilter highpass(48000.0F, 1U, high_parameters);

    std::array<float, 2048> source{};
    for (std::size_t frame = 0; frame < source.size(); ++frame) {
        source[frame] = 0.45F * std::sin(
            2.0F * 3.14159265358979323846F * 1800.0F *
            static_cast<float>(frame) / 48000.0F);
    }
    source[0] = 0.5F;
    auto low_audio = source;
    auto band_audio = source;
    auto high_audio = source;
    lowpass.process(low_audio, 1U);
    bandpass.process(band_audio, 1U);
    highpass.process(high_audio, 1U);

    require(std::isfinite(low_audio[0]) && std::abs(low_audio[0]) > 1.0e-7F &&
                std::isfinite(band_audio[0]) && std::abs(band_audio[0]) > 1.0e-7F &&
                std::isfinite(high_audio[0]) && std::abs(high_audio[0]) > 1.0e-7F,
            "filter output must be produced from the current sample without a block delay");
    require(squared_difference(low_audio, band_audio) > 0.01 &&
                squared_difference(low_audio, high_audio) > 0.01 &&
                squared_difference(band_audio, high_audio) > 0.01,
            "low-pass, band-pass, and high-pass modes must produce distinct output");
    require(std::all_of(low_audio.begin(), low_audio.end(),
                        [](float sample) { return std::isfinite(sample); }) &&
                std::all_of(band_audio.begin(), band_audio.end(),
                            [](float sample) { return std::isfinite(sample); }) &&
                std::all_of(high_audio.begin(), high_audio.end(),
                            [](float sample) { return std::isfinite(sample); }),
            "a filter mode emitted non-finite output");
}

void test_direction_stereo_and_realtime_allocation() {
    tabletfx::EnvelopeWahParameters parameters;
    parameters.sensitivity = 1.0F;
    parameters.attack_ms = 0.0F;
    parameters.release_ms = 0.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::MutantFilter up(48000.0F, 2U, parameters);
    parameters.direction = tabletfx::EnvelopeWahDirection::Down;
    tabletfx::MutantFilter down(48000.0F, 2U, parameters);
    require(down.parameters().direction == tabletfx::EnvelopeWahDirection::Down,
            "wrapper did not preserve the selected sweep direction");

    std::array<float, 1024> up_audio{};
    for (std::size_t frame = 0; frame < up_audio.size() / 2U; ++frame) {
        const float sample = frame < 40U ? 0.02F : 0.4F * std::sin(
            2.0F * 3.14159265358979323846F * 600.0F *
            static_cast<float>(frame) / 48000.0F);
        up_audio[frame * 2U] = sample;
        up_audio[frame * 2U + 1U] = sample;
    }
    auto down_audio = up_audio;
    up.process(up_audio, 2U);
    down.process(down_audio, 2U);
    require(squared_difference(up_audio, down_audio) > 0.001,
            "up/down sweep selections did not change the envelope filter output");
    require(std::all_of(up_audio.begin(), up_audio.end(),
                        [](float sample) { return std::isfinite(sample); }) &&
                std::all_of(down_audio.begin(), down_audio.end(),
                            [](float sample) { return std::isfinite(sample); }),
            "stereo sweep produced non-finite output");

    std::array<float, 512> realtime_block{};
    realtime_block.fill(0.125F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        up.process(realtime_block, 2U);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "Mutant Filter allocated during audio processing");
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

int main() {
    try {
        test_defaults_mode_outputs_and_zero_buffering();
        test_direction_stereo_and_realtime_allocation();
        std::cout << "All Mutant Filter tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Mutant Filter test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
