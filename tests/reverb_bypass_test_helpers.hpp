#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace reverb_test_support {

template <typename Reverb>
void verify_bypass_spillover(Reverb& no_tails, Reverb& tails,
                             Reverb& partitioned, Reverb& dry_input,
                             Reverb& reenabled, const std::string& name) {
    constexpr std::size_t seed_frames = 4096;
    std::vector<float> seed(seed_frames, 0.0F);
    seed[0] = 0.8F;

    auto no_tail_seed = seed;
    no_tails.process(no_tail_seed, 1);
    std::vector<float> dry_bypass(1024, 0.125F);
    const auto expected_dry = dry_bypass;
    no_tails.process_bypassed(dry_bypass, 1);
    if (dry_bypass != expected_dry) {
        throw std::runtime_error(name + " default bypass changed dry audio");
    }
    std::vector<float> cleared_state(4096, 0.0F);
    no_tails.process(cleared_state, 1);
    if (std::any_of(cleared_state.begin(), cleared_state.end(),
                    [](float sample) { return sample != 0.0F; })) {
        throw std::runtime_error(name + " default bypass retained a reverb tail");
    }

    auto tails_seed = seed;
    auto partitioned_seed = seed;
    auto dry_seed = seed;
    auto reenabled_seed = seed;
    tails.process(tails_seed, 1);
    partitioned.process(partitioned_seed, 1);
    dry_input.process(dry_seed, 1);
    reenabled.process(reenabled_seed, 1);

    constexpr std::size_t drain_frames = 5U * 48000U;
    std::vector<float> tail(drain_frames, 0.0F);
    std::vector<float> dry_during_tail(drain_frames, 0.125F);
    std::vector<float> split_tail(drain_frames, 0.0F);
    tails.process_bypassed(tail, 1);
    dry_input.process_bypassed(dry_during_tail, 1);

    const std::size_t block_sizes[]{173, 509, 127, 1024, 61};
    std::size_t offset = 0;
    std::size_t block_index = 0;
    while (offset < split_tail.size()) {
        const std::size_t count = std::min(
            block_sizes[block_index++ % std::size(block_sizes)],
            split_tail.size() - offset);
        partitioned.process_bypassed(
            std::span<float>(split_tail.data() + offset, count), 1);
        offset += count;
    }

    double early_energy = 0.0;
    double late_energy = 0.0;
    for (std::size_t frame = 0; frame < drain_frames; ++frame) {
        if (std::abs(tail[frame] - split_tail[frame]) > 1.0e-6F) {
            throw std::runtime_error(name + " tail changed with block partitioning");
        }
        if (std::abs((dry_during_tail[frame] - 0.125F) - tail[frame]) > 1.0e-6F) {
            throw std::runtime_error(name + " bypass tail was excited by new dry input");
        }
        const double power = static_cast<double>(tail[frame]) * tail[frame];
        if (frame < 48000U) early_energy += power;
        if (frame >= drain_frames - 48000U) late_energy += power;
    }
    if (early_energy <= 1.0e-7) {
        throw std::runtime_error(name + " enabled bypass produced no stored tail");
    }
    if (late_energy >= early_energy) {
        throw std::runtime_error(name + " bypass tail did not decay over time");
    }

    std::vector<float> bypass_before_reenable(256, 0.0F);
    reenabled.process_bypassed(bypass_before_reenable, 1);
    std::vector<float> active_after_reenable(4096, 0.0F);
    reenabled.process(active_after_reenable, 1);
    const double resumed_energy = [&] {
        double total = 0.0;
        for (float sample : active_after_reenable) {
            total += static_cast<double>(sample) * sample;
        }
        return total;
    }();
    if (resumed_energy <= 1.0e-7) {
        throw std::runtime_error(name + " re-enable discarded enabled spillover state");
    }
}

} // namespace reverb_test_support
