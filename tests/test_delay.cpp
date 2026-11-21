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

void test_ping_pong_echoes_alternate_channels() {
    tabletfx::Delay delay(1000.0F, 2, 100.0F, 4.0F, 0.5F, 1.0F);
    delay.configure_ping_pong(true);
    require(delay.ping_pong_enabled(), "ping-pong mode did not retain its requested state");

    std::array<float, 20> settle{};
    delay.process(settle, 2);
    std::array<float, 28> impulse{};
    impulse[0] = 1.0F;
    delay.process(impulse, 2);
    require(near(impulse[9], 1.0F),
            "a left impulse's first ping-pong echo must appear on the right");
    require(near(impulse[16], 0.5F),
            "the second ping-pong echo must return to the left at the feedback level");
    require(near(impulse[25], 0.25F),
            "the third ping-pong echo must alternate back to the right");

    delay.reset();
    std::array<float, 12> after_reset{};
    after_reset[1] = 1.0F;
    delay.process(after_reset, 2);
    require(near(after_reset[8], 1.0F),
            "reset must clear old echoes while preserving the selected routing mode");
}

void test_ping_pong_mode_crossfade_and_mono_fallback() {
    tabletfx::Delay stereo(1000.0F, 2, 100.0F, 4.0F, 0.0F, 1.0F);
    std::array<float, 40> steady{};
    for (std::size_t frame = 0; frame < steady.size() / 2; ++frame) {
        steady[frame * 2] = 0.25F;
        steady[frame * 2 + 1] = -0.25F;
    }
    stereo.process(steady, 2);
    stereo.configure_ping_pong(true);
    std::array<float, 22> transition{};
    for (std::size_t frame = 0; frame < transition.size() / 2; ++frame) {
        transition[frame * 2] = 0.25F;
        transition[frame * 2 + 1] = -0.25F;
    }
    stereo.process(transition, 2);
    for (std::size_t frame = 1; frame < transition.size() / 2; ++frame) {
        require(std::abs(transition[frame * 2] - transition[(frame - 1) * 2]) < 0.06F &&
                    std::abs(transition[frame * 2 + 1] - transition[(frame - 1) * 2 + 1]) < 0.06F,
                "stereo routing change was not crossfaded smoothly over 10 ms");
    }
    require(transition[20] < -0.2F && transition[21] > 0.2F,
            "ping-pong routing crossfade did not reach the opposite-channel taps");

    tabletfx::Delay mono(1000.0F, 1, 100.0F, 2.0F, 0.5F, 1.0F);
    mono.configure_ping_pong(true);
    std::array<float, 10> mono_settle{};
    mono.process(mono_settle, 1);
    std::array<float, 8> mono_impulse{};
    mono_impulse[0] = 1.0F;
    mono.process(mono_impulse, 1);
    require(near(mono_impulse[2], 1.0F) && near(mono_impulse[4], 0.5F),
            "mono ping-pong configuration must safely retain same-channel echoes");
}

void test_independent_dual_times_and_feedback() {
    tabletfx::Delay stereo(8000.0F, 2, 100.0F, 4.0F, 0.0F, 0.0F, true);
    stereo.configure_dual(4.0F, 0.5F, 1.0F, 6.0F, 0.25F, 1.0F);
    require(stereo.dual_enabled(), "dual delay mode did not retain its requested state");
    stereo.reset();

    std::array<float, 210> impulses{};
    impulses[0] = 1.0F;
    impulses[3] = 0.5F;
    stereo.process(impulses, 2);
    require(near(impulses[64], 1.0F) && near(impulses[65], 0.0F),
            "Delay A must use its 4 ms time on the left without channel leakage");
    require(near(impulses[99], 0.5F) && near(impulses[98], 0.0F),
            "Delay B must use its independent 6 ms time on the right");
    require(near(impulses[128], 0.5F) && near(impulses[195], 0.125F),
            "the two independent feedback levels must produce separate repeats");

    tabletfx::Delay mono(8000.0F, 1, 100.0F, 4.0F, 0.0F, 0.0F, true);
    mono.configure_dual(4.0F, 0.0F, 1.0F, 6.0F, 0.0F, 1.0F);
    mono.reset();
    std::array<float, 56> mono_impulse{};
    mono_impulse[0] = 1.0F;
    mono.process(mono_impulse, 1);
    require(near(mono_impulse[32], 0.5F) && near(mono_impulse[48], 0.5F),
            "mono dual mode must mix both independently timed taps at half level");
}

void test_dual_routing_crossfade_and_no_allocation() {
    tabletfx::Delay delay(1000.0F, 2, 100.0F, 4.0F, 0.0F, 0.0F, true);
    std::array<float, 40> steady{};
    for (std::size_t frame = 0; frame < steady.size() / 2; ++frame) {
        steady[frame * 2] = 0.25F;
        steady[frame * 2 + 1] = -0.25F;
    }
    delay.process(steady, 2);
    delay.configure_dual(4.0F, 0.2F, 0.5F, 6.0F, 0.4F, 0.5F);
    std::array<float, 24> transition{};
    for (std::size_t frame = 0; frame < transition.size() / 2; ++frame) {
        transition[frame * 2] = 0.25F;
        transition[frame * 2 + 1] = -0.25F;
    }
    delay.process(transition, 2);
    for (std::size_t frame = 1; frame < transition.size() / 2; ++frame) {
        require(std::abs(transition[frame * 2] - transition[(frame - 1) * 2]) < 0.06F &&
                    std::abs(transition[frame * 2 + 1] - transition[(frame - 1) * 2 + 1]) < 0.06F,
                "switching to independent dual delay should crossfade without a jump");
    }

    tabletfx::Delay mono(48000.0F, 1, 100.0F, 20.0F, 0.5F, 0.5F, true);
    mono.configure_dual(120.0F, 0.25F, 0.25F, 240.0F, 0.4F, 0.3F);
    std::array<float, 512> mono_audio{};
    mono_audio.fill(0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) mono.process(mono_audio, 1);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "dual-delay processing allocated memory");
}

void test_multitap_levels_pan_feedback_and_no_allocation() {
    tabletfx::Delay delay(8000.0F, 2, 100.0F, 4.0F, 0.0F, 1.0F);
    delay.configure_multitap_tap(0, 4.0F, 1.0F, -1.0F);
    delay.configure_multitap_tap(1, 6.0F, 0.5F, 1.0F);
    delay.configure_multitap_tap(2, 8.0F, 0.0F, 0.0F);
    delay.configure_multitap_tap(3, 10.0F, 0.0F, 0.0F);
    delay.configure_multitap(true, 0.0F, 1.0F);
    delay.reset();

    std::array<float, 112> stereo_impulse{};
    stereo_impulse[0] = 1.0F;
    delay.process(stereo_impulse, 2);
    require(near(stereo_impulse[32 * 2], 0.5F) &&
                near(stereo_impulse[32 * 2 + 1], 0.0F),
            "tap A must use its hard-left pan and full level on a left impulse");
    require(near(stereo_impulse[48 * 2], 0.0F) &&
                near(stereo_impulse[48 * 2 + 1], 0.25F),
            "tap B must use its independent time, half level, and hard-right pan");

    tabletfx::Delay feedback(8000.0F, 1, 100.0F, 4.0F, 0.0F, 1.0F);
    feedback.configure_multitap_tap(0, 4.0F, 1.0F, -1.0F);
    feedback.configure_multitap_tap(1, 8.0F, 0.0F, 0.0F);
    feedback.configure_multitap_tap(2, 12.0F, 0.0F, 0.0F);
    feedback.configure_multitap_tap(3, 16.0F, 0.0F, 0.0F);
    feedback.configure_multitap(true, 0.5F, 1.0F);
    feedback.reset();
    std::array<float, 96> mono_impulse{};
    mono_impulse[0] = 1.0F;
    feedback.process(mono_impulse, 1);
    require(near(mono_impulse[32], 1.0F) && near(mono_impulse[64], 0.5F),
            "multi-tap feedback must repeat at the first tap's interval");

    std::array<float, 256> block{};
    block.fill(0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        feedback.configure_multitap((iteration & 1) == 0, 0.5F, 0.5F);
        feedback.configure_multitap_tap(static_cast<std::size_t>(iteration) %
                                            tabletfx::maximum_delay_taps,
                                        4.0F + static_cast<float>(iteration % 4),
                                        0.5F, -0.5F + 0.01F * static_cast<float>(iteration % 100));
        feedback.process(block, 1);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "multi-tap processing allocated memory");
}

void test_multitap_route_and_tap_changes_are_smoothed() {
    tabletfx::Delay delay(1000.0F, 2, 100.0F, 10.0F, 0.0F, 0.5F);
    delay.configure_multitap_tap(0, 10.0F, 1.0F, 0.0F);
    delay.configure_multitap_tap(1, 20.0F, 0.0F, 0.0F);
    delay.configure_multitap_tap(2, 30.0F, 0.0F, 0.0F);
    delay.configure_multitap_tap(3, 40.0F, 0.0F, 0.0F);
    std::array<float, 80> steady{};
    for (std::size_t frame = 0; frame < steady.size() / 2; ++frame) {
        steady[frame * 2] = 0.25F;
        steady[frame * 2 + 1] = 0.25F;
    }
    delay.process(steady, 2);

    delay.configure_multitap(true, 0.2F, 0.5F);
    std::array<float, 32> route_transition{};
    for (std::size_t frame = 0; frame < route_transition.size() / 2; ++frame) {
        route_transition[frame * 2] = 0.25F;
        route_transition[frame * 2 + 1] = 0.25F;
    }
    delay.process(route_transition, 2);
    for (std::size_t frame = 1; frame < route_transition.size() / 2; ++frame) {
        require(std::abs(route_transition[frame * 2] - route_transition[(frame - 1) * 2]) < 0.04F &&
                    std::abs(route_transition[frame * 2 + 1] -
                             route_transition[(frame - 1) * 2 + 1]) < 0.04F,
                "multi-tap mode entry must crossfade without an output jump");
    }

    delay.configure_multitap_tap(0, 20.0F, 0.4F, 1.0F);
    std::array<float, 32> tap_transition{};
    for (std::size_t frame = 0; frame < tap_transition.size() / 2; ++frame) {
        tap_transition[frame * 2] = 0.25F;
        tap_transition[frame * 2 + 1] = 0.25F;
    }
    delay.process(tap_transition, 2);
    for (std::size_t frame = 1; frame < tap_transition.size() / 2; ++frame) {
        require(std::abs(tap_transition[frame * 2] - tap_transition[(frame - 1) * 2]) < 0.04F &&
                    std::abs(tap_transition[frame * 2 + 1] -
                             tap_transition[(frame - 1) * 2 + 1]) < 0.04F,
                "tap time, level, and pan changes must ramp without an output jump");
    }
}

void test_multitap_controls_clamp_and_retarget_safely() {
    tabletfx::Delay delay(1000.0F, 1, 20.0F, 4.0F, 0.0F, 0.0F);
    delay.configure_multitap_tap(0, -1.0F, 2.0F, 2.0F);
    delay.configure_multitap_tap(1, std::numeric_limits<float>::quiet_NaN(),
                                 std::numeric_limits<float>::infinity(),
                                 -std::numeric_limits<float>::infinity());
    delay.configure_multitap_tap(tabletfx::maximum_delay_taps, 4.0F, 1.0F, 0.0F);
    delay.configure_multitap(true, 2.0F, 2.0F);
    require(near(delay.feedback(), 0.95F) && near(delay.mix(), 1.0F),
            "multi-tap master feedback and mix must retain safe limits");
    delay.reset();
    std::array<float, 8> impulse{};
    impulse[0] = 1.0F;
    delay.process(impulse, 1);
    require(near(impulse[1], 1.0F),
            "out-of-range mono tap time/level controls should clamp to a one-sample full-level tap");
    require(std::all_of(impulse.begin(), impulse.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "invalid multi-tap controls must not poison the output");
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
    tabletfx::Delay stereo(48000.0F, 2, 100.0F, 20.0F, 0.5F, 0.5F);
    stereo.configure_ping_pong(true);
    std::array<float, 512> stereo_audio{};
    stereo_audio.fill(0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) delay.process(audio, 1);
    for (int iteration = 0; iteration < 100; ++iteration) stereo.process(stereo_audio, 2);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "delay processing or ping-pong routing allocated memory");
}

} // namespace

int main() {
    try {
        test_tempo_divisions_and_ms_mode();
        test_tempo_bounds_division_fallback_and_delay_cap();
        test_delay_bounds_and_non_finite_controls_are_safe();
        test_stereo_impulses_feedback_and_no_extra_dry_latency();
        test_ping_pong_echoes_alternate_channels();
        test_ping_pong_mode_crossfade_and_mono_fallback();
        test_independent_dual_times_and_feedback();
        test_dual_routing_crossfade_and_no_allocation();
        test_multitap_levels_pan_feedback_and_no_allocation();
        test_multitap_route_and_tap_changes_are_smoothed();
        test_multitap_controls_clamp_and_retarget_safely();
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
