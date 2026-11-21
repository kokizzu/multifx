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

} // namespace

int main() {
    try {
        test_fixed_delay_when_depth_is_zero();
        test_modulation_spreads_repeat_energy_and_stereo_phase();
        test_waveforms_and_triggered_mode_are_deterministic();
        test_controls_are_bounded_and_reset_clears_history();
        std::cout << "Modulated Delay tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Modulated Delay test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
