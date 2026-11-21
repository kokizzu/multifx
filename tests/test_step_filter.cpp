#include "tabletfx/effects/step_filter.hpp"

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
    if (!condition) {
        throw std::runtime_error(message);
    }
}

tabletfx::StepFilterParameters static_filter_parameters() {
    tabletfx::StepFilterParameters parameters;
    parameters.tempo_sync = false;
    parameters.rate_hz = 8.0F;
    parameters.mode = tabletfx::StepFilterMode::LowPass;
    parameters.minimum_cutoff_hz = 600.0F;
    parameters.maximum_cutoff_hz = 600.0F;
    parameters.resonance_q = 0.707F;
    parameters.depth = 0.0F;
    parameters.shape = 0.0F;
    parameters.smoothing_ms = 0.0F;
    parameters.mix = 1.0F;
    parameters.steps.fill(0.5F);
    return parameters;
}

double render_tone_rms(tabletfx::StepFilterParameters parameters,
                       float frequency_hz, std::uint32_t sample_rate = 48000U) {
    constexpr std::size_t frames = 24000U;
    constexpr std::size_t skip = 4000U;
    std::vector<float> audio(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        audio[frame] = 0.5F * std::sin(
            2.0F * 3.14159265358979323846F * frequency_hz *
            static_cast<float>(frame) / static_cast<float>(sample_rate));
    }
    tabletfx::StepFilter filter(static_cast<float>(sample_rate), parameters);
    filter.process(audio, 1);
    double sum = 0.0;
    for (std::size_t frame = skip; frame < frames; ++frame) {
        const double sample = audio[frame];
        sum += sample * sample;
    }
    return std::sqrt(sum / static_cast<double>(frames - skip));
}

void test_defaults_bounds_and_fixed_memory() {
    tabletfx::StepFilter filter;
    const auto defaults = filter.parameters();
    require(filter.id() == "step-filter", "stable ID changed");
    require(defaults.tempo_sync && defaults.tempo_bpm == 120.0F &&
                defaults.division == tabletfx::RhythmicDivision::Sixteenth &&
                defaults.minimum_cutoff_hz == 120.0F &&
                defaults.maximum_cutoff_hz == 6000.0F &&
                defaults.resonance_q == 0.707F && defaults.depth == 1.0F &&
                defaults.steps.size() == 16U,
            "project-authored Step Filter defaults changed");
    require(filter.memory_bytes() == sizeof(filter),
            "Step Filter memory report is inaccurate");

    auto parameters = defaults;
    parameters.tempo_bpm = 999.0F;
    parameters.division = static_cast<tabletfx::RhythmicDivision>(255U);
    parameters.rate_hz = -2.0F;
    parameters.mode = static_cast<tabletfx::StepFilterMode>(255U);
    parameters.minimum_cutoff_hz = 90000.0F;
    parameters.maximum_cutoff_hz = 2.0F;
    parameters.resonance_q = 100.0F;
    parameters.depth = -1.0F;
    parameters.shape = 3.0F;
    parameters.smoothing_ms = -1.0F;
    parameters.mix = 3.0F;
    parameters.phase = std::numeric_limits<float>::quiet_NaN();
    parameters.steps[0] = std::numeric_limits<float>::infinity();
    tabletfx::StepFilter bounded(8000.0F, parameters);
    const auto sanitized = bounded.parameters();
    require(sanitized.tempo_bpm == 300.0F &&
                sanitized.division == tabletfx::RhythmicDivision::Sixteenth &&
                sanitized.rate_hz == 0.05F &&
                sanitized.mode == tabletfx::StepFilterMode::LowPass &&
                sanitized.minimum_cutoff_hz == 3200.0F &&
                sanitized.maximum_cutoff_hz == 3200.0F &&
                sanitized.resonance_q == 12.0F && sanitized.depth == 0.0F &&
                sanitized.shape == 1.0F && sanitized.smoothing_ms == 0.0F &&
                sanitized.mix == 1.0F && sanitized.phase == 0.0F &&
                sanitized.steps[0] == 0.5F,
            "unsafe or non-finite controls were not bounded");
    require(bounded.memory_bytes() == sizeof(bounded),
            "configuration changed the fixed storage size");
}

void test_tempo_timing_pattern_and_shape() {
    auto parameters = static_filter_parameters();
    parameters.tempo_sync = true;
    parameters.tempo_bpm = 120.0F;
    parameters.division = tabletfx::RhythmicDivision::Sixteenth;
    parameters.minimum_cutoff_hz = 100.0F;
    parameters.maximum_cutoff_hz = 1600.0F;
    parameters.depth = 1.0F;
    parameters.shape = 0.0F;
    parameters.steps.fill(0.0F);
    parameters.steps[1] = 1.0F;
    tabletfx::StepFilter filter(8000.0F, parameters);
    require(std::abs(filter.effective_step_rate_hz() - 8.0F) < 1.0e-6F &&
                std::abs(filter.current_cutoff_hz() - 100.0F) < 1.0e-3F,
            "120 BPM sixteenth-step timing/default cutoff is wrong");
    std::array<float, 1001> silence{};
    filter.process(silence, 1);
    require(std::abs(filter.current_cutoff_hz() - 1600.0F) < 0.1F,
            "the pattern did not advance to its second cutoff step at the tempo boundary");

    parameters.shape = 1.0F;
    tabletfx::StepFilter smooth_pattern(8000.0F, parameters);
    std::array<float, 501> quiet{};
    smooth_pattern.process(quiet, 1);
    require(smooth_pattern.current_cutoff_hz() > 200.0F &&
                smooth_pattern.current_cutoff_hz() < 700.0F,
            "full-step easing did not interpolate the cutoff in logarithmic space");

    parameters.division = tabletfx::RhythmicDivision::EighthTriplet;
    parameters.tempo_bpm = 120.0F;
    smooth_pattern.set_parameters(parameters);
    require(std::abs(smooth_pattern.effective_step_rate_hz() - 6.0F) < 1.0e-5F,
            "tempo subdivision did not update the step rate");
}

void test_filter_responses_and_mix() {
    auto parameters = static_filter_parameters();
    parameters.minimum_cutoff_hz = 600.0F;
    parameters.maximum_cutoff_hz = 600.0F;
    parameters.depth = 0.0F;
    parameters.resonance_q = 0.707F;
    parameters.mode = tabletfx::StepFilterMode::LowPass;
    const double lowpass_below = render_tone_rms(parameters, 100.0F);
    const double lowpass_above = render_tone_rms(parameters, 6000.0F);
    require(lowpass_below > 0.16 && lowpass_above < 0.04,
            "low-pass mode cutoff response is incorrect");

    parameters.mode = tabletfx::StepFilterMode::HighPass;
    const double highpass_below = render_tone_rms(parameters, 100.0F);
    const double highpass_above = render_tone_rms(parameters, 6000.0F);
    require(highpass_below < 0.04 && highpass_above > 0.30,
            "high-pass mode cutoff response is incorrect");

    parameters.mode = tabletfx::StepFilterMode::BandPass;
    parameters.resonance_q = 4.0F;
    const double bandpass_near = render_tone_rms(parameters, 600.0F);
    const double bandpass_far = render_tone_rms(parameters, 100.0F);
    require(bandpass_near > bandpass_far * 2.0,
            "band-pass mode did not emphasize its configured cutoff");

    parameters.mode = tabletfx::StepFilterMode::Notch;
    const double notch_mid = render_tone_rms(parameters, 600.0F);
    require(std::isfinite(notch_mid) && notch_mid >= 0.0,
            "notch mode emitted invalid output");

    parameters = static_filter_parameters();
    parameters.mix = 0.0F;
    tabletfx::StepFilter dry_filter(48000.0F, parameters);
    std::array<float, 8> dry{0.125F, -0.25F, 0.5F, -0.75F,
                             0.9F, -0.8F, 0.3F, -0.1F};
    const auto original = dry;
    dry_filter.process(dry, 2);
    require(dry == original, "zero mix did not pass finite audio through exactly");
}

void test_stereo_isolation_layout_and_unsupported_channels() {
    auto parameters = static_filter_parameters();
    tabletfx::StepFilter stereo(48000.0F, parameters);
    std::array<float, 512> audio{};
    for (std::size_t frame = 0; frame < audio.size() / 2U; ++frame) {
        audio[frame * 2U] = 0.5F;
        audio[frame * 2U + 1U] = 0.0F;
    }
    stereo.process(audio, 2);
    require(audio[0] != 0.0F, "left channel did not produce filtered output");
    for (std::size_t frame = 0; frame < audio.size() / 2U; ++frame) {
        require(audio[frame * 2U + 1U] == 0.0F,
                "right-channel zero input received cross-channel signal");
    }

    std::array<float, 4> partial{0.25F, -0.5F, 0.75F, -0.125F};
    const auto unchanged = partial;
    stereo.process(partial, 3);
    require(partial == unchanged, "unsupported channel layouts must be untouched");
    stereo.process(partial, 0);
    require(partial == unchanged, "zero-channel input must be untouched");
}

void test_variable_block_invariance_reset_and_finite_recovery() {
    auto parameters = static_filter_parameters();
    parameters.tempo_sync = true;
    parameters.tempo_bpm = 137.0F;
    parameters.division = tabletfx::RhythmicDivision::EighthTriplet;
    parameters.minimum_cutoff_hz = 100.0F;
    parameters.maximum_cutoff_hz = 9000.0F;
    parameters.resonance_q = 5.0F;
    parameters.depth = 1.0F;
    parameters.shape = 0.43F;
    parameters.smoothing_ms = 2.3F;
    parameters.steps = {0.1F, 0.8F, 0.3F, 1.0F, 0.0F, 0.4F, 0.6F, 0.2F,
                        0.9F, 0.2F, 0.7F, 0.05F, 0.5F, 0.3F, 1.0F, 0.0F};
    tabletfx::StepFilter whole(44100.0F, parameters);
    tabletfx::StepFilter chunked(44100.0F, parameters);
    std::vector<float> expected(16000U);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        expected[index] = 0.6F * std::sin(static_cast<float>(index) * 0.013F);
    }
    auto actual = expected;
    whole.process(expected, 2);
    const std::array<std::size_t, 6> block_sizes{1U, 127U, 53U, 300U, 17U, 89U};
    std::size_t frame = 0;
    std::size_t block = 0;
    while (frame < actual.size() / 2U) {
        const std::size_t frames = std::min(
            block_sizes[block % block_sizes.size()], actual.size() / 2U - frame);
        chunked.process(std::span<float>(actual).subspan(frame * 2U, frames * 2U), 2);
        frame += frames;
        ++block;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(std::abs(expected[index] - actual[index]) < 2.0e-6F,
                "variable block sizes changed filter output");
    }

    whole.reset();
    chunked.reset();
    std::array<float, 64> reset_a{};
    std::array<float, 64> reset_b{};
    reset_a.fill(0.5F);
    reset_b.fill(0.5F);
    whole.process(reset_a, 1);
    chunked.process(reset_b, 1);
    require(reset_a == reset_b, "reset did not restore phase and filter memory");

    tabletfx::StepFilter bad_input(48000.0F, parameters);
    std::array<float, 4> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
    };
    bad_input.process(invalid, 1);
    require(std::all_of(invalid.begin(), invalid.end(), [](float value) {
                return std::isfinite(value);
            }),
            "non-finite or extreme finite input poisoned the filter output");
    bad_input.reset();
    tabletfx::StepFilter clean(48000.0F, parameters);
    clean.reset();
    std::array<float, 64> recovered{};
    std::array<float, 64> reference{};
    recovered.fill(0.25F);
    reference.fill(0.25F);
    bad_input.process(recovered, 1);
    clean.process(reference, 1);
    require(recovered == reference,
            "reset after invalid/extreme input did not restore deterministic audio");
}

void test_no_process_allocations() {
    auto parameters = static_filter_parameters();
    tabletfx::StepFilter filter(48000.0F, parameters);
    std::array<float, 512> audio{};
    audio.fill(0.25F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        filter.process(audio, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "Step Filter allocated during process()");
    require(filter.memory_bytes() == sizeof(filter),
            "Step Filter memory grew while processing");
    require(std::all_of(audio.begin(), audio.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "Step Filter produced non-finite output");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
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
        test_defaults_bounds_and_fixed_memory();
        test_tempo_timing_pattern_and_shape();
        test_filter_responses_and_mix();
        test_stereo_isolation_layout_and_unsupported_channels();
        test_variable_block_invariance_reset_and_finite_recovery();
        test_no_process_allocations();
        std::cout << "All TabletFX Step Filter tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
