#include "tabletfx/effects/chorus.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
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

double budget_percent(double microseconds) {
    return microseconds * 100.0 / kCallbackBudgetUs;
}

Percentiles summarize(const std::vector<double>& sorted) {
    return {percentile(sorted, 0.50), percentile(sorted, 0.95),
            percentile(sorted, 0.99), percentile(sorted, 0.999), sorted.back()};
}

bool run_profile(std::string_view name, tabletfx::ChorusParameters parameters) {
    tabletfx::Chorus chorus(kSampleRate, kChannels, parameters);
    std::array<float, kBlockSamples> source{};
    std::array<float, kBlockSamples> block{};
    for (std::size_t frame = 0; frame < kBlockFrames; ++frame) {
        const float phase = static_cast<float>(frame) * 2.0F * 3.14159265358979323846F *
                            220.0F / kSampleRate;
        const float sample = 0.4F * std::sin(phase);
        source[frame * kChannels] = sample;
        source[frame * kChannels + 1U] = sample;
    }

    std::vector<double> durations;
    durations.reserve(kMeasuredCallbacks);
    double checksum = 0.0;
    bool finite = true;
    for (std::size_t callback = 0;
         callback < kWarmupCallbacks + kMeasuredCallbacks; ++callback) {
        std::copy(source.begin(), source.end(), block.begin());
        const auto start = std::chrono::steady_clock::now();
        chorus.process(block, kChannels);
        const auto finish = std::chrono::steady_clock::now();
        for (float sample : block) {
            finite = finite && std::isfinite(sample);
            checksum += static_cast<double>(sample);
        }
        if (callback >= kWarmupCallbacks) {
            durations.push_back(
                std::chrono::duration<double, std::micro>(finish - start).count());
        }
    }

    std::sort(durations.begin(), durations.end());
    const auto stats = summarize(durations);
    std::cout << name << " callbacks=" << kMeasuredCallbacks
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " number_of_voices="
              << static_cast<unsigned int>(parameters.number_of_voices)
              << " sample_rate_hz=" << static_cast<int>(kSampleRate)
              << " callback_budget_us=" << std::fixed << std::setprecision(3)
              << kCallbackBudgetUs
              << " memory_bytes=" << chorus.memory_bytes()
              << " algorithmic_latency_samples=0"
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
    return finite;
}

} // namespace

int main() {
    tabletfx::ChorusParameters free_rate;
    tabletfx::ChorusParameters quarter_sync;
    quarter_sync.tempo_sync = true;
    quarter_sync.tempo_bpm = 120.0F;
    bool passed = run_profile("chorus_free_0_8_hz", free_rate) &&
                  run_profile("chorus_quarter_sync_120_bpm", quarter_sync);
    for (const std::uint8_t voices :
         std::array<std::uint8_t, 4>{1U, 2U, 4U, 8U}) {
        tabletfx::ChorusParameters parameters;
        parameters.number_of_voices = voices;
        passed = run_profile("chorus_voice_count", parameters) && passed;
    }
    return passed ? 0 : 1;
}
