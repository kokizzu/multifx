#include "tabletfx/effects/shuffling_looper.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

using tabletfx::effects::ShufflingLooper;
using tabletfx::effects::ShufflingLooperCapacityStatus;
using tabletfx::effects::ShufflingLooperParameters;

constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kMaximumChannels = 2U;
constexpr std::size_t kFramesPerCallback = 128U;
constexpr std::size_t kWarmupCallbacks = 1000U;
constexpr std::size_t kMeasuredCallbacks = 5000U;
constexpr double kPi = 3.14159265358979323846;
constexpr double kCallbackDeadlineNs =
    static_cast<double>(kFramesPerCallback) / kSampleRate * 1.0e9;

double percentile(const std::vector<double>& sorted, const double fraction) {
    if (sorted.empty()) return 0.0;
    const std::size_t index = std::min(
        sorted.size() - 1U,
        static_cast<std::size_t>(std::ceil(fraction * sorted.size()) - 1.0));
    return sorted[index];
}

void fill_input(std::array<float, kFramesPerCallback * kMaximumChannels>& block,
                const std::uint16_t channels, double& phase) {
    const double increment = 2.0 * kPi * 110.0 / kSampleRate;
    for (std::size_t frame = 0U; frame < kFramesPerCallback; ++frame) {
        const float sample = static_cast<float>(
            0.2 * std::sin(phase) + 0.04 * std::sin(2.0 * phase));
        phase += increment;
        if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
        for (std::size_t channel = 0U; channel < channels; ++channel) {
            block[frame * channels + channel] = sample;
        }
    }
}

void run_profile(const std::uint16_t channels, const float requested_seconds,
                 const bool quantize_recording) {
    ShufflingLooper effect(kSampleRate, channels, 0x51A77U);
    const std::size_t prior_storage_bytes = effect.capacity_storage_bytes();
    const auto capacity_status =
        effect.configure_capacity_seconds(requested_seconds);
    const std::size_t storage_bytes = effect.capacity_storage_bytes();
    const std::size_t transition_peak_bytes = prior_storage_bytes + storage_bytes;
    if (transition_peak_bytes > ShufflingLooper::kMaximumTransitionSampleStorageBytes) {
        throw std::runtime_error("looper exceeded its sample-storage transition bound");
    }
    if (effect.capacity_seconds() + 1.0e-4F < requested_seconds) {
        throw std::runtime_error("requested maximum capacity was not accepted");
    }

    ShufflingLooperParameters parameters = effect.parameters();
    parameters.slices = 16U;
    parameters.sequence_length = 32U;
    parameters.shuffle = 0.5F;
    parameters.pitch_probability = 0.5F;
    parameters.reverse_probability = 0.5F;
    parameters.repeat_probability = 0.25F;
    parameters.smoothing = 0.25F;
    parameters.sequence_drift = 0.5F;
    parameters.playback = 1.0F;
    parameters.low_cut_hz = 80.0F;
    parameters.high_cut_hz = 10000.0F;
    parameters.quantize_recording = quantize_recording;
    parameters.tempo_bpm = 120.0F;
    effect.configure(parameters);

    std::array<float, kFramesPerCallback * kMaximumChannels> block{};
    double phase = 0.0;
    if (!effect.record()) throw std::runtime_error("could not start benchmark recording");
    const std::size_t record_callbacks =
        (effect.capacity_frames() + kFramesPerCallback - 1U) /
        kFramesPerCallback;
    for (std::size_t index = 0U; index < record_callbacks; ++index) {
        fill_input(block, channels, phase);
        effect.process(std::span<float>(block.data(),
                                        kFramesPerCallback * channels), channels);
    }
    if (effect.state() == tabletfx::effects::ShufflingLooperState::Recording &&
        !effect.stop_recording()) {
        throw std::runtime_error("could not start benchmark loop playback");
    }
    if (effect.state() != tabletfx::effects::ShufflingLooperState::Playing) {
        throw std::runtime_error("benchmark loop did not enter playback");
    }

    const auto render_callback = [&] {
        fill_input(block, channels, phase);
        const auto begin = std::chrono::steady_clock::now();
        effect.process(std::span<float>(block.data(),
                                        kFramesPerCallback * channels), channels);
        const auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::nano>(end - begin).count();
    };
    for (std::size_t index = 0U; index < kWarmupCallbacks; ++index) {
        (void)render_callback();
    }
    if (!effect.play()) throw std::runtime_error("could not reset benchmark playback");

    std::vector<double> durations_ns;
    durations_ns.reserve(kMeasuredCallbacks);
    double output_sum = 0.0;
    for (std::size_t index = 0U; index < kMeasuredCallbacks; ++index) {
        durations_ns.push_back(render_callback());
        for (std::size_t sample = 0U; sample < kFramesPerCallback * channels;
             ++sample) {
            output_sum += block[sample];
        }
    }
    std::sort(durations_ns.begin(), durations_ns.end());
    const auto percent_of_deadline = [](const double duration_ns) {
        return 100.0 * duration_ns / kCallbackDeadlineNs;
    };
    const std::size_t deadline_misses = static_cast<std::size_t>(
        std::count_if(durations_ns.begin(), durations_ns.end(),
                      [](const double duration_ns) {
                          return duration_ns > kCallbackDeadlineNs;
                      }));
    const char* label = channels == 1U ? "mono-60s" : "stereo-30s";
    std::cout << label
              << " quantize=" << (quantize_recording ? "Beat" : "Off")
              << " sample_rate_hz=" << static_cast<unsigned>(kSampleRate)
              << " channels=" << channels
              << " capacity_seconds=" << std::fixed << std::setprecision(3)
              << effect.capacity_seconds()
              << " capacity_status=" << static_cast<unsigned>(capacity_status)
              << " effect_memory_bytes=" << effect.memory_bytes()
              << " fixed_state_memory_bytes="
              << effect.memory_bytes() - storage_bytes
              << " sample_storage_bytes=" << storage_bytes
              << " transition_peak_sample_bytes=" << transition_peak_bytes
              << " transition_limit_bytes="
              << ShufflingLooper::kMaximumTransitionSampleStorageBytes
              << " callback_frames=" << kFramesPerCallback
              << " callback_deadline_ms=" << std::setprecision(3)
              << kCallbackDeadlineNs / 1.0e6
              << " p50=" << std::setprecision(4)
              << percent_of_deadline(percentile(durations_ns, 0.50)) << "%"
              << " p95=" << percent_of_deadline(percentile(durations_ns, 0.95)) << "%"
              << " p99=" << percent_of_deadline(percentile(durations_ns, 0.99)) << "%"
              << " p99.9=" << percent_of_deadline(percentile(durations_ns, 0.999)) << "%"
              << " max=" << percent_of_deadline(durations_ns.back()) << "%"
              << " deadline_misses=" << deadline_misses << '/'
              << kMeasuredCallbacks << " output_checksum=" << output_sum << '\n';
}

}  // namespace

int main() {
    try {
        std::cout << "Shuffling Looper project benchmark; desktop timing only; "
                  << "warmup=" << kWarmupCallbacks
                  << " measured=" << kMeasuredCallbacks << " callbacks\n";
        run_profile(1U, ShufflingLooper::kMaximumMonoCapacitySeconds, false);
        run_profile(1U, ShufflingLooper::kMaximumMonoCapacitySeconds, true);
        run_profile(2U, ShufflingLooper::kMaximumStereoCapacitySeconds, false);
        run_profile(2U, ShufflingLooper::kMaximumStereoCapacitySeconds, true);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Shuffling Looper benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
