#include "tabletfx/effects/modulated_delay.hpp"

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

void test_fixed_delay_when_depth_is_zero() {
    tabletfx::ModulatedDelayParameters parameters;
    parameters.time_ms = 4.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.depth_ms = 0.0F;
    tabletfx::ModulatedDelay delay(1000.0F, 1, 100.0F, parameters);
    std::array<float, 8> audio{0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F};
    delay.process(audio, 1);
    const std::array<float, 8> expected{0, 0, 0, 0, 0.1F, 0.2F, 0.3F, 0.4F};
    for (std::size_t i = 0; i < audio.size(); ++i) {
        require(std::abs(audio[i] - expected[i]) < 1.0e-6F,
                "zero-depth modulation must preserve the configured delay time and mix");
    }
}

std::vector<float> render_impulse(tabletfx::ModulatedDelayWaveform waveform,
                                  float spread_degrees = 180.0F) {
    tabletfx::ModulatedDelayParameters parameters;
    parameters.time_ms = 8.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.rate_hz = 62.5F;
    parameters.depth_ms = 3.0F;
    parameters.waveform = waveform;
    parameters.stereo_spread_degrees = spread_degrees;
    tabletfx::ModulatedDelay delay(1000.0F, 2, 100.0F, parameters);
    std::vector<float> audio(48U * 2U, 0.0F);
    audio[0] = 1.0F;
    audio[1] = 1.0F;
    delay.process(audio, 2);
    return audio;
}

void test_modulation_spreads_repeat_energy_and_stereo_phase() {
    const auto stereo = render_impulse(tabletfx::ModulatedDelayWaveform::sine);
    const auto mono_phase = render_impulse(tabletfx::ModulatedDelayWaveform::sine, 0.0F);
    std::size_t left_active = 0;
    std::size_t right_active = 0;
    bool stereo_differs = false;
    bool synced_channels = true;
void test_stereo_memory_delay_times_are_independent_only_when_enabled() {
    tabletfx::ModulatedDelayParameters linked_parameters;
    linked_parameters.time_ms = 4.0F;
    linked_parameters.right_time_ms = 8.0F;
    linked_parameters.independent_stereo_times = false;
    linked_parameters.feedback = 0.0F;
    linked_parameters.mix = 1.0F;
    linked_parameters.depth_ms = 0.0F;
    tabletfx::ModulatedDelay linked(1000.0F, 2, 100.0F, linked_parameters);
    std::array<float, 20> linked_audio{};
    linked_audio[0] = 1.0F;
    linked_audio[1] = 1.0F;
    linked.process(linked_audio, 2);
    require(std::abs(linked_audio[8] - 1.0F) < 1.0e-6F &&
                std::abs(linked_audio[9] - 1.0F) < 1.0e-6F,
            "linked stereo timing must retain the left time on both channels");

    linked_parameters.independent_stereo_times = true;
    tabletfx::ModulatedDelay independent(1000.0F, 2, 100.0F,
                                         linked_parameters);
    std::array<float, 20> independent_audio{};
    independent_audio[0] = 1.0F;
    independent_audio[1] = 1.0F;
    independent.process(independent_audio, 2);
    require(std::abs(independent_audio[8] - 1.0F) < 1.0e-6F &&
                std::abs(independent_audio[9]) < 1.0e-6F &&
                std::abs(independent_audio[17] - 1.0F) < 1.0e-6F,
            "independent stereo timing must delay right-channel repeats separately");

    std::array<float, 128> linked_signal{};
    std::array<float, 128> independent_signal{};
    for (std::size_t frame = 0; frame < linked_signal.size() / 2U; ++frame) {
        const float left = static_cast<float>((frame * 17U) % 31U) / 31.0F - 0.5F;
        const float right = static_cast<float>((frame * 11U) % 29U) / 29.0F - 0.5F;
        linked_signal[frame * 2U] = left;
        linked_signal[frame * 2U + 1U] = right;
        independent_signal[frame * 2U] = left;
        independent_signal[frame * 2U + 1U] = right;
    }
    linked.reset();
    independent.reset();
    linked.process(linked_signal, 2);
    independent.process(independent_signal, 2);
    float left_channel_difference = 0.0F;
    float right_channel_difference = 0.0F;
    for (std::size_t frame = 0; frame < linked_signal.size() / 2U; ++frame) {
        left_channel_difference = std::max(
            left_channel_difference,
            std::abs(linked_signal[frame * 2U] - independent_signal[frame * 2U]));
        right_channel_difference = std::max(
            right_channel_difference,
            std::abs(linked_signal[frame * 2U + 1U] - independent_signal[frame * 2U + 1U]));
    }
    require(left_channel_difference == 0.0F && right_channel_difference > 1.0e-3F,
            "independent right timing must change only the right channel");
}

    for (std::size_t frame = 3; frame < 14; ++frame) {
        const float left = stereo[frame * 2U];
        const float right = stereo[frame * 2U + 1U];
        if (std::abs(left) > 1.0e-5F) ++left_active;
        if (std::abs(right) > 1.0e-5F) ++right_active;
        stereo_differs = stereo_differs || std::abs(left - right) > 1.0e-4F;
        synced_channels = synced_channels && std::abs(mono_phase[frame * 2U] -
                                                        mono_phase[frame * 2U + 1U]) < 1.0e-6F;
    }
    require(left_active > 1 && right_active > 1,
            "modulation must spread the delayed impulse over multiple read positions");
    require(stereo_differs,
            "stereo spread must offset modulation between the left and right channels");
    require(synced_channels,
            "zero stereo spread must keep left and right delay modulation in phase");
}

void test_waveforms_and_triggered_mode_are_deterministic() {
    for (const auto mode : {tabletfx::ModulatedDelayWaveform::sine,
                            tabletfx::ModulatedDelayWaveform::triangle,
                            tabletfx::ModulatedDelayWaveform::random,
                            tabletfx::ModulatedDelayWaveform::triggered_ramp}) {
        auto first = render_impulse(mode, 90.0F);
        auto second = render_impulse(mode, 90.0F);
        require(first == second,
                "each modulator waveform must render deterministically after reset");
        require(std::all_of(first.begin(), first.end(), [](float sample) {
                    return std::isfinite(sample);
                }),
                "modulated delay must keep every waveform finite");
    }

    tabletfx::ModulatedDelayParameters parameters;
    parameters.time_ms = 4.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.depth_ms = 1.0F;
    parameters.rate_hz = 125.0F;
    parameters.waveform = tabletfx::ModulatedDelayWaveform::triggered_ramp;
    parameters.trigger_threshold_db = -12.0F;
    tabletfx::ModulatedDelay delay(1000.0F, 1, 100.0F, parameters);
    std::array<float, 16> audio{};
    audio[0] = 0.8F;
    audio[8] = 0.8F;
    delay.process(audio, 1);
    require(std::abs(audio[4]) > 0.0F && audio[8] == 0.0F &&
                std::abs(audio[12]) > 0.0F,
            "triggered-ramp mode must reset modulation on a rising input above threshold");
}

void test_controls_are_bounded_and_reset_clears_history() {
    tabletfx::ModulatedDelay delay(1000.0F, 1, 100.0F);
    auto parameters = delay.parameters();
    parameters.time_ms = 5000.0F;
    parameters.feedback = 4.0F;
    parameters.mix = -1.0F;
    parameters.rate_hz = 100.0F;
    parameters.depth_ms = 100.0F;
    parameters.stereo_spread_degrees = 400.0F;
    parameters.trigger_threshold_db = 10.0F;
    parameters.waveform = static_cast<tabletfx::ModulatedDelayWaveform>(99U);
    delay.configure(parameters);
    const auto safe = delay.parameters();
    require(safe.time_ms == 100.0F && safe.feedback == 0.95F && safe.mix == 0.0F &&
                safe.rate_hz == 10.0F && safe.depth_ms <= 50.0F &&
                safe.stereo_spread_degrees == 180.0F &&
                safe.trigger_threshold_db == 0.0F &&
                safe.waveform == tabletfx::ModulatedDelayWaveform::sine,
            "modulated delay controls must remain within project-authored bounds");
    require(delay.id() == "modulated-delay" && delay.memory_bytes() > sizeof(delay),
            "modulated delay identity or memory accounting is incorrect");

    auto test = safe;
    test.time_ms = 4.0F;
    test.feedback = 0.0F;
    test.mix = 1.0F;
    test.depth_ms = 0.0F;
    tabletfx::ModulatedDelay clearing(1000.0F, 1, 100.0F, test);
    std::array<float, 6> before{0.8F, 0, 0, 0, 0, 0};
    clearing.process(before, 1);
    clearing.reset();
    std::array<float, 6> after{0, 0, 0, 0, 0, 0};
    clearing.process(after, 1);
    require(std::all_of(after.begin(), after.end(), [](float sample) {
                return sample == 0.0F;
            }),
            "reset must clear delay history and restore modulation state");
}

std::array<float, 12> render_input_gain(float input_gain_db) {
    tabletfx::ModulatedDelayParameters parameters;
    parameters.time_ms = 4.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.depth_ms = 0.0F;
    parameters.input_gain_db = input_gain_db;
    tabletfx::ModulatedDelay delay(1000.0F, 1, 100.0F, parameters);
    std::array<float, 12> audio{};
    audio[0] = 0.1F;
    delay.process(audio, 1);
    return audio;
}

void test_memory_delay_gain_only_changes_the_recorded_wet_input() {
    const auto unity = render_input_gain(0.0F);
    const auto lower = render_input_gain(-10.0F);
    const auto higher = render_input_gain(20.0F);
    require(std::abs(unity[4] - 0.1F) < 1.0e-6F &&
                std::abs(lower[4] - 0.1F * std::pow(10.0F, -10.0F / 20.0F)) <
                    1.0e-6F &&
                std::abs(higher[4] - 1.0F) < 1.0e-6F,
            "input gain must scale the recorded echo within the documented -10..+20 dB range");
    require(unity[0] == lower[0] && unity[0] == higher[0],
            "input gain must leave the immediate dry path untouched");
}

void test_memory_delay_grit_is_off_by_default_and_deterministic() {
    tabletfx::ModulatedDelayParameters clean_parameters;
    clean_parameters.time_ms = 8.0F;
    clean_parameters.feedback = 0.0F;
    clean_parameters.mix = 1.0F;
    clean_parameters.depth_ms = 0.0F;
    std::array<float, 96> silence{};
    tabletfx::ModulatedDelay clean(1000.0F, 1, 100.0F, clean_parameters);
    clean.process(silence, 1);
    require(std::all_of(silence.begin(), silence.end(),
                        [](float sample) { return sample == 0.0F; }),
            "default-off Grit must preserve the previous silent output");

    auto gritty_parameters = clean_parameters;
    gritty_parameters.grit_percent = 100.0F;
    tabletfx::ModulatedDelay first(1000.0F, 1, 100.0F, gritty_parameters);
    tabletfx::ModulatedDelay second(1000.0F, 1, 100.0F, gritty_parameters);
    std::array<float, 96> first_audio{};
    std::array<float, 96> second_audio{};
    first.process(first_audio, 1);
    second.process(second_audio, 1);
    require(std::any_of(first_audio.begin(), first_audio.end(),
                        [](float sample) { return sample != 0.0F; }),
            "Grit must add a low-level BBD-noise-like wet component");
    require(first_audio == second_audio,
            "Grit noise must be deterministic for repeatable tests and renders");
    first.reset();
    std::array<float, 96> reset_audio{};
    first.process(reset_audio, 1);
    require(reset_audio == first_audio,
            "reset must restore the deterministic Grit noise sequence");
    require(std::all_of(first_audio.begin(), first_audio.end(),
                        [](float sample) { return std::abs(sample) < 0.001F; }),
            "Grit noise must remain bounded and subtle at its maximum setting");
}

void test_memory_delay_chorus_and_vibrato_modes_are_distinct() {
    auto chorus_parameters = tabletfx::ModulatedDelayParameters{};
    chorus_parameters.time_ms = 8.0F;
    chorus_parameters.mix = 0.25F;
    chorus_parameters.depth_ms = 0.0F;
    chorus_parameters.feedback = 0.0F;
    auto vibrato_parameters = chorus_parameters;
    vibrato_parameters.mode = tabletfx::ModulatedDelayMode::vibrato;

    tabletfx::ModulatedDelay chorus(1000.0F, 1, 100.0F, chorus_parameters);
    tabletfx::ModulatedDelay vibrato(1000.0F, 1, 100.0F, vibrato_parameters);
    std::array<float, 16> chorus_audio{};
    std::array<float, 16> vibrato_audio{};
    chorus_audio[0] = 1.0F;
    vibrato_audio[0] = 1.0F;
    chorus.process(chorus_audio, 1);
    vibrato.process(vibrato_audio, 1);
    require(std::abs(chorus_audio[0] - 0.75F) < 1.0e-6F &&
                vibrato_audio[0] == 0.0F,
            "Chorus blends the immediate dry signal while Vibrato uses a wet-only path");
    require(std::abs(chorus_audio[8] - 0.25F) < 1.0e-6F &&
                std::abs(vibrato_audio[8] - 0.25F) < 1.0e-6F,
            "both modes must retain the same selected wet delay level");
}

void test_memory_delay_controls_ramp_and_are_block_partition_invariant() {
    tabletfx::ModulatedDelayParameters parameters;
    parameters.time_ms = 6.0F;
    parameters.feedback = 0.4F;
    parameters.mix = 0.4F;
    parameters.depth_ms = 1.5F;
    parameters.input_gain_db = 8.0F;
    parameters.grit_percent = 35.0F;
    parameters.mode = tabletfx::ModulatedDelayMode::vibrato;
    tabletfx::ModulatedDelay one_block(1000.0F, 1, 100.0F, parameters);
    tabletfx::ModulatedDelay many_blocks(1000.0F, 1, 100.0F, parameters);
    std::array<float, 256> whole{};
    std::array<float, 256> divided{};
    for (std::size_t i = 0U; i < whole.size(); ++i) {
        whole[i] = std::sin(static_cast<float>(i) * 0.17F) * 0.2F;
    }
    divided = whole;
    one_block.process(whole, 1);
    std::size_t offset = 0U;
    constexpr std::array<std::size_t, 7> blocks{1U, 7U, 31U, 3U, 64U, 19U, 131U};
    for (const std::size_t count : blocks) {
        many_blocks.process(std::span<float>(divided).subspan(offset, count), 1);
        offset += count;
    }
    require(offset == divided.size() && whole == divided,
            "Gain/Grit/mode ramps and deterministic noise must be block-partition invariant");

    auto changed = one_block.parameters();
    changed.input_gain_db = -5.0F;
    changed.grit_percent = 0.0F;
    changed.mode = tabletfx::ModulatedDelayMode::chorus;
    one_block.configure(changed);
    std::array<float, 480> ramped{};
    one_block.process(ramped, 1);
    require(std::all_of(ramped.begin(), ramped.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "live mode/Gain/Grit changes must ramp without producing non-finite audio");
}

void test_memory_delay_spillover_keeps_the_dry_path_and_stops_recording() {
    tabletfx::ModulatedDelayParameters parameters;
    parameters.time_ms = 8.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 0.5F;
    parameters.depth_ms = 0.0F;
    parameters.spillover = true;
    tabletfx::ModulatedDelay delay(1000.0F, 1, 100.0F, parameters);
    std::array<float, 4> active{};
    active[0] = 0.5F;
    delay.process(active, 1);

    std::array<float, 8> bypassed{};
    bypassed[0] = 0.2F;
    delay.process_bypassed(bypassed, 1);
    require(std::abs(bypassed[0] - 0.2F) < 1.0e-6F &&
                std::abs(bypassed[4] - 0.25F) < 1.0e-6F,
            "spillover bypass must retain unity dry input and output the stored tail (dry=" +
                std::to_string(bypassed[0]) + ", tail=" + std::to_string(bypassed[4]) + ")");

    std::array<float, 8> after_bypass{};
    delay.process_bypassed(after_bypass, 1);
    require(std::all_of(after_bypass.begin(), after_bypass.end(),
                        [](float sample) { return sample == 0.0F; }),
            "bypassed input must not be recorded into the delay buffer");

    parameters.spillover = false;
    tabletfx::ModulatedDelay dry_bypass(1000.0F, 1, 100.0F, parameters);
    std::array<float, 8> dry_only{};
    dry_only[0] = 0.3F;
    dry_bypass.process_bypassed(dry_only, 1);
    require(dry_only[0] == 0.3F &&
                std::all_of(dry_only.begin() + 1, dry_only.end(),
                            [](float sample) { return sample == 0.0F; }),
            "Spillover off must remain dry-only");
}

void test_memory_delay_bounds_nonfinite_audio_and_no_process_allocations() {
    tabletfx::ModulatedDelay delay(1000.0F, 1, 100.0F);
    auto parameters = delay.parameters();
    parameters.input_gain_db = 99.0F;
    parameters.grit_percent = 500.0F;
    parameters.mode = static_cast<tabletfx::ModulatedDelayMode>(99U);
    delay.configure(parameters);
    require(delay.parameters().input_gain_db == 20.0F &&
                delay.parameters().grit_percent == 100.0F &&
                delay.parameters().mode == tabletfx::ModulatedDelayMode::chorus,
            "Memory Delay controls must clamp to documented/project-authored bounds");

    auto safe_parameters = delay.parameters();
    safe_parameters.time_ms = 4.0F;
    safe_parameters.mix = 1.0F;
    safe_parameters.depth_ms = 0.0F;
    safe_parameters.feedback = 0.0F;
    safe_parameters.input_gain_db = 0.0F;
    safe_parameters.grit_percent = 0.0F;
    delay.configure(safe_parameters);
    delay.reset();
    std::array<float, 24> audio{};
    audio[0] = std::numeric_limits<float>::quiet_NaN();
    audio[1] = std::numeric_limits<float>::infinity();
    const auto allocations_before =
        g_allocation_count.load(std::memory_order_relaxed);
    delay.process(audio, 1);
    const auto allocations_after =
        g_allocation_count.load(std::memory_order_relaxed);
    require(allocations_before == allocations_after,
            "process() must not allocate with Memory Delay controls active");
    require(std::all_of(audio.begin(), audio.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "Memory Delay must recover from non-finite input samples");
}

} // namespace

int main() {
    try {
        test_fixed_delay_when_depth_is_zero();
        test_modulation_spreads_repeat_energy_and_stereo_phase();
        test_waveforms_and_triggered_mode_are_deterministic();
        test_controls_are_bounded_and_reset_clears_history();
        test_memory_delay_gain_only_changes_the_recorded_wet_input();
        test_memory_delay_grit_is_off_by_default_and_deterministic();
        test_memory_delay_chorus_and_vibrato_modes_are_distinct();
        test_memory_delay_controls_ramp_and_are_block_partition_invariant();
        test_memory_delay_spillover_keeps_the_dry_path_and_stops_recording();
        test_memory_delay_bounds_nonfinite_audio_and_no_process_allocations();
        std::cout << "Modulated Delay tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Modulated Delay test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
        test_stereo_memory_delay_times_are_independent_only_when_enabled();
