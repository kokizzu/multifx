#include "tabletfx/effects/pitch_ring_mod.hpp"

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

using Clock = std::chrono::steady_clock;
constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kFramesPerCallback = 128U;
constexpr std::size_t kWarmupCallbacks = 2000U;
constexpr std::size_t kMeasuredCallbacks = 20000U;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeadlineNanoseconds =
    static_cast<double>(kFramesPerCallback) * 1.0e9 / kSampleRate;

double percentile(const std::vector<double>& sorted, double fraction) {
    const double position = fraction *
                            static_cast<double>(sorted.size() - 1U);
    const auto index = static_cast<std::size_t>(std::ceil(position));
    return sorted[std::min(index, sorted.size() - 1U)];
}

void fill_block(std::array<float, kFramesPerCallback * kChannels>& block,
                double frequency_hz, std::size_t first_frame,
                float amplitude = 0.4F) noexcept {
    for (std::size_t frame = 0U; frame < kFramesPerCallback; ++frame) {
        const double absolute_frame =
            static_cast<double>(first_frame + frame);
        const float sample = static_cast<float>(
            static_cast<double>(amplitude) *
            std::sin(2.0 * kPi * frequency_hz * absolute_frame /
                     static_cast<double>(kSampleRate)));
        block[frame * 2U] = sample;
        block[frame * 2U + 1U] = sample * 0.7F;
    }
}

void process_block(tabletfx::effects::PitchRingMod& effect,
                   std::array<float, kFramesPerCallback * kChannels>& block,
                   double frequency_hz, std::size_t& frame_cursor,
                   float amplitude = 0.4F) noexcept {
    fill_block(block, frequency_hz, frame_cursor, amplitude);
    effect.process(block, kChannels);
    frame_cursor += kFramesPerCallback;
}

double measure_tracking_warmup(tabletfx::effects::PitchRingMod& effect,
                               std::array<float, kFramesPerCallback *
                                                kChannels>& block) {
    effect.reset();
    std::size_t frame_cursor = 0U;
    constexpr std::size_t kMaximumFrames =
        static_cast<std::size_t>(kSampleRate * 2.0F);
    while (frame_cursor < kMaximumFrames && !effect.has_tracked_pitch()) {
        process_block(effect, block, 220.0, frame_cursor);
    }
    if (!effect.has_tracked_pitch()) {
        return -1.0;
    }
    return 1000.0 * static_cast<double>(frame_cursor) / kSampleRate;
}

double measure_pitch_step_response(
    tabletfx::effects::PitchRingMod& effect,
    std::array<float, kFramesPerCallback * kChannels>& block) {
    effect.reset();
    std::size_t frame_cursor = 0U;
    constexpr std::size_t kMaximumFrames =
        static_cast<std::size_t>(kSampleRate * 2.0F);
    while (frame_cursor < kMaximumFrames && !effect.has_tracked_pitch()) {
        process_block(effect, block, 220.0, frame_cursor);
    }
    if (!effect.has_tracked_pitch()) {
        return -1.0;
    }

    const std::size_t step_start = frame_cursor;
    while (frame_cursor - step_start < kMaximumFrames) {
        process_block(effect, block, 440.0, frame_cursor);
        if (effect.has_tracked_pitch() &&
            std::abs(effect.tracked_pitch_hz() - 440.0F) <= 5.0F) {
            return 1000.0 * static_cast<double>(frame_cursor - step_start) /
                   kSampleRate;
        }
    }
    return -1.0;
}

double measure_fallback_response(
    tabletfx::effects::PitchRingMod& effect,
    std::array<float, kFramesPerCallback * kChannels>& block) {
    effect.reset();
    std::size_t frame_cursor = 0U;
    constexpr std::size_t kMaximumFrames =
        static_cast<std::size_t>(kSampleRate * 2.0F);
    while (frame_cursor < kMaximumFrames && !effect.has_tracked_pitch()) {
        process_block(effect, block, 220.0, frame_cursor);
    }
    if (!effect.has_tracked_pitch()) {
        return -1.0;
    }

    const std::size_t silence_start = frame_cursor;
    while (frame_cursor - silence_start < kMaximumFrames &&
           effect.has_tracked_pitch()) {
        fill_block(block, 0.0, frame_cursor, 0.0F);
        effect.process(block, kChannels);
        frame_cursor += kFramesPerCallback;
    }
    if (effect.has_tracked_pitch()) {
        return -1.0;
    }
    return 1000.0 * static_cast<double>(frame_cursor - silence_start) /
           kSampleRate;
}

}  // namespace

int main() {
    using tabletfx::effects::PitchRingMod;
    PitchRingMod effect(kSampleRate, kChannels);
    std::array<float, kFramesPerCallback * kChannels> block{};
    const double warmup_ms = measure_tracking_warmup(effect, block);
    const double step_response_ms = measure_pitch_step_response(effect, block);
    const double fallback_response_ms = measure_fallback_response(effect, block);

    // Prime the tracker before timing. The input generation, clock reads, and
    // timing-vector allocation stay outside the audio processor call.
    effect.reset();
    std::size_t frame_cursor = 0U;
    while (frame_cursor < static_cast<std::size_t>(kSampleRate * 0.5F) &&
           !effect.has_tracked_pitch()) {
        process_block(effect, block, 220.0, frame_cursor);
    }
    for (std::size_t callback = 0U; callback < kWarmupCallbacks; ++callback) {
        process_block(effect, block, 220.0, frame_cursor);
    }

    std::vector<double> elapsed_nanoseconds;
    elapsed_nanoseconds.reserve(kMeasuredCallbacks);
    std::size_t missed_deadlines = 0U;
    for (std::size_t callback = 0U; callback < kMeasuredCallbacks; ++callback) {
        fill_block(block, 220.0, frame_cursor);
        const auto start = Clock::now();
        effect.process(block, kChannels);
        const auto stop = Clock::now();
        frame_cursor += kFramesPerCallback;
        const double elapsed =
            std::chrono::duration<double, std::nano>(stop - start).count();
        elapsed_nanoseconds.push_back(elapsed);
        if (elapsed > kDeadlineNanoseconds) {
            ++missed_deadlines;
        }
    }
    std::sort(elapsed_nanoseconds.begin(), elapsed_nanoseconds.end());

    const auto as_deadline_percent = [](double nanoseconds) {
        return 100.0 * nanoseconds / kDeadlineNanoseconds;
    };
    std::cout << std::fixed << std::setprecision(4)
              << "Project-authored monophonic pitch ring-mod benchmark; "
                 "host timing is not target-device DSP or Line 6 parity.\n"
              << "48 kHz stereo, 128 frames/callback, deadline="
              << kDeadlineNanoseconds / 1.0e6 << " ms; measured callbacks="
              << kMeasuredCallbacks << "; state=" << effect.memory_bytes()
              << " B\n"
              << "callback load p50/p95/p99/p99.9/max="
              << as_deadline_percent(percentile(elapsed_nanoseconds, 0.50))
              << "/"
              << as_deadline_percent(percentile(elapsed_nanoseconds, 0.95))
              << "/"
              << as_deadline_percent(percentile(elapsed_nanoseconds, 0.99))
              << "/"
              << as_deadline_percent(percentile(elapsed_nanoseconds, 0.999))
              << "/"
              << as_deadline_percent(elapsed_nanoseconds.back()) << "% of deadline; "
              << "missed=" << missed_deadlines << '\n'
              << "tracking warm-up=" << warmup_ms
              << " ms; 220-to-440 Hz response to within 5 Hz="
              << step_response_ms << " ms; silence-to-fallback="
              << fallback_response_ms << " ms\n";
}
