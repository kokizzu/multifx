#include "tabletfx/effects/feedback_generator.hpp"

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

using tabletfx::effects::FeedbackGenerator;
using tabletfx::effects::FeedbackGeneratorParameters;
using tabletfx::effects::FeedbackGeneratorType;

constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kFramesPerCallback = 128U;
constexpr std::size_t kWarmupCallbacks = 1000U;
constexpr std::size_t kMeasuredCallbacks = 3000U;
constexpr double kPi = 3.14159265358979323846;
constexpr double kCallbackDeadlineNs =
    static_cast<double>(kFramesPerCallback) / kSampleRate * 1.0e9;

struct Profile {
    const char* name;
    FeedbackGeneratorType type;
    double input_hz;
};

double percentile(const std::vector<double>& sorted, double fraction) {
    if (sorted.empty()) return 0.0;
    const std::size_t index = std::min(
        sorted.size() - 1U,
        static_cast<std::size_t>(std::ceil(fraction * sorted.size()) - 1.0));
    return sorted[index];
}

void run_profile(const Profile& profile) {
    FeedbackGeneratorParameters parameters;
    parameters.type = profile.type;
    parameters.feedback_gain = 0.5F;
    parameters.attack_ms = 15.0F;
    parameters.release_ms = 600.0F;
    parameters.dry_level = 0.0F;
    parameters.silence_threshold_db = -72.0F;
    FeedbackGenerator effect(kSampleRate, kChannels, parameters);

    std::vector<double> durations_ns;
    durations_ns.reserve(kMeasuredCallbacks);
    std::array<float, kFramesPerCallback * kChannels> block{};
    double phase = 0.0;
    const double phase_increment = 2.0 * kPi * profile.input_hz / kSampleRate;
    const auto render_callback = [&] {
        for (std::size_t frame = 0U; frame < kFramesPerCallback; ++frame) {
            const float sample = static_cast<float>(
                0.18 * std::sin(phase) + 0.035 * std::sin(2.0 * phase));
            phase += phase_increment;
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
            block[frame * kChannels] = sample;
            block[frame * kChannels + 1U] = sample;
        }
        const auto begin = std::chrono::steady_clock::now();
        effect.process(block, kChannels);
        const auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::nano>(end - begin).count();
    };

    for (std::size_t index = 0U; index < kWarmupCallbacks; ++index) {
        (void)render_callback();
    }
    for (std::size_t index = 0U; index < kMeasuredCallbacks; ++index) {
        durations_ns.push_back(render_callback());
    }
    std::sort(durations_ns.begin(), durations_ns.end());

    const auto percent = [](double duration_ns) {
        return 100.0 * duration_ns / kCallbackDeadlineNs;
    };
    const std::size_t deadline_misses = static_cast<std::size_t>(
        std::count_if(durations_ns.begin(), durations_ns.end(),
                      [](double duration_ns) {
                          return duration_ns > kCallbackDeadlineNs;
                      }));

    std::cout << profile.name
              << ": input_hz=" << profile.input_hz
              << " rate=" << static_cast<unsigned>(kSampleRate)
              << "Hz layout=stereo callback_frames=" << kFramesPerCallback
              << " deadline_ms=" << std::fixed << std::setprecision(3)
              << kCallbackDeadlineNs / 1.0e6
              << " memory_bytes=" << effect.memory_bytes()
              << " tracker_response_ms=" << effect.pitch_response_latency_ms()
              << " p50=" << percent(percentile(durations_ns, 0.50))
              << "% p95=" << percent(percentile(durations_ns, 0.95))
              << "% p99=" << percent(percentile(durations_ns, 0.99))
              << "% p99.9=" << percent(percentile(durations_ns, 0.999))
              << "% max=" << percent(durations_ns.back())
              << "% deadline_misses=" << deadline_misses << '/'
              << kMeasuredCallbacks << '\n';
}

}  // namespace

int main() {
    std::cout << "Feedback Generator project benchmark; desktop timing only; "
              << "warmup=" << kWarmupCallbacks
              << " measured=" << kMeasuredCallbacks << " callbacks\n";
    for (const Profile profile : {
             Profile{"octave-down-mid", FeedbackGeneratorType::OctaveDown, 220.0},
             Profile{"unison-mid", FeedbackGeneratorType::Unison, 220.0},
             Profile{"octave-up-mid", FeedbackGeneratorType::OctaveUp, 220.0},
             Profile{"octave-fifth-mid", FeedbackGeneratorType::OctaveAndFifth, 220.0},
             Profile{"two-octaves-mid", FeedbackGeneratorType::TwoOctaves, 220.0},
             Profile{"two-octaves-third-mid", FeedbackGeneratorType::TwoOctavesAndThird, 220.0},
             Profile{"two-octaves-fifth-mid", FeedbackGeneratorType::TwoOctavesAndFifth, 220.0},
             Profile{"two-octaves-seventh-mid", FeedbackGeneratorType::TwoOctavesAndSeventh, 220.0},
             Profile{"octave-down-low-E", FeedbackGeneratorType::OctaveDown, 82.4069},
             Profile{"two-octaves-seventh-low-E", FeedbackGeneratorType::TwoOctavesAndSeventh, 82.4069},
         }) {
        run_profile(profile);
    }
    return 0;
}
