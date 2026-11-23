#include "tabletfx/effects/rock_box_boost.hpp"

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

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> signal(std::size_t frames, float value = 0.01F) {
    return std::vector<float>(frames, value);
}

double rms(const std::vector<float>& samples, std::size_t skip = 0U) {
    double sum = 0.0;
    for (std::size_t i = skip; i < samples.size(); ++i) {
        const double value = samples[i];
        sum += value * value;
    }
    return std::sqrt(sum / static_cast<double>(samples.size() - skip));
}

std::vector<float> render(tabletfx::RockBoxBoostParameters parameters,
                          std::vector<float> samples) {
    parameters.smoothing_ms = 0.0F;
    tabletfx::RockBoxBoost effect(48000.0F, parameters);
    effect.process(samples, 1U);
    return samples;
}

void test_defaults_bounds_and_identity() {
    tabletfx::RockBoxBoost effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "rock-box-boost", "Rock Box Boost stable ID changed");
    require(defaults.boost_db == 0.0F && defaults.level_db == 0.0F &&
                defaults.smoothing_ms == 10.0F,
            "project-authored Rock Box Boost defaults changed");
    require(effect.memory_bytes() == sizeof(effect),
            "Rock Box Boost memory report changed");

    std::array<float, 4> identity{0.1F, -0.2F, 0.3F, -0.4F};
    const auto original = identity;
    effect.process(identity, 1U);
    require(identity == original, "zero boost and level should be unity gain");

    tabletfx::RockBoxBoostParameters p;
    p.boost_db = 100.0F;
    p.level_db = -100.0F;
    p.smoothing_ms = -1.0F;
    effect.set_parameters(p);
    const auto bounded = effect.parameters();
    require(bounded.boost_db == 24.0F && bounded.level_db == -24.0F &&
                bounded.smoothing_ms == 0.0F,
            "project-authored Boost bounds failed");

    p.boost_db = std::numeric_limits<float>::quiet_NaN();
    p.level_db = std::numeric_limits<float>::infinity();
    p.smoothing_ms = std::numeric_limits<float>::infinity();
    effect.set_parameters(p);
    const auto fallback = effect.parameters();
    require(fallback.boost_db == 0.0F && fallback.level_db == 0.0F &&
                fallback.smoothing_ms == 10.0F,
            "non-finite parameter fallbacks failed");
}

void test_boost_and_block_level_are_independent_controls() {
    const auto source = signal(8192U);
    const auto unity = render({}, source);

    tabletfx::RockBoxBoostParameters p;
    p.boost_db = 6.0F;
    const auto boosted = render(p, source);
    require(rms(boosted, 128U) > rms(unity, 128U) * 1.9,
            "Boost did not raise the boost circuit output");

    p = {};
    p.level_db = 6.0F;
    const auto leveled = render(p, source);
    require(rms(leveled, 128U) > rms(unity, 128U) * 1.9,
            "Level did not raise the overall block output");

    p.boost_db = 6.0F;
    p.level_db = -6.0F;
    const auto compensated = render(p, source);
    require(std::abs(rms(compensated, 128U) / rms(unity, 128U) - 1.0) < 0.001,
            "separate Boost and Level gains did not compensate independently");
}

void test_stereo_partial_frames_ramp_reset_and_no_allocations() {
    tabletfx::RockBoxBoostParameters start;
    start.smoothing_ms = 1.0F;
    tabletfx::RockBoxBoost whole(48000.0F, start);
    tabletfx::RockBoxBoost split(48000.0F, start);
    auto target = start;
    target.boost_db = 12.0F;
    target.level_db = -3.0F;
    whole.set_parameters(target);
    split.set_parameters(target);

    auto whole_audio = signal(997U);
    auto split_audio = whole_audio;
    whole.process(whole_audio, 1U);
    constexpr std::array<std::size_t, 5> blocks{1U, 127U, 53U, 300U, 17U};
    std::size_t offset = 0U;
    std::size_t block = 0U;
    while (offset < split_audio.size()) {
        const auto count = std::min(blocks[block % blocks.size()],
                                    split_audio.size() - offset);
        split.process(std::span<float>(split_audio).subspan(offset, count), 1U);
        offset += count;
        ++block;
    }
    require(whole_audio == split_audio,
            "Boost parameter ramp changed with callback partitioning");

    std::array<float, 3> partial{0.1F, -0.2F, 0.3F};
    tabletfx::RockBoxBoost stereo(48000.0F, target);
    stereo.process(partial, 2U);
    require(partial[0] != 0.1F && partial[1] != -0.2F &&
                std::isfinite(partial[2]),
            "partial stereo frame was not processed safely");

    std::array<float, 4> audio{0.1F, -0.2F, 0.3F, -0.4F};
    audio[2] = std::numeric_limits<float>::quiet_NaN();
    allocation_count = 0U;
    count_allocations = true;
    stereo.process(audio, 2U);
    count_allocations = false;
    require(allocation_count == 0U, "Rock Box Boost processing allocated memory");
    require(audio[2] == 0.0F, "non-finite input was not cleared");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_bounds_and_identity();
        test_boost_and_block_level_are_independent_controls();
        test_stereo_partial_frames_ramp_reset_and_no_allocations();
        std::cout << "Rock Box Boost tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        count_allocations = false;
        std::cerr << "Rock Box Boost test failure: " << exception.what() << '\n';
        return 1;
    }
}
