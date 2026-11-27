#include "tabletfx/effects/flanger.hpp"

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
#include <string_view>
#include <vector>

namespace {

constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kBlockFrames = 128U;
constexpr std::size_t kBlockSamples = kBlockFrames * kChannels;
constexpr std::size_t kWarmupCallbacks = 3000U;
constexpr std::size_t kMeasuredCallbacks = 20000U;
constexpr double kBudgetUs = static_cast<double>(kBlockFrames) * 1.0e6 /
                             static_cast<double>(kSampleRate);
std::atomic<std::size_t> allocations{0U};

struct Percentiles {
    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double p999 = 0.0;
    double maximum = 0.0;
};

double percentile(const std::vector<double>& sorted, double fraction) {
    const double rank = std::ceil(fraction * static_cast<double>(sorted.size()));
    const std::size_t index = std::clamp<std::size_t>(
        static_cast<std::size_t>(rank > 0.0 ? rank - 1.0 : 0.0),
        0U, sorted.size() - 1U);
    return sorted[index];
}

Percentiles summarize(const std::vector<double>& sorted) {
    return {percentile(sorted, 0.50), percentile(sorted, 0.95),
            percentile(sorted, 0.99), percentile(sorted, 0.999), sorted.back()};
}

double budget_percent(double microseconds) {
    return microseconds * 100.0 / kBudgetUs;
}

bool run_profile(std::string_view name, tabletfx::FlangerParameters parameters) {
    tabletfx::Flanger flanger(kSampleRate, kChannels, parameters);
    std::array<float, kBlockSamples> source{};
    std::array<float, kBlockSamples> block{};
    for (std::size_t frame = 0; frame < kBlockFrames; ++frame) {
        const float phase = static_cast<float>(frame) * 2.0F *
                            3.14159265358979323846F * 220.0F / kSampleRate;
        const float sample = 0.4F * std::sin(phase);
        source[frame * kChannels] = sample;
        source[frame * kChannels + 1U] = sample;
    }

    std::vector<double> durations;
    durations.reserve(kMeasuredCallbacks);
    std::size_t process_allocations = 0U;
    std::size_t deadline_misses = 0U;
    double checksum = 0.0;
    bool finite = true;
    for (std::size_t callback = 0U;
         callback < kWarmupCallbacks + kMeasuredCallbacks; ++callback) {
        std::copy(source.begin(), source.end(), block.begin());
        const auto allocation_count_before = allocations.load(std::memory_order_relaxed);
        const auto start = std::chrono::steady_clock::now();
        flanger.process(block, kChannels);
        const auto finish = std::chrono::steady_clock::now();
        process_allocations += allocations.load(std::memory_order_relaxed) -
                               allocation_count_before;
        for (float sample : block) {
            finite = finite && std::isfinite(sample);
            checksum += static_cast<double>(sample);
        }
        if (callback >= kWarmupCallbacks) {
            const double elapsed = std::chrono::duration<double, std::micro>(
                finish - start).count();
            durations.push_back(elapsed);
            deadline_misses += elapsed > kBudgetUs ? 1U : 0U;
        }
    }

    std::sort(durations.begin(), durations.end());
    const auto stats = summarize(durations);
    const float effective_mix = flanger.effective_mix();
    std::cout << name
              << " callbacks=" << kMeasuredCallbacks
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " sample_rate_hz=" << static_cast<int>(kSampleRate)
              << " callback_budget_us=" << std::fixed << std::setprecision(3)
              << kBudgetUs
              << " base_mix=" << parameters.mix
              << " global_mix_enabled=" << (parameters.global_mix_enabled ? "true" : "false")
              << " global_mix_offset=" << parameters.global_mix_offset
              << " effective_mix=" << effective_mix
              << " memory_bytes=" << flanger.memory_bytes()
              << " fixed_added_latency_frames=0"
              << " process_allocations=" << process_allocations
              << " deadline_misses=" << deadline_misses
              << " p50_us=" << stats.p50 << " p50_percent=" << budget_percent(stats.p50)
              << " p95_us=" << stats.p95 << " p95_percent=" << budget_percent(stats.p95)
              << " p99_us=" << stats.p99 << " p99_percent=" << budget_percent(stats.p99)
              << " p99_9_us=" << stats.p999 << " p99_9_percent=" << budget_percent(stats.p999)
              << " max_us=" << stats.maximum
              << " max_percent=" << budget_percent(stats.maximum)
              << " finite_output=" << (finite ? "true" : "false")
              << " checksum=" << checksum << '\n';
    return finite && process_allocations == 0U;
}

} // namespace

void* operator new(std::size_t size) {
    allocations.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocations.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    tabletfx::FlangerParameters disabled;
    disabled.mix = 0.35F;
    disabled.global_mix_offset = 0.2F;
    const bool disabled_ok = run_profile("flanger_global_mix_disabled", disabled);
    disabled.global_mix_enabled = true;
    const bool enabled_ok = run_profile("flanger_global_mix_enabled_plus_20pp", disabled);
    return disabled_ok && enabled_ok ? 0 : 1;
}
