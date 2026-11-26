#include "tabletfx/effects/compressor.hpp"

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

constexpr std::uint32_t kSampleRate = 48000;
constexpr std::size_t kFrames = 128;
constexpr std::uint16_t kChannels = 2;
constexpr std::size_t kSamples = kFrames * kChannels;
constexpr std::size_t kWarmupCallbacks = 3000;
constexpr std::size_t kMeasuredCallbacks = 20000;
constexpr double kDeadlineNanoseconds =
    1.0e9 * static_cast<double>(kFrames) / static_cast<double>(kSampleRate);

struct Profile {
    const char* name;
    bool external_detector;
};

void run_profile(Profile profile, const std::array<float, kSamples>& input,
                 const std::array<float, kSamples>& detector) {
    tabletfx::Compressor compressor(static_cast<float>(kSampleRate));
    std::array<float, kSamples> audio{};
    std::vector<double> durations;
    durations.reserve(kMeasuredCallbacks);
    double checksum = 0.0;

    const auto process = [&] {
        audio = input;
        if (profile.external_detector) {
            compressor.process_with_detector(audio, detector, kChannels);
        } else {
            compressor.process(audio, kChannels);
        }
    };

    for (std::size_t callback = 0; callback < kWarmupCallbacks; ++callback) {
        process();
    }
    for (std::size_t callback = 0; callback < kMeasuredCallbacks; ++callback) {
        audio = input;
        const auto start = std::chrono::steady_clock::now();
        if (profile.external_detector) {
            compressor.process_with_detector(audio, detector, kChannels);
        } else {
            compressor.process(audio, kChannels);
        }
        const auto stop = std::chrono::steady_clock::now();
        durations.push_back(std::chrono::duration<double, std::nano>(stop - start).count());
        checksum += static_cast<double>(audio[callback % kSamples]);
    }

    std::sort(durations.begin(), durations.end());
    const auto percentile_ns = [&durations](double percentile) {
        const std::size_t index = static_cast<std::size_t>(
            percentile * static_cast<double>(durations.size() - 1));
        return durations[index];
    };
    const auto percent_of_deadline = [](double nanoseconds) {
        return nanoseconds / kDeadlineNanoseconds * 100.0;
    };

    std::cout << profile.name
              << " p50/p95/p99/p99.9/max="
              << percent_of_deadline(percentile_ns(0.50)) << "/"
              << percent_of_deadline(percentile_ns(0.95)) << "/"
              << percent_of_deadline(percentile_ns(0.99)) << "/"
              << percent_of_deadline(percentile_ns(0.999)) << "/"
              << percent_of_deadline(durations.back())
              << "% of 2.6667 ms callback"
              << "; compressor_state=" << compressor.memory_bytes() << " B"
              << "; checksum=" << checksum << '\n';
}

} // namespace

int main() {
    std::array<float, kSamples> input{};
    std::array<float, kSamples> detector{};
    for (std::size_t frame = 0; frame < kFrames; ++frame) {
        const float sample = 0.35F * std::sin(static_cast<float>(frame) * 0.071F);
        input[frame * kChannels] = sample;
        input[frame * kChannels + 1] = sample * 0.8F;
        detector[frame * kChannels] = sample * 1.4F;
        detector[frame * kChannels + 1] = sample * 1.2F;
    }

    std::cout << std::fixed << std::setprecision(4)
              << "Stadium Dynamics sidechain project mode benchmark; "
              << "48 kHz stereo, 128 frames, " << kWarmupCallbacks << " warm-up and "
              << kMeasuredCallbacks << " measured callbacks.\n";
    run_profile({"Block input (default)", false}, input, detector);
    run_profile({"Pre-effect dry-chain detector", true}, input, detector);
    return 0;
}
