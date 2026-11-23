#include "tabletfx/effects/rock_box_dist.hpp"

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

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> sine_signal(std::size_t frames, float frequency,
                               float amplitude = 0.35F) {
    std::vector<float> samples(frames);
    constexpr float sample_rate = 48000.0F;
    constexpr float two_pi = 6.28318530717958647692F;
    for (std::size_t i = 0; i < frames; ++i) {
        samples[i] = amplitude * std::sin(
            two_pi * frequency * static_cast<float>(i) / sample_rate);
    }
    return samples;
}

double rms(const std::vector<float>& samples, std::size_t skip = 0U) {
    double sum = 0.0;
    for (std::size_t i = skip; i < samples.size(); ++i) {
        const double value = samples[i];
        sum += value * value;
    }
    return std::sqrt(sum / static_cast<double>(samples.size() - skip));
}

double difference(const std::vector<float>& left,
                  const std::vector<float>& right) {
    require(left.size() == right.size(), "test vectors have different sizes");
    double sum = 0.0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        sum += std::abs(static_cast<double>(left[i] - right[i]));
    }
    return sum;
}

std::vector<float> render(tabletfx::RockBoxDistortionParameters parameters,
                          std::vector<float> samples) {
    parameters.smoothing_ms = 0.0F;
    tabletfx::RockBoxDistortion effect(48000.0F, parameters);
    effect.process(samples, 1U);
    return samples;
}

void test_defaults_bounds_and_identity() {
    tabletfx::RockBoxDistortion effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "rock-box-distortion",
            "Rock Box Distortion stable ID changed");
    require(defaults.drive == 0.5F && defaults.tone == 0.5F &&
                defaults.level_db == 0.0F && defaults.smoothing_ms == 10.0F,
            "project-authored Rock Box defaults changed");
    require(effect.memory_bytes() == sizeof(effect),
            "Rock Box memory report omitted or invented owned storage");

    tabletfx::RockBoxDistortionParameters p;
    p.drive = -1.0F;
    p.tone = 2.0F;
    p.level_db = std::numeric_limits<float>::quiet_NaN();
    p.smoothing_ms = -1.0F;
    effect.set_parameters(p);
    auto bounded = effect.parameters();
    require(bounded.drive == 0.0F && bounded.tone == 1.0F &&
                bounded.level_db == 0.0F && bounded.smoothing_ms == 0.0F,
            "project-authored bounds or non-finite fallbacks failed");

    p.drive = std::numeric_limits<float>::quiet_NaN();
    p.tone = std::numeric_limits<float>::infinity();
    p.level_db = -100.0F;
    p.smoothing_ms = std::numeric_limits<float>::infinity();
    effect.set_parameters(p);
    bounded = effect.parameters();
    require(bounded.drive == 0.5F && bounded.tone == 0.5F &&
                bounded.level_db == -24.0F && bounded.smoothing_ms == 10.0F,
            "invalid parameter fallback or lower Level bound failed");
}

void test_drive_tone_and_level_change_signal() {
    auto source = sine_signal(24000U, 220.0F);
    tabletfx::RockBoxDistortionParameters base;
    base.smoothing_ms = 0.0F;
    const auto baseline = render(base, source);

    auto low_drive = base;
    low_drive.drive = 0.0F;
    require(difference(baseline, render(low_drive, source)) > 10.0,
            "Drive did not change the distortion amount");

    auto dark_tone = base;
    dark_tone.tone = 0.0F;
    auto bright_tone = base;
    bright_tone.tone = 1.0F;
    const auto dark = render(dark_tone, sine_signal(24000U, 6000.0F));
    const auto bright = render(bright_tone, sine_signal(24000U, 6000.0F));
    require(rms(bright, 2048U) > rms(dark, 2048U) * 2.0,
            "higher Tone did not produce a brighter response");

    auto quiet = base;
    quiet.drive = 0.0F;
    const auto quiet_input = render(quiet, sine_signal(12000U, 220.0F, 0.002F));
    auto raised = quiet;
    raised.level_db = 6.0F;
    const auto raised_output = render(
        raised, sine_signal(12000U, 220.0F, 0.002F));
    require(rms(raised_output, 1024U) > rms(quiet_input, 1024U) * 1.8,
            "positive Level did not increase output level");
}

void test_stereo_independence_partial_frames_and_invalid_channels() {
    tabletfx::RockBoxDistortionParameters p;
    p.drive = 0.65F;
    p.tone = 0.3F;
    p.smoothing_ms = 0.0F;
    tabletfx::RockBoxDistortion stereo(48000.0F, p);
    tabletfx::RockBoxDistortion left_effect(48000.0F, p);
    tabletfx::RockBoxDistortion right_effect(48000.0F, p);
    auto left = sine_signal(2048U, 220.0F);
    auto right = sine_signal(2048U, 440.0F, -0.2F);
    std::vector<float> interleaved(left.size() * 2U);
    for (std::size_t i = 0; i < left.size(); ++i) {
        interleaved[2U * i] = left[i];
        interleaved[2U * i + 1U] = right[i];
    }
    left_effect.process(left, 1U);
    right_effect.process(right, 1U);
    stereo.process(interleaved, 2U);
    for (std::size_t i = 0; i < left.size(); ++i) {
        require(std::abs(interleaved[2U * i] - left[i]) < 1.0e-6F &&
                    std::abs(interleaved[2U * i + 1U] - right[i]) < 1.0e-6F,
                "stereo channels shared filter history");
    }

    std::array<float, 3> partial{0.1F, -0.2F, 0.3F};
    const auto original = partial;
    stereo.process(partial, 2U);
    require(partial != original,
            "partial final frame did not process its available channel");
    for (float sample : partial) {
        require(std::isfinite(sample), "partial frame produced non-finite audio");
    }

    std::array<float, 3> unsupported{0.1F, -0.2F, 0.3F};
    const auto unchanged = unsupported;
    stereo.process(unsupported, 3U);
    require(unsupported == unchanged,
            "unsupported channel count modified audio");
}

void test_ramps_reset_invalid_samples_and_no_allocations() {
    tabletfx::RockBoxDistortionParameters start;
    start.drive = 0.0F;
    start.tone = 0.0F;
    start.level_db = -6.0F;
    start.smoothing_ms = 1.0F;
    tabletfx::RockBoxDistortion whole(48000.0F, start);
    tabletfx::RockBoxDistortion split(48000.0F, start);
    auto target = start;
    target.drive = 1.0F;
    target.tone = 1.0F;
    target.level_db = 3.0F;
    whole.set_parameters(target);
    split.set_parameters(target);
    auto whole_audio = sine_signal(997U, 330.0F);
    auto split_audio = whole_audio;
    whole.process(whole_audio, 1U);
    constexpr std::array<std::size_t, 5> blocks{1U, 127U, 53U, 300U, 17U};
    std::size_t offset = 0U;
    std::size_t block_index = 0U;
    while (offset < split_audio.size()) {
        const std::size_t count = std::min(
            blocks[block_index % blocks.size()], split_audio.size() - offset);
        split.process(std::span<float>(split_audio).subspan(offset, count), 1U);
        offset += count;
        ++block_index;
    }
    require(difference(whole_audio, split_audio) < 1.0e-4,
            "parameter smoothing changed with callback partitioning");

    auto first = sine_signal(4096U, 220.0F);
    auto after_reset = first;
    tabletfx::RockBoxDistortion configured(48000.0F, target);
    configured.process(first, 1U);
    configured.reset();
    configured.process(after_reset, 1U);
    auto fresh = sine_signal(4096U, 220.0F);
    tabletfx::RockBoxDistortion fresh_effect(48000.0F, target);
    fresh_effect.process(fresh, 1U);
    require(after_reset == fresh,
            "reset did not restore deterministic filter state and targets");

    std::array<float, 8> audio{0.2F, -0.2F, 0.4F, -0.4F,
                               0.6F, -0.6F, 0.8F, -0.8F};
    audio[3] = std::numeric_limits<float>::quiet_NaN();
    allocation_count = 0U;
    count_allocations = true;
    configured.process(audio, 2U);
    count_allocations = false;
    require(allocation_count == 0U,
            "Rock Box distortion process allocated memory");
    for (float sample : audio) {
        require(std::isfinite(sample), "non-finite input was not sanitized");
    }
    require(audio[3] == 0.0F, "non-finite input was not cleared");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
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
        test_defaults_bounds_and_identity();
        test_drive_tone_and_level_change_signal();
        test_stereo_independence_partial_frames_and_invalid_channels();
        test_ramps_reset_invalid_samples_and_no_allocations();
        std::cout << "Rock Box distortion tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        count_allocations = false;
        std::cerr << "Rock Box distortion test failure: "
                  << exception.what() << '\n';
        return 1;
    }
}
