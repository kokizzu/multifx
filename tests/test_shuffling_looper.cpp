#include "tabletfx/effects/shuffling_looper.hpp"

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
#include <span>
#include <stdexcept>

using tabletfx::effects::ShufflingLooper;
using tabletfx::effects::ShufflingLooperParameters;
using tabletfx::effects::ShufflingLooperState;

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};

void require(const bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void configure_ordered(ShufflingLooper& looper, const std::uint8_t slices,
                       const std::uint8_t sequence_length) {
    ShufflingLooperParameters parameters = looper.parameters();
    parameters.slices = slices;
    parameters.sequence_length = sequence_length;
    parameters.shuffle = 0.0F;
    parameters.pitch_probability = 0.0F;
    parameters.reverse_probability = 0.0F;
    parameters.repeat_probability = 0.0F;
    parameters.smoothing = 0.0F;
    parameters.sequence_drift = 0.0F;
    parameters.playback = 1.0F;
    parameters.low_cut_hz = 0.0F;
    parameters.high_cut_hz = 0.0F;
    looper.configure(parameters);
}

void record_samples(ShufflingLooper& looper, std::span<float> samples,
                    const std::uint16_t channels) {
    require(looper.record(), "record command should start a recording");
    looper.process(samples, channels);
    require(looper.stop_recording(), "nonempty recording should start playback");
}

void test_identity_bounds_and_constructor_validation() {
    ShufflingLooper stereo;
    require(stereo.id() == "shuffling-looper", "looper ID must be stable");
    require(stereo.channels() == 2U, "default channel layout should be stereo");
    require(stereo.state() == ShufflingLooperState::Stopped,
            "new looper should be stopped");
    require(stereo.loop_frames() == 0U, "new looper should be empty");
    require(stereo.memory_bytes() <= 1'600'000U,
            "stereo storage must stay within the fixed memory budget");
    require(ShufflingLooper::kMaximumFrames == 192'000U,
            "fixed capacity should equal four seconds at 48 kHz");

    ShufflingLooper invalid_rate(1000.0F, 1U);
    require(invalid_rate.sample_rate() == ShufflingLooper::kDefaultSampleRate,
            "unsupported sample rate should use the safe default");

    bool rejected = false;
    try {
        ShufflingLooper invalid_channels(48000.0F, 3U);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "unsupported channel count must be rejected");
}

void test_record_to_sequence_playback_and_stop_resume() {
    ShufflingLooper looper(48000.0F, 1U, 123U);
    configure_ordered(looper, 2U, 4U);
    std::array<float, 8> source{0.1F, 0.2F, 0.3F, 0.4F,
                                0.5F, 0.6F, 0.7F, 0.8F};
    record_samples(looper, source, 1U);
    require(looper.loop_frames() == source.size(), "recorded frame count is wrong");
    require(looper.sequence_steps() == 4U, "configured sequence length is wrong");
    require(looper.sequence_slice(0U) == 0U && looper.sequence_slice(1U) == 1U &&
                looper.sequence_slice(2U) == 0U && looper.sequence_slice(3U) == 1U,
            "zero shuffle should follow ordered slices and repeat them as needed");

    std::array<float, 8> first_playback{};
    looper.process(first_playback, 1U);
    for (std::size_t i = 0U; i < source.size(); ++i) {
        require(std::abs(first_playback[i] - source[i]) < 1.0e-6F,
                "record-stop should immediately play the ordered slices");
    }

    looper.stop();
    require(looper.state() == ShufflingLooperState::Stopped,
            "stop should stop playback");
    require(looper.loop_frames() == source.size(), "stop must retain recorded audio");
    require(looper.play(), "retained loop should restart");
    std::array<float, 8> resumed{};
    looper.process(resumed, 1U);
    require(resumed == first_playback, "play should restart the same sequence");
}

void test_stereo_channels_and_dry_mix() {
    ShufflingLooper looper(48000.0F, 2U, 9U);
    configure_ordered(looper, 2U, 2U);
    std::array<float, 8> source{0.1F, -0.1F, 0.2F, -0.2F,
                               0.3F, -0.3F, 0.4F, -0.4F};
    record_samples(looper, source, 2U);

    std::array<float, 8> output{0.01F, -0.02F, 0.03F, -0.04F,
                                0.05F, -0.06F, 0.07F, -0.08F};
    const auto dry = output;
    looper.process(output, 2U);
    for (std::size_t i = 0U; i < output.size(); ++i) {
        require(std::abs(output[i] - (source[i] + dry[i])) < 1.0e-6F,
                "stereo playback should preserve channels and mix with dry input");
    }
}

void test_replacement_is_destructive_but_playback_is_non_destructive() {
    ShufflingLooper looper(48000.0F, 1U, 55U);
    configure_ordered(looper, 2U, 2U);
    std::array<float, 4> first{0.1F, 0.2F, 0.3F, 0.4F};
    record_samples(looper, first, 1U);

    std::array<float, 4> playback_a{};
    looper.process(playback_a, 1U);
    require(looper.loop_frames() == first.size(),
            "playback must not consume or mutate loop length");
    require(looper.play(), "loop should be replayable");
    std::array<float, 4> playback_b{};
    looper.process(playback_b, 1U);
    require(playback_a == playback_b, "playback must not mutate recorded samples");

    std::array<float, 4> replacement{0.8F, 0.7F, 0.6F, 0.5F};
    require(looper.record(), "record should start destructive replacement");
    require(looper.loop_frames() == 0U,
            "starting a replacement should make the prior loop unavailable");
    looper.process(replacement, 1U);
    require(looper.stop_recording(), "replacement recording should finalize");
    std::array<float, 4> replacement_playback{};
    looper.process(replacement_playback, 1U);
    require(replacement_playback == replacement,
            "replacement recording should be the only loop that remains");
}

void test_random_sequence_and_drift_are_deterministic() {
    ShufflingLooper left(48000.0F, 1U, 4567U);
    ShufflingLooper right(48000.0F, 1U, 4567U);
    ShufflingLooperParameters parameters = left.parameters();
    parameters.slices = 4U;
    parameters.sequence_length = 8U;
    parameters.shuffle = 1.0F;
    parameters.pitch_probability = 1.0F;
    parameters.reverse_probability = 0.5F;
    parameters.repeat_probability = 0.25F;
    parameters.sequence_drift = 1.0F;
    parameters.playback = 1.0F;
    left.configure(parameters);
    right.configure(parameters);

    std::array<float, 16> left_source{};
    std::array<float, 16> right_source{};
    for (std::size_t i = 0U; i < left_source.size(); ++i) {
        left_source[i] = right_source[i] = static_cast<float>(i + 1U) / 32.0F;
    }
    record_samples(left, left_source, 1U);
    record_samples(right, right_source, 1U);
    for (std::size_t i = 0U; i < left.sequence_steps(); ++i) {
        require(left.sequence_slice(i) == right.sequence_slice(i),
                "identical seeds and inputs should create identical sequences");
    }

    require(left.randomize_sequence() && right.randomize_sequence(),
            "recorded loop should allow explicit sequence randomization");
    for (std::size_t i = 0U; i < left.sequence_steps(); ++i) {
        require(left.sequence_slice(i) == right.sequence_slice(i),
                "explicit randomization should remain reproducible by seed");
    }
    std::array<float, 512> left_audio{};
    std::array<float, 512> right_audio{};
    left.process(left_audio, 1U);
    right.process(right_audio, 1U);
    require(left_audio == right_audio,
            "shuffle, pitch, reverse, repeat, and drift should be deterministic");
}

void test_controls_are_bounded_and_nonfinite_controls_fall_back() {
    ShufflingLooper looper;
    ShufflingLooperParameters parameters = looper.parameters();
    parameters.slices = 0U;
    parameters.sequence_length = std::numeric_limits<std::uint8_t>::max();
    parameters.shuffle = 4.0F;
    parameters.pitch_probability = -1.0F;
    parameters.reverse_probability = 0.5F;
    parameters.repeat_probability = 2.0F;
    parameters.smoothing = std::numeric_limits<float>::quiet_NaN();
    parameters.sequence_drift = -1.0F;
    parameters.playback = 5.0F;
    parameters.low_cut_hz = 10.0F;
    parameters.high_cut_hz = 50000.0F;
    parameters.interval1_semitones = std::numeric_limits<std::int8_t>::min();
    parameters.interval2_semitones = std::numeric_limits<std::int8_t>::max();
    looper.configure(parameters);

    const auto& bounded = looper.parameters();
    require(bounded.slices == 2U && bounded.sequence_length == 32U,
            "slice and sequence counts should be clamped");
    require(bounded.shuffle == 1.0F && bounded.pitch_probability == 0.0F &&
                bounded.repeat_probability == 1.0F &&
                bounded.sequence_drift == 0.0F && bounded.playback == 1.0F,
            "probability and playback controls should be clamped to [0, 1]");
    require(bounded.smoothing == 0.0F,
            "non-finite controls should fall back to the prior value");
    require(bounded.low_cut_hz == ShufflingLooper::kMinimumCutHz &&
                bounded.high_cut_hz == ShufflingLooper::kMaximumCutHz,
            "cut controls should honor the project frequency bounds");
    require(bounded.interval1_semitones == -12 &&
                bounded.interval2_semitones == 12,
            "pitch intervals should be bounded to project-authored octave limits");
}

void test_smoothing_filters_clear_and_reset() {
    ShufflingLooper looper(48000.0F, 1U, 3U);
    configure_ordered(looper, 2U, 2U);
    ShufflingLooperParameters parameters = looper.parameters();
    parameters.smoothing = 1.0F;
    parameters.low_cut_hz = 0.0F;
    parameters.high_cut_hz = 0.0F;
    looper.configure(parameters);
    std::array<float, 16> source{};
    source.fill(0.5F);
    record_samples(looper, source, 1U);
    std::array<float, 8> output{};
    looper.process(output, 1U);
    require(std::all_of(output.begin(), output.end(), [](const float value) {
                return std::isfinite(value);
            }), "smoothing and playback filters should keep output finite");
    require(output.front() < output[1],
            "smoothing should fade into a slice rather than start at full level");

    parameters.low_cut_hz = 100.0F;
    parameters.high_cut_hz = 8000.0F;
    looper.configure(parameters);
    require(looper.play(), "filtered playback should restart");
    std::array<float, 8> filtered_output{};
    looper.process(filtered_output, 1U);
    require(std::all_of(filtered_output.begin(), filtered_output.end(),
                        [](const float value) { return std::isfinite(value); }),
            "configured playback filters should keep output finite");

    require(looper.clear(), "clear should remove an existing loop");
    require(looper.loop_frames() == 0U &&
                looper.state() == ShufflingLooperState::Stopped,
            "clear should stop and erase the loop");
    require(!looper.clear(), "clearing an empty looper should report no change");
    const auto retained_parameters = looper.parameters();
    looper.reset();
    require(looper.loop_frames() == 0U &&
                looper.state() == ShufflingLooperState::Stopped &&
                looper.parameters().smoothing == retained_parameters.smoothing,
            "reset should clear transport while retaining user parameters");
}

void test_invalid_layout_and_samples() {
    ShufflingLooper stereo(48000.0F, 2U, 7U);
    std::array<float, 3> partial{0.1F, 0.2F, 0.3F};
    const auto original = partial;
    stereo.process(partial, 2U);
    require(partial == original, "partial stereo frame should be ignored");

    std::array<float, 4> wrong_layout{0.1F, 0.2F, 0.3F, 0.4F};
    const auto wrong_original = wrong_layout;
    stereo.process(wrong_layout, 1U);
    require(wrong_layout == wrong_original, "mismatched layout should be ignored");

    ShufflingLooper mono(48000.0F, 1U, 11U);
    configure_ordered(mono, 2U, 2U);
    std::array<float, 4> bad_samples{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -0.2F, 0.3F};
    record_samples(mono, bad_samples, 1U);
    std::array<float, 4> output{};
    mono.process(output, 1U);
    require(std::all_of(output.begin(), output.end(), [](const float value) {
                return std::isfinite(value);
            }), "non-finite inputs should be sanitized before storage/playback");
}

void test_process_does_not_allocate() {
    ShufflingLooper looper(48000.0F, 2U, 17U);
    configure_ordered(looper, 2U, 2U);
    std::array<float, 64> source{};
    source.fill(0.125F);
    record_samples(looper, source, 2U);

    std::array<float, 256> output{};
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    looper.process(output, 2U);
    count_allocations.store(false, std::memory_order_release);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "process() must not allocate after construction");
}

}  // namespace

void* operator new(const std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](const std::size_t size) {
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
        test_identity_bounds_and_constructor_validation();
        test_record_to_sequence_playback_and_stop_resume();
        test_stereo_channels_and_dry_mix();
        test_replacement_is_destructive_but_playback_is_non_destructive();
        test_random_sequence_and_drift_are_deterministic();
        test_controls_are_bounded_and_nonfinite_controls_fall_back();
        test_smoothing_filters_clear_and_reset();
        test_invalid_layout_and_samples();
        test_process_does_not_allocate();
        std::fprintf(stdout, "All TabletFX shuffling looper tests passed.\n");
        return 0;
    } catch (const std::exception& error) {
        count_allocations.store(false, std::memory_order_release);
        std::fprintf(stderr, "Shuffling looper test failure: %s\n", error.what());
        return 1;
    }
}
