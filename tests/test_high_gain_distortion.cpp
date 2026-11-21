#include "tabletfx/effects/high_gain_distortion.hpp"

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
    if (!condition) throw std::runtime_error(message);
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

float tone_rms(float frequency_hz, float low_cut_hz, float high_cut_hz,
               float post_tone_hz) {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t samples = 48000;
    constexpr std::size_t skip = 8000;
    tabletfx::HighGainDistortionParameters parameters{};
    parameters.drive = 0.0F;
    parameters.pre_low_cut_hz = low_cut_hz;
    parameters.pre_high_cut_hz = high_cut_hz;
    parameters.post_tone_hz = post_tone_hz;
    parameters.level_db = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::HighGainDistortion effect(sample_rate, parameters);
    std::vector<float> signal(samples);
    constexpr float amplitude = 0.1F;
    constexpr float two_pi = 6.28318530717958647692F;
    for (std::size_t i = 0; i < samples; ++i) {
        signal[i] = amplitude * std::sin(two_pi * frequency_hz * static_cast<float>(i) /
                                         sample_rate);
    }
    effect.process(signal, 1);
    double sum = 0.0;
    for (std::size_t i = skip; i < samples; ++i) {
        sum += static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(samples - skip)));
}

void test_defaults_bounds_and_dry_passthrough() {
    tabletfx::HighGainDistortion distortion;
    require(distortion.id() == "high-gain-distortion", "stable distortion ID changed");
    require(distortion.drive() == 0.75F && distortion.pre_low_cut_hz() == 100.0F &&
                distortion.pre_high_cut_hz() == 9000.0F &&
                distortion.post_tone_hz() == 5200.0F && distortion.level_db() == -9.0F &&
                distortion.mix() == 1.0F,
            "distortion defaults changed");
    require(distortion.memory_bytes() == sizeof(distortion),
            "distortion memory report omitted or invented owned storage");

    tabletfx::HighGainDistortionParameters bypass{};
    bypass.drive = 1.0F;
    bypass.level_db = 0.0F;
    bypass.mix = 0.0F;
    bypass.smoothing_ms = 0.0F;
    distortion.configure(bypass);
    std::array<float, 4> input{0.25F, -0.5F, 0.75F, -1.0F};
    const auto original = input;
    distortion.process(input, 2);
    require(input == original, "zero mix and zero level must be exact dry passthrough");

    std::array<float, 4> large_input{32.0F, -64.0F, 128.0F, -256.0F};
    const auto original_large_input = large_input;
    distortion.process(large_input, 2);
    require(large_input == original_large_input,
            "large finite samples must remain unchanged in exact dry bypass");

    distortion.set_drive(4.0F);
    require(distortion.drive() == 1.0F, "drive upper bound failed");
    distortion.set_drive(-1.0F);
    require(distortion.drive() == 0.0F, "drive lower bound failed");
    distortion.set_drive(std::numeric_limits<float>::quiet_NaN());
    require(distortion.drive() == 0.75F, "invalid drive must return to its sane default");
    distortion.set_pre_low_cut_hz(-5.0F);
    require(distortion.pre_low_cut_hz() == 20.0F, "pre low-cut lower bound failed");
    distortion.set_pre_low_cut_hz(2000.0F);
    require(distortion.pre_low_cut_hz() == 1000.0F, "pre low-cut upper bound failed");
    distortion.set_pre_high_cut_hz(100.0F);
    require(distortion.pre_high_cut_hz() >= 1500.0F,
            "pre high-cut must remain above the configured low-cut");
    distortion.set_post_tone_hz(100.0F);
    require(distortion.post_tone_hz() == 500.0F, "post tone lower bound failed");
    distortion.set_level_db(-100.0F);
    require(distortion.level_db() == -24.0F, "level lower bound failed");
    distortion.set_level_db(100.0F);
    require(distortion.level_db() == 12.0F, "level upper bound failed");
    distortion.set_mix(-1.0F);
    require(distortion.mix() == 0.0F, "mix lower bound failed");
    distortion.set_mix(2.0F);
    require(distortion.mix() == 1.0F, "mix upper bound failed");
}

void test_high_gain_symmetry_and_stereo_independence() {
    tabletfx::HighGainDistortionParameters parameters{};
    parameters.drive = 0.95F;
    parameters.pre_low_cut_hz = 40.0F;
    parameters.pre_high_cut_hz = 16000.0F;
    parameters.post_tone_hz = 12000.0F;
    parameters.level_db = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::HighGainDistortion distortion(48000.0F, parameters);

    constexpr std::size_t frames = 512;
    constexpr float two_pi = 6.28318530717958647692F;
    std::array<float, frames * 2> stereo{};
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float sample = 0.8F * std::sin(two_pi * static_cast<float>(frame) / 64.0F);
        stereo[frame * 2] = sample;
        stereo[frame * 2 + 1] = -sample;
    }
    distortion.process(stereo, 2);
    float peak = 0.0F;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float left = stereo[frame * 2];
        const float right = stereo[frame * 2 + 1];
        require(std::abs(left + right) < 1.0e-6F,
                "stereo filter states were coupled or channel order changed");
        require(std::isfinite(left) && std::isfinite(right),
                "high-gain processing produced a non-finite sample");
        peak = std::max(peak, std::abs(left));
    }
    require(peak > 0.2F, "high-gain setting did not produce a useful distorted signal");
    require(peak <= 1.001F, "saturator exceeded its expected level at unity output gain");
}

void test_pre_and_post_filtering() {
    const float hp_sub = tone_rms(80.0F, 300.0F, 18000.0F, 18000.0F);
    const float hp_mid = tone_rms(1000.0F, 300.0F, 18000.0F, 18000.0F);
    require(hp_sub < hp_mid * 0.55F,
            "pre-distortion low-cut did not reduce low-frequency content");

    const float pre_mid = tone_rms(1000.0F, 20.0F, 3000.0F, 18000.0F);
    const float pre_high = tone_rms(6000.0F, 20.0F, 3000.0F, 18000.0F);
    require(pre_high < pre_mid * 0.7F,
            "pre-distortion high-cut did not reduce high-frequency content");

    const float post_mid = tone_rms(1000.0F, 20.0F, 18000.0F, 1800.0F);
    const float post_high = tone_rms(6000.0F, 20.0F, 18000.0F, 1800.0F);
    require(post_high < post_mid * 0.5F,
            "post-distortion tone control did not reduce high-frequency content");
}

void test_zero_fixed_latency() {
    tabletfx::HighGainDistortionParameters parameters{};
    parameters.drive = 0.8F;
    parameters.level_db = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::HighGainDistortion distortion(48000.0F, parameters);
    std::array<float, 8> impulse{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
    distortion.process(impulse, 1);
    require(impulse[0] != 0.0F,
            "the effect must respond in the input frame with no fixed sample delay");
}

void test_smoothing_block_invariance_and_reset() {
    tabletfx::HighGainDistortionParameters initial{};
    initial.drive = 0.05F;
    initial.pre_low_cut_hz = 80.0F;
    initial.pre_high_cut_hz = 10000.0F;
    initial.post_tone_hz = 7000.0F;
    initial.level_db = 0.0F;
    initial.mix = 1.0F;
    tabletfx::HighGainDistortion whole(48000.0F, initial);
    tabletfx::HighGainDistortion split(48000.0F, initial);
    whole.set_drive(0.9F);
    split.set_drive(0.9F);
    whole.set_pre_low_cut_hz(180.0F);
    split.set_pre_low_cut_hz(180.0F);
    whole.set_pre_high_cut_hz(5000.0F);
    split.set_pre_high_cut_hz(5000.0F);
    whole.set_post_tone_hz(3600.0F);
    split.set_post_tone_hz(3600.0F);

    constexpr std::size_t frames = 1024;
    constexpr float two_pi = 6.28318530717958647692F;
    std::vector<float> whole_buffer(frames * 2);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float sample = 0.4F * std::sin(two_pi * 440.0F * static_cast<float>(frame) /
                                             48000.0F);
        whole_buffer[frame * 2] = sample;
        whole_buffer[frame * 2 + 1] = 0.5F * sample;
    }
    std::vector<float> split_buffer = whole_buffer;
    whole.process(whole_buffer, 2);
    constexpr std::array<std::size_t, 5> blocks{1, 127, 53, 300, 17};
    std::size_t offset_frames = 0;
    std::size_t block_index = 0;
    while (offset_frames < frames) {
        const std::size_t count = std::min(blocks[block_index % blocks.size()],
                                           frames - offset_frames);
        split.process(std::span<float>(split_buffer).subspan(offset_frames * 2, count * 2), 2);
        offset_frames += count;
        ++block_index;
    }
    for (std::size_t i = 0; i < whole_buffer.size(); ++i) {
        require(std::abs(whole_buffer[i] - split_buffer[i]) < 1.0e-6F,
                "parameter smoothing depends on callback partitioning");
    }

    whole.reset();
    tabletfx::HighGainDistortion settled(48000.0F, initial);
    settled.set_drive(0.9F);
    settled.set_pre_low_cut_hz(180.0F);
    settled.set_pre_high_cut_hz(5000.0F);
    settled.set_post_tone_hz(3600.0F);
    settled.reset();
    std::array<float, 2> after_reset{0.4F, 0.2F};
    std::array<float, 2> settled_sample{0.4F, 0.2F};
    whole.process(after_reset, 2);
    settled.process(settled_sample, 2);
    require(std::abs(after_reset[0] - settled_sample[0]) < 1.0e-6F &&
                std::abs(after_reset[1] - settled_sample[1]) < 1.0e-6F,
            "reset did not clear filter state and settle to configured targets");
}

void test_invalid_layout_non_finite_and_no_allocations() {
    tabletfx::HighGainDistortion distortion;
    std::array<float, 2> untouched{0.25F, -0.5F};
    const auto original = untouched;
    distortion.set_drive(1.0F);
    distortion.process(untouched, 0);
    require(untouched == original, "zero-channel layout must not touch samples");
    distortion.process(untouched, 3);
    require(untouched == original, "unsupported channel layouts must not touch samples");

    std::array<float, 5> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max()};
    distortion.process(invalid, 1);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F && invalid[2] == 0.0F,
            "non-finite audio must recover to silence");
    for (std::size_t i = 3; i < invalid.size(); ++i) {
        require(std::isfinite(invalid[i]), "large finite samples must remain finite");
    }

    std::vector<float> buffer(256 * 2, 0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int i = 0; i < 100; ++i) distortion.process(buffer, 2);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "process allocated memory");
}

} // namespace

int main() {
    try {
        test_defaults_bounds_and_dry_passthrough();
        test_high_gain_symmetry_and_stereo_independence();
        test_pre_and_post_filtering();
        test_zero_fixed_latency();
        test_smoothing_block_invariance_and_reset();
        test_invalid_layout_non_finite_and_no_allocations();
        std::cout << "All high-gain-distortion tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "High-gain-distortion test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
