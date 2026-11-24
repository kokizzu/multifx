#include "tabletfx/effects/stadium_seven_band_peq.hpp"

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

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
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
               std::uint16_t channel = 0) {
    const std::size_t frames = samples.size() / channels;
    double sum = 0.0;
    for (std::size_t frame = frames / 2; frame < frames; ++frame) {
        const double sample = samples[frame * channels + channel];
        sum += sample * sample;
    }
    return static_cast<float>(std::sqrt(sum / (frames - frames / 2)));
}

void test_default_is_neutral_and_has_project_memory_bound() {
    tabletfx::StadiumSevenBandParametricEQ eq;
    require(eq.id() == "stadium-seven-band-peq", "unexpected PEQ id");
    require(eq.memory_bytes() < 4096, "seven-band PEQ state must remain compact");
    std::vector<float> samples{0.1F, -0.2F, 0.3F, -0.4F, 1.0F, -1.0F};
    const auto original = samples;
    eq.process(samples, 2);
    require(samples == original, "neutral seven-band EQ must preserve input exactly");
}

void test_each_peaking_band_boosts_its_center() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 48000;
    constexpr std::array<float, 7> centers{
        63.0F, 160.0F, 400.0F, 1000.0F, 2500.0F, 6300.0F, 12500.0F};
    for (std::size_t index = 0; index < centers.size(); ++index) {
        tabletfx::StadiumSevenBandParametricEQ eq(kSampleRate, 1);
        auto bands = eq.parameters();
        bands[index].gain_db = 12.0F;
        bands[index].q = 1.0F;
        eq.configure(bands);
        auto tone = make_sine(kSampleRate, centers[index], kFrames);
        eq.process(tone, 1);
        require(tail_rms(tone, 1) > 0.50F,
                "peaking band failed to boost its center tone");
    }
}

void test_per_band_bypass_skips_filter_and_preserves_other_bands() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 48000;
    tabletfx::StadiumSevenBandParametricEQ eq(kSampleRate, 1);
    auto bands = eq.parameters();
    bands[2].gain_db = 12.0F;
    bands[4].gain_db = -9.0F;
    bands[2].enabled = false;
    eq.configure(bands);
    auto center = make_sine(kSampleRate, bands[2].frequency_hz, kFrames);
    eq.process(center, 1);
    require(tail_rms(center, 1) < 0.19F,
            "bypassed peaking band still changed its center tone");
    auto other_center = make_sine(kSampleRate, bands[4].frequency_hz, kFrames);
    eq.reset();
    eq.process(other_center, 1);
    require(tail_rms(other_center, 1) < 0.11F,
            "bypassing one band incorrectly disabled another active band");
}

void test_cut_slopes_change_rolloff_with_fixed_sections() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 48000;
    tabletfx::StadiumSevenBandParametricEQ gentle(kSampleRate, 1);
    tabletfx::StadiumSevenBandParametricEQ steep(kSampleRate, 1);
    auto gentle_bands = gentle.parameters();
    auto steep_bands = steep.parameters();
    gentle_bands[0].shape = tabletfx::SevenBandEQShape::LowCut;
    steep_bands[0].shape = tabletfx::SevenBandEQShape::LowCut;
    gentle_bands[0].frequency_hz = 1000.0F;
    steep_bands[0].frequency_hz = 1000.0F;
    gentle_bands[0].slope_db_per_octave = 6.0F;
    steep_bands[0].slope_db_per_octave = 36.0F;
    gentle.configure(gentle_bands);
    steep.configure(steep_bands);
    auto below_gentle = make_sine(kSampleRate, 500.0F, kFrames);
    auto below_steep = below_gentle;
    gentle.process(below_gentle, 1);
    steep.process(below_steep, 1);
    require(tail_rms(below_steep, 1) < tail_rms(below_gentle, 1) * 0.08F,
            "steeper low cut did not attenuate more strongly below cutoff");

    auto highcut_bands = steep.parameters();
    highcut_bands[6].shape = tabletfx::SevenBandEQShape::HighCut;
    highcut_bands[6].frequency_hz = 1000.0F;
    highcut_bands[6].slope_db_per_octave = 36.0F;
    steep.configure(highcut_bands);
    auto above = make_sine(kSampleRate, 2000.0F, kFrames);
    steep.process(above, 1);
    require(tail_rms(above, 1) < 0.01F,
            "36 dB/oct high cut did not attenuate one octave above cutoff");
}

void test_bounds_and_nonfinite_controls_are_sanitized() {
    tabletfx::StadiumSevenBandParametricEQ eq(48000.0F, 1);
    auto bands = eq.parameters();
    bands[0].frequency_hz = 30000.0F;
    bands[0].gain_db = 100.0F;
    bands[0].q = 0.01F;
    bands[0].slope_db_per_octave = 100.0F;
    bands[1].shape = static_cast<tabletfx::SevenBandEQShape>(255U);
    bands[2].frequency_hz = std::numeric_limits<float>::quiet_NaN();
    bands[2].gain_db = std::numeric_limits<float>::infinity();
    bands[2].q = -std::numeric_limits<float>::infinity();
    bands[2].slope_db_per_octave = std::numeric_limits<float>::quiet_NaN();
    eq.configure(bands);
    const auto actual = eq.parameters();
    require(actual[0].frequency_hz == 20000.0F && actual[0].gain_db == 18.0F &&
                actual[0].q == 0.25F && actual[0].slope_db_per_octave == 36.0F,
            "seven-band EQ control limits were not enforced");
    require(actual[1].shape == tabletfx::SevenBandEQShape::Peaking,
            "invalid band shape did not fall back to Peaking");
    require(actual[2].frequency_hz == 400.0F && actual[2].gain_db == 0.0F &&
                actual[2].q == 0.707F && actual[2].slope_db_per_octave == 12.0F,
            "non-finite controls did not restore the project defaults");
}

void test_stereo_channels_are_isolated_and_processing_does_not_allocate() {
    constexpr float kSampleRate = 48000.0F;
    tabletfx::StadiumSevenBandParametricEQ eq(kSampleRate, 2);
    auto bands = eq.parameters();
    bands[3].gain_db = 9.0F;
    eq.configure(bands);
    auto samples = make_sine(kSampleRate, 1000.0F, 8192, 2, 1);
    allocation_count.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    eq.process(samples, 2);
    count_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0,
            "process() allocated memory");
    for (std::size_t frame = 0; frame < samples.size() / 2; ++frame) {
        require(samples[frame * 2] == 0.0F,
                "right-channel EQ signal leaked into the left state");
    }
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_default_is_neutral_and_has_project_memory_bound();
        test_each_peaking_band_boosts_its_center();
        test_per_band_bypass_skips_filter_and_preserves_other_bands();
        test_cut_slopes_change_rolloff_with_fixed_sections();
        test_bounds_and_nonfinite_controls_are_sanitized();
        test_stereo_channels_are_isolated_and_processing_does_not_allocate();
        std::cout << "All Stadium seven-band PEQ tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        count_allocations.store(false, std::memory_order_relaxed);
        std::cerr << "Stadium seven-band PEQ test failed: " << error.what() << '\n';
        return 1;
    }
}
