#include "tabletfx/effects/modulated_delay.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr float kSampleRate = 48'000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr float kMaximumDelayMs = 2'000.0F;
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
    tabletfx::ModulatedDelayParameters parameters;
};

double percentile(const std::vector<double>& sorted, double fraction) {
    if (sorted.empty()) return 0.0;
    const double rank = std::ceil(fraction * static_cast<double>(sorted.size()));
    const auto index = std::clamp<std::size_t>(
        static_cast<std::size_t>(rank > 0.0 ? rank - 1.0 : 0.0),
        0U, sorted.size() - 1U);
    return sorted[index];
}

std::array<float, kBlockSamples> source_audio(std::size_t callback_index) {
    std::array<float, kBlockSamples> audio{};
    const double frame_offset =
        static_cast<double>(callback_index * kBlockFrames);
    for (std::size_t frame = 0U; frame < kBlockFrames; ++frame) {
        const double time = (frame_offset + static_cast<double>(frame)) /
                           static_cast<double>(kSampleRate);
        const double phase = 2.0 * kPi * 110.0 * time;
        audio[frame * 2U] = static_cast<float>(
            0.18 * std::sin(phase) + 0.035 * std::sin(2.0 * phase));
        audio[frame * 2U + 1U] = static_cast<float>(
            0.16 * std::sin(phase * 1.013) + 0.025 * std::sin(2.0 * phase));
    }
    return audio;
}

std::string host_cpu_name() {
    std::ifstream cpu_info("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpu_info, line)) {
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string_view text(line);
        if (!text.starts_with("model name") && !text.starts_with("Hardware") &&
            !text.starts_with("Model")) {
            continue;
        }
        std::string name = line.substr(colon + 1U);
        const std::size_t first = name.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        name.erase(0U, first);
        std::replace_if(name.begin(), name.end(),
                        [](unsigned char character) {
                            return std::isspace(character) != 0;
                        },
                        '_');
        return name;
    }
    return "unavailable";
}

bool run_profile(const Profile& profile,
                 const std::array<float, kBlockSamples>& source,
                 std::string_view host_cpu,
                 std::size_t ring_storage_bytes) {
    tabletfx::ModulatedDelay effect(kSampleRate, kChannels, kMaximumDelayMs,
                                    profile.parameters);
    std::array<float, kBlockSamples> block{};
    std::vector<double> durations_us;
    durations_us.reserve(kMeasuredCallbacks);
    std::size_t allocation_count = 0U;
    double checksum = 0.0;
    bool finite_output = true;

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
              << " host_cpu=" << host_cpu
              << " sample_rate_hz=" << static_cast<unsigned>(kSampleRate)
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " deadline_us=" << std::fixed << std::setprecision(3)
              << kCallbackDeadlineUs
              << " processor_memory_bytes=" << effect.memory_bytes()
              << " ring_storage_estimate_bytes=" << ring_storage_bytes
              << " memory_scope=sizeof_object_plus_vector_capacity_excludes_allocator_overhead"
              << " process_operator_new_allocations=" << allocation_count
              << " p50_us=" << p50_us
              << " p50_deadline_percent=" << deadline_percent(p50_us)
              << " p95_us=" << p95_us
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
    const std::string cpu = host_cpu_name();
    const std::size_t ring_frames = static_cast<std::size_t>(std::ceil(
        static_cast<double>(kSampleRate) * kMaximumDelayMs / 1000.0)) + 3U;
    const std::size_t ring_bytes_estimate =
        ring_frames * kChannels * sizeof(float);
    std::cout << "Project-authored Modulated Delay benchmark; host CPU timing is not "
                 "device DSP or Fender parity; warmup="
              << kWarmupCallbacks << " callbacks; measured="
              << kMeasuredCallbacks
              << " callbacks per profile; memory_bytes uses the processor's "
                 "sizeof(object)+vector-capacity accounting and excludes allocator metadata.\n";

    const std::array<float, kBlockSamples> source = source_audio(0U);
    Profile default_chorus{"default-chorus", {}};

    tabletfx::ModulatedDelayParameters gritty_chorus;
    gritty_chorus.time_ms = 420.0F;
    gritty_chorus.feedback = 0.35F;
    gritty_chorus.mix = 0.3F;
    gritty_chorus.depth_ms = 3.0F;
    gritty_chorus.input_gain_db = 6.0F;
    gritty_chorus.grit_percent = 50.0F;
    Profile gain_grit_chorus{"gain-grit-chorus", gritty_chorus};

    tabletfx::ModulatedDelayParameters vibrato = gritty_chorus;
    vibrato.mode = tabletfx::ModulatedDelayMode::vibrato;
    vibrato.grit_percent = 100.0F;
    Profile gain_grit_vibrato{"gain-grit-vibrato", vibrato};

    bool okay = true;
    okay = run_profile(default_chorus, source, cpu, ring_bytes_estimate) && okay;
    okay = run_profile(gain_grit_chorus, source, cpu, ring_bytes_estimate) && okay;
    okay = run_profile(gain_grit_vibrato, source, cpu, ring_bytes_estimate) && okay;
    return okay ? EXIT_SUCCESS : EXIT_FAILURE;
}
