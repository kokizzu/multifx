#include "tabletfx/effects/tremolo.hpp"

#include <algorithm>
#include <array>
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
constexpr double kCallbackBudgetUs =
    static_cast<double>(kBlockFrames) * 1000000.0 / kSampleRate;

bool g_track_process_allocations = false;
std::size_t g_process_allocations = 0U;

struct Percentiles {
    double p50_us = 0.0;
    double p95_us = 0.0;
    double p99_us = 0.0;
    double p999_us = 0.0;
    double max_us = 0.0;
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

double budget_percent(double microseconds) {
    return microseconds * 100.0 / kCallbackBudgetUs;
}

bool run_profile(std::string_view name, tabletfx::TremoloWaveform waveform,
                 float duty_cycle_percent) {
    tabletfx::TremoloParameters parameters;
    parameters.rate_hz = 4.0F;
    parameters.depth = 0.8F;
    parameters.waveform = waveform;
    parameters.duty_cycle_percent = duty_cycle_percent;
    parameters.depth_smoothing_ms = 0.0F;
    tabletfx::Tremolo tremolo(kSampleRate, parameters);

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
    double checksum = 0.0;
    bool finite = true;
    for (std::size_t callback = 0;
         callback < kWarmupCallbacks + kMeasuredCallbacks; ++callback) {
        std::copy(source.begin(), source.end(), block.begin());
        const auto start = std::chrono::steady_clock::now();
        g_track_process_allocations = true;
        tremolo.process(block, kChannels);
        g_track_process_allocations = false;
        const auto finish = std::chrono::steady_clock::now();
        for (float sample : block) {
            finite = finite && std::isfinite(sample);
            checksum += static_cast<double>(sample);
        }
        if (callback >= kWarmupCallbacks) {
            durations.push_back(
                std::chrono::duration<double, std::micro>(finish - start).count());
            process_allocations += g_process_allocations;
        }
        g_process_allocations = 0U;
    }

    std::sort(durations.begin(), durations.end());
    const auto stats = summarize(durations);
    std::cout << name << " callbacks=" << kMeasuredCallbacks
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " sample_rate_hz=" << static_cast<int>(kSampleRate)
              << " callback_budget_us=" << std::fixed << std::setprecision(3)
              << kCallbackBudgetUs
              << " memory_bytes=" << tremolo.memory_bytes()
              << " algorithmic_latency_samples=0"
              << " process_allocations=" << process_allocations
              << " p50_us=" << stats.p50_us
              << " p50_percent=" << budget_percent(stats.p50_us)
              << " p95_us=" << stats.p95_us
              << " p95_percent=" << budget_percent(stats.p95_us)
              << " p99_us=" << stats.p99_us
              << " p99_percent=" << budget_percent(stats.p99_us)
              << " p99_9_us=" << stats.p999_us
              << " p99_9_percent=" << budget_percent(stats.p999_us)
              << " max_us=" << stats.max_us
              << " max_percent=" << budget_percent(stats.max_us)
              << " finite_output=" << (finite ? "true" : "false")
              << " checksum=" << checksum << '\n';
    return finite && process_allocations == 0U;
}

} // namespace

void* operator new(std::size_t size) {
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        if (g_track_process_allocations) {
            ++g_process_allocations;
        }
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        if (g_track_process_allocations) {
            ++g_process_allocations;
        }
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    return run_profile("tremolo_square_duty_50_percent",
                       tabletfx::TremoloWaveform::Square, 50.0F) &&
                   run_profile("tremolo_square_duty_20_percent",
                               tabletfx::TremoloWaveform::Square, 20.0F) &&
                   run_profile("tremolo_sine_duty_50_percent_control",
                               tabletfx::TremoloWaveform::Sine, 50.0F)
               ? 0
               : 1;
}
