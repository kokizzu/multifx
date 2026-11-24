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

void test_spillover_bypass_preserves_dry_and_emits_only_stored_tail() {
    tabletfx::DuckingDelayParameters parameters;
    parameters.time_ms = 2.0F;
    parameters.feedback = 0.5F;
    parameters.mix = 0.5F;
    parameters.threshold_db = 0.0F;
    parameters.attenuation_db = 0.0F;
    parameters.spillover = true;
    tabletfx::DuckingDelay delay(1000.0F, 1, 100.0F, parameters);

    std::array<float, 2> source{0.8F, 0.0F};
    delay.process(source, 1);
    std::array<float, 2> bypassed{0.25F, 0.25F};
    delay.process_bypassed(bypassed, 1);
    require(std::abs(bypassed[0] - 0.65F) < 1.0e-6F &&
                std::abs(bypassed[1] - 0.25F) < 1.0e-6F,
            "spillover bypass must preserve immediate dry audio and add the stored wet tail");

    std::array<float, 2> following{0.25F, 0.25F};
    delay.process_bypassed(following, 1);
    require(std::abs(following[0] - 0.45F) < 1.0e-6F &&
                std::abs(following[1] - 0.25F) < 1.0e-6F,
            "spillover must decay through feedback without recording new bypassed input");
}

void test_spillover_off_bypass_is_transparent_and_clears_the_tail() {
    tabletfx::DuckingDelayParameters parameters;
    parameters.time_ms = 2.0F;
    parameters.feedback = 0.5F;
    parameters.mix = 1.0F;
    parameters.threshold_db = 0.0F;
    parameters.attenuation_db = 0.0F;
    tabletfx::DuckingDelay delay(1000.0F, 1, 100.0F, parameters);

    std::array<float, 2> source{0.8F, 0.0F};
    delay.process(source, 1);
    std::array<float, 2> bypassed{0.25F, 0.5F};
    delay.process_bypassed(bypassed, 1);
    require(bypassed[0] == 0.25F && bypassed[1] == 0.5F,
            "bypass without spillover must pass the dry signal unchanged");

    std::array<float, 4> after_reenable{};
    delay.process(after_reenable, 1);
    require(std::all_of(after_reenable.begin(), after_reenable.end(), [](float sample) {
                return sample == 0.0F;
            }),
            "bypass without spillover must clear pending delay state");
}

void test_tempo_sync_uses_tap_division_and_keeps_free_time() {
    tabletfx::DuckingDelayParameters parameters;
    parameters.time_ms = 7.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.threshold_db = 0.0F;
    parameters.attenuation_db = 0.0F;
    parameters.time_mode = tabletfx::DuckingDelayTimeMode::tempo_sync;
    parameters.tempo_bpm = 120.0F;
    parameters.tap_division = tabletfx::DuckingDelayTapDivision::eighth;
    tabletfx::DuckingDelay delay(1000.0F, 1, 1000.0F, parameters);

    std::array<float, 260> audio{};
    audio[0] = 0.8F;
    delay.process(audio, 1);
    require(std::all_of(audio.begin(), audio.begin() + 250,
                        [](float sample) { return sample == 0.0F; }) &&
                audio[250] > 0.0F,
            "120 BPM eighth-note sync must produce its repeat at 250 ms");
    const auto configured = delay.parameters();
    require(configured.time_ms == 7.0F &&
                configured.time_mode == tabletfx::DuckingDelayTimeMode::tempo_sync &&
                configured.tempo_bpm == 120.0F &&
                configured.tap_division == tabletfx::DuckingDelayTapDivision::eighth,
            "tempo sync must retain the separate free-time value and selected division");
}

void test_repeat_cuts_leave_first_echo_and_filter_feedback_repeats() {
    auto make_impulse = [](float low_cut_hz, float high_cut_hz) {
        tabletfx::DuckingDelayParameters parameters;
        parameters.time_ms = 2.0F;
        parameters.feedback = 0.8F;
        parameters.mix = 1.0F;
        parameters.threshold_db = 0.0F;
        parameters.attenuation_db = 0.0F;
        parameters.low_cut_hz = low_cut_hz;
        parameters.high_cut_hz = high_cut_hz;
        tabletfx::DuckingDelay delay(48000.0F, 1, 100.0F, parameters);
        std::array<float, 320> audio{};
        audio[0] = 1.0F;
        delay.process(audio, 1);
        return audio;
    };

    const auto unfiltered = make_impulse(0.0F, 0.0F);
    const auto low_cut = make_impulse(1000.0F, 0.0F);
    const auto high_cut = make_impulse(0.0F, 2000.0F);
    constexpr std::size_t first_echo = 96U;
    constexpr std::size_t second_echo = 192U;
    require(std::abs(unfiltered[first_echo] - 1.0F) < 1.0e-6F &&
                std::abs(low_cut[first_echo] - unfiltered[first_echo]) < 1.0e-6F &&
                std::abs(high_cut[first_echo] - unfiltered[first_echo]) < 1.0e-6F,
            "repeat-path cuts must leave the first echo unchanged");
    require(std::abs(unfiltered[second_echo] - 0.8F) < 1.0e-6F &&
                low_cut[second_echo] < unfiltered[second_echo] * 0.95F &&
                high_cut[second_echo] < unfiltered[second_echo] * 0.5F,
            "Low Cut and High Cut must shape the signal fed into later repeats");
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
    parameters.time_mode = tabletfx::DuckingDelayTimeMode::tempo_sync;
    parameters.tempo_bpm = 900.0F;
    parameters.tap_division = static_cast<tabletfx::DuckingDelayTapDivision>(255);
    parameters.low_cut_hz = -10.0F;
    parameters.high_cut_hz = 90000.0F;
    delay.configure(parameters);
    const auto safe = delay.parameters();
    require(safe.time_ms == 2000.0F && safe.feedback == 0.95F && safe.mix == 0.0F &&
                safe.threshold_db <= 0.0F && safe.attenuation_db <= 60.0F &&
                safe.release_ms <= 2000.0F && safe.tempo_bpm == 300.0F &&
                safe.tap_division == tabletfx::DuckingDelayTapDivision::quarter &&
                safe.low_cut_hz == 0.0F && safe.high_cut_hz <= 20000.0F,
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
        test_spillover_bypass_preserves_dry_and_emits_only_stored_tail();
        test_spillover_off_bypass_is_transparent_and_clears_the_tail();
        test_tempo_sync_uses_tap_division_and_keeps_free_time();
        test_repeat_cuts_leave_first_echo_and_filter_feedback_repeats();
        test_controls_are_bounded_and_invalid_samples_recover();
        std::cout << "Ducking Delay tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Ducking Delay test failure: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
