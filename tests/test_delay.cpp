#include "tabletfx/effects.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float left, float right, float tolerance = 1.0e-5F) {
    return std::abs(left - right) <= tolerance;
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

void test_tempo_divisions_and_ms_mode() {
    struct DivisionCase {
        tabletfx::DelayDivision division;
        float milliseconds;
        std::size_t samples;
    };
    constexpr std::array cases{
        DivisionCase{tabletfx::DelayDivision::whole, 2000.0F, 2000},
        DivisionCase{tabletfx::DelayDivision::dotted_half, 1500.0F, 1500},
        DivisionCase{tabletfx::DelayDivision::half, 1000.0F, 1000},
        DivisionCase{tabletfx::DelayDivision::dotted_quarter, 750.0F, 750},
        DivisionCase{tabletfx::DelayDivision::quarter, 500.0F, 500},
        DivisionCase{tabletfx::DelayDivision::quarter_triplet, 333.33334F, 333},
        DivisionCase{tabletfx::DelayDivision::dotted_eighth, 375.0F, 375},
        DivisionCase{tabletfx::DelayDivision::eighth, 250.0F, 250},
        DivisionCase{tabletfx::DelayDivision::eighth_triplet, 166.66667F, 167},
        DivisionCase{tabletfx::DelayDivision::dotted_sixteenth, 187.5F, 188},
        DivisionCase{tabletfx::DelayDivision::sixteenth, 125.0F, 125},
        DivisionCase{tabletfx::DelayDivision::sixteenth_triplet, 83.333336F, 83},
    };

    for (const auto& test : cases) {
        tabletfx::Delay delay(1000.0F, 1, 6000.0F, test.milliseconds, 0.0F, 1.0F);
        require(delay.time_mode() == tabletfx::DelayTimeMode::milliseconds,
                "constructor must retain the legacy millisecond mode");
        delay.configure_tempo(120.0F, test.division, 0.0F, 1.0F);
        require(delay.time_mode() == tabletfx::DelayTimeMode::tempo_sync,
                "tempo configure did not select tempo-sync mode");
        require(delay.division() == test.division,
                "tempo configure did not retain the selected note division");
        require(near(delay.tempo_bpm(), 120.0F), "tempo value changed unexpectedly");
        require(delay.delay_samples() == test.samples,
                "tempo division converted to the wrong sample delay");
        require(near(delay.delay_time_ms(), static_cast<float>(test.samples)),
                "reported delay time must reflect the sample-accurate rounded delay");
    }

    tabletfx::Delay legacy(1000.0F, 1, 1000.0F, 125.0F, 0.0F, 1.0F);
    require(legacy.delay_samples() == 125, "legacy millisecond mode changed sample position");
    legacy.configure(250.0F, 0.0F, 1.0F);
    require(legacy.time_mode() == tabletfx::DelayTimeMode::milliseconds &&
                legacy.delay_samples() == 250,
            "millisecond configure must return to free-time mode");
}

void test_tempo_bounds_division_fallback_and_delay_cap() {
    tabletfx::Delay delay(1000.0F, 1, 5000.0F, 500.0F, 0.25F, 0.25F);
    delay.configure_tempo(0.0F, tabletfx::DelayDivision::whole, 0.25F, 0.25F);
    require(near(delay.tempo_bpm(), 20.0F), "low BPM should clamp to 20");
    require(delay.delay_samples() == 5000,
            "long tempo subdivisions must clamp to the allocated maximum");
    delay.configure_tempo(1000.0F, tabletfx::DelayDivision::quarter, 0.25F, 0.25F);
    require(near(delay.tempo_bpm(), 300.0F), "high BPM should clamp to 300");
    require(delay.delay_samples() == 200, "high BPM quarter note conversion failed");

    delay.configure_tempo(std::numeric_limits<float>::quiet_NaN(),
                          static_cast<tabletfx::DelayDivision>(255),
                          std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity());
    require(near(delay.tempo_bpm(), 120.0F), "non-finite tempo should use 120 BPM default");
    require(delay.division() == tabletfx::DelayDivision::quarter,
            "invalid division should fall back to quarter note");
    require(std::isfinite(delay.delay_time_ms()), "invalid sync controls poisoned delay time");
}

void test_delay_bounds_and_non_finite_controls_are_safe() {
    tabletfx::Delay delay(48000.0F, 1, 0.01F, 0.0F, 0.25F, 0.25F);
    require(delay.delay_samples() == 1,
            "sub-millisecond delay allocation should still have one valid sample");
    delay.configure(std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::infinity());
    require(delay.delay_samples() == 1 && std::isfinite(delay.delay_time_ms()) &&
                std::isfinite(delay.feedback()) && std::isfinite(delay.mix()),
            "non-finite delay controls must preserve safe current settings");

    bool rejected_rate = false;
    try {
        tabletfx::Delay invalid(std::numeric_limits<float>::quiet_NaN(), 1);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    require(rejected_rate, "non-finite sample rate must be rejected before allocation");

    bool rejected_channels = false;
    try {
        tabletfx::Delay invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    require(rejected_channels, "the digital guitar delay should accept mono or stereo only");
}

void test_stereo_impulses_feedback_and_no_extra_dry_latency() {
    tabletfx::Delay delay(1000.0F, 2, 100.0F, 4.0F, 0.5F, 1.0F);
    std::array<float, 26> audio{};
    audio[0] = 1.0F;
    audio[1] = -0.5F;
    delay.process(audio, 2);
    require(audio[0] == 0.0F && audio[1] == 0.0F,
            "fully wet delay must not add a dry-path impulse");
    require(near(audio[8], 1.0F) && near(audio[9], -0.5F),
            "stereo first repeats appeared at the wrong frames or crossed channels");
    require(near(audio[16], 0.5F) && near(audio[17], -0.25F),
            "stereo feedback repeats have the wrong sign or level");
}

void test_tempo_change_crossfades_without_discontinuity() {
    tabletfx::Delay delay(1000.0F, 1, 1000.0F, 100.0F, 0.0F, 1.0F);
    std::array<float, 700> fill{};
    fill.fill(0.25F);
    delay.process(fill, 1);

    delay.configure_tempo(120.0F, tabletfx::DelayDivision::quarter, 0.0F, 1.0F);
    std::array<float, 16> transition{};
    transition.fill(0.25F);
    delay.process(transition, 1);
    for (float sample : transition) {
        require(near(sample, 0.25F), "delay-time crossfade produced a discontinuity on steady input");
    }
}

void test_parameter_ramps_are_block_partition_invariant_and_resettable() {
    tabletfx::Delay whole(1000.0F, 1, 1000.0F, 4.0F, 0.0F, 0.0F);
    tabletfx::Delay split(1000.0F, 1, 1000.0F, 4.0F, 0.0F, 0.0F);
    whole.configure(8.0F, 0.65F, 0.9F);
    split.configure(8.0F, 0.65F, 0.9F);
    std::vector<float> input(997);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = std::sin(static_cast<float>(i) * 0.031F) * 0.4F;
    }
    auto split_input = input;
    whole.process(input, 1);
    constexpr std::array<std::size_t, 6> blocks{1, 127, 53, 300, 17, 211};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < split_input.size()) {
        const std::size_t count = std::min(blocks[block % blocks.size()],
                                           split_input.size() - offset);
        split.process(std::span<float>(split_input).subspan(offset, count), 1);
        offset += count;
        ++block;
    }
    for (std::size_t i = 0; i < input.size(); ++i) {
        require(near(input[i], split_input[i]),
                "delay control ramps depend on callback block partitioning");
    }

    whole.configure_tempo(90.0F, tabletfx::DelayDivision::dotted_eighth, 0.3F, 0.7F);
    whole.reset();
    tabletfx::Delay fresh(1000.0F, 1, 1000.0F, 500.0F, 0.3F, 0.7F);
    fresh.configure_tempo(90.0F, tabletfx::DelayDivision::dotted_eighth, 0.3F, 0.7F);
    fresh.reset();
    require(whole.delay_samples() == fresh.delay_samples() &&
                whole.time_mode() == fresh.time_mode() &&
                near(whole.feedback(), fresh.feedback()) && near(whole.mix(), fresh.mix()),
            "reset must settle pending tempo/parameter changes to their selected targets");
}

void test_layout_invalid_audio_and_process_allocation() {
    tabletfx::Delay delay(1000.0F, 1, 10.0F, 2.0F, 0.95F, 0.5F);
    std::array<float, 2> untouched{0.25F, -0.5F};
    delay.process(untouched, 0);
    require(untouched == std::array<float, 2>{0.25F, -0.5F},
            "zero-channel processing must leave samples unchanged");
    delay.process(untouched, 2);
    require(untouched == std::array<float, 2>{0.25F, -0.5F},
            "mismatched channel layout must leave samples unchanged");

    std::array<float, 5> partial{0.25F, 0.5F, 0.75F, -0.25F, 9.0F};
    delay.process(partial, 2);
    require(partial.back() == 9.0F, "partial trailing frame must remain untouched");

    std::array<float, 4> invalid{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::max()};
    delay.process(invalid, 1);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float value) { return std::isfinite(value); }),
            "invalid/extreme samples poisoned delay output or feedback state");

    std::array<float, 512> audio{};
    audio.fill(0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) delay.process(audio, 1);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "delay processing allocated memory");
}

} // namespace

int main() {
    try {
        test_tempo_divisions_and_ms_mode();
        test_tempo_bounds_division_fallback_and_delay_cap();
        test_delay_bounds_and_non_finite_controls_are_safe();
        test_stereo_impulses_feedback_and_no_extra_dry_latency();
        test_tempo_change_crossfades_without_discontinuity();
        test_parameter_ramps_are_block_partition_invariant_and_resettable();
        test_layout_invalid_audio_and_process_allocation();
        std::cout << "All digital delay tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Digital delay test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
