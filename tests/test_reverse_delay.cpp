#include "tabletfx/effects/reverse_delay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void test_reverses_contiguous_snippets() {
    tabletfx::ReverseDelayParameters parameters;
    parameters.time_ms = 4.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.crossfade_ms = 0.0F;
    tabletfx::ReverseDelay delay(1000.0F, 1, 1000.0F, parameters);
    std::array<float, 12> audio{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    delay.process(audio, 1);
    const std::array<float, 12> expected{0, 0, 0, 0, 4, 3, 2, 1, 8, 7, 6, 5};
    require(audio == expected,
            "reverse snippets must play backward after one Time interval");
}

void test_channels_are_independent_and_crossfade_is_finite() {
    tabletfx::ReverseDelayParameters parameters;
    parameters.time_ms = 8.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.crossfade_ms = 2.0F;
    tabletfx::ReverseDelay delay(1000.0F, 2, 1000.0F, parameters);
    std::vector<float> audio(48, 0.0F);
    for (std::size_t frame = 0; frame < 24; ++frame) {
        audio[frame * 2] = static_cast<float>(frame + 1);
        audio[frame * 2 + 1] = -static_cast<float>(frame + 1);
    }
    delay.process(audio, 2);
    require(audio[0] == 0.0F && audio[1] == 0.0F,
            "reverse-delay wet output must wait for one captured snippet");
    require(audio[16] > 0.0F && audio[17] < 0.0F,
            "stereo channels must reverse independently");
    require(std::all_of(audio.begin(), audio.end(), [](float value) {
                return std::isfinite(value);
            }),
            "crossfade generated a non-finite sample");
}

void test_run_mutes_wet_and_restart_restarts_capture_window() {
    tabletfx::ReverseDelayParameters parameters;
    parameters.time_ms = 4.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.crossfade_ms = 0.0F;
    tabletfx::ReverseDelay delay(1000.0F, 1, 1000.0F, parameters);
    std::array<float, 4> first{1, 2, 3, 4};
    delay.process(first, 1);
    delay.set_running(false);
    std::array<float, 2> muted{5, 6};
    delay.process(muted, 1);
    require(muted[0] == 0.0F && muted[1] == 0.0F,
            "Run off must mute wet-only reverse playback while capture continues");
    delay.set_running(true);
    delay.trigger_restart();
    std::array<float, 4> next{7, 8, 9, 10};
    delay.process(next, 1);
    require(next[0] == 0.0F && next[1] == 0.0F &&
                next[2] == 0.0F && next[3] == 0.0F,
            "Trigger Restart must wait for a fresh full snippet");
    std::array<float, 4> following{11, 12, 13, 14};
    delay.process(following, 1);
    require(following[0] == 10.0F && following[1] == 9.0F &&
                following[2] == 8.0F && following[3] == 7.0F,
            "restarted capture must play the new snippet backward");
}

void test_processing_is_independent_of_block_boundaries() {
    tabletfx::ReverseDelayParameters parameters;
    parameters.time_ms = 8.0F;
    parameters.feedback = 0.4F;
    parameters.mix = 0.8F;
    parameters.crossfade_ms = 2.0F;
    tabletfx::ReverseDelay whole(1000.0F, 2, 1000.0F, parameters);
    tabletfx::ReverseDelay split(1000.0F, 2, 1000.0F, parameters);
    std::vector<float> uninterrupted(160);
    for (std::size_t index = 0; index < uninterrupted.size(); ++index) {
        uninterrupted[index] = static_cast<float>(static_cast<int>(index % 19U) - 9) / 9.0F;
    }
    std::vector<float> chunked = uninterrupted;
    whole.process(uninterrupted, 2);
    std::size_t frame_offset = 0;
    for (const std::size_t frames : {1U, 7U, 13U, 19U, 40U}) {
        split.process(std::span<float>(chunked.data() + frame_offset * 2U,
                                       frames * 2U), 2);
        frame_offset += frames;
    }
    require(uninterrupted == chunked,
            "Reverse Delay output must not depend on process block boundaries");
}

void test_bounds_reset_and_invalid_audio() {
    tabletfx::ReverseDelay delay;
    auto parameters = delay.parameters();
    parameters.time_ms = 9000.0F;
    parameters.feedback = 4.0F;
    parameters.mix = -2.0F;
    parameters.crossfade_ms = 900.0F;
    delay.set_parameters(parameters);
    auto sanitized = delay.parameters();
    require(sanitized.time_ms == 2000.0F && sanitized.feedback == 0.95F &&
                sanitized.mix == 0.0F && sanitized.crossfade_ms <= 50.0F,
            "reverse-delay controls must stay within project-authored bounds");
    require(delay.id() == "reverse-delay" && delay.memory_bytes() > sizeof(delay),
            "reverse-delay identity or state-memory accounting is incorrect");

    parameters.time_ms = 4.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.crossfade_ms = 0.0F;
    tabletfx::ReverseDelay recovered(1000.0F, 1, 1000.0F, parameters);
    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 3, 4, 5, 6, 7, 8};
    recovered.process(invalid, 1);
    require(std::all_of(invalid.begin(), invalid.end(), [](float value) {
                return std::isfinite(value);
            }),
            "invalid input must not contaminate the history buffer");
    recovered.reset();
    std::array<float, 4> after_reset{9, 10, 11, 12};
    recovered.process(after_reset, 1);
    require(std::all_of(after_reset.begin(), after_reset.end(), [](float value) {
                return value == 0.0F;
            }),
            "reset must clear captured snippets while preserving wet-only settings");
}

} // namespace

int main() {
    try {
        test_reverses_contiguous_snippets();
        test_channels_are_independent_and_crossfade_is_finite();
        test_run_mutes_wet_and_restart_restarts_capture_window();
        test_processing_is_independent_of_block_boundaries();
        test_bounds_reset_and_invalid_audio();
        std::cout << "Reverse Delay tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Reverse Delay test failure: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
