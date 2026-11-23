#include "tabletfx/effects/step_filter_delay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_defaults_and_bounds_are_project_authored() {
    tabletfx::StepFilterDelay effect;
    const auto reported_memory_bytes = effect.memory_bytes();
    if (reported_memory_bytes > 800'000U) {
        throw std::runtime_error("Step Filter Delay memory_bytes=" +
                                 std::to_string(reported_memory_bytes) +
                                 " exceeds the project-authored 800 kB budget");
    }
    const auto defaults = effect.parameters();
    require(effect.id() == "step-filter-delay",
            "Step Filter Delay ID changed");
    require(defaults.delay_time_ms == 350.0F &&
                defaults.feedback == 0.35F && defaults.mix == 0.25F &&
                defaults.steps.size() == 16U &&
                defaults.step_tempo_sync &&
                defaults.step_division == tabletfx::DelayDivision::sixteenth,
            "project-authored defaults changed");
    require(effect.memory_bytes() > sizeof(effect),
            "delay storage is missing from memory accounting");

    auto parameters = defaults;
    parameters.tempo_bpm = 999.0F;
    parameters.delay_time_ms = -1.0F;
    parameters.feedback = 2.0F;
    parameters.mix = -1.0F;
    parameters.minimum_cutoff_hz = 90000.0F;
    parameters.maximum_cutoff_hz = 1.0F;
    parameters.resonance_q = 100.0F;
    parameters.depth = -1.0F;
    parameters.shape = 2.0F;
    parameters.smoothing_ms = -1.0F;
    parameters.step_rate_hz = -3.0F;
    parameters.filter_mode = static_cast<tabletfx::StepFilterDelayMode>(255U);
    parameters.delay_division = static_cast<tabletfx::DelayDivision>(255U);
    parameters.steps[0] = std::numeric_limits<float>::infinity();
    tabletfx::StepFilterDelay bounded(8000.0F, 1, 2000.0F, parameters);
    const auto safe = bounded.parameters();
    require(safe.tempo_bpm == 300.0F && safe.delay_time_ms == 1.0F &&
                safe.feedback == 0.95F && safe.mix == 0.0F &&
                safe.minimum_cutoff_hz == 3200.0F &&
                safe.maximum_cutoff_hz == 3200.0F &&
                safe.resonance_q == 12.0F && safe.depth == 0.0F &&
                safe.shape == 1.0F && safe.smoothing_ms == 0.0F &&
                safe.step_rate_hz == 0.05F &&
                safe.filter_mode == tabletfx::StepFilterDelayMode::LowPass &&
                safe.delay_division == tabletfx::DelayDivision::sixteenth &&
                safe.steps[0] == 0.5F,
            "non-finite or out-of-range controls were not bounded");
}

void test_tempo_clock_and_delayed_wet_signal() {
    tabletfx::StepFilterDelayParameters parameters;
    parameters.delay_time_ms = 10.0F;
    parameters.delay_tempo_sync = true;
    parameters.delay_division = tabletfx::DelayDivision::quarter;
    parameters.tempo_bpm = 120.0F;
    parameters.step_tempo_sync = true;
    parameters.step_division = tabletfx::DelayDivision::sixteenth;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.minimum_cutoff_hz = 1000.0F;
    parameters.maximum_cutoff_hz = 1000.0F;
    parameters.depth = 0.0F;
    parameters.smoothing_ms = 0.0F;
    parameters.steps.fill(0.5F);
    tabletfx::StepFilterDelay effect(8000.0F, 1, 2000.0F, parameters);

    require(std::abs(effect.effective_delay_time_ms() - 500.0F) < 1.0e-4F &&
                std::abs(effect.effective_step_rate_hz() - 8.0F) < 1.0e-5F,
            "project-authored delay/step tempo conversion is incorrect");

    parameters.delay_tempo_sync = false;
    parameters.delay_time_ms = 10.0F;
    tabletfx::StepFilterDelay short_delay(8000.0F, 1, 2000.0F, parameters);
    std::array<float, 96> impulse{};
    impulse[0] = 0.8F;
    short_delay.process(impulse, 1);
    require(impulse[0] == 0.0F && impulse[79] == 0.0F &&
                impulse[80] > 0.0F && impulse[80] < 0.8F,
            "100% wet output must return a filtered repeat at the configured delay");
}

void test_filter_step_changes_repeats() {
    tabletfx::StepFilterDelayParameters parameters;
    parameters.delay_time_ms = 100.0F;
    parameters.delay_tempo_sync = false;
    parameters.step_tempo_sync = false;
    parameters.step_rate_hz = 8.0F;
    parameters.feedback = 0.8F;
    parameters.mix = 1.0F;
    parameters.minimum_cutoff_hz = 120.0F;
    parameters.maximum_cutoff_hz = 3000.0F;
    parameters.resonance_q = 0.707F;
    parameters.shape = 0.0F;
    parameters.smoothing_ms = 0.0F;
    parameters.steps.fill(0.0F);

    tabletfx::StepFilterDelay stepped(8000.0F, 1, 2000.0F, parameters);
    auto static_parameters = parameters;
    static_parameters.steps.fill(0.0F);
    tabletfx::StepFilterDelay static_filter(8000.0F, 1, 2000.0F,
                                            static_parameters);
    auto stepped_audio = std::vector<float>(1700U, 0.0F);
    auto static_audio = stepped_audio;
    stepped_audio[0] = 1.0F;
    static_audio[0] = 1.0F;
    parameters.steps[1] = 1.0F;
    stepped.configure(parameters);
    stepped.process(stepped_audio, 1);
    static_filter.process(static_audio, 1);
    require(std::abs(stepped_audio[1600] - static_audio[1600]) > 1.0e-4F,
            "changing a step must change a later filtered repeat");
}

void test_stereo_isolation_reset_and_invalid_audio() {
    tabletfx::StepFilterDelayParameters parameters;
    parameters.delay_time_ms = 2.0F;
    parameters.feedback = 0.5F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::StepFilterDelay stereo(48000.0F, 2, 2000.0F, parameters);
    std::array<float, 256> audio{};
    audio[0] = 0.8F;
    stereo.process(audio, 2);
    require(audio[0] == 0.0F && audio[1] == 0.0F,
            "100% wet stereo output leaked immediate dry audio");
    for (std::size_t frame = 0; frame < audio.size() / 2U; ++frame) {
        require(audio[frame * 2U + 1U] == 0.0F,
                "dual-mono delay crossfed left signal into the right channel");
    }

    const auto first_run = audio;
    stereo.reset();
    audio.fill(0.0F);
    audio[0] = 0.8F;
    stereo.process(audio, 2);
    require(audio == first_run, "reset did not restore deterministic delay state");

    std::array<float, 4> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
    };
    stereo.process(invalid, 1); // Wrong layout must leave data untouched.
    require(std::isnan(invalid[0]),
            "unsupported channel layouts should be left untouched");
    stereo.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float value) { return std::isfinite(value); }),
            "invalid/extreme input produced non-finite output");
}

void test_variable_block_sizes_match() {
    tabletfx::StepFilterDelayParameters parameters;
    parameters.delay_time_ms = 13.5F;
    parameters.step_tempo_sync = false;
    parameters.step_rate_hz = 7.3F;
    parameters.feedback = 0.42F;
    parameters.mix = 0.6F;
    parameters.resonance_q = 4.0F;
    parameters.shape = 0.4F;
    parameters.smoothing_ms = 1.0F;
    tabletfx::StepFilterDelay whole(44100.0F, 2, 2000.0F, parameters);
    tabletfx::StepFilterDelay chunked(44100.0F, 2, 2000.0F, parameters);

    std::vector<float> expected(4096U);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        expected[index] = 0.4F * std::sin(static_cast<float>(index) * 0.01F);
    }
    auto actual = expected;
    whole.process(expected, 2);
    constexpr std::array<std::size_t, 5> block_sizes{1U, 17U, 128U, 53U, 301U};
    std::size_t frame = 0;
    std::size_t block = 0;
    while (frame < actual.size() / 2U) {
        const std::size_t frames = std::min(
            block_sizes[block % block_sizes.size()], actual.size() / 2U - frame);
        chunked.process(std::span<float>(actual).subspan(frame * 2U, frames * 2U), 2);
        frame += frames;
        ++block;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(std::abs(expected[index] - actual[index]) < 2.0e-6F,
                "variable block sizes changed the effect output");
    }
}

} // namespace

int main() {
    try {
        test_defaults_and_bounds_are_project_authored();
        test_tempo_clock_and_delayed_wet_signal();
        test_filter_step_changes_repeats();
        test_stereo_isolation_reset_and_invalid_audio();
        test_variable_block_sizes_match();
        std::cout << "Step Filter Delay prototype tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Step Filter Delay prototype tests failed: "
                  << error.what() << '\n';
        return 1;
    }
}
