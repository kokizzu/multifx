#include "tabletfx/effects/three_band_eq.hpp"

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
    std::vector<float> samples(frames * channels, 0.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float phase = 2.0F * 3.14159265358979323846F * frequency *
                            static_cast<float>(frame) / sample_rate;
        samples[frame * channels + active_channel] = std::sin(phase) * 0.25F;
    }
    return samples;
}

float tail_rms(std::span<const float> samples, std::uint16_t channels,
               std::uint16_t channel) {
    const std::size_t frames = samples.size() / channels;
    const std::size_t begin = frames / 2;
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t frame = begin; frame < frames; ++frame) {
        const double value = samples[frame * channels + channel];
        sum += value * value;
        ++count;
    }
    return count == 0 ? 0.0F : static_cast<float>(std::sqrt(sum / count));
}

void test_neutral_is_exact_passthrough() {
    tabletfx::ThreeBandParametricEQ eq;
    std::vector<float> samples{0.1F, -0.2F, 0.3F, -0.4F, 1.0F, -1.0F};
    const std::vector<float> original = samples;
    eq.process(samples, 2);
    require(samples == original, "neutral EQ must preserve samples exactly");
}

void test_all_three_peak_bands_increase_their_center_response() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 48000;

    const float center_frequencies[3]{120.0F, 1000.0F, 8000.0F};
    for (std::size_t active_band = 0; active_band < 3; ++active_band) {
        auto tone = make_sine(kSampleRate, center_frequencies[active_band], kFrames);
        tabletfx::ThreeBandParametricEQ eq(kSampleRate, 1);
        eq.configure(120.0F, active_band == 0 ? 9.0F : 0.0F, 1.0F,
                     1000.0F, active_band == 1 ? 9.0F : 0.0F, 1.0F,
                     8000.0F, active_band == 2 ? 9.0F : 0.0F, 1.0F);
        eq.process(tone, 1);
        require(tail_rms(tone, 1, 0) > 0.40F,
                "peaking EQ band did not boost its center-frequency tone");
    }
}

void test_silence_nonfinite_input_and_parameter_bounds() {
    tabletfx::ThreeBandParametricEQ eq;
    eq.configure(std::numeric_limits<float>::quiet_NaN(),
                 std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::quiet_NaN(),
                 100.0F,
                 1.0e30F, -1.0e30F, 0.0F);
    require(eq.band1_frequency_hz() == 120.0F && eq.band1_gain_db() == 0.0F &&
            eq.band1_q() == 0.707F,
            "non-finite band 1 settings did not use safe defaults");
    require(eq.band2_frequency_hz() == 800.0F && eq.band2_gain_db() == 0.0F &&
            eq.band2_q() == 10.0F,
            "non-finite band 2 settings did not use safe defaults or bounds");
    require(eq.band3_frequency_hz() == 21600.0F && eq.band3_gain_db() == -18.0F &&
            eq.band3_q() == 0.25F,
            "band 3 settings were not safely bounded");

    std::vector<float> silence(1024, 0.0F);
    eq.process(silence, 2);
    for (float sample : silence) {
        require(sample == 0.0F, "EQ produced output from silence");
    }

    std::vector<float> invalid{std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity(), -0.25F, 0.5F};
    eq.process(invalid, 2);
    for (float sample : invalid) {
        require(std::isfinite(sample), "EQ did not contain a non-finite input");
    }
}

void test_stereo_state_is_independent() {
    constexpr float kSampleRate = 48000.0F;
    tabletfx::ThreeBandParametricEQ eq(kSampleRate, 2);
    eq.configure(120.0F, 0.0F, 0.707F,
                 1000.0F, 12.0F, 0.707F,
                 8000.0F, 0.0F, 0.707F);
    std::vector<float> samples = make_sine(kSampleRate, 1000.0F, 4096, 2, 1);
    eq.process(samples, 2);
    for (std::size_t frame = 0; frame < samples.size() / 2; ++frame) {
        require(samples[frame * 2] == 0.0F,
                "right-channel signal leaked into the left channel");
    }
    require(tail_rms(samples, 2, 1) > 0.2F,
            "active stereo channel was unexpectedly silent");
}

void test_reset_and_variable_blocks_are_deterministic() {
    constexpr float kSampleRate = 48000.0F;
    std::vector<float> input = make_sine(kSampleRate, 420.0F, 4096, 2, 0);
    std::vector<float> full = input;
    std::vector<float> chunked = input;
    tabletfx::ThreeBandParametricEQ full_eq(kSampleRate, 2);
    tabletfx::ThreeBandParametricEQ chunked_eq(kSampleRate, 2);
    full_eq.configure(120.0F, 4.0F, 0.8F,
                      750.0F, -3.0F, 0.8F,
                      6200.0F, 2.0F, 0.8F);
    chunked_eq.configure(120.0F, 4.0F, 0.8F,
                          750.0F, -3.0F, 0.8F,
                          6200.0F, 2.0F, 0.8F);
    full_eq.process(full, 2);
    const std::size_t block_sizes[]{1, 63, 257, 128, 509, 17, 1024, 31};
    std::size_t frame = 0;
    std::size_t block_index = 0;
    while (frame < chunked.size() / 2) {
        const std::size_t block = std::min(block_sizes[block_index % 8],
                                           chunked.size() / 2 - frame);
        chunked_eq.process(std::span<float>(chunked).subspan(frame * 2, block * 2), 2);
        frame += block;
        ++block_index;
    }
    for (std::size_t i = 0; i < full.size(); ++i) {
        require(std::abs(full[i] - chunked[i]) < 1.0e-6F,
                "variable block sizes changed the EQ result");
    }

    chunked_eq.reset();
    std::vector<float> after_reset = input;
    chunked_eq.process(after_reset, 2);
    for (std::size_t i = 0; i < full.size(); ++i) {
        require(std::abs(full[i] - after_reset[i]) < 1.0e-6F,
                "reset did not restore the initial filter state");
    }
}

void test_process_allocates_nothing() {
    tabletfx::ThreeBandParametricEQ eq;
    require(eq.memory_bytes() == sizeof(eq),
            "parametric EQ should use fixed in-object state only");
    eq.configure(120.0F, 3.0F, 0.707F,
                 800.0F, -2.0F, 0.707F,
                 5000.0F, 4.0F, 0.707F);
    std::vector<float> samples(512, 0.125F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    eq.process(samples, 2);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "ThreeBandParametricEQ allocated memory during process()");
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
        test_neutral_is_exact_passthrough();
        test_all_three_peak_bands_increase_their_center_response();
        test_silence_nonfinite_input_and_parameter_bounds();
        test_stereo_state_is_independent();
        test_reset_and_variable_blocks_are_deterministic();
        test_process_allocates_nothing();
        std::cout << "ThreeBandEQ tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
