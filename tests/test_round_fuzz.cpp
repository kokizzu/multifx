#include "tabletfx/effects/round_fuzz.hpp"

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

bool near(float left, float right, float tolerance = 1.0e-6F) {
    return std::abs(left - right) <= tolerance;
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

namespace {

void test_defaults_bounds_and_exact_dry_path() {
    tabletfx::RoundFuzz fuzz;
    require(fuzz.id() == "round-fuzz", "stable effect ID changed");
    require(fuzz.fuzz() == 0.55F && fuzz.level_db() == -6.0F && fuzz.mix() == 0.85F,
            "sane default controls changed");
    require(fuzz.memory_bytes() == sizeof(fuzz),
            "memory report must match the fixed-state object");

    tabletfx::RoundFuzzParameters dry{};
    dry.fuzz = 1.0F;
    dry.level_db = 0.0F;
    dry.mix = 0.0F;
    dry.smoothing_ms = 0.0F;
    fuzz.configure(dry);
    std::array<float, 6> samples{
        0.25F, -0.75F, 1.0F, -2.0F, std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max()};
    const auto original = samples;
    fuzz.process(samples, 2);
    require(samples == original, "zero mix at unity level must be exact dry passthrough");

    fuzz.set_fuzz(-1.0F);
    require(fuzz.fuzz() == 0.0F, "fuzz lower bound failed");
    fuzz.set_fuzz(2.0F);
    require(fuzz.fuzz() == 1.0F, "fuzz upper bound failed");
    fuzz.set_level_db(-100.0F);
    require(fuzz.level_db() == -24.0F, "level lower bound failed");
    fuzz.set_level_db(100.0F);
    require(fuzz.level_db() == 12.0F, "level upper bound failed");
    fuzz.set_mix(-1.0F);
    require(fuzz.mix() == 0.0F, "mix lower bound failed");
    fuzz.set_mix(2.0F);
    require(fuzz.mix() == 1.0F, "mix upper bound failed");

    fuzz.set_fuzz(std::numeric_limits<float>::quiet_NaN());
    fuzz.set_level_db(std::numeric_limits<float>::infinity());
    fuzz.set_mix(std::numeric_limits<float>::quiet_NaN());
    require(fuzz.fuzz() == 0.55F && fuzz.level_db() == -6.0F && fuzz.mix() == 0.85F,
            "non-finite controls must return to their sane defaults");
}

void test_rounded_saturator_is_monotonic_and_odd() {
    tabletfx::RoundFuzzParameters parameters{};
    parameters.fuzz = 0.2F;
    parameters.level_db = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::RoundFuzz fuzz(48000.0F, parameters);
    constexpr std::array<float, 11> input{
        -1.0F, -0.5F, -0.25F, -0.1F, -0.05F, 0.0F,
        0.05F, 0.1F, 0.25F, 0.5F, 1.0F};
    auto output = input;
    fuzz.process(output, 1);

    for (std::size_t index = 1; index < output.size(); ++index) {
        require(output[index] >= output[index - 1],
                "rounded saturation must be monotonic over finite input");
    }
    for (std::size_t index = 0; index < output.size() / 2; ++index) {
        require(near(output[index], -output[output.size() - index - 1]),
                "round fuzz transfer must preserve odd symmetry");
        require(std::abs(output[index]) <= 1.0F,
                "saturator output must stay bounded at unity level");
    }
    require(output[5] == 0.0F && output[7] > 0.5F && output[7] < 1.0F &&
                output[8] > output[7] && output[8] < 1.0F,
            "fuzz curve should rise smoothly before reaching saturation");
}

void test_large_finite_inputs_silence_and_invalid_audio() {
    tabletfx::RoundFuzzParameters parameters{};
    parameters.fuzz = 1.0F;
    parameters.level_db = 12.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::RoundFuzz fuzz(48000.0F, parameters);
    std::array<float, 5> large{
        std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
        1.0e30F, -1.0e30F, 0.0F};
    fuzz.process(large, 1);
    const float output_limit = std::pow(10.0F, 12.0F / 20.0F);
    for (std::size_t index = 0; index < large.size(); ++index) {
        require(std::isfinite(large[index]), "large finite input produced non-finite output");
        require(std::abs(large[index]) <= output_limit + 1.0e-5F,
                "fully wet fuzz should remain bounded after output gain");
    }
    require(large[0] > 0.0F && large[1] < 0.0F && large[4] == 0.0F,
            "large finite inputs should saturate with preserved sign and silence");

    parameters.mix = 0.0F;
    tabletfx::RoundFuzz dry_boost(48000.0F, parameters);
    std::array<float, 1> huge_dry{std::numeric_limits<float>::max()};
    dry_boost.process(huge_dry, 1);
    require(std::isfinite(huge_dry[0]) && huge_dry[0] > 0.0F,
            "large finite dry input must not overflow at positive output gain");

    std::array<float, 3> invalid{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()};
    fuzz.process(invalid, 1);
    require(invalid == std::array<float, 3>{0.0F, 0.0F, 0.0F},
            "non-finite audio must recover to silence");
}

void test_stereo_isolation_and_linked_ramps() {
    tabletfx::RoundFuzzParameters initial{};
    initial.fuzz = 0.0F;
    initial.level_db = 0.0F;
    initial.mix = 1.0F;
    initial.smoothing_ms = 10.0F;
    tabletfx::RoundFuzz stereo(48000.0F, initial);
    tabletfx::RoundFuzz left(48000.0F, initial);
    tabletfx::RoundFuzz right(48000.0F, initial);
    stereo.set_fuzz(1.0F);
    left.set_fuzz(1.0F);
    right.set_fuzz(1.0F);

    std::array<float, 12> interleaved{
        0.08F, -0.16F, 0.11F, 0.22F, -0.07F, 0.31F,
        0.14F, -0.09F, -0.21F, 0.05F, 0.27F, -0.13F};
    std::array<float, 6> left_samples{};
    std::array<float, 6> right_samples{};
    for (std::size_t frame = 0; frame < left_samples.size(); ++frame) {
        left_samples[frame] = interleaved[frame * 2];
        right_samples[frame] = interleaved[frame * 2 + 1];
    }

    stereo.process(interleaved, 2);
    left.process(left_samples, 1);
    right.process(right_samples, 1);
    for (std::size_t frame = 0; frame < left_samples.size(); ++frame) {
        require(near(interleaved[frame * 2], left_samples[frame]) &&
                    near(interleaved[frame * 2 + 1], right_samples[frame]),
                "stereo channels must retain independent sample paths with linked controls");
    }

    tabletfx::RoundFuzz linked(48000.0F, initial);
    linked.set_fuzz(1.0F);
    std::array<float, 32> identical{};
    identical.fill(0.1F);
    linked.process(identical, 2);
    for (std::size_t index = 0; index < identical.size(); index += 2) {
        require(identical[index] == identical[index + 1],
                "stereo control ramps must advance once per frame");
    }
}

void test_variable_blocks_reset_and_ramp_completion() {
    tabletfx::RoundFuzzParameters initial{};
    initial.fuzz = 0.0F;
    initial.level_db = 0.0F;
    initial.mix = 1.0F;
    tabletfx::RoundFuzz whole(48000.0F, initial);
    tabletfx::RoundFuzz split(48000.0F, initial);
    whole.set_fuzz(1.0F);
    whole.set_level_db(-3.0F);
    whole.set_mix(0.75F);
    split.set_fuzz(1.0F);
    split.set_level_db(-3.0F);
    split.set_mix(0.75F);

    std::vector<float> whole_buffer(997, 0.1F);
    auto split_buffer = whole_buffer;
    whole.process(whole_buffer, 1);
    constexpr std::array<std::size_t, 5> blocks{1, 127, 53, 300, 17};
    std::size_t offset = 0;
    std::size_t block_index = 0;
    while (offset < split_buffer.size()) {
        const std::size_t count = std::min(blocks[block_index % blocks.size()],
                                           split_buffer.size() - offset);
        split.process(std::span<float>(split_buffer).subspan(offset, count), 1);
        offset += count;
        ++block_index;
    }
    for (std::size_t index = 0; index < whole_buffer.size(); ++index) {
        require(near(whole_buffer[index], split_buffer[index]),
                "control ramps must not depend on callback block partitioning");
    }
    require(whole_buffer.front() < whole_buffer.back(),
            "fuzz ramp did not progress toward its target");

    tabletfx::RoundFuzzParameters reset_parameters = initial;
    reset_parameters.level_db = -3.0F;
    reset_parameters.mix = 0.75F;
    tabletfx::RoundFuzz after_reset(48000.0F, reset_parameters);
    whole.set_fuzz(0.25F);
    for (std::size_t index = 0; index < 100; ++index) {
        std::array<float, 1> sample{0.1F};
        whole.process(sample, 1);
    }
    whole.reset();
    after_reset.set_fuzz(0.25F);
    after_reset.reset();
    std::array<float, 1> reset_sample{0.1F};
    std::array<float, 1> settled_sample{0.1F};
    whole.process(reset_sample, 1);
    after_reset.process(settled_sample, 1);
    require(near(reset_sample[0], settled_sample[0]),
            "reset must settle immediately to the configured target");

    tabletfx::RoundFuzz completion(48000.0F, initial);
    completion.set_fuzz(1.0F);
    std::array<float, 481> ramp{};
    ramp.fill(0.1F);
    completion.process(std::span<float>(ramp).first(480), 1);
    const float first_complete = ramp[479];
    completion.process(std::span<float>(ramp).subspan(480), 1);
    require(ramp[479] > ramp[0] && ramp[480] == first_complete,
            "10 ms ramp must reach its target on sample 480 and remain settled");
}

void test_layout_guards_partial_frame_and_no_allocations() {
    tabletfx::RoundFuzzParameters parameters{};
    parameters.fuzz = 0.2F;
    parameters.level_db = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::RoundFuzz effect(48000.0F, parameters);
    std::array<float, 2> untouched{0.25F, -0.5F};
    const auto original = untouched;
    effect.process(untouched, 0);
    require(untouched == original, "zero-channel layout must not touch samples");

    std::array<float, 3> partial{0.1F, -0.2F, 0.3F};
    auto expected = partial;
    tabletfx::RoundFuzz mono(48000.0F, parameters);
    mono.process(expected, 1);
    effect.process(partial, 2);
    require(partial == expected, "partial trailing interleaved frame was not processed safely");

    std::array<float, 512> buffer{};
    buffer.fill(0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) effect.process(buffer, 2);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "process allocated memory");
}

} // namespace

int main() {
    try {
        test_defaults_bounds_and_exact_dry_path();
        test_rounded_saturator_is_monotonic_and_odd();
        test_large_finite_inputs_silence_and_invalid_audio();
        test_stereo_isolation_and_linked_ramps();
        test_variable_blocks_reset_and_ramp_completion();
        test_layout_guards_partial_frame_and_no_allocations();
        std::cout << "All round-fuzz tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Round-fuzz test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
