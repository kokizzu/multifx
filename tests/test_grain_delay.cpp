#include "tabletfx/effects/grain_delay.hpp"

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

double energy(std::span<const float> audio) {
    double sum = 0.0;
    for (const float sample : audio) {
        sum += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return sum;
}

double positive_zero_crossing_frequency(std::span<const float> audio,
                                        double sample_rate) {
    if (audio.size() < 2U) return 0.0;
    std::size_t crossings = 0;
    for (std::size_t index = 1; index < audio.size(); ++index) {
        if (audio[index - 1U] <= 0.0F && audio[index] > 0.0F) {
            ++crossings;
        }
    }
    return static_cast<double>(crossings) * sample_rate /
           static_cast<double>(audio.size() - 1U);
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

void test_identity_bounds_and_fixed_memory() {
    tabletfx::GrainDelay delay;
    require(delay.id() == "grain-delay",
            "Grain Delay stable identifier changed");
    require(delay.memory_bytes() >=
                sizeof(delay) + delay.capacity_frames() * sizeof(float),
            "Grain Delay memory estimate omitted or mis-sized its ring");
    require(delay.capacity_frames() >= 109800U,
            "ring does not cover maximum delay, spray, and negative-pitch grain travel");

    auto parameters = delay.parameters();
    parameters.time_ms = std::numeric_limits<float>::infinity();
    parameters.feedback = 5.0F;
    parameters.mix = -1.0F;
    parameters.grain_size_ms = 900.0F;
    parameters.overlap = 9;
    parameters.pitch_semitones = -90.0F;
    parameters.spray_ms = 900.0F;
    delay.configure(parameters);
    const auto bounded = delay.parameters();
    require(bounded.time_ms == 350.0F && bounded.feedback == 0.9F &&
                bounded.mix == 0.0F && bounded.grain_size_ms == 250.0F &&
                bounded.overlap == 4U && bounded.pitch_semitones == -24.0F &&
                bounded.spray_ms == 100.0F,
            "Grain Delay controls did not apply configured project bounds");

    parameters.time_ms = 0.0F;
    parameters.feedback = -1.0F;
    parameters.mix = 2.0F;
    parameters.grain_size_ms = -5.0F;
    parameters.overlap = 0U;
    parameters.pitch_semitones = 90.0F;
    parameters.spray_ms = -10.0F;
    delay.configure(parameters);
    const auto low_bounded = delay.parameters();
    require(low_bounded.time_ms == 20.0F && low_bounded.feedback == 0.0F &&
                low_bounded.mix == 1.0F && low_bounded.grain_size_ms == 10.0F &&
                low_bounded.overlap == 1U &&
                low_bounded.pitch_semitones == 24.0F &&
                low_bounded.spray_ms == 0.0F,
            "Grain Delay lower bounds were not applied");
}

void test_grains_produce_a_delayed_wet_texture() {
    tabletfx::GrainDelayParameters parameters;
    parameters.time_ms = 20.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.grain_size_ms = 10.0F;
    parameters.overlap = 2U;
    parameters.pitch_semitones = 0.0F;
    parameters.spray_ms = 0.0F;
    tabletfx::GrainDelay delay(8000.0F, 1, parameters);

    std::array<float, 1024> audio{};
    std::fill(audio.begin(), audio.begin() + 80, 0.5F);
    delay.process(audio, 1);
    require(energy(std::span<const float>(audio).first(150U)) == 0.0,
            "wet grains appeared before the configured delay");
    require(energy(std::span<const float>(audio).subspan(160U)) > 0.1,
            "granular delay produced no delayed wet output");
}

void test_hann_window_fades_each_grain_edge() {
    tabletfx::GrainDelayParameters parameters;
    parameters.time_ms = 20.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.grain_size_ms = 10.0F;
    parameters.overlap = 1U;
    parameters.pitch_semitones = 0.0F;
    parameters.spray_ms = 0.0F;
    tabletfx::GrainDelay delay(8000.0F, 1, parameters);

    std::array<float, 512> audio{};
    std::fill(audio.begin(), audio.begin() + 80, 0.5F);
    delay.process(audio, 1);

    require(std::abs(audio[160U]) < 1.0e-7F,
            "Hann window did not fade in at the grain start");
    require(audio[200U] > 0.45F,
            "Hann window did not reach its center level");
    require(std::abs(audio[239U]) < 1.0e-6F,
            "Hann window did not fade out at the grain end");
}

void test_pitch_changes_playback_and_grain_length_stays_in_history() {
    tabletfx::GrainDelayParameters base;
    base.time_ms = 20.0F;
    base.feedback = 0.0F;
    base.mix = 1.0F;
    base.grain_size_ms = 40.0F;
    base.overlap = 1U;
    base.spray_ms = 0.0F;

    auto pitched = base;
    pitched.pitch_semitones = 12.0F;
    tabletfx::GrainDelay unshifted(8000.0F, 1, base);
    tabletfx::GrainDelay shifted(8000.0F, 1, pitched);
    std::array<float, 1024> dry{};
    for (std::size_t i = 0; i < dry.size(); ++i) {
        dry[i] = 0.5F * std::sin(
            2.0F * 3.14159265358979323846F * 440.0F *
            static_cast<float>(i) / 8000.0F);
    }
    auto unshifted_audio = dry;
    auto shifted_audio = dry;
    unshifted.process(unshifted_audio, 1);
    shifted.process(shifted_audio, 1);
    const auto measurement = std::span<const float>(shifted_audio).subspan(180U, 120U);
    const auto reference = std::span<const float>(unshifted_audio).subspan(180U, 120U);
    const double shifted_hz = positive_zero_crossing_frequency(measurement, 8000.0);
    const double unshifted_hz = positive_zero_crossing_frequency(reference, 8000.0);
    require(unshifted_hz > 350.0 && unshifted_hz < 530.0,
            "zero-semitone grains did not preserve the 440 Hz input pitch");
    require(shifted_hz > unshifted_hz * 1.8 && shifted_hz < unshifted_hz * 2.2,
            "one-octave grain shift did not produce approximately twice the input frequency");
    require(std::all_of(shifted_audio.begin(), shifted_audio.end(),
                        [](float value) { return std::isfinite(value); }),
            "short-delay, high-pitch grains read invalid audio history");
}

void test_stereo_isolation_and_parameter_change_preserves_active_grains() {
    tabletfx::GrainDelayParameters parameters;
    parameters.time_ms = 20.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.grain_size_ms = 10.0F;
    parameters.overlap = 2U;
    tabletfx::GrainDelay stereo(8000.0F, 2, parameters);
    std::array<float, 1024> audio{};
    for (std::size_t frame = 0; frame < 64U; ++frame) {
        audio[frame * 2U] = 0.5F;
    }
    stereo.process(std::span<float>(audio).first(2U), 2);
    auto next = parameters;
    next.pitch_semitones = 7.0F;
    stereo.configure(next);
    stereo.process(std::span<float>(audio).subspan(2U), 2);
    bool left_wet = false;
    for (std::size_t frame = 80U; frame < audio.size() / 2U; ++frame) {
        left_wet = left_wet || std::abs(audio[frame * 2U]) > 1.0e-7F;
        require(std::abs(audio[frame * 2U + 1U]) < 1.0e-8F,
                "left-only grains leaked into the right channel");
    }
    require(left_wet,
            "active grains were lost after a parameter change");
}

void test_stereo_input_keeps_a_linked_grain_image() {
    tabletfx::GrainDelayParameters parameters;
    parameters.time_ms = 25.0F;
    parameters.feedback = 0.2F;
    parameters.mix = 0.8F;
    parameters.grain_size_ms = 12.0F;
    parameters.overlap = 3U;
    parameters.pitch_semitones = 5.0F;
    parameters.spray_ms = 20.0F;
    tabletfx::GrainDelay delay(8000.0F, 2, parameters);

    std::array<float, 2048> audio{};
    for (std::size_t frame = 0; frame < 200U; ++frame) {
        const float sample = 0.35F * std::sin(
            static_cast<float>(frame) * 0.037F);
        audio[frame * 2U] = sample;
        audio[frame * 2U + 1U] = sample;
    }
    delay.process(audio, 2);
    for (std::size_t frame = 0; frame < audio.size() / 2U; ++frame) {
        require(audio[frame * 2U] == audio[frame * 2U + 1U],
                "linked stereo grains changed a centered input image");
    }
}

void test_deterministic_spray_reset_and_irregular_blocks() {
    tabletfx::GrainDelayParameters parameters;
    parameters.time_ms = 25.0F;
    parameters.feedback = 0.35F;
    parameters.mix = 0.8F;
    parameters.grain_size_ms = 12.0F;
    parameters.overlap = 3U;
    parameters.pitch_semitones = -5.0F;
    parameters.spray_ms = 20.0F;
    tabletfx::GrainDelay whole(8000.0F, 1, parameters);
    tabletfx::GrainDelay split(8000.0F, 1, parameters);

    std::vector<float> input(6000U, 0.0F);
    for (std::size_t i = 0; i < 600U; ++i) {
        input[i] = 0.4F * std::sin(static_cast<float>(i) * 0.071F);
    }
    auto whole_audio = input;
    auto split_audio = input;
    whole.process(whole_audio, 1);
    std::size_t offset = 0;
    for (const std::size_t block : {1U, 17U, 83U, 7U, 256U, 31U}) {
        const std::size_t count = std::min(block, split_audio.size() - offset);
        split.process(std::span<float>(split_audio).subspan(offset, count), 1);
        offset += count;
    }
    while (offset < split_audio.size()) {
        const std::size_t count =
            std::min<std::size_t>(113U, split_audio.size() - offset);
        split.process(std::span<float>(split_audio).subspan(offset, count), 1);
        offset += count;
    }
    require(whole_audio == split_audio,
            "sprayed grains changed output across irregular process blocks");

    whole.reset();
    auto repeated = input;
    whole.process(repeated, 1);
    require(repeated == whole_audio,
            "reset did not restore the deterministic grain/random sequence");
}

void test_nonfinite_audio_recovery_and_no_process_allocation() {
    tabletfx::GrainDelay delay(8000.0F, 2);
    std::array<float, 256> invalid{};
    invalid[0] = std::numeric_limits<float>::quiet_NaN();
    invalid[1] = std::numeric_limits<float>::infinity();
    invalid[2] = 0.5F;
    invalid[3] = -0.25F;
    delay.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float value) { return std::isfinite(value); }),
            "nonfinite audio contaminated Grain Delay output/history");

    std::array<float, 512> block{};
    block.fill(0.1F);
    const std::size_t before =
        allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        delay.process(block, 2);
    }
    const std::size_t after =
        allocation_count.load(std::memory_order_relaxed);
    require(after == before, "Grain Delay process() allocated memory");
}

void test_constructor_rejects_unsupported_layouts() {
    bool bad_rate = false;
    try {
        tabletfx::GrainDelay invalid(1000.0F, 1);
    } catch (const std::invalid_argument&) {
        bad_rate = true;
    }
    require(bad_rate, "unsupported sample rate was accepted");

    bool bad_channels = false;
    try {
        tabletfx::GrainDelay invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        bad_channels = true;
    }
    require(bad_channels, "three-channel layout was accepted");
}

} // namespace

int main() {
    try {
        test_identity_bounds_and_fixed_memory();
        test_grains_produce_a_delayed_wet_texture();
        test_hann_window_fades_each_grain_edge();
        test_pitch_changes_playback_and_grain_length_stays_in_history();
        test_stereo_isolation_and_parameter_change_preserves_active_grains();
        test_stereo_input_keeps_a_linked_grain_image();
        test_deterministic_spray_reset_and_irregular_blocks();
        test_nonfinite_audio_recovery_and_no_process_allocation();
        test_constructor_rejects_unsupported_layouts();
        std::cout << "Grain Delay tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Grain Delay test failure: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
