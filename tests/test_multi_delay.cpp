#include "tabletfx/effects/multi_delay.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>

namespace {

std::atomic<bool> watch_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

tabletfx::MultiDelayParameters impulse_parameters() {
    tabletfx::MultiDelayParameters parameters;
    parameters.time_ms = {1.0F, 2.0F, 3.0F, 4.0F};
    parameters.time_mode.fill(tabletfx::DelayTimeMode::milliseconds);
    parameters.feedback.fill(0.0F);
    parameters.cross_feedback.fill(0.0F);
    parameters.level = {1.0F, 1.0F, 1.0F, 1.0F};
    parameters.pan = {0.0F, 1.0F, 0.5F, 0.5F};
    parameters.low_cut_hz = 0.0F;
    parameters.high_cut_hz = 20000.0F;
    parameters.mix = 1.0F;
    return parameters;
}

} // namespace

void* operator new(std::size_t size) {
    if (watch_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    if (watch_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

void test_four_independent_times_and_pans() {
    auto parameters = impulse_parameters();
    tabletfx::MultiDelay delay(8000.0F, 100.0F, parameters);
    require(delay.delay_samples(0U) == 8U &&
                delay.delay_samples(1U) == 16U &&
                delay.delay_samples(2U) == 24U &&
                delay.delay_samples(3U) == 32U,
            "the four parallel lines must retain independent delay times");

    std::array<float, 80> stereo{};
    stereo[0] = 1.0F;
    stereo[1] = 1.0F;
    delay.process(stereo, 2U);
    require(stereo[0] == 0.0F && stereo[1] == 0.0F &&
                stereo[16] > 0.8F && stereo[16] > stereo[17] &&
                stereo[33] > 0.8F && stereo[33] > stereo[32],
            "tap times and per-line stereo pan must be audible at their outputs");
}

void test_cyclic_cross_feedback_routes_between_lines() {
    auto parameters = impulse_parameters();
    parameters.level = {0.0F, 1.0F, 0.0F, 0.0F};
    parameters.pan = {0.5F, 0.5F, 0.5F, 0.5F};
    parameters.cross_feedback[0] = 0.5F;
    tabletfx::MultiDelay delay(8000.0F, 100.0F, parameters);
    std::array<float, 40> mono{};
    mono[0] = 1.0F;
    delay.process(mono, 1U);
    require(mono[16] > 0.8F && mono[24] > mono[16] * 0.35F &&
                mono[24] < mono[16] * 0.65F,
            "line 1 must cross-feed line 2 and create its later repeat");
}

void test_quad_tap_keeps_four_feedback_lines_independent() {
    auto parameters = impulse_parameters();
    parameters.topology = tabletfx::MultiDelayTopology::quad_tap;
    parameters.level = {0.0F, 1.0F, 0.0F, 0.0F};
    parameters.feedback[1] = 0.5F;
    parameters.master_feedback = 0.8F;
    parameters.cross_feedback[0] = 0.5F;
    tabletfx::MultiDelay delay(8000.0F, 100.0F, parameters);
    std::array<float, 48> mono{};
    mono[0] = 1.0F;
    delay.process(mono, 1U);
    require(mono[16] > 0.8F && std::abs(mono[24]) < 1e-6F &&
                mono[32] > 0.3F && mono[32] < 0.45F,
            "Quad-Tap lines must keep their self-feedback independent and ignore cross-feedback");
    parameters = delay.parameters();
    parameters.master_time = 1.5F;
    delay.configure(parameters);
    require(delay.delay_samples(0U) == 12U &&
                delay.delay_samples(1U) == 24U &&
                delay.delay_samples(2U) == 36U &&
                delay.delay_samples(3U) == 48U,
            "Quad-Tap Master Time must scale all four independent delays");
}

void test_quad_tap_band_places_filters_outside_feedback_loop() {
    const auto render = [](tabletfx::MultiDelayTopology topology) {
        auto parameters = impulse_parameters();
        parameters.topology = topology;
        parameters.time_ms = {1.0F, 100.0F, 100.0F, 100.0F};
        parameters.level = {1.0F, 0.0F, 0.0F, 0.0F};
        parameters.feedback[0] = 0.5F;
        parameters.master_feedback = 1.0F;
        parameters.high_cut_hz = 1000.0F;
        tabletfx::MultiDelay delay(8000.0F, 100.0F, parameters);
        std::array<float, 32> mono{};
        mono[0] = 1.0F;
        delay.process(mono, 1U);
        return mono;
    };
    const auto filtered_feedback = render(tabletfx::MultiDelayTopology::quad_tap);
    const auto external_filter = render(tabletfx::MultiDelayTopology::quad_tap_band);
    require(external_filter[8] > 0.1F &&
                external_filter[16] > filtered_feedback[16] * 1.4F,
            "Quad Tap Band must filter audible taps outside the feedback loop");
}

void test_self_feedback_and_negative_phase() {
    auto parameters = impulse_parameters();
    parameters.time_ms = {1.0F, 10.0F, 10.0F, 10.0F};
    parameters.level = {1.0F, 0.0F, 0.0F, 0.0F};
    parameters.feedback[0] = 0.4F;
    tabletfx::MultiDelay delay(8000.0F, 100.0F, parameters);
    std::array<float, 32> mono{};
    mono[0] = 1.0F;
    delay.process(mono, 1U);
    require(mono[8] > 0.8F && mono[16] > mono[8] * 0.3F &&
                mono[16] < mono[8] * 0.5F,
            "self feedback must repeat a selected line at a bounded gain");

    parameters.feedback[0] = -0.4F;
    delay.configure(parameters);
    delay.reset();
    mono.fill(0.0F);
    mono[0] = 1.0F;
    delay.process(mono, 1U);
    require(mono[8] > 0.8F && mono[16] < 0.0F,
            "negative feedback must invert the next repeat's polarity");
}

void test_tempo_sync_and_project_bounds() {
    auto parameters = impulse_parameters();
    parameters.time_mode[0] = tabletfx::DelayTimeMode::tempo_sync;
    parameters.division[0] = tabletfx::DelayDivision::quarter;
    parameters.tempo_bpm = 120.0F;
    tabletfx::MultiDelay delay(8000.0F, 2000.0F, parameters);
    require(delay.delay_samples(0U) == 4000U,
            "quarter-note sync at 120 BPM must resolve to 500 ms");

    parameters = delay.parameters();
    parameters.time_ms[0] = 9000.0F;
    parameters.time_ms[1] = -5.0F;
    parameters.feedback[0] = 3.0F;
    parameters.cross_feedback[0] = -3.0F;
    parameters.level[0] = 9.0F;
    parameters.pan[1] = -2.0F;
    parameters.mix = 2.0F;
    delay.configure(parameters);
    const auto bounded = delay.parameters();
    require(bounded.time_ms[0] == 2000.0F && bounded.time_ms[1] == 1.0F &&
                bounded.feedback[0] == 0.45F &&
                bounded.cross_feedback[0] == -0.45F &&
                bounded.level[0] == 2.0F && bounded.pan[1] == 0.0F &&
                bounded.mix == 1.0F,
            "project-authored ranges must clamp times, feedback, levels and mix");
}

void test_quad_series_compounds_delays_and_routes_selected_feedback() {
    auto parameters = impulse_parameters();
    parameters.topology = tabletfx::MultiDelayTopology::quad_series;
    parameters.time_ms = {1.0F, 2.0F, 3.0F, 4.0F};
    parameters.feedback.fill(0.0F);
    parameters.cross_feedback.fill(0.0F);
    tabletfx::MultiDelay series(8000.0F, 100.0F, parameters);
    std::array<float, 112> mono{};
    mono[0] = 1.0F;
    series.process(mono, 1U);
    require(mono[8] > 0.5F && mono[24] > 0.5F &&
                mono[48] > 0.5F && mono[80] > 0.5F,
            "Quad Series taps must occur at compounded cumulative delay times");

    parameters = impulse_parameters();
    parameters.topology = tabletfx::MultiDelayTopology::quad_series;
    parameters.time_ms = {1.0F, 20.0F, 30.0F, 40.0F};
    parameters.feedback.fill(0.0F);
    parameters.cross_feedback.fill(0.0F);
    parameters.level = {1.0F, 0.0F, 0.0F, 0.0F};
    parameters.series_feedback = 0.5F;
    parameters.series_feedback_send = 0U;
    parameters.series_feedback_return = 0U;
    tabletfx::MultiDelay feedback(8000.0F, 100.0F, parameters);
    mono.fill(0.0F);
    mono[0] = 1.0F;
    feedback.process(mono, 1U);
    require(mono[8] > 0.8F && mono[16] > 0.35F && mono[16] < 0.65F,
            "Quad Series send/return feedback must create a bounded repeat");

    parameters.series_feedback = 4.0F;
    parameters.series_feedback_send = 9U;
    parameters.series_feedback_return = 9U;
    feedback.configure(parameters);
    const auto bounded = feedback.parameters();
    require(bounded.series_feedback == 0.9F &&
                bounded.series_feedback_send == 3U &&
                bounded.series_feedback_return == 3U,
            "project Quad Series feedback and selector bounds must be enforced");
}

void test_quad_diffusor_allpass_chain_and_project_master_controls() {
    auto parameters = impulse_parameters();
    parameters.topology = tabletfx::MultiDelayTopology::quad_diffusor;
    parameters.time_ms = {1.0F, 2.0F, 3.0F, 4.0F};
    parameters.master_feedback = 0.5F;
    parameters.cross_feedback.fill(0.0F);
    parameters.low_cut_hz = 0.0F;
    parameters.high_cut_hz = 20000.0F;
    parameters.mix = 1.0F;
    tabletfx::MultiDelay diffuser(8000.0F, 100.0F, parameters);
    std::array<float, 128> mono{};
    mono[0] = 1.0F;
    diffuser.process(mono, 1U);
    require(std::abs(mono[0]) > 0.03F && std::abs(mono[80]) > 0.01F,
            "Quad Diffusor must cascade four feedback all-pass sections");
    require(std::all_of(mono.begin(), mono.end(), [](float sample) {
                return std::isfinite(sample) && std::abs(sample) <= 8.0F;
            }),
            "Quad Diffusor output must remain finite and bounded");

    parameters = diffuser.parameters();
    parameters.master_time = 2.0F;
    parameters.master_feedback = 0.9F;
    diffuser.configure(parameters);
    require(diffuser.delay_samples(0U) == 16U &&
                diffuser.delay_samples(1U) == 32U &&
                diffuser.delay_samples(2U) == 48U &&
                diffuser.delay_samples(3U) == 64U,
            "Quad Diffusor Master Time must scale all four diffuser delays");
    parameters.master_time = 20.0F;
    parameters.master_feedback = 20.0F;
    diffuser.configure(parameters);
    require(diffuser.parameters().master_time == 2.0F &&
                diffuser.parameters().master_feedback == 0.9F,
            "project Quad Diffusor master-control bounds must be enforced");
}

void test_memory_cap_reset_and_process_no_allocations() {
    tabletfx::MultiDelay capped(384000.0F, 2000.0F);
    require(capped.maximum_delay_ms() < 200.0F &&
                capped.memory_bytes() <= sizeof(capped) +
                    tabletfx::multi_delay_storage_limit_bytes,
            "the fixed storage cap must bound high-rate delay duration");

    auto parameters = impulse_parameters();
    parameters.time_ms = {1.0F, 2.0F, 3.0F, 4.0F};
    parameters.cross_feedback[0] = 0.2F;
    tabletfx::MultiDelay delay(48000.0F, 100.0F, parameters);
    std::array<float, 256> audio{};
    audio[0] = 0.75F;
    audio[1] = -0.25F;
    allocation_count.store(0U, std::memory_order_relaxed);
    watch_allocations.store(true, std::memory_order_relaxed);
    delay.process(audio, 2U);
    watch_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "Multi-Delay process() must not allocate");
    require(std::all_of(audio.begin(), audio.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "stereo output with feedback must remain finite");

    delay.reset();
    std::array<float, 128> silence{};
    delay.process(silence, 2U);
    require(std::all_of(silence.begin(), silence.end(), [](float sample) {
                return sample == 0.0F;
            }),
            "reset must clear all four delay lines and filter state");

    std::array<float, 4> unsupported{1.0F, 2.0F, 3.0F, 4.0F};
    delay.process(unsupported, 3U);
    require(unsupported[0] == 1.0F && unsupported[3] == 4.0F,
            "unsupported channel layouts must leave input unchanged");
}

} // namespace

int main() {
    try {
        test_four_independent_times_and_pans();
        test_cyclic_cross_feedback_routes_between_lines();
        test_quad_tap_keeps_four_feedback_lines_independent();
        test_quad_tap_band_places_filters_outside_feedback_loop();
        test_self_feedback_and_negative_phase();
        test_tempo_sync_and_project_bounds();
        test_quad_series_compounds_delays_and_routes_selected_feedback();
        test_quad_diffusor_allpass_chain_and_project_master_controls();
        test_memory_cap_reset_and_process_no_allocations();
        std::cout << "Multi-Delay tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Multi-Delay tests failed: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
