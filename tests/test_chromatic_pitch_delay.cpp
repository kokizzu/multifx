#include "tabletfx/effects/chromatic_pitch_delay.hpp"

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
#include <vector>

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};
constexpr float kSampleRate = 48000.0F;
constexpr double kPi = 3.14159265358979323846;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> render_tone(tabletfx::ChromaticPitchDelay& effect,
                               double frequency_hz,
                               std::size_t total_frames = 72000,
                               std::uint16_t channels = 1) {
    std::vector<float> output(total_frames * channels);
    std::array<float, 128 * 2> block{};
    double phase = 0.0;
    std::size_t offset = 0;
    while (offset < total_frames) {
        const std::size_t frames = std::min<std::size_t>(128, total_frames - offset);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const float sample = static_cast<float>(0.2 * std::sin(phase));
            phase += 2.0 * kPi * frequency_hz / static_cast<double>(kSampleRate);
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
            for (std::uint16_t channel = 0; channel < channels; ++channel) {
                block[frame * channels + channel] = sample;
            }
        }
        auto span = std::span<float>(block.data(), frames * channels);
        effect.process(span, channels);
        std::copy_n(span.begin(), span.size(),
                    output.begin() + static_cast<std::ptrdiff_t>(offset * channels));
        offset += frames;
    }
    return output;
}

double amplitude_near(const std::vector<float>& samples, double frequency_hz,
                      std::size_t start_frame, std::size_t length,
                      std::uint16_t channel = 0, std::uint16_t channels = 1) {
    double cosine = 0.0;
    double sine = 0.0;
    double window_sum = 0.0;
    for (std::size_t i = 0; i < length; ++i) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * kPi * static_cast<double>(i) / static_cast<double>(length - 1));
        const double angle = 2.0 * kPi * frequency_hz *
                             static_cast<double>(start_frame + i) /
                             static_cast<double>(kSampleRate);
        const double value = static_cast<double>(
            samples[(start_frame + i) * channels + channel]) * window;
        cosine += value * std::cos(angle);
        sine += value * std::sin(angle);
        window_sum += window;
    }
    return 2.0 * std::hypot(cosine, sine) / window_sum;
}

double peak_frequency_near(const std::vector<float>& samples,
                           double expected_frequency,
                           std::size_t start_frame = 12000,
                           std::size_t length = 24000) {
    double peak_frequency = expected_frequency;
    double peak_amplitude = 0.0;
    const double lower = expected_frequency * 0.90;
    const double upper = expected_frequency * 1.10;
    for (double frequency = lower; frequency <= upper; frequency += 0.5) {
        const double amplitude = amplitude_near(samples, frequency, start_frame,
                                                length);
        if (amplitude > peak_amplitude) {
            peak_amplitude = amplitude;
            peak_frequency = frequency;
        }
    }
    return peak_frequency;
}

std::vector<float> render_impulse(tabletfx::ChromaticPitchDelayParameters p,
                                  std::size_t frames,
                                  bool bypass_after_first_block = false,
                                  bool spillover = false) {
    p.spillover = spillover;
    tabletfx::ChromaticPitchDelay effect(kSampleRate, 1, 500.0F, p);
    std::vector<float> output(frames, 0.0F);
    output[0] = 1.0F;
    if (!bypass_after_first_block) {
        effect.process(output, 1);
        return output;
    }
    const std::size_t active_frames = std::min<std::size_t>(2048, frames);
    effect.process(std::span<float>(output.data(), active_frames), 1);
    if (active_frames < frames) {
        effect.process_bypassed(
            std::span<float>(output.data() + active_frames, frames - active_frames), 1);
    }
    return output;
}

void test_defaults_bounds_and_memory() {
    tabletfx::ChromaticPitchDelay effect;
    const auto p = effect.parameters();
    require(effect.id() == "chromatic-pitch-delay",
            "Chromatic Pitch Delay ID changed");
    require(effect.max_pitch_semitones() == 24.0F && p.pitch_semitones == 0.0F &&
                p.time_ms == 350.0F && p.time_mode ==
                    tabletfx::ChromaticPitchDelayTimeMode::Milliseconds &&
                p.feedback == 0.25F && p.mix == 0.25F && p.tone == 0.65F &&
                !p.spillover,
            "project-authored defaults changed unexpectedly");
    require(effect.memory_bytes() > 1U * 1024U * 1024U &&
                effect.memory_bytes() < 2U * 1024U * 1024U,
            "pitch and stereo 2-second ring memory should remain bounded below 2 MiB");
    require(effect.dry_path_latency_ms() == 0.0F &&
                effect.pitch_engine_latency_ms() < 6.0F,
            "dry path should be immediate and pitch-engine latency should remain bounded");

    auto invalid = p;
    invalid.pitch_semitones = 50.0F;
    invalid.time_ms = -1.0F;
    invalid.tempo_bpm = 900.0F;
    invalid.feedback = 3.0F;
    invalid.tone = -4.0F;
    invalid.mix = std::numeric_limits<float>::quiet_NaN();
    invalid.tap_division = static_cast<tabletfx::ChromaticPitchDelayTapDivision>(255);
    invalid.time_mode = static_cast<tabletfx::ChromaticPitchDelayTimeMode>(255);
    effect.configure(invalid);
    const auto bounded = effect.parameters();
    require(bounded.pitch_semitones == 24.0F && bounded.time_ms == 10.0F &&
                bounded.tempo_bpm == 300.0F && bounded.feedback == 0.95F &&
                bounded.tone == 0.0F && bounded.mix == 0.25F &&
                bounded.tap_division == tabletfx::ChromaticPitchDelayTapDivision::Quarter &&
                bounded.time_mode == tabletfx::ChromaticPitchDelayTimeMode::Milliseconds,
            "invalid project controls must clamp or fall back to safe values");
    require(effect.effective_time_ms() == 10.0F,
            "milliseconds mode must use the clamped free-time setting");

    invalid = p;
    invalid.time_mode = tabletfx::ChromaticPitchDelayTimeMode::TempoSync;
    invalid.tempo_bpm = 120.0F;
    invalid.tap_division = tabletfx::ChromaticPitchDelayTapDivision::Eighth;
    effect.configure(invalid);
    require(std::abs(effect.effective_time_ms() - 250.0F) < 0.01F,
            "120 BPM eighth-note tap should calculate to 250 ms");
    invalid.tempo_bpm = 300.0F;
    invalid.tap_division = tabletfx::ChromaticPitchDelayTapDivision::SixtyFourth;
    effect.configure(invalid);
    require(std::abs(effect.effective_time_ms() - 12.5F) < 0.01F,
            "short tempo divisions should remain available above the 10 ms floor");
}

void test_two_octave_pitch_endpoints() {
    for (const float semitones : {-24.0F, 24.0F}) {
        tabletfx::ChromaticPitchDelayParameters p;
        p.pitch_semitones = semitones;
        p.time_ms = 40.0F;
        p.feedback = 0.0F;
        p.mix = 1.0F;
        p.tone = 1.0F;
        tabletfx::ChromaticPitchDelay effect(kSampleRate, 1, 500.0F, p);
        const double input_frequency = 220.0;
        const double expected = input_frequency *
            std::exp2(static_cast<double>(semitones) / 12.0);
        const auto output = render_tone(effect, input_frequency);
        const double actual = peak_frequency_near(output, expected);
        const double cents = 1200.0 * std::log2(actual / expected);
        require(std::abs(cents) <= 50.0,
                "two-octave chromatic pitch output deviates more than 50 cents");
        require(amplitude_near(output, expected, 12000, 24000) > 0.02,
                "two-octave pitch output is too weak on the test tone");
    }
}

void test_time_and_tempo_tap_delay() {
    tabletfx::ChromaticPitchDelayParameters p;
    p.time_ms = 50.0F;
    p.feedback = 0.0F;
    p.mix = 1.0F;
    p.pitch_semitones = 0.0F;
    p.tone = 1.0F;
    tabletfx::ChromaticPitchDelay effect(kSampleRate, 2, 250.0F, p);

    constexpr std::size_t frames = 6000;
    constexpr std::size_t expected_peak = 2400;
    std::vector<float> audio(frames * 2U, 0.0F);
    audio[0] = 1.0F;
    effect.process(audio, 2);
    auto first = std::find_if(audio.begin(), audio.end(), [](float sample) {
        return std::abs(sample) > 1.0e-6F;
    });
    require(first != audio.end(), "50 ms pitch-delay impulse was lost");
    const std::size_t first_frame =
        static_cast<std::size_t>(std::distance(audio.begin(), first)) / 2U;
    require(first_frame >= expected_peak - 256U &&
                first_frame < expected_peak + 128U,
            "pitch delay onset should include the configured time and compensated shifter latency");

    for (std::size_t frame = 0; frame < frames; ++frame) {
        require(audio[frame * 2U + 1U] == 0.0F,
                "left-channel impulse must not leak into the right output");
    }

    p.time_mode = tabletfx::ChromaticPitchDelayTimeMode::TempoSync;
    p.tempo_bpm = 120.0F;
    p.tap_division = tabletfx::ChromaticPitchDelayTapDivision::Eighth;
    tabletfx::ChromaticPitchDelay synced(kSampleRate, 1, 500.0F, p);
    std::vector<float> synced_audio(15000, 0.0F);
    synced_audio[0] = 1.0F;
    synced.process(synced_audio, 1);
    const auto synced_first = std::find_if(
        synced_audio.begin(), synced_audio.end(), [](float sample) {
            return std::abs(sample) > 1.0e-6F;
        });
    require(synced_first != synced_audio.end(),
            "tempo-synced pitch delay lost its impulse");
    const auto synced_frame = static_cast<std::size_t>(
        std::distance(synced_audio.begin(), synced_first));
    require(synced_frame >= 12000U - 256U && synced_frame < 12000U + 128U,
            "tempo-sync eighth note did not produce a 250 ms repeat");
}

void test_tone_filter_and_feedback_repeats() {
    const auto render_high_tone = [](float tone) {
        tabletfx::ChromaticPitchDelayParameters p;
        p.time_ms = 40.0F;
        p.feedback = 0.0F;
        p.mix = 1.0F;
        p.pitch_semitones = 0.0F;
        p.tone = tone;
        tabletfx::ChromaticPitchDelay effect(kSampleRate, 1, 100.0F, p);
        return render_tone(effect, 8000.0, 24000);
    };
    const auto dark = render_high_tone(0.0F);
    const auto bright = render_high_tone(1.0F);
    const double dark_level = amplitude_near(dark, 8000.0, 10000, 10000);
    const double bright_level = amplitude_near(bright, 8000.0, 10000, 10000);
    require(bright_level > dark_level * 2.0,
            "Tone should change the wet path high-frequency response");

    tabletfx::ChromaticPitchDelayParameters p;
    p.time_ms = 40.0F;
    p.feedback = 0.0F;
    p.mix = 1.0F;
    p.pitch_semitones = 0.0F;
    p.tone = 1.0F;
    const auto no_feedback = render_impulse(p, 6000);
    p.feedback = 0.5F;
    const auto with_feedback = render_impulse(p, 6000);
    const auto window_peak = [](const std::vector<float>& signal,
                                std::size_t start, std::size_t end) {
        float peak = 0.0F;
        for (std::size_t frame = start; frame < end; ++frame) {
            peak = std::max(peak, std::abs(signal[frame]));
        }
        return peak;
    };
    require(window_peak(no_feedback, 3600, 4400) < 1.0e-4F &&
                window_peak(with_feedback, 3600, 4400) > 1.0e-3F,
            "positive feedback must create a later repeat while zero feedback does not");
}

void test_spillover_bypass_and_reset() {
    tabletfx::ChromaticPitchDelayParameters p;
    p.time_ms = 40.0F;
    p.feedback = 0.5F;
    p.mix = 1.0F;
    p.pitch_semitones = 0.0F;
    p.tone = 1.0F;

    const auto tail = render_impulse(p, 7000, true, true);
    require(std::any_of(tail.begin() + 2048, tail.end(), [](float sample) {
                return std::abs(sample) > 1.0e-4F;
            }),
            "enabled spillover should emit the stored pitch-delay tail while bypassed");

    const auto muted = render_impulse(p, 7000, true, false);
    require(std::all_of(muted.begin() + 2048, muted.end(), [](float sample) {
                return sample == 0.0F;
            }),
            "bypass without spillover must clear the wet tail and pass dry silence");

    tabletfx::ChromaticPitchDelay first(kSampleRate, 1, 200.0F, p);
    tabletfx::ChromaticPitchDelay fresh(kSampleRate, 1, 200.0F, p);
    std::array<float, 4096> original{};
    original[0] = 1.0F;
    auto a = original;
    auto b = original;
    first.process(a, 1);
    first.reset();
    a = original;
    first.process(a, 1);
    fresh.process(b, 1);
    for (std::size_t i = 0; i < a.size(); ++i) {
        require(std::abs(a[i] - b[i]) <= 1.0e-7F,
                "reset must clear pitch, tone, and delay histories");
    }
}

void test_stereo_independence_irregular_blocks_and_no_allocations() {
    tabletfx::ChromaticPitchDelayParameters p;
    p.time_ms = 50.0F;
    p.feedback = 0.2F;
    p.mix = 0.8F;
    p.pitch_semitones = 7.0F;
    p.tone = 0.8F;
    tabletfx::ChromaticPitchDelay whole(kSampleRate, 2, 250.0F, p);
    tabletfx::ChromaticPitchDelay partitioned(kSampleRate, 2, 250.0F, p);
    constexpr std::size_t frames = 12000;
    std::vector<float> source(frames * 2U);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        source[frame * 2U] = static_cast<float>(0.15 * std::sin(0.13 * frame));
        source[frame * 2U + 1U] = static_cast<float>(0.12 * std::cos(0.09 * frame));
    }
    auto expected = source;
    whole.process(expected, 2);
    auto actual = source;
    constexpr std::array<std::size_t, 7> chunks{1, 7, 127, 128, 513, 31, 64};
    std::size_t offset = 0;
    std::size_t chunk_index = 0;
    while (offset < frames) {
        const std::size_t chunk = std::min(chunks[chunk_index % chunks.size()],
                                           frames - offset);
        partitioned.process(std::span<float>(actual.data() + offset * 2U,
                                             chunk * 2U), 2);
        offset += chunk;
        ++chunk_index;
    }
    for (std::size_t i = 0; i < actual.size(); ++i) {
        require(std::isfinite(actual[i]) && std::abs(actual[i] - expected[i]) < 1e-7F,
                "processing output must be finite and independent of callback partitioning");
    }

    std::array<float, 256> block{};
    for (int i = 0; i < 64; ++i) whole.process(block, 2);
    allocation_count.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    constexpr std::array<std::size_t, 9> counts{1, 7, 15, 31, 32, 63, 64, 127, 128};
    for (int i = 0; i < 128; ++i) {
        const std::size_t n = counts[static_cast<std::size_t>(i) % counts.size()];
        auto span = std::span<float>(block.data(), n * 2U);
        whole.process(span, 2);
        for (float sample : span) {
            if (!std::isfinite(sample)) {
                count_allocations.store(false, std::memory_order_release);
                throw std::runtime_error("irregular callback produced non-finite output");
            }
        }
    }
    count_allocations.store(false, std::memory_order_release);
    require(allocation_count.load(std::memory_order_relaxed) == 0,
            "Chromatic Pitch Delay process() must not allocate");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_bounds_and_memory();
        test_two_octave_pitch_endpoints();
        test_time_and_tempo_tap_delay();
        test_tone_filter_and_feedback_repeats();
        test_spillover_bypass_and_reset();
        test_stereo_independence_irregular_blocks_and_no_allocations();
        std::cout << "Chromatic Pitch Delay tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        count_allocations.store(false, std::memory_order_release);
        std::cerr << "Chromatic Pitch Delay test failed: " << error.what() << '\n';
        return 1;
    }
}
