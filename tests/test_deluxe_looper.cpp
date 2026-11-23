#include "tabletfx/effects/deluxe_looper.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool near(float actual, float expected, float tolerance = 0.0001F) {
    return std::fabs(actual - expected) <= tolerance;
}

void test_capacity_and_controls() {
    using tabletfx::DeluxeLooper;

    {
        DeluxeLooper mono(1U);
        check(mono.channels() == 1U, "mono channel count");
        check(mono.capacity_frames() == 5'760'000U,
              "mono 120-second capacity at 48 kHz");
        check(mono.memory_bytes() == sizeof(mono) + 46'080'000U,
              "mono reports exact loop and undo storage bytes");
    }

    {
        DeluxeLooper stereo(2U);
        check(stereo.channels() == 2U, "stereo channel count");
        check(stereo.capacity_frames() == 2'880'000U,
              "stereo 60-second capacity at 48 kHz");
        check(stereo.memory_bytes() == sizeof(stereo) + 46'080'000U,
              "stereo reports exact loop and undo storage bytes");
        check(stereo.id() == "deluxe-looper", "stable effect id");
        check(stereo.maximum_cutoff_hz() == 20'000.0F,
              "48 kHz cutoff maximum is 20 kHz");

        stereo.set_playback_level(-1.0F);
        check(stereo.playback_level() == 0.0F,
              "playback level clamps at zero");
        stereo.set_playback_level(2.0F);
        check(stereo.playback_level() == 1.0F,
              "playback level clamps at one");
        stereo.set_overdub_decay_db(-40.0F);
        check(stereo.overdub_decay_db() == -24.0F,
              "overdub decay clamps at project minimum");
        stereo.set_overdub_decay_db(4.0F);
        check(stereo.overdub_decay_db() == 0.0F,
              "overdub decay clamps at zero dB");
        stereo.set_low_cut_hz(21'000.0F);
        check(stereo.low_cut_hz() == 20'000.0F,
              "low cut clamps to the safe project maximum");
        stereo.set_high_cut_hz(-10.0F);
        check(stereo.high_cut_hz() == 0.0F,
              "zero disables high cut");
    }

    {
        DeluxeLooper low_rate(2U, 4'000.0F);
        check(low_rate.maximum_cutoff_hz() == 1'800.0F,
              "cutoff maximum follows 0.45 times sample rate");
    }
}

void test_record_playback_speed_and_reverse() {
    using tabletfx::DeluxeLooper;
    using State = DeluxeLooper::State;

    DeluxeLooper looper(1U);
    std::array<float, 3> recorded{1.0F, 2.0F, 3.0F};
    check(!looper.play(), "empty looper does not play");
    check(looper.record(), "record starts from empty state");
    looper.process(recorded, 1U);
    check(recorded[0] == 1.0F && recorded[1] == 2.0F && recorded[2] == 3.0F,
          "recording passes the input through unchanged");
    check(looper.state() == State::Recording && looper.loop_frames() == 3U,
          "recording captures three frames");
    looper.stop();
    check(looper.state() == State::Playing,
          "finishing a recording starts playback");

    std::array<float, 3> one_shot{};
    check(looper.play_once(), "Play x1 starts with captured loop");
    looper.process(one_shot, 1U);
    check(one_shot[0] == 1.0F && one_shot[1] == 2.0F && one_shot[2] == 3.0F,
          "Play x1 emits the captured frames in order");
    check(looper.state() == State::Stopped && !looper.one_shot(),
          "Play x1 stops at the captured loop end");

    looper.set_reverse(true);
    std::array<float, 3> reverse{};
    check(looper.play_once(), "reverse Play x1 starts");
    looper.process(reverse, 1U);
    check(reverse[0] == 3.0F && reverse[1] == 2.0F && reverse[2] == 1.0F,
          "reverse playback starts at the loop end");
    check(looper.state() == State::Stopped,
          "reverse Play x1 stops after one loop");

    looper.set_reverse(false);
    looper.set_half_speed(true);
    std::array<float, 3> half_speed{};
    check(looper.play(), "cyclic playback starts");
    looper.process(half_speed, 1U);
    check(near(half_speed[0], 1.0F) && near(half_speed[1], 1.5F) &&
              near(half_speed[2], 2.0F),
          "half speed interpolates at half the playback rate");
    check(looper.state() == State::Playing,
          "ordinary playback loops cyclically");
}

void test_overdub_undo_redo_and_decay() {
    using tabletfx::DeluxeLooper;

    DeluxeLooper looper(1U);
    std::array<float, 3> initial{1.0F, 2.0F, 3.0F};
    check(looper.record(), "initial recording starts");
    looper.process(initial, 1U);
    looper.stop();

    check(looper.overdub(), "overdub starts on existing loop");
    std::array<float, 3> overdub_input{10.0F, 20.0F, 30.0F};
    looper.process(overdub_input, 1U);
    check(near(overdub_input[0], 11.0F) && near(overdub_input[1], 22.0F) &&
              near(overdub_input[2], 33.0F),
          "overdub monitor adds loop playback to live input");
    looper.stop();

    check(looper.undo(), "undo restores the pre-overdub loop");
    check(!looper.undo(), "a single snapshot cannot be undone twice");
    std::array<float, 3> undone{};
    check(looper.play_once(), "undone loop plays");
    looper.process(undone, 1U);
    check(undone[0] == 1.0F && undone[1] == 2.0F && undone[2] == 3.0F,
          "undo removes the most recent overdub");

    check(looper.redo(), "redo restores the overdubbed loop");
    std::array<float, 3> redone{};
    check(looper.play_once(), "redone loop plays");
    looper.process(redone, 1U);
    check(redone[0] == 11.0F && redone[1] == 22.0F && redone[2] == 33.0F,
          "redo restores the latest overdub");

    looper.set_overdub_decay_db(-6.0F);
    check(looper.overdub(), "second overdub starts");
    std::array<float, 3> silent_input{};
    looper.process(silent_input, 1U);
    looper.stop();
    std::array<float, 1> one_frame{};
    looper.process(one_frame, 1U);
    check(near(one_frame[0], 11.0F * std::pow(10.0F, -6.0F / 20.0F),
               0.001F),
          "overdub decay attenuates loop playback after a complete cycle");

    looper.clear();
    check(looper.state() == DeluxeLooper::State::Stopped &&
              looper.loop_frames() == 0U,
          "clear removes the captured loop");
    check(!looper.undo() && !looper.redo(),
          "clear removes undo and redo history");
}

void test_mismatched_channel_count_is_unchanged() {
    tabletfx::DeluxeLooper looper(2U);
    std::array<float, 2> stereo{0.25F, -0.25F};
    const auto original = stereo;
    (void)looper.record();
    looper.process(stereo, 1U);
    check(stereo == original,
          "mismatched process channel count leaves audio unchanged");
}

} // namespace

int main() {
    test_capacity_and_controls();
    test_record_playback_speed_and_reverse();
    test_overdub_undo_redo_and_decay();
    test_mismatched_channel_count_is_unchanged();
    if (failures != 0) {
        std::cerr << failures << " Deluxe Looper test(s) failed\n";
        return 1;
    }
    std::cout << "Deluxe Looper tests passed\n";
    return 0;
}
