#include "tabletfx/effects/shelf_tilt_eq.hpp"

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
    std::vector<float> samples(frames * channels, 0.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float phase = 2.0F * 3.14159265358979323846F * frequency *
                            static_cast<float>(frame) / sample_rate;
        samples[frame * channels + active_channel] = 0.2F * std::sin(phase);
    }
    return samples;
}

float rms(std::span<const float> samples, std::uint16_t channels,
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

void test_low_and_high_shelf_response() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 24000;

    auto low_tone = make_sine(kSampleRate, 80.0F, kFrames);
    tabletfx::ShelfTiltEQ low(kSampleRate, 1, tabletfx::ShelfTiltMode::LowShelf,
                              200.0F, 9.0F);
    low.process(low_tone, 1);
    require(rms(low_tone, 1, 0) > 0.2F,
            "low shelf did not boost frequencies below its corner");

    auto high_tone = make_sine(kSampleRate, 8000.0F, kFrames);
    tabletfx::ShelfTiltEQ high(kSampleRate, 1, tabletfx::ShelfTiltMode::HighShelf,
                               4000.0F, 9.0F);
    high.process(high_tone, 1);
    require(rms(high_tone, 1, 0) > 0.2F,
            "high shelf did not boost frequencies above its corner");
}

void test_complementary_tilt_response() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 24000;
    tabletfx::ShelfTiltEQ tilt(kSampleRate, 1, tabletfx::ShelfTiltMode::Tilt,
                               500.0F, 6.0F);
    auto low_tone = make_sine(kSampleRate, 80.0F, kFrames);
    auto pivot_tone = make_sine(kSampleRate, 500.0F, kFrames);
    auto high_tone = make_sine(kSampleRate, 8000.0F, kFrames);
    const auto pivot_original = pivot_tone;
    tilt.process(low_tone, 1);
    tilt.reset();
    tilt.process(pivot_tone, 1);
    tilt.reset();
    tilt.process(high_tone, 1);

    require(rms(low_tone, 1, 0) > 0.2F,
            "positive tilt did not raise the low-frequency side");
    require(rms(high_tone, 1, 0) < 0.12F,
            "positive tilt did not lower the high-frequency side");
    require(std::abs(rms(pivot_tone, 1, 0) - rms(pivot_original, 1, 0)) < 0.01F,
            "tilt response did not cross near its pivot frequency");
}

void test_bounds_modes_and_parameter_changes() {
    tabletfx::ShelfTiltEQ eq(48000.0F, 1, tabletfx::ShelfTiltMode::LowShelf,
                             200.0F, 0.0F);
    require(eq.id() == "shelf-tilt-eq", "shelf/tilt EQ ID changed");
    require(eq.memory_bytes() == sizeof(eq), "shelf/tilt memory report is inaccurate");

    eq.set_frequency_hz(-100.0F);
    require(eq.frequency_hz() == tabletfx::ShelfTiltEQ::kMinimumFrequencyHz,
            "frequency did not respect its lower bound");
    eq.set_frequency_hz(100000.0F);
    require(eq.frequency_hz() == 21600.0F,
            "frequency did not respect its sample-rate-aware upper bound");
    eq.set_frequency_hz(std::numeric_limits<float>::quiet_NaN());
    require(eq.frequency_hz() == 1000.0F,
            "non-finite frequency did not use its safe default");

    eq.set_gain_db(100.0F);
    require(eq.gain_db() == 12.0F, "gain did not respect its upper bound");
    eq.set_gain_db(-100.0F);
    require(eq.gain_db() == -12.0F, "gain did not respect its lower bound");
    eq.set_gain_db(std::numeric_limits<float>::infinity());
    require(eq.gain_db() == 0.0F, "non-finite gain did not use its safe default");

    eq.configure(tabletfx::ShelfTiltMode::HighShelf, 2500.0F, 6.0F);
    require(eq.mode() == tabletfx::ShelfTiltMode::HighShelf &&
            eq.frequency_hz() == 2500.0F && eq.gain_db() == 6.0F,
            "configure did not update mode and parameters");
    eq.set_mode(tabletfx::ShelfTiltMode::Tilt);
    require(eq.mode() == tabletfx::ShelfTiltMode::Tilt,
            "mode change did not select tilt processing");

    auto low_tone = make_sine(48000.0F, 80.0F, 12000);
    eq.process(low_tone, 1);
    require(rms(low_tone, 1, 0) > 0.2F,
            "mode change did not rebuild the active shelf coefficients");
}

void test_stereo_isolation_and_nonfinite_recovery() {
    constexpr float kSampleRate = 48000.0F;
    tabletfx::ShelfTiltEQ eq(kSampleRate, 2, tabletfx::ShelfTiltMode::LowShelf,
                             200.0F, 6.0F);
    auto stereo = make_sine(kSampleRate, 80.0F, 4096, 2, 1);
    eq.process(stereo, 2);
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        require(stereo[frame * 2] == 0.0F,
                "right-channel signal leaked into the left channel");
    }
    require(rms(stereo, 2, 1) > 0.15F,
            "active stereo channel was unexpectedly silent");

    std::array<float, 6> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.1F, -0.1F, 0.2F,
    };
    eq.process(invalid, 2);
    for (float sample : invalid) {
        require(std::isfinite(sample), "non-finite input escaped shelf/tilt EQ");
    }

    std::array<float, 4> extreme{
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        0.25F, -0.25F,
    };
    eq.process(extreme, 2);
    for (float sample : extreme) {
        require(std::isfinite(sample), "extreme finite input produced non-finite output");
    }
}

void test_irregular_blocks_and_reset() {
    constexpr float kSampleRate = 48000.0F;
    auto input = make_sine(kSampleRate, 437.0F, 4103, 2, 0);
    auto full = input;
    auto chunked = input;
    tabletfx::ShelfTiltEQ full_eq(kSampleRate, 2, tabletfx::ShelfTiltMode::Tilt,
                                  700.0F, 5.0F);
    tabletfx::ShelfTiltEQ chunked_eq(kSampleRate, 2, tabletfx::ShelfTiltMode::Tilt,
                                     700.0F, 5.0F);
    full_eq.process(full, 2);

    constexpr std::array<std::size_t, 6> block_sizes{1, 63, 257, 128, 509, 17};
    std::size_t frame = 0;
    std::size_t block_index = 0;
    while (frame < chunked.size() / 2) {
        const std::size_t count = std::min(
            block_sizes[block_index % block_sizes.size()],
            chunked.size() / 2 - frame);
        chunked_eq.process(std::span<float>(chunked).subspan(frame * 2, count * 2), 2);
        frame += count;
        ++block_index;
    }
    for (std::size_t i = 0; i < full.size(); ++i) {
        require(std::abs(full[i] - chunked[i]) < 1.0e-6F,
                "irregular blocks changed shelf/tilt output");
    }

    chunked_eq.reset();
    auto after_reset = input;
    chunked_eq.process(after_reset, 2);
    for (std::size_t i = 0; i < full.size(); ++i) {
        require(std::abs(full[i] - after_reset[i]) < 1.0e-6F,
                "reset did not restore the initial shelf/tilt state");
    }
}

void test_process_allocates_nothing() {
    tabletfx::ShelfTiltEQ eq(48000.0F, 2, tabletfx::ShelfTiltMode::Tilt,
                             300.0F, 4.0F);
    std::vector<float> samples(512, 0.125F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        eq.process(samples, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "shelf/tilt EQ allocated during process()");
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
        test_low_and_high_shelf_response();
        test_complementary_tilt_response();
        test_bounds_modes_and_parameter_changes();
        test_stereo_isolation_and_nonfinite_recovery();
        test_irregular_blocks_and_reset();
        test_process_allocates_nothing();
        std::cout << "All shelf/tilt EQ tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
