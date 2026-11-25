#include "tabletfx/effects/fractal_diatonic_harmonizer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

namespace {

using Harmonizer = tabletfx::effects::FractalDiatonicHarmonizer;
using Config = tabletfx::effects::FractalDiatonicHarmonizerConfig;

constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kFramesPerCallback = 128U;
constexpr std::size_t kWarmupCallbacks = 2000U;
constexpr std::size_t kMeasuredCallbacks = 5000U;
constexpr double kPi = 3.14159265358979323846;
constexpr double kCallbackDeadlineNs =
    static_cast<double>(kFramesPerCallback) / kSampleRate * 1.0e9;

struct Profile {
    const char* name;
    std::uint8_t voices;
    float delay_ms;
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
    Harmonizer effect(kSampleRate, kChannels);
    Config config = effect.config();
    config.voice_count = profile.voices;
    for (std::size_t voice = 0U; voice < config.voices.size(); ++voice) {
        config.voices[voice].level = voice < profile.voices
            ? 0.35F
            : 0.0F;
        config.voices[voice].harmony_steps =
            voice == 0U ? 2 : voice == 1U ? 4 : voice == 2U ? -2 : -4;
        config.voices[voice].delay_ms = voice < profile.voices
            ? profile.delay_ms
            : 0.0F;
        config.voices[voice].feedback = voice < profile.voices
            ? 0.2F
            : 0.0F;
    }
    effect.configure(config);

    std::vector<double> durations_ns;
    durations_ns.reserve(kMeasuredCallbacks);
    std::array<float, kFramesPerCallback * kChannels> block{};
    double phase = 0.0;
    const auto render_callback = [&] {
        for (std::size_t frame = 0U; frame < kFramesPerCallback; ++frame) {
            const float sample = static_cast<float>(
                0.18 * std::sin(phase) + 0.035 * std::sin(2.0 * phase));
            phase += 2.0 * kPi * profile.input_hz / kSampleRate;
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

    const double p50_ns = percentile(durations_ns, 0.50);
    const double p95_ns = percentile(durations_ns, 0.95);
    const double p99_ns = percentile(durations_ns, 0.99);
    const double p999_ns = percentile(durations_ns, 0.999);
    const double max_ns = durations_ns.back();
    const std::size_t deadline_misses = static_cast<std::size_t>(
        std::count_if(durations_ns.begin(), durations_ns.end(),
                      [](double duration_ns) {
                          return duration_ns > kCallbackDeadlineNs;
                      }));
    const auto percent = [](double duration_ns) {
        return 100.0 * duration_ns / kCallbackDeadlineNs;
    };

    std::cout << profile.name << ": voices=" << static_cast<unsigned>(profile.voices)
              << " delay_ms=" << profile.delay_ms
              << " input_hz=" << profile.input_hz
              << " rate=" << static_cast<unsigned>(kSampleRate)
              << "Hz layout=stereo callback_frames=" << kFramesPerCallback
              << " deadline_ms=" << std::fixed << std::setprecision(3)
              << (kCallbackDeadlineNs / 1.0e6)
              << " memory_bytes=" << effect.memory_bytes()
              << " pitch_control_bound_ms="
              << (1000.0 * effect.pitch_control_latency_samples() / kSampleRate)
              << " p50=" << percent(p50_ns) << "% p95=" << percent(p95_ns)
              << "% p99=" << percent(p99_ns)
              << "% p99.9=" << percent(p999_ns)
              << "% max=" << percent(max_ns)
              << "% deadline_misses=" << deadline_misses << '/'
              << kMeasuredCallbacks << '\n';
}

}  // namespace

int main() {
    std::cout << "Fractal Diatonic Harmonizer project benchmark; desktop timing only; "
              << "warmup=" << kWarmupCallbacks
              << " measured=" << kMeasuredCallbacks << " callbacks\n";
    for (const Profile profile : {
             Profile{"dual-mid-no-delay", 2U, 0.0F, 220.0},
             Profile{"dual-low-E-no-delay", 2U, 0.0F, 82.4069},
             Profile{"quad-mid-no-delay", 4U, 0.0F, 220.0},
             Profile{"quad-low-E-no-delay", 4U, 0.0F, 82.4069},
             Profile{"dual-mid-2s-delay", 2U, 2000.0F, 220.0},
             Profile{"quad-mid-2s-delay", 4U, 2000.0F, 220.0},
             Profile{"dual-low-E-2s-delay", 2U, 2000.0F, 82.4069},
             Profile{"quad-low-E-2s-delay", 4U, 2000.0F, 82.4069},
         }) {
        run_profile(profile);
    }
    return 0;
}
