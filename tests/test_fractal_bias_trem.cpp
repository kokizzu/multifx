#include "tabletfx/effects/fractal_bias_trem.hpp"

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

void test_defaults_bounds_memory_and_latency() {
    tabletfx::BiasTrem trem;
    const auto parameters = trem.parameters();
    require(trem.id() == "fractal-bias-trem",
            "project-authored Bias Trem ID changed");
    require(parameters.rate_hz == 4.5F && parameters.depth == 0.35F &&
                parameters.waveform == tabletfx::BiasTremWaveform::Sine &&
                !parameters.tempo_sync && !parameters.trigger_sync &&
                parameters.ducking == 0.0F && parameters.bias_drive == 0.18F &&
                parameters.mix == 1.0F && parameters.level_db == 0.0F &&
                parameters.balance == 0.0F && !parameters.bypass,
            "project-authored safe defaults changed");
    require(trem.memory_bytes() == sizeof(trem) && sizeof(trem) <= 256,
            "Bias Trem memory report is inaccurate or unexpectedly large");
    require(trem.latency_frames() == 0,
            "Bias Trem must have zero fixed latency");

    tabletfx::BiasTremParameters out_of_range;
    out_of_range.rate_hz = -1.0F;
    out_of_range.depth = 2.0F;
    out_of_range.waveform = static_cast<tabletfx::BiasTremWaveform>(255);
    out_of_range.tempo_bpm = 500.0F;
    out_of_range.beats_per_cycle = 0.0F;
    out_of_range.trigger_threshold_db = -100.0F;
    out_of_range.ducking = 2.0F;
    out_of_range.bias_drive = -0.5F;
    out_of_range.mix = 2.0F;
    out_of_range.level_db = 20.0F;
    out_of_range.balance = -2.0F;
    trem.configure(48000.0F, out_of_range);
    const auto bounded = trem.parameters();
    require(bounded.rate_hz == 0.05F && bounded.depth == 1.0F &&
                bounded.waveform == tabletfx::BiasTremWaveform::Sine &&
                bounded.tempo_bpm == 300.0F &&
                bounded.beats_per_cycle == 0.0625F &&
                bounded.trigger_threshold_db == -80.0F &&
                bounded.ducking == 1.0F && bounded.bias_drive == 0.0F &&
                bounded.mix == 1.0F && bounded.level_db == 12.0F &&
                bounded.balance == -1.0F,
            "Bias Trem controls were not clamped to documented project bounds");

    auto non_finite = bounded;
    non_finite.depth = std::numeric_limits<float>::quiet_NaN();
    non_finite.level_db = std::numeric_limits<float>::infinity();
    trem.set_parameters(non_finite);
    require(trem.parameters().depth == bounded.depth &&
                trem.parameters().level_db == bounded.level_db,
            "non-finite control values did not preserve the last valid settings");
}

void test_modulation_transfer_and_asymmetric_drive() {
    tabletfx::BiasTremParameters modulation;
    modulation.rate_hz = 1.0F;
    modulation.depth = 1.0F;
    modulation.bias_drive = 0.0F;
    tabletfx::BiasTrem trem(8000.0F, modulation);
    std::array<float, 8000> constant{};
    constant.fill(0.5F);
    trem.process(constant, 1);
    const auto limits = std::minmax_element(constant.begin(), constant.end());
    require(*limits.first < 0.002F && *limits.second > 0.49F &&
                *limits.second <= 0.5F,
            "sine bias trem output range was " + std::to_string(*limits.first) +
                ".." + std::to_string(*limits.second) +
                ", expected approximately 0..0.5");

    tabletfx::BiasTremParameters nonlinear;
    nonlinear.rate_hz = 1.0F;
    nonlinear.depth = 0.0F;
    nonlinear.bias_drive = 1.0F;
    std::array<float, 2001> positive{};
    std::array<float, 2001> negative{};
    positive.back() = 0.7F;
    negative.back() = -0.7F;
    tabletfx::BiasTrem positive_trem(8000.0F, nonlinear);
    tabletfx::BiasTrem negative_trem(8000.0F, nonlinear);
    positive_trem.process(positive, 1);
    negative_trem.process(negative, 1);
    require(std::abs(positive.back()) != std::abs(negative.back()),
            "LFO-varying bias curve did not produce a polarity asymmetry");

    nonlinear.bias_drive = 0.05F;
    tabletfx::BiasTrem bounded_curve(8000.0F, nonlinear);
    std::array<float, 1> excessive{1000000.0F};
    bounded_curve.process(excessive, 1);
    require(std::isfinite(excessive[0]) && std::abs(excessive[0]) <= 4.0F,
            "large finite input escaped the processor's output bound");
}

void test_tempo_ducking_trigger_and_mix_controls() {
    tabletfx::BiasTremParameters parameters;
    parameters.tempo_sync = true;
    parameters.tempo_bpm = 120.0F;
    parameters.beats_per_cycle = 0.5F;
    tabletfx::BiasTrem tempo(48000.0F, parameters);
    require(std::abs(tempo.effective_rate_hz() - 4.0F) < 1.0e-6F,
            "tempo sync did not convert quarter-note BPM to cycle rate");

    parameters = {};
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.waveform = tabletfx::BiasTremWaveform::Square;
    parameters.bias_drive = 0.0F;
    parameters.ducking = 1.0F;
    tabletfx::BiasTrem ducked(8000.0F, parameters);
    std::array<float, 8000> loud{};
    loud.fill(1.0F);
    ducked.process(loud, 1);
    require(loud.back() > 0.95F,
            "input-level ducking did not reduce tremolo depth on loud input");

    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.ducking = 0.0F;
    parameters.trigger_sync = true;
    parameters.trigger_threshold_db = -20.0F;
    tabletfx::BiasTrem triggered(8000.0F, parameters);
    std::array<float, 4000> silent{};
    triggered.process(silent, 1);
    std::array<float, 1> onset{0.25F};
    triggered.process(onset, 1);
    require(std::abs(onset[0] - 0.25F) < 1.0e-6F,
            "rising input above threshold did not restart LFO phase");

    parameters = {};
    parameters.depth = 1.0F;
    parameters.bias_drive = 0.0F;
    parameters.mix = 0.0F;
    parameters.level_db = 0.0F;
    tabletfx::BiasTrem dry(48000.0F, parameters);
    std::array<float, 4> dry_samples{0.25F, -0.5F, 0.75F, -1.0F};
    const auto original = dry_samples;
    dry.process(dry_samples, 2);
    require(dry_samples == original,
            "zero wet mix should preserve the dry stereo signal");

    parameters.level_db = 6.0F;
    parameters.balance = -1.0F;
    tabletfx::BiasTrem output_controls(48000.0F, parameters);
    std::array<float, 2> balanced{0.25F, 0.25F};
    output_controls.process(balanced, 2);
    require(std::abs(balanced[0] - 0.25F * std::pow(10.0F, 6.0F / 20.0F)) <
                    1.0e-6F && balanced[1] == 0.0F,
            "level and stereo balance controls did not affect the output as specified");

    parameters.bypass = true;
    tabletfx::BiasTrem bypassed(48000.0F, parameters);
    std::array<float, 4> bypass_samples{0.25F, -0.5F, 0.75F, -1.0F};
    const auto bypass_original = bypass_samples;
    bypassed.process(bypass_samples, 2);
    require(bypass_samples == bypass_original,
            "bypass should preserve the original input");
}

void test_stereo_link_reset_and_irregular_blocks() {
    tabletfx::BiasTremParameters parameters;
    parameters.rate_hz = 7.3F;
    parameters.depth = 0.73F;
    parameters.bias_drive = 0.27F;
    tabletfx::BiasTrem whole(44100.0F, parameters);
    tabletfx::BiasTrem chunked(44100.0F, parameters);
    std::vector<float> expected(8194);
    for (std::size_t i = 0; i < expected.size(); i += 2) {
        const float sample = 0.4F * std::sin(static_cast<float>(i) * 0.013F);
        expected[i] = sample;
        expected[i + 1] = sample;
    }
    auto actual = expected;
    whole.process(expected, 2);
    const std::array<std::size_t, 5> block_sizes{1, 127, 53, 300, 17};
    std::size_t frame = 0;
    std::size_t block = 0;
    while (frame < actual.size() / 2) {
        const std::size_t frames = std::min(block_sizes[block % block_sizes.size()],
                                            actual.size() / 2 - frame);
        chunked.process(std::span<float>(actual).subspan(frame * 2, frames * 2), 2);
        frame += frames;
        ++block;
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        require(std::abs(expected[i] - actual[i]) < 1.0e-7F,
                "irregular block sizes changed LFO, detector, or output state");
        require(std::abs(expected[i] - expected[i ^ 1U]) < 1.0e-7F,
                "stereo channels did not share modulation and detector state");
    }

    whole.reset();
    chunked.reset();
    std::array<float, 8> after_reset_a{};
    std::array<float, 8> after_reset_b{};
    after_reset_a.fill(0.6F);
    after_reset_b.fill(0.6F);
    whole.process(after_reset_a, 2);
    chunked.process(after_reset_b, 2);
    require(after_reset_a == after_reset_b,
            "reset did not restore deterministic oscillator and detector state");

    parameters.waveform = tabletfx::BiasTremWaveform::SampleHold;
    tabletfx::BiasTrem random_a(8000.0F, parameters);
    tabletfx::BiasTrem random_b(8000.0F, parameters);
    std::array<float, 32000> random_audio_a{};
    std::array<float, 32000> random_audio_b{};
    random_audio_a.fill(0.4F);
    random_audio_b.fill(0.4F);
    random_a.process(random_audio_a, 1);
    random_b.process(random_audio_b, 1);
    require(random_audio_a == random_audio_b,
            "sample-and-hold LFO was not reproducible across equal resets");
}

void test_invalid_input_recovery_and_zero_allocations() {
    tabletfx::BiasTrem trem;
    std::array<float, 6> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.25F,
        -0.5F,
        0.125F,
    };
    trem.process(invalid, 1);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F && invalid[2] == 0.0F &&
                std::isfinite(invalid[3]) && std::isfinite(invalid[4]) &&
                std::isfinite(invalid[5]),
            "non-finite samples were not silenced and followed by finite recovery");

    std::array<float, 4> untouched{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before_zero_channels = untouched;
    trem.process(untouched, 0);
    require(untouched == before_zero_channels,
            "zero-channel processing should leave the buffer untouched");

    std::array<float, 512> block{};
    block.fill(0.5F);
    trem.reset();
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        trem.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "Bias Trem allocated memory during process()");
    for (float sample : block) {
        require(std::isfinite(sample), "Bias Trem emitted non-finite audio");
    }
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
        test_defaults_bounds_memory_and_latency();
        test_modulation_transfer_and_asymmetric_drive();
        test_tempo_ducking_trigger_and_mix_controls();
        test_stereo_link_reset_and_irregular_blocks();
        test_invalid_input_recovery_and_zero_allocations();
        std::cout << "All TabletFX project-authored Bias Trem tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
