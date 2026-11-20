#include "tabletfx/effects/wah.hpp"

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

std::vector<float> make_tone(float sample_rate, float frequency,
                             std::size_t frames) {
    std::vector<float> samples(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float phase = 2.0F * 3.14159265358979323846F * frequency *
                            static_cast<float>(frame) / sample_rate;
        samples[frame] = 0.25F * std::sin(phase);
    }
    return samples;
}

float tail_rms(std::span<const float> samples) {
    const std::size_t first = samples.size() / 2;
    double sum_squares = 0.0;
    for (std::size_t index = first; index < samples.size(); ++index) {
        const double sample = samples[index];
        sum_squares += sample * sample;
    }
    const std::size_t count = samples.size() - first;
    return count == 0 ? 0.0F : static_cast<float>(std::sqrt(sum_squares / count));
}

float tone_rms_at_position(float position, float frequency) {
    tabletfx::WahParameters parameters;
    parameters.position = position;
    parameters.smoothing_ms = 0.0F;
    tabletfx::Wah wah(48000.0F, 1, parameters);
    auto samples = make_tone(48000.0F, frequency, 48000);
    wah.process(samples, 1);
    return tail_rms(samples);
}

void test_default_range_and_frequency_response() {
    tabletfx::Wah wah;
    const auto defaults = wah.parameters();
    require(wah.id() == "wah", "wah stable ID changed");
    require(defaults.position == 0.5F && defaults.minimum_hz == 350.0F &&
                defaults.maximum_hz == 2200.0F && defaults.resonance_q == 4.0F &&
                defaults.level_db == 0.0F && defaults.smoothing_ms == 10.0F,
            "sane guitar wah defaults changed unexpectedly");
    require(wah.memory_bytes() == sizeof(wah),
            "wah memory report is inaccurate");

    const float low_position_low_tone = tone_rms_at_position(0.0F, 350.0F);
    const float low_position_high_tone = tone_rms_at_position(0.0F, 2200.0F);
    const float high_position_low_tone = tone_rms_at_position(1.0F, 350.0F);
    const float high_position_high_tone = tone_rms_at_position(1.0F, 2200.0F);
    require(low_position_low_tone > low_position_high_tone * 2.0F,
            "heel position did not emphasize the low end of the wah sweep");
    require(high_position_high_tone > high_position_low_tone * 2.0F,
            "toe position did not emphasize the high end of the wah sweep");
}

void test_sweep_position_and_parameter_bounds() {
    tabletfx::Wah wah;
    wah.set_position(-2.0F);
    require(wah.parameters().position == 0.0F &&
                std::abs(wah.target_frequency_hz() - 350.0F) < 1.0e-3F,
            "heel position clamp or minimum-frequency target failed");
    wah.set_position(2.0F);
    require(wah.parameters().position == 1.0F &&
                std::abs(wah.target_frequency_hz() - 2200.0F) < 1.0e-3F,
            "toe position clamp or maximum-frequency target failed");
    wah.set_position(std::numeric_limits<float>::quiet_NaN());
    require(wah.parameters().position == 1.0F,
            "non-finite position should preserve the last valid setting");

    wah.set_sweep_range_hz(8000.0F, 400.0F);
    require(wah.parameters().minimum_hz == 400.0F &&
                wah.parameters().maximum_hz == 8000.0F,
            "reversed finite sweep endpoints were not normalized");
    wah.set_sweep_range_hz(10000.0F, 500.0F);
    require(wah.parameters().minimum_hz == 500.0F &&
                wah.parameters().maximum_hz == 10000.0F,
            "sweep range update did not preserve a valid ordered interval");
    wah.set_sweep_range_hz(std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity());
    require(wah.parameters().minimum_hz == 500.0F &&
                wah.parameters().maximum_hz == 10000.0F,
            "non-finite sweep endpoints should preserve prior values");

    wah.set_resonance_q(100.0F);
    require(wah.parameters().resonance_q == 10.0F,
            "resonance Q upper bound failed");
    wah.set_resonance_q(-1.0F);
    require(wah.parameters().resonance_q == 0.7F,
            "resonance Q lower bound failed");
    wah.set_resonance_q(std::numeric_limits<float>::quiet_NaN());
    require(wah.parameters().resonance_q == 0.7F,
            "non-finite Q should preserve prior value");

    wah.set_level_db(30.0F);
    require(wah.parameters().level_db == 12.0F,
            "wah level upper bound failed");
    wah.set_level_db(-100.0F);
    require(wah.parameters().level_db == -24.0F,
            "wah level lower bound failed");
    wah.set_level_db(std::numeric_limits<float>::infinity());
    require(wah.parameters().level_db == -24.0F,
            "non-finite level should preserve prior value");

    wah.set_smoothing_ms(-1.0F);
    require(wah.parameters().smoothing_ms == 0.0F,
            "negative smoothing time was not bounded to immediate response");
    wah.set_smoothing_ms(std::numeric_limits<float>::quiet_NaN());
    require(wah.parameters().smoothing_ms == 10.0F,
            "non-finite smoothing did not use the safe default");
}

void test_smoothing_and_block_invariance() {
    tabletfx::Wah whole_wah(48000.0F, 1);
    tabletfx::Wah chunked_wah(48000.0F, 1);
    whole_wah.set_position(0.0F);
    chunked_wah.set_position(0.0F);
    whole_wah.reset();
    chunked_wah.reset();
    whole_wah.set_position(1.0F);
    chunked_wah.set_position(1.0F);
    std::vector<float> whole(480, 0.2F);
    std::vector<float> chunked = whole;

    float previous_frequency = whole_wah.current_frequency_hz();
    for (std::size_t frame = 0; frame < whole.size(); ++frame) {
        whole_wah.process(std::span<float>(whole).subspan(frame, 1), 1);
        const float frequency = whole_wah.current_frequency_hz();
        require(frequency >= previous_frequency,
                "increasing expression position caused a non-monotonic sweep ramp");
        previous_frequency = frequency;
    }
    require(std::abs(whole_wah.current_frequency_hz() - 2200.0F) < 1.0e-2F,
            "10 ms wah sweep ramp did not reach its target");

    const std::array<std::size_t, 6> blocks{1, 127, 53, 211, 17, 71};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < chunked.size()) {
        const std::size_t count = std::min(blocks[block % blocks.size()],
                                           chunked.size() - offset);
        chunked_wah.process(std::span<float>(chunked).subspan(offset, count), 1);
        offset += count;
        ++block;
    }
    for (std::size_t frame = 0; frame < whole.size(); ++frame) {
        require(std::abs(whole[frame] - chunked[frame]) < 1.0e-6F,
                "variable block sizes changed the wah control ramp/output");
    }
}

void test_stereo_independence_and_channel_guard() {
    tabletfx::WahParameters parameters;
    parameters.smoothing_ms = 0.0F;
    tabletfx::Wah wah(48000.0F, 2, parameters);
    std::vector<float> stereo(8192, 0.0F);
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        const float value = 0.2F * std::sin(static_cast<float>(frame) * 0.11F);
        stereo[frame * 2] = value;
        stereo[frame * 2 + 1] = value;
    }
    wah.process(stereo, 2);
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        require(std::abs(stereo[frame * 2] - stereo[frame * 2 + 1]) < 1.0e-7F,
                "identical stereo input channels produced different wah output");
    }

    std::vector<float> untouched{0.1F, 0.2F, 0.3F};
    const auto original = untouched;
    wah.process(untouched, 1);
    require(untouched == original,
            "wah processed a channel layout different from its prepared layout");
    wah.process(untouched, 0);
    require(untouched == original, "zero-channel wah input should be ignored");
}

void test_reset_finite_recovery_and_allocations() {
    tabletfx::WahParameters parameters;
    parameters.position = 0.7F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::Wah wah(48000.0F, 1, parameters);
    tabletfx::Wah fresh(48000.0F, 1, parameters);
    std::vector<float> first = make_tone(48000.0F, 900.0F, 2048);
    auto expected = first;
    fresh.process(expected, 1);
    wah.process(first, 1);
    wah.reset();
    std::vector<float> after_reset = make_tone(48000.0F, 900.0F, 2048);
    wah.process(after_reset, 1);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        require(std::abs(expected[i] - after_reset[i]) < 1.0e-6F,
                "reset did not clear filter state and restore the configured response");
    }

    std::array<float, 5> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
        0.25F,
    };
    wah.process(invalid, 1);
    for (float sample : invalid) {
        require(std::isfinite(sample), "wah failed to recover from non-finite/extreme input");
    }

    std::array<float, 256> block{};
    block.fill(0.125F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        wah.process(block, 1);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "wah process() allocated memory");
}

void test_invalid_construction() {
    bool rejected_rate = false;
    try {
        tabletfx::Wah invalid_rate(100.0F, 1);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    require(rejected_rate, "invalid sample rate was accepted");

    bool rejected_channels = false;
    try {
        tabletfx::Wah invalid_channels(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    require(rejected_channels, "unsupported channel count was accepted");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_default_range_and_frequency_response();
        test_sweep_position_and_parameter_bounds();
        test_smoothing_and_block_invariance();
        test_stereo_independence_and_channel_guard();
        test_reset_finite_recovery_and_allocations();
        test_invalid_construction();
        std::cout << "All TabletFX wah tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
