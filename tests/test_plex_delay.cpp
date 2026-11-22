#include "tabletfx/effects/plex_delay.hpp"

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

std::atomic<bool> watch_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

tabletfx::PlexDelayParameters simple_parameters() {
    tabletfx::PlexDelayParameters p;
    p.active_lines = 2U;
    p.time_ms.fill(1.0F);
    p.time_mode.fill(tabletfx::DelayTimeMode::milliseconds);
    p.line_level.fill(1.0F);
    p.line_pan.fill(0.0F);
    p.decay_seconds = 3.0F;
    p.diffusion = 0.0F;
    p.low_cut_hz = 0.0F;
    p.high_cut_hz = 20000.0F;
    p.mix = 1.0F;
    return p;
}

} // namespace

void* operator new(std::size_t size) {
    if (watch_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    if (watch_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

void test_line_timing_count_pan_and_dry_path() {
    auto p = simple_parameters();
    p.active_lines = 1U;
    p.time_ms[0] = 1.0F;
    p.line_pan[0] = -1.0F;
    p.line_pan[1] = 1.0F;
    p.line_level[1] = 0.0F;
    tabletfx::PlexDelay delay(8000.0F, 100.0F, p);
    require(delay.delay_samples(0U) == 8U && delay.maximum_delay_ms() == 100.0F,
            "line time should resolve to frames within requested storage");

    std::array<float, 40U> stereo{};
    stereo[0] = 1.0F;
    stereo[1] = 1.0F;
    delay.process(stereo, 2U);
    require(stereo[16U] > 0.8F && stereo[17U] == 0.0F,
            "hard-left line pan should only emit on the left channel");

    delay.reset();
    p = delay.parameters();
    p.mix = 0.0F;
    delay.configure(p);
    stereo.fill(0.0F);
    stereo[0] = 0.25F;
    stereo[1] = -0.5F;
    delay.process(stereo, 2U);
    require(stereo[0] == 0.25F && stereo[1] == -0.5F,
            "zero mix should preserve the dry stereo input exactly");
}

void test_matrix_diffusion_routes_energy_to_other_line() {
    auto p = simple_parameters();
    p.time_ms[0] = 1.0F;
    p.time_ms[1] = 2.0F;
    p.line_level.fill(0.0F);
    p.line_level[1] = 1.0F;
    p.diffusion = 1.0F;
    tabletfx::PlexDelay delay(8000.0F, 100.0F, p);
    require(delay.delay_samples(0U) == 8U && delay.delay_samples(1U) == 16U,
            "the matrix must retain distinct per-line timing");
    std::array<float, 64U> mono{};
    mono[0] = 1.0F;
    delay.process(mono, 1U);
    require(mono[16U] > 0.8F && mono[24U] > mono[16U] * 0.8F,
            "diffusion should route a delayed line into a later line output");
}

void test_tempo_pre_delay_master_time_and_bounds() {
    auto p = simple_parameters();
    p.active_lines = 1U;
    p.time_mode[0] = tabletfx::DelayTimeMode::tempo_sync;
    p.division[0] = tabletfx::DelayDivision::quarter;
    p.tempo_bpm = 120.0F;
    p.master_time = 0.5F;
    p.pre_delay_ms = 10.0F;
    tabletfx::PlexDelay delay(8000.0F, 1000.0F, p);
    require(delay.delay_samples(0U) == 2000U &&
                delay.pre_delay_samples() == 80U,
            "quarter-note 120 BPM at half master time and pre-delay must be exact");

    p = delay.parameters();
    p.active_lines = 99U;
    p.time_ms[0] = -10.0F;
    p.tempo_bpm = 10000.0F;
    p.master_time = 50.0F;
    p.decay_seconds = 0.0F;
    p.diffusion = 2.0F;
    p.pre_delay_ms = 50000.0F;
    p.line_level[0] = 9.0F;
    p.line_pan[0] = -9.0F;
    p.balance = -5.0F;
    delay.configure(p);
    const auto bounded = delay.parameters();
    require(bounded.active_lines == 8U && bounded.time_ms[0] == 1.0F &&
                bounded.tempo_bpm == 300.0F && bounded.master_time == 4.0F &&
                bounded.decay_seconds == 0.1F && bounded.diffusion == 1.0F &&
                bounded.pre_delay_ms == delay.maximum_delay_ms() &&
                bounded.line_level[0] == 2.0F &&
                bounded.line_pan[0] == -1.0F && bounded.balance == -1.0F,
            "the project-authored parameter limits should be enforced");
}

void test_stack_hold_and_reset() {
    auto p = simple_parameters();
    p.active_lines = 1U;
    p.time_ms[0] = 1.0F;
    p.decay_seconds = 0.1F;
    tabletfx::PlexDelay delay(8000.0F, 100.0F, p);
    std::array<float, 16U> seed{};
    seed[0] = 1.0F;
    delay.process(seed, 1U);

    p = delay.parameters();
    p.stack_mode = tabletfx::PlexDelayStackMode::hold;
    delay.configure(p);
    std::array<float, 32U> held{};
    delay.process(held, 1U);
    require(held[0U] > 0.8F && held[8U] > 0.8F && held[16U] > 0.8F,
            "Hold should preserve the tail; outputs: " +
                std::to_string(held[0U]) + ", " +
                std::to_string(held[8U]) + ", " +
                std::to_string(held[16U]));

    delay.reset();
    held.fill(0.0F);
    delay.process(held, 1U);
    require(std::all_of(held.begin(), held.end(),
                        [](float x) { return x == 0.0F; }),
            "reset should clear line, pre-delay, and filter history");
}

void test_memory_cap_finite_audio_and_no_process_allocations() {
    tabletfx::PlexDelay capped(384000.0F, 2000.0F);
    require(capped.maximum_delay_ms() < 100.0F &&
                capped.memory_bytes() <= sizeof(capped) +
                    tabletfx::plex_delay_storage_limit_bytes,
            "the fixed storage cap should bound per-line time at high sample rates");

    auto p = simple_parameters();
    p.diffusion = 1.0F;
    p.stack_mode = tabletfx::PlexDelayStackMode::stack;
    tabletfx::PlexDelay delay(48000.0F, 100.0F, p);
    std::array<float, 512U> audio{};
    audio[0] = 0.8F;
    audio[1] = -0.2F;
    allocation_count.store(0U, std::memory_order_relaxed);
    watch_allocations.store(true, std::memory_order_relaxed);
    delay.process(audio, 2U);
    watch_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "Plex Delay process() must not allocate");
    require(std::all_of(audio.begin(), audio.end(),
                        [](float x) { return std::isfinite(x) && std::abs(x) <= 8.0F; }),
            "matrix feedback must remain finite and signal bounded");
}

void test_unknown_enums_and_nonfinite_inputs_recover() {
    auto p = simple_parameters();
    p.time_ms[0] = std::numeric_limits<float>::quiet_NaN();
    p.time_mode[0] = static_cast<tabletfx::DelayTimeMode>(99U);
    p.division[0] = static_cast<tabletfx::DelayDivision>(99U);
    p.stack_mode = static_cast<tabletfx::PlexDelayStackMode>(99U);
    p.mix = std::numeric_limits<float>::infinity();
    tabletfx::PlexDelay delay(8000.0F, 100.0F, p);
    require(delay.parameters().time_ms[0] == 45.0F &&
                delay.parameters().time_mode[0] == tabletfx::DelayTimeMode::milliseconds &&
                delay.parameters().stack_mode == tabletfx::PlexDelayStackMode::off &&
                delay.parameters().mix == 0.25F,
            "invalid parameters and enum values should fall back safely");
    std::array<float, 4U> audio{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 100.0F, -100.0F};
    delay.process(audio, 2U);
    require(std::all_of(audio.begin(), audio.end(),
                        [](float x) { return std::isfinite(x); }),
            "nonfinite and extreme input samples should not contaminate output");
}

} // namespace

int main() {
    try {
        test_line_timing_count_pan_and_dry_path();
        test_matrix_diffusion_routes_energy_to_other_line();
        test_tempo_pre_delay_master_time_and_bounds();
        test_stack_hold_and_reset();
        test_memory_cap_finite_audio_and_no_process_allocations();
        test_unknown_enums_and_nonfinite_inputs_recover();
        std::cout << "Plex Delay tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Plex Delay test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
