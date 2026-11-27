#include "tabletfx/effects/resonator.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr float kSampleRate = 48000.0F;
constexpr std::uint16_t kChannels = 2U;
constexpr std::size_t kBlockFrames = 128U;
constexpr std::size_t kBlockSamples = kBlockFrames * kChannels;
constexpr std::size_t kWarmupCallbacks = 2000U;
constexpr std::size_t kMeasuredCallbacks = 20000U;
constexpr double kCallbackBudgetUs =
    static_cast<double>(kBlockFrames) * 1000000.0 / kSampleRate;

struct Percentiles {
    double p50_us = 0.0;
    double p95_us = 0.0;
    double p99_us = 0.0;
    double p999_us = 0.0;
    double max_us = 0.0;
    std::size_t deadline_misses = 0U;
};

double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    const double rank = std::ceil(p * static_cast<double>(sorted.size()));
    const std::size_t index = std::clamp<std::size_t>(
        static_cast<std::size_t>(rank > 0.0 ? rank - 1.0 : 0.0),
        0U, sorted.size() - 1U);
    return sorted[index];
}

void fill_source(std::array<float, kBlockSamples>& block) {
    for (std::size_t frame = 0; frame < kBlockFrames; ++frame) {
        const double phase = 2.0 * 3.14159265358979323846 *
                             static_cast<double>(frame) / 128.0;
        const float sample = static_cast<float>(
            0.18 * std::sin(phase) + 0.05 * std::sin(phase * 3.1));
        block[frame * 2U] = sample;
        block[frame * 2U + 1U] = sample * 0.85F;
    }
}

Percentiles run_profile(std::string_view name, tabletfx::Resonator& resonator,
                        const std::array<float, kBlockSamples>& source,
                        std::array<float, kBlockSamples>& block) {
    for (std::size_t callback = 0; callback < kWarmupCallbacks; ++callback) {
        block = source;
        resonator.process(block, kChannels);
    }

    std::vector<double> elapsed_us;
    elapsed_us.reserve(kMeasuredCallbacks);
    for (std::size_t callback = 0; callback < kMeasuredCallbacks; ++callback) {
        block = source;
        const auto start = Clock::now();
        resonator.process(block, kChannels);
        const auto end = Clock::now();
        elapsed_us.push_back(
            std::chrono::duration<double, std::micro>(end - start).count());
    }
    std::sort(elapsed_us.begin(), elapsed_us.end());

    const Percentiles result{
        percentile(elapsed_us, 0.50),
        percentile(elapsed_us, 0.95),
        percentile(elapsed_us, 0.99),
        percentile(elapsed_us, 0.999),
        elapsed_us.back(),
        static_cast<std::size_t>(std::count_if(
            elapsed_us.begin(), elapsed_us.end(),
            [](double duration_us) { return duration_us >= kCallbackBudgetUs; })),
    };
    const auto percent = [](double duration_us) {
        return duration_us / kCallbackBudgetUs * 100.0;
    };
    std::cout << "profile=" << name
              << " callbacks=" << kMeasuredCallbacks
              << " block_frames=" << kBlockFrames
              << " channels=" << kChannels
              << " sample_rate_hz=" << static_cast<int>(kSampleRate)
              << " callback_budget_us=" << std::fixed << std::setprecision(3)
              << kCallbackBudgetUs
              << " memory_bytes=" << resonator.memory_bytes()
              << " algorithmic_latency_samples=0"
              << " p50_us=" << result.p50_us
              << " p50_percent=" << percent(result.p50_us)
              << " p95_us=" << result.p95_us
              << " p95_percent=" << percent(result.p95_us)
              << " p99_us=" << result.p99_us
              << " p99_percent=" << percent(result.p99_us)
              << " p99_9_us=" << result.p999_us
              << " p99_9_percent=" << percent(result.p999_us)
              << " max_us=" << result.max_us
              << " max_percent=" << percent(result.max_us)
              << " deadline_misses=" << result.deadline_misses << "/"
              << kMeasuredCallbacks << '\n';
    if (!std::all_of(block.begin(), block.end(),
                     [](float sample) { return std::isfinite(sample); })) {
        std::cerr << "profile produced non-finite output: " << name << '\n';
    }
    return result;
}

} // namespace

int main() {
    std::array<float, kBlockSamples> source{};
    std::array<float, kBlockSamples> block{};
    fill_source(source);

    tabletfx::Resonator manual(kSampleRate);
    auto manual_parameters = manual.parameters();
    manual_parameters.feedback.fill(0.7F);
    manual_parameters.filter_q.fill(3.0F);
    manual_parameters.mix = 0.65F;
    manual.set_parameters(manual_parameters);
    const Percentiles manual_stats =
        run_profile("manual_four_voice", manual, source, block);

    tabletfx::Resonator chordal(kSampleRate);
    auto chordal_parameters = chordal.parameters();
    chordal_parameters.mode = tabletfx::ResonatorMode::Chordal;
    chordal_parameters.chord = tabletfx::ResonatorChord::Dominant7;
    chordal_parameters.chord_root_frequency_hz = 146.83F;
    chordal_parameters.feedback.fill(0.75F);
    chordal_parameters.filter_q.fill(4.0F);
    chordal_parameters.mix = 0.7F;
    chordal.set_parameters(chordal_parameters);
    const Percentiles chordal_stats =
        run_profile("chordal_four_voice", chordal, source, block);

    tabletfx::Resonator global_mix(kSampleRate);
    auto global_mix_parameters = global_mix.parameters();
    global_mix_parameters.feedback.fill(0.7F);
    global_mix_parameters.filter_q.fill(3.0F);
    global_mix_parameters.mix = 0.65F;
    global_mix_parameters.global_mix_enabled = true;
    global_mix_parameters.global_mix_offset_percentage_points = 10.0F;
    global_mix.set_parameters(global_mix_parameters);
    const Percentiles global_mix_stats =
        run_profile("manual_four_voice_global_mix_enabled", global_mix,
                    source, block);

    const bool finite = std::all_of(
        block.begin(), block.end(),
        [](float sample) { return std::isfinite(sample); });
    const bool completed = manual_stats.p50_us >= 0.0 &&
                           chordal_stats.p50_us >= 0.0 &&
                           global_mix_stats.p50_us >= 0.0;
    return finite && completed ? 0 : 1;
}
