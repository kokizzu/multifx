#include "tabletfx/effects/enhancer.hpp"

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

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float actual, float expected, float tolerance = 2.0e-5F) {
    return std::abs(actual - expected) <= tolerance;
}

void test_defaults_and_bounded_controls() {
    tabletfx::Enhancer enhancer;
    const auto defaults = enhancer.parameters();
    require(enhancer.id() == "enhancer", "enhancer stable ID changed");
    require(defaults.mode == tabletfx::EnhancerMode::Modern &&
                defaults.width == 1.0F && defaults.depth == 0.25F &&
                defaults.low_cut_hz == 120.0F &&
                defaults.high_cut_hz == 12000.0F &&
                defaults.classic_delay_ms == 7.0F && defaults.mix == 1.0F,
            "safe enhancer defaults changed");
    require(enhancer.memory_bytes() < 64U * 1024U,
            "default enhancer delay storage should remain modest");

    tabletfx::EnhancerParameters values;
    values.width = 20.0F;
    values.depth = -1.0F;
    values.low_cut_hz = 50000.0F;
    values.high_cut_hz = 100.0F;
    values.classic_delay_ms = 50.0F;
    values.pan_left = -2.0F;
    values.pan_right = 3.0F;
    values.balance = 4.0F;
    values.mix = -1.0F;
    enhancer.set_parameters(values);
    const auto bounded = enhancer.parameters();
    require(bounded.width == 2.0F && bounded.depth == 0.0F &&
                bounded.low_cut_hz <= bounded.high_cut_hz - 20.0F &&
                bounded.high_cut_hz == 200.0F &&
                bounded.classic_delay_ms == 20.0F && bounded.pan_left == 0.0F &&
                bounded.pan_right == 1.0F && bounded.balance == 1.0F &&
                bounded.mix == 0.0F,
            "project-authored enhancer bounds were not enforced");

    values.width = std::numeric_limits<float>::quiet_NaN();
    values.depth = std::numeric_limits<float>::infinity();
    values.low_cut_hz = std::numeric_limits<float>::quiet_NaN();
    values.high_cut_hz = std::numeric_limits<float>::infinity();
    values.classic_delay_ms = std::numeric_limits<float>::quiet_NaN();
    values.pan_left = std::numeric_limits<float>::infinity();
    values.mix = std::numeric_limits<float>::quiet_NaN();
    enhancer.configure(values);
    const auto finite = enhancer.parameters();
    require(std::isfinite(finite.width) && std::isfinite(finite.depth) &&
                std::isfinite(finite.low_cut_hz) &&
                std::isfinite(finite.high_cut_hz) &&
                std::isfinite(finite.classic_delay_ms) &&
                std::isfinite(finite.pan_left) && std::isfinite(finite.mix),
            "non-finite enhancer controls were not replaced by safe defaults");
}

void test_modern_widens_selected_band_and_preserves_mono_sum() {
    tabletfx::EnhancerParameters parameters;
    parameters.mode = tabletfx::EnhancerMode::Modern;
    parameters.width = 1.0F;
    parameters.depth = 0.8F;
    parameters.low_cut_hz = 100.0F;
    parameters.high_cut_hz = 6000.0F;
    tabletfx::Enhancer enhancer(48000.0F, 2, parameters);

    std::array<float, 4096> audio{};
    for (std::size_t frame = 0; frame < audio.size() / 2U; ++frame) {
        const float sample = 0.4F * std::sin(
            2.0F * 3.14159265358979323846F * 900.0F *
            static_cast<float>(frame) / 48000.0F);
        audio[frame * 2U] = sample;
        audio[frame * 2U + 1U] = sample;
    }
    const auto original = audio;
    enhancer.process(audio, 2);

    double stereo_difference = 0.0;
    for (std::size_t frame = 0; frame < audio.size() / 2U; ++frame) {
        const float left = audio[frame * 2U];
        const float right = audio[frame * 2U + 1U];
        stereo_difference += std::abs(static_cast<double>(left - right));
        require(near(left + right, original[frame * 2U] * 2.0F, 1.0e-4F),
                "Modern side enhancement changed the mono fold-down");
    }
    require(stereo_difference > 1.0,
            "Modern depth did not create a frequency-dependent stereo difference");
}

void test_classic_right_delay_pan_and_phase() {
    tabletfx::EnhancerParameters parameters;
    parameters.mode = tabletfx::EnhancerMode::Classic;
    parameters.classic_delay_ms = 1.0F;
    parameters.mix = 1.0F;
    tabletfx::Enhancer enhancer(8000.0F, 2, parameters);
    std::array<float, 40> impulse{};
    impulse[0] = 0.75F;
    impulse[1] = 0.5F;
    enhancer.process(impulse, 2);
    require(near(impulse[0], 0.75F) && near(impulse[1], 0.0F),
            "Classic mode should preserve left now and delay right");
    require(near(impulse[17], 0.5F),
            "Classic right-channel delay did not appear at 1 ms");

    parameters.classic_delay_ms = 0.0F;
    parameters.pan_left = 1.0F;
    parameters.pan_right = 0.0F;
    parameters.phase_invert = tabletfx::EnhancerPhaseInvert::Left;
    enhancer.set_parameters(parameters);
    std::array<float, 4> swapped{-0.25F, 0.5F, 0.0F, 0.0F};
    enhancer.process(swapped, 2);
    require(near(swapped[0], 0.5F) && near(swapped[1], 0.25F),
            "Classic L/R pan or left phase inversion is incorrect");
}

void test_stereoizer_creates_stereo_and_reset_clears_state() {
    tabletfx::EnhancerParameters parameters;
    parameters.mode = tabletfx::EnhancerMode::Stereoizer;
    parameters.width = 1.0F;
    parameters.depth = 0.8F;
    parameters.mix = 1.0F;
    tabletfx::Enhancer enhancer(48000.0F, 2, parameters);
    std::array<float, 2048> tone{};
    for (std::size_t frame = 0; frame < tone.size() / 2U; ++frame) {
        const float sample = 0.3F * std::sin(
            2.0F * 3.14159265358979323846F * 440.0F *
            static_cast<float>(frame) / 48000.0F);
        tone[frame * 2U] = sample;
        tone[frame * 2U + 1U] = sample;
    }
    enhancer.process(tone, 2);
    double difference = 0.0;
    for (std::size_t frame = 0; frame < tone.size() / 2U; ++frame) {
        difference += std::abs(static_cast<double>(tone[frame * 2U] -
                                                   tone[frame * 2U + 1U]));
    }
    require(difference > 0.5,
            "Stereoizer mode did not produce a decorrelated stereo image");

    enhancer.reset();
    std::array<float, 128> silence{};
    enhancer.process(silence, 2);
    require(std::all_of(silence.begin(), silence.end(),
                        [](float sample) { return std::abs(sample) < 1.0e-7F; }),
            "reset should clear enhancer filter and delay history");
}

void test_mono_layout_mismatch_and_nonfinite_samples() {
    tabletfx::Enhancer mono(48000.0F, 1);
    std::array<float, 4> samples{0.1F, -0.2F, 0.3F, -0.4F};
    const auto original = samples;
    mono.process(samples, 1);
    require(samples == original,
            "mono in-place processing should remain pass-through");

    std::array<float, 4> mismatch{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before_mismatch = mismatch;
    mono.process(mismatch, 2);
    require(mismatch == before_mismatch,
            "mismatched channel layout should be left untouched");
    mono.process(mismatch, 0);
    require(mismatch == before_mismatch,
            "zero-channel processing should be left untouched");

    tabletfx::Enhancer stereo;
    std::array<float, 6> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 0.1F,
        -std::numeric_limits<float>::infinity(), 0.2F, -0.3F};
    stereo.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "enhancer emitted non-finite output from invalid input");
}

void test_processing_allocates_nothing() {
    tabletfx::Enhancer enhancer;
    std::array<float, 512> audio{};
    audio.fill(0.2F);
    allocation_count.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    for (int iteration = 0; iteration < 64; ++iteration) {
        enhancer.process(audio, 2);
    }
    count_allocations.store(false, std::memory_order_release);
    require(allocation_count.load(std::memory_order_relaxed) == 0,
            "enhancer process() allocated memory");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
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
        test_defaults_and_bounded_controls();
        test_modern_widens_selected_band_and_preserves_mono_sum();
        test_classic_right_delay_pan_and_phase();
        test_stereoizer_creates_stereo_and_reset_clears_state();
        test_mono_layout_mismatch_and_nonfinite_samples();
        test_processing_allocates_nothing();
        std::cout << "All TabletFX enhancer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
