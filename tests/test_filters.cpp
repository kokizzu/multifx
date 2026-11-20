#include "tabletfx/effects/filters.hpp"

#include <algorithm>
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

float rms_tail(std::span<const float> samples, std::uint16_t channels,
               std::uint16_t channel = 0) {
    const std::size_t frames = samples.size() / channels;
    const std::size_t first = frames / 2;
    double power = 0.0;
    std::size_t count = 0;
    for (std::size_t frame = first; frame < frames; ++frame) {
        const double sample = samples[frame * channels + channel];
        power += sample * sample;
        ++count;
    }
    return count == 0 ? 0.0F : static_cast<float>(std::sqrt(power / count));
}

template <typename Filter>
std::vector<float> filtered_tone(float frequency, Filter& filter,
                                float sample_rate = 48000.0F) {
    std::vector<float> samples = make_sine(sample_rate, frequency, 48000);
    filter.process(samples, 1);
    return samples;
}

void test_high_pass_response_and_bounds() {
    constexpr float kSampleRate = 48000.0F;
    tabletfx::HighPassFilter filter(kSampleRate, 1, 1000.0F);
    const auto low_tone = filtered_tone(100.0F, filter);
    require(rms_tail(low_tone, 1) < 0.04F,
            "high-pass did not sufficiently attenuate below-cutoff tone");
    filter.reset();
    const auto high_tone = filtered_tone(4000.0F, filter);
    require(rms_tail(high_tone, 1) > 0.15F,
            "high-pass attenuated a passband tone excessively");

    filter.configure(std::numeric_limits<float>::quiet_NaN());
    require(filter.cutoff_hz() == 1000.0F,
            "non-finite high-pass cutoff did not fall back to the default");
    filter.configure(-1000.0F);
    require(filter.cutoff_hz() == 20.0F, "high-pass cutoff lower bound failed");
    filter.configure(1.0e30F);
    require(filter.cutoff_hz() == 21600.0F, "high-pass cutoff upper bound failed");
}

void test_low_pass_response_and_bounds() {
    constexpr float kSampleRate = 48000.0F;
    tabletfx::LowPassFilter filter(kSampleRate, 1, 1000.0F);
    const auto low_tone = filtered_tone(100.0F, filter);
    require(rms_tail(low_tone, 1) > 0.15F,
            "low-pass attenuated a passband tone excessively");
    filter.reset();
    const auto high_tone = filtered_tone(8000.0F, filter);
    require(rms_tail(high_tone, 1) < 0.02F,
            "low-pass did not sufficiently attenuate above-cutoff tone");

    filter.configure(std::numeric_limits<float>::infinity());
    require(filter.cutoff_hz() == 1000.0F,
            "non-finite low-pass cutoff did not fall back to its default");
    filter.configure(0.0F);
    require(filter.cutoff_hz() == 20.0F, "low-pass cutoff lower bound failed");
    filter.configure(1.0e30F);
    require(filter.cutoff_hz() == 21600.0F, "low-pass cutoff upper bound failed");
}

void test_stereo_isolation_and_channel_mismatch() {
    constexpr float kSampleRate = 48000.0F;
    tabletfx::HighPassFilter filter(kSampleRate, 2, 500.0F);
    std::vector<float> stereo = make_sine(kSampleRate, 900.0F, 4096, 2, 1);
    filter.process(stereo, 2);
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        require(stereo[frame * 2] == 0.0F,
                "right-channel signal leaked into left channel");
    }
    require(rms_tail(stereo, 2, 1) > 0.1F,
            "active stereo channel unexpectedly became silent");

    std::vector<float> untouched{0.1F, 0.2F, 0.3F};
    const auto original = untouched;
    filter.process(untouched, 1);
    require(untouched == original,
            "filter processed a channel layout different from its prepared layout");
}

void test_variable_blocks_and_reset() {
    constexpr float kSampleRate = 48000.0F;
    std::vector<float> input = make_sine(kSampleRate, 430.0F, 4096, 2, 0);
    std::vector<float> whole = input;
    std::vector<float> chunks = input;
    tabletfx::LowPassFilter whole_filter(kSampleRate, 2, 4200.0F);
    tabletfx::LowPassFilter chunk_filter(kSampleRate, 2, 4200.0F);
    whole_filter.process(whole, 2);

    const std::size_t sizes[]{1, 63, 257, 128, 509, 17, 1024, 31};
    std::size_t frame = 0;
    std::size_t index = 0;
    while (frame < chunks.size() / 2) {
        const std::size_t block = std::min(sizes[index % 8], chunks.size() / 2 - frame);
        chunk_filter.process(std::span<float>(chunks).subspan(frame * 2, block * 2), 2);
        frame += block;
        ++index;
    }
    for (std::size_t sample = 0; sample < whole.size(); ++sample) {
        require(std::abs(whole[sample] - chunks[sample]) < 1.0e-6F,
                "variable block sizes changed filter output");
    }

    chunk_filter.reset();
    std::vector<float> after_reset = input;
    chunk_filter.process(after_reset, 2);
    for (std::size_t sample = 0; sample < whole.size(); ++sample) {
        require(std::abs(whole[sample] - after_reset[sample]) < 1.0e-6F,
                "reset did not restore the initial filter response");
    }
}

void test_extreme_impulse_stability_and_nonfinite_recovery() {
    tabletfx::HighPassFilter high(48000.0F, 1, 20.0F);
    tabletfx::LowPassFilter low(48000.0F, 1, 21600.0F);
    std::vector<float> impulse(8192, 0.0F);
    impulse[0] = 1.0F;
    high.process(impulse, 1);
    low.process(impulse, 1);
    for (float sample : impulse) {
        require(std::isfinite(sample), "extreme cutoff produced non-finite output");
    }

    std::vector<float> invalid{std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity(), 0.25F};
    high.process(invalid, 1);
    for (float sample : invalid) {
        require(std::isfinite(sample), "filter did not recover from non-finite input");
    }
}

void test_construction_memory_and_process_allocations() {
    bool rejected_rate = false;
    try {
        tabletfx::HighPassFilter bad_rate(100.0F, 1);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    require(rejected_rate, "invalid sample rate was accepted");

    bool rejected_channels = false;
    try {
        tabletfx::LowPassFilter bad_channels(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    require(rejected_channels, "unsupported channel count was accepted");

    tabletfx::HighPassFilter high(48000.0F, 2);
    tabletfx::LowPassFilter low(48000.0F, 2);
    require(high.cutoff_hz() == 80.0F && low.cutoff_hz() == 12000.0F,
            "default high/low-pass cutoffs changed unexpectedly");
    require(high.memory_bytes() == sizeof(high) && low.memory_bytes() == sizeof(low),
            "memory_bytes must report fixed in-object processor storage");
    std::vector<float> samples(512, 0.125F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    high.process(samples, 2);
    low.process(samples, 2);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "filter process() allocated memory");
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
        test_high_pass_response_and_bounds();
        test_low_pass_response_and_bounds();
        test_stereo_isolation_and_channel_mismatch();
        test_variable_blocks_and_reset();
        test_extreme_impulse_stability_and_nonfinite_recovery();
        test_construction_memory_and_process_allocations();
        std::cout << "High/low-pass filter tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
