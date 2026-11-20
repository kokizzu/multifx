#include "tabletfx/effects/compressor.hpp"

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

tabletfx::CompressorParameters hard_knee_settings() {
    tabletfx::CompressorParameters parameters;
    parameters.threshold_db = -24.0F;
    parameters.ratio = 4.0F;
    parameters.knee_db = 0.0F;
    parameters.attack_ms = 0.1F;
    parameters.release_ms = 50.0F;
    parameters.makeup_db = 0.0F;
    return parameters;
}

void test_steady_state_transfer_curve() {
    tabletfx::Compressor compressor(48000.0F, hard_knee_settings());
    std::vector<float> samples(48000, 0.5F);
    compressor.process(samples, 1);

    const float input_db = 20.0F * std::log10(0.5F);
    const float expected_db = -24.0F + (input_db + 24.0F) / 4.0F;
    const float expected = std::pow(10.0F, expected_db / 20.0F);
    require(std::abs(samples.back() - expected) < 2.0e-3F,
            "steady-state output does not follow the configured compression ratio");
}

void test_stereo_link_preserves_relative_image() {
    tabletfx::Compressor compressor(48000.0F, hard_knee_settings());
    std::vector<float> samples(48000 * 2, 0.0F);
    for (std::size_t frame = 0; frame < samples.size() / 2; ++frame) {
        samples[frame * 2] = 0.25F;
        samples[frame * 2 + 1] = 0.5F;
    }
    compressor.process(samples, 2);

    const float left_gain = samples[samples.size() - 2] / 0.25F;
    const float right_gain = samples.back() / 0.5F;
    require(left_gain < 0.3F, "the louder channel did not trigger linked compression");
    require(std::abs(left_gain - right_gain) < 1.0e-5F,
            "stereo channels did not receive the same gain reduction");
}

void test_defaults_bounds_reset_and_nonfinite_input() {
    tabletfx::Compressor compressor;
    const auto defaults = compressor.parameters();
    require(defaults.threshold_db == -18.0F && defaults.ratio == 3.0F &&
            defaults.knee_db == 6.0F && defaults.attack_ms == 8.0F &&
            defaults.release_ms == 100.0F && defaults.makeup_db == 0.0F,
            "default guitar compressor settings changed unexpectedly");

    tabletfx::CompressorParameters extreme;
    extreme.threshold_db = -100.0F;
    extreme.ratio = 100.0F;
    extreme.knee_db = -1.0F;
    extreme.attack_ms = 0.0F;
    extreme.release_ms = 1.0F;
    extreme.makeup_db = 30.0F;
    compressor.configure(-1.0F, extreme);
    const auto bounded = compressor.parameters();
    require(bounded.threshold_db == -60.0F && bounded.ratio == 20.0F &&
            bounded.knee_db == 0.0F && bounded.attack_ms == 0.1F &&
            bounded.release_ms == 5.0F && bounded.makeup_db == 18.0F,
            "out-of-range compressor parameters were not bounded");

    tabletfx::CompressorParameters invalid;
    invalid.threshold_db = std::numeric_limits<float>::quiet_NaN();
    invalid.ratio = std::numeric_limits<float>::infinity();
    invalid.knee_db = 0.0F;
    invalid.attack_ms = 2.0F;
    invalid.release_ms = 50.0F;
    invalid.makeup_db = 0.0F;
    compressor.set_parameters(invalid);
    require(compressor.parameters().threshold_db == -18.0F &&
            compressor.parameters().ratio == 3.0F,
            "non-finite compressor parameters did not fall back to sane defaults");

    std::array<float, 2048> loud{};
    loud.fill(0.8F);
    compressor.process(loud, 1);
    compressor.reset();
    std::array<float, 2> quiet{0.05F, 0.05F};
    compressor.process(quiet, 1);
    require(std::abs(quiet.back() - 0.05F) < 1.0e-4F,
            "reset did not clear the compressor detector and gain state");

    std::array<float, 3> invalid_audio{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()};
    compressor.process(invalid_audio, 1);
    for (float sample : invalid_audio) {
        require(std::isfinite(sample), "non-finite input escaped the compressor");
    }
}

void test_variable_block_sizes_and_no_process_allocations() {
    tabletfx::Compressor compressor(44100.0F, hard_knee_settings());
    require(compressor.memory_bytes() == sizeof(compressor),
            "compressor should use fixed in-object state only");
    std::array<float, 1> one_frame{0.2F};
    std::array<float, 7> irregular_block{0.1F, -0.1F, 0.2F, -0.2F,
                                          0.3F, -0.3F, 0.4F};
    std::array<float, 256 * 2> stereo_block{};
    stereo_block.fill(0.2F);

    compressor.process(one_frame, 1);
    compressor.process(irregular_block, 1);
    compressor.process(stereo_block, 2);

    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        compressor.process(one_frame, 1);
        compressor.process(irregular_block, 1);
        compressor.process(stereo_block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "compressor allocated memory during process()");
    for (float sample : stereo_block) {
        require(std::isfinite(sample), "compressor emitted a non-finite sample");
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
        test_steady_state_transfer_curve();
        test_stereo_link_preserves_relative_image();
        test_defaults_bounds_reset_and_nonfinite_input();
        test_variable_block_sizes_and_no_process_allocations();
        std::cout << "All TabletFX compressor tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
