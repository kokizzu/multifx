#include "tabletfx/effects/fractal_three_voice_synth.hpp"

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

using tabletfx::effects::FractalSynthTracking;
using tabletfx::effects::FractalSynthTrackingMode;
using tabletfx::effects::FractalSynthVoiceParameters;
using tabletfx::effects::FractalSynthWaveform;
using tabletfx::effects::DiatonicHarmonizer;
using tabletfx::effects::DiatonicHarmonizerConfig;
using tabletfx::effects::FractalThreeVoiceSynth;
using tabletfx::effects::FractalThreeVoiceSynthConfig;

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
constexpr float kPi = 3.14159265358979323846F;
constexpr std::size_t kAnalysisFrames = 16384U;

FractalThreeVoiceSynth synth_a(kSampleRate, {}, 0x12345678U);
FractalThreeVoiceSynth synth_b(kSampleRate, {}, 0x12345678U);
FractalThreeVoiceSynth low_rate_synth(8000.0F);

FractalThreeVoiceSynthConfig single_voice(
    const FractalSynthWaveform waveform,
    const FractalSynthTracking tracking = FractalSynthTracking::Off,
    const float frequency_hz = 440.0F) {
    FractalThreeVoiceSynthConfig config;
    config.voices[0] = FractalSynthVoiceParameters{};
    config.voices[0].waveform = waveform;
    config.voices[0].tracking = tracking;
    config.voices[0].frequency_hz = frequency_hz;
    config.voices[0].level = 0.4F;
    config.voices[1].waveform = FractalSynthWaveform::Off;
    config.voices[1].tracking = FractalSynthTracking::Off;
    config.voices[2].waveform = FractalSynthWaveform::Off;
    config.voices[2].tracking = FractalSynthTracking::Off;
    return config;
}

void fill_sine(std::span<float> samples, const float frequency_hz,
               const float amplitude) {
    for (std::size_t i = 0U; i < samples.size(); ++i) {
        const float phase = kPi * 2.0F * frequency_hz *
            static_cast<float>(i) / kSampleRate;
        samples[i] = amplitude * std::sin(phase);
    }
}

float estimate_positive_crossing_hz(std::span<const float> samples) {
    double first = -1.0;
    double last = -1.0;
    std::size_t count = 0U;
    for (std::size_t i = 1U; i < samples.size(); ++i) {
        const float before = samples[i - 1U];
        const float after = samples[i];
        if (before <= 0.0F && after > 0.0F) {
            const double fraction = -static_cast<double>(before) /
                static_cast<double>(after - before);
            const double crossing = static_cast<double>(i - 1U) + fraction;
            if (count == 0U) {
                first = crossing;
            }
            last = crossing;
            ++count;
        }
    }
    if (count < 2U || !(last > first)) {
        return 0.0F;
    }
    return static_cast<float>(static_cast<double>(count - 1U) * kSampleRate /
                              (last - first));
}

float rms(std::span<const float> samples) {
    double energy = 0.0;
    for (const float sample : samples) {
        energy += static_cast<double>(sample) * sample;
    }
    return static_cast<float>(std::sqrt(energy / samples.size()));
}

void test_manual_frequency_shift_tuning_and_waveforms() {
    const std::array<FractalSynthWaveform, 4> waveforms{
        FractalSynthWaveform::Sine,
        FractalSynthWaveform::Triangle,
        FractalSynthWaveform::Square,
        FractalSynthWaveform::Sawtooth,
    };
    for (const auto waveform : waveforms) {
        synth_a.reset();
        auto config = single_voice(waveform);
        synth_a.configure(config);
        std::array<float, kAnalysisFrames> audio{};
        synth_a.process(audio, 1U);
        const float measured_hz = estimate_positive_crossing_hz(audio);
        assert(measured_hz > 0.0F);
        assert(std::abs(measured_hz - 440.0F) < 1.0F);
    }

    auto config = single_voice(FractalSynthWaveform::Sine);
    config.voices[0].shift_semitones = 12;
    config.voices[0].tune_cents = 50.0F;
    synth_a.configure(config);
    assert(std::abs(synth_a.voice_frequency_hz(0U) -
                    (440.0F * std::exp2(12.5F / 12.0F))) < 0.1F);
    assert(synth_a.voice_frequency_hz(3U) == 0.0F);
}

void test_tracker_pitch_env_only_and_quantize() {
    auto config = single_voice(FractalSynthWaveform::Sine,
                               FractalSynthTracking::PitchAndEnvelope);
    config.voices[0].attack_ms = 0.0F;
    config.voices[0].shift_semitones = 12;
    synth_a.reset();
    synth_a.configure(config);
    std::array<float, kAnalysisFrames> audio{};
    fill_sine(audio, 440.0F, 0.5F);
    synth_a.process(audio, 1U);
    assert(synth_a.detected_input_frequency_hz() > 430.0F);
    assert(synth_a.detected_input_frequency_hz() < 450.0F);
    assert(std::abs(synth_a.voice_frequency_hz(0U) - 880.0F) < 3.0F);
    assert(synth_a.voice_envelope(0U) > 0.35F);

    config = single_voice(FractalSynthWaveform::Sine,
                          FractalSynthTracking::Quantize);
    config.voices[0].attack_ms = 0.0F;
    synth_a.reset();
    synth_a.configure(config);
    audio.fill(0.0F);
    fill_sine(audio, 450.0F, 0.5F);
    synth_a.process(audio, 1U);
    assert(synth_a.detected_input_frequency_hz() > 440.0F);
    assert(std::abs(synth_a.voice_frequency_hz(0U) - 440.0F) < 2.0F);

    config = single_voice(FractalSynthWaveform::Sine,
                          FractalSynthTracking::EnvelopeOnly, 330.0F);
    config.voices[0].attack_ms = 0.0F;
    synth_a.reset();
    synth_a.configure(config);
    audio.fill(0.5F);
    synth_a.process(audio, 1U);
    assert(synth_a.voice_frequency_hz(0U) == 330.0F);
    assert(std::abs(synth_a.voice_envelope(0U) - 0.5F) < 0.02F);
    audio.fill(0.0F);
    synth_a.process(audio, 1U);
    assert(synth_a.voice_envelope(0U) < 0.01F);
}

void test_guitar_default_and_bass_tracking_range() {
    auto config = single_voice(FractalSynthWaveform::Sine,
                               FractalSynthTracking::PitchAndEnvelope);
    config.voices[0].attack_ms = 0.0F;
    assert(config.tracking_mode == FractalSynthTrackingMode::Guitar);

    synth_a.reset();
    synth_a.configure(config);
    std::array<float, 24000> guitar_low_audio{};
    fill_sine(guitar_low_audio, 30.0F, 0.5F);
    auto tracker_input = guitar_low_audio;
    DiatonicHarmonizer existing_guitar_tracker(kSampleRate);
    DiatonicHarmonizerConfig tracker_config;
    tracker_config.mix = 0.0F;
    tracker_config.pitch_interval = 0;
    tracker_config.delay_ms = 0.0F;
    tracker_config.feedback = 0.0F;
    existing_guitar_tracker.configure(tracker_config);
    existing_guitar_tracker.set_enabled(false);
    existing_guitar_tracker.process(tracker_input, 1U);
    synth_a.process(guitar_low_audio, 1U);
    assert(synth_a.detected_input_frequency_hz() ==
           existing_guitar_tracker.detected_input_frequency_hz());

    config.tracking_mode = FractalSynthTrackingMode::Bass;
    synth_a.reset();
    synth_a.configure(config);
    std::array<float, 24000> bass_audio{};
    fill_sine(bass_audio, 23.125F, 0.5F); // F#0, approximately.
    synth_a.process(bass_audio, 1U);
    const float detected_hz = synth_a.detected_input_frequency_hz();
    assert(detected_hz > 22.8F);
    assert(detected_hz < 23.5F);
    assert(std::abs(synth_a.voice_frequency_hz(0U) - detected_hz) < 0.05F);
}

void test_filter_and_stereo_pan() {
    auto high = single_voice(FractalSynthWaveform::Sine,
                             FractalSynthTracking::Off, 2000.0F);
    high.voices[0].level = 0.5F;
    high.voices[0].filter_frequency_hz = 18000.0F;
    synth_a.reset();
    synth_a.configure(high);
    std::array<float, 8192> high_audio{};
    synth_a.process(high_audio, 1U);
    const float high_rms = rms(high_audio);

    high.voices[0].filter_frequency_hz = 500.0F;
    synth_a.reset();
    synth_a.configure(high);
    std::array<float, 8192> low_audio{};
    synth_a.process(low_audio, 1U);
    assert(rms(low_audio) < high_rms * 0.2F);

    FractalThreeVoiceSynthConfig stereo_config;
    stereo_config.voices[0] = FractalSynthVoiceParameters{};
    stereo_config.voices[0].tracking = FractalSynthTracking::Off;
    stereo_config.voices[0].waveform = FractalSynthWaveform::Sine;
    stereo_config.voices[0].frequency_hz = 440.0F;
    stereo_config.voices[0].level = 0.5F;
    stereo_config.voices[0].pan = -1.0F;
    stereo_config.voices[1] = stereo_config.voices[0];
    stereo_config.voices[1].frequency_hz = 660.0F;
    stereo_config.voices[1].pan = 1.0F;
    stereo_config.voices[2].waveform = FractalSynthWaveform::Off;
    stereo_config.voices[2].tracking = FractalSynthTracking::Off;
    synth_a.reset();
    synth_a.configure(stereo_config);
    std::array<float, 4096> stereo{};
    synth_a.process(stereo, 2U);
    float left_energy = 0.0F;
    float right_energy = 0.0F;
    for (std::size_t frame = 0U; frame < stereo.size() / 2U; ++frame) {
        left_energy += std::abs(stereo[frame * 2U]);
        right_energy += std::abs(stereo[frame * 2U + 1U]);
    }
    assert(left_energy > 10.0F);
    assert(right_energy > 10.0F);
}

void test_noise_waveforms_are_finite_and_repeatable_after_reset() {
    for (const auto waveform : {FractalSynthWaveform::Random,
                                FractalSynthWaveform::WhiteNoise,
                                FractalSynthWaveform::PinkNoise}) {
        auto config = single_voice(waveform,
                                   FractalSynthTracking::Off, 220.0F);
        synth_a.reset();
        synth_b.reset();
        synth_a.configure(config);
        synth_b.configure(config);
        std::array<float, 2048> first{};
        std::array<float, 2048> second{};
        synth_a.process(first, 1U);
        synth_b.process(second, 1U);
        assert(first == second);
        for (const float sample : first) {
            assert(std::isfinite(sample));
        }
    }
}

void test_bounds_invalid_values_and_safe_layouts() {
    auto config = single_voice(FractalSynthWaveform::Sine);
    config.voices[0].tracking = static_cast<FractalSynthTracking>(99U);
    config.tracking_mode = static_cast<FractalSynthTrackingMode>(99U);
    config.voices[0].waveform = static_cast<FractalSynthWaveform>(99U);
    config.voices[0].frequency_hz = std::numeric_limits<float>::infinity();
    config.voices[0].shift_semitones = 99;
    config.voices[0].tune_cents = 1000.0F;
    config.voices[0].duty_cycle = std::numeric_limits<float>::quiet_NaN();
    config.voices[0].pan = -5.0F;
    config.voices[0].filter_frequency_hz = 500000.0F;
    config.voices[0].filter_q = -5.0F;
    config.voices[0].attack_ms = -10.0F;
    config.voices[0].level = 4.0F;
    synth_a.configure(config);
    const auto& bounded = synth_a.config().voices[0];
    assert(synth_a.config().tracking_mode == FractalSynthTrackingMode::Guitar);
    assert(bounded.tracking == FractalSynthTracking::Off);
    assert(bounded.waveform == FractalSynthWaveform::Off);
    assert(bounded.frequency_hz == 440.0F);
    assert(bounded.shift_semitones == 24);
    assert(bounded.tune_cents == 50.0F);
    assert(bounded.duty_cycle == 0.5F);
    assert(bounded.pan == -1.0F);
    assert(bounded.filter_frequency_hz <= kSampleRate * 0.45F);
    assert(bounded.filter_q == 0.25F);
    assert(bounded.attack_ms == 0.0F);
    assert(bounded.level == 1.0F);

    std::array<float, 32> unsupported{};
    unsupported.fill(0.25F);
    const auto original = unsupported;
    synth_a.process(unsupported, 3U);
    assert(unsupported == original);

    std::array<float, 32> invalid_audio{};
    invalid_audio.fill(std::numeric_limits<float>::quiet_NaN());
    synth_a.process(invalid_audio, 1U);
    for (const float sample : invalid_audio) {
        assert(std::isfinite(sample));
    }

    assert(low_rate_synth.memory_bytes() >= sizeof(low_rate_synth));
}

void test_block_output_level_bounds_smoothing_and_partitioning() {
    auto config = single_voice(FractalSynthWaveform::Sine,
                               FractalSynthTracking::Off);
    config.voices[0].level = 0.0F;
    config.voices[1].waveform = FractalSynthWaveform::Off;
    config.voices[2].waveform = FractalSynthWaveform::Off;

    std::array<float, 1200> unity_audio{};
    unity_audio.fill(0.25F);
    FractalThreeVoiceSynth unity(kSampleRate, config, 0x87654321U);
    unity.process(unity_audio, 1U);
    for (const float sample : unity_audio) {
        assert(sample == 0.25F);
    }

    constexpr float kTargetDb = -12.0F;
    constexpr std::size_t kRampFrames = 480U;
    const float expected_gain = std::pow(10.0F, kTargetDb / 20.0F);
    config.output_level_db = kTargetDb;
    std::array<float, 1200> whole_audio{};
    whole_audio.fill(0.25F);
    auto partitioned_audio = whole_audio;
    FractalThreeVoiceSynth whole(kSampleRate, config, 0x87654321U);
    FractalThreeVoiceSynth partitioned(kSampleRate, config, 0x87654321U);
    assert(whole.config().output_level_db == kTargetDb);
    whole.process(whole_audio, 1U);

    std::size_t offset = 0U;
    for (const std::size_t block_frames : {1U, 17U, 128U, 257U, 64U, 511U}) {
        const std::size_t frames = std::min(
            block_frames, partitioned_audio.size() - offset);
        partitioned.process(
            std::span<float>(partitioned_audio.data() + offset, frames), 1U);
        offset += frames;
        if (offset == partitioned_audio.size()) break;
    }
    if (offset < partitioned_audio.size()) {
        partitioned.process(
            std::span<float>(partitioned_audio.data() + offset,
                             partitioned_audio.size() - offset),
            1U);
    }
    assert(whole_audio == partitioned_audio);

    const float first_gain = 1.0F + (expected_gain - 1.0F) /
        static_cast<float>(kRampFrames);
    assert(std::abs(whole_audio[0] - 0.25F * first_gain) < 1.0e-6F);
    assert(std::abs(whole_audio[kRampFrames - 1U] -
                    0.25F * expected_gain) < 1.0e-6F);
    assert(std::abs(whole_audio.back() - 0.25F * expected_gain) < 1.0e-6F);

    config.output_level_db = 99.0F;
    whole.configure(config);
    assert(whole.config().output_level_db == 12.0F);
    config.output_level_db = -99.0F;
    whole.configure(config);
    assert(whole.config().output_level_db == -24.0F);
    config.output_level_db = std::numeric_limits<float>::quiet_NaN();
    whole.configure(config);
    assert(whole.config().output_level_db == 0.0F);
}

void test_process_does_not_allocate() {
    auto config = single_voice(FractalSynthWaveform::Sawtooth,
                               FractalSynthTracking::PitchAndEnvelope);
    config.output_level_db = -6.0F;
    synth_a.reset();
    synth_a.configure(config);
    std::array<float, 4096> audio{};
    fill_sine(audio, 220.0F, 0.5F);
    allocation_count.store(0U, std::memory_order_relaxed);
    synth_a.process(audio, 1U);
    assert(allocation_count.load(std::memory_order_relaxed) == 0U);
    for (const float sample : audio) {
        assert(std::isfinite(sample));
    }

    config.tracking_mode = FractalSynthTrackingMode::Bass;
    synth_a.reset();
    synth_a.configure(config);
    fill_sine(audio, 23.125F, 0.5F);
    allocation_count.store(0U, std::memory_order_relaxed);
    synth_a.process(audio, 1U);
    assert(allocation_count.load(std::memory_order_relaxed) == 0U);
    for (const float sample : audio) {
        assert(std::isfinite(sample));
    }
}

}  // namespace

void* operator new(const std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](const std::size_t size) {
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
    test_manual_frequency_shift_tuning_and_waveforms();
    test_tracker_pitch_env_only_and_quantize();
    test_guitar_default_and_bass_tracking_range();
    test_filter_and_stereo_pan();
    test_noise_waveforms_are_finite_and_repeatable_after_reset();
    test_bounds_invalid_values_and_safe_layouts();
    test_block_output_level_bounds_smoothing_and_partitioning();
    test_process_does_not_allocate();
    std::fprintf(stdout, "All TabletFX three-voice synth tests passed.\n");
    return 0;
}
