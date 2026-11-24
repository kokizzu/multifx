#include "tabletfx/effects/note_generator.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

using tabletfx::effects::NoteGenerator;
using tabletfx::effects::NoteGeneratorParameters;
using tabletfx::effects::NoteGeneratorVoiceParameters;
using tabletfx::effects::NoteGeneratorWaveform;
using tabletfx::effects::FourOscGenerator;

#ifdef assert
#undef assert
#endif
#define assert(...)                                                         \
    do {                                                                    \
        if (!static_cast<bool>((__VA_ARGS__))) {                            \
            std::fprintf(stderr, "TEST FAILURE: %s (%s:%d)\n",             \
                         #__VA_ARGS__, __FILE__, __LINE__);                 \
            std::abort();                                                   \
        }                                                                   \
    } while (false)

namespace {

std::atomic<std::size_t> allocation_count{0U};

constexpr float kSampleRate = 48000.0F;
constexpr std::size_t kFrames = 16384U;

float estimate_positive_crossing_hz(const std::array<float, kFrames>& samples) {
    double first_crossing = -1.0;
    double last_crossing = -1.0;
    std::size_t crossings = 0U;
    for (std::size_t i = 1U; i < samples.size(); ++i) {
        const float before = samples[i - 1U];
        const float after = samples[i];
        if (before <= 0.0F && after > 0.0F) {
            const double fraction = -static_cast<double>(before) /
                                    static_cast<double>(after - before);
            const double crossing = static_cast<double>(i - 1U) + fraction;
            if (crossings == 0U) {
                first_crossing = crossing;
            }
            last_crossing = crossing;
            ++crossings;
        }
    }
    if (crossings < 2U || !(last_crossing > first_crossing)) {
        return 0.0F;
    }
    return static_cast<float>(static_cast<double>(crossings - 1U) * kSampleRate /
                              (last_crossing - first_crossing));
}

NoteGeneratorParameters single_tone(NoteGeneratorWaveform waveform,
                                    int note_number = 69) {
    NoteGeneratorParameters parameters;
    parameters.voice_count = 1U;
    parameters.output_level = 1.0F;
    parameters.voices[0] = {waveform, note_number, 0, 0.8F};
    return parameters;
}

void test_defaults_bounds_and_small_fixed_state() {
    NoteGenerator generator;
    const auto& defaults = generator.parameters();
    assert(generator.id() == "note-generator");
    assert(defaults.voice_count == 1U);
    assert(defaults.voices[0].waveform == NoteGeneratorWaveform::Sine);
    assert(defaults.voices[0].note_number == 60);
    assert(defaults.voices[0].octave == 0);
    assert(defaults.voices[0].level == 0.5F);
    assert(defaults.output_level == 0.5F);
    assert(generator.memory_bytes() == sizeof(generator));
    assert(generator.memory_bytes() < 256U);
    assert(std::abs(generator.voice_frequency_hz(0U) - 261.6256F) < 0.02F);
    assert(generator.voice_frequency_hz(4U) == 0.0F);
}

void test_waveforms_and_note_frequency() {
    const std::array<NoteGeneratorWaveform, 4> waveforms{
        NoteGeneratorWaveform::Sine,
        NoteGeneratorWaveform::Triangle,
        NoteGeneratorWaveform::Sawtooth,
        NoteGeneratorWaveform::Square,
    };

    for (const auto waveform : waveforms) {
        NoteGenerator generator(kSampleRate, single_tone(waveform));
        std::array<float, kFrames> audio{};
        audio.fill(std::numeric_limits<float>::quiet_NaN());
        generator.process(audio, 1U);
        for (const float sample : audio) {
            assert(std::isfinite(sample));
        }
        const float measured_hz = estimate_positive_crossing_hz(audio);
        assert(measured_hz > 0.0F);
        assert(std::abs(measured_hz - 440.0F) < 1.0F);
        assert(std::abs(generator.voice_frequency_hz(0U) - 440.0F) < 0.01F);
    }

    auto octave_parameters = single_tone(NoteGeneratorWaveform::Sine, 69);
    octave_parameters.voices[0].octave = -1;
    NoteGenerator octave_down(kSampleRate, octave_parameters);
    assert(std::abs(octave_down.voice_frequency_hz(0U) - 220.0F) < 0.01F);
    octave_parameters.voices[0].octave = 1;
    octave_down.configure(octave_parameters);
    assert(std::abs(octave_down.voice_frequency_hz(0U) - 880.0F) < 0.02F);
}

void test_per_voice_levels_mute_and_summing() {
    NoteGeneratorParameters parameters;
    parameters.voice_count = 2U;
    parameters.output_level = 0.8F;
    parameters.voices[0] = {NoteGeneratorWaveform::Sine, 69, 0, 0.25F};
    parameters.voices[1] = {NoteGeneratorWaveform::Sine, 81, 0, 0.5F};

    NoteGenerator both(kSampleRate, parameters);
    NoteGenerator one(kSampleRate, parameters);
    auto one_parameters = parameters;
    one_parameters.voices[1].level = 0.0F;
    one.configure(one_parameters);

    std::array<float, 128> both_audio{};
    std::array<float, 128> one_audio{};
    both.process(both_audio, 1U);
    one.process(one_audio, 1U);
    assert(both_audio != one_audio);
    for (std::size_t i = 0U; i < both_audio.size(); ++i) {
        const float expected_first_voice = one_audio[i];
        assert(std::abs(expected_first_voice) <= 0.2F + 1.0e-6F);
    }

    auto half_level_parameters = one_parameters;
    half_level_parameters.voices[0].level = 0.125F;
    NoteGenerator half_level(kSampleRate, half_level_parameters);
    std::array<float, 128> half_audio{};
    half_level.process(half_audio, 1U);
    for (std::size_t i = 0U; i < half_audio.size(); ++i) {
        assert(std::abs(half_audio[i] * 2.0F - one_audio[i]) < 1.0e-6F);
    }

    auto all_muted_parameters = parameters;
    all_muted_parameters.voices[0].level = 0.0F;
    all_muted_parameters.voices[1].level = 0.0F;
    NoteGenerator all_muted(kSampleRate, all_muted_parameters);
    std::array<float, 128> muted_audio{};
    all_muted.process(muted_audio, 1U);
    for (const float sample : muted_audio) {
        assert(sample == 0.0F);
    }
}

void test_fourth_oscillator_voice_is_independent() {
    NoteGeneratorParameters parameters;
    parameters.voice_count = 4U;
    parameters.output_level = 0.5F;
    parameters.voices[0] = {NoteGeneratorWaveform::Sine, 60, 0, 0.0F};
    parameters.voices[1] = {NoteGeneratorWaveform::Sine, 64, 0, 0.0F};
    parameters.voices[2] = {NoteGeneratorWaveform::Sine, 67, 0, 0.0F};
    parameters.voices[3] = {NoteGeneratorWaveform::Sine, 72, 0, 1.0F};

    NoteGenerator generator(kSampleRate, parameters);
    std::array<float, kFrames> audio{};
    generator.process(audio, 1U);
    assert(std::abs(generator.voice_frequency_hz(3U) - 523.2511F) < 0.02F);
    const float measured_hz = estimate_positive_crossing_hz(audio);
    assert(measured_hz > 0.0F);
    assert(std::abs(measured_hz - 523.2511F) < 1.0F);
    assert(generator.voice_frequency_hz(4U) == 0.0F);
}

void test_all_four_oscillators_mix_independently() {
    NoteGeneratorParameters combined_parameters;
    combined_parameters.voice_count = 4U;
    combined_parameters.output_level = 0.25F;
    combined_parameters.voices[0] =
        {NoteGeneratorWaveform::Sine, 48, -1, 0.4F};
    combined_parameters.voices[1] =
        {NoteGeneratorWaveform::Triangle, 60, 0, 0.3F};
    combined_parameters.voices[2] =
        {NoteGeneratorWaveform::Sawtooth, 67, 1, 0.2F};
    combined_parameters.voices[3] =
        {NoteGeneratorWaveform::Square, 72, -1, 0.1F};

    NoteGenerator combined(kSampleRate, combined_parameters);
    std::array<std::array<float, 1024>, 4> isolated_audio{};
    for (std::size_t voice = 0U; voice < isolated_audio.size(); ++voice) {
        auto isolated_parameters = combined_parameters;
        for (std::size_t other = 0U; other < isolated_parameters.voices.size();
             ++other) {
            if (other != voice) {
                isolated_parameters.voices[other].level = 0.0F;
            }
        }
        NoteGenerator isolated(kSampleRate, isolated_parameters);
        isolated.process(isolated_audio[voice], 1U);
        assert(std::abs(isolated.voice_frequency_hz(voice) -
                        combined.voice_frequency_hz(voice)) < 0.001F);
    }

    std::array<float, 1024> combined_audio{};
    combined.process(combined_audio, 1U);
    for (std::size_t frame = 0U; frame < combined_audio.size(); ++frame) {
        float sum = 0.0F;
        for (const auto& voice : isolated_audio) {
            sum += voice[frame];
        }
        assert(std::abs(combined_audio[frame] - sum) < 1.0e-6F);
    }
}

void test_control_bounds_and_safe_fallbacks() {
    NoteGenerator generator;
    NoteGeneratorParameters parameters;
    parameters.voice_count = 0U;
    parameters.output_level = -4.0F;
    generator.configure(parameters);
    assert(generator.parameters().voice_count == 1U);
    assert(generator.parameters().output_level == 0.0F);

    parameters.voice_count = std::numeric_limits<std::uint8_t>::max();
    parameters.output_level = 4.0F;
    parameters.voices[0] = {
        static_cast<NoteGeneratorWaveform>(255U), -99, -99,
        std::numeric_limits<float>::quiet_NaN()};
    parameters.voices[1] = {NoteGeneratorWaveform::Square, 999, 99, 4.0F};
    parameters.voices[2] = {NoteGeneratorWaveform::Triangle, 20, -9, -1.0F};
    parameters.voices[3] = {NoteGeneratorWaveform::Sine, 48, 0, 0.75F};
    generator.configure(parameters);

    const auto& bounded = generator.parameters();
    assert(bounded.voice_count == 4U);
    assert(bounded.output_level == 1.0F);
    assert(bounded.voices[0].waveform == NoteGeneratorWaveform::Sine);
    assert(bounded.voices[0].note_number == 0);
    assert(bounded.voices[0].octave == -2);
    assert(bounded.voices[0].level == 0.5F);
    assert(bounded.voices[1].note_number == 127);
    assert(bounded.voices[1].octave == 2);
    assert(bounded.voices[1].level == 1.0F);
    assert(bounded.voices[2].note_number == 20);
    assert(bounded.voices[2].octave == -2);
    assert(bounded.voices[2].level == 0.0F);
    assert(bounded.voices[3].note_number == 48);
    assert(bounded.voices[3].level == 0.75F);
    assert(generator.voice_frequency_hz(1U) <= 20000.0F);
    assert(generator.voice_frequency_hz(2U) > 0.0F);

    NoteGenerator low_sample_rate(8000.0F, bounded);
    assert(low_sample_rate.voice_frequency_hz(1U) <= 3600.0F);

    NoteGenerator invalid_rate(1000.0F, single_tone(NoteGeneratorWaveform::Sine));
    assert(std::abs(invalid_rate.voice_frequency_hz(0U) - 440.0F) < 0.01F);
}

void test_reset_retains_controls_and_rewinds_phase() {
    auto parameters = single_tone(NoteGeneratorWaveform::Sine, 69);
    parameters.output_level = 0.75F;
    NoteGenerator generator(kSampleRate, parameters);
    NoteGenerator fresh(kSampleRate, parameters);
    std::array<float, 256> first{};
    std::array<float, 256> progressed{};
    std::array<float, 256> after_reset{};
    std::array<float, 256> fresh_audio{};
    generator.process(first, 1U);
    generator.process(progressed, 1U);
    generator.reset();
    assert(generator.parameters().output_level == 0.75F);
    generator.process(after_reset, 1U);
    fresh.process(fresh_audio, 1U);
    assert(after_reset == fresh_audio);
    assert(progressed != after_reset);
}

void test_stereo_is_dual_mono_and_input_independent() {
    NoteGeneratorParameters parameters;
    parameters.voice_count = 3U;
    parameters.voices[0] = {NoteGeneratorWaveform::Sine, 60, 0, 0.3F};
    parameters.voices[1] = {NoteGeneratorWaveform::Triangle, 67, 1, 0.2F};
    parameters.voices[2] = {NoteGeneratorWaveform::Square, 72, -1, 0.1F};

    NoteGenerator mono(kSampleRate, parameters);
    NoteGenerator stereo(kSampleRate, parameters);
    std::array<float, 1024> mono_audio{};
    std::array<float, 2048> stereo_audio{};
    mono_audio.fill(-0.9F);
    for (std::size_t i = 0U; i < stereo_audio.size(); ++i) {
        stereo_audio[i] = i % 2U == 0U ? 0.7F : -0.4F;
    }
    mono.process(mono_audio, 1U);
    stereo.process(stereo_audio, 2U);
    for (std::size_t frame = 0U; frame < mono_audio.size(); ++frame) {
        assert(mono_audio[frame] == stereo_audio[frame * 2U]);
        assert(mono_audio[frame] == stereo_audio[frame * 2U + 1U]);
    }

    std::array<float, 32> unsupported{};
    unsupported.fill(0.37F);
    const auto original = unsupported;
    stereo.process(unsupported, 3U);
    assert(unsupported == original);
}

void test_process_does_not_allocate() {
    NoteGenerator generator(kSampleRate, single_tone(NoteGeneratorWaveform::Sawtooth));
    std::array<float, 4096> audio{};
    audio.fill(0.25F);
    allocation_count.store(0U, std::memory_order_relaxed);
    generator.process(audio, 1U);
    assert(allocation_count.load(std::memory_order_relaxed) == 0U);
    for (const float sample : audio) {
        assert(std::isfinite(sample));
    }
}

void test_four_osc_generator_is_safe_offline_and_stereo_dual_mono() {
    FourOscGenerator generator(kSampleRate);
    const auto& parameters = generator.parameters();
    assert(generator.id() == "four-osc-free-generator");
    assert(parameters.voice_count == 4U);
    assert(parameters.output_level == 0.35F);
    assert(parameters.voices[0].note_number == 48);
    assert(parameters.voices[1].note_number == 55);
    assert(parameters.voices[2].note_number == 60);
    assert(parameters.voices[3].note_number == 64);
    assert(generator.memory_bytes() == sizeof(generator));
    assert(generator.memory_bytes() < 512U);
    assert(std::abs(generator.voice_frequency_hz(0U) - 130.8128F) < 0.02F);
    assert(std::abs(generator.voice_frequency_hz(3U) - 329.6276F) < 0.03F);

    std::array<float, 2048> stereo{};
    stereo.fill(0.7F);
    allocation_count.store(0U, std::memory_order_relaxed);
    generator.process(stereo, 2U);
    assert(allocation_count.load(std::memory_order_relaxed) == 0U);
    float peak = 0.0F;
    for (std::size_t frame = 0U; frame < stereo.size() / 2U; ++frame) {
        assert(std::isfinite(stereo[frame * 2U]));
        assert(stereo[frame * 2U] == stereo[frame * 2U + 1U]);
        peak = std::max(peak, std::abs(stereo[frame * 2U]));
    }
    assert(peak <= 0.210001F);
}

}  // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    test_defaults_bounds_and_small_fixed_state();
    test_waveforms_and_note_frequency();
    test_per_voice_levels_mute_and_summing();
    test_fourth_oscillator_voice_is_independent();
    test_all_four_oscillators_mix_independently();
    test_control_bounds_and_safe_fallbacks();
    test_reset_retains_controls_and_rewinds_phase();
    test_stereo_is_dual_mono_and_input_independent();
    test_process_does_not_allocate();
    test_four_osc_generator_is_safe_offline_and_stereo_dual_mono();
    std::fprintf(stdout, "All TabletFX note generator tests passed.\n");
    return 0;
}
