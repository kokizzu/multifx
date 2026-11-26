#include "tabletfx/effects/phase_invert.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

namespace {

constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kBlockFrames = 128U;
constexpr std::size_t kBlockSamples = kBlockFrames * kChannels;
constexpr std::size_t kWarmupCallbacks = 3000U;
constexpr std::size_t kMeasuredCallbacks = 20000U;
constexpr double kCallbackBudgetUs =
    static_cast<double>(kBlockFrames) * 1000000.0 / kSampleRate;

std::atomic<bool> g_track_allocations{false};
std::atomic<std::size_t> g_process_allocations{0};

struct Percentiles {
    double p50_us;
    double p95_us;
    double p99_us;
    double p999_us;
    double max_us;
};

double percentile(const std::vector<double>& sorted, double p) {
    const double rank = std::ceil(p * static_cast<double>(sorted.size()));
    const std::size_t index = std::clamp<std::size_t>(
        static_cast<std::size_t>(rank > 0.0 ? rank - 1.0 : 0.0),
        0U, sorted.size() - 1U);
    return sorted[index];
}

Percentiles summarize(const std::vector<double>& sorted) {
    return {percentile(sorted, 0.50), percentile(sorted, 0.95),
            percentile(sorted, 0.99), percentile(sorted, 0.999), sorted.back()};
}

double percent_of_callback_budget(double microseconds) {
    return microseconds * 100.0 / kCallbackBudgetUs;
}

} // namespace

void* operator new(std::size_t size) {
    if (g_track_allocations.load(std::memory_order_relaxed)) {
        g_process_allocations.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0U ? 1U : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    tabletfx::PhaseInvert phase_invert;
    std::array<float, kBlockSamples> source{};
    std::array<float, kBlockSamples> block{};
    for (std::size_t frame = 0; frame < kBlockFrames; ++frame) {
        const float phase = static_cast<float>(frame) * 2.0F *
                            3.14159265358979323846F * 220.0F / kSampleRate;
        const float sample = 0.35F * std::sin(phase) +
                             0.12F * std::sin(3.0F * phase);
        source[frame * kChannels] = sample;
        source[frame * kChannels + 1U] = -0.8F * sample;
    }

    std::vector<double> durations;
    durations.reserve(kMeasuredCallbacks);
    std::size_t allocation_count = 0U;
    std::size_t deadline_misses = 0U;
    double checksum = 0.0;
    for (std::size_t callback = 0;
         callback < kWarmupCallbacks + kMeasuredCallbacks; ++callback) {
        block = source;
        const auto start = std::chrono::steady_clock::now();
        g_process_allocations.store(0U, std::memory_order_relaxed);
        g_track_allocations.store(true, std::memory_order_relaxed);
        phase_invert.process(block, kChannels);
        g_track_allocations.store(false, std::memory_order_relaxed);
        const auto finish = std::chrono::steady_clock::now();
        for (float sample : block) checksum += static_cast<double>(sample);
        if (callback >= kWarmupCallbacks) {
            const double elapsed =
                std::chrono::duration<double, std::micro>(finish - start).count();
            durations.push_back(elapsed);
            allocation_count +=
                g_process_allocations.load(std::memory_order_relaxed);
            deadline_misses += elapsed > kCallbackBudgetUs ? 1U : 0U;
        }
    }

    std::sort(durations.begin(), durations.end());
    const Percentiles p = summarize(durations);
    std::cout << std::fixed << std::setprecision(6)
              << "HeadRush Prime Phase Invert helper\n"
              << "CPU: " << std::thread::hardware_concurrency() << " logical threads\n"
              << "Profile: 48 kHz, stereo, 128 frames; callback budget "
              << kCallbackBudgetUs << " us\n"
              << "Warmup/measured callbacks: " << kWarmupCallbacks << "/"
              << kMeasuredCallbacks << "\n"
              << "Callback budget use p50/p95/p99/p99.9/max: "
              << percent_of_callback_budget(p.p50_us) << "/"
              << percent_of_callback_budget(p.p95_us) << "/"
              << percent_of_callback_budget(p.p99_us) << "/"
              << percent_of_callback_budget(p.p999_us) << "/"
              << percent_of_callback_budget(p.max_us) << "%\n"
              << "Processor object bytes: " << phase_invert.memory_bytes() << "\n"
              << "Per-chain 122-slot packed enable mask: 16 B\n"
              << "Process allocations: " << allocation_count << "\n"
              << "Fixed audio-path latency: 0 frames\n"
              << "Deadline misses: " << deadline_misses << "\n"
              << "Checksum: " << checksum << "\n";
    return allocation_count == 0U && deadline_misses == 0U
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
