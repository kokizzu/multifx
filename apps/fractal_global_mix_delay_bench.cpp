#include "tabletfx/effects.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
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

double percentile(const std::vector<double>& sorted, double fraction) {
    const double rank = std::ceil(fraction * static_cast<double>(sorted.size()));
    const std::size_t index = std::clamp<std::size_t>(
        static_cast<std::size_t>(rank > 0.0 ? rank - 1.0 : 0.0),
        0U, sorted.size() - 1U);
    return sorted[index];
}

double budget_percent(double microseconds) {
    return microseconds * 100.0 / kBudgetUs;
}

bool run_profile(std::string_view name, bool global_mix_enabled) {
    tabletfx::Delay delay(kSampleRate, kChannels, 2000.0F, 250.0F, 0.35F, 0.3F);
    delay.configure_global_mix_offset(global_mix_enabled, 0.2F);
    delay.reset();

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
        const auto before = allocations.load(std::memory_order_relaxed);
        const auto start = std::chrono::steady_clock::now();
        delay.process(block, kChannels);
        const auto finish = std::chrono::steady_clock::now();
        process_allocations += allocations.load(std::memory_order_relaxed) - before;
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
    const float effective_mix = delay.effective_mix();
    std::cout << name
              << " callbacks=" << kMeasuredCallbacks
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " sample_rate_hz=" << static_cast<int>(kSampleRate)
              << " callback_budget_us=" << std::fixed << std::setprecision(3)
              << kBudgetUs
              << " base_mix=" << delay.mix()
              << " global_mix_enabled=" << (global_mix_enabled ? "true" : "false")
              << " global_mix_offset=0.2"
              << " effective_mix=" << effective_mix
              << " memory_bytes=" << delay.memory_bytes()
              << " fixed_added_latency_frames=0"
              << " process_allocations=" << process_allocations
              << " deadline_misses=" << deadline_misses
              << " p50_percent=" << budget_percent(percentile(durations, 0.50))
              << " p95_percent=" << budget_percent(percentile(durations, 0.95))
              << " p99_percent=" << budget_percent(percentile(durations, 0.99))
              << " p99_9_percent=" << budget_percent(percentile(durations, 0.999))
              << " max_percent=" << budget_percent(durations.back())
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
    const bool disabled_ok = run_profile("delay_global_mix_disabled", false);
    const bool enabled_ok = run_profile("delay_global_mix_enabled_plus_20pp", true);
    return disabled_ok && enabled_ok ? 0 : 1;
}
