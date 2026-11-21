#include "tabletfx/effects/fet_compressor.hpp"

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

tabletfx::FetCompressorParameters hard_knee_settings() {
    tabletfx::FetCompressorParameters parameters;
    parameters.threshold_db = -24.0F;
    parameters.ratio = 4.0F;
    parameters.knee_db = 0.0F;
    parameters.attack_ms = 0.05F;
    parameters.release_ms = 50.0F;
    parameters.makeup_db = 0.0F;
    return parameters;
}

void test_steady_state_transfer_curve() {
    tabletfx::FetCompressor compressor(48000.0F, hard_knee_settings());
    std::vector<float> samples(48000, 0.5F);
    compressor.process(samples, 1);

    const float input_db = 20.0F * std::log10(0.5F);
    const float expected_db = -24.0F + (input_db + 24.0F) / 4.0F;
    const float expected = std::pow(10.0F, expected_db / 20.0F);
    require(std::abs(samples.back() - expected) < 2.0e-3F,
            "steady-state peak output does not follow the configured ratio");
}

void test_attack_preserves_more_of_a_transient_when_slower() {
    auto fast_parameters = hard_knee_settings();
    fast_parameters.attack_ms = 0.05F;
    auto slow_parameters = fast_parameters;
    slow_parameters.attack_ms = 10.0F;
    tabletfx::FetCompressor fast(48000.0F, fast_parameters);
    tabletfx::FetCompressor slow(48000.0F, slow_parameters);

    std::array<float, 1> fast_impulse{1.0F};
    std::array<float, 1> slow_impulse{1.0F};
    fast.process(fast_impulse, 1);
    slow.process(slow_impulse, 1);
    require(fast_impulse[0] < 0.9F,
            "fast peak detection did not reduce the initial transient");
    require(slow_impulse[0] > fast_impulse[0] + 0.1F,
            "a slower attack did not preserve more of the initial transient");
}

void test_stereo_link_preserves_relative_image() {
    tabletfx::FetCompressor compressor(48000.0F, hard_knee_settings());
    std::vector<float> samples(12000 * 2, 0.0F);
    for (std::size_t frame = 0; frame < samples.size() / 2; ++frame) {
        samples[frame * 2] = 0.25F;
        samples[frame * 2 + 1] = 0.5F;
    }
    compressor.process(samples, 2);

    const float left_gain = samples[samples.size() - 2] / 0.25F;
    const float right_gain = samples.back() / 0.5F;
    require(left_gain < 0.5F,
            "the louder channel did not trigger linked peak compression");
    require(std::abs(left_gain - right_gain) < 1.0e-5F,
            "stereo channels did not receive the same gain reduction");
}

void test_release_control_and_reset() {
    auto fast_parameters = hard_knee_settings();
    fast_parameters.release_ms = 5.0F;
    auto slow_parameters = fast_parameters;
    slow_parameters.release_ms = 300.0F;
    tabletfx::FetCompressor fast(48000.0F, fast_parameters);
    tabletfx::FetCompressor slow(48000.0F, slow_parameters);

    std::vector<float> fast_loud(48000, 0.8F);
    std::vector<float> slow_loud(48000, 0.8F);
    fast.process(fast_loud, 1);
    slow.process(slow_loud, 1);
    std::vector<float> fast_recovery(2400, 0.01F);
    std::vector<float> slow_recovery(2400, 0.01F);
    fast.process(fast_recovery, 1);
    slow.process(slow_recovery, 1);
    require(fast_recovery.back() > slow_recovery.back() * 1.5F,
            "a shorter release did not recover faster after sustained compression");

    fast.reset();
    std::array<float, 1> quiet{0.05F};
    fast.process(quiet, 1);
    require(std::abs(quiet[0] - 0.05F) < 1.0e-4F,
            "reset did not clear the peak detector and gain envelope");
}

void test_defaults_bounds_and_nonfinite_recovery() {
    tabletfx::FetCompressor compressor;
    const auto defaults = compressor.parameters();
    require(defaults.threshold_db == -24.0F && defaults.ratio == 8.0F &&
                defaults.knee_db == 2.0F && defaults.attack_ms == 0.1F &&
                defaults.release_ms == 50.0F && defaults.makeup_db == 0.0F,
            "fast peak compressor defaults changed unexpectedly");
    require(compressor.memory_bytes() == sizeof(compressor),
            "the FET-style compressor should use fixed in-object state");

    tabletfx::FetCompressorParameters extreme;
    extreme.threshold_db = -100.0F;
    extreme.ratio = 100.0F;
    extreme.knee_db = 100.0F;
    extreme.attack_ms = 0.0F;
    extreme.release_ms = 0.0F;
    extreme.makeup_db = 30.0F;
    compressor.configure(-1.0F, extreme);
    const auto bounded = compressor.parameters();
    require(bounded.threshold_db == -60.0F && bounded.ratio == 20.0F &&
                bounded.knee_db == 18.0F && bounded.attack_ms == 0.01F &&
                bounded.release_ms == 1.0F && bounded.makeup_db == 18.0F,
            "out-of-range FET-style controls were not bounded");

    tabletfx::FetCompressorParameters invalid;
    invalid.threshold_db = std::numeric_limits<float>::quiet_NaN();
    invalid.ratio = std::numeric_limits<float>::infinity();
    invalid.knee_db = 0.0F;
    invalid.attack_ms = 0.5F;
    invalid.release_ms = 100.0F;
    invalid.makeup_db = 0.0F;
    compressor.set_parameters(invalid);
    require(compressor.parameters().threshold_db == -24.0F &&
                compressor.parameters().ratio == 8.0F,
            "non-finite controls did not fall back to sane defaults");

    std::array<float, 4> invalid_audio{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max()};
    compressor.process(invalid_audio, 1);
    for (float sample : invalid_audio) {
        require(std::isfinite(sample), "non-finite output escaped the compressor");
    }
}

void test_variable_blocks_and_zero_channels() {
    const auto parameters = hard_knee_settings();
    tabletfx::FetCompressor whole(44100.0F, parameters);
    tabletfx::FetCompressor partitioned(44100.0F, parameters);
    std::vector<float> source(997 * 2);
    for (std::size_t frame = 0; frame < source.size() / 2; ++frame) {
        const float sample = 0.75F * std::sin(0.013F * static_cast<float>(frame));
        source[frame * 2] = sample;
        source[frame * 2 + 1] = -0.5F * sample;
    }
    auto contiguous = source;
    auto split = source;
    whole.process(contiguous, 2);

    constexpr std::array<std::size_t, 7> block_frames{1, 7, 31, 2, 113, 19, 5};
    std::size_t frame_offset = 0;
    std::size_t block_index = 0;
    while (frame_offset < split.size() / 2) {
        const std::size_t count = std::min(block_frames[block_index % block_frames.size()],
                                           split.size() / 2 - frame_offset);
        partitioned.process(std::span<float>(split.data() + frame_offset * 2, count * 2), 2);
        frame_offset += count;
        ++block_index;
    }
    for (std::size_t index = 0; index < split.size(); ++index) {
        require(std::abs(split[index] - contiguous[index]) < 1.0e-6F,
                "variable block partition changed compressor output");
    }

    std::array<float, 2> no_channels{0.25F, -0.25F};
    partitioned.process(no_channels, 0);
    require(no_channels[0] == 0.25F && no_channels[1] == -0.25F,
            "zero-channel input should be left untouched");
}

void test_process_does_not_allocate() {
    tabletfx::FetCompressor compressor(44100.0F, hard_knee_settings());
    std::array<float, 1> mono_one{0.1F};
    std::array<float, 9> mono_irregular{0.1F, -0.2F, 0.3F, -0.4F, 0.5F,
                                         -0.6F, 0.7F, -0.8F, 0.9F};
    std::array<float, 256 * 2> stereo{};
    stereo.fill(0.2F);
    compressor.process(mono_one, 1);
    compressor.process(mono_irregular, 1);
    compressor.process(stereo, 2);

    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        compressor.process(mono_one, 1);
        compressor.process(mono_irregular, 1);
        compressor.process(stereo, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "compressor allocated memory during process()");
    for (float sample : stereo) {
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
        test_attack_preserves_more_of_a_transient_when_slower();
        test_stereo_link_preserves_relative_image();
        test_release_control_and_reset();
        test_defaults_bounds_and_nonfinite_recovery();
        test_variable_blocks_and_zero_channels();
        test_process_does_not_allocate();
        std::cout << "All TabletFX FET-style compressor tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
