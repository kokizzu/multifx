#include "tabletfx/effects/auto_swell_delay.hpp"

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

std::atomic<bool> watch_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

tabletfx::AutoSwellDelayParameters basic_parameters() {
    tabletfx::AutoSwellDelayParameters parameters;
    parameters.time_ms = 4.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.time_mode = tabletfx::DelayTimeMode::milliseconds;
    parameters.tap_division = tabletfx::DelayDivision::quarter;
    parameters.tempo_bpm = 120.0F;
    parameters.swell_ms = 1.0F;
    parameters.mod = 0.0F;
    parameters.tone = 1.0F;
    parameters.sense_db = -36.0F;
    parameters.taper = tabletfx::AutoSwellDelayTaper::normal;
    parameters.dry = false;
    parameters.spillover = false;
    return parameters;
}

void test_identity_bounds_and_tempo_division() {
    auto parameters = basic_parameters();
    parameters.time_mode = tabletfx::DelayTimeMode::tempo_sync;
    parameters.tap_division = tabletfx::DelayDivision::eighth;
    parameters.tempo_bpm = 120.0F;
    tabletfx::AutoSwellDelay delay(8000.0F, 1, 2000.0F, parameters);
    require(delay.id() == "auto-swell-delay" && delay.enabled(),
            "Auto-Swell Delay identity or initial enable state is incorrect");
    require(std::abs(delay.effective_time_ms() - 250.0F) < 0.1F,
            "tempo-synced eighth notes at 120 BPM must be 250 ms");
    require(delay.memory_bytes() > sizeof(delay),
            "Auto-Swell Delay must report its fixed delay-ring storage");

    parameters.time_mode = static_cast<tabletfx::DelayTimeMode>(255U);
    parameters.tap_division = static_cast<tabletfx::DelayDivision>(255U);
    parameters.time_ms = 5000.0F;
    parameters.feedback = 2.0F;
    parameters.mix = -1.0F;
    parameters.swell_ms = -10.0F;
    parameters.mod = 4.0F;
    parameters.tone = 3.0F;
    parameters.sense_db = 20.0F;
    parameters.tempo_bpm = 500.0F;
    parameters.taper = static_cast<tabletfx::AutoSwellDelayTaper>(255U);
    delay.configure(parameters);
    const auto safe = delay.parameters();
    require(safe.time_mode == tabletfx::DelayTimeMode::milliseconds &&
                safe.tap_division == tabletfx::DelayDivision::quarter &&
                safe.time_ms == 2000.0F && safe.feedback == 0.95F &&
                safe.mix == 0.0F && safe.swell_ms == 1.0F &&
                safe.mod == 1.0F && safe.tone == 1.0F &&
                safe.sense_db == 0.0F && safe.tempo_bpm == 300.0F &&
                safe.taper == tabletfx::AutoSwellDelayTaper::normal,
            "Auto-Swell Delay controls must remain within project-authored bounds");
}

void test_dry_switch_and_swell_tapers() {
    auto parameters = basic_parameters();
    parameters.time_ms = 20.0F;
    parameters.mix = 0.0F;
    parameters.swell_ms = 4.0F;
    parameters.sense_db = -20.0F;
    parameters.taper = tabletfx::AutoSwellDelayTaper::normal;
    parameters.dry = true;
    tabletfx::AutoSwellDelay swollen_dry(8000.0F, 1, 100.0F, parameters);
    std::array<float, 3> dry_audio{1.0F, 1.0F, 1.0F};
    swollen_dry.process(dry_audio, 1);
    require(std::abs(dry_audio[0] - 0.03125F) < 0.004F &&
                dry_audio[1] > dry_audio[0],
            "Dry enabled must apply the rising project taper to direct sound");

    parameters.dry = false;
    tabletfx::AutoSwellDelay unchanged_dry(8000.0F, 1, 100.0F, parameters);
    std::array<float, 2> direct_audio{1.0F, 1.0F};
    unchanged_dry.process(direct_audio, 1);
    require(direct_audio[0] == 1.0F && direct_audio[1] == 1.0F,
            "Dry disabled must leave the direct path unswelled");

    parameters.dry = true;
    // Keep the detector decisively above its threshold throughout both
    // renders so this comparison measures only the selected taper curve.
    parameters.sense_db = -72.0F;
    parameters.taper = tabletfx::AutoSwellDelayTaper::slow;
    tabletfx::AutoSwellDelay slow(8000.0F, 1, 100.0F, parameters);
    parameters.taper = tabletfx::AutoSwellDelayTaper::fast;
    tabletfx::AutoSwellDelay fast(8000.0F, 1, 100.0F, parameters);
    std::array<float, 12> slow_audio{};
    std::array<float, 12> fast_audio{};
    slow_audio.fill(1.0F);
    fast_audio.fill(1.0F);
    slow.process(slow_audio, 1);
    fast.process(fast_audio, 1);
    require(fast_audio[3] > slow_audio[3],
            "fast taper must rise above the slow taper at the same phase");
}

void test_delay_time_and_bounded_feedback() {
    auto parameters = basic_parameters();
    parameters.feedback = 0.5F;
    tabletfx::AutoSwellDelay delay(8000.0F, 1, 100.0F, parameters);
    std::array<float, 72> audio{};
    audio[0] = 1.0F;
    delay.process(audio, 1);
    require(std::abs(audio[32]) > 0.10F && std::abs(audio[32]) < 0.13F,
            "a 4 ms setting at 8 kHz must produce a swell-shaped first repeat");
    require(std::abs(audio[64]) > std::abs(audio[32]) * 0.3F &&
                std::abs(audio[64]) < std::abs(audio[32]) * 0.6F,
            "bounded 0.5 feedback must produce a quieter second repeat");
}

void test_mod_and_tone_shape_the_repeats() {
    auto parameters = basic_parameters();
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    tabletfx::AutoSwellDelay plain(8000.0F, 1, 100.0F, parameters);
    parameters.mod = 1.0F;
    tabletfx::AutoSwellDelay modulated(8000.0F, 1, 100.0F, parameters);
    parameters.mod = 0.0F;
    parameters.tone = 0.0F;
    tabletfx::AutoSwellDelay dark(8000.0F, 1, 100.0F, parameters);

    std::array<float, 40> plain_audio{};
    std::array<float, 40> modulated_audio{};
    std::array<float, 40> dark_audio{};
    plain_audio[0] = 1.0F;
    modulated_audio[0] = 1.0F;
    dark_audio[0] = 1.0F;
    plain.process(plain_audio, 1);
    modulated.process(modulated_audio, 1);
    dark.process(dark_audio, 1);
    require(std::abs(modulated_audio[32]) < std::abs(plain_audio[32]) * 0.98F,
            "Mod must alter the delayed repeat timing through pitch modulation");
    require(std::abs(dark_audio[32]) < std::abs(plain_audio[32]) * 0.5F,
            "lower Tone must darken the delayed repeat");
}

void test_stereo_independence_and_finite_recovery() {
    auto parameters = basic_parameters();
    parameters.feedback = 0.0F;
    tabletfx::AutoSwellDelay stereo(8000.0F, 2, 100.0F, parameters);
    std::array<float, 80> audio{};
    audio[0] = 1.0F;
    stereo.process(audio, 2);
    require(std::abs(audio[64]) > 0.05F && std::abs(audio[65]) < 1.0e-7F,
            "left-channel input must not leak into the right-channel delay");

    std::array<float, 6> invalid{NAN, INFINITY, 0.5F, -INFINITY, 0.0F, 0.25F};
    stereo.reset();
    stereo.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "non-finite input must not contaminate the delay or output");

    stereo.reset();
    std::array<float, 80> after_reset{};
    stereo.process(after_reset, 2);
    require(std::all_of(after_reset.begin(), after_reset.end(), [](float sample) {
                return sample == 0.0F;
            }),
            "reset must clear delay, filter, detector, and modulation state");
}

void test_spillover_and_nonspillover_bypass() {
    auto parameters = basic_parameters();
    parameters.spillover = true;
    tabletfx::AutoSwellDelay spilling(8000.0F, 1, 100.0F, parameters);
    std::array<float, 8> initial{};
    initial[0] = 1.0F;
    spilling.process(initial, 1);
    spilling.set_enabled(false);
    require(!spilling.enabled(), "set_enabled(false) must bypass the processor");

    std::array<float, 64> tail{};
    spilling.process(tail, 1);
    require(std::abs(tail[24]) > 0.08F,
            "spillover must preserve the pending delay repeat after bypass");
    std::array<float, 128> expired{};
    spilling.process(expired, 1);
    require(std::all_of(expired.begin(), expired.end(), [](float sample) {
                return std::abs(sample) < 1.0e-7F;
            }),
            "a zero-feedback spillover tail must expire after its remaining repeat");

    parameters.spillover = false;
    tabletfx::AutoSwellDelay cut(8000.0F, 1, 100.0F, parameters);
    initial.fill(0.0F);
    initial[0] = 1.0F;
    cut.process(initial, 1);
    cut.set_enabled(false);
    std::array<float, 64> silence{};
    cut.process(silence, 1);
    require(std::all_of(silence.begin(), silence.end(), [](float sample) {
                return sample == 0.0F;
            }),
            "bypass with Spillover off must cut the stored delay tail");
}

void test_variable_blocks_and_no_process_allocations() {
    auto parameters = basic_parameters();
    parameters.time_ms = 12.0F;
    parameters.feedback = 0.35F;
    parameters.mix = 0.6F;
    parameters.swell_ms = 35.0F;
    parameters.mod = 0.7F;
    parameters.taper = tabletfx::AutoSwellDelayTaper::slow;
    tabletfx::AutoSwellDelay whole(8000.0F, 2, 100.0F, parameters);
    tabletfx::AutoSwellDelay chunked(8000.0F, 2, 100.0F, parameters);

    std::vector<float> source(257U * 2U);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(static_cast<float>(i) * 0.071F) *
                    (i % 19U == 0U ? 0.8F : 0.2F);
    }
    std::vector<float> whole_audio = source;
    std::vector<float> chunked_audio = source;
    whole.process(whole_audio, 2);
    const std::array<std::size_t, 5> chunk_sizes{7U, 19U, 33U, 2U, 41U};
    std::size_t frame_offset = 0;
    std::size_t chunk_index = 0;
    while (frame_offset < 257U) {
        const std::size_t frames = std::min(
            chunk_sizes[chunk_index % chunk_sizes.size()], 257U - frame_offset);
        chunked.process(std::span<float>(chunked_audio).subspan(
                            frame_offset * 2U, frames * 2U),
                        2);
        frame_offset += frames;
        ++chunk_index;
    }
    for (std::size_t i = 0; i < whole_audio.size(); ++i) {
        require(std::abs(whole_audio[i] - chunked_audio[i]) < 1.0e-6F,
                "processing partitioned blocks must match one contiguous block");
    }

    std::array<float, 512> allocation_probe{};
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    watch_allocations.store(true, std::memory_order_relaxed);
    whole.process(allocation_probe, 2);
    watch_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == before,
            "Auto-Swell Delay process() must not allocate memory");
}

} // namespace

void* operator new(std::size_t size) {
    if (watch_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* pointer = std::malloc(size)) return pointer;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (watch_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* pointer = std::malloc(size)) return pointer;
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
    try {
        test_identity_bounds_and_tempo_division();
        test_dry_switch_and_swell_tapers();
        test_delay_time_and_bounded_feedback();
        test_mod_and_tone_shape_the_repeats();
        test_stereo_independence_and_finite_recovery();
        test_spillover_and_nonspillover_bypass();
        test_variable_blocks_and_no_process_allocations();
        std::cout << "Auto-Swell Delay tests passed\n";
    } catch (const std::exception& exception) {
        watch_allocations.store(false, std::memory_order_relaxed);
        std::cerr << "Auto-Swell Delay test failure: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
