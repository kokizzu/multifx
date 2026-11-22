#include "tabletfx/effects/celestial_reverb.hpp"

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

int failures = 0;
std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};
constexpr double kPi = 3.14159265358979323846;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

double amplitude_near(std::span<const float> samples, double frequency,
                     std::size_t start_frame, std::size_t frame_count,
                     std::uint16_t channel, std::uint16_t channels) {
    double cosine = 0.0;
    double sine = 0.0;
    double window_sum = 0.0;
    for (std::size_t index = 0; index < frame_count; ++index) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * kPi * static_cast<double>(index) /
            static_cast<double>(frame_count - 1U));
        const double phase = 2.0 * kPi * frequency *
            static_cast<double>(start_frame + index) / 48000.0;
        const double value = static_cast<double>(
            samples[(start_frame + index) * channels + channel]) * window;
        cosine += value * std::cos(phase);
        sine += value * std::sin(phase);
        window_sum += window;
    }
    return 2.0 * std::hypot(cosine, sine) / window_sum;
}

void constructor_validates_rate_and_channel_layout() {
    bool rejects_rate = false;
    try {
        tabletfx::CelestialReverb invalid(7999.0F, 1);
    } catch (const std::invalid_argument&) {
        rejects_rate = true;
    }
    check(rejects_rate, "sample rates below 8 kHz should be rejected");

    bool rejects_channels = false;
    try {
        tabletfx::CelestialReverb invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        rejects_channels = true;
    }
    check(rejects_channels, "channel layouts other than mono/stereo should be rejected");
}

void dual_pitch_controls_are_bounded_and_nonfinite_values_fall_back() {
    tabletfx::CelestialReverb effect;
    tabletfx::CelestialReverbParameters parameters;
    parameters.mix = 3.0F;
    parameters.decay_seconds = 0.01F;
    parameters.pitch_1_semitones = 24.0F;
    parameters.pitch_2_semitones = -24.0F;
    parameters.pitch_decay_seconds = 40.0F;
    parameters.high_cut_hz = 90000.0F;
    parameters.blend = -2.0F;
    parameters.spillover = true;
    effect.configure(parameters);

    const auto actual = effect.parameters();
    check(actual.mix == 1.0F, "Mix should clamp to the project maximum");
    check(actual.decay_seconds == 0.2F,
          "Decay should clamp to the project minimum");
    check(actual.pitch_1_semitones == 12.0F &&
              actual.pitch_2_semitones == -12.0F,
          "both pitch controls should clamp to one octave in either direction");
    check(actual.pitch_decay_seconds == 12.0F,
          "Pitch Decay should clamp to the project maximum");
    check(actual.high_cut_hz <= 20000.0F,
          "High Cut should not exceed the project/sample-rate limit");
    check(actual.blend == 0.0F,
          "Blend should clamp to the project minimum");
    check(actual.spillover,
          "boolean Spillover control should be preserved");

    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    parameters.decay_seconds = std::numeric_limits<float>::infinity();
    parameters.pitch_1_semitones = std::numeric_limits<float>::quiet_NaN();
    effect.configure(parameters);
    const auto finite = effect.parameters();
    check(std::isfinite(finite.mix) && std::isfinite(finite.decay_seconds) &&
              std::isfinite(finite.pitch_1_semitones),
          "non-finite control values should fall back to defaults");
}

void zero_mix_is_a_dry_path_and_channel_mismatch_is_ignored() {
    tabletfx::CelestialReverbParameters parameters;
    parameters.mix = 0.0F;
    tabletfx::CelestialReverb effect(48000.0F, 1, parameters);
    std::array<float, 9> audio{0.25F, -0.5F, 0.75F, -1.0F, 0.2F,
                               0.0F, 0.3F, -0.4F, 0.1F};
    const auto original = audio;
    effect.process(audio, 1);
    check(audio == original, "zero Mix should preserve the input samples");

    auto wrong_channel_call = original;
    effect.process(wrong_channel_call, 2);
    check(wrong_channel_call == original,
          "a channel-count mismatch should leave the buffer unchanged");
}

void stereo_inputs_do_not_cross_channels() {
    tabletfx::CelestialReverbParameters parameters;
    parameters.mix = 1.0F;
    parameters.blend = 0.0F;
    tabletfx::CelestialReverb effect(48000.0F, 2, parameters);
    std::array<float, 4096> audio{};
    for (std::size_t frame = 0; frame < audio.size() / 2U; ++frame) {
        audio[frame * 2U] = 0.2F * std::sin(
            6.28318530718F * 330.0F * static_cast<float>(frame) / 48000.0F);
    }
    effect.process(audio, 2);
    bool finite = std::all_of(audio.begin(), audio.end(), [](float sample) {
        return std::isfinite(sample);
    });
    bool right_silent = true;
    for (std::size_t frame = 0; frame < audio.size() / 2U; ++frame) {
        right_silent = right_silent && audio[frame * 2U + 1U] == 0.0F;
    }
    check(finite, "stereo output should remain finite");
    check(right_silent,
          "left-channel audio should not leak into the right channel");
}

void pitch_controls_shift_only_the_wet_reverb_signal() {
    tabletfx::CelestialReverbParameters parameters;
    parameters.mix = 1.0F;
    parameters.decay_seconds = 2.0F;
    parameters.pitch_1_semitones = 12.0F;
    parameters.pitch_2_semitones = 12.0F;
    parameters.pitch_decay_seconds = 0.2F;
    parameters.high_cut_hz = 18000.0F;
    parameters.blend = 1.0F;
    tabletfx::CelestialReverb effect(48000.0F, 1, parameters);

    constexpr std::size_t total_frames = 24000;
    constexpr std::size_t driven_frames = 16000;
    std::vector<float> audio(total_frames, 0.0F);
    for (std::size_t frame = 0; frame < driven_frames; ++frame) {
        audio[frame] = 0.2F * std::sin(
            2.0 * kPi * 220.0 * static_cast<double>(frame) / 48000.0);
    }
    effect.process(audio, 1);

    constexpr std::size_t analysis_start = 5000;
    constexpr std::size_t analysis_frames = 4096;
    const double octave_up = amplitude_near(audio, 440.0, analysis_start,
                                            analysis_frames, 0, 1);
    const double original = amplitude_near(audio, 220.0, analysis_start,
                                           analysis_frames, 0, 1);
    check(octave_up > 1.0e-4,
          "the wet output should contain the configured pitch-shifted tone");
    check(octave_up > original * 1.1,
          "full Blend should favor the pitch returns over the unshifted wet tone");
    check(std::all_of(audio.begin(), audio.end(), [](float sample) {
              return std::isfinite(sample) && std::abs(sample) <= 16.0F;
          }), "pitched reverb output should remain finite and bounded");
}

void reset_clears_all_reverb_and_pitch_history() {
    tabletfx::CelestialReverbParameters parameters;
    parameters.mix = 0.8F;
    parameters.pitch_decay_seconds = 0.5F;
    tabletfx::CelestialReverb reset_effect(48000.0F, 1, parameters);
    tabletfx::CelestialReverb fresh_effect(48000.0F, 1, parameters);

    std::array<float, 4096> excitation{};
    for (std::size_t frame = 0; frame < excitation.size(); ++frame) {
        excitation[frame] = 0.15F * std::sin(
            2.0 * kPi * 330.0 * static_cast<double>(frame) / 48000.0);
    }
    reset_effect.process(excitation, 1);
    reset_effect.reset();

    std::array<float, 2048> input{};
    for (std::size_t frame = 0; frame < input.size(); ++frame) {
        input[frame] = 0.1F * std::sin(
            2.0 * kPi * 196.0 * static_cast<double>(frame) / 48000.0);
    }
    auto reset_output = input;
    auto fresh_output = input;
    reset_effect.process(reset_output, 1);
    fresh_effect.process(fresh_output, 1);
    for (std::size_t frame = 0; frame < input.size(); ++frame) {
        check(std::abs(reset_output[frame] - fresh_output[frame]) <= 1.0e-6F,
              "reset must clear the reverb, pitch-shifter, and feedback history");
    }
}

void invalid_audio_samples_are_sanitized() {
    tabletfx::CelestialReverbParameters parameters;
    parameters.mix = 1.0F;
    tabletfx::CelestialReverb effect(48000.0F, 2, parameters);
    std::array<float, 256> audio{};
    audio[0] = std::numeric_limits<float>::quiet_NaN();
    audio[1] = std::numeric_limits<float>::infinity();
    audio[2] = -std::numeric_limits<float>::infinity();
    audio[3] = std::numeric_limits<float>::max();
    audio[4] = -std::numeric_limits<float>::max();
    effect.process(audio, 2);
    check(std::all_of(audio.begin(), audio.end(), [](float sample) {
              return std::isfinite(sample) && std::abs(sample) <= 16.0F;
          }), "invalid or oversized audio samples should be sanitized");
}

void spillover_setting_controls_bypassed_wet_tail() {
    tabletfx::CelestialReverbParameters parameters;
    parameters.mix = 1.0F;
    parameters.blend = 0.0F;
    parameters.decay_seconds = 3.0F;
    parameters.spillover = true;
    tabletfx::CelestialReverb tails_on(48000.0F, 1, parameters);
    std::array<float, 4096> excitation{};
    for (std::size_t frame = 0; frame < excitation.size(); ++frame) {
        excitation[frame] = 0.2F * std::sin(
            6.28318530718F * 440.0F * static_cast<float>(frame) / 48000.0F);
    }
    tails_on.process(excitation, 1);
    std::array<float, 512> tail{};
    tails_on.process_bypassed(tail, 1);
    float tail_energy = 0.0F;
    for (float sample : tail) tail_energy += sample * sample;
    check(tail_energy > 1.0e-8F,
          "Spillover should emit the stored wet tail while bypassed");

    tabletfx::CelestialReverb spillover_passthrough(48000.0F, 1, parameters);
    std::array<float, 16> dry_during_bypass{};
    dry_during_bypass.fill(0.125F);
    const auto expected_dry = dry_during_bypass;
    spillover_passthrough.process_bypassed(dry_during_bypass, 1);
    check(dry_during_bypass == expected_dry,
          "Spillover On should pass the bypassed input through");

    parameters.spillover = false;
    tabletfx::CelestialReverb tails_off(48000.0F, 1, parameters);
    std::array<float, 4096> second_excitation{};
    for (std::size_t frame = 0; frame < second_excitation.size(); ++frame) {
        second_excitation[frame] = 0.2F * std::sin(
            6.28318530718F * 440.0F * static_cast<float>(frame) / 48000.0F);
    }
    tails_off.process(second_excitation, 1);
    std::array<float, 512> dry_bypass{};
    dry_bypass.fill(0.125F);
    const auto expected = dry_bypass;
    tails_off.process_bypassed(dry_bypass, 1);
    check(dry_bypass == expected,
          "Spillover Off should clear its tail and leave bypassed dry audio alone");
}

void processing_does_not_allocate_after_construction() {
    tabletfx::CelestialReverb effect;
    std::array<float, 512> audio{};
    audio.fill(0.1F);
    allocation_count.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    effect.process(audio, 2);
    count_allocations.store(false, std::memory_order_relaxed);
    check(allocation_count.load(std::memory_order_relaxed) == 0,
          "process() should not allocate after construction");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    constructor_validates_rate_and_channel_layout();
    dual_pitch_controls_are_bounded_and_nonfinite_values_fall_back();
    zero_mix_is_a_dry_path_and_channel_mismatch_is_ignored();
    stereo_inputs_do_not_cross_channels();
    pitch_controls_shift_only_the_wet_reverb_signal();
    reset_clears_all_reverb_and_pitch_history();
    invalid_audio_samples_are_sanitized();
    spillover_setting_controls_bypassed_wet_tail();
    processing_does_not_allocate_after_construction();
    if (failures != 0) return 1;
    std::cout << "CelestialReverb tests passed.\n";
    return 0;
}
