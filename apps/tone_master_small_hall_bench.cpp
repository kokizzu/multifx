#include "tabletfx/effects/tone_master_small_hall.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <new>
#include <string_view>
#include <vector>

namespace {

constexpr float kSampleRate = 48'000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kBlockFrames = 128U;
constexpr std::size_t kBlockSamples = kBlockFrames * kChannels;
constexpr std::size_t kWarmupCallbacks = 3'000U;
constexpr std::size_t kMeasuredCallbacks = 20'000U;
constexpr double kCallbackDeadlineUs =
    static_cast<double>(kBlockFrames) * 1'000'000.0 / kSampleRate;
constexpr double kPi = 3.14159265358979323846;

std::atomic<bool> watch_allocations{false};
std::atomic<std::size_t> process_allocations{0U};

struct Profile {
    std::string_view name;
    tabletfx::ToneMasterSmallHallParameters parameters;
};

double percentile(const std::vector<double>& sorted, double fraction) {
    if (sorted.empty()) return 0.0;
    const double rank = std::ceil(fraction * static_cast<double>(sorted.size()));
    const auto index = std::clamp<std::size_t>(
        static_cast<std::size_t>(rank > 0.0 ? rank - 1.0 : 0.0),
        0U, sorted.size() - 1U);
    return sorted[index];
}

std::array<float, kBlockSamples> guitar_audio() {
    std::array<float, kBlockSamples> audio{};
    for (std::size_t frame = 0U; frame < kBlockFrames; ++frame) {
        const double time = static_cast<double>(frame) / kSampleRate;
        const double phase = 2.0 * kPi * 110.0 * time;
        audio[frame * 2U] = static_cast<float>(
            0.18 * std::sin(phase) + 0.035 * std::sin(2.0 * phase));
        audio[frame * 2U + 1U] = static_cast<float>(
            0.16 * std::sin(phase * 1.013) + 0.025 * std::sin(2.0 * phase));
    }
    return audio;
}

bool run_profile(const Profile& profile,
                 const std::array<float, kBlockSamples>& source) {
    tabletfx::ToneMasterSmallHallReverb effect(kSampleRate, kChannels,
                                                profile.parameters);
    std::array<float, kBlockSamples> block{};
    std::vector<double> durations_us;
    durations_us.reserve(kMeasuredCallbacks);
    std::size_t allocation_count = 0U;
    bool finite_output = true;
    double checksum = 0.0;

    for (std::size_t callback = 0U;
         callback < kWarmupCallbacks + kMeasuredCallbacks; ++callback) {
        block = source;
        process_allocations.store(0U, std::memory_order_relaxed);
        watch_allocations.store(true, std::memory_order_relaxed);
        const auto start = std::chrono::steady_clock::now();
        effect.process(block, kChannels);
        const auto finish = std::chrono::steady_clock::now();
        watch_allocations.store(false, std::memory_order_relaxed);
        const std::size_t callback_allocations =
            process_allocations.exchange(0U, std::memory_order_relaxed);

        if (callback >= kWarmupCallbacks) {
            durations_us.push_back(
                std::chrono::duration<double, std::micro>(finish - start).count());
            allocation_count += callback_allocations;
        }
        for (const float sample : block) {
            finite_output = finite_output && std::isfinite(sample);
            checksum += static_cast<double>(sample);
        }
    }

    std::sort(durations_us.begin(), durations_us.end());
    const auto deadline_percent = [](double duration_us) {
        return 100.0 * duration_us / kCallbackDeadlineUs;
    };
    const double p50_us = percentile(durations_us, 0.50);
    const double p95_us = percentile(durations_us, 0.95);
    const double p99_us = percentile(durations_us, 0.99);
    const double maximum_us = durations_us.back();
    const std::size_t deadline_misses = static_cast<std::size_t>(
        std::count_if(durations_us.begin(), durations_us.end(),
                      [](double duration_us) {
                          return duration_us > kCallbackDeadlineUs;
                      }));

    std::cout << "profile=" << profile.name
              << " sample_rate_hz=" << static_cast<unsigned>(kSampleRate)
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " deadline_us=" << std::fixed << std::setprecision(3)
              << kCallbackDeadlineUs
              << " processor_memory_bytes=" << effect.memory_bytes()
              << " process_operator_new_allocations=" << allocation_count
              << " p50_us=" << p50_us
              << " p50_deadline_percent=" << deadline_percent(p50_us)
              << " p95_us=" << percentile(durations_us, 0.95)
              << " p95_deadline_percent=" << deadline_percent(p95_us)
              << " p99_us=" << p99_us
              << " p99_deadline_percent=" << deadline_percent(p99_us)
              << " max_us=" << maximum_us
              << " max_deadline_percent=" << deadline_percent(maximum_us)
              << " deadline_misses=" << deadline_misses << '/'
              << kMeasuredCallbacks
              << " finite_output=" << (finite_output ? "true" : "false")
              << " checksum=" << checksum << '\n';
    return finite_output && allocation_count == 0U;
}

} // namespace

void* operator new(std::size_t size) {
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        if (watch_allocations.load(std::memory_order_relaxed)) {
            process_allocations.fetch_add(1U, std::memory_order_relaxed);
        }
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        if (watch_allocations.load(std::memory_order_relaxed)) {
            process_allocations.fetch_add(1U, std::memory_order_relaxed);
        }
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

int main() {
    std::cout << "Project-authored Tone Master Small Hall benchmark; "
                 "48 kHz stereo, 128-frame callbacks (2.667 ms deadline), "
              << kWarmupCallbacks << " warm-up and " << kMeasuredCallbacks
              << " measured callbacks per profile. Timing is isolated callback "
                 "cost on this host, not device DSP or Fender parity.\n";

    tabletfx::ToneMasterSmallHallParameters default_parameters;
    Profile defaults{"sane-default", default_parameters};

    auto dense_long = default_parameters;
    dense_long.level = 0.7F;
    dense_long.decay_seconds = 10.0F;
    dense_long.diffuse = 1.0F;
    dense_long.rate_hz = 6.0F;
    dense_long.depth = 1.0F;
    dense_long.spillover = true;
    Profile dense_long_hall{"maximum-decay-modulation", dense_long};

    const auto source = guitar_audio();
    bool okay = true;
    okay = run_profile(defaults, source) && okay;
    okay = run_profile(dense_long_hall, source) && okay;
    return okay ? EXIT_SUCCESS : EXIT_FAILURE;
}
