#include "tabletfx/effects/five_band_eq.hpp"

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
        const double value = samples[frame * channels + channel];
        sum += value * value;
    }
    return static_cast<float>(std::sqrt(sum / (frames - frames / 2)));
}

void test_neutral_is_exact_passthrough_and_identifies_effect() {
    tabletfx::FiveBandParametricEQ eq;
    require(eq.id() == "five-band-parametric-eq", "unexpected effect id");
    std::vector<float> samples{0.1F, -0.2F, 0.3F, -0.4F, 1.0F, -1.0F};
    const auto original = samples;
    eq.process(samples, 2);
    require(samples == original, "neutral five-band EQ must preserve samples exactly");
}

void test_each_band_boosts_its_center_frequency() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 48000;
    constexpr std::array<float, 5> centers{100.0F, 350.0F, 1000.0F, 3500.0F, 8000.0F};
    for (std::size_t active = 0; active < centers.size(); ++active) {
        tabletfx::FiveBandParametricEQ eq(kSampleRate, 1);
        auto bands = eq.parameters();
        bands[active].frequency_hz = centers[active];
        bands[active].gain_db = 9.0F;
        bands[active].q = 1.0F;
        eq.configure(bands);
        auto tone = make_sine(kSampleRate, centers[active], kFrames);
        eq.process(tone, 1);
        require(tail_rms(tone, 1) > 0.4F,
                "peaking EQ band did not boost its center-frequency tone");
    }
}

void test_each_band_uses_safe_project_bounds() {
    tabletfx::FiveBandParametricEQ eq(48000.0F, 1);
    auto bands = eq.parameters();
    bands[0] = {9000.0F, 100.0F, 0.01F};
    bands[1] = {50.0F, -100.0F, 20.0F};
    bands[2] = {500.0F, 0.0F, 0.707F};
    bands[3] = {50000.0F, 0.0F, 0.707F};
    bands[4] = {10.0F, 0.0F, 0.707F};
    eq.configure(bands);
    const auto actual = eq.parameters();
    require(actual[0].frequency_hz == 2000.0F && actual[0].gain_db == 18.0F &&
                actual[0].q == 0.25F,
            "band 1 settings were not clamped to project bounds");
    require(actual[1].frequency_hz == 100.0F && actual[1].gain_db == -18.0F &&
                actual[1].q == 10.0F,
            "band 2 settings were not clamped to project bounds");
    require(actual[3].frequency_hz == 10000.0F && actual[4].frequency_hz == 200.0F,
            "band-specific frequency ranges were not applied");

    bands = actual;
    bands[2].frequency_hz = std::numeric_limits<float>::quiet_NaN();
    bands[2].gain_db = std::numeric_limits<float>::infinity();
    bands[2].q = -std::numeric_limits<float>::infinity();
    eq.configure(bands);
    const auto safe = eq.parameters();
    require(safe[2].frequency_hz == 1000.0F && safe[2].gain_db == 0.0F &&
                safe[2].q == 0.707F,
            "non-finite controls did not restore documented project defaults");
}

void test_shelf_shapes_boost_only_their_side_of_the_spectrum() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 48000;
    {
        tabletfx::FiveBandParametricEQ eq(kSampleRate, 1);
        auto bands = eq.parameters();
        bands[0].shape = tabletfx::FiveBandEQShape::LowShelf;
        bands[0].frequency_hz = 500.0F;
        bands[0].gain_db = 9.0F;
        eq.configure(bands);
        auto low = make_sine(kSampleRate, 100.0F, kFrames);
        auto high = make_sine(kSampleRate, 8000.0F, kFrames);
        eq.process(low, 1);
        eq.reset();
        eq.process(high, 1);
        require(tail_rms(low, 1) > 0.45F,
                "low shelf did not boost the low-frequency test tone");
        require(tail_rms(high, 1) > 0.16F && tail_rms(high, 1) < 0.21F,
                "low shelf changed the high-frequency passband unexpectedly");
    }
    {
        tabletfx::FiveBandParametricEQ eq(kSampleRate, 1);
        auto bands = eq.parameters();
        bands[4].shape = tabletfx::FiveBandEQShape::HighShelf;
        bands[4].frequency_hz = 2000.0F;
        bands[4].gain_db = 9.0F;
        eq.configure(bands);
        auto low = make_sine(kSampleRate, 100.0F, kFrames);
        auto high = make_sine(kSampleRate, 8000.0F, kFrames);
        eq.process(low, 1);
        eq.reset();
        eq.process(high, 1);
        require(tail_rms(low, 1) > 0.16F && tail_rms(low, 1) < 0.21F,
                "high shelf changed the low-frequency passband unexpectedly");
        require(tail_rms(high, 1) > 0.45F,
                "high shelf did not boost the high-frequency test tone");
    }
}

void test_outer_band_blocking_slope_controls_cut_response() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 48000;
    {
        tabletfx::FiveBandParametricEQ eq(kSampleRate, 1);
        auto bands = eq.parameters();
        bands[4].shape = tabletfx::FiveBandEQShape::HighCut;
        bands[4].frequency_hz = 1000.0F;
        bands[4].slope_db_per_octave = 36.0F;
        eq.configure(bands);
        auto below = make_sine(kSampleRate, 500.0F, kFrames);
        auto above = make_sine(kSampleRate, 2000.0F, kFrames);
        eq.process(below, 1);
        eq.reset();
        eq.process(above, 1);
        require(tail_rms(below, 1) > 0.14F,
                "high-cut filter removed its lower passband");
        require(tail_rms(above, 1) < 0.01F,
                "36 dB/oct high-cut filter did not sufficiently attenuate one octave above cutoff");
    }
    {
        tabletfx::FiveBandParametricEQ eq(kSampleRate, 1);
        auto bands = eq.parameters();
        bands[0].shape = tabletfx::FiveBandEQShape::LowCut;
        bands[0].frequency_hz = 1000.0F;
        bands[0].slope_db_per_octave = 24.0F;
        eq.configure(bands);
        auto below = make_sine(kSampleRate, 500.0F, kFrames);
        auto above = make_sine(kSampleRate, 2000.0F, kFrames);
        eq.process(below, 1);
        eq.reset();
        eq.process(above, 1);
        require(tail_rms(below, 1) < 0.04F,
                "24 dB/oct low-cut filter did not attenuate one octave below cutoff");
        require(tail_rms(above, 1) > 0.14F,
                "low-cut filter removed its upper passband");
    }
}

void test_shapes_are_limited_to_project_supported_bands() {
    tabletfx::FiveBandParametricEQ eq;
    auto bands = eq.parameters();
    bands[0].shape = tabletfx::FiveBandEQShape::Peaking2;
    bands[1].shape = tabletfx::FiveBandEQShape::LowShelf2;
    bands[3].shape = tabletfx::FiveBandEQShape::HighShelf2;
    bands[4].shape = tabletfx::FiveBandEQShape::Peaking2;
    bands[1].shape = tabletfx::FiveBandEQShape::HighCut;
    bands[2].shape = tabletfx::FiveBandEQShape::LowShelf;
    bands[4].slope_db_per_octave = 100.0F;
    eq.configure(bands);
    const auto actual = eq.parameters();
    require(actual[1].shape == tabletfx::FiveBandEQShape::Peaking &&
                actual[2].shape == tabletfx::FiveBandEQShape::Peaking &&
                actual[0].shape == tabletfx::FiveBandEQShape::Peaking2 &&
                actual[3].shape == tabletfx::FiveBandEQShape::HighShelf2 &&
                actual[4].shape == tabletfx::FiveBandEQShape::Peaking2,
            "unsupported outer-band filter shapes were not reset to peaking");
    require(actual[4].slope_db_per_octave == 36.0F,
            "blocking slope was not clamped to the project maximum");

    bands = actual;
    bands[4].slope_db_per_octave = 8.0F;
    eq.configure(bands);
    require(eq.parameters()[4].slope_db_per_octave == 6.0F,
            "blocking slope was not quantized to 6 dB/octave steps");
}

void test_project_peaking2_has_distinct_low_q_response() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 48000;
    tabletfx::FiveBandParametricEQ standard(kSampleRate, 1);
    tabletfx::FiveBandParametricEQ alternate(kSampleRate, 1);
    auto standard_bands = standard.parameters();
    auto alternate_bands = alternate.parameters();
    standard_bands[2] = {1000.0F, 9.0F, 0.25F};
    alternate_bands[2] = {1000.0F, 9.0F, 0.25F,
                          tabletfx::FiveBandEQShape::Peaking2};
    standard.configure(standard_bands);
    alternate.configure(alternate_bands);
    auto standard_tone = make_sine(kSampleRate, 700.0F, kFrames);
    auto alternate_tone = standard_tone;
    standard.process(standard_tone, 1);
    alternate.process(alternate_tone, 1);
    require(std::abs(tail_rms(standard_tone, 1) -
                     tail_rms(alternate_tone, 1)) > 0.01F,
            "project Peaking 2 did not distinguish the low-Q response");
}

void test_project_shelving2_has_cutoff_overshoot() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 48000;
    tabletfx::FiveBandParametricEQ standard(kSampleRate, 1);
    tabletfx::FiveBandParametricEQ alternate(kSampleRate, 1);
    auto standard_bands = standard.parameters();
    auto alternate_bands = alternate.parameters();
    standard_bands[0] = {200.0F, 9.0F, 0.707F,
                         tabletfx::FiveBandEQShape::LowShelf};
    alternate_bands[0] = {200.0F, 9.0F, 0.707F,
                          tabletfx::FiveBandEQShape::LowShelf2};
    standard.configure(standard_bands);
    alternate.configure(alternate_bands);
    auto standard_tone = make_sine(kSampleRate, 200.0F, kFrames);
    auto alternate_tone = standard_tone;
    standard.process(standard_tone, 1);
    alternate.process(alternate_tone, 1);
    require(tail_rms(alternate_tone, 1) > tail_rms(standard_tone, 1),
            "project passive shelf did not add cutoff overshoot");
}

void test_solo_band_selects_one_frequency_region() {
    constexpr float kSampleRate = 48000.0F;
    constexpr std::size_t kFrames = 48000;
    tabletfx::FiveBandParametricEQ eq(kSampleRate, 1);
    auto bands = eq.parameters();
    bands[2].frequency_hz = 1000.0F;
    bands[2].q = 2.0F;
    eq.configure(bands);
    auto low = make_sine(kSampleRate, 100.0F, kFrames);
    auto center = make_sine(kSampleRate, 1000.0F, kFrames);
    auto high = make_sine(kSampleRate, 8000.0F, kFrames);
    std::vector<float> mixed(kFrames);
    for (std::size_t index = 0; index < mixed.size(); ++index) {
        mixed[index] = low[index] + center[index] + high[index];
    }
    eq.set_solo_band(3);
    eq.process(mixed, 1);
    require(tail_rms(mixed, 1) > 0.08F,
            "solo band removed the selected center-frequency signal");
}

void test_stereo_state_is_independent_and_nonfinite_input_is_contained() {
    tabletfx::FiveBandParametricEQ eq(48000.0F, 2);
    auto bands = eq.parameters();
    bands[2].gain_db = 12.0F;
    eq.configure(bands);
    auto samples = make_sine(48000.0F, bands[2].frequency_hz, 4096, 2, 1);
    eq.process(samples, 2);
    for (std::size_t frame = 0; frame < samples.size() / 2; ++frame) {
        require(samples[frame * 2] == 0.0F,
                "right-channel signal leaked into the left EQ state");
    }
    require(tail_rms(samples, 2, 1) > 0.2F,
            "right-channel EQ output is unexpectedly silent");

    std::vector<float> invalid{std::numeric_limits<float>::quiet_NaN(), 0.25F,
                               std::numeric_limits<float>::infinity(), -0.5F};
    eq.process(invalid, 2);
    for (float sample : invalid) {
        require(std::isfinite(sample), "EQ did not contain a non-finite input");
    }
}

void test_processing_allocates_nothing() {
    tabletfx::FiveBandParametricEQ eq;
    std::vector<float> samples(512, 0.125F);
    const auto before = allocation_count.load(std::memory_order_relaxed);
    eq.process(samples, 2);
    const auto after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "FiveBandParametricEQ allocated during process()");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_neutral_is_exact_passthrough_and_identifies_effect();
        test_each_band_boosts_its_center_frequency();
        test_each_band_uses_safe_project_bounds();
        test_shelf_shapes_boost_only_their_side_of_the_spectrum();
        test_outer_band_blocking_slope_controls_cut_response();
        test_shapes_are_limited_to_project_supported_bands();
        test_project_peaking2_has_distinct_low_q_response();
        test_project_shelving2_has_cutoff_overshoot();
        test_solo_band_selects_one_frequency_region();
        test_stereo_state_is_independent_and_nonfinite_input_is_contained();
        test_processing_allocates_nothing();
        std::cout << "FiveBandEQ tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
