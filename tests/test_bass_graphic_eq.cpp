#include "tabletfx/effects/bass_graphic_eq.hpp"

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
    tabletfx::BassGraphicEQ four(48000.0F, 1,
                                tabletfx::BassGraphicEQLayout::FourBand);
    constexpr std::array<float, 4> four_centers{50.0F, 250.0F,
                                               1000.0F, 4000.0F};
    require(four.id() == "bass-graphic-eq", "bass graphic EQ ID changed");
    require(four.band_count() == four_centers.size(),
            "4-band layout has the wrong number of bands");
    for (std::size_t band = 0; band < four_centers.size(); ++band) {
        require(four.band_frequency_hz(band) == four_centers[band],
                "4-band bass center frequencies changed");
    }
    require(four.band_frequency_hz(4) == 0.0F,
            "inactive 4-band index should report no center frequency");
    require(four.set_band_gain_db(1, 6.0F), "active 4-band gain was rejected");
    four.set_layout(tabletfx::BassGraphicEQLayout::SevenBand);
    constexpr std::array<float, 7> seven_centers{40.0F, 80.0F, 160.0F,
                                                320.0F, 640.0F, 1600.0F,
                                                5000.0F};
    require(four.layout() == tabletfx::BassGraphicEQLayout::SevenBand &&
            four.band_count() == seven_centers.size(),
            "layout switch did not select seven bands");
    for (std::size_t band = 0; band < seven_centers.size(); ++band) {
        require(four.band_frequency_hz(band) == seven_centers[band],
                "7-band bass center frequencies changed");
        require(four.band_gain_db(band) == 0.0F,
                "layout switch should clear the band gains");
    }
}

void test_low_and_mid_response() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 48000;

    auto low_tone = make_sine(kSampleRate, 40.0F, kFrames);
    tabletfx::BassGraphicEQ seven(kSampleRate, 1,
                                 tabletfx::BassGraphicEQLayout::SevenBand);
    require(seven.set_band_gain_db(0, 9.0F), "40-Hz band gain was rejected");
    seven.process(low_tone, 1);
    require(rms(low_tone, 1, 0) > 0.2F,
            "7-band EQ did not boost the low bass center frequency");

    auto low_mid_tone = make_sine(kSampleRate, 250.0F, kFrames);
    tabletfx::BassGraphicEQ four(kSampleRate, 1,
                                tabletfx::BassGraphicEQLayout::FourBand);
    require(four.set_band_gain_db(1, -9.0F), "250-Hz band cut was rejected");
    four.process(low_mid_tone, 1);
    require(rms(low_mid_tone, 1, 0) < 0.08F,
            "4-band EQ did not cut its low-mid center frequency");
}

void test_gain_bounds_and_nonfinite_controls() {
    tabletfx::BassGraphicEQ eq(48000.0F, 1,
                              tabletfx::BassGraphicEQLayout::SevenBand);
    require(eq.set_band_gain_db(0, 100.0F) && eq.band_gain_db(0) == 12.0F,
            "bass EQ gain did not respect its upper bound");
    require(eq.set_band_gain_db(1, -100.0F) && eq.band_gain_db(1) == -12.0F,
            "bass EQ gain did not respect its lower bound");
    require(eq.set_band_gain_db(2, std::numeric_limits<float>::quiet_NaN()) &&
            eq.band_gain_db(2) == 0.0F,
            "NaN bass EQ gain did not fall back to 0 dB");
    require(eq.set_band_gain_db(3, std::numeric_limits<float>::infinity()) &&
            eq.band_gain_db(3) == 0.0F,
            "infinite bass EQ gain did not fall back to 0 dB");
    require(!eq.set_band_gain_db(eq.band_count(), 3.0F),
            "out-of-layout bass band index should be rejected");
    require(eq.memory_bytes() == sizeof(eq), "bass EQ memory report is inaccurate");
}

void test_mono_stereo_and_nonfinite_recovery() {
    constexpr float kSampleRate = 48000.0F;
    tabletfx::BassGraphicEQ mono(kSampleRate, 1,
                                tabletfx::BassGraphicEQLayout::SevenBand);
    require(mono.set_band_gain_db(2, 6.0F), "mono band gain was rejected");
    auto mono_tone = make_sine(kSampleRate, 160.0F, 4096, 1);
    mono.process(mono_tone, 1);
    require(rms(mono_tone, 1, 0) > 0.15F,
            "mono EQ did not process its bass-register signal");

    tabletfx::BassGraphicEQ stereo(kSampleRate, 2,
                                  tabletfx::BassGraphicEQLayout::SevenBand);
    require(stereo.set_band_gain_db(2, 6.0F), "stereo band gain was rejected");
    auto stereo_tone = make_sine(kSampleRate, 160.0F, 4096, 2, 1);
    stereo.process(stereo_tone, 2);
    for (std::size_t frame = 0; frame < stereo_tone.size() / 2; ++frame) {
        require(stereo_tone[frame * 2] == 0.0F,
                "right-channel signal leaked into the left channel");
    }
    require(rms(stereo_tone, 2, 1) > 0.15F,
            "active stereo channel was unexpectedly silent");

    std::array<float, 6> nonfinite{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.1F, -0.1F, 0.2F,
    };
    stereo.process(nonfinite, 2);
    for (float sample : nonfinite) {
        require(std::isfinite(sample), "non-finite sample escaped the bass EQ");
    }

    std::array<float, 4> extreme{
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        0.25F, -0.25F,
    };
    stereo.process(extreme, 2);
    for (float sample : extreme) {
        require(std::isfinite(sample), "extreme finite sample produced non-finite output");
    }
}

void test_irregular_blocks_and_reset() {
    constexpr float kSampleRate = 48000.0F;
    auto input = make_sine(kSampleRate, 187.0F, 4103, 2, 0);
    auto full = input;
    auto chunked = input;
    tabletfx::BassGraphicEQ full_eq(kSampleRate, 2,
                                   tabletfx::BassGraphicEQLayout::SevenBand);
    tabletfx::BassGraphicEQ chunked_eq(kSampleRate, 2,
                                      tabletfx::BassGraphicEQLayout::SevenBand);
    require(full_eq.set_band_gain_db(1, 3.0F), "full EQ band 2 setup failed");
    require(full_eq.set_band_gain_db(3, -4.0F), "full EQ band 4 setup failed");
    require(full_eq.set_band_gain_db(5, 2.0F), "full EQ band 6 setup failed");
    require(chunked_eq.set_band_gain_db(1, 3.0F), "chunked EQ band 2 setup failed");
    require(chunked_eq.set_band_gain_db(3, -4.0F), "chunked EQ band 4 setup failed");
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
                "irregular blocks changed bass graphic EQ output");
    }

    chunked_eq.reset();
    auto after_reset = input;
    chunked_eq.process(after_reset, 2);
    for (std::size_t i = 0; i < full.size(); ++i) {
        require(std::abs(full[i] - after_reset[i]) < 1.0e-6F,
                "reset did not restore the initial bass EQ state");
    }
}

void test_process_allocates_nothing() {
    tabletfx::BassGraphicEQ eq(48000.0F, 2,
                              tabletfx::BassGraphicEQLayout::SevenBand);
    require(eq.set_band_gain_db(1, 3.0F), "allocation-test band 2 setup failed");
    require(eq.set_band_gain_db(3, -2.0F), "allocation-test band 4 setup failed");
    std::vector<float> samples(512, 0.125F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        eq.process(samples, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "bass graphic EQ allocated during process()");
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
        test_low_and_mid_response();
        test_gain_bounds_and_nonfinite_controls();
        test_mono_stereo_and_nonfinite_recovery();
        test_irregular_blocks_and_reset();
        test_process_allocates_nothing();
        std::cout << "All bass graphic EQ tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
