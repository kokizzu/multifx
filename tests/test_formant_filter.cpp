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
                defaults.mix == 1.0F && defaults.level_db == 0.0F &&
                !defaults.control_lfo_enabled &&
                defaults.control_lfo_waveform ==
                    tabletfx::FormantControlLfoWaveform::sine &&
                defaults.control_lfo_rate_hz == 0.5F &&
                defaults.control_lfo_min_percent == 0.0F &&
                defaults.control_lfo_max_percent == 100.0F,
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

    parameters.control_lfo_rate_hz = 100.0F;
    parameters.control_lfo_min_percent = -20.0F;
    parameters.control_lfo_max_percent = 150.0F;
    parameters.control_lfo_waveform =
        static_cast<tabletfx::FormantControlLfoWaveform>(255U);
    filter.set_parameters(parameters);
    actual = filter.parameters();
    require(actual.control_lfo_rate_hz == 8.0F &&
                actual.control_lfo_min_percent == 0.0F &&
                actual.control_lfo_max_percent == 100.0F &&
                actual.control_lfo_waveform ==
                    tabletfx::FormantControlLfoWaveform::sine,
            "Control LFO bounds and invalid waveform were not sanitized");

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

void test_three_vowel_morph_changes_profiles_and_clamps_controls() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 48000U;
    constexpr std::size_t skip = 12000U;
    auto start_audio = tone(sample_rate, 430.0F, frames);
    auto middle_audio = start_audio;
    auto end_audio = start_audio;

    tabletfx::FormantFilterParameters parameters;
    parameters.three_vowel_morph = true;
    parameters.start_vowel = tabletfx::FormantVowel::aaa;
    parameters.middle_vowel = tabletfx::FormantVowel::iii;
    parameters.end_vowel = tabletfx::FormantVowel::ooo;
    parameters.control_percent = 0.0F;
    tabletfx::FormantFilter start_filter(sample_rate, parameters);
    start_filter.process(start_audio, 1);

    parameters.control_percent = 50.0F;
    tabletfx::FormantFilter middle_filter(sample_rate, parameters);
    middle_filter.process(middle_audio, 1);

    parameters.control_percent = 100.0F;
    tabletfx::FormantFilter end_filter(sample_rate, parameters);
    end_filter.process(end_audio, 1);

    const float start_response = rms(start_audio, skip);
    const float middle_response = rms(middle_audio, skip);
    const float end_response = rms(end_audio, skip);
    require(std::isfinite(start_response) && std::isfinite(middle_response) &&
                std::isfinite(end_response) &&
                std::abs(start_response - middle_response) > 0.001F &&
                std::abs(middle_response - end_response) > 0.001F,
            "Start, Middle, and End morph positions must select distinct project profiles");

    parameters.start_vowel = static_cast<tabletfx::FormantVowel>(255U);
    parameters.middle_vowel = tabletfx::FormantVowel::err;
    parameters.end_vowel = tabletfx::FormantVowel::ahh;
    parameters.control_percent = 150.0F;
    parameters.balance_percent = -150.0F;
    tabletfx::FormantFilter clamp_filter(sample_rate, parameters);
    const auto clamped = clamp_filter.parameters();
    require(clamped.start_vowel == tabletfx::FormantVowel::aaa &&
                clamped.middle_vowel == tabletfx::FormantVowel::err &&
                clamped.end_vowel == tabletfx::FormantVowel::ahh &&
                clamped.control_percent == 100.0F &&
                clamped.balance_percent == -100.0F,
            "morph selections, Control, and Balance must stay in the project range");
}

void test_control_lfo_moves_three_vowel_morph() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 24000U;
    tabletfx::FormantFilterParameters parameters;
    parameters.three_vowel_morph = true;
    parameters.start_vowel = tabletfx::FormantVowel::aaa;
    parameters.middle_vowel = tabletfx::FormantVowel::iii;
    parameters.end_vowel = tabletfx::FormantVowel::ooo;
    parameters.control_lfo_enabled = true;
    parameters.control_lfo_waveform =
        tabletfx::FormantControlLfoWaveform::triangle;
    parameters.control_lfo_rate_hz = 2.0F;
    parameters.control_lfo_min_percent = 0.0F;
    parameters.control_lfo_max_percent = 100.0F;

    auto audio = tone(sample_rate, 430.0F, frames);
    tabletfx::FormantFilter filter(sample_rate, parameters);
    filter.process(audio, 1);
    const float start_response = rms(
        std::span<const float>(audio.data(), 2400U), 0U);

    auto middle_audio = tone(sample_rate, 430.0F, frames);
    tabletfx::FormantFilter middle_filter(sample_rate, parameters);
    middle_filter.process(std::span<float>(middle_audio.data(), 6000U), 1);
    const float middle_response = rms(
        std::span<const float>(middle_audio.data(), 6000U), 4800U);

    require(std::isfinite(start_response) && std::isfinite(middle_response) &&
                std::abs(start_response - middle_response) > 0.001F,
            "the Control LFO did not move the three-vowel filter response");

    auto split_audio = tone(sample_rate, 430.0F, frames);
    tabletfx::FormantFilter split_filter(sample_rate, parameters);
    std::size_t offset = 0U;
    while (offset < split_audio.size()) {
        const std::size_t count = std::min<std::size_t>(127U,
                                                        split_audio.size() - offset);
        split_filter.process(
            std::span<float>(split_audio.data() + offset, count), 1);
        offset += count;
    }
    for (std::size_t i = 0; i < frames; ++i) {
        require(std::abs(audio[i] - split_audio[i]) < 1.0e-6F,
                "Control LFO output depends on audio callback partitioning");
    }
}

void test_stereo_balance_is_smoothed_and_mono_safe() {
    tabletfx::FormantFilterParameters parameters;
    parameters.mix = 0.0F;
    parameters.balance_percent = 100.0F;
    tabletfx::FormantFilter filter(48000.0F, parameters);
    std::array<float, 9600> stereo{};
    for (std::size_t i = 0; i < stereo.size(); i += 2U) {
        stereo[i] = 0.25F;
        stereo[i + 1U] = 0.25F;
    }
    filter.process(stereo, 2);
    require(std::abs(stereo[stereo.size() - 2U]) < 1.0e-6F &&
                std::abs(stereo.back() - 0.25F) < 1.0e-6F,
            "full-right project balance must attenuate only the left output");

    std::array<float, 64> mono{};
    mono.fill(0.25F);
    tabletfx::FormantFilter mono_filter(48000.0F, parameters);
    mono_filter.process(mono, 1);
    require(std::all_of(mono.begin(), mono.end(), [](float sample) {
                return std::abs(sample - 0.25F) < 1.0e-6F;
            }), "stereo balance must not alter mono audio");
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
    tabletfx::FormantFilterParameters parameters;
    parameters.three_vowel_morph = true;
    parameters.control_lfo_enabled = true;
    parameters.control_lfo_waveform =
        tabletfx::FormantControlLfoWaveform::triangle;
    tabletfx::FormantFilter filter(48000.0F, parameters);
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
        test_three_vowel_morph_changes_profiles_and_clamps_controls();
        test_control_lfo_moves_three_vowel_morph();
        test_stereo_balance_is_smoothed_and_mono_safe();
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
