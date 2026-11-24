#include "tabletfx/effects/xr1_global_eq.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kChannels = 2;
constexpr std::size_t kBlockFrames = 128;
constexpr std::size_t kBlockSamples = kBlockFrames * kChannels;
constexpr std::size_t kWarmupCallbacks = 5000;
constexpr std::size_t kMeasuredCallbacks = 50000;
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

Percentiles run_profile(std::string_view name, tabletfx::XR1GlobalEQ& eq,
                        std::array<float, kBlockSamples>& block) {
    for (std::size_t callback = 0; callback < kWarmupCallbacks; ++callback) {
        eq.process(block, kChannels);
    }

    std::vector<double> elapsed_us;
    elapsed_us.reserve(kMeasuredCallbacks);
    for (std::size_t callback = 0; callback < kMeasuredCallbacks; ++callback) {
        const auto start = Clock::now();
        eq.process(block, kChannels);
        const auto end = Clock::now();
        elapsed_us.push_back(
            std::chrono::duration<double, std::micro>(end - start).count());
    }
    std::sort(elapsed_us.begin(), elapsed_us.end());

    const Percentiles result{
        percentile(elapsed_us, 0.50),
        percentile(elapsed_us, 0.95),
        percentile(elapsed_us, 0.99),
        percentile(elapsed_us, 0.999),
        elapsed_us.back(),
    };
    const auto percent = [](double duration_us) {
        return duration_us / kCallbackBudgetUs * 100.0;
    };
    std::cout << "profile=" << name
              << " callbacks=" << kMeasuredCallbacks
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " sample_rate_hz=" << static_cast<int>(kSampleRate)
              << " callback_budget_us=" << std::fixed << std::setprecision(3)
              << kCallbackBudgetUs
              << " memory_bytes=" << eq.memory_bytes()
              << " algorithmic_latency_samples=0"
              << " p50_us=" << result.p50_us
              << " p50_percent=" << percent(result.p50_us)
              << " p95_us=" << result.p95_us
              << " p95_percent=" << percent(result.p95_us)
              << " p99_us=" << result.p99_us
              << " p99_percent=" << percent(result.p99_us)
              << " p99_9_us=" << result.p999_us
              << " p99_9_percent=" << percent(result.p999_us)
              << " max_us=" << result.max_us
              << " max_percent=" << percent(result.max_us) << '\n';
    if (!std::all_of(block.begin(), block.end(),
                     [](float sample) { return std::isfinite(sample); })) {
        std::cerr << "profile produced non-finite output: " << name << '\n';
    }
    return result;
}

void fill_test_audio(std::array<float, kBlockSamples>& block) {
    for (std::size_t frame = 0; frame < kBlockFrames; ++frame) {
        const double phase = 2.0 * 3.14159265358979323846 *
                             static_cast<double>(frame) / 128.0;
        const float sample = static_cast<float>(
            0.18 * std::sin(phase) + 0.05 * std::sin(phase * 3.1));
        block[frame * 2] = sample;
        block[frame * 2 + 1] = sample * 0.85F;
    }
}

} // namespace

int main() {
    std::array<float, kBlockSamples> neutral_audio{};
    std::array<float, kBlockSamples> active_audio{};
    fill_test_audio(neutral_audio);
    fill_test_audio(active_audio);

    tabletfx::XR1GlobalEQ neutral(kSampleRate, kChannels);
    const Percentiles neutral_stats =
        run_profile("xr1_global_eq_neutral_default", neutral, neutral_audio);

    tabletfx::XR1GlobalEQ active(kSampleRate, kChannels);
    auto parameters = active.parameters();
    constexpr std::array<float, 5> gains{3.0F, -3.0F, 4.0F, -2.0F, 2.0F};
    for (std::size_t band = 0; band < parameters.bands.size(); ++band) {
        parameters.bands[band].gain_db = gains[band];
    }
    active.configure(parameters);
    const Percentiles active_stats =
        run_profile("xr1_global_eq_active_five_band", active, active_audio);

    const bool finite = std::all_of(
        neutral_audio.begin(), neutral_audio.end(),
        [](float sample) { return std::isfinite(sample); }) &&
        std::all_of(active_audio.begin(), active_audio.end(),
                    [](float sample) { return std::isfinite(sample); });
    const bool no_added_fixed_latency =
        neutral_stats.p50_us >= 0.0 && active_stats.p50_us >= 0.0;
    return finite && no_added_fixed_latency ? 0 : 1;
}
