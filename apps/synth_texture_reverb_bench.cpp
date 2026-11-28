#include "tabletfx/effects/synth_texture_reverb.hpp"

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

using tabletfx::effects::SynthTextureReverb;
using tabletfx::effects::SynthTextureReverbParameters;

constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kFramesPerCallback = 128U;
constexpr std::size_t kWarmupCallbacks = 3000U;
constexpr std::size_t kMeasuredCallbacks = 20000U;
constexpr double kCallbackDeadlineNs =
    1.0e9 * static_cast<double>(kFramesPerCallback) / kSampleRate;

using Block = std::array<float, kFramesPerCallback * kChannels>;

struct Profile {
    const char* name;
    SynthTextureReverbParameters parameters;
};

double percentile(const std::vector<double>& sorted, double fraction) {
    if (sorted.empty()) return 0.0;
    const std::size_t index = std::min(
        sorted.size() - 1U,
        static_cast<std::size_t>(std::ceil(
            fraction * static_cast<double>(sorted.size() - 1U))));
    return sorted[index];
}

double percent_of_deadline(double nanoseconds) {
    return 100.0 * nanoseconds / kCallbackDeadlineNs;
}

void prepare_input(Block& block, std::size_t callback) {
    constexpr double kTwoPi = 6.28318530717958647692;
    for (std::size_t frame = 0U; frame < kFramesPerCallback; ++frame) {
        const double absolute_frame =
            static_cast<double>(callback) * kFramesPerCallback + frame;
        const double time = absolute_frame / kSampleRate;
        const float guitar = static_cast<float>(
            0.18 * std::sin(kTwoPi * 110.0 * time) +
            0.09 * std::sin(kTwoPi * 659.0 * time) +
            0.04 * std::sin(kTwoPi * 2637.0 * time));
        block[frame * 2U] = guitar;
        block[frame * 2U + 1U] = guitar * 0.87F;
    }
}

void run_profile(const Profile& profile) {
    SynthTextureReverb effect(kSampleRate, kChannels, profile.parameters);
    Block block{};
    for (std::size_t callback = 0U; callback < kWarmupCallbacks; ++callback) {
        prepare_input(block, callback);
        effect.process(block, kChannels);
    }

    std::vector<double> timings;
    timings.reserve(kMeasuredCallbacks);
    double checksum = 0.0;
    for (std::size_t callback = 0U; callback < kMeasuredCallbacks; ++callback) {
        prepare_input(block, callback + kWarmupCallbacks);
        const auto begin = std::chrono::steady_clock::now();
        effect.process(block, kChannels);
        const auto end = std::chrono::steady_clock::now();
        timings.push_back(
            std::chrono::duration<double, std::nano>(end - begin).count());
        checksum += static_cast<double>(block[callback % block.size()]);
    }
    std::sort(timings.begin(), timings.end());
    const std::size_t deadline_misses = static_cast<std::size_t>(
        std::count_if(timings.begin(), timings.end(), [](double duration) {
            return duration > kCallbackDeadlineNs;
        }));

    std::cout << profile.name
              << " memory_bytes=" << effect.memory_bytes()
              << " fixed_latency_samples=" << effect.fixed_latency_samples()
              << " p50=" << percent_of_deadline(percentile(timings, 0.50))
              << "% p95=" << percent_of_deadline(percentile(timings, 0.95))
              << "% p99=" << percent_of_deadline(percentile(timings, 0.99))
              << "% p99.9=" << percent_of_deadline(percentile(timings, 0.999))
              << "% max=" << percent_of_deadline(timings.back())
              << "% deadline_misses=" << deadline_misses << '/'
              << kMeasuredCallbacks << " of 2.6667 ms; checksum="
              << checksum << '\n';
}

}  // namespace

int main() {
    SynthTextureReverbParameters default_parameters;
    SynthTextureReverbParameters dense_parameters;
    dense_parameters.mix = 0.7F;
    dense_parameters.texture = 0.95F;
    dense_parameters.decay_seconds = 9.0F;

    std::cout << std::fixed << std::setprecision(4)
              << "Project synthetic-texture reverb benchmark; 48 kHz stereo, "
              << kFramesPerCallback << " frames per callback, "
              << kWarmupCallbacks << " warm-up and " << kMeasuredCallbacks
              << " measured callbacks. Callback timing is isolated desktop CPU "
              << "cost, not device/chain latency. Percentages use the 2.6667 ms "
              << "callback deadline.\n";
    run_profile({"project-default", default_parameters});
    run_profile({"dense-texture-long-decay", dense_parameters});
    return 0;
}
