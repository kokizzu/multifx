#include "tabletfx/effects/diatonic_harmonizer.hpp"

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

using tabletfx::effects::DiatonicHarmonizer;
using tabletfx::effects::DiatonicHarmonizerConfig;
using tabletfx::effects::DiatonicScale;
using tabletfx::effects::TapDivision;

namespace {

std::atomic<bool> track_allocations{false};
std::atomic<std::size_t> allocation_count{0};

} // namespace

// Keep the checks active in Release builds, where NDEBUG would otherwise
// remove every assertion from this standalone test executable.
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

// Keep the fixed-memory processors out of the comparatively small test stack.
DiatonicHarmonizer whole_block(48000.0F);
DiatonicHarmonizer split_blocks(48000.0F);
DiatonicHarmonizer reset_check(48000.0F);
DiatonicHarmonizer latency_44100(44100.0F);
DiatonicHarmonizer latency_96000(96000.0F);
DiatonicHarmonizer pitch_check(48000.0F);
DiatonicHarmonizer wide_pitch_check(48000.0F, 24);
DiatonicHarmonizer custom_scale_check(48000.0F);
DiatonicHarmonizer adapter_reference(48000.0F);
DiatonicHarmonizer adapter_in_place(48000.0F);

float estimate_tone_hz(const float* samples, std::size_t count,
                       float sample_rate) {
    float best_frequency = 0.0F;
    double best_power = -1.0;
    for (int frequency = 100; frequency <= 500; ++frequency) {
        const double omega = 2.0 * 3.14159265358979323846 * frequency /
                             static_cast<double>(sample_rate);
        const double coefficient = 2.0 * std::cos(omega);
        double s1 = 0.0;
        double s2 = 0.0;
        for (std::size_t i = 0; i < count; ++i) {
            const double s0 = static_cast<double>(samples[i]) +
                              coefficient * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const double power = s1 * s1 + s2 * s2 - coefficient * s1 * s2;
        if (power > best_power) {
            best_power = power;
            best_frequency = static_cast<float>(frequency);
        }
    }
    return best_frequency;
}

double tone_amplitude_at(const float* samples, std::size_t start,
                         std::size_t count, double frequency,
                         double sample_rate) {
    double cosine = 0.0;
    double sine = 0.0;
    double window_sum = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * 3.14159265358979323846 * static_cast<double>(i) /
            static_cast<double>(count - 1U));
        const double angle = 2.0 * 3.14159265358979323846 * frequency *
                             static_cast<double>(i) / sample_rate;
        const double sample = static_cast<double>(samples[start + i]) * window;
        cosine += sample * std::cos(angle);
        sine += sample * std::sin(angle);
        window_sum += window;
    }
    return 2.0 * std::hypot(cosine, sine) / window_sum;
}

void test_scale_mapping() {
    assert(DiatonicHarmonizer::harmony_midi_note(
               60, 0, DiatonicScale::Major, 2) == 64);  // C to E
    assert(DiatonicHarmonizer::harmony_midi_note(
               69, 0, DiatonicScale::Major, 2) == 72);  // A to C
    assert(DiatonicHarmonizer::harmony_midi_note(
               60, 0, DiatonicScale::Major, -2) == 57); // C down to A
    assert(DiatonicHarmonizer::harmony_midi_note(
               62, 7, DiatonicScale::Major, 2) == 66);  // D to F# in G
    assert(DiatonicHarmonizer::harmony_midi_note(
               64, 9, DiatonicScale::NaturalMinor, 2) == 67); // E to G in Am
    assert(DiatonicHarmonizer::harmony_midi_note(
               57, 9, DiatonicScale::HarmonicMinor, 2) == 60); // A to C
    assert(DiatonicHarmonizer::harmony_midi_note(
               61, 0, DiatonicScale::Major, 0) == 60); // tie chooses lower C
    assert(DiatonicHarmonizer::harmony_midi_note(
               60, 0, DiatonicScale::Major, 99) == 101); // clamped to +24 degrees
    assert(DiatonicHarmonizer::harmony_midi_note(
               60, 0, DiatonicScale::Major, -99) == 19); // clamped to -24 degrees
    assert(DiatonicHarmonizer::harmony_midi_note(
               60, -1, DiatonicScale::Major, 1) == 62); // key clamped to C
    assert(DiatonicHarmonizer::scale_name(DiatonicScale::Blues) == "Blues");
}

void test_custom_scale_mapping_and_key() {
    constexpr std::array<std::uint8_t, 8> a_major_pentatonic{
        0U, 2U, 4U, 7U, 9U, 0U, 0U, 0U};
    assert(DiatonicHarmonizer::harmony_midi_note_custom(
               69, 9, 1, a_major_pentatonic, 5U) == 71); // A to B
    assert(DiatonicHarmonizer::harmony_midi_note_custom(
               69, 9, 2, a_major_pentatonic, 5U) == 73); // A to C#
    assert(DiatonicHarmonizer::harmony_midi_note_custom(
               69, 9, -1, a_major_pentatonic, 5U) == 66); // A down to F#
    assert(DiatonicHarmonizer::harmony_midi_note_custom(
               62, 9, 1, a_major_pentatonic, 5U) == 64); // D snaps to C#, then E
    // Key controls pitch membership. With key C the same relative template
    // maps A up one degree to C5 instead of B4.
    assert(DiatonicHarmonizer::harmony_midi_note_custom(
               69, 0, 1, a_major_pentatonic, 5U) == 72);
    assert(DiatonicHarmonizer::harmony_midi_note(
               60, 0, DiatonicScale::Custom, 2) == 64);
    assert(DiatonicHarmonizer::scale_name(DiatonicScale::Custom) == "Custom");

    // Invalid count falls back to four active notes; the short all-zero input
    // is deterministically projected to four valid ascending semitones.
    constexpr std::array<std::uint8_t, 8> repeated{};
    assert(DiatonicHarmonizer::harmony_midi_note_custom(
               60, 0, 1, repeated, 0U) == 61);
    // Counts over the supported eight-note maximum clamp to eight, with enough
    // room reserved for every later note.
    constexpr std::array<std::uint8_t, 8> all_high{
        255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U};
    assert(DiatonicHarmonizer::harmony_midi_note_custom(
               60, 0, 1, all_high, 255U) == 64);
}

void test_custom_scale_config_sanitization_and_allocation_free_processing() {
    DiatonicHarmonizerConfig requested;
    requested.scale = DiatonicScale::Custom;
    requested.key = 7;
    requested.custom_scale_note_count = 6U;
    requested.custom_scale_notes = {9U, 9U, 255U, 1U, 5U, 5U, 8U, 11U};
    custom_scale_check.configure(requested);
    const auto& sanitized = custom_scale_check.config();
    assert(sanitized.scale == DiatonicScale::Custom);
    assert(sanitized.key == 7);
    assert(sanitized.custom_scale_note_count == 6U);
    constexpr std::array<std::uint8_t, 6> expected{6U, 7U, 8U, 9U, 10U, 11U};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        assert(sanitized.custom_scale_notes[index] == expected[index]);
        if (index > 0U) {
            assert(sanitized.custom_scale_notes[index] >=
                   sanitized.custom_scale_notes[index - 1U] + 1U);
        }
    }
    assert(sanitized.custom_scale_notes[6] == 0U &&
           sanitized.custom_scale_notes[7] == 0U);

    requested.custom_scale_note_count = 1U;
    requested.custom_scale_notes = {0U, 2U, 4U, 5U, 7U, 9U, 11U, 0U};
    requested.key = 99;
    custom_scale_check.configure(requested);
    assert(custom_scale_check.config().custom_scale_note_count == 4U);
    assert(custom_scale_check.config().key == 11);
    assert(custom_scale_check.config().custom_scale_notes[0] == 0U &&
           custom_scale_check.config().custom_scale_notes[1] == 2U &&
           custom_scale_check.config().custom_scale_notes[2] == 4U &&
           custom_scale_check.config().custom_scale_notes[3] == 5U);

    requested.custom_scale_note_count = 9U;
    requested.custom_scale_notes.fill(0U);
    custom_scale_check.configure(requested);
    assert(custom_scale_check.config().custom_scale_note_count == 8U);
    for (std::size_t index = 0; index < 8U; ++index) {
        assert(custom_scale_check.config().custom_scale_notes[index] == index);
    }

    // Use a custom A major pentatonic mapping in the live detector path:
    // A3 plus one custom scale degree should produce B3.
    requested.custom_scale_note_count = 5U;
    requested.custom_scale_notes = {0U, 2U, 4U, 7U, 9U, 0U, 0U, 0U};
    requested.key = 9;
    requested.pitch_interval = 1;
    requested.mix = 1.0F;
    requested.delay_ms = 0.0F;
    requested.feedback = 0.0F;
    custom_scale_check.configure(requested);
    custom_scale_check.reset();
    std::array<float, 12000> input{};
    std::array<float, 12000> output{};
    for (std::size_t frame = 0; frame < input.size(); ++frame) {
        input[frame] = 0.2F * std::sin(
            2.0F * 3.14159265358979323846F * 220.0F *
            static_cast<float>(frame) / 48000.0F);
    }
    allocation_count.store(0U, std::memory_order_relaxed);
    track_allocations.store(true, std::memory_order_relaxed);
    custom_scale_check.process(input.data(), nullptr, output.data(), nullptr,
                               input.size());
    const std::size_t observed_allocations = allocation_count.load(
        std::memory_order_relaxed);
    track_allocations.store(false, std::memory_order_relaxed);
    assert(observed_allocations == 0U);
    const float measured = estimate_tone_hz(output.data() + 2500U,
                                            output.size() - 2500U, 48000.0F);
    assert(std::abs(measured - 246.94F) <= 8.0F);
}

void test_extended_scale_degree_audio_bounds() {
    constexpr std::size_t kFrames = 48000U;
    constexpr std::size_t kMeasureStart = 24000U;
    constexpr std::size_t kMeasureFrames = 24000U;
    constexpr float kSampleRate = 48000.0F;
    constexpr int kInputMidi = 60;
    constexpr double kInputHz = 440.0 *
                                0.5946035575013605;  // C4, MIDI 60
    std::array<float, kFrames> input{};
    std::array<float, kFrames> output{};

    for (const int interval : {-24, 24}) {
        DiatonicHarmonizerConfig config;
        config.mix = 1.0F;
        config.pitch_interval = interval;
        config.key = 0;
        config.scale = DiatonicScale::Major;
        wide_pitch_check.configure(config);
        assert(wide_pitch_check.config().pitch_interval == interval);
        wide_pitch_check.reset();

        for (std::size_t frame = 0; frame < kFrames; ++frame) {
            input[frame] = 0.25F * std::sin(
                2.0 * 3.14159265358979323846 * kInputHz *
                static_cast<double>(frame) / kSampleRate);
        }
        wide_pitch_check.process(input.data(), nullptr, output.data(), nullptr,
                                 kFrames);

        const int target_midi = DiatonicHarmonizer::harmony_midi_note(
            kInputMidi, 0, DiatonicScale::Major, interval);
        const double target_hz = 440.0 * std::exp2(
            static_cast<double>(target_midi - 69) / 12.0);
        assert(target_hz >= 20.0 && target_hz < 0.45 * kSampleRate);
        const double amplitude = tone_amplitude_at(
            output.data(), kMeasureStart, kMeasureFrames, target_hz,
            kSampleRate);
        assert(std::isfinite(amplitude) && amplitude > 0.01);
    }
}

void test_config_bounds_and_latency() {
    DiatonicHarmonizerConfig requested;
    requested.mix = std::numeric_limits<float>::quiet_NaN();
    requested.pitch_interval = 99;
    requested.key = -10;
    requested.scale = static_cast<DiatonicScale>(255);
    requested.tone_hz = std::numeric_limits<float>::infinity();
    requested.delay_ms = 900.0F;
    requested.feedback = 2.0F;
    requested.tempo_bpm = 1.0F;
    requested.tap_division = static_cast<TapDivision>(255);
    reset_check.configure(requested);

    const auto& config = reset_check.config();
    assert(std::abs(config.mix - 0.35F) < 1.0e-6F);
    assert(config.pitch_interval == 7);
    assert(config.key == 0);
    assert(config.scale == DiatonicScale::Major);
    assert(std::abs(config.tone_hz - 8000.0F) < 1.0e-6F);
    assert(std::abs(config.delay_ms - 600.0F) < 1.0e-6F);
    assert(std::abs(config.feedback - 0.85F) < 1.0e-6F);
    assert(std::abs(config.tempo_bpm - 30.0F) < 1.0e-6F);
    assert(config.tap_division == TapDivision::Free);
    assert(reset_check.effective_delay_samples() == 28800);

    requested.tap_division = TapDivision::Quarter;
    requested.tempo_bpm = 120.0F;
    reset_check.configure(requested);
    assert(reset_check.effective_delay_samples() == 24000);
    requested.tap_division = TapDivision::DottedEighth;
    reset_check.configure(requested);
    assert(reset_check.effective_delay_samples() == 18000);

    assert(whole_block.latency_samples() == 256);
    assert(whole_block.pitch_control_latency_samples() == 1280);
    assert(std::abs(whole_block.latency_ms() - 5.3333F) < 0.02F);
    assert(std::abs(latency_44100.latency_ms() - 5.805F) < 0.03F);
    assert(std::abs(latency_96000.latency_ms() - 2.6667F) < 0.02F);
    // The stream shifter is under 20 ms, but waiting for the tracker frame
    // still puts first-note control response above the provisional gate.
    assert(whole_block.pitch_control_latency_samples() * 1000.0F / 48000.0F >
           20.0F);
}

void test_block_partition_and_finite_recovery() {
    DiatonicHarmonizerConfig config;
    config.mix = 1.0F;
    config.pitch_interval = 2;
    config.key = 0;
    config.scale = DiatonicScale::Major;
    whole_block.configure(config);
    split_blocks.configure(config);
    whole_block.reset();
    split_blocks.reset();

    constexpr std::size_t kFrames = 12000;
    std::array<float, kFrames> input{};
    std::array<float, kFrames> output_whole{};
    std::array<float, kFrames> output_split{};
    for (std::size_t i = 0; i < kFrames; ++i) {
        input[i] = 0.25F * std::sin(
            2.0F * 3.14159265358979323846F * 220.0F *
            static_cast<float>(i) / 48000.0F);
    }

    whole_block.process(input.data(), nullptr, output_whole.data(), nullptr,
                        kFrames);
    constexpr std::array<std::size_t, 5> chunks = {1, 113, 256, 7, 509};
    std::size_t offset = 0;
    std::size_t chunk_index = 0;
    while (offset < kFrames) {
        const std::size_t count = std::min(
            chunks[chunk_index++ % chunks.size()], kFrames - offset);
        split_blocks.process(input.data() + offset, nullptr,
                             output_split.data() + offset, nullptr, count);
        offset += count;
    }

    for (std::size_t i = 0; i < kFrames; ++i) {
        assert(std::isfinite(output_whole[i]));
        assert(std::abs(output_whole[i] - output_split[i]) < 1.0e-6F);
    }

    std::array<float, 16> exceptional{};
    std::array<float, 16> recovered{};
    exceptional[3] = std::numeric_limits<float>::quiet_NaN();
    exceptional[7] = std::numeric_limits<float>::infinity();
    exceptional[12] = -std::numeric_limits<float>::infinity();
    split_blocks.process(exceptional.data(), nullptr, recovered.data(), nullptr,
                         exceptional.size());
    for (float sample : recovered) {
        assert(std::isfinite(sample));
    }
}

void test_stereo_link_and_reset() {
    reset_check.reset();
    DiatonicHarmonizerConfig config;
    config.mix = 0.5F;
    config.delay_ms = 0.0F;
    config.spillover = false;
    reset_check.configure(config);

    constexpr std::size_t kFrames = 4096;
    std::array<float, kFrames> left{};
    std::array<float, kFrames> right{};
    std::array<float, kFrames> out_left{};
    std::array<float, kFrames> out_right{};
    for (std::size_t i = 0; i < kFrames; ++i) {
        const float phase = 2.0F * 3.14159265358979323846F * 220.0F *
                            static_cast<float>(i) / 48000.0F;
        left[i] = 0.2F * std::sin(phase);
        right[i] = 0.1F * std::sin(phase + 0.4F);
    }
    reset_check.process(left.data(), right.data(), out_left.data(), out_right.data(),
                        kFrames);
    bool stereo_difference_observed = false;
    for (std::size_t i = 0; i < kFrames; ++i) {
        assert(std::isfinite(out_left[i]));
        assert(std::isfinite(out_right[i]));
        if (std::abs(out_left[i] - out_right[i]) > 1.0e-4F) {
            stereo_difference_observed = true;
        }
    }
    assert(stereo_difference_observed);

    reset_check.reset();
    std::array<float, 128> silence{};
    std::array<float, 128> silent_output{};
    reset_check.process(silence.data(), nullptr, silent_output.data(), nullptr,
                        silence.size());
    for (float sample : silent_output) {
        assert(std::isfinite(sample));
        assert(std::abs(sample) < 1.0e-7F);
    }

    reset_check.set_enabled(false);
    assert(!reset_check.enabled());
    reset_check.set_enabled(true);
    assert(reset_check.enabled());
}

void test_detected_input_frequency_api() {
    constexpr std::size_t kFrames = 4096;
    constexpr float kSampleRate = 48000.0F;
    constexpr float kInputHz = 220.0F;
    std::array<float, kFrames> input{};
    std::array<float, kFrames> output{};
    std::array<float, kFrames> low_confidence_noise{};
    std::array<float, kFrames> silence{};

    DiatonicHarmonizerConfig config;
    config.mix = 1.0F;
    config.pitch_interval = 2;
    config.key = 0;
    config.scale = DiatonicScale::Major;
    pitch_check.configure(config);
    pitch_check.reset();
    assert(pitch_check.detected_input_frequency_hz() == 0.0F);

    for (std::size_t i = 0; i < kFrames; ++i) {
        input[i] = 0.2F * std::sin(
            2.0F * 3.14159265358979323846F * kInputHz *
            static_cast<float>(i) / kSampleRate);
    }
    pitch_check.process(input.data(), nullptr, output.data(), nullptr, kFrames);
    const float detected_hz = pitch_check.detected_input_frequency_hz();
    assert(detected_hz > 0.0F);
    assert(std::abs(detected_hz - kInputHz) < 2.0F);
    // The API reports the raw input fundamental, not the C-major harmony voice.
    assert(std::abs(detected_hz - 261.6256F) > 20.0F);
    assert(pitch_check.latency_samples() == 256U);
    assert(pitch_check.pitch_control_latency_samples() == 1280U);

    // A deterministic, aperiodic signal has ample level but should fail the
    // tracker confidence threshold and immediately hide the previous note.
    std::uint32_t state = 0x12345678U;
    for (float& sample : low_confidence_noise) {
        state = state * 1664525U + 1013904223U;
        const float unit = static_cast<float>((state >> 8U) & 0x00ffffffU) /
                           16777215.0F;
        sample = (2.0F * unit - 1.0F) * 0.05F;
    }
    pitch_check.process(low_confidence_noise.data(), nullptr, output.data(),
                        nullptr, kFrames);
    assert(pitch_check.detected_input_frequency_hz() == 0.0F);

    pitch_check.reset();
    assert(pitch_check.detected_input_frequency_hz() == 0.0F);
    pitch_check.process(silence.data(), nullptr, output.data(), nullptr,
                        kFrames);
    assert(pitch_check.detected_input_frequency_hz() == 0.0F);

    pitch_check.process(input.data(), nullptr, output.data(), nullptr, kFrames);
    assert(pitch_check.detected_input_frequency_hz() > 0.0F);
    pitch_check.reset();
    assert(pitch_check.detected_input_frequency_hz() == 0.0F);
}

void test_generated_pitch_and_out_of_scale_correction() {
    constexpr std::size_t kFrames = 12000;
    constexpr std::size_t kMeasureStart = 2500;
    std::array<float, kFrames> input{};
    std::array<float, kFrames> output{};
    const auto check = [&](float input_hz, int interval, float expected_hz) {
        DiatonicHarmonizerConfig config;
        config.mix = 1.0F;
        config.pitch_interval = interval;
        config.key = 0;
        config.scale = DiatonicScale::Major;
        config.delay_ms = 0.0F;
        config.feedback = 0.0F;
        pitch_check.configure(config);
        pitch_check.reset();
        for (std::size_t i = 0; i < kFrames; ++i) {
            input[i] = 0.2F * std::sin(
                2.0F * 3.14159265358979323846F * input_hz *
                static_cast<float>(i) / 48000.0F);
        }
        pitch_check.process(input.data(), nullptr, output.data(), nullptr,
                            kFrames);
        const float measured = estimate_tone_hz(
            output.data() + kMeasureStart, kFrames - kMeasureStart, 48000.0F);
        if (std::abs(measured - expected_hz) > 8.0F) {
            std::fprintf(stderr, "Pitch check failed: input=%.2f Hz interval=%d expected=%.2f Hz measured=%.2f Hz\n",
                         static_cast<double>(input_hz), interval,
                         static_cast<double>(expected_hz),
                         static_cast<double>(measured));
        }
        assert(std::abs(measured - expected_hz) <= 8.0F);
    };

    // A3 + two scale degrees in C major -> C4.
    check(220.0F, 2, 261.63F);
    // C#4 is outside C major and ties downward to C4; the generated voice
    // should land on E4, not F4 from applying the snapped-note interval to C#.
    check(277.18F, 2, 329.63F);
    // C4 - two scale degrees in C major -> A3.
    check(261.63F, -2, 220.0F);
}

void test_in_place_effect_api() {
    DiatonicHarmonizerConfig config;
    config.mix = 0.55F;
    config.pitch_interval = 2;
    config.delay_ms = 0.0F;
    adapter_reference.configure(config);
    adapter_in_place.configure(config);
    adapter_reference.reset();
    adapter_in_place.reset();

    std::array<float, 512> separate_input{};
    std::array<float, 512> in_place{};
    for (std::size_t frame = 0; frame < 256; ++frame) {
        const float phase = 2.0F * 3.14159265358979323846F * 220.0F *
                            static_cast<float>(frame) / 48000.0F;
        separate_input[frame * 2] = 0.2F * std::sin(phase);
        separate_input[frame * 2 + 1] = 0.1F * std::sin(phase + 0.3F);
    }
    in_place = separate_input;
    // The reference API is planar. Prepare equivalent planes before comparing
    // against the interleaved in-place Effect entry point.
    std::array<float, 256> left{};
    std::array<float, 256> right{};
    std::array<float, 256> out_left{};
    std::array<float, 256> out_right{};
    for (std::size_t frame = 0; frame < 256; ++frame) {
        left[frame] = separate_input[frame * 2];
        right[frame] = separate_input[frame * 2 + 1];
    }
    adapter_reference.reset();
    adapter_reference.process(left.data(), right.data(), out_left.data(),
                              out_right.data(), 256);
    adapter_in_place.process(in_place, 2);
    assert(adapter_in_place.id() == "diatonic-harmonizer");
    assert(adapter_in_place.memory_bytes() >= sizeof(adapter_in_place));
    for (std::size_t frame = 0; frame < 256; ++frame) {
        assert(std::isfinite(in_place[frame * 2]));
        assert(std::isfinite(in_place[frame * 2 + 1]));
        assert(std::abs(in_place[frame * 2] - out_left[frame]) < 1.0e-6F);
        assert(std::abs(in_place[frame * 2 + 1] - out_right[frame]) < 1.0e-6F);
    }
}

}  // namespace

int main() {
    test_scale_mapping();
    test_custom_scale_mapping_and_key();
    test_custom_scale_config_sanitization_and_allocation_free_processing();
    test_extended_scale_degree_audio_bounds();
    test_config_bounds_and_latency();
    test_block_partition_and_finite_recovery();
    test_stereo_link_and_reset();
    test_detected_input_frequency_api();
    test_generated_pitch_and_out_of_scale_correction();
    test_in_place_effect_api();
    return 0;
}

void* operator new(std::size_t size) {
    if (track_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (track_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}
