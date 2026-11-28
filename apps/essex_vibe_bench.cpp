#include "tabletfx/effects/essex_vibe.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>

int main() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frame_count = 128U;
    constexpr std::size_t warmup_callbacks = 3000U;
    constexpr std::size_t measured_callbacks = 20000U;
    constexpr double callback_deadline_us = 1000000.0 *
        static_cast<double>(frame_count) / static_cast<double>(sample_rate);
    constexpr float pi = 3.14159265358979323846F;

    tabletfx::EssexVibeParameters parameters;
    parameters.rate_hz = 0.8F;
    parameters.depth = 0.65F;
    parameters.trem = 0.0F;
    parameters.stages = 2U;
    parameters.spread = 0.0F;
    tabletfx::EssexVibe effect(sample_rate, parameters);

    std::array<float, frame_count * 2U> samples{};
    for (std::size_t frame = 0U; frame < frame_count; ++frame) {
        const float time = static_cast<float>(frame) / sample_rate;
        const float value = 0.25F * std::sin(2.0F * pi * 220.0F * time);
        samples[frame * 2U] = value;
        samples[frame * 2U + 1U] = value;
    }

    for (std::size_t iteration = 0U; iteration < warmup_callbacks; ++iteration) {
        effect.process(samples, 2U);
    }

    std::array<double, measured_callbacks> callback_us{};
    for (std::size_t iteration = 0U; iteration < measured_callbacks; ++iteration) {
        const auto start = std::chrono::steady_clock::now();
        effect.process(samples, 2U);
        const auto finish = std::chrono::steady_clock::now();
        callback_us[iteration] =
            std::chrono::duration<double, std::micro>(finish - start).count();
    }
    std::sort(callback_us.begin(), callback_us.end());
    const auto percentile = [&callback_us](double fraction) {
        const auto index = static_cast<std::size_t>(
            fraction * static_cast<double>(callback_us.size() - 1U));
        return callback_us[index];
    };
    double checksum = 0.0;
    for (const float sample : samples) {
        checksum += static_cast<double>(sample);
    }

    const auto print_percent = [callback_deadline_us](const char* name,
                                                       double time_us) {
        std::cout << ' ' << name << "_us=" << time_us
                  << ' ' << name << "_percent="
                  << 100.0 * time_us / callback_deadline_us;
    };
    std::cout << "profile=two-stage-default sample_rate_hz=" << sample_rate
              << " block_frames=" << frame_count
              << " channels=2 deadline_us=" << callback_deadline_us
              << " warmup_callbacks=" << warmup_callbacks
              << " measured_callbacks=" << measured_callbacks
              << " processor_memory_bytes=" << effect.memory_bytes()
              << " algorithmic_latency_samples=0";
    print_percent("p50", percentile(0.50));
    print_percent("p95", percentile(0.95));
    print_percent("p99", percentile(0.99));
    print_percent("p99_9", percentile(0.999));
    print_percent("max", callback_us.back());
    std::cout << " finite_output=";
    bool finite = true;
    for (const float sample : samples) {
        finite = finite && std::isfinite(sample);
    }
    std::cout << (finite ? "true" : "false")
              << " checksum=" << checksum << '\n';
    return 0;
}
