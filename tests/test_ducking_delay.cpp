#include "tabletfx/effects/ducking_delay.hpp"

#include <algorithm>
#include <array>
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

void test_delay_time_and_mix() {
    tabletfx::DuckingDelayParameters parameters;
    parameters.time_ms = 4.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.threshold_db = 0.0F;
    parameters.attenuation_db = 0.0F;
    tabletfx::DuckingDelay delay(1000.0F, 1, 100.0F, parameters);
    std::array<float, 8> audio{0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F};
    delay.process(audio, 1);
    const std::array<float, 8> expected{0, 0, 0, 0, 0.1F, 0.2F, 0.3F, 0.4F};
    for (std::size_t i = 0; i < audio.size(); ++i) {
        require(std::abs(audio[i] - expected[i]) < 1.0e-6F,
                "Ducking Delay must preserve the configured wet delay time and mix");
    }
}

void test_input_ducks_repeats_then_release_restores_them() {
    tabletfx::DuckingDelayParameters parameters;
    parameters.time_ms = 2.0F;
    parameters.feedback = 0.95F;
    parameters.mix = 1.0F;
    parameters.threshold_db = -6.0F;
    parameters.attenuation_db = 24.0F;
    parameters.release_ms = 2.0F;
    tabletfx::DuckingDelay delay(1000.0F, 1, 100.0F, parameters);
    std::array<float, 20> audio{};
    audio[0] = 0.8F;
    audio[2] = 0.8F;
    delay.process(audio, 1);
    require(audio[2] < 0.2F,
            "a loud input above threshold must duck the simultaneous wet repeat; got " +
                std::to_string(audio[2]));
    require(audio[16] > audio[2] * 2.0F,
            "the delayed signal must recover after the input falls below threshold");
}

void test_ducker_is_stereo_linked_and_reset_clears_state() {
    tabletfx::DuckingDelayParameters parameters;
    parameters.time_ms = 1.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.threshold_db = -6.0F;
    parameters.attenuation_db = 24.0F;
    parameters.release_ms = 1.0F;
    tabletfx::DuckingDelay delay(1000.0F, 2, 100.0F, parameters);
    std::array<float, 4> stereo{0.1F, 0.2F, 0.8F, 0.0F};
    delay.process(stereo, 2);
    require(stereo[3] < 0.05F,
            "a loud left input must duck the right-channel delayed signal");

    delay.reset();
    std::array<float, 2> after_reset{0.1F, 0.2F};
    delay.process(after_reset, 2);
    require(std::all_of(after_reset.begin(), after_reset.end(), [](float sample) {
                return sample == 0.0F;
            }),
            "reset must clear delay memory and restore the ducker envelope");
}

void test_controls_are_bounded_and_invalid_samples_recover() {
    tabletfx::DuckingDelay delay;
    auto parameters = delay.parameters();
    parameters.time_ms = 9000.0F;
    parameters.feedback = 4.0F;
    parameters.mix = -1.0F;
    parameters.threshold_db = 20.0F;
    parameters.attenuation_db = 100.0F;
    parameters.release_ms = 9000.0F;
    delay.configure(parameters);
    const auto safe = delay.parameters();
    require(safe.time_ms == 2000.0F && safe.feedback == 0.95F && safe.mix == 0.0F &&
                safe.threshold_db <= 0.0F && safe.attenuation_db <= 60.0F &&
                safe.release_ms <= 2000.0F,
            "Ducking Delay controls must stay within the project-authored bounds");
    require(delay.id() == "ducking-delay" && delay.memory_bytes() > sizeof(delay),
            "Ducking Delay identity or memory accounting is incorrect");

    auto valid = safe;
    valid.time_ms = 2.0F;
    valid.feedback = 0.0F;
    valid.mix = 1.0F;
    valid.threshold_db = 0.0F;
    valid.attenuation_db = 0.0F;
    tabletfx::DuckingDelay recovered(1000.0F, 1, 100.0F, valid);
    std::array<float, 6> invalid{NAN, INFINITY, 0.3F, 0.4F, 0.5F, 0.6F};
    recovered.process(invalid, 1);
    require(std::all_of(invalid.begin(), invalid.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "invalid input must not contaminate delay history or output");
}

} // namespace

int main() {
    try {
        test_delay_time_and_mix();
        test_input_ducks_repeats_then_release_restores_them();
        test_ducker_is_stereo_linked_and_reset_clears_state();
        test_controls_are_bounded_and_invalid_samples_recover();
        std::cout << "Ducking Delay tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Ducking Delay test failure: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
