#include "tabletfx/effects/tone_master_pro_3d_chorus.hpp"

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

constexpr float kSampleRate = 48'000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kFramesPerCallback = 128U;
constexpr std::size_t kWarmupCallbacks = 3'000U;
constexpr std::size_t kMeasuredCallbacks = 20'000U;
constexpr double kCallbackDeadlineNs =
    1'000'000'000.0 * static_cast<double>(kFramesPerCallback) / kSampleRate;

double percentile(std::vector<double> values, double fraction) {
    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(fraction * static_cast<double>(values.size() - 1U)));
    return values[std::min(index, values.size() - 1U)];
}

double percent_of_deadline(double nanoseconds) {
    return 100.0 * nanoseconds / kCallbackDeadlineNs;
}

void run_mode(tabletfx::ToneMasterPro3DChorusMode mode,
              std::string_view label) {
    tabletfx::ToneMasterPro3DChorus chorus(kSampleRate, kChannels);
    chorus.set_mode(mode);
    std::array<float, kFramesPerCallback * kChannels> block{};
    for (std::size_t frame = 0; frame < kFramesPerCallback; ++frame) {
        const float time = static_cast<float>(frame) / kSampleRate;
        const float value = 0.2F * std::sin(2.0F * 3.14159265358979323846F *
                                            110.0F * time);
        block[frame * 2U] = value;
        block[frame * 2U + 1U] = value;
    }
    for (std::size_t callback = 0; callback < kWarmupCallbacks; ++callback) {
        chorus.process(block, kChannels);
    }

    std::vector<double> timings;
    timings.reserve(kMeasuredCallbacks);
    for (std::size_t callback = 0; callback < kMeasuredCallbacks; ++callback) {
        const auto start = std::chrono::steady_clock::now();
        chorus.process(block, kChannels);
        const auto end = std::chrono::steady_clock::now();
        timings.push_back(std::chrono::duration<double, std::nano>(end - start)
                              .count());
    }

    std::cout << label << " memory_bytes=" << chorus.memory_bytes()
              << " p50=" << percent_of_deadline(percentile(timings, 0.50))
              << "% p95=" << percent_of_deadline(percentile(timings, 0.95))
              << "% p99=" << percent_of_deadline(percentile(timings, 0.99))
              << "% p99.9=" << percent_of_deadline(percentile(timings, 0.999))
              << "% max=" << percent_of_deadline(*std::max_element(
                     timings.begin(), timings.end()))
              << "%\n";
}

} // namespace

int main() {
    std::cout << std::fixed << std::setprecision(4)
              << "Tone Master Pro 3D Chorus project-authored DSP benchmark; "
              << "48 kHz stereo, 128 frames (2.6667 ms callback), "
              << kWarmupCallbacks << " warm-up and " << kMeasuredCallbacks
              << " measured callbacks. Percentages are isolated callback cost, "
              << "not total-chain or device latency.\n";
    run_mode(tabletfx::ToneMasterPro3DChorusMode::Mode1, "mode1-subtle");
    run_mode(tabletfx::ToneMasterPro3DChorusMode::Mode2, "mode2");
    run_mode(tabletfx::ToneMasterPro3DChorusMode::Mode3, "mode3");
    run_mode(tabletfx::ToneMasterPro3DChorusMode::Mode4, "mode4-pronounced");
    return 0;
}
