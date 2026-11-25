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
using tabletfx::effects::ShufflingLooperCapacityStatus;
using tabletfx::effects::ShufflingLooperParameters;
using tabletfx::effects::ShufflingLooperState;

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};
std::atomic<bool> fail_next_array_allocation{false};

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

void configure_quantized(ShufflingLooper& looper, const float tempo_bpm = 120.0F) {
    ShufflingLooperParameters parameters = looper.parameters();
    parameters.quantize_recording = true;
    parameters.tempo_bpm = tempo_bpm;
    parameters.playback = 0.0F;
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
    require(stereo.capacity_frames() ==
                ShufflingLooper::kDefaultCapacityFramesAt48k &&
                stereo.capacity_seconds() == 4.0F &&
                stereo.capacity_storage_bytes() == 1'536'000U,
            "default capacity should remain four seconds at 48 kHz");

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

void test_capacity_reaches_model_limits_and_respects_memory_budget() {
    {
        ShufflingLooper mono(48000.0F, 1U);
        const std::size_t previous_storage_bytes = mono.capacity_storage_bytes();
        const auto status = mono.configure_capacity_seconds(60.0F);
        require(status == ShufflingLooperCapacityStatus::Applied,
                "60-second mono request should fit the default memory budget");
        require(mono.capacity_seconds() == 60.0F &&
                    mono.capacity_frames() == 2'880'000U &&
                    mono.capacity_storage_bytes() == 11'520'000U,
                "mono maximum should provide 60 seconds at 48 kHz");
        require(previous_storage_bytes + mono.capacity_storage_bytes() <=
                    ShufflingLooper::kMaximumTransitionSampleStorageBytes,
                "mono growth overlap must stay within the 16 MiB sample-storage cap");
        require(mono.memory_bytes() <= mono.instance_memory_limit_bytes(),
                "mono maximum must stay within its per-instance memory limit");
    }

    {
        ShufflingLooper stereo(48000.0F, 2U);
        const std::size_t previous_storage_bytes = stereo.capacity_storage_bytes();
        const auto status = stereo.configure_capacity_seconds(60.0F);
        require(status == ShufflingLooperCapacityStatus::ClampedByModelLimit,
                "60-second stereo request should report the channel-specific clamp");
        require(stereo.capacity_seconds() == 30.0F &&
                    stereo.capacity_frames() == 1'440'000U &&
                    stereo.capacity_storage_bytes() == 11'520'000U,
                "stereo maximum should provide 30 seconds at 48 kHz");
        require(previous_storage_bytes + stereo.capacity_storage_bytes() <=
                    ShufflingLooper::kMaximumTransitionSampleStorageBytes,
                "stereo growth overlap must stay within the 16 MiB sample-storage cap");
        require(stereo.memory_bytes() <= stereo.instance_memory_limit_bytes(),
                "stereo maximum must stay within its per-instance memory limit");
    }

    {
        ShufflingLooper mono(96000.0F, 1U);
        const std::size_t previous_storage_bytes = mono.capacity_storage_bytes();
        require(mono.configure_capacity_seconds(60.0F) ==
                    ShufflingLooperCapacityStatus::ClampedByMemoryLimit,
                "96 kHz mono request should report the transient memory clamp");
        require(mono.capacity_frames() ==
                    (ShufflingLooper::kMaximumTransitionSampleStorageBytes -
                     previous_storage_bytes) / sizeof(float) &&
                    mono.capacity_storage_bytes() + previous_storage_bytes <=
                        ShufflingLooper::kMaximumTransitionSampleStorageBytes &&
                    mono.capacity_seconds() < 60.0F,
                "96 kHz mono growth must clamp to the remaining transition budget");
        require(mono.memory_bytes() <= mono.instance_memory_limit_bytes(),
                "96 kHz mono storage must honor the instance limit");
    }

    {
        ShufflingLooper stereo(96000.0F, 2U);
        const std::size_t previous_storage_bytes = stereo.capacity_storage_bytes();
        require(stereo.configure_capacity_seconds(60.0F) ==
                    ShufflingLooperCapacityStatus::ClampedByModelAndMemoryLimit,
                "96 kHz stereo request should report model and transient clamps");
        require(stereo.capacity_frames() ==
                    (ShufflingLooper::kMaximumTransitionSampleStorageBytes -
                     previous_storage_bytes) / (2U * sizeof(float)) &&
                    stereo.capacity_storage_bytes() + previous_storage_bytes <=
                        ShufflingLooper::kMaximumTransitionSampleStorageBytes &&
                    stereo.capacity_seconds() < 30.0F,
                "96 kHz stereo growth must clamp to the remaining transition budget");
        require(stereo.memory_bytes() <= stereo.instance_memory_limit_bytes(),
                "96 kHz stereo storage must honor the instance limit");
    }

    {
        ShufflingLooper constrained(48000.0F, 1U, 1U, 1'000'000U);
        const auto status = constrained.configure_capacity_seconds(60.0F);
        require(status == ShufflingLooperCapacityStatus::ClampedByMemoryLimit,
                "a smaller instance budget should report the memory clamp");
        require(constrained.capacity_storage_bytes() <= 1'000'000U - sizeof(constrained) &&
                    constrained.capacity_seconds() < 60.0F,
                "memory-limited capacity must stay within its byte budget");
        require(constrained.memory_bytes() <= 1'000'000U,
                "reported instance memory must stay within the supplied budget");
    }
}

void test_capacity_transition_bound_rejects_oversized_shrinks() {
    ShufflingLooper looper(96000.0F, 1U);
    const std::size_t default_storage_bytes = looper.capacity_storage_bytes();
    require(looper.configure_capacity_seconds(60.0F) ==
                ShufflingLooperCapacityStatus::ClampedByMemoryLimit,
            "96 kHz mono capacity should clamp to the transition budget");
    const std::size_t clamped_storage_bytes = looper.capacity_storage_bytes();
    require(default_storage_bytes + clamped_storage_bytes <=
                ShufflingLooper::kMaximumTransitionSampleStorageBytes,
            "accepted capacity change must keep old and replacement storage bounded");

    const std::size_t clamped_frames = looper.capacity_frames();
    require(looper.configure_capacity_seconds(20.0F) ==
                ShufflingLooperCapacityStatus::RejectedTransitionMemoryLimit,
            "a shrink whose old and replacement buffers exceed 16 MiB must be rejected");
    require(looper.capacity_frames() == clamped_frames &&
                looper.capacity_storage_bytes() == clamped_storage_bytes,
            "rejected transition must leave the existing capacity intact");
}

void test_capacity_reconfiguration_preserves_loop_or_rejects_truncation() {
    ShufflingLooper looper(48000.0F, 1U, 21U);
    configure_ordered(looper, 2U, 2U);
    require(looper.configure_capacity_seconds(8.0F) ==
                ShufflingLooperCapacityStatus::Applied,
            "capacity should grow while the looper is empty");
    std::array<float, 8> source{0.1F, 0.2F, 0.3F, 0.4F,
                                0.5F, 0.6F, 0.7F, 0.8F};
    record_samples(looper, source, 1U);
    const auto recorded_frames = looper.loop_frames();
    const auto recorded_state = looper.state();

    require(looper.configure_capacity_seconds(16.0F) ==
                ShufflingLooperCapacityStatus::Applied,
            "capacity growth should preserve a recorded loop");
    require(looper.loop_frames() == recorded_frames &&
                looper.state() == recorded_state,
            "capacity growth must preserve loop and transport state");
    require(looper.play(), "preserved loop should remain playable");
    std::array<float, 8> output{};
    looper.process(output, 1U);
    require(output == source, "capacity growth must preserve recorded samples");

    const auto state_before_reject = looper.state();
    const auto frames_before_reject = looper.loop_frames();
    const auto status = looper.configure_capacity_seconds(0.0001F);
    require(status ==
                ShufflingLooperCapacityStatus::RejectedLoopWouldBeTruncated,
            "a shrink that would discard recorded samples must be rejected");
    require(looper.state() == state_before_reject &&
                looper.loop_frames() == frames_before_reject,
            "rejected shrink must preserve loop and transport state");

    require(looper.configure_capacity_seconds(
                std::numeric_limits<float>::quiet_NaN()) ==
                ShufflingLooperCapacityStatus::RejectedInvalidRequest,
            "non-finite capacity requests must be rejected explicitly");
}

void test_capacity_allocation_failure_preserves_recorded_loop() {
    ShufflingLooper looper(48000.0F, 1U, 31U);
    configure_ordered(looper, 2U, 2U);
    std::array<float, 4> source{0.1F, 0.2F, 0.3F, 0.4F};
    record_samples(looper, source, 1U);
    const std::size_t previous_capacity_frames = looper.capacity_frames();
    const std::size_t previous_loop_frames = looper.loop_frames();

    fail_next_array_allocation.store(true, std::memory_order_release);
    const auto status = looper.configure_capacity_seconds(8.0F);
    require(!fail_next_array_allocation.load(std::memory_order_acquire),
            "capacity replacement should reach the injected allocation failure");
    require(status == ShufflingLooperCapacityStatus::AllocationFailed,
            "allocation failure should be reported without throwing");
    require(looper.capacity_frames() == previous_capacity_frames &&
                looper.loop_frames() == previous_loop_frames &&
                looper.state() == ShufflingLooperState::Playing,
            "allocation failure must preserve capacity, loop, and transport");

    std::array<float, 4> output{};
    looper.process(output, 1U);
    require(output == source,
            "recorded samples must remain playable after allocation failure");
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

void test_quantized_recording_waits_for_selected_beats_without_audio_latency() {
    ShufflingLooper looper(48000.0F, 1U, 31U);
    configure_ordered(looper, 4U, 4U);
    configure_quantized(looper, 120.0F);

    std::array<float, 1> phase_advance{0.125F};
    looper.process(phase_advance, 1U);
    require(phase_advance[0] == 0.125F,
            "stopped audio should pass through while the beat clock advances");
    require(looper.record(), "quantized record should arm");
    require(looper.state() == ShufflingLooperState::WaitingToRecord &&
                looper.loop_frames() == 0U,
            "quantized record should wait for the next selected beat");

    std::array<float, 23999> waiting{};
    waiting.fill(0.125F);
    looper.process(waiting, 1U);
    require(looper.state() == ShufflingLooperState::WaitingToRecord &&
                looper.loop_frames() == 0U,
            "recording must remain armed until the exact next beat frame");
    require(std::all_of(waiting.begin(), waiting.end(), [](const float sample) {
                return sample == 0.125F;
            }),
            "waiting to record must not delay or alter the live audio path");

    std::array<float, 1> first_capture_frame{0.25F};
    looper.process(first_capture_frame, 1U);
    require(looper.state() == ShufflingLooperState::Recording &&
                looper.loop_frames() == 1U,
            "recording should begin on the next beat frame");
    require(first_capture_frame[0] == 0.25F,
            "the first captured frame should remain audible immediately");

    ShufflingLooperParameters changed_tempo = looper.parameters();
    changed_tempo.tempo_bpm = 60.0F;
    looper.configure(changed_tempo);
    require(looper.stop_recording(), "quantized stop should arm at the next beat");
    require(looper.state() == ShufflingLooperState::WaitingToStop,
            "quantized stop should expose its pending state");

    std::array<float, 23999> recording{};
    recording.fill(0.25F);
    looper.process(recording, 1U);
    require(looper.state() == ShufflingLooperState::WaitingToStop &&
                looper.loop_frames() == 24000U,
            "capture should retain the tempo latched at record start");
    require(std::all_of(recording.begin(), recording.end(), [](const float sample) {
                return sample == 0.25F;
            }),
            "recording while stop is armed must remain zero-latency pass-through");

    std::array<float, 1> stop_boundary{0.375F};
    looper.process(stop_boundary, 1U);
    require(looper.state() == ShufflingLooperState::Playing &&
                looper.loop_frames() == 24000U,
            "stop should finalize exactly at the next beat boundary");
    require(stop_boundary[0] == 0.375F,
            "the stop boundary must not add fixed audio-path latency");
}

void test_quantized_capture_uses_shared_stereo_beat_boundary() {
    ShufflingLooper looper(48000.0F, 2U, 37U);
    configure_ordered(looper, 4U, 4U);
    configure_quantized(looper, 120.0F);
    require(looper.record(), "stereo quantized record should arm");

    std::array<float, 48000> one_beat{};
    for (std::size_t frame = 0U; frame < one_beat.size() / 2U; ++frame) {
        one_beat[frame * 2U] = 0.125F;
        one_beat[frame * 2U + 1U] = -0.25F;
    }
    looper.process(one_beat, 2U);
    require(looper.state() == ShufflingLooperState::Recording &&
                looper.loop_frames() == 24000U,
            "stereo capture should count complete interleaved frames");
    require(looper.stop_recording(), "stereo stop should arm at the beat edge");
    std::array<float, 2> boundary{0.0F, 0.0F};
    looper.process(boundary, 2U);
    require(looper.state() == ShufflingLooperState::Playing &&
                looper.loop_frames() == 24000U,
            "both stereo channels should finalize on the same beat boundary");
    require(boundary[0] == 0.0F && boundary[1] == 0.0F,
            "zero playback mix should leave both live stereo channels untouched");
}

void test_quantized_capture_rejects_subbeat_capacity_and_fits_buffer() {
    ShufflingLooper too_short(48000.0F, 1U, 41U);
    require(too_short.configure_capacity_seconds(0.49F) ==
                ShufflingLooperCapacityStatus::Applied,
            "short capacity fixture should configure");
    configure_quantized(too_short, 120.0F);
    require(!too_short.record() && too_short.loop_frames() == 0U &&
                too_short.state() == ShufflingLooperState::Stopped,
            "record must reject capacity that cannot hold one full beat");

    ShufflingLooper one_beat(48000.0F, 1U, 43U);
    require(one_beat.configure_capacity_seconds(0.51F) ==
                ShufflingLooperCapacityStatus::Applied,
            "one-beat capacity fixture should configure");
    configure_quantized(one_beat, 120.0F);
    require(one_beat.record(), "capacity holding one beat should arm");
    std::array<float, 24000> beat{};
    beat.fill(0.125F);
    one_beat.process(beat, 1U);
    require(one_beat.state() == ShufflingLooperState::Playing &&
                one_beat.loop_frames() == beat.size() &&
                one_beat.loop_frames() <= one_beat.capacity_frames(),
            "quantized automatic completion must fit the existing buffer bound");
}

void test_process_does_not_allocate() {
    ShufflingLooper looper(48000.0F, 2U, 17U);
    configure_ordered(looper, 2U, 2U);
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    const auto capacity_status = looper.configure_capacity_seconds(8.0F);
    count_allocations.store(false, std::memory_order_release);
    require(capacity_status == ShufflingLooperCapacityStatus::Applied &&
                allocation_count.load(std::memory_order_relaxed) > 0U,
            "capacity storage should allocate during explicit configuration");

    std::array<float, 64> source{};
    source.fill(0.125F);
    record_samples(looper, source, 2U);

    std::array<float, 256> output{};
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    looper.process(output, 2U);
    count_allocations.store(false, std::memory_order_release);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "process() must not allocate after capacity configuration");
}

void test_quantized_process_does_not_allocate() {
    ShufflingLooper looper(48000.0F, 1U, 47U);
    configure_quantized(looper, 120.0F);
    require(looper.record(), "quantized no-allocation test should arm");
    std::array<float, 256> input{};
    input.fill(0.125F);
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    looper.process(input, 1U);
    count_allocations.store(false, std::memory_order_release);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "quantized process() must not allocate while waiting for a beat");
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
    if (fail_next_array_allocation.exchange(false, std::memory_order_acq_rel)) {
        throw std::bad_alloc();
    }
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
        test_capacity_reaches_model_limits_and_respects_memory_budget();
        test_capacity_transition_bound_rejects_oversized_shrinks();
        test_capacity_reconfiguration_preserves_loop_or_rejects_truncation();
        test_capacity_allocation_failure_preserves_recorded_loop();
        test_record_to_sequence_playback_and_stop_resume();
        test_stereo_channels_and_dry_mix();
        test_replacement_is_destructive_but_playback_is_non_destructive();
        test_random_sequence_and_drift_are_deterministic();
        test_controls_are_bounded_and_nonfinite_controls_fall_back();
        test_smoothing_filters_clear_and_reset();
        test_invalid_layout_and_samples();
        test_quantized_recording_waits_for_selected_beats_without_audio_latency();
        test_quantized_capture_uses_shared_stereo_beat_boundary();
        test_quantized_capture_rejects_subbeat_capacity_and_fits_buffer();
        test_process_does_not_allocate();
        test_quantized_process_does_not_allocate();
        std::fprintf(stdout, "All TabletFX shuffling looper tests passed.\n");
        return 0;
    } catch (const std::exception& error) {
        count_allocations.store(false, std::memory_order_release);
        std::fprintf(stderr, "Shuffling looper test failure: %s\n", error.what());
        return 1;
    }
}
