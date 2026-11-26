#include "tabletfx/effects/manual_thru_zero_flanger.hpp"

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

Percentiles summarize(const std::vector<double>& sorted) {
    return {percentile(sorted, 0.50), percentile(sorted, 0.95),
            percentile(sorted, 0.99), percentile(sorted, 0.999), sorted.back()};
}

double budget_percent(double microseconds) {
    return microseconds * 100.0 / kCallbackBudgetUs;
}

bool run_profile(std::string_view name,
                 tabletfx::FlangerVcoResponse response,
                 float exponent) {
    tabletfx::ManualThruZeroFlangerParameters parameters;
    parameters.mode = tabletfx::ThruZeroMode::Auto;
    parameters.lfo_waveform = tabletfx::ThruZeroLfoWaveform::Triangle;
    parameters.lfo_rate_hz = 0.8F;
    parameters.lfo_depth = 10.0F;
    parameters.lfo_phase_degrees = 180.0F;
    parameters.vco_response = response;
    parameters.vco_exponent = exponent;
    parameters.smoothing_ms = 0.0F;
    tabletfx::ManualThruZeroFlanger flanger(kSampleRate, kChannels, parameters);

    std::array<float, kBlockSamples> source{};
    std::array<float, kBlockSamples> block{};
    for (std::size_t frame = 0; frame < kBlockFrames; ++frame) {
        const float phase = static_cast<float>(frame) * 2.0F *
                            3.14159265358979323846F * 110.0F / kSampleRate;
        const float left = 0.30F * std::sin(phase) + 0.10F * std::sin(3.0F * phase);
        const float right = 0.27F * std::sin(phase + 0.12F) +
                            0.08F * std::sin(3.0F * phase + 0.12F);
        source[frame * kChannels] = left;
        source[frame * kChannels + 1U] = right;
    }

    std::vector<double> durations;
    durations.reserve(kMeasuredCallbacks);
    double checksum = 0.0;
    bool finite = true;
    for (std::size_t callback = 0;
         callback < kWarmupCallbacks + kMeasuredCallbacks; ++callback) {
        std::copy(source.begin(), source.end(), block.begin());
        const auto start = std::chrono::steady_clock::now();
        flanger.process(block, kChannels);
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
    std::cout << "profile=" << name
              << " callbacks=" << kMeasuredCallbacks
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " sample_rate_hz=" << static_cast<int>(kSampleRate)
              << " callback_budget_us=" << std::fixed << std::setprecision(3)
              << kCallbackBudgetUs
              << " vco_response=" << static_cast<unsigned int>(response)
              << " vco_exponent=" << exponent
              << " memory_bytes=" << flanger.memory_bytes()
              << " configured_max_path_latency_frames="
              << flanger.maximum_path_latency_frames()
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
    const bool passed =
        run_profile("linear", tabletfx::FlangerVcoResponse::Linear, 1.0F) &&
        run_profile("exponential_1_5", tabletfx::FlangerVcoResponse::Exponential, 1.5F) &&
        run_profile("exponential_4", tabletfx::FlangerVcoResponse::Exponential, 4.0F) &&
        run_profile("square_law", tabletfx::FlangerVcoResponse::SquareLaw, 1.0F);
    if (!passed) {
        std::cerr << "Flanger VCO response benchmark produced non-finite output.\n";
        return 1;
    }
    return 0;
}
