#include "tabletfx/effects/fractal_diatonic_harmonizer.hpp"
#include "tabletfx/wav.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

using Harmonizer = tabletfx::effects::FractalDiatonicHarmonizer;
using Config = tabletfx::effects::FractalDiatonicHarmonizerConfig;
using Voice = tabletfx::effects::FractalDiatonicVoiceParameters;
using Scale = tabletfx::effects::DiatonicScale;

constexpr float kSampleRate = 48000.0F;
constexpr std::size_t kBlockFrames = 128U;
constexpr double kPi = 3.14159265358979323846;
std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> render_tone(Harmonizer& effect, double frequency,
                               std::size_t frames,
                               std::uint16_t channels = 2U) {
    std::vector<float> result(frames * channels, 0.0F);
    std::array<float, kBlockFrames * 2U> block{};
    double phase = 0.0;
    for (std::size_t offset = 0U; offset < frames;) {
        const std::size_t count = std::min(kBlockFrames, frames - offset);
        for (std::size_t frame = 0U; frame < count; ++frame) {
            const float sample = static_cast<float>(0.2 * std::sin(phase));
            phase += 2.0 * kPi * frequency / static_cast<double>(kSampleRate);
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
            block[frame * channels] = sample;
            if (channels == 2U) block[frame * channels + 1U] = sample;
        }
        const std::size_t samples = count * channels;
        effect.process(std::span<float>(block.data(), samples), channels);
        std::copy_n(block.begin(), samples,
                    result.begin() + static_cast<std::ptrdiff_t>(offset * channels));
        offset += count;
    }
    return result;
}

double amplitude_near(const std::vector<float>& samples, double frequency,
                      std::size_t start_frame, std::size_t frame_count,
                      std::uint16_t channel = 0U) {
    double cosine = 0.0;
    double sine = 0.0;
    double window_sum = 0.0;
    for (std::size_t index = 0U; index < frame_count; ++index) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * kPi * static_cast<double>(index) /
            static_cast<double>(frame_count - 1U));
        const double angle = 2.0 * kPi * frequency *
            static_cast<double>(index) / static_cast<double>(kSampleRate);
        const double value = static_cast<double>(
            samples[(start_frame + index) * 2U + channel]) * window;
        cosine += value * std::cos(angle);
        sine += value * std::sin(angle);
        window_sum += window;
    }
    return 2.0 * std::hypot(cosine, sine) / window_sum;
}

float prerecorded_low_note_reference(const float* samples,
                                    double nominal_frequency_hz) {
    constexpr std::size_t window_size = 4096U;
    if (samples == nullptr) return 0.0F;
    const float minimum_hz = static_cast<float>(nominal_frequency_hz / 1.26);
    const float maximum_hz = static_cast<float>(nominal_frequency_hz * 1.26);
    std::array<double, window_size> centered{};
    double mean = 0.0;
    for (std::size_t index = 0U; index < window_size; ++index) {
        mean += samples[index];
    }
    mean /= static_cast<double>(window_size);
    for (std::size_t index = 0U; index < window_size; ++index) {
        centered[index] = static_cast<double>(samples[index]) - mean;
    }

    const std::size_t minimum_lag = std::max<std::size_t>(
        2U, static_cast<std::size_t>(kSampleRate / maximum_hz));
    const std::size_t maximum_lag = std::min<std::size_t>(
        window_size - 32U,
        static_cast<std::size_t>(kSampleRate / minimum_hz));
    std::array<double, window_size> difference{};
    for (std::size_t lag = minimum_lag; lag <= maximum_lag; ++lag) {
        double sum = 0.0;
        const std::size_t count = window_size - lag;
        for (std::size_t index = 0U; index < count; ++index) {
            const double delta = centered[index] - centered[index + lag];
            sum += delta * delta;
        }
        difference[lag] = sum / static_cast<double>(count);
    }

    double cumulative = 0.0;
    double best_cmnd = 1.0;
    std::size_t best_lag = 0U;
    std::size_t selected_lag = 0U;
    for (std::size_t lag = minimum_lag; lag <= maximum_lag; ++lag) {
        cumulative += difference[lag];
        if (cumulative <= 1.0e-18) continue;
        const double cmnd = difference[lag] * static_cast<double>(lag) /
                            cumulative;
        if (cmnd < best_cmnd) {
            best_cmnd = cmnd;
            best_lag = lag;
        }
        if (cmnd < 0.2 && lag + 1U <= maximum_lag &&
            difference[lag] <= difference[lag + 1U]) {
            selected_lag = lag;
            break;
        }
    }
    if (selected_lag == 0U) selected_lag = best_lag;
    if (selected_lag == 0U) return 0.0F;
    double refined_lag = static_cast<double>(selected_lag);
    if (selected_lag > minimum_lag && selected_lag < maximum_lag) {
        const double before = difference[selected_lag - 1U];
        const double center = difference[selected_lag];
        const double after = difference[selected_lag + 1U];
        const double denominator = before - 2.0 * center + after;
        if (std::abs(denominator) > 1.0e-18) {
            refined_lag += std::clamp(
                0.5 * (before - after) / denominator, -0.5, 0.5);
        }
    }
    return refined_lag > 0.0
        ? static_cast<float>(static_cast<double>(kSampleRate) / refined_lag)
        : 0.0F;
}

void test_defaults_scale_mapping_and_latency() {
    Harmonizer effect;
    const Config defaults = effect.config();
    require(effect.id() == "fractal-diatonic-harmonizer",
            "stable project effect ID changed");
    require(defaults.voice_count == 2U,
            "safe default should activate two voices");
    require(defaults.voices[0].harmony_steps == 2 &&
                defaults.voices[1].harmony_steps == 4,
            "safe default should form project-authored third/fifth voices");
    require(defaults.voices[2].level == 0.0F &&
                defaults.voices[3].level == 0.0F,
            "quad-only voices must start inactive");
    require(effect.latency_samples() == 256U,
            "wet/dry audio latency must match the shared shifter");
    require(std::abs(effect.algorithmic_latency_ms() - 5.333333F) < 0.02F,
            "48 kHz audio latency estimate should be 5.333 ms");
    require(effect.pitch_control_latency_samples() == 1664U,
            "low-note control estimate should include confirmation and shifter");
    require(effect.memory_bytes() > sizeof(effect) &&
                effect.memory_bytes() < 1024U * 1024U,
            "four voice cores and tracker should stay below 1 MiB without delays");

    require(tabletfx::effects::DiatonicHarmonizer::harmony_midi_note(
                60, 0, Scale::Major, 2) == 64,
            "C major third mapping should resolve C4 to E4");
    require(tabletfx::effects::DiatonicHarmonizer::harmony_midi_note(
                62, 0, Scale::Major, 2) == 65,
            "C major third mapping should resolve D4 to F4");
    require(tabletfx::effects::DiatonicHarmonizer::harmony_midi_note(
                60, 0, Scale::Major, 4) == 67,
            "C major fifth mapping should resolve C4 to G4");
    const std::array<std::uint8_t, 8> pentatonic{0U, 2U, 4U, 7U, 9U,
                                                 0U, 0U, 0U};
    require(tabletfx::effects::DiatonicHarmonizer::harmony_midi_note_custom(
                60, 0, 4, pentatonic, 5U) == 69,
            "custom scale degree mapping should resolve C to its fifth scale step");
}

void test_configuration_bounds_and_validation() {
    Harmonizer effect;
    Config config = effect.config();
    config.voice_count = 3U;
    config.key = -2;
    config.scale = static_cast<Scale>(255U);
    config.mix = std::numeric_limits<float>::quiet_NaN();
    config.glide_ms = 1000.0F;
    config.custom_scale_note_count = 2U;
    config.custom_scale_notes = {11U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
    config.voices[0] = {-99, 100.0F, 4.0F, -2.0F, 2500.0F, 2.0F};
    effect.configure(config);
    const Config bounded = effect.config();
    require(bounded.voice_count == 2U && bounded.key == 0 &&
                bounded.scale == Scale::Major,
            "invalid voice count/key/scale must fall back safely");
    require(bounded.mix == 0.5F && bounded.glide_ms == 500.0F,
            "global controls must sanitize finite ranges and NaN");
    require(bounded.custom_scale_note_count == 4U &&
                bounded.custom_scale_notes[0] == 8U &&
                bounded.custom_scale_notes[1] == 9U &&
                bounded.custom_scale_notes[2] == 10U &&
                bounded.custom_scale_notes[3] == 11U,
            "custom scale must be strictly ordered and retain valid cardinality");
    require(bounded.voices[0].harmony_steps == -24 &&
                bounded.voices[0].detune_cents == 50.0F &&
                bounded.voices[0].level == 1.0F &&
                bounded.voices[0].pan == -1.0F &&
                bounded.voices[0].delay_ms == 2000.0F &&
                bounded.voices[0].feedback == 0.9F,
            "per-voice controls must clamp to their documented project bounds");

    Voice bad = bounded.voices[0];
    bad.detune_cents = std::numeric_limits<float>::infinity();
    require(!effect.set_voice_parameters(0U, bad),
            "non-finite per-voice settings must be rejected");
    require(!effect.set_voice_parameters(4U, Voice{}),
            "out-of-range voice index must be rejected");
    std::array<std::uint8_t, 8> custom{0U, 1U, 4U, 6U, 8U, 10U, 11U, 0U};
    require(effect.set_custom_scale(7U, custom),
            "valid custom scale must be accepted");
    require(effect.config().custom_scale_note_count == 7U &&
                effect.config().custom_scale_notes[6] == 11U,
            "custom scale setter must retain all active notes");
}

void test_dual_quad_modes_and_pitch_tracking() {
    Harmonizer effect;
    Config config = effect.config();
    config.mix = 0.0F;
    config.voices[0].level = 0.0F;
    config.voices[1].level = 0.0F;
    effect.configure(config);

    std::array<float, kBlockFrames * 2U> block{};
    double phase = 0.0;
    std::size_t first_mid_detection = 0U;
    for (std::size_t offset = 0U; offset < 48000U; offset += kBlockFrames) {
        for (std::size_t frame = 0U; frame < kBlockFrames; ++frame) {
            const float sample = static_cast<float>(0.2 * std::sin(phase));
            phase += 2.0 * kPi * 220.0 / kSampleRate;
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
            block[frame * 2U] = sample;
            block[frame * 2U + 1U] = sample;
        }
        effect.process(block, 2U);
        if (first_mid_detection == 0U &&
            effect.detected_input_frequency_hz() > 0.0F) {
            first_mid_detection = offset + kBlockFrames;
        }
    }
    require(std::abs(effect.detected_input_frequency_hz() - 220.0F) < 2.0F,
            "midrange guitar note tracker must resolve 220 Hz");
    require(first_mid_detection > 0U && first_mid_detection <= 640U,
            "midrange fast detector should respond within 13.3 ms");
    require(effect.pitch_control_latency_samples() == 1024U,
            "fast path control latency estimate should use its 512-sample window");
    std::cout << "220 Hz first tracker update: " << first_mid_detection
              << " samples ("
              << 1000.0 * static_cast<double>(first_mid_detection) /
                     static_cast<double>(kSampleRate)
              << " ms); update plus measured 256-sample shifter latency: "
              << first_mid_detection + effect.latency_samples() << " samples ("
              << 1000.0 * static_cast<double>(first_mid_detection +
                                               effect.latency_samples()) /
                     static_cast<double>(kSampleRate)
              << " ms); conservative reported bound: "
              << effect.pitch_control_latency_samples() << " samples ("
              << 1000.0 * static_cast<double>(effect.pitch_control_latency_samples()) /
                     static_cast<double>(kSampleRate)
              << " ms)\n";

    config = effect.config();
    config.voice_count = 4U;
    config.voices[2].level = 0.3F;
    config.voices[3].level = 0.3F;
    config.voices[2].harmony_steps = -2;
    config.voices[3].harmony_steps = -4;
    effect.configure(config);
    require(effect.config().voice_count == 4U,
            "quad mode must activate after two-voice default");
    require(effect.memory_bytes() < 1024U * 1024U,
            "switching voice count must not allocate more STFT cores");

    effect.reset();
    phase = 0.0;
    std::size_t first_low_detection = 0U;
    for (std::size_t offset = 0U; offset < 48000U; offset += kBlockFrames) {
        for (std::size_t frame = 0U; frame < kBlockFrames; ++frame) {
            const float sample = static_cast<float>(0.2 * std::sin(phase));
            phase += 2.0 * kPi * 82.4069 / kSampleRate;
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
            block[frame * 2U] = sample;
            block[frame * 2U + 1U] = sample;
        }
        effect.process(block, 2U);
        if (first_low_detection == 0U &&
            effect.detected_input_frequency_hz() > 0.0F) {
            first_low_detection = offset + kBlockFrames;
        }
    }
    require(std::abs(effect.detected_input_frequency_hz() - 82.4069F) < 2.0F,
            "low E guitar fundamental must be resolved by the short path");
    require(first_low_detection > 0U && first_low_detection <= 1024U,
            "confirmed low E 768-sample detector should respond within 21.3 ms");
    require(effect.pitch_control_latency_samples() == 1408U,
            "low note control estimate should include its 768-sample window");
    std::cout << "82.4 Hz first confirmed tracker update: " << first_low_detection
              << " samples ("
              << 1000.0 * static_cast<double>(first_low_detection) /
                     static_cast<double>(kSampleRate)
              << " ms); update plus 256-sample shifter latency: "
              << first_low_detection + effect.latency_samples() << " samples ("
              << 1000.0 * static_cast<double>(first_low_detection +
                                               effect.latency_samples()) /
                     static_cast<double>(kSampleRate)
              << " ms); conservative 768-sample-window bound: "
              << effect.pitch_control_latency_samples() << " samples ("
              << 1000.0 * static_cast<double>(effect.pitch_control_latency_samples()) /
                     static_cast<double>(kSampleRate)
              << " ms)\n";
}

void test_generated_low_string_frets_and_silence_gate() {
    constexpr std::array<double, 9> frequencies{
        61.7355, 82.4069, 87.3071, 92.4986, 98.0, 103.826, 110.0,
        116.541, 123.471};
    for (const double frequency : frequencies) {
        Harmonizer effect(kSampleRate, 1U);
        Config config = effect.config();
        config.mix = 0.0F;
        config.voices[0].level = 0.0F;
        config.voices[1].level = 0.0F;
        effect.configure(config);

        std::array<float, kBlockFrames> block{};
        for (unsigned silent_block = 0U; silent_block < 8U; ++silent_block) {
            effect.process(block, 1U);
        }
        require(effect.detected_input_frequency_hz() == 0.0F,
                "silence must not create a false tracked pitch");

        double phase = 0.0;
        std::size_t first_update = 0U;
        for (std::size_t offset = 0U; offset < 4800U; offset += kBlockFrames) {
            for (std::size_t frame = 0U; frame < kBlockFrames; ++frame) {
                const double fundamental = std::sin(phase);
                const double sample = 0.16 * fundamental +
                    0.055 * std::sin(2.0 * phase) +
                    0.02 * std::sin(3.0 * phase);
                block[frame] = static_cast<float>(sample);
                phase += 2.0 * kPi * frequency /
                         static_cast<double>(kSampleRate);
                if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
            }
            effect.process(block, 1U);
            if (first_update == 0U &&
                effect.detected_input_frequency_hz() > 0.0F) {
                first_update = offset + kBlockFrames;
            }
        }
        const float detected = effect.detected_input_frequency_hz();
        std::cout << "Generated fret input=" << frequency
                  << "Hz tracker=" << detected
                  << "Hz first_update=" << first_update
                  << " samples, plus shifter="
                  << first_update + effect.latency_samples()
                  << " samples, conservative_bound="
                  << effect.pitch_control_latency_samples() << " samples\n";
        require(std::abs(static_cast<double>(detected) - frequency) < 1.5,
                "768-sample low-string path lost a generated fret fundamental");
        require(first_update > 0U && first_update <= 1408U,
                "generated low-string note did not recover promptly after silence");
    }
}

void test_prerecorded_open_low_e_and_nearby_frets_when_available() {
    const std::array<std::filesystem::path, 2> prefixes{
        std::filesystem::path("../../testdata/reference_audio/egfxset-clean/Clean/Bridge"),
        std::filesystem::path("testdata/reference_audio/egfxset-clean/Clean/Bridge")};
    std::filesystem::path directory;
    for (const auto& candidate : prefixes) {
        if (std::filesystem::is_directory(candidate)) {
            directory = candidate;
            break;
        }
    }
    if (directory.empty()) {
        std::cout << "EGFxSet local fixture unavailable; prerecorded low-E check skipped.\n";
        return;
    }

    struct FixtureNote {
        const char* filename;
        double expected_hz;
    };
    constexpr std::array<FixtureNote, 3> notes{{
        {"6-0.wav", 82.4069},
        {"6-1.wav", 87.3071},
        {"6-2.wav", 92.4986},
    }};
    for (const FixtureNote& note : notes) {
        const auto path = directory / note.filename;
        if (!std::filesystem::is_regular_file(path)) {
            std::cout << "EGFxSet low-string fixture missing " << path
                      << "; prerecorded check skipped.\n";
            return;
        }
        auto audio = tabletfx::read_wav(path);
        require(audio.sample_rate == static_cast<std::uint32_t>(kSampleRate) &&
                    audio.channels == 1U,
                "EGFxSet low-string fixture must be 48 kHz mono DI audio");
        Harmonizer effect(kSampleRate, 1U);
        const std::vector<float> source_audio = audio.interleaved;
        Config config = effect.config();
        config.mix = 0.0F;
        config.voices[0].level = 0.0F;
        config.voices[1].level = 0.0F;
        effect.configure(config);

        const std::size_t maximum_frames = std::min<std::size_t>(
            audio.frames(), static_cast<std::size_t>(kSampleRate / 4U));
        std::size_t first_raw_update = 0U;
        float first_raw_estimate_hz = 0.0F;
        std::size_t first_confirmed_update = 0U;
        float first_confirmed_hz = 0.0F;
        std::size_t first_accurate_update = 0U;
        std::size_t accurate_updates = 0U;
        std::size_t reference_compared_updates = 0U;
        std::size_t reference_mismatches = 0U;
        float first_confirmed_reference_hz = 0.0F;
        std::vector<double> reference_cents_errors;
        for (std::size_t offset = 0U; offset < maximum_frames;
             offset += kBlockFrames) {
            const std::size_t count = std::min(
                kBlockFrames, maximum_frames - offset);
            auto block = std::span<float>(audio.interleaved.data() + offset,
                                          count);
            effect.process(block, 1U);
            const float candidate = effect.candidate_input_frequency_hz();
            if (first_raw_update == 0U && candidate > 0.0F) {
                first_raw_update = offset + count;
                first_raw_estimate_hz = candidate;
            }
            const float detected = effect.detected_input_frequency_hz();
            if (first_confirmed_update == 0U && detected > 0.0F) {
                first_confirmed_update = offset + count;
                first_confirmed_hz = detected;
                if (offset + 4096U <= source_audio.size()) {
                    first_confirmed_reference_hz =
                        prerecorded_low_note_reference(
                            source_audio.data() + offset, note.expected_hz);
                }
            }
            if (detected > 0.0F &&
                offset + 4096U <= source_audio.size()) {
                const float reference_hz = prerecorded_low_note_reference(
                    source_audio.data() + offset, note.expected_hz);
                if (reference_hz > 0.0F) {
                    ++reference_compared_updates;
                    const double cents_error = std::abs(
                        1200.0 * std::log2(
                            static_cast<double>(detected) / reference_hz));
                    reference_cents_errors.push_back(cents_error);
                    if (cents_error <= 50.0) {
                        ++accurate_updates;
                    } else {
                        ++reference_mismatches;
                    }
                    if (first_accurate_update == 0U &&
                        cents_error <= 50.0) {
                        first_accurate_update = offset + count;
                    }
                }
            }
        }
        std::sort(reference_cents_errors.begin(),
                  reference_cents_errors.end());
        const double p95_cents_error = reference_cents_errors.empty()
            ? std::numeric_limits<double>::infinity()
            : reference_cents_errors[static_cast<std::size_t>(
                  std::ceil(0.95 * reference_cents_errors.size()) - 1.0)];
        const double maximum_cents_error = reference_cents_errors.empty()
            ? std::numeric_limits<double>::infinity()
            : reference_cents_errors.back();
        std::cout << "EGFxSet " << note.filename
                  << " expected=" << note.expected_hz
                  << "Hz first_raw=" << first_raw_estimate_hz
                  << "Hz at " << first_raw_update
                  << " samples; first_confirmed=" << first_confirmed_hz
                  << "Hz at " << first_confirmed_update
                  << " samples; reference there="
                  << first_confirmed_reference_hz << " Hz"
                  << "; first_in_tune=" << first_accurate_update
                  << " samples; tracker+shifter="
                  << 1000.0 * static_cast<double>(first_accurate_update +
                                                    effect.latency_samples()) /
                         static_cast<double>(kSampleRate)
                  << " ms; reference-matched updates=" << accurate_updates
                  << '/' << reference_compared_updates
                  << "; <=50-cent p95=" << p95_cents_error
                  << " cents; max=" << maximum_cents_error
                  << " cents; >50-cent updates=" << reference_mismatches
                  << '\n';
        require(first_accurate_update > 0U,
                "prerecorded low-string note never reached its expected fundamental");
        require(reference_compared_updates >= 8U && p95_cents_error <= 50.0,
                "prerecorded low-string p95 tuning error exceeded 50 cents");
        require(accurate_updates >= 8U,
                "prerecorded low-string fret note was not stable on the short path");
    }
}

void test_tonal_output_mix_alignment_and_safe_state() {
    Harmonizer effect;
    Config config = effect.config();
    config.mix = 1.0F;
    config.scale = Scale::Major;
    config.key = 0;
    config.voice_count = 2U;
    config.voices[0] = {2, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F};
    config.voices[1].level = 0.0F;
    config.voices[2].level = 0.0F;
    config.voices[3].level = 0.0F;
    effect.configure(config);
    const auto rendered = render_tone(effect, 261.625565, 48000U);
    const double input_level = amplitude_near(rendered, 261.625565,
                                               24000U, 12000U);
    const double third_level = amplitude_near(rendered, 329.627557,
                                               24000U, 12000U);
    require(third_level > 0.03,
            "C major third voice must produce a sustained E4 component");
    require(third_level > input_level * 1.5,
            "wet-only harmonizer output should favor the configured third");

    Harmonizer dry_effect(kSampleRate, 1U);
    Config dry_config = dry_effect.config();
    dry_config.mix = 0.0F;
    dry_config.voices[0].level = 0.0F;
    dry_config.voices[1].level = 0.0F;
    dry_effect.configure(dry_config);
    dry_effect.reset();
    std::array<float, 512> impulse{};
    impulse[0] = 1.0F;
    dry_effect.process(impulse, 1U);
    std::size_t first_nonzero = impulse.size();
    for (std::size_t index = 0U; index < impulse.size(); ++index) {
        if (std::abs(impulse[index]) > 0.5F) {
            first_nonzero = index;
            break;
        }
    }
    require(first_nonzero == 256U,
            "dry signal must align with the shifter's 256-sample latency");

    effect.reset();
    std::array<float, 512> invalid{};
    invalid[5] = std::numeric_limits<float>::quiet_NaN();
    invalid[9] = std::numeric_limits<float>::infinity();
    effect.process(invalid, 2U);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float value) { return std::isfinite(value); }),
            "non-finite input must recover to finite output");
    require(effect.detected_input_frequency_hz() == 0.0F,
            "reset must clear detector state");

    effect.set_enabled(false);
    std::array<float, 16> bypass{0.1F, -0.1F, 0.2F, -0.2F,
                                0.3F, -0.3F, 0.4F, -0.4F,
                                0.1F, -0.1F, 0.2F, -0.2F,
                                0.3F, -0.3F, 0.4F, -0.4F};
    const auto expected = bypass;
    effect.process(bypass, 2U);
    require(bypass == expected, "disabled effect should pass input unchanged");
    effect.set_enabled(true);
    require(effect.detected_input_frequency_hz() == 0.0F,
            "enable transition must reset stale pitch estimates");
}

void test_delay_memory_and_no_process_allocations() {
    Harmonizer effect;
    const std::size_t default_memory = effect.memory_bytes();
    Config config = effect.config();
    config.voices[0].delay_ms = 10.0F;
    effect.configure(config);
    const std::size_t delayed_memory = effect.memory_bytes();
    require(delayed_memory >= default_memory + 48000U * 2U * sizeof(float),
            "first delay use should reserve its two-second voice ring outside process");

    std::array<float, kBlockFrames * 2U> block{};
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    for (unsigned iteration = 0U; iteration < 12U; ++iteration) {
        effect.process(block, 2U);
    }
    count_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "audio process must not allocate after delay setup");
}

void test_custom_scale_and_key_learning() {
    Harmonizer effect;
    Config config = effect.config();
    config.mix = 0.0F;
    config.scale = Scale::Custom;
    config.key = 0;
    config.learn = true;
    config.voices[0].harmony_steps = 2;
    config.voices[0].level = 0.0F;
    effect.configure(config);
    const auto rendered = render_tone(effect, 293.664768, 24000U);
    require(effect.detected_input_frequency_hz() > 290.0F &&
                effect.detected_input_frequency_hz() < 297.0F,
            "learn/custom path must detect a D4 input");
    require(effect.config().key == 2,
            "Learn should follow the input pitch class as a project behavior");
    require(rendered.size() == 48000U,
            "stereo custom-scale rendering should preserve frame count");
}

}  // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
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
        test_defaults_scale_mapping_and_latency();
        test_configuration_bounds_and_validation();
        test_dual_quad_modes_and_pitch_tracking();
        test_tonal_output_mix_alignment_and_safe_state();
        test_delay_memory_and_no_process_allocations();
        test_custom_scale_and_key_learning();
        test_generated_low_string_frets_and_silence_gate();
        test_prerecorded_open_low_e_and_nearby_frets_when_available();
        std::cout << "All Fractal Diatonic Harmonizer tests passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAILED: " << exception.what() << '\n';
        return 1;
    }
}
