#include "tabletfx/effects/tape_echo.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};
constexpr float kSampleRate = 48000.0F;
constexpr float kTolerance = 1.0e-5F;

void test_defaults_bounds_and_invalid_configuration() {
    using namespace tabletfx;
    TapeEcho effect;
    assert(effect.id() == "tape-echo");
    assert(effect.parameters().time_ms == 350.0F);
    assert(effect.parameters().feedback == 0.35F);
    assert(effect.parameters().mix == 0.25F);
    assert(effect.parameters().record_level == 0.25F);
    assert(effect.selected_time_ms() == 350.0F);
    assert(effect.memory_bytes() > sizeof(effect));

    TapeEchoParameters parameters = effect.parameters();
    parameters.time_ms = -4.0F;
    parameters.feedback = 1.0F;
    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    parameters.record_level = 3.0F;
    parameters.tape_bias = -1.0F;
    parameters.tape_noise = 4.0F;
    parameters.wow_flutter = std::numeric_limits<float>::infinity();
    parameters.tempo_bpm = 600.0F;
    effect.configure(parameters);
    assert(effect.parameters().time_ms == 1.0F);
    assert(effect.parameters().feedback == 0.95F);
    assert(effect.parameters().mix == 0.25F);
    assert(effect.parameters().record_level == 1.0F);
    assert(effect.parameters().tape_bias == 0.0F);
    assert(effect.parameters().tape_noise == 1.0F);
    assert(effect.parameters().wow_flutter == 0.15F);
    assert(effect.parameters().tempo_bpm == 300.0F);

    parameters = {};
    parameters.time_mode = DelayTimeMode::tempo_sync;
    parameters.tempo_bpm = 120.0F;
    parameters.tap_division = DelayDivision::quarter;
    effect.configure(parameters);
    assert(std::abs(effect.selected_time_ms() - 500.0F) < kTolerance);

    bool invalid_channels = false;
    try {
        TapeEcho unsupported(kSampleRate, 3U);
    } catch (const std::invalid_argument&) {
        invalid_channels = true;
    }
    assert(invalid_channels);

    bool invalid_rate = false;
    try {
        TapeEcho unsupported(7999.0F, 1U);
    } catch (const std::invalid_argument&) {
        invalid_rate = true;
    }
    assert(invalid_rate);
}

void test_free_time_impulse_and_mix() {
    using namespace tabletfx;
    TapeEchoParameters parameters;
    parameters.time_ms = 10.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.record_level = 0.0F;
    parameters.tape_bias = 0.0F;
    parameters.tape_noise = 0.0F;
    parameters.wow_flutter = 0.0F;
    TapeEcho effect(kSampleRate, 1U, 2000.0F, parameters);
    std::vector<float> audio(600U, 0.0F);
    audio[0] = 0.5F;
    effect.process(audio, 1U);
    assert(std::abs(audio[479U]) < kTolerance);
    assert(audio[480U] > 0.4F);
    assert(std::abs(audio[481U]) < kTolerance);

    parameters.mix = 0.0F;
    TapeEcho dry(kSampleRate, 1U, 2000.0F, parameters);
    std::array<float, 32U> dry_block{};
    dry_block[0] = -0.25F;
    dry.process(dry_block, 1U);
    assert(dry_block[0] == -0.25F);
    assert(std::all_of(dry_block.begin() + 1, dry_block.end(),
                       [](float value) { return value == 0.0F; }));
}

void test_tempo_synced_time() {
    using namespace tabletfx;
    TapeEchoParameters parameters;
    parameters.time_mode = DelayTimeMode::tempo_sync;
    parameters.tempo_bpm = 120.0F;
    parameters.tap_division = DelayDivision::quarter;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.record_level = 0.0F;
    parameters.tape_bias = 0.0F;
    parameters.tape_noise = 0.0F;
    parameters.wow_flutter = 0.0F;
    TapeEcho effect(kSampleRate, 1U, 2000.0F, parameters);
    std::vector<float> audio(24016U, 0.0F);
    audio[0] = 0.5F;
    effect.process(audio, 1U);
    assert(std::abs(audio[23999U]) < kTolerance);
    assert(audio[24000U] > 0.4F);
}

void test_record_level_bias_noise_and_modulation_are_active() {
    using namespace tabletfx;
    TapeEchoParameters clean;
    clean.time_ms = 40.0F;
    clean.feedback = 0.0F;
    clean.mix = 1.0F;
    clean.record_level = 0.0F;
    clean.tape_bias = 0.0F;
    clean.wow_flutter = 0.0F;
    constexpr std::size_t frames = 12000U;
    std::vector<float> input(frames, 0.0F);
    for (std::size_t index = 0U; index < input.size(); ++index) {
        input[index] = 0.8F * std::sin(0.037F * static_cast<float>(index));
    }

    auto low_record_audio = input;
    TapeEcho low_record(kSampleRate, 1U, 2000.0F, clean);
    low_record.process(low_record_audio, 1U);

    auto hot_record = clean;
    hot_record.record_level = 1.0F;
    hot_record.tape_bias = 1.0F;
    auto hot_record_audio = input;
    TapeEcho saturated(kSampleRate, 1U, 2000.0F, hot_record);
    saturated.process(hot_record_audio, 1U);
    double saturation_difference = 0.0;
    for (std::size_t index = 2000U; index < frames; ++index) {
        saturation_difference += std::abs(
            static_cast<double>(low_record_audio[index] - hot_record_audio[index]));
    }
    assert(saturation_difference > 1.0);

    auto noisy = clean;
    noisy.tape_noise = 1.0F;
    std::vector<float> silence(6000U, 0.0F);
    TapeEcho noise_effect(kSampleRate, 1U, 2000.0F, noisy);
    noise_effect.process(silence, 1U);
    double noise_energy = 0.0;
    for (std::size_t index = 2000U; index < silence.size(); ++index) {
        noise_energy += std::abs(silence[index]);
    }
    assert(noise_energy > 0.0);

    auto steady = clean;
    steady.time_ms = 70.0F;
    steady.feedback = 0.75F;
    auto moving = steady;
    moving.wow_flutter = 1.0F;
    std::vector<float> impulse_a(16000U, 0.0F);
    impulse_a[0] = 0.5F;
    auto impulse_b = impulse_a;
    TapeEcho fixed_echo(kSampleRate, 1U, 2000.0F, steady);
    TapeEcho mod_echo(kSampleRate, 1U, 2000.0F, moving);
    fixed_echo.process(impulse_a, 1U);
    mod_echo.process(impulse_b, 1U);
    double modulation_difference = 0.0;
    for (std::size_t index = 2000U; index < impulse_a.size(); ++index) {
        modulation_difference += std::abs(
            static_cast<double>(impulse_a[index] - impulse_b[index]));
    }
    assert(modulation_difference > 0.001);
}

void test_spillover_and_stereo_isolation() {
    using namespace tabletfx;
    TapeEchoParameters parameters;
    parameters.time_ms = 20.0F;
    parameters.feedback = 0.7F;
    parameters.mix = 0.5F;
    parameters.record_level = 0.0F;
    parameters.tape_bias = 0.0F;
    parameters.tape_noise = 0.0F;
    parameters.wow_flutter = 0.0F;
    parameters.spillover = true;
    TapeEcho effect(kSampleRate, 2U, 2000.0F, parameters);
    std::vector<float> stereo(1100U * 2U, 0.0F);
    stereo[0] = 0.5F;
    effect.process(stereo, 2U);
    std::array<float, 1000U * 2U> tail{};
    tail[0] = 0.1F;
    effect.process_bypassed(tail, 2U);
    assert(tail[0] > 0.09F);
    assert(tail[820U * 2U] > 0.05F);
    for (std::size_t frame = 0U; frame < tail.size() / 2U; ++frame) {
        assert(tail[frame * 2U + 1U] == 0.0F);
    }

    parameters.spillover = false;
    TapeEcho no_tail(kSampleRate, 1U, 2000.0F, parameters);
    std::array<float, 32U> no_tail_input{};
    no_tail_input[0] = 0.2F;
    no_tail.process_bypassed(no_tail_input, 1U);
    assert(no_tail_input[0] == 0.2F);
}

void test_invalid_samples_channel_mismatch_and_no_allocations() {
    using namespace tabletfx;
    TapeEcho effect;
    std::array<float, 64U> samples{};
    samples[3U] = std::numeric_limits<float>::quiet_NaN();
    samples[9U] = std::numeric_limits<float>::infinity();
    count_allocations.store(true, std::memory_order_release);
    effect.process(samples, 1U);
    count_allocations.store(false, std::memory_order_release);
    assert(allocation_count.load(std::memory_order_relaxed) == 0U);
    assert(std::all_of(samples.begin(), samples.end(), [](float value) {
        return std::isfinite(value);
    }));

    std::array<float, 16U> mismatch{};
    mismatch.fill(0.25F);
    const auto before = mismatch;
    effect.process(mismatch, 2U);
    assert(mismatch == before);
}

}  // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    test_defaults_bounds_and_invalid_configuration();
    test_free_time_impulse_and_mix();
    test_tempo_synced_time();
    test_record_level_bias_noise_and_modulation_are_active();
    test_spillover_and_stereo_isolation();
    test_invalid_samples_channel_mismatch_and_no_allocations();
}
