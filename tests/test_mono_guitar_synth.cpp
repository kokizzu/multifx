#include "tabletfx/effects/mono_guitar_synth.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <span>

using tabletfx::effects::MonoGuitarSynth;
using tabletfx::effects::MonoGuitarSynthConfig;
using tabletfx::effects::MonoGuitarSynthWaveform;

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

std::atomic<std::size_t> allocation_count{0};

constexpr float kSampleRate = 48000.0F;
constexpr float kPi = 3.14159265358979323846F;

// Keep the fixed-size pitch trackers out of the comparatively small test stack.
MonoGuitarSynth oscillator_test(kSampleRate);
MonoGuitarSynth envelope_test(kSampleRate);
MonoGuitarSynth whole_block(kSampleRate);
MonoGuitarSynth split_blocks(kSampleRate);

void require_finite(const std::span<const float> samples) {
    for (const float sample : samples) {
        assert(std::isfinite(sample));
    }
}

void fill_sine(std::span<float> samples, float frequency_hz,
               float amplitude, float start_frame = 0.0F) {
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const float phase = 2.0F * kPi * frequency_hz *
                            (start_frame + static_cast<float>(i)) /
                            kSampleRate;
        samples[i] = amplitude * std::sin(phase);
    }
}

float estimate_positive_crossing_hz(std::span<const float> samples,
                                    float sample_rate) {
    double first_crossing = -1.0;
    double last_crossing = -1.0;
    std::size_t crossings = 0;
    for (std::size_t i = 1; i < samples.size(); ++i) {
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
    return static_cast<float>(
        static_cast<double>(crossings - 1U) * sample_rate /
        (last_crossing - first_crossing));
}

struct WaveStats {
    float peak = 0.0F;
    float rms = 0.0F;
};

WaveStats wave_stats(std::span<const float> samples) {
    double squares = 0.0;
    float peak = 0.0F;
    for (const float sample : samples) {
        peak = std::max(peak, std::abs(sample));
        squares += static_cast<double>(sample) * sample;
    }
    return {peak, static_cast<float>(std::sqrt(
        squares / static_cast<double>(samples.size())))};
}

MonoGuitarSynthConfig clean_synth_config() {
    MonoGuitarSynthConfig config;
    config.waveform = MonoGuitarSynthWaveform::Sine;
    config.mix = 1.0F;
    config.level = 0.8F;
    config.gate_threshold_db = -60.0F;
    config.attack_ms = 0.0F;
    config.release_ms = 100.0F;
    config.portamento_ms = 0.0F;
    return config;
}

void test_oscillator_waveforms_and_tuning() {
    constexpr std::size_t kFrames = 16384;
    constexpr std::size_t kAnalysisStart = 4096;
    constexpr std::size_t kAnalysisFrames = 8192;
    constexpr float kInputFrequency = 220.0F;
    std::array<float, kFrames> guitar{};
    std::array<float, kFrames> audio{};
    fill_sine(guitar, kInputFrequency, 0.25F);

    const std::array<MonoGuitarSynthWaveform, 4> waveforms{
        MonoGuitarSynthWaveform::Sine,
        MonoGuitarSynthWaveform::Triangle,
        MonoGuitarSynthWaveform::Sawtooth,
        MonoGuitarSynthWaveform::Square,
    };
    std::array<WaveStats, 4> stats{};
    for (std::size_t index = 0; index < waveforms.size(); ++index) {
        auto config = clean_synth_config();
        config.waveform = waveforms[index];
        oscillator_test.configure(config);
        oscillator_test.reset();
        audio = guitar;
        oscillator_test.process(audio, 1U);
        require_finite(audio);

        const std::span<const float> steady(
            audio.data() + kAnalysisStart, kAnalysisFrames);
        const float measured_hz = estimate_positive_crossing_hz(
            steady, kSampleRate);
        assert(measured_hz > 0.0F);
        assert(std::abs(measured_hz - kInputFrequency) < 3.0F);
        stats[index] = wave_stats(steady);
        assert(stats[index].rms > 0.2F);
        assert(stats[index].peak > 0.35F);
    }

    const float sine_crest = stats[0].peak / stats[0].rms;
    const float triangle_crest = stats[1].peak / stats[1].rms;
    const float square_crest = stats[3].peak / stats[3].rms;
    assert(sine_crest > 1.3F && sine_crest < 1.5F);
    assert(triangle_crest > 1.6F && triangle_crest < 1.85F);
    assert(square_crest > 0.9F && square_crest < 1.2F);
    assert(stats[2].peak / stats[2].rms > 1.35F);
    assert(oscillator_test.detected_input_frequency_hz() > 0.0F);
    assert(oscillator_test.pitch_response_latency_samples() == 1024U);
    assert(std::abs(oscillator_test.pitch_response_latency_ms() - 21.3333F) <
           0.02F);
}

void test_envelope_gate_attack_and_release() {
    constexpr std::size_t kFrames = 8192;
    constexpr std::size_t kFirstChunk = 4096;
    std::array<float, kFrames> guitar{};
    std::array<float, kFrames> audio{};
    std::array<float, kFrames> silence{};
    fill_sine(guitar, 220.0F, 0.25F);

    auto config = clean_synth_config();
    config.attack_ms = 60.0F;
    config.release_ms = 120.0F;
    envelope_test.configure(config);
    envelope_test.reset();
    audio = guitar;
    envelope_test.process(std::span<float>(audio.data(), kFirstChunk), 1U);
    const float attack_level = envelope_test.envelope();
    assert(attack_level > 0.2F && attack_level < 0.85F);

    std::copy(guitar.begin() + static_cast<std::ptrdiff_t>(kFirstChunk),
              guitar.end(), audio.begin());
    envelope_test.process(std::span<float>(audio.data(), kFrames - kFirstChunk),
                          1U);
    const float sustained_level = envelope_test.envelope();
    assert(sustained_level > attack_level);
    assert(sustained_level > 0.8F);

    envelope_test.process(silence, 1U);
    const float early_release = envelope_test.envelope();
    assert(early_release < sustained_level);
    assert(early_release > 0.1F);
    silence.fill(0.0F);
    envelope_test.process(silence, 1U);
    const float late_release = envelope_test.envelope();
    assert(late_release < early_release);
    assert(late_release < 0.1F);
    require_finite(silence);
}

void test_silence_noise_and_finite_recovery() {
    constexpr std::size_t kFrames = 8192;
    std::array<float, kFrames> audio{};
    std::array<float, kFrames> noise{};

    auto config = clean_synth_config();
    config.attack_ms = 0.0F;
    config.release_ms = 0.0F;
    oscillator_test.configure(config);
    oscillator_test.reset();
    oscillator_test.process(audio, 1U);
    assert(oscillator_test.envelope() == 0.0F);
    for (float sample : audio) {
        assert(std::abs(sample) < 1.0e-7F);
    }

    std::uint32_t state = 0x91e10da5U;
    for (float& sample : noise) {
        state = state * 1664525U + 1013904223U;
        const float unit = static_cast<float>((state >> 8U) & 0x00ffffffU) /
                           16777215.0F;
        sample = (2.0F * unit - 1.0F) * 0.05F;
    }
    oscillator_test.reset();
    oscillator_test.process(noise, 1U);
    assert(oscillator_test.detected_input_frequency_hz() == 0.0F);
    assert(oscillator_test.envelope() == 0.0F);
    for (float sample : noise) {
        assert(std::abs(sample) < 1.0e-7F);
    }

    config.mix = 0.0F;
    oscillator_test.configure(config);
    oscillator_test.reset();
    std::array<float, 64> exceptional{};
    exceptional.fill(1.0e30F);
    exceptional[3] = std::numeric_limits<float>::quiet_NaN();
    exceptional[9] = std::numeric_limits<float>::infinity();
    exceptional[17] = -std::numeric_limits<float>::infinity();
    oscillator_test.process(exceptional, 1U);
    require_finite(exceptional);
    for (float sample : exceptional) {
        assert(std::abs(sample) <= 1000000.0F);
    }
}

void test_block_partition_invariance() {
    constexpr std::size_t kFrames = 4096;
    constexpr std::array<std::size_t, 7> chunks{
        1U, 17U, 127U, 3U, 256U, 61U, 5U};
    std::array<float, kFrames> input{};
    std::array<float, kFrames> whole{};
    std::array<float, kFrames> split{};
    fill_sine(input, 220.0F, 0.2F);

    auto config = clean_synth_config();
    config.portamento_ms = 15.0F;
    whole_block.configure(config);
    split_blocks.configure(config);
    whole_block.reset();
    split_blocks.reset();
    whole = input;
    split = input;
    whole_block.process(whole, 1U);

    std::size_t offset = 0;
    std::size_t chunk_index = 0;
    while (offset < kFrames) {
        const std::size_t count = std::min(
            chunks[chunk_index++ % chunks.size()], kFrames - offset);
        split_blocks.process(
            std::span<float>(split.data() + offset, count), 1U);
        offset += count;
    }

    require_finite(whole);
    require_finite(split);
    for (std::size_t i = 0; i < kFrames; ++i) {
        assert(std::abs(whole[i] - split[i]) < 1.0e-6F);
    }
    assert(whole_block.detected_input_frequency_hz() > 0.0F);
    assert(split_blocks.detected_input_frequency_hz() > 0.0F);
}

void test_stereo_linked_pitch_and_invalid_channel_counts() {
    constexpr std::size_t kFrames = 4096;
    std::array<float, kFrames * 2U> audio{};
    std::array<float, kFrames * 3U> unsupported{};
    std::array<float, kFrames> mono{};
    fill_sine(mono, 220.0F, 0.25F);
    for (std::size_t frame = 0; frame < kFrames; ++frame) {
        // Opposite-polarity stereo input checks that pitch and gate detection
        // use linked energy/peak rather than summing channels and cancelling.
        audio[frame * 2U] = mono[frame];
        audio[frame * 2U + 1U] = -mono[frame];
    }

    auto config = clean_synth_config();
    oscillator_test.configure(config);
    oscillator_test.reset();
    oscillator_test.process(audio, 2U);
    require_finite(audio);
    assert(std::abs(oscillator_test.detected_input_frequency_hz() - 220.0F) <
           4.0F);
    // The generated oscillator is mono and is added equally to both channels.
    // Since the wet mix is 100%, the opposite-polarity guitar input is absent.
    for (std::size_t frame = 0; frame < kFrames; ++frame) {
        assert(audio[frame * 2U] == audio[frame * 2U + 1U]);
    }

    unsupported.fill(0.125F);
    const auto before = unsupported;
    oscillator_test.process(unsupported, 3U);
    assert(unsupported == before);
    std::array<float, 8> no_channels{1.0F, -1.0F, 0.5F, -0.5F,
                                     0.25F, -0.25F, 0.0F, 0.0F};
    const auto before_no_channels = no_channels;
    oscillator_test.process(no_channels, 0U);
    assert(no_channels == before_no_channels);
}

void test_configuration_bounds() {
    auto config = clean_synth_config();
    config.waveform = static_cast<MonoGuitarSynthWaveform>(255U);
    config.mix = std::numeric_limits<float>::quiet_NaN();
    config.level = std::numeric_limits<float>::infinity();
    config.gate_threshold_db = -120.0F;
    config.attack_ms = -5.0F;
    config.release_ms = 9000.0F;
    config.portamento_ms = std::numeric_limits<float>::quiet_NaN();
    oscillator_test.configure(config);

    const auto& safe = oscillator_test.config();
    assert(safe.waveform == MonoGuitarSynthWaveform::Sawtooth);
    assert(safe.mix == 0.75F);
    assert(safe.level == 0.6F);
    assert(safe.gate_threshold_db == -90.0F);
    assert(safe.attack_ms == 0.0F);
    assert(safe.release_ms == 5000.0F);
    assert(safe.portamento_ms == 8.0F);
    assert(oscillator_test.memory_bytes() >= sizeof(oscillator_test));
}

void test_process_does_not_allocate() {
    std::array<float, 2048> audio{};
    fill_sine(audio, 220.0F, 0.2F);
    auto config = clean_synth_config();
    oscillator_test.configure(config);
    oscillator_test.reset();

    allocation_count.store(0U, std::memory_order_relaxed);
    oscillator_test.process(audio, 1U);
    assert(allocation_count.load(std::memory_order_relaxed) == 0U);
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
    test_oscillator_waveforms_and_tuning();
    test_envelope_gate_attack_and_release();
    test_silence_noise_and_finite_recovery();
    test_block_partition_invariance();
    test_stereo_linked_pitch_and_invalid_channel_counts();
    test_configuration_bounds();
    test_process_does_not_allocate();
    return 0;
}
