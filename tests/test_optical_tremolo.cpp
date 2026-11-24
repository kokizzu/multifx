#include "tabletfx/effects/optical_tremolo.hpp"

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

std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<float> render_one_cycle(tabletfx::OpticalTremoloVariant variant) {
    tabletfx::OpticalTremoloParameters parameters;
    parameters.variant = variant;
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.lfo = tabletfx::OpticalTremoloWaveform::Sine;
    parameters.mix = 1.0F;
    parameters.level_db = 0.0F;
    tabletfx::OpticalTremolo effect(8000.0F, parameters);
    std::vector<float> samples(8000U, 1.0F);
    effect.process(samples, 1U);
    return samples;
}

std::vector<float> render_two_cycles(tabletfx::OpticalTremoloWaveform waveform,
                                     tabletfx::OpticalTremoloVariant variant =
                                         tabletfx::OpticalTremoloVariant::Optical1) {
    tabletfx::OpticalTremoloParameters parameters;
    parameters.variant = variant;
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.lfo = waveform;
    parameters.duty_cycle_percent = 35.0F;
    parameters.mix = 1.0F;
    tabletfx::OpticalTremolo effect(8000.0F, parameters);
    std::vector<float> samples(16000U, 1.0F);
    effect.process(samples, 1U);
    return samples;
}

void test_defaults_bounds_and_memory() {
    tabletfx::OpticalTremolo effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "optical-tremolo",
            "optical tremolo stable ID changed");
    require(defaults.variant == tabletfx::OpticalTremoloVariant::Optical1 &&
                defaults.rate_hz == 4.5F && defaults.depth == 0.5F &&
                defaults.lfo == tabletfx::OpticalTremoloWaveform::Sine &&
                !defaults.tempo_sync && defaults.tempo_bpm == 120.0F &&
                defaults.duty_cycle_percent == 50.0F &&
                defaults.shape_percent == 50.0F && !defaults.trigger_enabled &&
                defaults.mix == 1.0F && defaults.level_db == 0.0F &&
                effect.effective_rate_hz() == 4.5F,
            "project-authored optical tremolo defaults changed");
    require(effect.memory_bytes() == sizeof(effect),
            "optical tremolo memory report is inaccurate");

    auto parameters = defaults;
    parameters.rate_hz = -1.0F;
    parameters.depth = 2.0F;
    parameters.duty_cycle_percent = -20.0F;
    parameters.shape_percent = 200.0F;
    parameters.phase_degrees = 400.0F;
    parameters.start_phase_degrees = -10.0F;
    parameters.trigger_threshold_dbfs = -100.0F;
    parameters.ducking_percent = 110.0F;
    parameters.mix = -1.0F;
    parameters.level_db = 20.0F;
    parameters.balance_percent = -150.0F;
    effect.set_parameters(parameters);
    const auto bounded = effect.parameters();
    require(bounded.rate_hz == 0.05F && bounded.depth == 1.0F &&
                bounded.duty_cycle_percent == 5.0F &&
                bounded.shape_percent == 100.0F &&
                bounded.phase_degrees == 360.0F &&
                bounded.start_phase_degrees == 0.0F &&
                bounded.trigger_threshold_dbfs == -90.0F &&
                bounded.ducking_percent == 100.0F && bounded.mix == 0.0F &&
                bounded.level_db == 12.0F && bounded.balance_percent == -100.0F,
            "numeric controls escaped project safety bounds");

    parameters = bounded;
    parameters.rate_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.depth = std::numeric_limits<float>::infinity();
    parameters.level_db = std::numeric_limits<float>::quiet_NaN();
    effect.set_parameters(parameters);
    const auto finite = effect.parameters();
    require(finite.rate_hz == 4.5F && finite.depth == 0.5F &&
                finite.level_db == 0.0F,
            "non-finite controls did not restore their safe defaults");
}

void test_variants_are_distinct_and_bounded() {
    const auto optical1 = render_one_cycle(
        tabletfx::OpticalTremoloVariant::Optical1);
    const auto optical2 = render_one_cycle(
        tabletfx::OpticalTremoloVariant::Optical2);
    const auto neon = render_one_cycle(tabletfx::OpticalTremoloVariant::Neon);
    const float difference12 = std::abs(optical1[1000] - optical2[1000]);
    require(difference12 > 0.04F,
            "Optical 1 and Optical 2 project curves became indistinguishable");
    require(neon[1000] > 0.99F && neon[4000] < 0.01F,
            "Neon project fixed-LFO pulse did not remain distinct");

    const auto [minimum, maximum] = std::minmax_element(optical1.begin(),
                                                        optical1.end());
    require(*minimum >= -1.0e-6F && *minimum < 0.01F &&
                *maximum > 0.99F && *maximum <= 1.0F,
            "full-depth optical modulation escaped its 0..1 gain range");
}

void test_all_lfo_shapes_and_deterministic_random_reset() {
    using tabletfx::OpticalTremoloWaveform;
    constexpr std::array<OpticalTremoloWaveform, 10U> shapes{
        OpticalTremoloWaveform::Sine,
        OpticalTremoloWaveform::Triangle,
        OpticalTremoloWaveform::Square,
        OpticalTremoloWaveform::SawUp,
        OpticalTremoloWaveform::SawDown,
        OpticalTremoloWaveform::Random,
        OpticalTremoloWaveform::Logarithmic,
        OpticalTremoloWaveform::Exponential,
        OpticalTremoloWaveform::Trapezoid,
        OpticalTremoloWaveform::Astable,
    };
    std::array<std::vector<float>, shapes.size()> rendered;
    for (std::size_t index = 0; index < shapes.size(); ++index) {
        rendered[index] = render_two_cycles(shapes[index]);
        require(std::all_of(rendered[index].begin(), rendered[index].end(),
                            [](float sample) {
                                return std::isfinite(sample) && sample >= 0.0F &&
                                       sample <= 1.0F;
                            }),
                "an LFO shape emitted an unbounded/non-finite tremolo gain");
    }
    for (std::size_t left = 0; left < rendered.size(); ++left) {
        for (std::size_t right = left + 1U; right < rendered.size(); ++right) {
            double difference = 0.0;
            for (std::size_t sample = 0; sample < rendered[left].size(); ++sample) {
                difference += std::abs(static_cast<double>(rendered[left][sample]) -
                                       static_cast<double>(rendered[right][sample]));
            }
            difference /= static_cast<double>(rendered[left].size());
            require(difference > 0.01,
                    "two documented LFO labels map to indistinguishable project shapes");
        }
    }

    tabletfx::OpticalTremoloParameters random_parameters;
    random_parameters.variant = tabletfx::OpticalTremoloVariant::Optical1;
    random_parameters.rate_hz = 1.0F;
    random_parameters.depth = 1.0F;
    random_parameters.lfo = OpticalTremoloWaveform::Random;
    random_parameters.mix = 1.0F;
    tabletfx::OpticalTremolo random_effect(8000.0F, random_parameters);
    std::vector<float> first(16000U, 1.0F);
    random_effect.process(first, 1U);
    random_effect.reset();
    std::vector<float> repeated(16000U, 1.0F);
    random_effect.process(repeated, 1U);
    require(first == repeated,
            "sample-hold random LFO sequence changed after deterministic reset");
    require(std::all_of(first.begin(), first.begin() + 8000,
                        [sample = first.front()](float current) {
                            return current == sample;
                        }) &&
                (first.front() != first[8000] || first.front() != first[8001]),
            "random LFO is not a bounded, cycle-held sample-and-hold");

    const auto neon_sine = render_two_cycles(OpticalTremoloWaveform::Sine,
                                              tabletfx::OpticalTremoloVariant::Neon);
    const auto neon_random = render_two_cycles(OpticalTremoloWaveform::Random,
                                                tabletfx::OpticalTremoloVariant::Neon);
    require(neon_sine == neon_random,
            "Neon variant must retain its fixed LFO regardless of editable LFO shape");
}

void test_lfo_tempo_mix_phase_and_reset() {
    tabletfx::OpticalTremoloParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.tempo_sync = true;
    parameters.tempo_bpm = 120.0F;
    parameters.beats_per_cycle = 0.5F;
    parameters.mix = 1.0F;
    tabletfx::OpticalTremolo effect(8000.0F, parameters);
    require(std::abs(effect.effective_rate_hz() - 4.0F) < 1.0e-6F,
            "tempo-synced eighth-note cycle at 120 BPM is not 4 Hz");

    parameters = effect.parameters();
    parameters.tempo_bpm = 300.0F;
    parameters.beats_per_cycle = 0.0625F;
    effect.set_parameters(parameters);
    require(effect.effective_rate_hz() == 20.0F,
            "tempo rate failed to respect the maximum oscillator bound");

    parameters = effect.parameters();
    parameters.tempo_sync = false;
    parameters.rate_hz = 1.0F;
    parameters.mix = 0.0F;
    parameters.start_phase_degrees = 90.0F;
    effect.configure(8000.0F, parameters);
    std::array<float, 8> unchanged{0.25F, -0.5F, 0.75F, -1.0F,
                                   0.125F, -0.25F, 0.4F, -0.2F};
    const auto original = unchanged;
    effect.process(unchanged, 2U);
    require(unchanged == original,
            "zero mix did not preserve the input exactly");

    parameters.mix = 1.0F;
    effect.configure(8000.0F, parameters);
    std::array<float, 32> first{};
    first.fill(1.0F);
    effect.process(first, 1U);
    effect.reset();
    std::array<float, 32> after_reset{};
    after_reset.fill(1.0F);
    effect.process(after_reset, 1U);
    require(first == after_reset,
            "reset did not restore the configured starting phase");
}

void test_stereo_link_finite_recovery_trigger_and_no_allocation() {
    tabletfx::OpticalTremoloParameters parameters;
    parameters.rate_hz = 7.0F;
    parameters.depth = 0.8F;
    parameters.ducking_percent = 35.0F;
    parameters.balance_percent = 0.0F;
    tabletfx::OpticalTremolo effect(48000.0F, parameters);
    std::vector<float> stereo(2048U, 0.25F);
    const auto allocation_before = allocation_count.load();
    effect.process(stereo, 2U);
    const auto allocation_after = allocation_count.load();
    require(allocation_before == allocation_after,
            "optical tremolo allocated during process()");
    for (std::size_t frame = 0; frame < stereo.size() / 2U; ++frame) {
        require(std::abs(stereo[frame * 2U] - stereo[frame * 2U + 1U]) < 1.0e-7F,
                "shared LFO phase diverged across stereo channels");
        require(std::isfinite(stereo[frame * 2U]) &&
                    std::abs(stereo[frame * 2U]) <= 0.25F + 1.0e-6F,
                "stereo output became non-finite or exceeded its input ceiling");
    }

    parameters = effect.parameters();
    parameters.trigger_enabled = true;
    parameters.trigger_threshold_dbfs = -18.0F;
    parameters.start_phase_degrees = 90.0F;
    effect.set_parameters(parameters);
    std::vector<float> triggered(1024U, 0.5F);
    effect.process(triggered, 1U);
    require(std::all_of(triggered.begin(), triggered.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "triggered optical tremolo produced a non-finite sample");

    std::array<float, 6> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
        0.5F, -0.25F, 0.125F};
    effect.process(invalid, 2U);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F &&
                invalid[2] == 0.0F && std::abs(invalid[3]) <= 0.5F &&
                std::all_of(invalid.begin(), invalid.end(), [](float sample) {
                    return std::isfinite(sample);
                }),
            "non-finite input did not recover to finite output");
}

void test_channel_and_block_edge_cases() {
    tabletfx::OpticalTremolo effect;
    std::array<float, 3> partial{0.2F, -0.3F, 0.7F};
    const auto original = partial;
    effect.process(partial, 0U);
    require(partial == original, "zero-channel process altered input");
    effect.process(partial, 2U);
    require(std::isfinite(partial[0]) && std::isfinite(partial[1]) &&
                partial[2] == original[2],
            "partial interleaved frame was treated as a complete frame");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
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
        test_defaults_bounds_and_memory();
        test_variants_are_distinct_and_bounded();
        test_all_lfo_shapes_and_deterministic_random_reset();
        test_lfo_tempo_mix_phase_and_reset();
        test_stereo_link_finite_recovery_trigger_and_no_allocation();
        test_channel_and_block_edge_cases();
        std::cout << "optical tremolo tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
