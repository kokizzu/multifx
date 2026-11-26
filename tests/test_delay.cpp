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

void test_trails_runtime_toggle_is_safe_and_allocates_nothing() {
    tabletfx::Delay delay(1000.0F, 1, 20.0F, 4.0F, 0.5F, 1.0F);
    delay.configure_trails_on_bypass(true);
    std::array<float, 4> impulse{1.0F, 0.0F, 0.0F, 0.0F};
    delay.process(impulse, 1);

    std::array<float, 8> bypass{};
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    delay.process_bypassed(bypass, 1);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "delay tail draining allocated memory");
    require(bypass[0] > 0.9F && bypass[4] > 0.4F,
            "enabled delay trails must drain the stored feedback tail");

    delay.configure_trails_on_bypass(false);
    std::array<float, 8> cleared{};
    delay.process_bypassed(cleared, 1);
    require(std::all_of(cleared.begin(), cleared.end(),
                        [](float sample) { return sample == 0.0F; }),
            "turning Trails off while bypassed must clear pending echoes");

    delay.configure_trails_on_bypass(true);
    std::array<float, 8> dry{};
    dry.fill(0.2F);
    delay.process_bypassed(dry, 1);
    require(std::all_of(dry.begin(), dry.end(),
                        [](float sample) { return std::abs(sample - 0.2F) < 1.0e-6F; }),
            "enabled delay trails must preserve dry bypass input when no echoes remain");
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
        DivisionCase{tabletfx::DelayDivision::thirty_second, 62.5F, 63},
        DivisionCase{tabletfx::DelayDivision::sixty_fourth, 31.25F, 31},
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

void test_bbd_style_colors_echoes_without_allocating() {
    tabletfx::Delay digital(48000.0F, 1, 2.0F, 1.0F, 0.0F, 1.0F);
    tabletfx::Delay bbd(48000.0F, 1, 2.0F, 1.0F, 0.0F, 1.0F);
    bbd.configure_bbd_style(true);
    bbd.reset();

    std::array<float, 128> digital_impulse{};
    std::array<float, 128> bbd_impulse{};
    digital_impulse[0] = 1.0F;
    bbd_impulse[0] = 1.0F;
    digital.process(digital_impulse, 1);
    const std::size_t allocations_before = allocation_count.load(std::memory_order_relaxed);
    bbd.process(bbd_impulse, 1);
    const std::size_t allocations_after = allocation_count.load(std::memory_order_relaxed);
    require(allocations_after == allocations_before,
            "BBD-style delay processing must not allocate");

    require(near(digital_impulse[48], 1.0F),
            "digital mode must retain its single-sample impulse echo");
    require(bbd_impulse[48] > 0.40F && bbd_impulse[48] < 0.46F,
            "BBD-style mode must soften the first impulse echo with its bandwidth limit");
    require(bbd_impulse[49] > 0.20F && bbd_impulse[49] < bbd_impulse[48],
            "BBD-style mode must have a decaying, bandwidth-limited impulse tail");
    for (const float sample : bbd_impulse) {
        require(std::isfinite(sample) && std::abs(sample) <= 1.0F,
                "BBD-style delay must keep finite bounded output for a full-scale impulse");
    }

    tabletfx::Delay stereo(48000.0F, 2, 2.0F, 1.0F, 0.2F, 1.0F);
    stereo.configure_bbd_style(true);
    stereo.reset();
    std::array<float, 256> stereo_impulse{};
    stereo_impulse[0] = 1.0F;
    const std::size_t stereo_allocations_before =
        allocation_count.load(std::memory_order_relaxed);
    stereo.process(stereo_impulse, 2);
    const std::size_t stereo_allocations_after =
        allocation_count.load(std::memory_order_relaxed);
    require(stereo_allocations_after == stereo_allocations_before,
            "stereo BBD-style delay processing must not allocate");
    require(stereo_impulse[48 * 2] > 0.40F &&
                std::abs(stereo_impulse[48 * 2 + 1]) < 1.0e-7F,
            "stereo BBD-style delay must preserve independent left/right lines");
    require(std::all_of(stereo_impulse.begin(), stereo_impulse.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "stereo BBD-style delay must remain finite through feedback repeats");
}

void test_tape_echo_modulation_saturation_and_no_allocation() {
    constexpr std::size_t frames = 2048;
    tabletfx::Delay digital(48000.0F, 2, 100.0F, 10.0F, 0.0F, 1.0F);
    tabletfx::Delay tape(48000.0F, 2, 100.0F, 10.0F, 0.0F, 1.0F);
    tape.configure_tape_echo(true, 0.0F, 0.8F, 0.35F, 0.12F, 7.0F);
    tape.reset();
    std::array<float, frames * 2> digital_audio{};
    std::array<float, frames * 2> tape_audio{};
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float sample = 0.4F * std::sin(
            6.2831853071795864769F * 220.0F * static_cast<float>(frame) / 48000.0F);
        digital_audio[frame * 2] = sample;
        digital_audio[frame * 2 + 1] = sample;
        tape_audio[frame * 2] = sample;
        tape_audio[frame * 2 + 1] = sample;
    }
    digital.process(digital_audio, 2);
    const std::size_t allocations_before = allocation_count.load(std::memory_order_relaxed);
    tape.process(tape_audio, 2);
    const std::size_t allocations_after = allocation_count.load(std::memory_order_relaxed);
    require(allocations_after == allocations_before,
            "tape echo modulation and saturation must not allocate");
    double difference_energy = 0.0;
    double reference_energy = 0.0;
    for (std::size_t frame = 480; frame < frames; ++frame) {
        const float processed = tape_audio[frame * 2];
        const float difference = processed - digital_audio[frame * 2];
        difference_energy += static_cast<double>(difference) * difference;
        reference_energy += static_cast<double>(digital_audio[frame * 2]) *
                            digital_audio[frame * 2];
        require(std::isfinite(processed), "tape echo must keep finite stereo output");
        require(near(tape_audio[frame * 2], tape_audio[frame * 2 + 1], 1.0e-6F),
                "tape wow/flutter modulation must remain linked across stereo channels");
    }
    const double relative_difference = std::sqrt(difference_energy / reference_energy);
    require(relative_difference > 0.02,
            "tape wow/flutter must audibly modulate a sustained sinusoid");

    tabletfx::Delay saturation(48000.0F, 1, 5.0F, 2.0F, 0.0F, 1.0F);
    saturation.configure_tape_echo(true, 0.45F, 0.0F, 0.35F, 0.0F, 7.0F);
    saturation.reset();
    std::array<float, 256> impulse{};
    impulse[0] = 1.0F;
    saturation.process(impulse, 1);
    const float first_echo = impulse[96];
    require(first_echo > 0.76F && first_echo < 0.79F,
            "tape saturation control must shape the first echo predictably");
}

void test_bbd_mod_mode_changes_repeat_saturation_without_allocating() {
    constexpr std::size_t frames = 1200;
    constexpr std::size_t delay_frames = 240;
    tabletfx::Delay classic(48000.0F, 1, 20.0F, 5.0F, 0.82F, 1.0F);
    tabletfx::Delay mod(48000.0F, 1, 20.0F, 5.0F, 0.82F, 1.0F);
    classic.configure_bbd_style(true);
    mod.configure_bbd_style(true);
    mod.configure_bbd_mode(true);
    classic.reset();
    mod.reset();

    std::array<float, frames> classic_audio{};
    std::array<float, frames> mod_audio{};
    classic_audio[0] = 0.95F;
    mod_audio[0] = 0.95F;
    classic.process(classic_audio, 1);
    const std::size_t allocations_before = allocation_count.load(std::memory_order_relaxed);
    mod.process(mod_audio, 1);
    const std::size_t allocations_after = allocation_count.load(std::memory_order_relaxed);
    require(allocations_after == allocations_before,
            "BBD MOD repeat processing must not allocate");

    double difference_energy = 0.0;
    for (std::size_t i = 0; i < frames; ++i) {
        require(std::isfinite(mod_audio[i]), "BBD MOD mode produced non-finite output");
        const double difference = static_cast<double>(mod_audio[i] - classic_audio[i]);
        difference_energy += difference * difference;
    }
    require(difference_energy > 1.0e-6,
            "BBD MOD mode did not change the project repeat coloration");

    const auto peak = [](const auto& samples, std::size_t begin,
                         std::size_t end) noexcept {
        float result = 0.0F;
        for (std::size_t i = begin; i < end; ++i) {
            result = std::max(result, std::abs(samples[i]));
        }
        return result;
    };
    const float classic_late = peak(classic_audio, delay_frames * 3U,
                                   delay_frames * 4U);
    const float mod_late = peak(mod_audio, delay_frames * 3U,
                                delay_frames * 4U);
    require(mod_late > classic_late,
            "BBD MOD's gentler repeat saturation should retain more late-repeat level");
}

void test_bbd_mod_repeat_gain_match_improves_feedback_linearity() {
    constexpr std::size_t frames = 1440;
    constexpr std::size_t delay_frames = 240;
    constexpr float feedback = 0.82F;
    tabletfx::Delay classic(48000.0F, 1, 20.0F, 5.0F, feedback, 1.0F);
    tabletfx::Delay mod(48000.0F, 1, 20.0F, 5.0F, feedback, 1.0F);
    classic.configure_bbd_style(true);
    mod.configure_bbd_style(true);
    mod.configure_bbd_mode(true);
    classic.reset();
    mod.reset();

    std::array<float, frames> classic_audio{};
    std::array<float, frames> mod_audio{};
    classic_audio[0] = 0.5F;
    mod_audio[0] = 0.5F;
    classic.process(classic_audio, 1);
    mod.process(mod_audio, 1);

    const auto peak = [](const auto& samples, std::size_t begin,
                         std::size_t end) noexcept {
        float result = 0.0F;
        for (std::size_t i = begin; i < end; ++i) {
            result = std::max(result, std::abs(samples[i]));
        }
        return result;
    };
    const float classic_first = peak(classic_audio, delay_frames,
                                     delay_frames * 2U);
    const float classic_second = peak(classic_audio, delay_frames * 2U,
                                      delay_frames * 3U);
    const float mod_first = peak(mod_audio, delay_frames,
                                 delay_frames * 2U);
    const float mod_second = peak(mod_audio, delay_frames * 2U,
                                  delay_frames * 3U);
    require(classic_first > 0.0F && classic_second > 0.0F &&
                mod_first > 0.0F && mod_second > 0.0F,
            "BBD feedback test must produce two measurable repeats");

    const float classic_ratio = classic_second / classic_first;
    const float mod_ratio = mod_second / mod_first;
    require(std::abs(mod_ratio - feedback) < std::abs(classic_ratio - feedback),
            "MOD repeat decay should track the requested feedback more closely");
    require(mod_first > classic_first,
            "MOD repeat gain matching should reduce the typical-level loss");
}

void test_tape_echo_character_changes_are_smoothed() {
    tabletfx::Delay delay(48000.0F, 1, 50.0F, 8.0F, 0.0F, 1.0F);
    std::array<float, 512> steady{};
    steady.fill(0.2F);
    delay.process(steady, 1);

    delay.configure_tape_echo(true, 0.8F, 0.0F, 0.35F, 0.0F, 6.0F);
    std::array<float, 1024> entering{};
    entering.fill(0.2F);
    delay.process(entering, 1);
    for (std::size_t frame = 1; frame < entering.size(); ++frame) {
        require(std::abs(entering[frame] - entering[frame - 1]) < 0.005F,
                "tape saturation entry must not make a sample discontinuity");
    }
    require(entering.back() < 0.195F,
            "tape saturation entry did not reach its requested feedback coloration");

    delay.configure_tape_echo(false, 0.8F, 0.0F, 0.35F, 0.0F, 6.0F);
    std::array<float, 1024> exiting{};
    exiting.fill(0.2F);
    delay.process(exiting, 1);
    for (std::size_t frame = 1; frame < exiting.size(); ++frame) {
        require(std::abs(exiting[frame] - exiting[frame - 1]) < 0.005F,
                "tape saturation exit must not make a sample discontinuity");
    }
    require(near(exiting.back(), 0.2F, 0.001F),
            "tape saturation exit did not return to the clean digital echo");
}

void test_tape_heads_select_spacing_and_mono_combinations() {
    tabletfx::Delay delay(1000.0F, 1, 40.0F, 4.0F, 0.0F, 1.0F);
    delay.configure_tape_heads(true, 4.0F, 0b0101);
    delay.reset();

    std::array<float, 18> impulse{};
    impulse[0] = 1.0F;
    delay.process(impulse, 1);
    require(near(impulse[4], 0.5F) && near(impulse[12], 0.5F),
            "selected tape heads must echo at their 1x and 3x spacing with normalized level");
    require(near(impulse[8], 0.0F) && near(impulse[16], 0.0F),
            "unselected tape heads must not produce echoes");
}

void test_tape_heads_stereo_timing_and_alternating_pan() {
    tabletfx::Delay delay(1000.0F, 2, 40.0F, 4.0F, 0.0F, 1.0F);
    std::array<float, 4> settle{};
    delay.process(settle, 2);
    delay.configure_tape_heads(true, 4.0F, 0b0001);
    delay.reset();

    std::array<float, 12> left_head{};
    left_head[0] = 1.0F;
    delay.process(left_head, 2);
    require(near(left_head[8], 0.5F * std::sqrt(0.775F), 1.0e-4F) &&
                near(left_head[9], 0.5F * std::sqrt(0.225F), 1.0e-4F),
            "the first selected head must echo at 4 ms with its documented stereo pan");

    delay.configure_tape_heads(true, 4.0F, 0b0010);
    delay.reset();
    std::array<float, 20> right_head{};
    right_head[0] = 1.0F;
    delay.process(right_head, 2);
    require(near(right_head[16], 0.5F * std::sqrt(0.225F), 1.0e-4F) &&
                near(right_head[17], 0.5F * std::sqrt(0.775F), 1.0e-4F),
            "the second selected head must echo at 8 ms with the opposite stereo pan");
}

void test_tape_heads_bound_controls_reset_and_allocate_nothing() {
    tabletfx::Delay delay(1000.0F, 1, 12.0F, 4.0F, 0.0F, 1.0F);
    delay.configure_tape_heads(true, std::numeric_limits<float>::infinity(), 0xFFU);
    delay.reset();

    std::array<float, 30> impulse{};
    impulse[0] = 1.0F;
    delay.process(impulse, 1);
    for (std::size_t frame : {3U, 6U, 9U, 12U}) {
        require(near(impulse[frame], 0.25F),
                "oversized head spacing must remain bounded by the allocated delay ring");
    }
    for (std::size_t frame : {1U, 2U, 4U, 5U, 7U, 8U, 10U, 11U}) {
        require(near(impulse[frame], 0.0F),
                "bounded tape-head times must not alias intermediate samples");
    }

    delay.reset();
    std::array<float, 24> after_reset{};
    delay.process(after_reset, 1);
    require(std::all_of(after_reset.begin(), after_reset.end(),
                        [](float sample) { return near(sample, 0.0F); }),
            "reset must clear tape-head echoes while retaining safe configuration");

    std::array<float, 512> audio{};
    audio.fill(0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) delay.process(audio, 1);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "tape-head processing must not allocate");

    delay.configure_tape_heads(true, 4.0F, 0U);
    delay.reset();
    std::array<float, 8> no_heads{};
    no_heads[0] = 1.0F;
    delay.process(no_heads, 1);
    require(near(no_heads[4], 1.0F),
            "an empty head mask must fall back to the existing single-delay route");
}

void test_tape_head_reconfiguration_is_smoothed() {
    tabletfx::Delay delay(1000.0F, 1, 40.0F, 4.0F, 0.0F, 1.0F);
    delay.configure_tape_heads(true, 4.0F, 0b0001);
    std::array<float, 100> steady{};
    steady.fill(0.25F);
    delay.process(steady, 1);

    delay.configure_tape_heads(true, 5.0F, 0b1010);
    std::array<float, 24> transition{};
    transition.fill(0.25F);
    delay.process(transition, 1);
    for (std::size_t frame = 1; frame < transition.size(); ++frame) {
        require(std::abs(transition[frame] - transition[frame - 1]) < 0.04F,
                "changing selected heads and spacing must ramp without an abrupt level jump");
    }
    require(near(transition.back(), 0.25F, 0.01F),
            "tape-head transition did not reach the normalized selected-head level");
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

void test_repeat_filters_shape_delay_path_and_preserve_dry_audio() {
    const auto capture_echoes = [](bool enabled, float low_cut_hz,
                                   float high_cut_hz) {
        tabletfx::Delay delay(48000.0F, 1, 10.0F, 1.0F, 0.95F, 1.0F);
        if (enabled) {
            delay.configure_repeat_filters(true, low_cut_hz, high_cut_hz);
        }
        std::array<float, 512> settle{};
        delay.process(settle, 1);

        std::array<float, 145> impulse{};
        impulse[0] = 1.0F;
        delay.process(impulse, 1);
        return impulse;
    };

    const auto unfiltered = capture_echoes(false, 120.0F, 6000.0F);
    const auto low_cut = capture_echoes(true, 1000.0F, 20000.0F);
    const auto high_cut = capture_echoes(true, 20.0F, 2000.0F);
    require(near(unfiltered[48], 1.0F),
            "unfiltered delay must pass its first echo at the selected delay time");
    require(low_cut[48] < unfiltered[48] * 0.95F,
            "low-cut control did not shape the first delayed signal");
    require(high_cut[48] < unfiltered[48] * 0.3F,
            "high-cut control did not shape the first delayed signal");
    require(low_cut[96] < unfiltered[96] * 0.95F,
            "low-cut control did not shape the feedback repeats");
    require(high_cut[96] < unfiltered[96] * 0.2F,
            "high-cut control did not shape the feedback repeats");

    tabletfx::Delay dry_unfiltered(48000.0F, 1, 10.0F, 1.0F, 0.95F, 0.0F);
    tabletfx::Delay dry_filtered(48000.0F, 1, 10.0F, 1.0F, 0.95F, 0.0F);
    dry_filtered.configure_repeat_filters(true, 1000.0F, 2000.0F);
    std::array<float, 512> dry_settle_unfiltered{};
    std::array<float, 512> dry_settle_filtered{};
    dry_unfiltered.process(dry_settle_unfiltered, 1);
    dry_filtered.process(dry_settle_filtered, 1);
    std::array<float, 96> dry_input_unfiltered{};
    std::array<float, 96> dry_input_filtered{};
    dry_input_unfiltered[0] = 1.0F;
    dry_input_filtered[0] = 1.0F;
    dry_unfiltered.process(dry_input_unfiltered, 1);
    dry_filtered.process(dry_input_filtered, 1);
    require(dry_input_unfiltered == dry_input_filtered &&
                dry_input_filtered[0] == 1.0F,
            "repeat filters must leave the direct path unchanged");
}

void test_repeat_filters_bypass_stereo_bounds_reset_and_allocations() {
    tabletfx::Delay defaults(48000.0F, 1, 10.0F, 1.0F, 0.8F, 1.0F);
    tabletfx::Delay bypassed(48000.0F, 1, 10.0F, 1.0F, 0.8F, 1.0F);
    bypassed.configure_repeat_filters(false, 1000.0F, 500.0F);
    std::array<float, 512> settle_defaults{};
    std::array<float, 512> settle_bypassed{};
    defaults.process(settle_defaults, 1);
    bypassed.process(settle_bypassed, 1);
    std::array<float, 145> default_impulse{};
    std::array<float, 145> bypass_impulse{};
    default_impulse[0] = 1.0F;
    bypass_impulse[0] = 1.0F;
    defaults.process(default_impulse, 1);
    bypassed.process(bypass_impulse, 1);
    require(default_impulse == bypass_impulse,
            "disabled repeat filters must preserve the default delay output");

    tabletfx::Delay stereo(48000.0F, 2, 10.0F, 1.0F, 0.8F, 1.0F);
    stereo.configure_repeat_filters(true, 400.0F, 8000.0F);
    std::array<float, 1024> stereo_settle{};
    stereo.process(stereo_settle, 2);
    std::array<float, 290> stereo_impulse{};
    stereo_impulse[0] = 1.0F;
    stereo.process(stereo_impulse, 2);
    for (std::size_t frame = 0; frame < stereo_impulse.size() / 2; ++frame) {
        require(stereo_impulse[frame * 2 + 1] == 0.0F,
                "repeat filter state crossed between stereo channels");
    }

    const auto capture_bounded = [](float low_cut_hz, float high_cut_hz) {
        tabletfx::Delay delay(48000.0F, 1, 10.0F, 1.0F, 0.95F, 1.0F);
        delay.configure_repeat_filters(true, low_cut_hz, high_cut_hz);
        std::array<float, 512> settle{};
        delay.process(settle, 1);
        std::array<float, 145> impulse{};
        impulse[0] = 1.0F;
        delay.process(impulse, 1);
        return impulse;
    };
    const auto clamped = capture_bounded(-100000.0F, 100000.0F);
    const auto explicit_bounds = capture_bounded(20.0F, 20000.0F);
    require(clamped == explicit_bounds,
            "finite cut controls outside project bounds must clamp deterministically");
    const auto fallbacks = capture_bounded(
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity());
    const auto explicit_fallbacks = capture_bounded(120.0F, 6000.0F);
    require(fallbacks == explicit_fallbacks,
            "non-finite cut controls must use the documented project defaults");

    tabletfx::Delay extreme(48000.0F, 1, 10.0F, 1.0F, 0.95F, 1.0F);
    extreme.configure_repeat_filters(true, -std::numeric_limits<float>::max(),
                                     std::numeric_limits<float>::max());
    std::array<float, 64> extreme_audio{};
    extreme_audio.fill(std::numeric_limits<float>::max());
    extreme.process(extreme_audio, 1);
    require(std::all_of(extreme_audio.begin(), extreme_audio.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "bounded repeat filters must keep extreme finite input from poisoning audio");

    tabletfx::Delay resettable(48000.0F, 1, 10.0F, 1.0F, 0.8F, 1.0F);
    resettable.configure_repeat_filters(true, 400.0F, 8000.0F);
    std::array<float, 512> prime{};
    prime.fill(0.25F);
    resettable.process(prime, 1);
    resettable.reset();
    tabletfx::Delay fresh(48000.0F, 1, 10.0F, 1.0F, 0.8F, 1.0F);
    fresh.configure_repeat_filters(true, 400.0F, 8000.0F);
    fresh.reset();
    std::array<float, 145> reset_audio{};
    std::array<float, 145> fresh_audio{};
    reset_audio[0] = 1.0F;
    fresh_audio[0] = 1.0F;
    resettable.process(reset_audio, 1);
    fresh.process(fresh_audio, 1);
    require(reset_audio == fresh_audio,
            "reset must clear repeat-filter history while preserving selected controls");

    std::array<float, 256> allocation_audio{};
    allocation_audio.fill(0.1F);
    tabletfx::Delay allocation_check(48000.0F, 1, 10.0F, 1.0F, 0.8F, 0.5F);
    allocation_check.configure_repeat_filters(true, 400.0F, 8000.0F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        allocation_check.process(allocation_audio, 1);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "repeat-filtered delay processing allocated memory");
}

void test_trails_on_bypass_default_off_clears_and_preserves_dry_audio() {
    tabletfx::Delay delay(1000.0F, 1, 100.0F, 4.0F, 0.5F, 1.0F);
    std::array<float, 1> impulse{1.0F};
    delay.process(impulse, 1);

    std::array<float, 8> bypassed{};
    bypassed.fill(0.25F);
    const auto expected_dry = bypassed;
    const std::size_t allocations_before = allocation_count.load(std::memory_order_relaxed);
    delay.process_bypassed(bypassed, 1);
    const std::size_t allocations_after = allocation_count.load(std::memory_order_relaxed);
    require(allocations_after == allocations_before,
            "default-off bypass handling must not allocate");
    require(bypassed == expected_dry,
            "default-off bypass must be an identity path for the dry signal");

    delay.configure_trails_on_bypass(true);
    std::array<float, 12> reenabled{};
    delay.process(reenabled, 1);
    require(std::all_of(reenabled.begin(), reenabled.end(),
                        [](float sample) { return near(sample, 0.0F); }),
            "default-off bypass must clear stored echoes before re-enable");
}

void test_trails_on_bypass_drains_wet_without_exciting_delay() {
    tabletfx::Delay with_dry(1000.0F, 1, 100.0F, 4.0F, 0.5F, 1.0F);
    tabletfx::Delay without_dry(1000.0F, 1, 100.0F, 4.0F, 0.5F, 1.0F);
    with_dry.configure_trails_on_bypass(true);
    without_dry.configure_trails_on_bypass(true);
    std::array<float, 1> dry_impulse{1.0F};
    std::array<float, 1> silent_impulse{1.0F};
    with_dry.process(dry_impulse, 1);
    without_dry.process(silent_impulse, 1);

    std::array<float, 16> bypass_dry{};
    bypass_dry.fill(0.25F);
    std::array<float, 16> bypass_silence{};
    with_dry.process_bypassed(bypass_dry, 1);
    without_dry.process_bypassed(bypass_silence, 1);

    require(near(bypass_silence[3], 1.0F) &&
                near(bypass_silence[7], 0.5F) &&
                near(bypass_silence[11], 0.25F),
            "enabled bypass must drain the stored delay tail with feedback decay");
    for (std::size_t frame = 0; frame < bypass_dry.size(); ++frame) {
        require(near(bypass_dry[frame] - bypass_silence[frame], 0.25F),
                "bypassed delay must preserve dry audio without writing it into the delay line");
    }

    tabletfx::Delay whole(1000.0F, 1, 100.0F, 4.0F, 0.5F, 1.0F);
    tabletfx::Delay split(1000.0F, 1, 100.0F, 4.0F, 0.5F, 1.0F);
    whole.configure_trails_on_bypass(true);
    split.configure_trails_on_bypass(true);
    std::array<float, 1> whole_impulse{1.0F};
    std::array<float, 1> split_impulse{1.0F};
    whole.process(whole_impulse, 1);
    split.process(split_impulse, 1);
    std::array<float, 24> whole_bypass{};
    std::array<float, 24> split_bypass{};
    whole.process_bypassed(whole_bypass, 1);
    split.process_bypassed(std::span<float>(split_bypass).first(1), 1);
    split.process_bypassed(std::span<float>(split_bypass).subspan(1, 7), 1);
    split.process_bypassed(std::span<float>(split_bypass).subspan(8), 1);
    require(whole_bypass == split_bypass,
            "delay tail drain must be independent of bypass callback block partitioning");

    tabletfx::Delay stable(1000.0F, 1, 10.0F, 5.0F, 0.95F, 1.0F);
    stable.configure_trails_on_bypass(true);
    std::array<float, 1> stable_impulse{1.0F};
    stable.process(stable_impulse, 1);
    std::array<float, 4096> long_tail{};
    const std::size_t allocations_before = allocation_count.load(std::memory_order_relaxed);
    stable.process_bypassed(long_tail, 1);
    const std::size_t allocations_after = allocation_count.load(std::memory_order_relaxed);
    require(allocations_after == allocations_before,
            "enabled bypass tail draining must not allocate");
    require(std::all_of(long_tail.begin(), long_tail.end(), [](float sample) {
                return std::isfinite(sample) && std::abs(sample) <= 1.0F;
            }),
            "delay tail feedback must remain finite and bounded while draining");
    require(std::abs(long_tail.back()) < 1.0e-8F,
            "high-feedback tail must decay toward silence during bypass");
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
        test_bbd_style_colors_echoes_without_allocating();
        test_bbd_mod_mode_changes_repeat_saturation_without_allocating();
    test_bbd_mod_repeat_gain_match_improves_feedback_linearity();
        test_tape_echo_modulation_saturation_and_no_allocation();
        test_tape_echo_character_changes_are_smoothed();
        test_tape_heads_select_spacing_and_mono_combinations();
        test_tape_heads_stereo_timing_and_alternating_pan();
        test_tape_heads_bound_controls_reset_and_allocate_nothing();
        test_tape_head_reconfiguration_is_smoothed();
        test_tempo_change_crossfades_without_discontinuity();
        test_parameter_ramps_are_block_partition_invariant_and_resettable();
        test_repeat_filters_shape_delay_path_and_preserve_dry_audio();
        test_repeat_filters_bypass_stereo_bounds_reset_and_allocations();
        test_trails_on_bypass_default_off_clears_and_preserves_dry_audio();
        test_trails_on_bypass_drains_wet_without_exciting_delay();
        test_layout_invalid_audio_and_process_allocation();
        test_trails_runtime_toggle_is_safe_and_allocates_nothing();
        std::cout << "All digital delay tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Digital delay test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
