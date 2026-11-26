#include "tabletfx/effects/tone_master_modulated_large_hall.hpp"

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
    tabletfx::ToneMasterModulatedLargeHallParameters parameters,
    std::size_t frames = 48000U, bool force_full_wet = true) {
    if (force_full_wet) parameters.level = 1.0F;
    tabletfx::ToneMasterModulatedLargeHallReverb effect(
        48000.0F, 2U, parameters);
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
    double total = 0.0;
    for (std::size_t index = begin; index < end; ++index) {
        const double sample = samples[index];
        total += sample * sample;
    }
    return total;
}

void test_identity_defaults_bounds_memory_and_channels() {
    tabletfx::ToneMasterModulatedLargeHallReverb effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "tone-master-modulated-large-hall",
            "Modulated Large Hall identifier changed");
    require(defaults.level == 0.28F && defaults.decay_seconds == 6.5F &&
                defaults.dwell == 1.0F && defaults.diffuse == 0.74F &&
                defaults.rate_hz == 0.23F && defaults.depth == 0.20F &&
                defaults.high_cut_gain_db == -1.5F &&
                defaults.low_cut_gain_db == -0.5F && !defaults.spillover,
            "project-authored Large Hall defaults changed unexpectedly");
    require(effect.memory_bytes() > sizeof(effect) &&
                effect.memory_bytes() < 200000U,
            "48 kHz stereo Large Hall state should be bounded below 200 KB");

    tabletfx::ToneMasterModulatedLargeHallParameters extreme;
    extreme.level = 2.0F;
    extreme.decay_seconds = 100.0F;
    extreme.dwell = 3.0F;
    extreme.diffuse = -1.0F;
    extreme.rate_hz = 100.0F;
    extreme.depth = 2.0F;
    extreme.high_cut_gain_db = 20.0F;
    extreme.low_cut_gain_db = -20.0F;
    extreme.spillover = true;
    effect.configure(extreme);
    const auto bounded = effect.parameters();
    require(bounded.level == 1.0F && bounded.decay_seconds == 18.0F &&
                bounded.dwell == 2.0F && bounded.diffuse == 0.0F &&
                bounded.rate_hz == 4.0F && bounded.depth == 1.0F &&
                bounded.high_cut_gain_db == 6.0F &&
                bounded.low_cut_gain_db == -12.0F && bounded.spillover,
            "controls were not clamped to project-authored bounds");

    tabletfx::ToneMasterModulatedLargeHallParameters invalid;
    invalid.level = std::numeric_limits<float>::quiet_NaN();
    invalid.decay_seconds = std::numeric_limits<float>::infinity();
    invalid.dwell = std::numeric_limits<float>::quiet_NaN();
    invalid.diffuse = std::numeric_limits<float>::infinity();
    invalid.rate_hz = std::numeric_limits<float>::quiet_NaN();
    invalid.depth = std::numeric_limits<float>::infinity();
    invalid.high_cut_gain_db = std::numeric_limits<float>::quiet_NaN();
    invalid.low_cut_gain_db = std::numeric_limits<float>::infinity();
    effect.configure(invalid);
    const auto safe = effect.parameters();
    require(safe.level == defaults.level &&
                safe.decay_seconds == defaults.decay_seconds &&
                safe.dwell == defaults.dwell && safe.diffuse == defaults.diffuse &&
                safe.rate_hz == defaults.rate_hz && safe.depth == defaults.depth &&
                safe.high_cut_gain_db == defaults.high_cut_gain_db &&
                safe.low_cut_gain_db == defaults.low_cut_gain_db,
            "non-finite controls must fall back to project defaults");

    bool rejected = false;
    try {
        tabletfx::ToneMasterModulatedLargeHallReverb invalid_rate(7999.0F, 2U);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "unsupported sample rate was accepted");
    rejected = false;
    try {
        tabletfx::ToneMasterModulatedLargeHallReverb invalid_channels(48000.0F, 3U);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "unsupported channel count was accepted");

    tabletfx::ToneMasterModulatedLargeHallReverb mono(48000.0F, 1U);
    std::array<float, 4U> wrong_channels{0.2F, -0.1F, 0.3F, -0.4F};
    const auto before = wrong_channels;
    mono.process(wrong_channels, 2U);
    require(wrong_channels == before,
            "channel mismatch must leave samples unchanged");
}

void test_immediate_dry_path_and_stereo_response() {
    tabletfx::ToneMasterModulatedLargeHallParameters dry_parameters;
    dry_parameters.level = 0.0F;
    tabletfx::ToneMasterModulatedLargeHallReverb dry(
        48000.0F, 2U, dry_parameters);
    std::array<float, 16U> samples{
        0.25F, -0.125F, 0.5F, 0.25F, -0.4F, 0.1F, 0.2F, -0.6F,
        0.0F, 0.3F, -0.2F, -0.1F, 0.7F, -0.8F, 0.05F, 0.1F};
    const auto original = samples;
    dry.process(samples, 2U);
    require(samples == original,
            "Level=0 must retain the immediate dry signal sample-for-sample");

    tabletfx::ToneMasterModulatedLargeHallParameters half_parameters;
    half_parameters.level = 0.5F;
    tabletfx::ToneMasterModulatedLargeHallReverb half(
        48000.0F, 1U, half_parameters);
    std::array<float, 2U> first_frame{0.8F, 0.0F};
    half.process(first_frame, 1U);
    require(std::abs(first_frame[0] - 0.4F) < 1.0e-6F,
            "the dry component must arrive without algorithmic latency");

    tabletfx::ToneMasterModulatedLargeHallParameters wet_parameters;
    wet_parameters.level = 1.0F;
    const auto stereo = render_impulse(wet_parameters, 24000U);
    require(energy(stereo, 0U, stereo.size()) > 1.0e-5,
            "stereo Large Hall did not produce a measurable wet response");
    double stereo_difference = 0.0;
    for (std::size_t frame = 0U; frame < stereo.size() / 2U; ++frame) {
        require(std::isfinite(stereo[frame * 2U]) &&
                    std::isfinite(stereo[frame * 2U + 1U]),
                "stereo wet output must stay finite");
        stereo_difference += std::abs(
            stereo[frame * 2U] - stereo[frame * 2U + 1U]);
    }
    require(stereo_difference > 1.0e-3,
            "identical stereo inputs should yield decorrelated wet channels");

    tabletfx::ToneMasterModulatedLargeHallReverb mono(48000.0F, 1U);
    std::array<float, 512U> mono_audio{};
    mono_audio[0] = 0.8F;
    mono.process(mono_audio, 1U);
    require(energy(mono_audio, 0U, mono_audio.size()) > 0.0,
            "mono processing did not preserve a finite direct path");
}

void test_documented_control_roles_change_the_response() {
    tabletfx::ToneMasterModulatedLargeHallParameters base;
    base.level = 0.7F;
    base.decay_seconds = 6.5F;
    base.dwell = 1.0F;
    base.diffuse = 0.74F;
    base.rate_hz = 0.23F;
    base.depth = 0.20F;
    const auto reference = render_impulse(base, 48000U);

    auto low_dwell = base;
    low_dwell.dwell = 0.2F;
    auto high_dwell = base;
    high_dwell.dwell = 1.8F;
    require(maximum_difference(render_impulse(low_dwell),
                               render_impulse(high_dwell)) > 1.0e-4F,
            "Dwell must change the input level into the reverb tank");

    auto sparse = base;
    sparse.diffuse = 0.0F;
    auto diffuse = base;
    diffuse.diffuse = 1.0F;
    require(maximum_difference(render_impulse(sparse),
                               render_impulse(diffuse)) > 1.0e-4F,
            "Diffuse must change reflection density");

    auto unmodulated = base;
    unmodulated.depth = 0.0F;
    auto modulated = base;
    modulated.rate_hz = 3.4F;
    modulated.depth = 1.0F;
    require(maximum_difference(render_impulse(unmodulated),
                               render_impulse(modulated)) > 1.0e-5F,
            "Rate and Depth must change modulation of the reverb trail");

    auto darker = base;
    darker.high_cut_gain_db = -10.0F;
    auto brighter = base;
    brighter.high_cut_gain_db = 5.0F;
    require(maximum_difference(render_impulse(darker),
                               render_impulse(brighter)) > 1.0e-5F,
            "High Cut shelving gain must alter wet high frequencies");

    auto low_reduced = base;
    low_reduced.low_cut_gain_db = -10.0F;
    auto low_boosted = base;
    low_boosted.low_cut_gain_db = 5.0F;
    require(maximum_difference(render_impulse(low_reduced),
                               render_impulse(low_boosted)) > 1.0e-5F,
            "Low Cut shelving gain must alter wet low frequencies");

    auto quiet = base;
    quiet.level = 0.1F;
    auto loud = base;
    loud.level = 0.9F;
    require(maximum_difference(render_impulse(quiet, 48000U, false),
                               render_impulse(loud, 48000U, false)) > 1.0e-4F,
            "Level must change the wet/dry blend");

    auto short_decay = base;
    short_decay.decay_seconds = 0.6F;
    auto long_decay = base;
    long_decay.decay_seconds = 16.0F;
    const auto short_audio = render_impulse(short_decay, 96000U);
    const auto long_audio = render_impulse(long_decay, 96000U);
    require(energy(long_audio, 96000U, long_audio.size()) >
                energy(short_audio, 96000U, short_audio.size()) * 1.25,
            "longer Decay must retain more late-tail energy");
    for (float sample : reference) {
        require(std::isfinite(sample), "wet response must remain finite");
    }
}

double bypass_tail_energy(bool spillover) {
    tabletfx::ToneMasterModulatedLargeHallParameters parameters;
    parameters.level = 1.0F;
    parameters.decay_seconds = 10.0F;
    parameters.spillover = spillover;
    tabletfx::ToneMasterModulatedLargeHallReverb effect(
        48000.0F, 2U, parameters);
    std::array<float, 256U> block{};
    block[0] = 0.8F;
    block[1] = 0.8F;
    effect.process(block, 2U);

    double result = 0.0;
    block.fill(0.0F);
    for (std::size_t callback = 0U; callback < 375U; ++callback) {
        effect.process_bypassed(block, 2U);
        result += energy(block, 0U, block.size());
        block.fill(0.0F);
    }
    return result;
}

void test_spillover_and_bypassed_dry_path() {
    require(bypass_tail_energy(true) > 1.0e-5,
            "Spillover should emit the stored tail during block bypass");
    require(bypass_tail_energy(false) == 0.0,
            "Spillover off should clear the stored tail during bypass");

    tabletfx::ToneMasterModulatedLargeHallParameters parameters;
    parameters.spillover = true;
    tabletfx::ToneMasterModulatedLargeHallReverb effect(
        48000.0F, 2U, parameters);
    std::array<float, 4U> dry{0.25F, -0.125F, 0.5F, -0.25F};
    const auto original = dry;
    effect.process_bypassed(dry, 2U);
    require(dry == original,
            "empty spillover state must preserve bypassed dry audio");
}

void test_finite_output_and_no_process_allocations() {
    tabletfx::ToneMasterModulatedLargeHallParameters parameters;
    parameters.spillover = true;
    tabletfx::ToneMasterModulatedLargeHallReverb effect(
        48000.0F, 2U, parameters);
    std::array<float, 16U> exceptional{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
        100000.0F, -100000.0F, 0.1F, -0.2F, 0.3F,
        0.0F, 0.5F, -0.5F, 0.1F, 0.2F, -0.1F, 0.4F, -0.4F};
    effect.process(exceptional, 2U);
    for (float sample : exceptional) {
        require(std::isfinite(sample) && std::abs(sample) <= 8.0F,
                "non-finite or unbounded audio escaped the processor");
    }

    std::array<float, 256U> block{};
    block[0] = 0.2F;
    allocation_count.store(0U, std::memory_order_relaxed);
    watch_allocations.store(true, std::memory_order_relaxed);
    for (std::size_t callback = 0U; callback < 64U; ++callback) {
        effect.process(block, 2U);
        effect.process_bypassed(block, 2U);
    }
    watch_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "active and spillover-bypass processing must not allocate");
}

} // namespace

void* operator new(std::size_t size) {
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        if (watch_allocations.load(std::memory_order_relaxed)) {
            allocation_count.fetch_add(1U, std::memory_order_relaxed);
        }
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        if (watch_allocations.load(std::memory_order_relaxed)) {
            allocation_count.fetch_add(1U, std::memory_order_relaxed);
        }
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_identity_defaults_bounds_memory_and_channels();
        test_immediate_dry_path_and_stereo_response();
        test_documented_control_roles_change_the_response();
        test_spillover_and_bypassed_dry_path();
        test_finite_output_and_no_process_allocations();
        std::cout << "All Modulated Large Hall tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        watch_allocations.store(false, std::memory_order_relaxed);
        std::cerr << "Modulated Large Hall test failure: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
