#include "tabletfx/effects/plex_delay.hpp"

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

void run_profile(std::string_view label, std::uint8_t voices,
                 tabletfx::PlexPitchDirection direction) {
    tabletfx::PlexDelayParameters parameters;
    parameters.active_lines = voices;
    parameters.pitch_shift_enabled = voices != 0U;
    parameters.pitch_direction = direction;
    parameters.mix = 0.35F;
    parameters.diffusion = 0.2F;
    constexpr std::array<float, tabletfx::plex_delay_line_count> shifts{
        12.0F, 7.0F, -12.0F, -5.0F, 19.0F, 5.0F, -19.0F, 2.0F};
    constexpr std::array<float, tabletfx::plex_delay_line_count> detunes{
        0.0F, -8.0F, 12.0F, 0.0F, -4.0F, 16.0F, 8.0F, -12.0F};
    parameters.pitch_shift_semitones = shifts;
    parameters.pitch_detune_cents = detunes;
    tabletfx::PlexDelay delay(kSampleRate, 1000.0F, parameters);

    std::array<float, kFramesPerCallback * kChannels> block{};
    for (std::size_t frame = 0; frame < kFramesPerCallback; ++frame) {
        const float time = static_cast<float>(frame) / kSampleRate;
        block[frame * 2U] = 0.2F * std::sin(
            2.0F * 3.14159265358979323846F * 110.0F * time);
        block[frame * 2U + 1U] = 0.16F * std::sin(
            2.0F * 3.14159265358979323846F * 165.0F * time);
    }
    for (std::size_t callback = 0; callback < kWarmupCallbacks; ++callback) {
        delay.process(block, kChannels);
    }

    std::vector<double> timings;
    timings.reserve(kMeasuredCallbacks);
    std::size_t deadline_misses = 0U;
    for (std::size_t callback = 0; callback < kMeasuredCallbacks; ++callback) {
        const auto start = std::chrono::steady_clock::now();
        delay.process(block, kChannels);
        const auto end = std::chrono::steady_clock::now();
        const double elapsed =
            std::chrono::duration<double, std::nano>(end - start).count();
        timings.push_back(elapsed);
        if (elapsed > kCallbackDeadlineNs) ++deadline_misses;
    }

    std::cout << label << " active_voices=" << static_cast<unsigned>(voices)
              << " direction="
              << (direction == tabletfx::PlexPitchDirection::reverse
                      ? "reverse" : "forward")
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
              << "Project-authored Plex Shift granule benchmark; 48 kHz, "
              << "stereo 128-frame callbacks (2.6667 ms deadline), "
              << kWarmupCallbacks << " warm-up and " << kMeasuredCallbacks
              << " measured callbacks. Percentages are isolated processor-call "
              << "cost, not total-chain or device latency.\n";
    run_profile("matrix-no-pitch", 0U,
                tabletfx::PlexPitchDirection::forward);
    run_profile("one-voice", 1U, tabletfx::PlexPitchDirection::forward);
    run_profile("four-voice", 4U, tabletfx::PlexPitchDirection::forward);
    run_profile("eight-voice", 8U, tabletfx::PlexPitchDirection::forward);
    run_profile("eight-voice-reverse", 8U,
                tabletfx::PlexPitchDirection::reverse);
    return 0;
}
