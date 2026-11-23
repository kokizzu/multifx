#include "tabletfx/effects/looper.hpp"

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

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void record(tabletfx::Looper& looper, std::span<const float> samples,
            std::uint16_t channels) {
    require(looper.record(), "record command should start recording");
    std::array<float, 64> block{};
    std::size_t offset = 0U;
    while (offset < samples.size()) {
        const std::size_t count = std::min(block.size(), samples.size() - offset);
        std::copy_n(samples.data() + offset, count, block.data());
        looper.process(std::span<float>(block.data(), count), channels);
        offset += count;
    }
    looper.stop();
}

void test_identity_bounds_and_constructor_validation() {
    tabletfx::Looper stereo;
    require(stereo.id() == "phrase-looper", "looper ID must be stable");
    require(stereo.channels() == 2U, "default looper must be stereo");
    require(stereo.loop_frames() == 0U, "new looper must be empty");
    require(stereo.memory_bytes() <= 3'100'000U,
            "48 kHz stereo looper must remain near the 3.1 MB storage budget");
    require(tabletfx::Looper::maximum_frames() == 192'000U,
            "fixed capacity must equal four seconds at 48 kHz");

    bool rejected = false;
    try {
        tabletfx::Looper invalid(3U);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "unsupported channel count must be rejected");
}

void test_mono_stereo_record_play_and_layout_guard() {
    tabletfx::Looper mono(1U);
    constexpr std::array<float, 4> source{0.125F, -0.25F, 0.5F, -0.75F};
    record(mono, source, 1U);
    require(mono.loop_frames() == source.size(), "recorded frame count is wrong");

    std::array<float, 4> output{};
    require(mono.play(), "nonempty loop should start playback");
    mono.process(output, 1U);
    for (std::size_t i = 0U; i < output.size(); ++i) {
        require(std::abs(output[i] - source[i]) < 1.0e-7F,
                "playback must reproduce the recorded mono samples");
    }

    std::array<float, 4> wrong_layout{0.1F, 0.2F, 0.3F, 0.4F};
    const auto before = wrong_layout;
    mono.process(wrong_layout, 2U);
    require(wrong_layout == before, "mismatched layout must not modify audio");

    std::array<float, 3> partial_frame{0.1F, 0.2F, 0.3F};
    const auto partial_before = partial_frame;
    tabletfx::Looper stereo(2U);
    stereo.process(partial_frame, 2U);
    require(partial_frame == partial_before,
            "partial interleaved frame must be ignored");
}

void test_stereo_isolation_and_dry_passthrough() {
    tabletfx::Looper stereo(2U);
    constexpr std::array<float, 8> source{
        0.1F, -0.1F, 0.2F, -0.2F, 0.3F, -0.3F, 0.4F, -0.4F};
    record(stereo, source, 2U);

    std::array<float, 8> output{};
    require(stereo.play(), "stereo loop should start");
    stereo.process(output, 2U);
    for (std::size_t i = 0U; i < output.size(); ++i) {
        require(std::abs(output[i] - source[i]) < 1.0e-7F,
                "stereo playback must preserve channel samples");
    }

    stereo.stop();
    std::array<float, 4> dry{0.11F, -0.22F, 0.33F, -0.44F};
    const auto expected = dry;
    stereo.process(dry, 2U);
    require(dry == expected, "stopped looper must pass audio through unchanged");
}

void test_reverse_and_half_speed_interpolation() {
    tabletfx::Looper reverse(1U);
    constexpr std::array<float, 4> source{0.1F, 0.2F, 0.3F, 0.4F};
    record(reverse, source, 1U);
    reverse.set_reverse(true);
    require(reverse.play(), "reverse loop should start");
    std::array<float, 4> reversed{};
    reverse.process(reversed, 1U);
    constexpr std::array<float, 4> expected_reverse{0.4F, 0.3F, 0.2F, 0.1F};
    for (std::size_t i = 0U; i < reversed.size(); ++i) {
        require(std::abs(reversed[i] - expected_reverse[i]) < 1.0e-6F,
                "reverse playback must start at the end and walk backward");
    }

    tabletfx::Looper half(1U);
    record(half, source, 1U);
    half.set_half_speed(true);
    require(half.play(), "half-speed loop should start");
    std::array<float, 4> slowed{};
    half.process(slowed, 1U);
    constexpr std::array<float, 4> expected_half{0.1F, 0.15F, 0.2F, 0.25F};
    for (std::size_t i = 0U; i < slowed.size(); ++i) {
        require(std::abs(slowed[i] - expected_half[i]) < 1.0e-6F,
                "half-speed playback must interpolate between source frames");
    }
}

void test_overdub_undo_redo_record_replacement_and_clear() {
    tabletfx::Looper looper(1U);
    constexpr std::array<float, 2> original{0.1F, 0.2F};
    constexpr std::array<float, 2> layer{0.3F, 0.4F};
    record(looper, original, 1U);
    looper.set_overdub_level(0.5F);
    require(looper.overdub(), "nonempty loop should accept overdub");
    std::array<float, 2> overdub_input{layer[0], layer[1]};
    looper.process(overdub_input, 1U);
    require(std::abs(overdub_input[0] - 0.4F) < 1.0e-6F &&
                std::abs(overdub_input[1] - 0.6F) < 1.0e-6F,
            "overdub output must mix dry input with the old loop");
    looper.stop();

    require(looper.can_undo() && !looper.can_redo(),
            "completed edit should expose one undo");
    require(looper.undo(), "overdub should be undoable");
    std::array<float, 2> old_output{};
    require(looper.play(), "undo should restore a playable loop");
    looper.process(old_output, 1U);
    require(std::abs(old_output[0] - original[0]) < 1.0e-6F &&
                std::abs(old_output[1] - original[1]) < 1.0e-6F,
            "undo must restore the previous loop samples");

    require(looper.redo(), "overdub should be redoable");
    std::array<float, 2> new_output{};
    require(looper.play(), "redo should restore the edited loop");
    looper.process(new_output, 1U);
    require(std::abs(new_output[0] - 0.25F) < 1.0e-6F &&
                std::abs(new_output[1] - 0.4F) < 1.0e-6F,
            "redo must restore overdubbed samples");

    constexpr std::array<float, 3> replacement{0.55F, -0.2F, 0.7F};
    record(looper, replacement, 1U);
    require(looper.undo(), "replacement recording should preserve prior loop");
    require(looper.loop_frames() == original.size(),
            "undo replacement must restore the prior loop length");
    require(looper.clear(), "nonempty loop should clear");
    require(looper.loop_frames() == 0U, "clear must make the loop empty");
    require(looper.undo(), "clear should be undoable");
    require(looper.loop_frames() == original.size(),
            "undo clear must restore the loop length");
    require(looper.redo(), "clear should be redoable");
    require(looper.loop_frames() == 0U, "redo clear must empty the loop");
}

void test_capacity_reset_and_invalid_samples() {
    tabletfx::Looper looper(1U);
    require(looper.record(), "record should start");
    std::array<float, 128> block{};
    block.fill(std::numeric_limits<float>::quiet_NaN());
    std::size_t remaining = tabletfx::Looper::maximum_frames();
    while (remaining > 0U) {
        const std::size_t count = std::min(remaining, block.size());
        looper.process(std::span<float>(block.data(), count), 1U);
        remaining -= count;
    }
    require(looper.loop_frames() == tabletfx::Looper::maximum_frames(),
            "recording must stop at the fixed capacity");
    require(looper.state() == tabletfx::LooperState::Playing,
            "a full recording should transition to playback");

    looper.reset();
    require(looper.state() == tabletfx::LooperState::Stopped &&
                looper.loop_frames() == 0U && !looper.can_undo() &&
                !looper.can_redo(),
            "reset must erase loop, history, and transport state");

    std::array<float, 4> dry{0.1F, 0.2F, 0.3F, 0.4F};
    const auto expected = dry;
    looper.process(dry, 1U);
    require(dry == expected, "reset looper must be dry pass-through");
}

void test_non_finite_samples_are_sanitized() {
    tabletfx::Looper looper(1U);
    std::array<float, 4> source{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
    };
    record(looper, source, 1U);
    std::array<float, 4> output{};
    require(looper.play(), "sanitized loop should remain playable");
    looper.process(output, 1U);
    require(std::all_of(output.begin(), output.end(), [](float sample) {
                return std::isfinite(sample) &&
                       std::abs(sample) <= tabletfx::Looper::kSafetyCeiling;
            }),
            "stored invalid or excessive samples must remain finite and bounded");
}

void test_auto_record_triggers_at_threshold_and_captures_crossing_frame() {
    tabletfx::Looper looper(1U);
    looper.set_auto_record_threshold_db(-20.0F);
    looper.set_auto_record_enabled(true);

    std::array<float, 3> below{0.05F, -0.08F, 0.09F};
    looper.process(below, 1U);
    require(looper.state() == tabletfx::LooperState::Stopped &&
                looper.loop_frames() == 0U,
            "auto-record must wait while input remains below threshold");

    std::array<float, 3> crossing{0.1F, -0.2F, 0.25F};
    looper.process(crossing, 1U);
    require(looper.state() == tabletfx::LooperState::Recording &&
                looper.loop_frames() == crossing.size(),
            "threshold crossing must start recording and capture that frame");
    looper.stop();

    std::array<float, 3> playback{};
    require(looper.play(), "auto-recorded loop should be playable");
    looper.process(playback, 1U);
    require(playback == crossing,
            "auto-record must capture the crossing frame without dropping samples");
}

void test_auto_record_starts_replacement_with_undoable_previous_loop() {
    tabletfx::Looper looper(1U);
    constexpr std::array<float, 2> original{0.2F, 0.3F};
    record(looper, original, 1U);
    looper.set_auto_record_threshold_db(-30.0F);
    looper.set_auto_record_enabled(true);

    std::array<float, 2> quiet{0.01F, 0.02F};
    looper.process(quiet, 1U);
    std::array<float, 2> new_loop{0.5F, 0.6F};
    looper.process(new_loop, 1U);
    require(looper.state() == tabletfx::LooperState::Recording &&
                looper.loop_frames() == new_loop.size(),
            "auto-record should begin a replacement after the threshold crossing");
    looper.stop();
    require(looper.undo() && looper.loop_frames() == original.size(),
            "auto-record replacement should retain one undo level");
    std::array<float, 2> restored{};
    require(looper.play(), "undo should restore the original loop for playback");
    looper.process(restored, 1U);
    require(restored == original,
            "undo after auto-record must restore the previous loop samples");
}

void test_process_does_not_allocate() {
    tabletfx::Looper looper(2U);
    constexpr std::array<float, 8> source{
        0.1F, -0.1F, 0.2F, -0.2F, 0.3F, -0.3F, 0.4F, -0.4F};
    record(looper, source, 2U);
    require(looper.overdub(), "loop should start overdub for allocation test");
    std::array<float, 256> block{};
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    for (int iteration = 0; iteration < 40; ++iteration) {
        looper.process(block, 2U);
    }
    count_allocations.store(false, std::memory_order_release);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "process() must not allocate after construction");
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
        test_identity_bounds_and_constructor_validation();
        test_mono_stereo_record_play_and_layout_guard();
        test_stereo_isolation_and_dry_passthrough();
        test_reverse_and_half_speed_interpolation();
        test_overdub_undo_redo_record_replacement_and_clear();
        test_capacity_reset_and_invalid_samples();
        test_non_finite_samples_are_sanitized();
        test_auto_record_triggers_at_threshold_and_captures_crossing_frame();
        test_auto_record_starts_replacement_with_undoable_previous_loop();
        test_process_does_not_allocate();
        std::cout << "All looper tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        count_allocations.store(false, std::memory_order_release);
        std::cerr << "Looper test failure: " << error.what() << '\n';
        return 1;
    }
}
