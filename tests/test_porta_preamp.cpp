#include "tabletfx/effects/porta_preamp.hpp"

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

std::atomic<bool> watch_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

float rms(const std::vector<float>& samples, std::size_t skip = 0U) {
    double sum = 0.0;
    for (std::size_t i = skip; i < samples.size(); ++i) {
        const double value = samples[i];
        sum += value * value;
    }
    return static_cast<float>(std::sqrt(
        sum / static_cast<double>(samples.size() - skip)));
}

std::vector<float> sine(float frequency_hz, float amplitude = 0.05F) {
    constexpr float sample_rate = 48000.0F;
    constexpr float two_pi = 6.28318530717958647692F;
    std::vector<float> samples(48000U);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = amplitude * std::sin(
            two_pi * frequency_hz * static_cast<float>(i) / sample_rate);
    }
    return samples;
}

} // namespace

void* operator new(std::size_t size) {
    if (watch_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    if (watch_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

void test_defaults_bounds_and_memory() {
    tabletfx::PortaPreamp preamp;
    const auto defaults = preamp.parameters();
    require(preamp.id() == "porta-preamp", "stable preamp ID changed");
    require(defaults.gain_1_db == 6.0F && defaults.gain_2_db == 3.0F &&
                defaults.bass_db == 0.0F && defaults.treble_db == 0.0F &&
                defaults.level_db == 0.0F && defaults.smoothing_ms == 10.0F,
            "project-authored preamp defaults changed");
    require(preamp.memory_bytes() == sizeof(preamp),
            "preamp memory report is inaccurate");

    tabletfx::PortaPreampParameters parameters;
    parameters.gain_1_db = -100.0F;
    parameters.gain_2_db = 100.0F;
    parameters.bass_db = std::numeric_limits<float>::quiet_NaN();
    parameters.treble_db = std::numeric_limits<float>::infinity();
    parameters.level_db = -100.0F;
    parameters.smoothing_ms = 2000.0F;
    preamp.set_parameters(parameters);
    const auto bounded = preamp.parameters();
    require(bounded.gain_1_db == -12.0F && bounded.gain_2_db == 24.0F &&
                bounded.bass_db == 0.0F && bounded.treble_db == 0.0F &&
                bounded.level_db == -24.0F && bounded.smoothing_ms == 1000.0F,
            "project-authored bounds or non-finite fallbacks failed");
}

void test_two_gain_stages_and_level() {
    tabletfx::PortaPreampParameters unity{};
    unity.gain_1_db = 0.0F;
    unity.gain_2_db = 0.0F;
    unity.level_db = 0.0F;
    unity.smoothing_ms = 0.0F;
    tabletfx::PortaPreamp clean(48000.0F, unity);

    auto driven_parameters = unity;
    driven_parameters.gain_1_db = 18.0F;
    driven_parameters.gain_2_db = 12.0F;
    tabletfx::PortaPreamp driven(48000.0F, driven_parameters);
    auto signal_clean = sine(440.0F, 0.55F);
    auto signal_driven = signal_clean;
    clean.process(signal_clean, 1U);
    driven.process(signal_driven, 1U);
    require(rms(signal_driven, 8000U) > rms(signal_clean, 8000U) * 1.15F,
            "the independent gain stages should increase level before saturation");
    require(std::all_of(signal_driven.begin(), signal_driven.end(), [](float x) {
                return std::isfinite(x) && std::abs(x) <= 8.0F;
            }),
            "strong drive must remain finite and within the processor output ceiling");

    auto level_parameters = unity;
    level_parameters.level_db = 6.0F;
    tabletfx::PortaPreamp raised_level(48000.0F, level_parameters);
    auto baseline = sine(440.0F, 0.1F);
    auto louder = baseline;
    clean.reset();
    clean.process(baseline, 1U);
    raised_level.process(louder, 1U);
    require(std::abs(rms(louder, 8000U) / rms(baseline, 8000U) - 1.9953F) < 0.03F,
            "Level should apply its project-authored decibel gain after tone shaping");
}

void test_bass_and_treble_shelves() {
    tabletfx::PortaPreampParameters flat{};
    flat.gain_1_db = 0.0F;
    flat.gain_2_db = 0.0F;
    flat.smoothing_ms = 0.0F;
    tabletfx::PortaPreamp reference(48000.0F, flat);

    auto bass_parameters = flat;
    bass_parameters.bass_db = 12.0F;
    tabletfx::PortaPreamp bass(48000.0F, bass_parameters);
    auto treble_parameters = flat;
    treble_parameters.treble_db = 12.0F;
    tabletfx::PortaPreamp treble(48000.0F, treble_parameters);

    auto low_flat = sine(80.0F);
    auto low_bass = low_flat;
    auto high_flat = sine(6000.0F);
    auto high_bass = high_flat;
    auto high_treble = high_flat;
    auto low_treble = low_flat;
    reference.process(low_flat, 1U);
    bass.process(low_bass, 1U);
    reference.reset();
    reference.process(high_flat, 1U);
    reference.reset();
    treble.process(high_treble, 1U);
    treble.reset();
    treble.process(low_treble, 1U);
    bass.reset();
    bass.process(high_bass, 1U);

    const float low_bass_ratio = rms(low_bass, 8000U) / rms(low_flat, 8000U);
    const float high_bass_ratio = rms(high_bass, 8000U) / rms(high_flat, 8000U);
    const float high_treble_ratio = rms(high_treble, 8000U) / rms(high_flat, 8000U);
    const float low_treble_ratio = rms(low_treble, 8000U) / rms(low_flat, 8000U);
    require(low_bass_ratio > 2.5F && high_bass_ratio < 1.2F,
            "Bass must act as a low-frequency shelf rather than a full-band gain");
    require(high_treble_ratio > 2.5F && low_treble_ratio < 1.2F,
            "Treble must act as a high-frequency shelf rather than a full-band gain");
}

void test_stereo_independence_partitioning_and_bad_input() {
    tabletfx::PortaPreampParameters parameters{};
    parameters.gain_1_db = 15.0F;
    parameters.gain_2_db = 7.0F;
    parameters.bass_db = 3.0F;
    parameters.treble_db = -2.0F;
    parameters.smoothing_ms = 12.0F;
    tabletfx::PortaPreamp whole(48000.0F, parameters);
    tabletfx::PortaPreamp chunks(48000.0F, parameters);

    std::vector<float> stereo(4096U * 2U);
    constexpr float two_pi = 6.28318530717958647692F;
    for (std::size_t frame = 0; frame < stereo.size() / 2U; ++frame) {
        const float sample = 0.7F * std::sin(two_pi *
            static_cast<float>(frame) / 53.0F);
        stereo[frame * 2U] = sample;
        stereo[frame * 2U + 1U] = -sample * 0.25F;
    }
    auto chunked = stereo;
    whole.process(stereo, 2U);
    std::size_t offset_frames = 0U;
    constexpr std::array<std::size_t, 5> blocks{1U, 17U, 64U, 129U, 257U};
    std::size_t block_index = 0U;
    while (offset_frames < chunked.size() / 2U) {
        const std::size_t count = std::min(
            blocks[block_index++ % blocks.size()],
            chunked.size() / 2U - offset_frames);
        const std::size_t offset = offset_frames * 2U;
        chunks.process(std::span<float>(chunked.data() + offset, count * 2U), 2U);
        offset_frames += count;
    }
    require(stereo == chunked,
            "processing the same stereo signal in irregular blocks must be invariant");
    for (std::size_t frame = 0; frame < stereo.size() / 2U; ++frame) {
        require(std::isfinite(stereo[frame * 2U]) &&
                    std::isfinite(stereo[frame * 2U + 1U]),
                "both stereo channels must stay finite");
    }

    std::array<float, 5> partial{0.1F, 0.2F, 0.3F, 0.4F, 7.0F};
    chunks.process(partial, 2U);
    require(partial[4] == 7.0F,
            "samples after the last complete interleaved frame must be untouched");
    std::array<float, 3> unsupported{1.0F, 2.0F, 3.0F};
    chunks.process(unsupported, 3U);
    require(unsupported == std::array<float, 3>{1.0F, 2.0F, 3.0F},
            "unsupported channel counts must leave input unchanged");

    std::array<float, 4> invalid{0.25F,
        std::numeric_limits<float>::quiet_NaN(), 0.5F,
        std::numeric_limits<float>::infinity()};
    chunks.process(invalid, 2U);
    require(std::all_of(invalid.begin(), invalid.end(), [](float x) {
                return std::isfinite(x);
            }) && invalid[1] == 0.0F && invalid[3] == 0.0F,
            "non-finite input must be silenced without contaminating filter state");
}

void test_parameter_ramp_reset_and_no_audio_thread_allocations() {
    tabletfx::PortaPreamp effect;
    tabletfx::PortaPreamp chunked_effect;
    std::array<float, 4096> audio{};
    for (std::size_t i = 0; i < audio.size(); ++i) {
        audio[i] = 0.4F * std::sin(static_cast<float>(i) * 0.013F);
    }
    auto chunked = audio;
    auto parameters = effect.parameters();
    parameters.gain_1_db = 24.0F;
    parameters.gain_2_db = 24.0F;
    parameters.bass_db = 12.0F;
    parameters.treble_db = 12.0F;
    parameters.level_db = 12.0F;
    parameters.smoothing_ms = 25.0F;
    effect.set_parameters(parameters);
    chunked_effect.set_parameters(parameters);

    allocation_count.store(0U, std::memory_order_relaxed);
    watch_allocations.store(true, std::memory_order_relaxed);
    effect.process(audio, 1U);
    watch_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "preamp process() must not allocate");
    require(std::all_of(audio.begin(), audio.end(), [](float x) {
                return std::isfinite(x) && std::abs(x) <= 8.0F;
            }),
            "maximum controls must remain finite during ramping");
    std::size_t offset = 0U;
    constexpr std::array<std::size_t, 4> blocks{1U, 33U, 128U, 257U};
    std::size_t block_index = 0U;
    while (offset < chunked.size()) {
        const std::size_t count = std::min(
            blocks[block_index++ % blocks.size()], chunked.size() - offset);
        chunked_effect.process(std::span<float>(chunked.data() + offset, count), 1U);
        offset += count;
    }
    require(audio == chunked,
            "smoothed gain/tone/level ramps must be invariant across irregular blocks");

    std::array<float, 256> silence{};
    effect.reset();
    effect.process(silence, 1U);
    require(std::all_of(silence.begin(), silence.end(), [](float x) {
                return x == 0.0F;
            }),
            "reset must clear both tone-filter histories and retain finite parameters");
}

} // namespace

int main() {
    try {
        test_defaults_bounds_and_memory();
        test_two_gain_stages_and_level();
        test_bass_and_treble_shelves();
        test_stereo_independence_partitioning_and_bad_input();
        test_parameter_ramp_reset_and_no_audio_thread_allocations();
        std::cout << "PortaPreamp tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "PortaPreamp tests failed: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
