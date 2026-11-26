#include "tabletfx/effects/fractal_surround_delay.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

constexpr float kSampleRate = 48'000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kFramesPerCallback = 128U;
constexpr std::size_t kWarmupCallbacks = 3'000U;
constexpr std::size_t kMeasuredCallbacks = 20'000U;
constexpr double kCallbackDeadlineNs =
    1'000'000'000.0 * static_cast<double>(kFramesPerCallback) /
    static_cast<double>(kSampleRate);

double percentile(std::vector<double> values, double fraction) {
    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(fraction * static_cast<double>(values.size() - 1U)));
    return values[std::min(index, values.size() - 1U)];
}

double percent_of_deadline(double nanoseconds) {
    return 100.0 * nanoseconds / kCallbackDeadlineNs;
}

void run_profile(std::string_view label,
                 tabletfx::FractalSurroundDelayParameters parameters,
                 std::uint16_t channels) {
    tabletfx::FractalSurroundDelay delay(kSampleRate, parameters);
    std::array<float, kFramesPerCallback * 2U> block{};
    for (std::size_t frame = 0; frame < kFramesPerCallback; ++frame) {
        const float time = static_cast<float>(frame) / kSampleRate;
        const float left = 0.2F * std::sin(
            2.0F * 3.14159265358979323846F * 110.0F * time);
        const float right = 0.16F * std::sin(
            2.0F * 3.14159265358979323846F * 165.0F * time);
        block[frame * 2U] = left;
        block[frame * 2U + 1U] = right;
    }

    const std::size_t sample_count = kFramesPerCallback * channels;
    for (std::size_t callback = 0; callback < kWarmupCallbacks; ++callback) {
        delay.process(std::span<float>(block.data(), sample_count), channels);
    }

    std::vector<double> timings;
    timings.reserve(kMeasuredCallbacks);
    for (std::size_t callback = 0; callback < kMeasuredCallbacks; ++callback) {
        const auto start = std::chrono::steady_clock::now();
        delay.process(std::span<float>(block.data(), sample_count), channels);
        const auto end = std::chrono::steady_clock::now();
        timings.push_back(std::chrono::duration<double, std::nano>(end - start)
                              .count());
    }

    std::cout << label << " channels=" << channels
              << " memory_bytes=" << delay.memory_bytes()
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
              << "Project-authored Fractal FM9-family Surround Delay benchmark; "
              << "48 kHz, 128-frame callbacks (2.6667 ms deadline), "
              << kWarmupCallbacks << " warm-up and " << kMeasuredCallbacks
              << " measured callbacks. Percentages are isolated processor-call "
              << "cost, not total-chain or device latency.\n";

    run_profile("default-stereo", {}, 2U);

    tabletfx::FractalSurroundDelayParameters rotating{};
    rotating.time_ms = 25.0F;
    rotating.feedback_left_percent = 80.0F;
    rotating.feedback_right_percent = 80.0F;
    rotating.rotation_percent = 100.0F;
    rotating.mix_percent = 50.0F;
    run_profile("short-rotating-high-feedback-stereo", rotating, 2U);

    tabletfx::FractalSurroundDelayParameters maximum_time{};
    maximum_time.time_ms = 500.0F;
    maximum_time.master_time_percent = 400.0F;
    maximum_time.right_time_ratio_percent = 200.0F;
    maximum_time.rotation_percent = -200.0F;
    run_profile("maximum-time-ratio-stereo", maximum_time, 2U);

    run_profile("default-mono", {}, 1U);
    return 0;
}
