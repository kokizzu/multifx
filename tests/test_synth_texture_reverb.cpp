#include "tabletfx/effects/synth_texture_reverb.hpp"

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

using tabletfx::effects::SynthTextureReverb;
using tabletfx::effects::SynthTextureReverbParameters;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

double energy(std::span<const float> samples) {
    double result = 0.0;
    for (const float sample : samples) {
        result += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return result;
}

void test_defaults_bounds_and_memory() {
    SynthTextureReverb effect(48000.0F, 2U);
    const auto defaults = effect.parameters();
    require(effect.id() == "synth-texture-reverb", "effect id changed");
    require(defaults.mix == 0.25F && defaults.texture == 0.45F &&
                defaults.decay_seconds == 4.5F && defaults.damping_hz == 7200.0F &&
                defaults.level_db == 0.0F && defaults.envelope_release_ms == 240.0F &&
                !defaults.spillover,
            "project defaults changed unexpectedly");
    require(effect.fixed_latency_samples() == 0U,
            "dry-path fixed algorithmic latency should be zero");
    require(effect.memory_bytes() > sizeof(effect) &&
                effect.memory_bytes() < 100000U,
            "48 kHz stereo state should remain bounded below 100 kB");

    SynthTextureReverbParameters extreme;
    extreme.mix = 4.0F;
    extreme.texture = -1.0F;
    extreme.decay_seconds = 80.0F;
    extreme.damping_hz = 100000.0F;
    extreme.level_db = 50.0F;
    extreme.envelope_release_ms = -3.0F;
    effect.configure(extreme);
    const auto bounded = effect.parameters();
    require(bounded.mix == 1.0F && bounded.texture == 0.0F &&
                bounded.decay_seconds == 12.0F && bounded.damping_hz == 18000.0F &&
                bounded.level_db == 12.0F && bounded.envelope_release_ms == 20.0F,
            "controls were not clamped to their declared project bounds");

    SynthTextureReverbParameters invalid;
    invalid.mix = std::numeric_limits<float>::quiet_NaN();
    invalid.texture = std::numeric_limits<float>::infinity();
    invalid.decay_seconds = -std::numeric_limits<float>::infinity();
    invalid.damping_hz = std::numeric_limits<float>::quiet_NaN();
    invalid.level_db = std::numeric_limits<float>::infinity();
    invalid.envelope_release_ms = std::numeric_limits<float>::quiet_NaN();
    effect.configure(invalid);
    const auto safe = effect.parameters();
    require(safe.mix == defaults.mix && safe.texture == defaults.texture &&
                safe.decay_seconds == defaults.decay_seconds &&
                safe.damping_hz == defaults.damping_hz &&
                safe.level_db == defaults.level_db &&
                safe.envelope_release_ms == defaults.envelope_release_ms,
            "non-finite controls should fall back to project defaults");
}

void test_mix_zero_is_exact_dry_and_latency_free() {
    SynthTextureReverbParameters parameters;
    parameters.mix = 0.0F;
    parameters.texture = 1.0F;
    parameters.spillover = false;
    SynthTextureReverb effect(48000.0F, 1U, parameters);
    std::array<float, 128U> audio{};
    for (std::size_t index = 0U; index < audio.size(); ++index) {
        audio[index] = 0.7F * std::sin(0.073F * static_cast<float>(index));
    }
    const auto original = audio;
    effect.process(audio, 1U);
    require(audio == original,
            "zero Mix must preserve ordinary dry samples exactly without latency");
    require(effect.fixed_latency_samples() == 0U,
            "no fixed lookahead or dry-path delay is introduced");
}

void test_synthetic_wet_response_and_stereo_width() {
    SynthTextureReverbParameters parameters;
    parameters.mix = 1.0F;
    parameters.texture = 0.9F;
    parameters.decay_seconds = 5.0F;
    SynthTextureReverb mono(48000.0F, 1U, parameters);
    std::vector<float> impulse(6000U, 0.0F);
    impulse[0] = 0.8F;
    mono.process(impulse, 1U);
    require(energy(std::span<const float>(impulse.data() + 700U,
                                          impulse.size() - 700U)) > 1.0e-7,
            "an input transient should excite a synthetic reverberant tail");
    require(std::all_of(impulse.begin(), impulse.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "wet response must remain finite");

    SynthTextureReverb stereo(48000.0F, 2U, parameters);
    std::vector<float> stereo_signal(5000U * 2U, 0.0F);
    for (std::size_t frame = 0U; frame < stereo_signal.size() / 2U; ++frame) {
        const float sample = frame == 0U
            ? 0.8F
            : 0.15F * std::sin(0.031F * static_cast<float>(frame));
        stereo_signal[frame * 2U] = sample;
        stereo_signal[frame * 2U + 1U] = sample;
    }
    stereo.process(stereo_signal, 2U);
    double channel_difference = 0.0;
    for (std::size_t frame = 0U; frame < stereo_signal.size() / 2U; ++frame) {
        channel_difference += std::abs(
            static_cast<double>(stereo_signal[frame * 2U] -
                                stereo_signal[frame * 2U + 1U]));
    }
    require(channel_difference > 1.0e-4,
            "stereo delay offsets should produce a distinct stereo wet field");
}

void test_reset_and_block_partition_continuity() {
    SynthTextureReverbParameters parameters;
    parameters.mix = 0.62F;
    parameters.texture = 0.7F;
    SynthTextureReverb first(44100.0F, 2U, parameters);
    SynthTextureReverb second(44100.0F, 2U, parameters);
    std::vector<float> source(1803U * 2U);
    for (std::size_t frame = 0U; frame < source.size() / 2U; ++frame) {
        const float sample = 0.45F * std::sin(0.017F * static_cast<float>(frame));
        source[frame * 2U] = sample;
        source[frame * 2U + 1U] = -0.72F * sample;
    }
    auto contiguous = source;
    auto partitioned = source;
    first.process(std::span<float>(contiguous.data(), 173U * 2U), 2U);
    second.process(std::span<float>(partitioned.data(), 173U * 2U), 2U);

    parameters.mix = 0.8F;
    parameters.texture = 0.25F;
    parameters.decay_seconds = 2.1F;
    first.configure(parameters);
    second.configure(parameters);
    first.process(std::span<float>(contiguous.data() + 173U * 2U,
                                   contiguous.size() - 173U * 2U), 2U);
    constexpr std::array<std::size_t, 7U> blocks{1U, 9U, 31U, 3U, 121U, 17U, 5U};
    std::size_t frame = 173U;
    std::size_t block_index = 0U;
    while (frame < partitioned.size() / 2U) {
        const std::size_t count = std::min(
            blocks[block_index % blocks.size()], partitioned.size() / 2U - frame);
        second.process(std::span<float>(partitioned.data() + frame * 2U,
                                        count * 2U), 2U);
        frame += count;
        ++block_index;
    }
    for (std::size_t index = 0U; index < contiguous.size(); ++index) {
        require(std::abs(contiguous[index] - partitioned[index]) < 1.0e-6F,
                "output should not depend on host block partitioning");
    }

    SynthTextureReverb resettable(48000.0F, 1U, parameters);
    std::array<float, 2048U> input{};
    for (std::size_t index = 0U; index < input.size(); ++index) {
        input[index] = 0.4F * std::sin(0.041F * static_cast<float>(index));
    }
    auto first_render = input;
    resettable.process(first_render, 1U);
    resettable.reset();
    auto second_render = input;
    resettable.process(second_render, 1U);
    require(first_render == second_render,
            "reset should clear the delay, envelope, and oscillator phases");
}

void test_invalid_audio_channel_guard_and_bypass() {
    SynthTextureReverbParameters parameters;
    parameters.mix = 0.7F;
    SynthTextureReverb effect(48000.0F, 2U, parameters);
    std::array<float, 12U> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
        0.75F, -0.75F, 0.2F, -0.1F, 0.0F, 0.0F, 0.0F};
    effect.process(invalid, 2U);
    require(std::all_of(invalid.begin(), invalid.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "invalid and extreme input audio must recover to finite output");

    std::array<float, 8U> wrong_layout{1.0F, 2.0F, 3.0F, 4.0F,
                                       5.0F, 6.0F, 7.0F, 8.0F};
    const auto before = wrong_layout;
    effect.process(wrong_layout, 4U);
    require(wrong_layout == before,
            "unsupported processing layout must leave caller audio untouched");

    SynthTextureReverbParameters bypass_parameters;
    bypass_parameters.mix = 0.8F;
    bypass_parameters.spillover = false;
    SynthTextureReverb bypass(48000.0F, 1U, bypass_parameters);
    std::array<float, 16U> dry{};
    dry[0] = 0.31F;
    dry[7] = -0.22F;
    const auto dry_before = dry;
    bypass.process_bypassed(dry, 1U);
    require(dry == dry_before,
            "bypass without spillover must be transparent");

    bypass_parameters.spillover = true;
    SynthTextureReverb tail(48000.0F, 1U, bypass_parameters);
    std::array<float, 4096U> hit{};
    hit[0] = 0.9F;
    tail.process(hit, 1U);
    std::array<float, 8192U> bypassed{};
    tail.process_bypassed(bypassed, 1U);
    require(std::any_of(bypassed.begin(), bypassed.end(), [](float sample) {
                return std::abs(sample) > 1.0e-7F;
            }),
            "spillover bypass should emit the stored wet tail");
}

void test_process_and_bypass_do_not_allocate() {
    SynthTextureReverbParameters parameters;
    parameters.spillover = true;
    SynthTextureReverb effect(48000.0F, 2U, parameters);
    std::array<float, 512U> block{};
    for (std::size_t index = 0U; index < block.size(); ++index) {
        block[index] = 0.16F * std::sin(0.13F * static_cast<float>(index));
    }
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    effect.process(block, 2U);
    effect.process_bypassed(block, 2U);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "audio processing must not allocate");
}

void test_invalid_constructor_arguments() {
    bool sample_rate_rejected = false;
    try {
        SynthTextureReverb invalid(1000.0F, 1U);
    } catch (const std::invalid_argument&) {
        sample_rate_rejected = true;
    }
    require(sample_rate_rejected, "out-of-range sample rate must be rejected");
    bool channels_rejected = false;
    try {
        SynthTextureReverb invalid(48000.0F, 3U);
    } catch (const std::invalid_argument&) {
        channels_rejected = true;
    }
    require(channels_rejected, "unsupported channel layout must be rejected");
}

}  // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_bounds_and_memory();
        test_mix_zero_is_exact_dry_and_latency_free();
        test_synthetic_wet_response_and_stereo_width();
        test_reset_and_block_partition_continuity();
        test_invalid_audio_channel_guard_and_bypass();
        test_process_and_bypass_do_not_allocate();
        test_invalid_constructor_arguments();
        std::cout << "All synthetic texture reverb tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Synthetic texture reverb test failure: "
                  << error.what() << '\n';
        return 1;
    }
}
