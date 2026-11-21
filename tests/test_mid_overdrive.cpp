#include "tabletfx/effects/mid_overdrive.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

double render_rms(std::uint32_t frequency,
                  tabletfx::MidFocusedOverdriveParameters parameters) {
    constexpr std::uint32_t sample_rate = 48000;
    std::vector<float> samples(sample_rate * 2U);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 0.25F * std::sin(2.0 * 3.14159265358979323846 * frequency *
                                     static_cast<double>(i) / sample_rate);
    }
    tabletfx::MidFocusedOverdrive effect(static_cast<float>(sample_rate), parameters);
    effect.process(samples, 1);
    double sum = 0.0;
    constexpr std::size_t first = 24000;
    for (std::size_t i = first; i < samples.size(); ++i) {
        sum += static_cast<double>(samples[i]) * samples[i];
    }
    return std::sqrt(sum / static_cast<double>(samples.size() - first));
}

void test_defaults_bounds_and_dry_mix() {
    tabletfx::MidFocusedOverdrive effect;
    require(effect.id() == "mid-focused-overdrive", "stable effect ID changed");
    require(effect.drive() == 0.18F && effect.mid_boost_db() == 6.0F &&
                effect.level_db() == -4.0F && effect.mix() == 0.4F,
            "sane default controls changed");
    require(effect.memory_bytes() == sizeof(effect),
            "memory report must match the fixed-state object");

    tabletfx::MidFocusedOverdriveParameters dry{};
    dry.drive = 0.5F;
    dry.mid_boost_db = 12.0F;
    dry.level_db = 0.0F;
    dry.mix = 0.0F;
    dry.smoothing_ms = 0.0F;
    effect.configure(dry);
    std::array<float, 8> samples{0.1F, -0.25F, 0.5F, -0.75F,
                                 0.0F, 0.125F, -0.5F, 0.9F};
    const auto original = samples;
    effect.process(samples, 2);
    require(samples == original, "zero mix at unity level must be exact dry passthrough");

    effect.set_drive(-1.0F);
    require(effect.drive() == 0.0F, "drive lower bound failed");
    effect.set_drive(9.0F);
    require(effect.drive() == 0.5F, "drive upper bound failed");
    effect.set_mid_boost_db(-1.0F);
    require(effect.mid_boost_db() == 0.0F, "mid boost lower bound failed");
    effect.set_mid_boost_db(20.0F);
    require(effect.mid_boost_db() == 12.0F, "mid boost upper bound failed");
    effect.set_level_db(-100.0F);
    require(effect.level_db() == -24.0F, "level lower bound failed");
    effect.set_level_db(100.0F);
    require(effect.level_db() == 12.0F, "level upper bound failed");
    effect.set_mix(-1.0F);
    require(effect.mix() == 0.0F, "mix lower bound failed");
    effect.set_mix(2.0F);
    require(effect.mix() == 1.0F, "mix upper bound failed");
    effect.set_drive(std::numeric_limits<float>::quiet_NaN());
    effect.set_mid_boost_db(std::numeric_limits<float>::infinity());
    effect.set_level_db(std::numeric_limits<float>::quiet_NaN());
    effect.set_mix(std::numeric_limits<float>::infinity());
    require(effect.drive() == 0.18F && effect.mid_boost_db() == 6.0F &&
                effect.level_db() == -4.0F && effect.mix() == 0.4F,
            "non-finite controls must fall back to documented defaults");
}

void test_mid_emphasis_precedes_soft_clipping() {
    tabletfx::MidFocusedOverdriveParameters parameters{};
    parameters.drive = 0.0F;
    parameters.mid_boost_db = 12.0F;
    parameters.level_db = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    const double bass = render_rms(90, parameters);
    const double mids = render_rms(800, parameters);
    require(mids > bass * 1.25,
            "mid-focused overdrive must emphasize mids before clipping");

    parameters.mid_boost_db = 0.0F;
    const double flat_low = render_rms(90, parameters);
    const double flat_mid = render_rms(800, parameters);
    require(std::abs(flat_mid - flat_low) < flat_low * 0.08,
            "zero mid boost should not add a frequency-dependent gain stage");
}

void test_soft_clip_symmetry_and_stereo_independence() {
    tabletfx::MidFocusedOverdriveParameters parameters{};
    parameters.drive = 0.35F;
    parameters.mid_boost_db = 6.0F;
    parameters.level_db = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::MidFocusedOverdrive stereo(48000.0F, parameters);
    std::array<float, 8> interleaved{0.5F, -0.5F, 0.2F, 0.7F,
                                    -2.0F, 2.0F, 0.3F, -0.1F};
    auto original = interleaved;
    stereo.process(interleaved, 2);
    require(interleaved[0] > 0.5F && interleaved[1] < -0.5F,
            "drive did not apply the soft clip");
    require(std::abs(interleaved[0] + interleaved[1]) < 1.0e-6F,
            "soft clip must preserve odd symmetry on matching channel histories");

    tabletfx::MidFocusedOverdrive left(48000.0F, parameters);
    tabletfx::MidFocusedOverdrive right(48000.0F, parameters);
    for (std::size_t frame = 0; frame < original.size() / 2; ++frame) {
        std::array<float, 1> left_sample{original[frame * 2]};
        std::array<float, 1> right_sample{original[frame * 2 + 1]};
        left.process(left_sample, 1);
        right.process(right_sample, 1);
        require(std::abs(interleaved[frame * 2] - left_sample[0]) < 1.0e-6F &&
                    std::abs(interleaved[frame * 2 + 1] - right_sample[0]) < 1.0e-6F,
                "stereo filter histories leaked across channels");
    }
    require(std::abs(interleaved[4]) <= 1.0F && std::abs(interleaved[5]) <= 1.0F,
            "large finite samples must saturate within the output bound");
}

void test_smoothing_block_invariance_and_reset() {
    tabletfx::MidFocusedOverdriveParameters initial{};
    initial.drive = 0.0F;
    initial.mid_boost_db = 0.0F;
    initial.level_db = 0.0F;
    initial.mix = 1.0F;
    initial.smoothing_ms = 10.0F;
    tabletfx::MidFocusedOverdrive whole(48000.0F, initial);
    tabletfx::MidFocusedOverdrive split(48000.0F, initial);
    whole.set_drive(0.5F);
    whole.set_mid_boost_db(12.0F);
    whole.set_level_db(-6.0F);
    split.set_drive(0.5F);
    split.set_mid_boost_db(12.0F);
    split.set_level_db(-6.0F);
    std::vector<float> whole_buffer(1994, 0.35F);
    auto split_buffer = whole_buffer;
    whole.process(whole_buffer, 2);
    // Stereo callbacks must split on frame boundaries (two interleaved samples).
    constexpr std::array<std::size_t, 5> blocks{2, 254, 106, 600, 34};
    std::size_t offset = 0;
    std::size_t block_index = 0;
    while (offset < split_buffer.size()) {
        const std::size_t count = std::min(blocks[block_index % blocks.size()],
                                           split_buffer.size() - offset);
        split.process(std::span<float>(split_buffer).subspan(offset, count), 2);
        offset += count;
        ++block_index;
    }
    for (std::size_t i = 0; i < whole_buffer.size(); ++i) {
        require(std::abs(whole_buffer[i] - split_buffer[i]) < 1.0e-6F,
                "control smoothing depends on callback partitioning");
    }

    tabletfx::MidFocusedOverdrive linked(48000.0F, initial);
    linked.set_drive(0.5F);
    linked.set_mid_boost_db(12.0F);
    linked.set_level_db(-6.0F);
    std::array<float, 1024> stereo_samples{};
    stereo_samples.fill(0.35F);
    linked.process(stereo_samples, 2);
    for (std::size_t i = 0; i < stereo_samples.size(); i += 2) {
        require(stereo_samples[i] == stereo_samples[i + 1],
                "stereo control ramps must advance once per frame");
    }

    whole.reset();
    tabletfx::MidFocusedOverdrive settled(48000.0F, initial);
    settled.set_drive(0.5F);
    settled.set_mid_boost_db(12.0F);
    settled.set_level_db(-6.0F);
    settled.reset();
    std::array<float, 1> after_reset{0.35F};
    std::array<float, 1> fresh_sample{0.35F};
    whole.process(after_reset, 1);
    settled.process(fresh_sample, 1);
    require(std::abs(after_reset[0] - fresh_sample[0]) < 1.0e-6F,
            "reset did not clear filters and settle configured parameters");
}

void test_invalid_layout_non_finite_and_no_allocations() {
    tabletfx::MidFocusedOverdrive effect;
    std::array<float, 4> untouched{0.25F, -0.5F, 0.75F, -1.0F};
    const auto original = untouched;
    effect.process(untouched, 0);
    effect.process(untouched, 3);
    require(untouched == original, "unsupported channel layout must be a no-op");

    std::array<float, 5> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.25F, -0.5F};
    effect.process(invalid, 1);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F && invalid[2] == 0.0F &&
                std::isfinite(invalid[3]) && std::isfinite(invalid[4]),
            "non-finite audio must be silenced and filter state recovered");

    std::array<float, 2> partial_frame{0.25F, -0.5F};
    effect.process(partial_frame, 2);
    require(std::isfinite(partial_frame[0]) && std::isfinite(partial_frame[1]),
            "short final interleaved frame must remain safe");

    std::vector<float> block(512, 0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) effect.process(block, 2);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "process() must not allocate memory");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_bounds_and_dry_mix();
        test_mid_emphasis_precedes_soft_clipping();
        test_soft_clip_symmetry_and_stereo_independence();
        test_smoothing_block_invariance_and_reset();
        test_invalid_layout_non_finite_and_no_allocations();
        std::cout << "All mid-focused-overdrive tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
