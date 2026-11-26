#include "tabletfx/effects/tone_master_small_hall.hpp"

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

std::atomic<bool> watch_allocations{false};
std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> render_impulse(
    tabletfx::ToneMasterSmallHallParameters parameters,
    std::size_t frames = 24000U) {
    parameters.level = 1.0F;
    tabletfx::ToneMasterSmallHallReverb effect(48000.0F, 2U, parameters);
    std::vector<float> audio(frames * 2U, 0.0F);
    audio[0] = 0.8F;
    audio[1] = 0.8F;
    effect.process(audio, 2U);
    return audio;
}

float maximum_difference(std::span<const float> left,
                         std::span<const float> right) {
    require(left.size() == right.size(), "comparison buffers differ in size");
    float result = 0.0F;
    for (std::size_t index = 0U; index < left.size(); ++index) {
        result = std::max(result, std::abs(left[index] - right[index]));
    }
    return result;
}

double energy(std::span<const float> samples, std::size_t begin,
              std::size_t end) {
    double sum = 0.0;
    for (std::size_t index = begin; index < end; ++index) {
        const double sample = samples[index];
        sum += sample * sample;
    }
    return sum;
}

void test_defaults_bounds_memory_and_channel_validation() {
    tabletfx::ToneMasterSmallHallReverb effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "tone-master-small-hall",
            "Tone Master Small Hall identifier changed");
    require(defaults.level == 0.25F && defaults.decay_seconds == 3.2F &&
                defaults.dwell_db == 0.0F && defaults.diffuse == 0.6F &&
                defaults.rate_hz == 0.3F && defaults.depth == 0.15F &&
                defaults.high_cut_gain_db == 0.0F &&
                defaults.low_cut_gain_db == 0.0F && !defaults.spillover,
            "project-authored default controls changed unexpectedly");
    require(effect.memory_bytes() > sizeof(effect) &&
                effect.memory_bytes() < 500000U,
            "stereo hall state should be fixed and below its memory budget");

    tabletfx::ToneMasterSmallHallParameters extreme;
    extreme.level = 2.0F;
    extreme.decay_seconds = 100.0F;
    extreme.dwell_db = 30.0F;
    extreme.diffuse = -1.0F;
    extreme.rate_hz = 100.0F;
    extreme.depth = 3.0F;
    extreme.high_cut_gain_db = 30.0F;
    extreme.low_cut_gain_db = -30.0F;
    extreme.spillover = true;
    effect.configure(extreme);
    const auto bounded = effect.parameters();
    require(bounded.level == 1.0F && bounded.decay_seconds == 10.0F &&
                bounded.dwell_db == 12.0F && bounded.diffuse == 0.0F &&
                bounded.rate_hz == 6.0F && bounded.depth == 1.0F &&
                bounded.high_cut_gain_db == 12.0F &&
                bounded.low_cut_gain_db == -12.0F && bounded.spillover,
            "controls were not clamped to project-authored bounds");

    tabletfx::ToneMasterSmallHallParameters invalid;
    invalid.level = std::numeric_limits<float>::quiet_NaN();
    invalid.decay_seconds = std::numeric_limits<float>::infinity();
    invalid.dwell_db = std::numeric_limits<float>::quiet_NaN();
    invalid.diffuse = std::numeric_limits<float>::infinity();
    invalid.rate_hz = std::numeric_limits<float>::quiet_NaN();
    invalid.depth = std::numeric_limits<float>::infinity();
    invalid.high_cut_gain_db = std::numeric_limits<float>::quiet_NaN();
    invalid.low_cut_gain_db = std::numeric_limits<float>::infinity();
    effect.configure(invalid);
    const auto safe = effect.parameters();
    require(safe.level == defaults.level &&
                safe.decay_seconds == defaults.decay_seconds &&
                safe.dwell_db == defaults.dwell_db &&
                safe.diffuse == defaults.diffuse &&
                safe.rate_hz == defaults.rate_hz &&
                safe.depth == defaults.depth &&
                safe.high_cut_gain_db == defaults.high_cut_gain_db &&
                safe.low_cut_gain_db == defaults.low_cut_gain_db,
            "non-finite values must fall back to sane project defaults");

    tabletfx::ToneMasterSmallHallReverb mono(48000.0F, 1U);
    std::array<float, 2U> wrong_channels{0.25F, -0.5F};
    const auto before = wrong_channels;
    mono.process(wrong_channels, 2U);
    require(wrong_channels == before,
            "a channel mismatch must leave the input unchanged");
    mono.reset();
}

void test_immediate_dry_path_and_mix() {
    tabletfx::ToneMasterSmallHallParameters dry_parameters;
    dry_parameters.level = 0.0F;
    tabletfx::ToneMasterSmallHallReverb dry(48000.0F, 2U, dry_parameters);
    std::array<float, 16U> samples{
        0.25F, -0.125F, 0.5F, 0.25F, -0.4F, 0.1F, 0.2F, -0.6F,
        0.0F, 0.3F, -0.2F, -0.1F, 0.7F, -0.8F, 0.05F, 0.1F};
    const auto original = samples;
    dry.process(samples, 2U);
    require(samples == original,
            "level=0 must preserve the dry path sample-for-sample");

    tabletfx::ToneMasterSmallHallReverb mixed(48000.0F, 1U);
    std::array<float, 8U> mono{0.8F, -0.4F, 0.2F, 0.0F,
                               0.1F, -0.1F, 0.3F, 0.2F};
    mixed.process(mono, 1U);
    require(std::abs(mono[0] - 0.6F) < 1.0e-6F,
            "the dry signal should be heard immediately before the wet tail");
}

void test_controls_change_wet_response_and_output_stays_finite() {
    tabletfx::ToneMasterSmallHallParameters base;
    base.decay_seconds = 3.2F;
    base.diffuse = 0.6F;
    base.depth = 0.15F;
    base.rate_hz = 0.3F;
    const auto reference = render_impulse(base);

    auto low_dwell = base;
    low_dwell.dwell_db = -12.0F;
    auto high_dwell = base;
    high_dwell.dwell_db = 12.0F;
    require(maximum_difference(render_impulse(low_dwell),
                               render_impulse(high_dwell)) > 1.0e-4F,
            "Dwell did not change the wet input level");

    auto sparse = base;
    sparse.diffuse = 0.0F;
    auto dense = base;
    dense.diffuse = 1.0F;
    require(maximum_difference(render_impulse(sparse),
                               render_impulse(dense)) > 1.0e-4F,
            "Diffuse did not change the wet response");

    auto still = base;
    still.depth = 0.0F;
    auto modulated = base;
    modulated.rate_hz = 3.0F;
    modulated.depth = 1.0F;
    require(maximum_difference(render_impulse(still),
                               render_impulse(modulated)) > 1.0e-5F,
            "Rate/Depth did not change the wet response");

    auto dark = base;
    dark.high_cut_gain_db = -12.0F;
    dark.low_cut_gain_db = -6.0F;
    auto bright = base;
    bright.high_cut_gain_db = 12.0F;
    bright.low_cut_gain_db = 6.0F;
    require(maximum_difference(render_impulse(dark),
                               render_impulse(bright)) > 1.0e-5F,
            "post-reverb shelf gain controls did not alter the wet response");

    auto short_tail = base;
    short_tail.decay_seconds = 0.25F;
    auto long_tail = base;
    long_tail.decay_seconds = 10.0F;
    const auto short_audio = render_impulse(short_tail, 48000U);
    const auto long_audio = render_impulse(long_tail, 48000U);
    require(energy(long_audio, 24000U, long_audio.size()) >
                energy(short_audio, 24000U, short_audio.size()) * 1.5,
            "longer decay should retain more late-tail energy");

    for (const float sample : reference) {
        require(std::isfinite(sample), "reverb output must remain finite");
    }
    tabletfx::ToneMasterSmallHallReverb mono(48000.0F, 1U);
    std::array<float, 256U> mono_audio{};
    mono_audio[0] = 0.8F;
    mono.process(mono_audio, 1U);
    for (const float sample : mono_audio) {
        require(std::isfinite(sample), "mono output must remain finite");
    }
}

double bypass_tail_energy(bool spillover) {
    tabletfx::ToneMasterSmallHallParameters parameters;
    parameters.level = 1.0F;
    parameters.decay_seconds = 5.0F;
    parameters.spillover = spillover;
    tabletfx::ToneMasterSmallHallReverb effect(48000.0F, 2U, parameters);
    std::array<float, 256U> audio{};
    audio[0] = 0.8F;
    audio[1] = 0.8F;
    effect.process(audio, 2U);

    double result = 0.0;
    audio.fill(0.0F);
    for (std::size_t block = 0U; block < 375U; ++block) {
        effect.process_bypassed(audio, 2U);
        result += energy(audio, 0U, audio.size());
        audio.fill(0.0F);
    }
    return result;
}

void test_spillover_behavior_on_bypass() {
    const double with_tail = bypass_tail_energy(true);
    const double cut_tail = bypass_tail_energy(false);
    require(with_tail > 1.0e-5,
            "spillover should preserve a reverb tail after bypass");
    require(cut_tail == 0.0,
            "spillover-off bypass should clear the tail immediately");
}

void test_processing_allocates_nothing() {
    tabletfx::ToneMasterSmallHallParameters parameters;
    parameters.spillover = true;
    tabletfx::ToneMasterSmallHallReverb effect(48000.0F, 2U, parameters);
    std::array<float, 256U> block{};
    block[0] = 0.25F;

    allocation_count.store(0U, std::memory_order_relaxed);
    watch_allocations.store(true, std::memory_order_relaxed);
    for (std::size_t iteration = 0U; iteration < 64U; ++iteration) {
        effect.process(block, 2U);
        effect.process_bypassed(block, 2U);
    }
    watch_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "active and bypass processing must not allocate");
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

int main() {
    try {
        test_defaults_bounds_memory_and_channel_validation();
        test_immediate_dry_path_and_mix();
        test_controls_change_wet_response_and_output_stays_finite();
        test_spillover_behavior_on_bypass();
        test_processing_allocates_nothing();
        std::cout << "Tone Master Small Hall tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        watch_allocations.store(false, std::memory_order_relaxed);
        std::cerr << "Tone Master Small Hall test failure: " << error.what()
                  << '\n';
        return 1;
    }
}
