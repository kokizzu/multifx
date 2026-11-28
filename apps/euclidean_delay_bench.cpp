#include "tabletfx/effects/euclidean_delay.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {

std::atomic<std::size_t> allocation_count{0U};

constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kFramesPerCallback = 128U;
constexpr std::size_t kWarmupCallbacks = 3000U;
constexpr std::size_t kMeasuredCallbacks = 20000U;
constexpr double kCallbackBudgetNanoseconds =
    static_cast<double>(kFramesPerCallback) * 1.0e9 / kSampleRate;

using AudioBlock = std::array<float, kFramesPerCallback * kChannels>;
using Timings = std::array<double, kMeasuredCallbacks>;

double percentile(const Timings& sorted, double fraction) {
    const std::size_t index = std::min(
        sorted.size() - 1U,
        static_cast<std::size_t>(std::ceil(fraction * sorted.size()) - 1.0));
    return sorted[index];
}

double deadline_percent(double nanoseconds) noexcept {
    return nanoseconds / kCallbackBudgetNanoseconds * 100.0;
}

void fill_input(AudioBlock& block, std::size_t callback) noexcept {
    const float base = static_cast<float>(callback * kFramesPerCallback);
    for (std::size_t frame = 0U; frame < kFramesPerCallback; ++frame) {
        const float phase = (base + static_cast<float>(frame)) * 0.013F;
        block[frame * 2U] = 0.4F * std::sin(phase);
        block[frame * 2U + 1U] = 0.3F * std::sin(phase + 0.21F);
    }
}

void benchmark_profile(const char* name,
                       tabletfx::EuclideanDelayParameters parameters) {
    tabletfx::EuclideanDelay effect(kSampleRate, parameters);
    AudioBlock block{};
    Timings durations{};
    for (std::size_t callback = 0U; callback < kWarmupCallbacks; ++callback) {
        fill_input(block, callback);
        effect.process(block, kChannels);
    }

    allocation_count.store(0U, std::memory_order_relaxed);
    for (std::size_t callback = 0U; callback < kMeasuredCallbacks; ++callback) {
        fill_input(block, callback + kWarmupCallbacks);
        const auto begin = std::chrono::steady_clock::now();
        effect.process(block, kChannels);
        const auto end = std::chrono::steady_clock::now();
        durations[callback] =
            std::chrono::duration<double, std::nano>(end - begin).count();
    }
    const std::size_t process_allocations =
        allocation_count.load(std::memory_order_relaxed);
    std::sort(durations.begin(), durations.end());
    const std::size_t deadline_misses = static_cast<std::size_t>(
        std::count_if(durations.begin(), durations.end(),
                      [](double time) {
                          return time >= kCallbackBudgetNanoseconds;
                      }));

    std::printf(
        "%s: steps=%u pulses=%u rotation=%u step_ms=%.2f feedback=%.2f "
        "mix=%.2f memory_bytes=%zu fixed_latency_frames=%zu "
        "process_allocations=%zu deadline_misses=%zu/%zu "
        "callback_load_percent_p50/p95/p99/p99.9/max="
        "%.4f/%.4f/%.4f/%.4f/%.4f\n",
        name, static_cast<unsigned>(effect.parameters().steps),
        static_cast<unsigned>(effect.parameters().pulses),
        static_cast<unsigned>(effect.parameters().rotation),
        effect.effective_step_time_ms(), effect.parameters().feedback,
        effect.parameters().mix, effect.memory_bytes(),
        effect.fixed_latency_frames(), process_allocations, deadline_misses,
        kMeasuredCallbacks, deadline_percent(percentile(durations, 0.50)),
        deadline_percent(percentile(durations, 0.95)),
        deadline_percent(percentile(durations, 0.99)),
        deadline_percent(percentile(durations, 0.999)),
        deadline_percent(durations.back()));
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0U ? 1U : size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0U ? 1U : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    std::printf(
        "Project-authored Euclidean Delay, not Stadium DSP parity; "
        "sample_rate=%.0f channels=%u frames=%zu deadline_us=%.4f "
        "warmup=%zu measured=%zu\n",
        kSampleRate, static_cast<unsigned>(kChannels), kFramesPerCallback,
        kCallbackBudgetNanoseconds / 1000.0, kWarmupCallbacks,
        kMeasuredCallbacks);

    tabletfx::EuclideanDelayParameters parameters;
    benchmark_profile("default", parameters);
    parameters.pulses = 1U;
    parameters.steps = 16U;
    benchmark_profile("sparse_1_of_16", parameters);
    parameters.pulses = 8U;
    benchmark_profile("dense_8_of_16", parameters);
    parameters.pulses = 0U;
    benchmark_profile("muted_0_of_16", parameters);
    return 0;
}
