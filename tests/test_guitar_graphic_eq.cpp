#include "tabletfx/effects/guitar_graphic_eq.hpp"

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

void test_layout_centers_and_switching() {
    tabletfx::GuitarGraphicEQ five(48000.0F, 1,
                                  tabletfx::GuitarGraphicEQLayout::FiveBand);
    constexpr std::array<float, 5> five_centers{100.0F, 250.0F, 630.0F,
                                               1600.0F, 4000.0F};
    require(five.id() == "guitar-graphic-eq", "graphic EQ ID changed");
    require(five.band_count() == five_centers.size(), "5-band layout has wrong band count");
    for (std::size_t band = 0; band < five_centers.size(); ++band) {
        require(five.band_frequency_hz(band) == five_centers[band],
                "5-band center frequencies changed");
    }
    require(five.band_frequency_hz(5) == 0.0F,
            "inactive 5-band index should report no center frequency");
    require(five.set_band_gain_db(1, 6.0F), "active band gain was rejected");
    five.set_layout(tabletfx::GuitarGraphicEQLayout::SevenBand);
    constexpr std::array<float, 7> seven_centers{100.0F, 200.0F, 400.0F,
                                                800.0F, 1600.0F, 3200.0F,
                                                6400.0F};
    require(five.layout() == tabletfx::GuitarGraphicEQLayout::SevenBand &&
            five.band_count() == seven_centers.size(),
            "layout switch did not select seven bands");
    for (std::size_t band = 0; band < seven_centers.size(); ++band) {
        require(five.band_frequency_hz(band) == seven_centers[band],
                "7-band center frequencies changed");
        require(five.band_gain_db(band) == 0.0F,
                "layout switch should clear all band gains");
    }
}

void test_center_response_for_both_layouts() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 24000;

    auto seven_tone = make_sine(kSampleRate, 800.0F, kFrames);
    tabletfx::GuitarGraphicEQ seven(kSampleRate, 1,
                                   tabletfx::GuitarGraphicEQLayout::SevenBand);
    require(seven.set_band_gain_db(3, 9.0F), "7-band center gain was rejected");
    seven.process(seven_tone, 1);
    require(rms(seven_tone, 1, 0) > 0.2F,
            "7-band boost did not increase its center-frequency response");

    auto five_tone = make_sine(kSampleRate, 630.0F, kFrames);
    tabletfx::GuitarGraphicEQ five(kSampleRate, 1,
                                  tabletfx::GuitarGraphicEQLayout::FiveBand);
    require(five.set_band_gain_db(2, -9.0F), "5-band cut was rejected");
    five.process(five_tone, 1);
    require(rms(five_tone, 1, 0) < 0.08F,
            "5-band cut did not reduce its center-frequency response");
}

void test_gain_bounds_and_nonfinite_control_values() {
    tabletfx::GuitarGraphicEQ eq(48000.0F, 1,
                                tabletfx::GuitarGraphicEQLayout::SevenBand);
    require(eq.set_band_gain_db(0, 100.0F) && eq.band_gain_db(0) == 12.0F,
            "gain did not respect its upper bound");
    require(eq.set_band_gain_db(1, -100.0F) && eq.band_gain_db(1) == -12.0F,
            "gain did not respect its lower bound");
    require(eq.set_band_gain_db(2, std::numeric_limits<float>::quiet_NaN()) &&
            eq.band_gain_db(2) == 0.0F,
            "NaN gain did not fall back to 0 dB");
    require(eq.set_band_gain_db(3, std::numeric_limits<float>::infinity()) &&
            eq.band_gain_db(3) == 0.0F,
            "infinite gain did not fall back to 0 dB");
    require(!eq.set_band_gain_db(eq.band_count(), 4.0F),
            "out-of-layout band index should be rejected");
    require(eq.memory_bytes() == sizeof(eq), "graphic EQ memory report is inaccurate");
}

void test_mono_stereo_and_nonfinite_samples() {
    constexpr float kSampleRate = 48000.0F;
    tabletfx::GuitarGraphicEQ eq(kSampleRate, 2,
                                tabletfx::GuitarGraphicEQLayout::SevenBand);
    require(eq.set_band_gain_db(3, 9.0F), "stereo band gain was rejected");
    auto stereo = make_sine(kSampleRate, 800.0F, 4096, 2, 1);
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
        require(std::isfinite(sample), "non-finite input escaped the EQ");
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

    std::array<float, 4> mismatch{0.1F, -0.2F, 0.3F, -0.4F};
    const auto original = mismatch;
    eq.process(mismatch, 1);
    require(mismatch == original,
            "channel mismatch should leave the input buffer unchanged");

    std::array<float, 5> partial_frame{0.1F, -0.1F, 0.2F, -0.2F, 0.3F};
    eq.process(partial_frame, 2);
    require(std::isfinite(partial_frame.back()),
            "incomplete trailing frame was not left finite");
}

void test_irregular_blocks_and_reset() {
    constexpr float kSampleRate = 48000.0F;
    auto input = make_sine(kSampleRate, 420.0F, 4097, 2, 0);
    auto full = input;
    auto chunked = input;
    tabletfx::GuitarGraphicEQ full_eq(kSampleRate, 2,
                                     tabletfx::GuitarGraphicEQLayout::SevenBand);
    tabletfx::GuitarGraphicEQ chunked_eq(kSampleRate, 2,
                                        tabletfx::GuitarGraphicEQLayout::SevenBand);
    require(full_eq.set_band_gain_db(2, 4.0F), "full EQ band 3 setup failed");
    require(full_eq.set_band_gain_db(3, -3.0F), "full EQ band 4 setup failed");
    require(full_eq.set_band_gain_db(5, 2.0F), "full EQ band 6 setup failed");
    require(chunked_eq.set_band_gain_db(2, 4.0F), "chunked EQ band 3 setup failed");
    require(chunked_eq.set_band_gain_db(3, -3.0F), "chunked EQ band 4 setup failed");
    require(chunked_eq.set_band_gain_db(5, 2.0F), "chunked EQ band 6 setup failed");
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
                "irregular blocks changed graphic EQ output");
    }

    chunked_eq.reset();
    auto after_reset = input;
    chunked_eq.process(after_reset, 2);
    for (std::size_t i = 0; i < full.size(); ++i) {
        require(std::abs(full[i] - after_reset[i]) < 1.0e-6F,
                "reset did not restore the initial graphic EQ state");
    }
}

void test_process_allocates_nothing() {
    tabletfx::GuitarGraphicEQ eq(48000.0F, 2,
                                tabletfx::GuitarGraphicEQLayout::SevenBand);
    require(eq.set_band_gain_db(1, 3.0F), "allocation-test band 2 setup failed");
    require(eq.set_band_gain_db(3, -2.0F), "allocation-test band 4 setup failed");
    std::vector<float> samples(512, 0.125F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        eq.process(samples, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "guitar graphic EQ allocated during process()");
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
        test_layout_centers_and_switching();
        test_center_response_for_both_layouts();
        test_gain_bounds_and_nonfinite_control_values();
        test_mono_stereo_and_nonfinite_samples();
        test_irregular_blocks_and_reset();
        test_process_allocates_nothing();
        std::cout << "All guitar graphic EQ tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
