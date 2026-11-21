#include "tabletfx/effects/harmonic_resonator.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstdint>
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

std::vector<float> sine(float frequency_hz, std::size_t frames,
                        std::uint16_t channels = 1,
                        std::uint16_t active_channel = 0) {
    constexpr float sample_rate = 48000.0F;
    std::vector<float> audio(frames * channels, 0.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float phase = 2.0F * 3.14159265358979323846F * frequency_hz *
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

void test_defaults_bounds_and_memory() {
    tabletfx::HarmonicResonator resonator;
    const auto defaults = resonator.parameters();
    require(resonator.id() == "harmonic-resonator",
            "stable effect ID changed");
    require(defaults.root_frequency_hz == 110.0F &&
                defaults.decay_seconds == 1.2F &&
                defaults.brightness == 0.55F && defaults.mix == 0.5F,
            "project-authored defaults changed");
    require(resonator.memory_bytes() == sizeof(resonator),
            "fixed-state memory report is inaccurate");

    tabletfx::HarmonicResonatorParameters p;
    p.root_frequency_hz = 5000.0F;
    p.decay_seconds = 100.0F;
    p.brightness = -1.0F;
    p.mix = 2.0F;
    tabletfx::HarmonicResonator bounded(8000.0F, p);
    auto result = bounded.parameters();
    require(result.root_frequency_hz == 450.0F &&
                result.decay_seconds == 8.0F && result.brightness == 0.0F &&
                result.mix == 1.0F,
            "project bounds or sample-rate-aware root limit failed");

    p.root_frequency_hz = std::numeric_limits<float>::quiet_NaN();
    p.decay_seconds = std::numeric_limits<float>::infinity();
    p.brightness = std::numeric_limits<float>::quiet_NaN();
    p.mix = std::numeric_limits<float>::quiet_NaN();
    bounded.set_parameters(p);
    result = bounded.parameters();
    require(result.root_frequency_hz == 110.0F &&
                result.decay_seconds == 1.2F && result.brightness == 0.55F &&
                result.mix == 0.5F,
            "non-finite controls did not use project defaults");
}

void test_harmonic_resonance_and_brightness() {
    tabletfx::HarmonicResonatorParameters p;
    p.root_frequency_hz = 110.0F;
    p.decay_seconds = 1.0F;
    p.brightness = 0.55F;
    p.mix = 1.0F;
    tabletfx::HarmonicResonator on_frequency(48000.0F, p);
    tabletfx::HarmonicResonator off_frequency(48000.0F, p);
    auto on = sine(110.0F, 24000U);
    auto off = sine(150.0F, 24000U);
    on_frequency.process(on, 1);
    off_frequency.process(off, 1);
    require(rms(on, 1) > rms(off, 1) * 2.0F,
            "root-frequency mode did not respond more strongly than an off-note");

    tabletfx::HarmonicResonator dark(48000.0F, p);
    p.brightness = 1.0F;
    tabletfx::HarmonicResonator bright(48000.0F, p);
    auto harmonic = sine(440.0F, 24000U);
    auto dark_audio = harmonic;
    auto bright_audio = harmonic;
    dark.process(dark_audio, 1);
    bright.process(bright_audio, 1);
    require(rms(bright_audio, 1) > rms(dark_audio, 1),
            "brightness did not increase the upper-harmonic contribution");
}

void test_mix_channels_invalid_samples_and_reset() {
    tabletfx::HarmonicResonatorParameters p;
    p.mix = 0.0F;
    tabletfx::HarmonicResonator dry(48000.0F, p);
    std::vector<float> audio{0.1F, -0.2F, 0.4F, -0.8F, 1.2F, 0.3F};
    const auto original = audio;
    dry.process(audio, 1);
    require(audio == original, "zero mix changed the exact dry signal");

    std::vector<float> large_audio{32.0F, -64.0F, 1.0e10F};
    const auto large_original = large_audio;
    dry.process(large_audio, 1);
    require(large_audio == large_original,
            "zero mix must preserve large finite dry samples exactly");

    p.mix = 1.0F;
    tabletfx::HarmonicResonator stereo(48000.0F, p);
    auto right_only = sine(220.0F, 12000U, 2U, 1U);
    stereo.process(right_only, 2);
    for (std::size_t frame = 0; frame < right_only.size() / 2U; ++frame) {
        require(right_only[frame * 2U] == 0.0F,
                "right-channel resonator state leaked into left channel");
    }
    require(std::all_of(right_only.begin(), right_only.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "resonator produced non-finite stereo output");

    std::vector<float> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.25F,
    };
    stereo.process(invalid, 1);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "invalid samples were not contained");

    tabletfx::HarmonicResonator first(48000.0F, p);
    tabletfx::HarmonicResonator second(48000.0F, p);
    std::vector<float> input(1024U, 0.0F);
    input[0] = 0.75F;
    auto output_a = input;
    auto output_b = input;
    first.process(output_a, 1);
    first.reset();
    second.process(output_b, 1);
    require(output_a == output_b,
            "reset did not make the resonator response deterministic");
}

void test_parameter_change_remains_finite_and_block_invariant() {
    constexpr std::size_t frames = 2400U;
    tabletfx::HarmonicResonator whole;
    tabletfx::HarmonicResonator split;
    auto input = sine(330.0F, frames);
    auto whole_audio = input;
    auto split_audio = input;
    tabletfx::HarmonicResonatorParameters p;
    p.root_frequency_hz = 165.0F;
    p.decay_seconds = 4.0F;
    p.brightness = 0.9F;
    p.mix = 0.8F;
    whole.set_parameters(p);
    split.set_parameters(p);
    whole.process(whole_audio, 1);
    for (std::size_t offset = 0; offset < split_audio.size();) {
        const std::size_t count = std::min<std::size_t>(79U,
                                                        split_audio.size() - offset);
        split.process(std::span<float>(split_audio).subspan(offset, count), 1);
        offset += count;
    }
    require(std::all_of(whole_audio.begin(), whole_audio.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "coefficient ramp produced non-finite output");
    require(std::equal(whole_audio.begin(), whole_audio.end(), split_audio.begin(),
                       [](float a, float b) { return std::abs(a - b) < 1.0e-6F; }),
            "processing changed across block boundaries");
}

void test_process_does_not_allocate() {
    tabletfx::HarmonicResonator resonator;
    std::array<float, 256> audio{};
    const std::size_t before = allocation_count.load();
    for (int i = 0; i < 20; ++i) resonator.process(audio, 2);
    if (allocation_count.load() != before) {
        throw std::runtime_error("process() allocated memory");
    }
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1U);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_bounds_and_memory();
        test_harmonic_resonance_and_brightness();
        test_mix_channels_invalid_samples_and_reset();
        test_parameter_change_remains_finite_and_block_invariant();
        test_process_does_not_allocate();
        std::cout << "harmonic-resonator tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "harmonic-resonator test failure: " << error.what() << '\n';
        return 1;
    }
}
