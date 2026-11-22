#include "tabletfx/effects/ten_tap_delay.hpp"

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

bool near(float left, float right, float tolerance = 1.0e-4F) {
    return std::abs(left - right) <= tolerance;
}

tabletfx::TenTapDelayParameters impulse_parameters() {
    tabletfx::TenTapDelayParameters parameters;
    parameters.tap_count = 2;
    parameters.delay_time_ms = 1.0F;
    parameters.division = tabletfx::TenTapDivision::free;
    parameters.decay_percent = 0.0F;
    parameters.shuffle_percent = 0.0F;
    parameters.spread_percent = 0.0F;
    parameters.ratio_percent = 100.0F;
    parameters.pan_shape = tabletfx::TenTapPanShape::constant;
    parameters.pan_alpha_percent = 100.0F;
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

void test_documented_tap_count_and_project_parameter_bounds() {
    auto parameters = impulse_parameters();
    parameters.tap_count = 255U;
    parameters.delay_time_ms = 9000.0F;
    parameters.tempo_bpm = 0.0F;
    parameters.decay_percent = 200.0F;
    parameters.shuffle_percent = -5.0F;
    parameters.spread_percent = 300.0F;
    parameters.ratio_percent = 500.0F;
    parameters.pan_alpha_percent = -1.0F;
    parameters.mix = 2.0F;
    parameters.level_db = 100.0F;
    parameters.tap_level_db[0] = 80.0F;
    tabletfx::TenTapDelay delay(8000.0F, 1, 100.0F, parameters);
    const auto bounded = delay.parameters();
    require(bounded.tap_count == 10U, "tap count must remain within 1–10");
    require(near(bounded.delay_time_ms, 2000.0F) &&
                near(bounded.tempo_bpm, 20.0F) &&
                near(bounded.decay_percent, 100.0F) &&
                near(bounded.shuffle_percent, 0.0F) &&
                near(bounded.spread_percent, 100.0F) &&
                near(bounded.ratio_percent, 200.0F) &&
                near(bounded.pan_alpha_percent, 0.0F) &&
                near(bounded.mix, 1.0F) && near(bounded.level_db, 12.0F),
            "project-authored parameter bounds must be enforced");
    require(near(bounded.tap_level_db[0], 20.0F),
            "per-tap level must preserve the guide's -80 to +20 dB bounds");

    auto invalid = delay.parameters();
    invalid.delay_time_ms = std::numeric_limits<float>::quiet_NaN();
    invalid.tap_level_db[0] = std::numeric_limits<float>::infinity();
    delay.set_parameters(invalid);
    require(std::isfinite(delay.parameters().delay_time_ms) &&
                std::isfinite(delay.parameters().tap_level_db[0]),
            "non-finite controls must fall back to finite values");
}

void test_ten_tap_spacing_decay_and_stereo_ratio() {
    auto parameters = impulse_parameters();
    parameters.tap_count = 4;
    parameters.delay_time_ms = 10.0F;
    parameters.shuffle_percent = 100.0F;
    parameters.ratio_percent = 150.0F;
    parameters.spread_percent = 100.0F;
    parameters.pan_shape = tabletfx::TenTapPanShape::increasing;
    parameters.decay_percent = 0.0F;
    tabletfx::TenTapDelay delay(8000.0F, 2, 500.0F, parameters);
    require(delay.tap_delay_samples(0, 1) == 80U &&
                delay.tap_delay_samples(0, 0) == 120U,
            "right delay and project-authored left/right ratio must be applied");
    require(delay.tap_delay_samples(1, 1) == 200U &&
                delay.tap_delay_samples(2, 1) == 240U &&
                delay.tap_delay_samples(3, 1) == 360U,
            "shuffle must offset alternating taps by at most half an interval");
    require(near(delay.effective_tap_pan(0), 0.0F) &&
                near(delay.effective_tap_pan(1), 1.0F / 3.0F) &&
                near(delay.effective_tap_pan(2), 2.0F / 3.0F) &&
                near(delay.effective_tap_pan(3), 1.0F),
            "increasing pan shape must span the selected stereo spread");

    parameters.tap_count = 2;
    parameters.shuffle_percent = 0.0F;
    parameters.ratio_percent = 100.0F;
    parameters.decay_percent = 50.0F;
    parameters.tap_level_db[1] = -6.0F;
    delay.set_parameters(parameters);
    require(delay.tap_delay_samples(0, 1) == 80U &&
                delay.tap_delay_samples(1, 1) == 160U,
            "unshuffled taps must use increasing multiples of the base interval");
    require(near(delay.parameters().decay_percent, 50.0F) &&
                near(delay.parameters().tap_level_db[1], -6.0F),
            "decay and individual tap level must be independently adjustable");
}

void test_tempo_division_and_tap_count_change() {
    auto parameters = impulse_parameters();
    parameters.tap_count = 1;
    parameters.tempo_bpm = 120.0F;
    parameters.division = tabletfx::TenTapDivision::quarter;
    tabletfx::TenTapDelay delay(48000.0F, 2, 1000.0F, parameters);
    require(delay.tap_delay_samples(0, 1) == 24000U,
            "quarter-note tempo sync at 120 BPM must resolve to 500 ms");

    parameters.tap_count = 10;
    parameters.division = tabletfx::TenTapDivision::eighth_triplet;
    parameters.shuffle_percent = 0.0F;
    tabletfx::TenTapDelay ten_taps(48000.0F, 2, 2000.0F, parameters);
    require(ten_taps.tap_delay_samples(0, 1) == 8000U &&
                ten_taps.tap_delay_samples(9, 1) == 80000U,
            "tempo subdivisions must set a stable ten-tap interval");
}

void test_impulse_output_filters_mono_and_reset() {
    auto parameters = impulse_parameters();
    parameters.tap_count = 1;
    parameters.spread_percent = 0.0F;
    tabletfx::TenTapDelay delay(8000.0F, 2, 100.0F, parameters);
    std::array<float, 32> audio{};
    audio[0] = 1.0F;
    audio[1] = 1.0F;
    delay.process(audio, 2);
    require(audio[16] > 0.5F && audio[17] > 0.5F,
            "the mono-compatible first tap must emerge at the configured delay");
    require(audio[0] == 0.0F && audio[1] == 0.0F,
            "fully wet mode must not leak dry input at time zero");

    delay.reset();
    std::array<float, 32> silence{};
    delay.process(silence, 2);
    require(std::all_of(silence.begin(), silence.end(),
                        [](float sample) { return sample == 0.0F; }),
            "reset must clear delay and filter state");

    parameters.mono = true;
    parameters.low_cut_hz = 500.0F;
    parameters.high_cut_hz = 3000.0F;
    delay.set_parameters(parameters);
    std::array<float, 16> stereo{};
    stereo[0] = 1.0F;
    stereo[1] = -1.0F;
    delay.process(stereo, 2);
    require(std::all_of(stereo.begin(), stereo.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "mono mode and wet tone filters must retain finite output");
}

void test_rhythm_tap_custom_spacing_quantize_and_bounds() {
    tabletfx::RhythmTapDelayParameters parameters;
    parameters.common = impulse_parameters();
    parameters.common.tap_count = 4;
    parameters.common.decay_percent = 0.0F;
    parameters.common.spread_percent = 0.0F;
    parameters.quantize = tabletfx::TenTapDivision::free;
    parameters.feedback_percent = 0.0F;
    parameters.tap_time_values = {10.0F, 30.0F, 20.0F, 40.0F, 220.0F,
                                  220.0F, 220.0F, 220.0F, 220.0F, 220.0F};
    tabletfx::RhythmTapDelay delay(8000.0F, 1, parameters);
    require(delay.tap_delay_samples(0, 0) == 80U &&
                delay.tap_delay_samples(1, 0) == 320U &&
                delay.tap_delay_samples(2, 0) == 480U &&
                delay.tap_delay_samples(3, 0) == 800U,
            "free rhythm intervals must accumulate from the preceding repeat");
    require(delay.id() == "rhythm-tap-delay" &&
                delay.parameters().common.shuffle_percent == 0.0F &&
                delay.parameters().common.ratio_percent == 100.0F,
            "Rhythm Tap must remain distinct from Ten-Tap shuffle and stereo ratio controls");

    parameters.common.tempo_bpm = 300.0F;
    parameters.common.tap_count = 3;
    parameters.quantize = tabletfx::TenTapDivision::quarter;
    parameters.tap_time_values = {1.0F, 2.0F, 3.0F, 1.0F, 1.0F,
                                  1.0F, 1.0F, 1.0F, 1.0F, 1.0F};
    delay.set_parameters(parameters);
    require(delay.tap_delay_samples(0, 0) == 1600U &&
                delay.tap_delay_samples(1, 0) == 4800U &&
                delay.tap_delay_samples(2, 0) == 9600U,
            "quantized rhythm intervals must accumulate at the selected tempo");

    parameters.feedback_percent = 120.0F;
    parameters.common.tap_count = 0;
    parameters.tap_time_values[0] = 0.0F;
    parameters.tap_time_values[1] = 9000.0F;
    delay.set_parameters(parameters);
    const auto bounded = delay.parameters();
    require(bounded.feedback_percent == 95.0F &&
                bounded.common.tap_count == 1U &&
                bounded.tap_time_values[0] == 1.0F &&
                bounded.tap_time_values[1] == 2000.0F,
            "project-defined feedback, repeat-count, and interval bounds must be enforced");
}

void test_rhythm_tap_feedback_repeats_pattern_and_processing_allocates_nothing() {
    tabletfx::RhythmTapDelayParameters parameters;
    parameters.common = impulse_parameters();
    parameters.common.tap_count = 2;
    parameters.common.decay_percent = 0.0F;
    parameters.common.low_cut_hz = 0.0F;
    parameters.common.high_cut_hz = 20000.0F;
    parameters.common.mix = 1.0F;
    parameters.feedback_percent = 50.0F;
    parameters.tap_time_values.fill(1.0F);
    tabletfx::RhythmTapDelay delay(8000.0F, 1, parameters);
    std::array<float, 64> audio{};
    audio[0] = 1.0F;
    delay.process(audio, 1);
    require(audio[8] > 0.5F && audio[16] > 0.5F,
            "the input must produce the configured first rhythm pattern");
    require(audio[24] > 0.2F && audio[24] < 0.8F &&
                audio[40] > 0.08F && audio[40] < 0.45F,
            "feedback from the final repeat must regenerate the pattern with decay");

    delay.reset();
    std::array<float, 256> repeated{};
    repeated.fill(0.125F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        delay.process(repeated, 1);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "Rhythm Tap processing must not allocate");
}

void test_memory_cap_channel_mismatch_and_no_process_allocations() {
    tabletfx::TenTapDelay capped(384000.0F, 2, 2000.0F);
    require(capped.memory_bytes() <= sizeof(capped) +
                tabletfx::ten_tap_delay_storage_limit_bytes,
            "delay storage must stay under its fixed one-MiB cap");
    require(capped.maximum_delay_ms() < 400.0F,
            "high-rate stereo buffer must report its memory-limited maximum");
    tabletfx::RhythmTapDelay rhythm_capped(384000.0F, 2);
    require(rhythm_capped.memory_bytes() <= sizeof(rhythm_capped) +
                tabletfx::ten_tap_delay_storage_limit_bytes &&
                rhythm_capped.maximum_delay_ms() < 400.0F,
            "Rhythm Tap must honor the same high-rate stereo storage cap");

    bool rejected_layout = false;
    try {
        tabletfx::TenTapDelay invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        rejected_layout = true;
    }
    require(rejected_layout, "constructor must reject non-mono/stereo layouts");

    auto parameters = impulse_parameters();
    tabletfx::TenTapDelay delay(48000.0F, 2, 500.0F, parameters);
    std::array<float, 512> audio{};
    audio.fill(0.125F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        delay.process(audio, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "Ten-Tap processing must not allocate");

    std::array<float, 8> wrong_layout{1, 2, 3, 4, 5, 6, 7, 8};
    const auto before_mismatch = wrong_layout;
    delay.process(wrong_layout, 1);
    require(wrong_layout == before_mismatch,
            "channel mismatch must leave the buffer unchanged");
}

} // namespace

int main() {
    try {
        test_documented_tap_count_and_project_parameter_bounds();
        test_ten_tap_spacing_decay_and_stereo_ratio();
        test_tempo_division_and_tap_count_change();
        test_impulse_output_filters_mono_and_reset();
        test_rhythm_tap_custom_spacing_quantize_and_bounds();
        test_rhythm_tap_feedback_repeats_pattern_and_processing_allocates_nothing();
        test_memory_cap_channel_mismatch_and_no_process_allocations();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS: Fractal-family Ten-Tap and Rhythm Tap project-authored delay controls\n";
    return 0;
}
