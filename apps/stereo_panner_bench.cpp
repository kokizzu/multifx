#include "tabletfx/effects/stereo_panner.hpp"

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

using Clock = std::chrono::steady_clock;
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

std::array<float, kBlockSamples> source_audio() {
    std::array<float, kBlockSamples> audio{};
    for (std::size_t frame = 0U; frame < kBlockFrames; ++frame) {
        const double phase = 2.0 * 3.14159265358979323846 *
                             220.0 * static_cast<double>(frame) / kSampleRate;
        const float sample = static_cast<float>(
            0.16 * std::sin(phase) + 0.035 * std::sin(phase * 2.71));
        audio[frame * 2U] = sample;
        audio[frame * 2U + 1U] = sample * 0.83F;
    }
    return audio;
}

bool run_profile(std::string_view name, tabletfx::StereoPannerWaveform waveform,
                 const std::array<float, kBlockSamples>& source) {
    tabletfx::StereoPannerParameters parameters;
    parameters.rate_hz = 0.5F;
    parameters.width_percent = 100.0F;
    parameters.waveform = waveform;
    parameters.phase_degrees = 180.0F;
    parameters.smoothing_ms = 10.0F;
    tabletfx::StereoPanner panner(kSampleRate, parameters);
    auto block = source;

    for (std::size_t callback = 0U; callback < kWarmupCallbacks; ++callback) {
        block = source;
        panner.process(block, kChannels);
    }

    std::vector<double> elapsed_us;
    elapsed_us.reserve(kMeasuredCallbacks);
    for (std::size_t callback = 0U; callback < kMeasuredCallbacks; ++callback) {
        block = source;
        const auto start = Clock::now();
        panner.process(block, kChannels);
        const auto end = Clock::now();
        elapsed_us.push_back(
            std::chrono::duration<double, std::micro>(end - start).count());
    }
    std::sort(elapsed_us.begin(), elapsed_us.end());

    const Percentiles stats{
        percentile(elapsed_us, 0.50), percentile(elapsed_us, 0.95),
        percentile(elapsed_us, 0.99), percentile(elapsed_us, 0.999),
        elapsed_us.back(),
    };
    const auto budget_percent = [](double duration_us) {
        return duration_us / kCallbackBudgetUs * 100.0;
    };
    const bool finite = std::all_of(block.begin(), block.end(),
                                    [](float sample) { return std::isfinite(sample); });
    std::cout << "profile=" << name
              << " callbacks=" << kMeasuredCallbacks
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " sample_rate_hz=" << static_cast<int>(kSampleRate)
              << " callback_budget_us=" << std::fixed << std::setprecision(3)
              << kCallbackBudgetUs
              << " memory_bytes=" << panner.memory_bytes()
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
              << " finite_output=" << (finite ? "true" : "false") << '\n';
    return finite;
}

} // namespace

int main() {
    const auto source = source_audio();
    const bool sine = run_profile("stereo_panner_sine",
                                  tabletfx::StereoPannerWaveform::Sine, source);
    const bool envelope = run_profile("stereo_panner_envelope",
                                      tabletfx::StereoPannerWaveform::Envelope,
                                      source);
    return sine && envelope ? 0 : 1;
}
