#include "tabletfx/effects.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

float sine_rms(float frequency_hz, bool filtered) {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 8192;
    constexpr std::size_t delay_frames = 96;
    std::vector<float> audio(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        audio[frame] = 0.5F * std::sin(
            6.2831853071795864769F * frequency_hz * static_cast<float>(frame) /
            sample_rate);
    }
    if (filtered) {
        tabletfx::FilteredDelayParameters parameters;
        parameters.time_ms = 2.0F;
        parameters.feedback = 0.0F;
        parameters.mix = 1.0F;
        parameters.low_cut_hz = frequency_hz < 1000.0F ? 1000.0F : 20.0F;
        parameters.high_cut_hz = frequency_hz > 1000.0F ? 2000.0F : 20000.0F;
        tabletfx::FilteredDelay delay(sample_rate, 1, 2000.0F, parameters);
        delay.process(audio, 1);
    } else {
        tabletfx::Delay delay(sample_rate, 1, 2000.0F, 2.0F, 0.0F, 1.0F);
        delay.process(audio, 1);
    }
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t frame = delay_frames + 256; frame < frames; ++frame) {
        const double sample = audio[frame];
        sum += sample * sample;
        ++count;
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
}

void test_filtered_delay_time_and_immediate_dry_mix() {
    tabletfx::FilteredDelayParameters parameters;
    parameters.time_ms = 2.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    tabletfx::FilteredDelay delay(48000.0F, 1, 2000.0F, parameters);
    std::vector<float> impulse(192);
    impulse[0] = 0.8F;
    delay.process(impulse, 1);
    require(impulse[0] == 0.0F,
            "100% wet Filtered Delay must not leak dry input");
    require(impulse[96] > 0.0F && impulse[96] < 0.8F,
            "Filtered Delay must return a filtered wet repeat at 2 ms");
    require(delay.id() == "filtered-delay" && delay.memory_bytes() > sizeof(delay),
            "Filtered Delay identity or memory accounting is incorrect");
}

void test_low_and_high_cuts_attenuate_the_selected_repeat_band() {
    const float low_reference = sine_rms(100.0F, false);
    const float low_filtered = sine_rms(100.0F, true);
    const float high_reference = sine_rms(10000.0F, false);
    const float high_filtered = sine_rms(10000.0F, true);
    require(low_filtered < low_reference * 0.35F,
            "Low Cut must attenuate low-frequency repeats");
    require(high_filtered < high_reference * 0.4F,
            "High Cut must attenuate high-frequency repeats");
}

void test_controls_are_bounded_and_reset_clears_state() {
    tabletfx::FilteredDelay delay;
    auto parameters = delay.parameters();
    parameters.time_ms = 9000.0F;
    parameters.feedback = 4.0F;
    parameters.mix = -1.0F;
    parameters.low_cut_hz = -10.0F;
    parameters.high_cut_hz = 90000.0F;
    delay.configure(parameters);
    const auto bounded = delay.parameters();
    require(bounded.time_ms == 2000.0F && bounded.feedback == 0.95F &&
                bounded.mix == 0.0F && bounded.low_cut_hz == 20.0F &&
                bounded.high_cut_hz <= 20000.0F,
            "Filtered Delay controls must remain within project-authored bounds");

    auto playable = bounded;
    playable.time_ms = 2.0F;
    playable.feedback = 0.0F;
    playable.mix = 1.0F;
    tabletfx::FilteredDelay recovered(48000.0F, 2, 2000.0F, playable);
    std::vector<float> stereo(256, 0.0F);
    stereo[0] = 0.4F;
    stereo[1] = -0.2F;
    recovered.process(stereo, 2);
    require(std::all_of(stereo.begin(), stereo.end(), [](float value) {
                return std::isfinite(value);
            }),
            "stereo filtered repeats must remain finite");
    recovered.reset();
    std::vector<float> silence(256, 0.0F);
    recovered.process(silence, 2);
    require(std::all_of(silence.begin(), silence.end(), [](float value) {
                return value == 0.0F;
            }),
            "reset must clear the delay ring and filter state");
}

} // namespace

int main() {
    try {
        test_filtered_delay_time_and_immediate_dry_mix();
        test_low_and_high_cuts_attenuate_the_selected_repeat_band();
        test_controls_are_bounded_and_reset_clears_state();
        std::cout << "Filtered Delay tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Filtered Delay tests failed: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
