#include "tabletfx/effects/phaser.hpp"

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
constexpr std::size_t kFramesPerCallback = 128U;
constexpr std::size_t kWarmupCallbacks = 3000U;
constexpr std::size_t kMeasuredCallbacks = 20000U;
constexpr double kCallbackNanoseconds =
    static_cast<double>(kFramesPerCallback) * 1.0e9 / kSampleRate;

double percentile(const std::vector<double>& sorted, double value) {
    const auto index = static_cast<std::size_t>(
        std::ceil(value * static_cast<double>(sorted.size() - 1U)));
    return sorted[std::min(index, sorted.size() - 1U)];
}

void fill_input(std::array<float, kFramesPerCallback * kChannels>& audio,
                std::size_t callback) noexcept {
    const float offset = static_cast<float>(callback * kFramesPerCallback);
    for (std::size_t frame = 0U; frame < kFramesPerCallback; ++frame) {
        const float phase = (offset + static_cast<float>(frame)) * 0.013F;
        audio[frame * 2U] = 0.45F * std::sin(phase);
        audio[frame * 2U + 1U] = 0.35F * std::sin(phase + 0.4F);
    }
}

void benchmark(std::string_view label, bool enabled, float offset_points) {
    tabletfx::PhaserParameters parameters;
    parameters.global_mix_enabled = enabled;
    parameters.global_mix_offset = offset_points;
    tabletfx::Phaser effect(kSampleRate, kChannels, parameters);
    std::array<float, kFramesPerCallback * kChannels> audio{};
    std::vector<double> elapsed_nanoseconds;
    elapsed_nanoseconds.reserve(kMeasuredCallbacks);

    for (std::size_t callback = 0U; callback < kWarmupCallbacks; ++callback) {
        fill_input(audio, callback);
        effect.process(audio, kChannels);
    }
    std::size_t deadline_misses = 0U;
    for (std::size_t callback = 0U; callback < kMeasuredCallbacks; ++callback) {
        fill_input(audio, callback + kWarmupCallbacks);
        const auto start = Clock::now();
        effect.process(audio, kChannels);
        const auto stop = Clock::now();
        const double nanoseconds =
            std::chrono::duration<double, std::nano>(stop - start).count();
        elapsed_nanoseconds.push_back(nanoseconds);
        deadline_misses += nanoseconds > kCallbackNanoseconds ? 1U : 0U;
    }

    std::sort(elapsed_nanoseconds.begin(), elapsed_nanoseconds.end());
    const auto percent = [](double nanoseconds) {
        return 100.0 * nanoseconds / kCallbackNanoseconds;
    };
    std::cout << label << ": state=" << effect.memory_bytes()
              << " B, callback-load p50/p95/p99/p99.9/max="
              << percent(percentile(elapsed_nanoseconds, 0.50)) << "/"
              << percent(percentile(elapsed_nanoseconds, 0.95)) << "/"
              << percent(percentile(elapsed_nanoseconds, 0.99)) << "/"
              << percent(percentile(elapsed_nanoseconds, 0.999)) << "/"
              << percent(elapsed_nanoseconds.back()) << "%; deadline misses="
              << deadline_misses << "/" << kMeasuredCallbacks << '\n';
}

} // namespace

int main() {
    std::cout << std::fixed << std::setprecision(4)
              << "Project Phaser Global Mix benchmark: 48 kHz, stereo, "
              << kFramesPerCallback << " frames, "
              << (kCallbackNanoseconds / 1.0e6)
              << " ms callback deadline; values are isolated host processor-call "
                 "time, not Fractal DSP or target-device guarantees\n";
    benchmark("Default, Global Mix off", false, 0.0F);
    benchmark("+20 pp stored, opt-in off", false, 20.0F);
    benchmark("+20 pp enabled", true, 20.0F);
    benchmark("-20 pp enabled", true, -20.0F);
}
