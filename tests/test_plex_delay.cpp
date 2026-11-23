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
#include <vector>

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

void test_input_select_routes_stereo_sources_to_wet_matrix() {
    const auto render = [](tabletfx::PlexDelayInputSelect input_select) {
        auto parameters = simple_parameters();
        parameters.active_lines = 1U;
        parameters.input_select = input_select;
        parameters.line_pan[0] = 0.0F;
        parameters.time_ms[0] = 1.0F;
        tabletfx::PlexDelay delay(8000.0F, 100.0F, parameters);
        std::array<float, 40U> audio{};
        audio[0] = 0.75F;
        audio[1] = 0.25F;
        delay.process(audio, 2U);
        return std::array<float, 2U>{audio[16U], audio[17U]};
    };

    const auto sum = render(tabletfx::PlexDelayInputSelect::sum_lr);
    const auto left = render(tabletfx::PlexDelayInputSelect::left);
    const auto right = render(tabletfx::PlexDelayInputSelect::right);
    require(left[0] > sum[0] && sum[0] > right[0] &&
                left[1] > sum[1] && sum[1] > right[1],
            "Left, Right, and summed input selection should route distinct wet sources");
    require(std::abs(left[0] - sum[0] * 1.5F) < 1.0e-5F &&
                std::abs(left[1] - sum[1] * 1.5F) < 1.0e-5F &&
                std::abs(right[0] - sum[0] * 0.5F) < 1.0e-5F &&
                std::abs(right[1] - sum[1] * 0.5F) < 1.0e-5F,
            "L+R should preserve the project default average of the stereo input");
    require(tabletfx::PlexDelayParameters{}.input_select ==
                tabletfx::PlexDelayInputSelect::sum_lr,
            "summed L+R should remain the default input selection");
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
    p.input_select = static_cast<tabletfx::PlexDelayInputSelect>(99U);
    p.mix = std::numeric_limits<float>::infinity();
    tabletfx::PlexDelay delay(8000.0F, 100.0F, p);
    require(delay.parameters().time_ms[0] == 45.0F &&
                delay.parameters().time_mode[0] == tabletfx::DelayTimeMode::milliseconds &&
                delay.parameters().stack_mode == tabletfx::PlexDelayStackMode::off &&
                delay.parameters().input_select == tabletfx::PlexDelayInputSelect::sum_lr &&
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

float render_plex_sine_rms(tabletfx::PlexDelayParameters parameters,
                           float frequency_hz) {
    parameters.active_lines = 1U;
    parameters.time_ms[0] = 1.0F;
    parameters.time_mode[0] = tabletfx::DelayTimeMode::milliseconds;
    parameters.line_level[0] = 1.0F;
    parameters.line_pan[0] = 0.0F;
    parameters.diffusion = 0.0F;
    parameters.low_cut_hz = 0.0F;
    parameters.high_cut_hz = 20000.0F;
    parameters.mix = 1.0F;
    tabletfx::PlexDelay delay(8000.0F, 100.0F, parameters);
    std::array<float, 8000U> audio{};
    for (std::size_t i = 0; i < audio.size(); ++i) {
        audio[i] = 0.2F * std::sin(2.0F * 3.14159265358979323846F *
                                   frequency_hz * static_cast<float>(i) /
                                   8000.0F);
    }
    delay.process(audio, 1U);
    double square_sum = 0.0;
    for (std::size_t i = 6000U; i < audio.size(); ++i) {
        square_sum += static_cast<double>(audio[i]) * audio[i];
    }
    return static_cast<float>(std::sqrt(square_sum / 2000.0));
}

void test_wet_filter_and_lfo_controls() {
    auto p = simple_parameters();
    p.wet_filter_type = tabletfx::PlexWetFilterType::low_pass;
    p.wet_filter_frequency_hz = 600.0F;
    const float low = render_plex_sine_rms(p, 200.0F);
    const float high = render_plex_sine_rms(p, 2400.0F);
    require(low > high * 4.0F,
            "the wet low-pass should attenuate upper frequencies");

    p.wet_filter_type = tabletfx::PlexWetFilterType::peaking;
    p.wet_filter_frequency_hz = 500.0F;
    p.wet_filter_q = 4.0F;
    p.wet_filter_gain_db = 6.0F;
    const float static_peak = render_plex_sine_rms(p, 1500.0F);
    p.filter_lfo_enabled = true;
    p.filter_lfo_type = tabletfx::PlexFilterLfoType::triangle;
    p.filter_lfo_rate_hz = 2.0F;
    p.filter_mod_frequency_hz = 3000.0F;
    const float modulated_peak = render_plex_sine_rms(p, 1500.0F);
    require(std::abs(modulated_peak - static_peak) > 0.001F,
            "the filter LFO should modulate the wet filter frequency");
}

void test_envelope_send_and_ducker() {
    auto p = simple_parameters();
    p.wet_filter_type = tabletfx::PlexWetFilterType::bypass;
    p.time_ms[0] = 1.0F;
    p.line_level.fill(0.0F);
    p.line_level[0] = 1.0F;
    p.mix = 1.0F;
    p.diffusion = 0.0F;
    p.low_cut_hz = 0.0F;
    p.high_cut_hz = 20000.0F;
    const float open_send = render_plex_sine_rms(p, 500.0F);
    p.envelope_enabled = true;
    p.envelope_threshold_db = -12.0F;
    p.envelope_attack_ms = 1.0F;
    p.envelope_release_ms = 1.0F;
    const float shaped_send = render_plex_sine_rms(p, 500.0F);
    require(shaped_send < open_send * 0.8F,
            "the envelope follower should scale the effect send");

    p.envelope_enabled = false;
    p.ducker_attenuation_db = 24.0F;
    p.ducker_threshold_db = -30.0F;
    p.ducker_release_ms = 20.0F;
    const float ducked = render_plex_sine_rms(p, 500.0F);
    require(ducked < open_send * 0.5F,
            "the ducker should reduce wet output while input exceeds threshold");
}

void test_stereo_spread_and_advanced_bounds() {
    auto p = simple_parameters();
    p.active_lines = 1U;
    p.time_ms[0] = 1.0F;
    p.line_pan[0] = -1.0F;
    p.mix = 1.0F;
    p.stereo_spread = 2.0F;
    tabletfx::PlexDelay delay(8000.0F, 100.0F, p);
    std::array<float, 24U> stereo{};
    stereo[0] = 1.0F;
    stereo[1] = 1.0F;
    delay.process(stereo, 2U);
    require(stereo[16U] > 0.7F && stereo[17U] < -0.2F,
            "200% spread should widen a hard-left wet image around the center");

    auto bounded = delay.parameters();
    bounded.wet_filter_frequency_hz = 50000.0F;
    bounded.wet_filter_q = 100.0F;
    bounded.wet_filter_gain_db = 100.0F;
    bounded.filter_lfo_rate_hz = 100.0F;
    bounded.filter_mod_frequency_hz = -100.0F;
    bounded.envelope_threshold_db = -200.0F;
    bounded.envelope_attack_ms = 0.0F;
    bounded.ducker_attenuation_db = 100.0F;
    bounded.ducker_threshold_db = -200.0F;
    bounded.ducker_release_ms = 0.0F;
    bounded.stereo_spread = -20.0F;
    delay.configure(bounded);
    const auto safe = delay.parameters();
    require(safe.wet_filter_frequency_hz == 3600.0F &&
                safe.wet_filter_q == 12.0F && safe.wet_filter_gain_db == 18.0F &&
                safe.filter_lfo_rate_hz == 10.0F &&
                safe.filter_mod_frequency_hz == 20.0F &&
                safe.envelope_threshold_db == -90.0F &&
                safe.envelope_attack_ms == 1.0F &&
                safe.ducker_attenuation_db == 36.0F &&
                safe.ducker_threshold_db == -90.0F &&
                safe.ducker_release_ms == 1.0F && safe.stereo_spread == -2.0F,
            "advanced controls should clamp to project-authored ranges");
}

} // namespace

int main() {
    try {
        test_line_timing_count_pan_and_dry_path();
        test_input_select_routes_stereo_sources_to_wet_matrix();
        test_matrix_diffusion_routes_energy_to_other_line();
        test_tempo_pre_delay_master_time_and_bounds();
        test_stack_hold_and_reset();
        test_memory_cap_finite_audio_and_no_process_allocations();
        test_unknown_enums_and_nonfinite_inputs_recover();
        test_wet_filter_and_lfo_controls();
        test_envelope_send_and_ducker();
        test_stereo_spread_and_advanced_bounds();
        std::cout << "Plex Delay tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Plex Delay test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
