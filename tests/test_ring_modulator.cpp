#include "tabletfx/effects/ring_modulator.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
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

void test_defaults_bounds_and_effective_frequency() {
    tabletfx::RingModulator ring;
    const auto defaults = ring.parameters();
    require(ring.id() == "ring-mod", "ring modulator stable ID changed");
    require(defaults.frequency_hz == 440.0F &&
                defaults.frequency_multiplier == 1.0F &&
                defaults.high_cut_hz == 20000.0F && defaults.mix == 1.0F &&
                ring.effective_frequency_hz() == 440.0F,
            "project-authored ring mod defaults changed");
    require(ring.memory_bytes() == sizeof(ring),
            "ring modulator memory report is inaccurate");

    tabletfx::RingModulatorParameters parameters;
    parameters.frequency_hz = -5.0F;
    parameters.frequency_multiplier = 20.0F;
    parameters.high_cut_hz = -1.0F;
    parameters.mix = 2.0F;
    ring.set_parameters(parameters);
    const auto bounded = ring.parameters();
    require(bounded.frequency_hz == 2.0F &&
                bounded.frequency_multiplier == 4.0F &&
                bounded.high_cut_hz == 20.0F && bounded.mix == 1.0F &&
                ring.effective_frequency_hz() == 8.0F,
            "project-authored ring mod parameter ranges were not enforced");

    parameters.frequency_hz = 2000.0F;
    parameters.frequency_multiplier = 4.0F;
    parameters.high_cut_hz = 20000.0F;
    ring.set_parameters(parameters);
    require(ring.parameters().frequency_hz == 2000.0F &&
                ring.parameters().frequency_multiplier == 4.0F &&
                ring.effective_frequency_hz() == 8000.0F,
            "project-authored upper oscillator or multiplier range changed");

    tabletfx::RingModulator low_rate(8000.0F, 1);
    require(low_rate.effective_frequency_hz() <= 3600.0F &&
                low_rate.effective_high_cut_hz() <= 3600.0F,
            "low sample rate did not keep oscillator and high cut below Nyquist");
}

void test_classic_modulation_mix_and_shared_stereo_phase() {
    tabletfx::RingModulatorParameters parameters;
    parameters.frequency_hz = 1000.0F;
    parameters.high_cut_hz = 20000.0F;
    parameters.mix = 1.0F;
    tabletfx::RingModulator mono(48000.0F, 1, parameters);
    std::array<float, 64> constant{};
    constant.fill(1.0F);
    mono.process(constant, 1);
    for (std::size_t frame = 0; frame < constant.size(); ++frame) {
        const float expected = std::sin(
            2.0F * std::numbers::pi_v<float> * 1000.0F *
            static_cast<float>(frame) / 48000.0F);
        require(std::abs(constant[frame] - expected) < 1.5e-2F,
                "classic ring modulation did not multiply by the carrier");
    }

    parameters.mix = 0.0F;
    tabletfx::RingModulator dry(48000.0F, 1, parameters);
    std::array<float, 64> unchanged{};
    for (std::size_t frame = 0; frame < unchanged.size(); ++frame) {
        unchanged[frame] = static_cast<float>(static_cast<int>(frame % 11) - 5) / 5.0F;
    }
    const auto expected_dry = unchanged;
    dry.process(unchanged, 1);
    require(unchanged == expected_dry,
            "zero-mix ring modulation changed the dry signal");

    parameters.mix = 1.0F;
    tabletfx::RingModulator stereo(48000.0F, 2, parameters);
    std::array<float, 256> stereo_input{};
    for (std::size_t frame = 0; frame < stereo_input.size() / 2U; ++frame) {
        stereo_input[frame * 2U] = 1.0F;
        stereo_input[frame * 2U + 1U] = 0.5F;
    }
    stereo.process(stereo_input, 2);
    for (std::size_t frame = 0; frame < stereo_input.size() / 2U; ++frame) {
        require(std::abs(stereo_input[frame * 2U] -
                         2.0F * stereo_input[frame * 2U + 1U]) < 1.0e-6F,
                "stereo channels did not share a carrier phase");
    }
}

void test_high_cut_reduces_high_frequency_energy() {
    tabletfx::RingModulatorParameters open_parameters;
    open_parameters.frequency_hz = 1000.0F;
    open_parameters.high_cut_hz = 20000.0F;
    open_parameters.mix = 1.0F;
    auto filtered_parameters = open_parameters;
    filtered_parameters.high_cut_hz = 1000.0F;
    tabletfx::RingModulator open(48000.0F, 1, open_parameters);
    tabletfx::RingModulator filtered(48000.0F, 1, filtered_parameters);

    constexpr std::size_t frames = 48000;
    std::vector<float> source(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        source[frame] = std::sin(
            2.0F * std::numbers::pi_v<float> * 6000.0F *
            static_cast<float>(frame) / 48000.0F);
    }
    auto open_audio = source;
    auto filtered_audio = source;
    open.process(open_audio, 1);
    filtered.process(filtered_audio, 1);

    double open_energy = 0.0;
    double filtered_energy = 0.0;
    for (std::size_t frame = 1000; frame < frames; ++frame) {
        open_energy += static_cast<double>(open_audio[frame]) * open_audio[frame];
        filtered_energy += static_cast<double>(filtered_audio[frame]) *
                           filtered_audio[frame];
    }
    require(filtered_energy < open_energy * 0.1,
            "lower high-cut frequency did not reduce upper-frequency energy");
}

void test_block_partition_reset_and_invalid_audio() {
    tabletfx::RingModulatorParameters parameters;
    parameters.frequency_hz = 713.0F;
    parameters.frequency_multiplier = 1.75F;
    parameters.high_cut_hz = 9000.0F;
    parameters.mix = 0.83F;
    tabletfx::RingModulator whole(44100.0F, 2, parameters);
    tabletfx::RingModulator chunks(44100.0F, 2, parameters);
    std::vector<float> input(4096);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = 0.4F * std::sin(static_cast<float>(i) * 0.017F);
    }
    auto expected = input;
    auto actual = input;
    whole.process(expected, 2);
    const std::array<std::size_t, 4> block_sizes{1, 53, 127, 211};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < actual.size()) {
        std::size_t count = std::min(block_sizes[block % block_sizes.size()],
                                     actual.size() - offset);
        count -= count % 2U;
        if (count == 0) {
            count = 2;
        }
        chunks.process(std::span<float>(actual).subspan(offset, count), 2);
        offset += count;
        ++block;
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        require(std::abs(expected[i] - actual[i]) < 1.0e-7F,
                "ring mod output changed with block partitioning");
    }

    whole.reset();
    chunks.reset();
    std::array<float, 32> reset_a{};
    reset_a.fill(0.4F);
    auto reset_b = reset_a;
    whole.process(reset_a, 2);
    chunks.process(reset_b, 2);
    require(reset_a == reset_b, "reset did not restore deterministic state");

    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.5F,
        std::numeric_limits<float>::quiet_NaN(),
        0.25F,
        -0.5F,
        0.75F,
    };
    whole.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "non-finite audio samples did not produce finite output");

    std::array<float, 5> untouched{0.1F, -0.2F, 0.3F, -0.4F, 0.5F};
    const auto before = untouched;
    whole.process(untouched, 1);
    require(untouched == before,
            "mismatched channel layout should leave the buffer untouched");
}

void test_extreme_finite_input_does_not_poison_filter_state() {
    tabletfx::RingModulatorParameters parameters;
    parameters.frequency_hz = 2000.0F;
    parameters.high_cut_hz = 20000.0F;
    parameters.mix = 1.0F;
    tabletfx::RingModulator stressed(48000.0F, 1, parameters);
    tabletfx::RingModulator reference(48000.0F, 1, parameters);

    const float largest = std::numeric_limits<float>::max();
    std::array<float, 8> extreme{};
    extreme.fill(largest);
    extreme.back() = -largest;
    stressed.process(extreme, 1);
    require(std::all_of(extreme.begin(), extreme.end(),
                        [](float sample) { return sample == 0.0F; }),
            "extreme finite samples were not silenced and isolated");

    std::array<float, 8> matching_silence{};
    reference.process(matching_silence, 1);

    std::array<float, 64> after_extremes{};
    std::array<float, 64> after_silence{};
    for (std::size_t index = 0; index < after_extremes.size(); ++index) {
        const float sample = static_cast<float>(static_cast<int>(index % 9U) - 4) *
                             0.125F;
        after_extremes[index] = sample;
        after_silence[index] = sample;
    }
    stressed.process(after_extremes, 1);
    reference.process(after_silence, 1);
    for (std::size_t index = 0; index < after_extremes.size(); ++index) {
        require(std::isfinite(after_extremes[index]) &&
                    std::abs(after_extremes[index] - after_silence[index]) < 1.0e-7F,
                "extreme finite input poisoned the filter state for following audio");
    }
}

void test_processing_does_not_allocate() {
    tabletfx::RingModulator ring;
    std::array<float, 512> block{};
    block.fill(0.25F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        ring.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "ring modulator allocated during process()");
    require(std::all_of(block.begin(), block.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "ring modulator emitted non-finite samples");
}

void test_invalid_construction() {
    bool bad_rate = false;
    try {
        tabletfx::RingModulator invalid(100.0F, 1);
    } catch (const std::invalid_argument&) {
        bad_rate = true;
    }
    require(bad_rate, "ring modulator accepted an unsupported sample rate");

    bool bad_channels = false;
    try {
        tabletfx::RingModulator invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        bad_channels = true;
    }
    require(bad_channels, "ring modulator accepted more than two channels");
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
        test_defaults_bounds_and_effective_frequency();
        test_classic_modulation_mix_and_shared_stereo_phase();
        test_high_cut_reduces_high_frequency_energy();
        test_block_partition_reset_and_invalid_audio();
        test_extreme_finite_input_does_not_poison_filter_state();
        test_processing_does_not_allocate();
        test_invalid_construction();
        std::cout << "All TabletFX ring modulator tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
