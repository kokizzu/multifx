#include "tabletfx/effects/prime_drum_machine.hpp"

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
#include <span>

namespace {

using tabletfx::effects::DrumMachine;
using tabletfx::effects::DrumMachineParameters;

std::atomic<std::size_t> allocation_count{0U};

constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kFramesPerCallback = 128U;
constexpr std::size_t kWarmupCallbacks = 2000U;
constexpr std::size_t kMeasuredCallbacks = 10000U;
constexpr double kDeadlineNanoseconds =
    static_cast<double>(kFramesPerCallback) / kSampleRate * 1.0e9;

double percentile(const std::array<double, kMeasuredCallbacks>& sorted,
                  const double fraction) {
    const std::size_t index = std::min(
        sorted.size() - 1U,
        static_cast<std::size_t>(std::ceil(fraction * sorted.size()) - 1.0));
    return sorted[index];
}

void render_callback(DrumMachine& effect,
                     std::array<float, kFramesPerCallback * kChannels>& block) {
    effect.process(block, kChannels);
}

double deadline_percent(const double duration_ns) {
    return duration_ns / kDeadlineNanoseconds * 100.0;
}

}  // namespace

void* operator new(const std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](const std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
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
    DrumMachineParameters parameters;
    parameters.mix = 0.6F;
    parameters.groove = 0.5F;
    parameters.reverb = 0.2F;
    parameters.drive = 0.15F;
    DrumMachine effect(kSampleRate, kChannels, parameters);
    effect.set_running(true);
    std::array<float, kFramesPerCallback * kChannels> input{};
    input.fill(0.1F);
    std::array<float, kFramesPerCallback * kChannels> block{};
    std::array<double, kMeasuredCallbacks> durations_ns{};

    for (std::size_t callback = 0U; callback < kWarmupCallbacks; ++callback) {
        block = input;
        render_callback(effect, block);
    }

    allocation_count.store(0U, std::memory_order_relaxed);
    std::size_t deadline_misses = 0U;
    for (std::size_t callback = 0U; callback < kMeasuredCallbacks; ++callback) {
        // Refill outside the timer so each callback receives fresh input rather
        // than the previously processed block.
        block = input;
        const auto begin = std::chrono::steady_clock::now();
        render_callback(effect, block);
        const auto end = std::chrono::steady_clock::now();
        durations_ns[callback] = std::chrono::duration<double, std::nano>(
                                     end - begin)
                                     .count();
        if (durations_ns[callback] >= kDeadlineNanoseconds) {
            ++deadline_misses;
        }
    }
    const std::size_t process_allocations =
        allocation_count.load(std::memory_order_relaxed);
    std::sort(durations_ns.begin(), durations_ns.end());

    std::printf("HeadRush Prime project-authored drum machine benchmark\n");
    std::printf("sample_rate=%.0f channels=%u block_frames=%zu callbacks=%zu "
                "input_restore=outside_timed_region\n",
                kSampleRate, static_cast<unsigned>(kChannels),
                kFramesPerCallback, kMeasuredCallbacks);
    std::printf("owned_memory_bytes=%zu process_allocations=%zu "
                "callback_deadline_us=%.2f deadline_misses=%zu/%zu\n",
                effect.memory_bytes(), process_allocations,
                kDeadlineNanoseconds / 1000.0, deadline_misses,
                kMeasuredCallbacks);
    std::printf("callback_time_percent_of_deadline p50=%.3f p95=%.3f "
                "p99=%.3f p99.9=%.3f max=%.3f\n",
                deadline_percent(percentile(durations_ns, 0.50)),
                deadline_percent(percentile(durations_ns, 0.95)),
                deadline_percent(percentile(durations_ns, 0.99)),
                deadline_percent(percentile(durations_ns, 0.999)),
                deadline_percent(durations_ns.back()));
    return process_allocations == 0U ? 0 : 1;
}
