#include "tabletfx/effects/formant_filter.hpp"

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
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> tone(float sample_rate, float frequency,
                        std::size_t frames) {
    std::vector<float> audio(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        audio[frame] = 0.2F * std::sin(
            2.0F * 3.14159265358979323846F * frequency *
            static_cast<float>(frame) / sample_rate);
    }
    return audio;
}

float rms(std::span<const float> audio, std::size_t begin) {
    double sum = 0.0;
    for (std::size_t i = begin; i < audio.size(); ++i) {
        const double sample = audio[i];
        sum += sample * sample;
    }
    return begin == audio.size() ? 0.0F : static_cast<float>(std::sqrt(
        sum / static_cast<double>(audio.size() - begin)));
}

void test_defaults_bounds_and_memory() {
    tabletfx::FormantFilter filter;
    const auto defaults = filter.parameters();
    require(filter.id() == "formant-filter", "stable effect ID changed");
    require(defaults.vowel_position == 0.0F && defaults.resonance_q == 4.0F &&
                defaults.mix == 1.0F && defaults.level_db == 0.0F,
            "project-authored defaults changed");
    require(filter.memory_bytes() == sizeof(filter),
            "fixed-state memory report is incorrect");

    tabletfx::FormantFilterParameters parameters;
    parameters.vowel_position = 100.0F;
    parameters.resonance_q = 100.0F;
    parameters.mix = -1.0F;
    parameters.level_db = 30.0F;
    filter.set_parameters(parameters);
    auto actual = filter.parameters();
    require(actual.vowel_position == 4.0F && actual.resonance_q == 12.0F &&
                actual.mix == 0.0F && actual.level_db == 12.0F,
            "upper parameter bounds were not enforced");

    parameters.vowel_position = -2.0F;
    parameters.resonance_q = 0.0F;
    parameters.mix = 2.0F;
    parameters.level_db = -40.0F;
    filter.set_parameters(parameters);
    actual = filter.parameters();
    require(actual.vowel_position == 0.0F && actual.resonance_q == 0.5F &&
                actual.mix == 1.0F && actual.level_db == -24.0F,
            "lower parameter bounds were not enforced");

    parameters.vowel_position = std::numeric_limits<float>::quiet_NaN();
    parameters.resonance_q = std::numeric_limits<float>::infinity();
    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    parameters.level_db = -std::numeric_limits<float>::infinity();
    filter.set_parameters(parameters);
    actual = filter.parameters();
    require(actual.vowel_position == 0.0F && actual.resonance_q == 4.0F &&
                actual.mix == 1.0F && actual.level_db == 0.0F,
            "non-finite controls did not fall back safely");

    tabletfx::FormantFilter low_rate(8000.0F);
    require(low_rate.memory_bytes() == sizeof(low_rate),
            "sample rate changed fixed memory usage");
}

void test_formant_profiles_change_spectral_response() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 48000U;
    constexpr std::size_t skip = 12000U;
    auto low = tone(sample_rate, 430.0F, frames);
    auto high = low;
    tabletfx::FormantFilterParameters parameters;
    parameters.vowel_position = 0.0F; // A profile has a lower first formant.
    tabletfx::FormantFilter a_filter(sample_rate, parameters);
    a_filter.process(low, 1);
    parameters.vowel_position = 2.0F; // I profile shifts the first two peaks.
    tabletfx::FormantFilter i_filter(sample_rate, parameters);
    i_filter.process(high, 1);
    const float a_response = rms(low, skip);
    const float i_response = rms(high, skip);
    require(std::isfinite(a_response) && std::isfinite(i_response) &&
                std::abs(a_response - i_response) > 0.001F,
            "moving the vowel profile did not change the filter response");
}

void test_mix_level_reset_and_stereo_isolation() {
    tabletfx::FormantFilterParameters parameters;
    parameters.mix = 0.0F;
    tabletfx::FormantFilter dry_filter(48000.0F, parameters);
    std::array<float, 8> dry{0.1F, -0.2F, 0.4F, -0.8F,
                             1.2F, 0.3F, -0.1F, 0.7F};
    const auto original = dry;
    dry_filter.process(dry, 2);
    require(dry == original, "zero mix and unity level must pass audio exactly");

    parameters.mix = 0.0F;
    parameters.level_db = 6.0F;
    tabletfx::FormantFilter gain_filter(48000.0F, parameters);
    std::array<float, 4800> gain_audio{};
    gain_audio.fill(0.1F);
    gain_filter.process(gain_audio, 1);
    require(gain_audio.back() > 0.19F && gain_audio.back() < 0.21F,
            "output level did not follow the configured dB gain");

    tabletfx::FormantFilter stereo;
    std::array<float, 2048> channels{};
    for (std::size_t frame = 0; frame < channels.size() / 2U; ++frame) {
        channels[frame * 2U + 1U] = 0.1F * std::sin(
            2.0F * 3.14159265358979323846F * 400.0F *
            static_cast<float>(frame) / 48000.0F);
    }
    stereo.process(channels, 2);
    for (std::size_t frame = 0; frame < channels.size() / 2U; ++frame) {
        require(channels[frame * 2U] == 0.0F,
                "right-channel state leaked into the left channel");
    }

    std::array<float, 32> first{};
    first[0] = 0.5F;
    std::array<float, 32> second = first;
    tabletfx::FormantFilter reset_filter;
    reset_filter.process(first, 1);
    reset_filter.reset();
    reset_filter.process(second, 1);
    require(first == second, "reset did not clear filter history deterministically");
}

void test_smoothing_block_invariance_invalid_samples_and_layout() {
    constexpr std::size_t frames = 1200U;
    tabletfx::FormantFilter whole;
    tabletfx::FormantFilter split;
    tabletfx::FormantFilterParameters parameters;
    parameters.vowel_position = 3.5F;
    parameters.resonance_q = 8.0F;
    parameters.mix = 0.7F;
    parameters.level_db = -2.0F;
    whole.set_parameters(parameters);
    split.set_parameters(parameters);
    auto audio = tone(48000.0F, 700.0F, frames);
    auto divided = audio;
    whole.process(audio, 1);
    std::size_t offset = 0U;
    for (const std::size_t block : std::array<std::size_t, 4>{31U, 17U, 512U, 640U}) {
        split.process(std::span<float>(divided.data() + offset, block), 1);
        offset += block;
    }
    require(offset == frames, "block fixture did not cover all samples");
    for (std::size_t i = 0; i < frames; ++i) {
        require(std::abs(audio[i] - divided[i]) < 1.0e-6F,
                "parameter smoothing depends on block partition");
    }

    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
        0.1F, 100000.0F, -100000.0F, 0.2F, -0.2F,
    };
    whole.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(), [](float sample) {
                return std::isfinite(sample);
            }), "invalid or extreme input poisoned filter state");

    std::array<float, 6> unsupported{0.1F, -0.2F, 0.3F, -0.4F, 0.5F, -0.6F};
    const auto before = unsupported;
    whole.process(unsupported, 3);
    require(unsupported == before, "unsupported layout must remain untouched");
}

void test_process_does_not_allocate() {
    tabletfx::FormantFilter filter;
    std::array<float, 256> audio{};
    const std::size_t before = allocation_count.load();
    for (int i = 0; i < 20; ++i) filter.process(audio, 2);
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
        test_formant_profiles_change_spectral_response();
        test_mix_level_reset_and_stereo_isolation();
        test_smoothing_block_invariance_invalid_samples_and_layout();
        test_process_does_not_allocate();
        std::cout << "Formant Filter tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Formant Filter test failure: " << error.what() << '\n';
        return 1;
    }
}
