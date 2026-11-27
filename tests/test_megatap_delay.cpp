#include "tabletfx/effects/megatap_delay.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float left, float right, float tolerance = 1.0e-3F) {
    return std::abs(left - right) <= tolerance;
}

tabletfx::MegatapDelayParameters impulse_parameters() {
    tabletfx::MegatapDelayParameters parameters;
    parameters.delay_time_ms = 4.0F;
    parameters.tap_count = 4U;
    parameters.time_alpha = 1.0F;
    parameters.amplitude_shape = tabletfx::MegatapAmplitudeShape::constant;
    parameters.pan_shape = tabletfx::MegatapPanShape::constant;
    parameters.pan_alpha_percent = 50.0F;
    parameters.low_cut_hz = 0.0F;
    parameters.high_cut_hz = 20000.0F;
    parameters.mix = 1.0F;
    return parameters;
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

void test_even_taps_and_time_alpha() {
    auto parameters = impulse_parameters();
    parameters.delay_time_ms = 8.0F;
    tabletfx::MegatapDelay delay(8000.0F, 100.0F, parameters);
    require(delay.tap_delay_samples(0U) == 16U &&
                delay.tap_delay_samples(1U) == 32U &&
                delay.tap_delay_samples(2U) == 48U &&
                delay.tap_delay_samples(3U) == 64U,
            "Time Alpha 1 must space four taps evenly through Delay Time");

    parameters.time_alpha = 2.0F;
    delay.set_parameters(parameters);
    require(delay.tap_delay_samples(0U) == 4U &&
                delay.tap_delay_samples(1U) == 13U &&
                delay.tap_delay_samples(2U) == 30U &&
                delay.tap_delay_samples(3U) == 64U,
            "Time Alpha must progressively increase the tap spacing");
    require(delay.id() == "megatap-delay" &&
                delay.memory_bytes() <= sizeof(delay) +
                    tabletfx::megatap_delay_storage_limit_bytes,
            "Megatap identity or storage accounting is incorrect");
}

void test_predelay_and_tap_count_control_impulse_positions() {
    auto parameters = impulse_parameters();
    parameters.delay_time_ms = 4.0F;
    parameters.predelay_ms = 1.0F;
    tabletfx::MegatapDelay delay(8000.0F, 100.0F, parameters);
    require(delay.tap_delay_samples(0U) == 16U &&
                delay.tap_delay_samples(1U) == 24U &&
                delay.tap_delay_samples(2U) == 32U &&
                delay.tap_delay_samples(3U) == 40U,
            "Predelay must offset all taps without changing their spacing");

    std::array<float, 48> audio{};
    audio[0] = 1.0F;
    delay.process(audio, 1U);
    require(audio[0] == 0.0F && audio[16] > 0.9F && audio[24] > 0.9F &&
                audio[32] > 0.9F && audio[40] > 0.9F,
            "fully wet output must place the impulse at the four predelayed taps");
}

void test_feedback_repeats_the_selected_tap() {
    auto parameters = impulse_parameters();
    parameters.delay_time_ms = 1.0F;
    parameters.tap_count = 1U;
    parameters.amplitude_shape = tabletfx::MegatapAmplitudeShape::decreasing;
    parameters.feedback = 0.5F;
    parameters.feedback_tap = 1U;
    tabletfx::MegatapDelay delay(8000.0F, 100.0F, parameters);
    std::array<float, 32> audio{};
    audio[0] = 1.0F;
    delay.process(audio, 1U);
    require(audio[8] > 0.85F && audio[16] > audio[8] * 0.4F &&
                audio[16] < audio[8] * 0.6F,
            "Feedback Tap must repeat the selected tap at the configured gain");
}

void test_stereo_spread_and_input_selection() {
    auto parameters = impulse_parameters();
    parameters.delay_time_ms = 3.0F;
    parameters.tap_count = 3U;
    parameters.pan_shape = tabletfx::MegatapPanShape::increasing;
    parameters.pan_alpha_percent = 100.0F;
    parameters.stereo_spread_percent = 0.0F;
    parameters.input_select = tabletfx::MegatapInputSelect::right;
    tabletfx::MegatapDelay delay(8000.0F, 100.0F, parameters);
    require(delay.tap_pan(0U) < delay.tap_pan(1U) &&
                delay.tap_pan(1U) < delay.tap_pan(2U),
            "increasing pan shape must move taps from left to right");

    std::array<float, 64> audio{};
    audio[1] = 1.0F;
    delay.process(audio, 2U);
    require(audio[16] > audio[17] && audio[17] > 0.0F &&
                audio[48] < audio[49] && audio[49] > 0.9F,
            "selected right input must feed taps with the configured stereo pan");
}

void test_randomization_is_deterministic_and_controls_are_bounded() {
    auto parameters = impulse_parameters();
    parameters.delay_time_ms = 60.0F;
    parameters.tap_count = 10U;
    parameters.time_randomize_percent = 70.0F;
    parameters.amplitude_randomize_percent = 50.0F;
    parameters.random_seed = 123456U;
    tabletfx::MegatapDelay left(8000.0F, 100.0F, parameters);
    tabletfx::MegatapDelay right(8000.0F, 100.0F, parameters);
    for (std::size_t tap = 0; tap < parameters.tap_count; ++tap) {
        require(left.tap_delay_samples(tap) == right.tap_delay_samples(tap) &&
                    near(left.tap_gain(tap), right.tap_gain(tap)),
                "matching project seeds must produce repeatable randomized taps");
    }

    parameters.tap_count = 255U;
    parameters.delay_time_ms = 9000.0F;
    parameters.predelay_ms = 9000.0F;
    parameters.feedback = 3.0F;
    parameters.time_alpha = 30.0F;
    parameters.pan_alpha_percent = -1.0F;
    parameters.level_db = 90.0F;
    left.set_parameters(parameters);
    const auto bounded = left.parameters();
    require(bounded.tap_count == 64U && bounded.delay_time_ms == 100.0F &&
                bounded.predelay_ms == 0.0F && bounded.feedback == 0.95F &&
                bounded.time_alpha == 4.0F &&
                bounded.pan_alpha_percent == 0.0F && bounded.level_db == 12.0F,
            "project-authored controls must be clamped to documented bounds");
}

void test_project_global_mix_offset_is_opt_in_bounded_and_allocation_free() {
    const auto render = [](tabletfx::MegatapDelayParameters parameters) {
        parameters.delay_time_ms = 4.0F;
        parameters.tap_count = 1U;
        parameters.amplitude_shape = tabletfx::MegatapAmplitudeShape::constant;
        parameters.pan_shape = tabletfx::MegatapPanShape::constant;
        parameters.low_cut_hz = 0.0F;
        tabletfx::MegatapDelay delay(8000.0F, 100.0F, parameters);
        std::array<float, 48U> audio{};
        audio[0] = 1.0F;
        delay.process(audio, 1U);
        return audio;
    };

    tabletfx::MegatapDelayParameters parameters;
    const auto base = render(parameters);
    parameters.global_mix_offset = 0.2F;
    const auto disabled = render(parameters);
    require(base == disabled,
            "a stored Global Mix offset must be inaudible while disabled");

    parameters.global_mix_enabled = true;
    parameters.delay_time_ms = 4.0F;
    parameters.tap_count = 1U;
    parameters.amplitude_shape = tabletfx::MegatapAmplitudeShape::constant;
    parameters.pan_shape = tabletfx::MegatapPanShape::constant;
    parameters.low_cut_hz = 0.0F;
    auto enabled_delay = tabletfx::MegatapDelay(8000.0F, 100.0F, parameters);
    const std::size_t memory_before = enabled_delay.memory_bytes();
    require(near(enabled_delay.parameters().mix, 0.25F) &&
                near(enabled_delay.parameters().global_mix_offset, 0.2F),
            "Global Mix must preserve the stored base Mix and offset");
    std::array<float, 48U> enabled{};
    enabled[0] = 1.0F;
    const std::size_t allocations_before =
        allocation_count.load(std::memory_order_relaxed);
    enabled_delay.process(enabled, 1U);
    const std::size_t allocations_after =
        allocation_count.load(std::memory_order_relaxed);
    require(near(enabled[0], 0.55F),
            "enabled +20 pp must add to the base Mix for the dry output");
    require(enabled[32U] > disabled[32U],
            "enabled +20 pp must raise the delayed wet output: enabled=" +
                std::to_string(enabled[32U]) + ", disabled=" +
                std::to_string(disabled[32U]));
    require(allocations_before == allocations_after,
            "Global Mix processing must not allocate in the audio callback");
    require(enabled_delay.memory_bytes() == memory_before,
            "Global Mix changes must not add or resize delay storage");

    parameters.mix = 0.8F;
    parameters.global_mix_offset = 8.0F;
    const auto upper = render(parameters);
    tabletfx::MegatapDelay upper_delay(8000.0F, 100.0F, parameters);
    require(near(upper_delay.parameters().mix, 0.8F) &&
                near(upper_delay.parameters().global_mix_offset, 0.5F) &&
                near(upper[0], 0.0F),
            "positive offset must clamp at +50 pp and at 100% effective Mix");

    parameters.mix = 0.2F;
    parameters.global_mix_offset = -8.0F;
    const auto lower = render(parameters);
    tabletfx::MegatapDelay lower_delay(8000.0F, 100.0F, parameters);
    require(near(lower_delay.parameters().mix, 0.2F) &&
                near(lower_delay.parameters().global_mix_offset, -0.5F) &&
                near(lower[0], 1.0F) && near(lower[32U], 0.0F),
            "negative offset must clamp at -50 pp and at 0% effective Mix");

    parameters.mix = 0.25F;
    parameters.global_mix_offset = std::numeric_limits<float>::infinity();
    const auto non_finite = render(parameters);
    tabletfx::MegatapDelay non_finite_delay(8000.0F, 100.0F, parameters);
    require(near(non_finite_delay.parameters().global_mix_offset, 0.0F) &&
                non_finite == base,
            "non-finite project offset must fall back to zero and preserve output");
}

void test_diffusion_envelope_reset_and_process_no_allocations() {
    auto parameters = impulse_parameters();
    parameters.diffusion_mix = 0.5F;
    parameters.diffusion_time_ms = 10.0F;
    parameters.diffusion_lfo_depth_ms = 0.5F;
    parameters.envelope_enabled = true;
    tabletfx::MegatapDelay delay(48000.0F, 100.0F, parameters);
    std::array<float, 256> audio{};
    audio[0] = 0.75F;
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    delay.process(audio, 1U);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(before == after,
            "audio processing must not allocate after the delay is constructed");
    require(std::all_of(audio.begin(), audio.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "diffusion and envelope processing must keep output finite");

    delay.reset();
    std::array<float, 64> silence{};
    delay.process(silence, 1U);
    require(std::all_of(silence.begin(), silence.end(), [](float sample) {
                return sample == 0.0F;
            }),
            "reset must clear the delay, diffuser, filter, and envelope state");

    std::array<float, 4> unchanged{1.0F, 2.0F, 3.0F, 4.0F};
    delay.process(unchanged, 3U);
    require(unchanged[0] == 1.0F && unchanged[3] == 4.0F,
            "unsupported channel layouts must be left unchanged");
}

void test_sample_rate_and_storage_validation() {
    bool rejected = false;
    try {
        tabletfx::MegatapDelay invalid(7000.0F);
        (void)invalid;
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "sample rates below 8 kHz must be rejected");

    tabletfx::MegatapDelay capped(384000.0F, 10000.0F);
    require(capped.maximum_delay_ms() < 700.0F &&
                capped.memory_bytes() <= sizeof(capped) +
                    tabletfx::megatap_delay_storage_limit_bytes,
            "the project storage cap must limit high-rate delay duration");
}

} // namespace

int main() {
    try {
        test_even_taps_and_time_alpha();
        test_predelay_and_tap_count_control_impulse_positions();
        test_feedback_repeats_the_selected_tap();
        test_stereo_spread_and_input_selection();
        test_randomization_is_deterministic_and_controls_are_bounded();
        test_project_global_mix_offset_is_opt_in_bounded_and_allocation_free();
        test_diffusion_envelope_reset_and_process_no_allocations();
        test_sample_rate_and_storage_validation();
        std::cout << "Megatap Delay tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Megatap Delay tests failed: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
