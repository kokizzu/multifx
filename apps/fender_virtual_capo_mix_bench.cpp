#include "tabletfx/effects/poly_pitch_shifter.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kChannels = 2;
constexpr std::size_t kBlockFrames = 128;
constexpr std::size_t kBlockSamples = kBlockFrames * kChannels;
constexpr std::size_t kWarmupCallbacks = 3000;
constexpr std::size_t kMeasuredCallbacks = 20000;
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
    if (sorted.empty()) return 0.0;
    const double rank = std::ceil(p * static_cast<double>(sorted.size()));
    const std::size_t index = std::clamp<std::size_t>(
        static_cast<std::size_t>(rank > 0.0 ? rank - 1.0 : 0.0),
        0, sorted.size() - 1);
    return sorted[index];
}

std::array<float, kBlockSamples> test_audio() {
    std::array<float, kBlockSamples> block{};
    for (std::size_t frame = 0; frame < kBlockFrames; ++frame) {
        const double phase = 2.0 * 3.14159265358979323846 *
                             220.0 * static_cast<double>(frame) / kSampleRate;
        const float sample = static_cast<float>(
            0.18 * std::sin(phase) + 0.04 * std::sin(phase * 2.97));
        block[frame * kChannels] = sample;
        block[frame * kChannels + 1] = sample * 0.82F;
    }
    return block;
}

bool run_profile(const char* name, float mix) {
    tabletfx::PolyPitchShifter shifter(kSampleRate, kChannels);
    shifter.set_pitch_semitones(7.0F);
    shifter.set_mix(mix);
    const auto source = test_audio();
    auto block = source;

    for (std::size_t callback = 0; callback < kWarmupCallbacks; ++callback) {
        block = source;
        shifter.process(block, kChannels);
    }

    std::vector<double> elapsed_us;
    elapsed_us.reserve(kMeasuredCallbacks);
    for (std::size_t callback = 0; callback < kMeasuredCallbacks; ++callback) {
        block = source;
        const auto start = Clock::now();
        shifter.process(block, kChannels);
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
              << " mix_percent=" << std::fixed << std::setprecision(1) << mix * 100.0F
              << " callbacks=" << kMeasuredCallbacks
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " sample_rate_hz=" << static_cast<int>(kSampleRate)
              << " callback_budget_us=" << std::setprecision(3) << kCallbackBudgetUs
              << " memory_bytes=" << shifter.memory_bytes()
              << " algorithmic_latency_samples=256"
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
    const bool fully_shifted = run_profile("fully_shifted", 1.0F);
    const bool equal_blend = run_profile("equal_shifted_original", 0.5F);
    return fully_shifted && equal_blend ? 0 : 1;
}
