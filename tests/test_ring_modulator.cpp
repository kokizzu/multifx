#include "tabletfx/effects/ring_modulator.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
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

void test_defaults_bounds_and_effective_frequency() {
    tabletfx::RingModulator ring;
    const auto defaults = ring.parameters();
    require(ring.id() == "ring-mod", "ring modulator stable ID changed");
    require(defaults.frequency_hz == 440.0F &&
                defaults.frequency_multiplier == 1.0F &&
                defaults.high_cut_hz == 20000.0F && defaults.mix == 1.0F &&
                defaults.type == tabletfx::RingModulatorType::classic &&
                ring.effective_frequency_hz() == 440.0F,
            "project-authored ring mod defaults changed");
    require(ring.memory_bytes() == sizeof(ring),
            "ring modulator memory report is inaccurate");

    tabletfx::RingModulatorParameters parameters;
    parameters.frequency_hz = -5.0F;
    parameters.frequency_multiplier = 20.0F;
    parameters.high_cut_hz = -1.0F;
    parameters.mix = 2.0F;
    ring.set_parameters(parameters);
    const auto bounded = ring.parameters();
    require(bounded.frequency_hz == 2.0F &&
                bounded.frequency_multiplier == 4.0F &&
                bounded.high_cut_hz == 20.0F && bounded.mix == 1.0F &&
                ring.effective_frequency_hz() == 8.0F,
            "project-authored ring mod parameter ranges were not enforced");

    parameters.frequency_hz = 2000.0F;
    parameters.frequency_multiplier = 4.0F;
    parameters.high_cut_hz = 20000.0F;
    ring.set_parameters(parameters);
    require(ring.parameters().frequency_hz == 2000.0F &&
                ring.parameters().frequency_multiplier == 4.0F &&
                ring.effective_frequency_hz() == 8000.0F,
            "project-authored upper oscillator or multiplier range changed");

    tabletfx::RingModulator low_rate(8000.0F, 1);
    require(low_rate.effective_frequency_hz() <= 3600.0F &&
                low_rate.effective_high_cut_hz() <= 3600.0F,
            "low sample rate did not keep oscillator and high cut below Nyquist");
}

void test_pitch_tracking_guitar_and_bass_ranges() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 48000U;
    const auto track_tone = [](const float input_frequency,
                               const tabletfx::RingModulatorTrackingMode mode) {
        tabletfx::RingModulatorParameters parameters;
        parameters.frequency_hz = 440.0F;
        parameters.frequency_multiplier = 2.0F;
        parameters.pitch_tracking = true;
        parameters.tracking_mode = mode;
        parameters.high_cut_hz = 20000.0F;
        parameters.mix = 1.0F;
        tabletfx::RingModulator ring(sample_rate, 1U, parameters);
        std::vector<float> audio(frames);
        for (std::size_t frame = 0U; frame < frames; ++frame) {
            audio[frame] = 0.5F * std::sin(
                2.0F * std::numbers::pi_v<float> * input_frequency *
                static_cast<float>(frame) / sample_rate);
        }
        ring.process(audio, 1U);
        return std::array<float, 2>{ring.detected_input_frequency_hz(),
                                    ring.effective_frequency_hz()};
    };

    const auto guitar = track_tone(
        110.0F, tabletfx::RingModulatorTrackingMode::guitar);
    require(std::abs(guitar[0] - 110.0F) < 1.5F &&
                std::abs(guitar[1] - 220.0F) < 3.0F,
            "Guitar pitch tracking or frequency multiplier was inaccurate");

    const auto bass = track_tone(
        30.0F, tabletfx::RingModulatorTrackingMode::bass);
    require(std::abs(bass[0] - 30.0F) < 1.5F &&
                std::abs(bass[1] - 60.0F) < 3.0F,
            "Bass pitch tracking did not cover its project low-frequency range");

    const auto low_guitar = track_tone(
        30.0F, tabletfx::RingModulatorTrackingMode::guitar);
    require(low_guitar[0] == 0.0F && std::abs(low_guitar[1] - 880.0F) < 0.01F,
            "Guitar tracking should retain its lower bound and static fallback");
}

double tone_projection_power(const std::vector<float>& samples, float frequency_hz,
                             float sample_rate, std::size_t first_frame) {
    double in_phase = 0.0;
    double quadrature = 0.0;
    for (std::size_t frame = first_frame; frame < samples.size(); ++frame) {
        const double phase = 2.0 * std::numbers::pi_v<double> *
            static_cast<double>(frequency_hz) * static_cast<double>(frame) /
            static_cast<double>(sample_rate);
        in_phase += static_cast<double>(samples[frame]) * std::cos(phase);
        quadrature += static_cast<double>(samples[frame]) * std::sin(phase);
    }
    const double count = static_cast<double>(samples.size() - first_frame);
    return (in_phase * in_phase + quadrature * quadrature) / (count * count);
}

void test_ssb_modes_shift_into_separate_sidebands() {
    constexpr float sample_rate = 48000.0F;
    constexpr float input_frequency = 4000.0F;
    constexpr float shift_frequency = 1000.0F;
    constexpr std::size_t frames = 48000U;
    constexpr std::size_t first_measurement_frame = 4800U;

    std::vector<float> source(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        source[frame] = 0.5F * std::sin(
            2.0F * std::numbers::pi_v<float> * input_frequency *
            static_cast<float>(frame) / sample_rate);
    }

    tabletfx::RingModulatorParameters parameters;
    parameters.type = tabletfx::RingModulatorType::ssb_upper;
    parameters.frequency_hz = shift_frequency;
    parameters.high_cut_hz = 20000.0F;
    parameters.mix = 1.0F;
    tabletfx::RingModulator upper(sample_rate, 1, parameters);
    auto upper_audio = source;
    upper.process(upper_audio, 1);
    require(upper.latency_samples() == 31U,
            "SSB upper did not report its project FIR group delay");
    const double upper_target = tone_projection_power(
        upper_audio, input_frequency + shift_frequency, sample_rate,
        first_measurement_frame);
    const double upper_image = tone_projection_power(
        upper_audio, input_frequency - shift_frequency, sample_rate,
        first_measurement_frame);
    require(upper_target > upper_image * 30.0,
            "SSB upper did not suppress the lower image sideband");
    require(upper_target > 0.01,
            "SSB upper output level was unexpectedly low");

    parameters.type = tabletfx::RingModulatorType::ssb_lower;
    tabletfx::RingModulator lower(sample_rate, 1, parameters);
    auto lower_audio = source;
    lower.process(lower_audio, 1);
    const double lower_target = tone_projection_power(
        lower_audio, input_frequency - shift_frequency, sample_rate,
        first_measurement_frame);
    const double lower_image = tone_projection_power(
        lower_audio, input_frequency + shift_frequency, sample_rate,
        first_measurement_frame);
    require(lower_target > lower_image * 30.0,
            "SSB lower did not suppress the upper image sideband");
    require(lower_target > 0.01,
            "SSB lower output level was unexpectedly low");

    tabletfx::RingModulatorParameters dry_parameters = parameters;
    dry_parameters.mix = 0.0F;
    tabletfx::RingModulator dry(sample_rate, 1, dry_parameters);
    std::array<float, 40> impulse{};
    impulse[0] = 1.0F;
    dry.process(impulse, 1);
    require(impulse[30] == 0.0F && impulse[31] == 1.0F,
            "SSB dry path was not aligned to the FIR group delay");

    lower.reset();
    tabletfx::RingModulator lower_fresh(sample_rate, 1, parameters);
    std::array<float, 128> after_reset{};
    std::array<float, 128> fresh_output{};
    for (std::size_t frame = 0; frame < after_reset.size(); ++frame) {
        const float sample = 0.5F * std::sin(
            2.0F * std::numbers::pi_v<float> * input_frequency *
            static_cast<float>(frame) / sample_rate);
        after_reset[frame] = sample;
        fresh_output[frame] = sample;
    }
    lower.process(after_reset, 1);
    lower_fresh.process(fresh_output, 1);
    require(after_reset == fresh_output,
            "SSB reset did not restore deterministic filter and carrier state");
}

void test_classic_modulation_mix_and_shared_stereo_phase() {
    tabletfx::RingModulatorParameters parameters;
    parameters.frequency_hz = 1000.0F;
    parameters.high_cut_hz = 20000.0F;
    parameters.mix = 1.0F;
    tabletfx::RingModulator mono(48000.0F, 1, parameters);
    std::array<float, 64> constant{};
    constant.fill(1.0F);
    mono.process(constant, 1);
    for (std::size_t frame = 0; frame < constant.size(); ++frame) {
        const float expected = std::sin(
            2.0F * std::numbers::pi_v<float> * 1000.0F *
            static_cast<float>(frame) / 48000.0F);
        require(std::abs(constant[frame] - expected) < 1.5e-2F,
                "classic ring modulation did not multiply by the carrier");
    }

    parameters.mix = 0.0F;
    tabletfx::RingModulator dry(48000.0F, 1, parameters);
    std::array<float, 64> unchanged{};
    for (std::size_t frame = 0; frame < unchanged.size(); ++frame) {
        unchanged[frame] = static_cast<float>(static_cast<int>(frame % 11) - 5) / 5.0F;
    }
    const auto expected_dry = unchanged;
    dry.process(unchanged, 1);
    require(unchanged == expected_dry,
            "zero-mix ring modulation changed the dry signal");

    parameters.mix = 1.0F;
    tabletfx::RingModulator stereo(48000.0F, 2, parameters);
    std::array<float, 256> stereo_input{};
    for (std::size_t frame = 0; frame < stereo_input.size() / 2U; ++frame) {
        stereo_input[frame * 2U] = 1.0F;
        stereo_input[frame * 2U + 1U] = 0.5F;
    }
    stereo.process(stereo_input, 2);
    for (std::size_t frame = 0; frame < stereo_input.size() / 2U; ++frame) {
        require(std::abs(stereo_input[frame * 2U] -
                         2.0F * stereo_input[frame * 2U + 1U]) < 1.0e-6F,
                "stereo channels did not share a carrier phase");
    }
}

void test_ssb_stereo_matches_independent_mono_paths() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 2048U;
    tabletfx::RingModulatorParameters parameters;
    parameters.type = tabletfx::RingModulatorType::ssb_upper;
    parameters.frequency_hz = 375.0F;
    parameters.frequency_multiplier = 1.2F;
    parameters.high_cut_hz = 18000.0F;
    parameters.mix = 1.0F;

    tabletfx::RingModulator stereo(sample_rate, 2, parameters);
    tabletfx::RingModulator left_mono(sample_rate, 1, parameters);
    tabletfx::RingModulator right_mono(sample_rate, 1, parameters);
    std::vector<float> interleaved(frames * 2U);
    std::vector<float> left(frames);
    std::vector<float> right(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float position = static_cast<float>(frame) / sample_rate;
        left[frame] = 0.35F * std::sin(
            2.0F * std::numbers::pi_v<float> * 2700.0F * position + 0.2F);
        right[frame] = 0.2F * std::sin(
            2.0F * std::numbers::pi_v<float> * 5100.0F * position - 0.7F);
        interleaved[frame * 2U] = left[frame];
        interleaved[frame * 2U + 1U] = right[frame];
    }

    stereo.process(interleaved, 2);
    left_mono.process(left, 1);
    right_mono.process(right, 1);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        require(std::abs(interleaved[frame * 2U] - left[frame]) < 1.0e-7F,
                "SSB left channel differs from an independent mono path");
        require(std::abs(interleaved[frame * 2U + 1U] - right[frame]) < 1.0e-7F,
                "SSB right channel differs from an independent mono path");
    }
}

void test_high_cut_reduces_high_frequency_energy() {
    tabletfx::RingModulatorParameters open_parameters;
    open_parameters.frequency_hz = 1000.0F;
    open_parameters.high_cut_hz = 20000.0F;
    open_parameters.mix = 1.0F;
    auto filtered_parameters = open_parameters;
    filtered_parameters.high_cut_hz = 1000.0F;
    tabletfx::RingModulator open(48000.0F, 1, open_parameters);
    tabletfx::RingModulator filtered(48000.0F, 1, filtered_parameters);

    constexpr std::size_t frames = 48000;
    std::vector<float> source(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        source[frame] = std::sin(
            2.0F * std::numbers::pi_v<float> * 6000.0F *
            static_cast<float>(frame) / 48000.0F);
    }
    auto open_audio = source;
    auto filtered_audio = source;
    open.process(open_audio, 1);
    filtered.process(filtered_audio, 1);

    double open_energy = 0.0;
    double filtered_energy = 0.0;
    for (std::size_t frame = 1000; frame < frames; ++frame) {
        open_energy += static_cast<double>(open_audio[frame]) * open_audio[frame];
        filtered_energy += static_cast<double>(filtered_audio[frame]) *
                           filtered_audio[frame];
    }
    require(filtered_energy < open_energy * 0.1,
            "lower high-cut frequency did not reduce upper-frequency energy");
}

void test_block_partition_reset_and_invalid_audio() {
    tabletfx::RingModulatorParameters parameters;
    parameters.frequency_hz = 713.0F;
    parameters.frequency_multiplier = 1.75F;
    parameters.high_cut_hz = 9000.0F;
    parameters.mix = 0.83F;
    tabletfx::RingModulator whole(44100.0F, 2, parameters);
    tabletfx::RingModulator chunks(44100.0F, 2, parameters);
    std::vector<float> input(4096);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = 0.4F * std::sin(static_cast<float>(i) * 0.017F);
    }
    auto expected = input;
    auto actual = input;
    whole.process(expected, 2);
    const std::array<std::size_t, 4> block_sizes{1, 53, 127, 211};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < actual.size()) {
        std::size_t count = std::min(block_sizes[block % block_sizes.size()],
                                     actual.size() - offset);
        count -= count % 2U;
        if (count == 0) {
            count = 2;
        }
        chunks.process(std::span<float>(actual).subspan(offset, count), 2);
        offset += count;
        ++block;
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        require(std::abs(expected[i] - actual[i]) < 1.0e-7F,
                "ring mod output changed with block partitioning");
    }

    whole.reset();
    chunks.reset();
    std::array<float, 32> reset_a{};
    reset_a.fill(0.4F);
    auto reset_b = reset_a;
    whole.process(reset_a, 2);
    chunks.process(reset_b, 2);
    require(reset_a == reset_b, "reset did not restore deterministic state");

    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.5F,
        std::numeric_limits<float>::quiet_NaN(),
        0.25F,
        -0.5F,
        0.75F,
    };
    whole.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "non-finite audio samples did not produce finite output");

    std::array<float, 5> untouched{0.1F, -0.2F, 0.3F, -0.4F, 0.5F};
    const auto before = untouched;
    whole.process(untouched, 1);
    require(untouched == before,
            "mismatched channel layout should leave the buffer untouched");
}

void test_extreme_finite_input_does_not_poison_filter_state() {
    tabletfx::RingModulatorParameters parameters;
    parameters.frequency_hz = 2000.0F;
    parameters.high_cut_hz = 20000.0F;
    parameters.mix = 1.0F;
    tabletfx::RingModulator stressed(48000.0F, 1, parameters);
    tabletfx::RingModulator reference(48000.0F, 1, parameters);

    const float largest = std::numeric_limits<float>::max();
    std::array<float, 8> extreme{};
    extreme.fill(largest);
    extreme.back() = -largest;
    stressed.process(extreme, 1);
    require(std::all_of(extreme.begin(), extreme.end(),
                        [](float sample) { return sample == 0.0F; }),
            "extreme finite samples were not silenced and isolated");

    std::array<float, 8> matching_silence{};
    reference.process(matching_silence, 1);

    std::array<float, 64> after_extremes{};
    std::array<float, 64> after_silence{};
    for (std::size_t index = 0; index < after_extremes.size(); ++index) {
        const float sample = static_cast<float>(static_cast<int>(index % 9U) - 4) *
                             0.125F;
        after_extremes[index] = sample;
        after_silence[index] = sample;
    }
    stressed.process(after_extremes, 1);
    reference.process(after_silence, 1);
    for (std::size_t index = 0; index < after_extremes.size(); ++index) {
        require(std::isfinite(after_extremes[index]) &&
                    std::abs(after_extremes[index] - after_silence[index]) < 1.0e-7F,
                "extreme finite input poisoned the filter state for following audio");
    }
}

void test_processing_does_not_allocate() {
    tabletfx::RingModulator ring;
    tabletfx::RingModulatorParameters parameters;
    parameters.type = tabletfx::RingModulatorType::ssb_upper;
    tabletfx::RingModulator sideband(48000.0F, 2, parameters);
    std::array<float, 512> block{};
    block.fill(0.25F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        ring.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "ring modulator allocated during process()");

    const std::size_t ssb_before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        sideband.process(block, 2);
    }
    const std::size_t ssb_after = allocation_count.load(std::memory_order_relaxed);
    require(ssb_after == ssb_before,
            "SSB ring modulator allocated during process()");

    parameters.type = tabletfx::RingModulatorType::classic;
    parameters.pitch_tracking = true;
    parameters.tracking_mode = tabletfx::RingModulatorTrackingMode::bass;
    tabletfx::RingModulator tracked(48000.0F, 2, parameters);
    const std::size_t tracking_before =
        allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        tracked.process(block, 2);
    }
    const std::size_t tracking_after =
        allocation_count.load(std::memory_order_relaxed);
    require(tracking_after == tracking_before,
            "pitch-tracked ring modulator allocated during process()");
    require(std::all_of(block.begin(), block.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "ring modulator emitted non-finite samples");
}

void test_invalid_construction() {
    bool bad_rate = false;
    try {
        tabletfx::RingModulator invalid(100.0F, 1);
    } catch (const std::invalid_argument&) {
        bad_rate = true;
    }
    require(bad_rate, "ring modulator accepted an unsupported sample rate");

    bool bad_channels = false;
    try {
        tabletfx::RingModulator invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        bad_channels = true;
    }
    require(bad_channels, "ring modulator accepted more than two channels");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_bounds_and_effective_frequency();
        test_pitch_tracking_guitar_and_bass_ranges();
        test_classic_modulation_mix_and_shared_stereo_phase();
        test_ssb_modes_shift_into_separate_sidebands();
        test_ssb_stereo_matches_independent_mono_paths();
        test_high_cut_reduces_high_frequency_energy();
        test_block_partition_reset_and_invalid_audio();
        test_extreme_finite_input_does_not_poison_filter_state();
        test_processing_does_not_allocate();
        test_invalid_construction();
        std::cout << "All TabletFX ring modulator tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
