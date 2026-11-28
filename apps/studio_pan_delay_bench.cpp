#include "tabletfx/effects/studio_pan_delay.hpp"

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
constexpr std::size_t kWarmupCallbacks = 2000U;
constexpr std::size_t kMeasuredCallbacks = 20000U;
constexpr double kCallbackNanoseconds =
    static_cast<double>(kFramesPerCallback) * 1.0e9 / kSampleRate;

const char* mode_name(tabletfx::StudioDelayPanMode mode) noexcept {
    switch (mode) {
    case tabletfx::StudioDelayPanMode::Sine: return "Sine";
    case tabletfx::StudioDelayPanMode::Triangle: return "Triangle";
    case tabletfx::StudioDelayPanMode::Envelope: return "Envelope";
    }
    return "Unknown";
}

double percentile(const std::vector<double>& sorted, double percentile_value) {
    const double scaled = percentile_value *
                          static_cast<double>(sorted.size() - 1U);
    const std::size_t index = static_cast<std::size_t>(std::ceil(scaled));
    return sorted[std::min(index, sorted.size() - 1U)];
}

void fill_input(std::array<float, kFramesPerCallback * kChannels>& audio,
                std::size_t callback_index) noexcept {
    const float offset = static_cast<float>(callback_index * kFramesPerCallback);
    for (std::size_t frame = 0U; frame < kFramesPerCallback; ++frame) {
        const float phase = (offset + static_cast<float>(frame)) * 0.013F;
        audio[frame * 2U] = 0.45F * std::sin(phase);
        audio[frame * 2U + 1U] = 0.35F * std::sin(phase + 0.4F);
    }
}

void benchmark_mode(tabletfx::StudioDelayPanMode mode) {
    tabletfx::StudioPanDelayParameters parameters;
    parameters.delay_time_ms = 350.0F;
    parameters.feedback = 0.35F;
    parameters.mix = 0.3F;
    parameters.pan_rate_hz = 1.7F;
    parameters.pan_depth = 1.0F;
    parameters.pan_mode = mode;
    tabletfx::StudioPanDelay effect(kSampleRate, parameters);
    std::array<float, kFramesPerCallback * kChannels> audio{};
    std::vector<double> elapsed_nanoseconds;
    elapsed_nanoseconds.reserve(kMeasuredCallbacks);

    for (std::size_t callback = 0U; callback < kWarmupCallbacks; ++callback) {
        fill_input(audio, callback);
        effect.process(audio, kChannels);
    }
    for (std::size_t callback = 0U; callback < kMeasuredCallbacks; ++callback) {
        fill_input(audio, callback + kWarmupCallbacks);
        const auto start = Clock::now();
        effect.process(audio, kChannels);
        const auto stop = Clock::now();
        elapsed_nanoseconds.push_back(
            std::chrono::duration<double, std::nano>(stop - start).count());
    }

    std::sort(elapsed_nanoseconds.begin(), elapsed_nanoseconds.end());
    const std::size_t deadline_misses = static_cast<std::size_t>(std::count_if(
        elapsed_nanoseconds.begin(), elapsed_nanoseconds.end(),
        [](double nanoseconds) { return nanoseconds >= kCallbackNanoseconds; }));
    const auto as_callback_percent = [](double nanoseconds) {
        return 100.0 * nanoseconds / kCallbackNanoseconds;
    };
    std::cout << mode_name(mode) << ": object+fixed-delay-state="
              << effect.memory_bytes()
              << " B, fixed-latency=" << effect.fixed_latency_frames()
              << " frames, delay=" << effect.parameters().delay_time_ms
              << " ms, feedback=" << effect.parameters().feedback
              << ", Mix=" << effect.parameters().mix
              << "; deadline-misses=" << deadline_misses << "/"
              << kMeasuredCallbacks
              << ", callback-load p50/p95/p99/p99.9/max="
              << as_callback_percent(percentile(elapsed_nanoseconds, 0.50))
              << "/"
              << as_callback_percent(percentile(elapsed_nanoseconds, 0.95))
              << "/"
              << as_callback_percent(percentile(elapsed_nanoseconds, 0.99))
              << "/"
              << as_callback_percent(percentile(elapsed_nanoseconds, 0.999))
              << "/"
              << as_callback_percent(elapsed_nanoseconds.back()) << "%\n";
}

} // namespace

int main() {
    std::cout << std::fixed << std::setprecision(4)
              << "Project-authored Studio Delay processor-call benchmark: host "
                 "timing is not device DSP or Fender sound parity; 48 kHz, stereo, "
              << kFramesPerCallback << " frames, "
              << (kCallbackNanoseconds / 1.0e6) << " ms deadline, "
              << kWarmupCallbacks << " warmup, " << kMeasuredCallbacks
              << " measured callbacks\n";
    benchmark_mode(tabletfx::StudioDelayPanMode::Sine);
    benchmark_mode(tabletfx::StudioDelayPanMode::Triangle);
    benchmark_mode(tabletfx::StudioDelayPanMode::Envelope);
}
