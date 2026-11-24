#include "tabletfx/effects/ducking_delay.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::atomic<std::size_t> allocation_count{};

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

std::size_t first_nonzero(const std::array<float, 120>& audio,
                          std::size_t begin, std::size_t end) {
    for (std::size_t index = begin; index < end; ++index) {
        if (std::abs(audio[index]) > 1.0e-5F) return index;
    }
    return audio.size();
}

std::array<float, 120> render_modulated_impulses(
    tabletfx::DuckingDelayModulationMode mode,
    float first_impulse = 0.8F,
    float second_impulse = 0.0F) {
    tabletfx::DuckingDelayParameters parameters;
    parameters.time_ms = 20.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.threshold_db = -6.0F;
    parameters.attenuation_db = 0.0F;
    parameters.mod_rate_hz = 10.0F;
    parameters.mod_depth_ms = 8.0F;
    parameters.modulation_mode = mode;
    tabletfx::DuckingDelay delay(1000.0F, 1, 100.0F, parameters);
    std::array<float, 120> audio{};
    audio[0] = first_impulse;
    audio[50] = second_impulse;
    delay.process(audio, 1);
    return audio;
}

void test_modulation_modes_are_distinct_bounded_and_deterministic() {
    using Mode = tabletfx::DuckingDelayModulationMode;
    const auto sine = render_modulated_impulses(Mode::sine);
    const auto triangle = render_modulated_impulses(Mode::triangle);
    const auto random_first = render_modulated_impulses(Mode::random);
    const auto random_second = render_modulated_impulses(Mode::random);
    const auto is_finite = [](const auto& samples) {
        return std::all_of(samples.begin(), samples.end(), [](float sample) {
            return std::isfinite(sample);
        });
    };

    require(first_nonzero(triangle, 5, 50) < first_nonzero(sine, 5, 50),
            "triangle and sine modes must modulate delay time differently");
    require(random_first == random_second,
            "random delay modulation must be deterministic after reset/construction");
    require(is_finite(sine) && is_finite(triangle) && is_finite(random_first),
            "all free-running modulation modes must produce finite samples");

    tabletfx::DuckingDelay delay;
    auto parameters = delay.parameters();
    parameters.mod_rate_hz = 100.0F;
    parameters.mod_depth_ms = 100.0F;
    parameters.modulation_mode = static_cast<Mode>(255);
    delay.configure(parameters);
    const auto safe = delay.parameters();
    require(safe.mod_rate_hz == 10.0F && safe.mod_depth_ms == 50.0F &&
                safe.modulation_mode == Mode::sine,
            "modulation controls must clamp to project bounds and invalid modes must fall back safely");

    parameters = safe;
    parameters.time_ms = 20.0F;
    parameters.mod_depth_ms = 50.0F;
    delay.configure(parameters);
    require(delay.parameters().mod_depth_ms <= 19.0F,
            "modulation depth must leave the selected delay inside the ring bounds");
}

void test_triggered_ramp_threshold_and_retrigger_behavior() {
    using Mode = tabletfx::DuckingDelayModulationMode;
    const auto below_threshold = render_modulated_impulses(Mode::triggered_ramp, 0.1F);
    const auto first_trigger = render_modulated_impulses(Mode::triggered_ramp, 0.8F);
    const auto retriggered = render_modulated_impulses(Mode::triggered_ramp, 0.8F, 0.8F);
    const auto not_retriggered = render_modulated_impulses(Mode::triggered_ramp, 0.8F, 0.4F);

    const auto quiet_echo = first_nonzero(below_threshold, 5, 45);
    const auto triggered_echo = first_nonzero(first_trigger, 5, 45);
    require(quiet_echo == 20U && triggered_echo > quiet_echo,
            "Triggered Ramp must remain unmodulated before Threshold and start its rising ramp after a crossing");

    const auto retriggered_echo = first_nonzero(retriggered, 60, 100);
    const auto continuing_echo = first_nonzero(not_retriggered, 60, 100);
    require(retriggered_echo < continuing_echo,
            "a second above-threshold onset must restart the Triggered Ramp while a quiet input does not");
}

void test_stereo_modulation_is_linked_resettable_and_allocation_free() {
    using Mode = tabletfx::DuckingDelayModulationMode;
    tabletfx::DuckingDelayParameters parameters;
    parameters.time_ms = 20.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.threshold_db = -6.0F;
    parameters.attenuation_db = 0.0F;
    parameters.mod_rate_hz = 10.0F;
    parameters.mod_depth_ms = 8.0F;
    parameters.modulation_mode = Mode::triggered_ramp;
    tabletfx::DuckingDelay delay(1000.0F, 2, 100.0F, parameters);

    std::array<float, 120U * 2U> audio{};
    audio[0] = 0.8F;
    audio[1] = 0.4F;
    const auto before = allocation_count.load(std::memory_order_relaxed);
    delay.process(audio, 2);
    const auto after = allocation_count.load(std::memory_order_relaxed);
    require(after == before,
            "Ducking Delay modulation must not allocate while processing");

    std::size_t left_echo = audio.size();
    std::size_t right_echo = audio.size();
    for (std::size_t frame = 1; frame < 60; ++frame) {
        if (left_echo == audio.size() && std::abs(audio[frame * 2U]) > 1.0e-5F) {
            left_echo = frame;
        }
        if (right_echo == audio.size() && std::abs(audio[frame * 2U + 1U]) > 1.0e-5F) {
            right_echo = frame;
        }
    }
    require(left_echo == right_echo && left_echo != audio.size(),
            "stereo channels must share the same modulation phase and trigger event");

    delay.reset();
    std::array<float, 120U * 2U> after_reset{};
    after_reset[0] = 0.8F;
    after_reset[1] = 0.4F;
    delay.process(after_reset, 2);
    require(after_reset == audio,
            "reset must restore modulation phase, trigger state, and delay history");

    // Exercise arbitrary callback partitioning with deterministic Random mode.
    parameters.modulation_mode = Mode::random;
    tabletfx::DuckingDelay one_block(1000.0F, 1, 100.0F, parameters);
    tabletfx::DuckingDelay split_blocks(1000.0F, 1, 100.0F, parameters);
    std::array<float, 120> whole{};
    whole[0] = 0.8F;
    auto split = whole;
    one_block.process(whole, 1);
    split_blocks.process(std::span<float>(split).first(13), 1);
    split_blocks.process(std::span<float>(split).subspan(13, 37), 1);
    split_blocks.process(std::span<float>(split).subspan(50), 1);
    require(whole == split,
            "modulation state and output must not depend on callback partitioning");
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
        test_modulation_modes_are_distinct_bounded_and_deterministic();
        test_triggered_ramp_threshold_and_retrigger_behavior();
        test_stereo_modulation_is_linked_resettable_and_allocation_free();
        test_controls_are_bounded_and_invalid_samples_recover();
        std::cout << "Ducking Delay tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Ducking Delay test failure: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
