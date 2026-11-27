#include "tabletfx/effects/megatap_delay.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

constexpr float kSampleRate = 48'000.0F;
constexpr std::size_t kFramesPerCallback = 128U;
constexpr std::size_t kWarmupCallbacks = 3'000U;
constexpr std::size_t kMeasuredCallbacks = 20'000U;
constexpr double kCallbackDeadlineNs =
    1'000'000'000.0 * static_cast<double>(kFramesPerCallback) /
    static_cast<double>(kSampleRate);

double percentile(std::vector<double> values, double fraction) {
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(
        std::ceil(fraction * static_cast<double>(values.size() - 1U)));
    return values[std::min(index, values.size() - 1U)];
}

double percent_of_deadline(double nanoseconds) {
    return 100.0 * nanoseconds / kCallbackDeadlineNs;
}

void run_profile(std::string_view label,
                 tabletfx::MegatapDelayParameters parameters,
                 std::uint16_t channels) {
    tabletfx::MegatapDelay delay(kSampleRate, 5000.0F, parameters);
    std::array<float, kFramesPerCallback * 2U> block{};
    for (std::size_t frame = 0; frame < kFramesPerCallback; ++frame) {
        const float time = static_cast<float>(frame) / kSampleRate;
        block[frame * 2U] = 0.2F * std::sin(
            2.0F * 3.14159265358979323846F * 110.0F * time);
        block[frame * 2U + 1U] = 0.16F * std::sin(
            2.0F * 3.14159265358979323846F * 165.0F * time);
    }

    const std::size_t sample_count = kFramesPerCallback * channels;
    for (std::size_t callback = 0; callback < kWarmupCallbacks; ++callback) {
        delay.process(std::span<float>(block.data(), sample_count), channels);
    }

    std::vector<double> timings;
    timings.reserve(kMeasuredCallbacks);
    std::size_t deadline_misses = 0U;
    for (std::size_t callback = 0; callback < kMeasuredCallbacks; ++callback) {
        const auto start = std::chrono::steady_clock::now();
        delay.process(std::span<float>(block.data(), sample_count), channels);
        const auto end = std::chrono::steady_clock::now();
        const double elapsed_ns =
            std::chrono::duration<double, std::nano>(end - start).count();
        timings.push_back(elapsed_ns);
        if (elapsed_ns > kCallbackDeadlineNs) ++deadline_misses;
    }

    std::cout << label << " channels=" << channels
              << " taps=" << static_cast<unsigned>(parameters.tap_count)
              << " memory_bytes=" << delay.memory_bytes()
              << " p50=" << percent_of_deadline(percentile(timings, 0.50))
              << "% p95=" << percent_of_deadline(percentile(timings, 0.95))
              << "% p99=" << percent_of_deadline(percentile(timings, 0.99))
              << "% p99.9=" << percent_of_deadline(percentile(timings, 0.999))
              << "% max=" << percent_of_deadline(*std::max_element(
                     timings.begin(), timings.end()))
              << "% deadline_misses=" << deadline_misses << "/"
              << kMeasuredCallbacks << '\n';
}

} // namespace

int main() {
    std::cout << std::fixed << std::setprecision(4)
              << "Project-authored Megatap Delay Global Mix benchmark; 48 kHz, "
              << "128-frame callbacks (2.6667 ms deadline), "
              << kWarmupCallbacks << " warm-up and " << kMeasuredCallbacks
              << " measured callbacks. Percentages are isolated processor-call "
              << "cost, not total-chain or device latency.\n";

    run_profile("default-stereo", {}, 2U);

    tabletfx::MegatapDelayParameters offset{};
    offset.global_mix_offset = 0.2F;
    run_profile("global-mix-stored-disabled", offset, 2U);
    offset.global_mix_enabled = true;
    run_profile("global-mix-enabled-plus-20pp", offset, 2U);

    tabletfx::MegatapDelayParameters maximum_taps{};
    maximum_taps.tap_count = 64U;
    maximum_taps.feedback_tap = 64U;
    run_profile("maximum-tap-count", maximum_taps, 2U);
    run_profile("default-mono", {}, 1U);
    return 0;
}
