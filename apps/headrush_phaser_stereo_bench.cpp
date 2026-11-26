#include "tabletfx/effects/phaser.hpp"

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

Percentiles summarize(const std::vector<double>& sorted) {
    return {percentile(sorted, 0.50), percentile(sorted, 0.95),
            percentile(sorted, 0.99), percentile(sorted, 0.999), sorted.back()};
}

bool run_profile(bool stereo) {
    tabletfx::PhaserParameters parameters;
    parameters.rate_hz = 0.8F;
    parameters.minimum_hz = 180.0F;
    parameters.maximum_hz = 2400.0F;
    parameters.feedback = 0.25F;
    parameters.stages = 6U;
    parameters.mix = 0.65F;
    parameters.stereo = stereo;
    tabletfx::Phaser phaser(kSampleRate, kChannels, parameters);

    std::array<float, kBlockSamples> source{};
    std::array<float, kBlockSamples> block{};
    for (std::size_t frame = 0; frame < kBlockFrames; ++frame) {
        const float phase = static_cast<float>(frame) * 2.0F *
                            3.14159265358979323846F * 220.0F / kSampleRate;
        source[frame * kChannels] =
            0.28F * std::sin(phase) + 0.08F * std::sin(3.0F * phase);
        source[frame * kChannels + 1U] =
            0.25F * std::sin(phase + 0.1F) + 0.07F * std::sin(3.0F * phase + 0.1F);
    }

    std::vector<double> durations;
    durations.reserve(kMeasuredCallbacks);
    bool finite = true;
    double checksum = 0.0;
    for (std::size_t callback = 0;
         callback < kWarmupCallbacks + kMeasuredCallbacks; ++callback) {
        std::copy(source.begin(), source.end(), block.begin());
        const auto start = std::chrono::steady_clock::now();
        phaser.process(block, kChannels);
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
    const auto percent = [](double microseconds) {
        return microseconds * 100.0 / kCallbackBudgetUs;
    };
    std::cout << "profile=headrush_prime_orange_phaser_stereo_"
              << (stereo ? "on" : "off")
              << " callbacks=" << kMeasuredCallbacks
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " sample_rate_hz=" << static_cast<int>(kSampleRate)
              << " callback_budget_us=" << std::fixed << std::setprecision(3)
              << kCallbackBudgetUs
              << " memory_bytes=" << phaser.memory_bytes()
              << " fixed_audio_path_latency_frames=0"
              << " p50_us=" << stats.p50_us
              << " p50_percent=" << percent(stats.p50_us)
              << " p95_us=" << stats.p95_us
              << " p95_percent=" << percent(stats.p95_us)
              << " p99_us=" << stats.p99_us
              << " p99_percent=" << percent(stats.p99_us)
              << " p99_9_us=" << stats.p999_us
              << " p99_9_percent=" << percent(stats.p999_us)
              << " max_us=" << stats.max_us
              << " max_percent=" << percent(stats.max_us)
              << " finite_output=" << (finite ? "true" : "false")
              << " checksum=" << checksum << '\n';
    return finite;
}

} // namespace

int main() {
    if (!run_profile(true) || !run_profile(false)) {
        std::cerr << "Phaser stereo benchmark produced non-finite output.\n";
        return 1;
    }
    return 0;
}
