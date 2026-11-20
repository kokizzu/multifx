#include "tabletfx/parallel_mixer.hpp"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_near(float actual, float expected, float tolerance, const char* message) {
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
}

void test_split_duplicates_mono_and_stereo() {
    const std::vector<float> mono{0.25F, -0.5F};
    std::vector<float> a(2);
    std::vector<float> b(2);
    auto result = tabletfx::split_to_parallel(mono, a, b, 1);
    require(result.succeeded() && result.frames == 2, "mono splitter failed");
    require(a == mono && b == mono, "mono splitter changed branch samples");

    const std::vector<float> stereo{0.1F, -0.2F, 0.3F, -0.4F};
    std::vector<float> stereo_a(4);
    std::vector<float> stereo_b(4);
    result = tabletfx::split_to_parallel(stereo, stereo_a, stereo_b, 2);
    require(result.succeeded() && stereo_a == stereo && stereo_b == stereo,
            "stereo splitter did not preserve interleaved branch data");
}

void test_default_mix_preserves_dry_signal() {
    tabletfx::ParallelMixer mixer(48000.0F, 0.0F);
    const std::vector<float> dry{0.25F, -0.5F, 0.75F, -1.0F};
    const std::vector<float> branch_a{1.0F, 1.0F, 1.0F, 1.0F};
    const std::vector<float> branch_b{-1.0F, -1.0F, -1.0F, -1.0F};
    std::vector<float> output(4);
    const auto result = mixer.process(dry, branch_a, branch_b, output, 2);
    require(result.succeeded() && output == dry,
            "safe default mixer did not pass the dry signal unchanged");
}

void test_branch_gain_balance_and_phase_inversion() {
    tabletfx::ParallelMixer mixer(48000.0F, 0.0F);
    tabletfx::ParallelMixParameters parameters;
    parameters.dry_gain = 1.0F;
    parameters.branch_a_gain = 0.5F;
    parameters.branch_a_balance = 1.0F;
    parameters.branch_b_gain = 0.25F;
    parameters.branch_b_balance = -1.0F;
    parameters.branch_b_inverted = true;
    mixer.set_parameters_immediate(parameters);

    const std::vector<float> dry{0.2F, 0.4F};
    const std::vector<float> branch_a{1.0F, 0.5F};
    const std::vector<float> branch_b{0.8F, 0.1F};
    std::vector<float> output(2);
    const auto result = mixer.process(dry, branch_a, branch_b, output, 2);
    require(result.succeeded(), "stereo parallel mixing failed");
    require_near(output[0], 0.0F, 1.0e-7F,
                 "left branch balance or phase inversion is incorrect");
    require_near(output[1], 0.65F, 1.0e-7F,
                 "right branch balance or gain is incorrect");

    parameters.branch_a_balance = 0.0F;
    parameters.branch_a_gain = 1.0F;
    parameters.branch_b_gain = 0.0F;
    parameters.branch_b_inverted = false;
    mixer.set_parameters_immediate(parameters);
    std::vector<float> cancellation(2);
    const std::vector<float> polarity{0.25F, -0.75F};
    const std::vector<float> silence(2, 0.0F);
    const auto cancel = mixer.process(silence, polarity, silence, cancellation, 2);
    require(cancel.succeeded(), "phase-inversion mix failed");
    parameters.branch_a_inverted = true;
    mixer.set_parameters_immediate(parameters);
    const auto inverted = mixer.process(silence, polarity, silence, cancellation, 2);
    require(inverted.succeeded(), "phase-inverted mix failed");
    require_near(cancellation[0], -polarity[0], 1.0e-7F,
                 "phase inversion did not reverse polarity");
    require_near(cancellation[1], -polarity[1], 1.0e-7F,
                 "phase inversion changed one channel incorrectly");
}

void test_mono_ignores_balance_and_branch_gain_is_bounded() {
    tabletfx::ParallelMixer mixer(48000.0F, 0.0F);
    tabletfx::ParallelMixParameters parameters;
    parameters.dry_gain = 0.0F;
    parameters.branch_a_gain = 10.0F;
    parameters.branch_a_balance = 1.0F;
    mixer.set_parameters_immediate(parameters);
    const std::vector<float> dry{0.0F};
    const std::vector<float> branch{0.25F};
    const std::vector<float> silence{0.0F};
    std::vector<float> output(1);
    const auto result = mixer.process(dry, branch, silence, output, 1);
    require(result.succeeded(), "mono mix failed");
    require_near(output[0], 0.5F, 1.0e-7F,
                 "mono balance was not ignored or branch gain was not bounded");
}

void test_gain_and_phase_changes_ramp_without_clicks() {
    tabletfx::ParallelMixer mixer(8000.0F, 10.0F);
    tabletfx::ParallelMixParameters parameters;
    parameters.dry_gain = 0.0F;
    parameters.branch_a_gain = 1.0F;
    mixer.set_parameters(parameters);
    const std::vector<float> dry(80, 0.0F);
    const std::vector<float> branch(80, 1.0F);
    const std::vector<float> silence(80, 0.0F);
    std::vector<float> output(80);
    const auto result = mixer.process(dry, branch, silence, output, 1);
    require(result.succeeded(), "gain ramp render failed");
    require_near(output.front(), 1.0F / 80.0F, 1.0e-7F,
                 "branch gain ramp did not start smoothly");
    require_near(output.back(), 1.0F, 1.0e-6F,
                 "branch gain ramp did not reach its target");

    parameters.branch_a_inverted = true;
    mixer.set_parameters(parameters);
    const std::vector<float> first(17, 0.0F);
    const std::vector<float> first_branch(17, 1.0F);
    const std::vector<float> first_silence(17, 0.0F);
    std::vector<float> first_output(17);
    auto part = mixer.process(first, first_branch, first_silence, first_output, 1);
    require(part.succeeded(), "phase-ramp first block failed");
    require(first_output.front() < 1.0F && first_output.front() > 0.0F,
            "phase inversion did not ramp from positive polarity");
    const std::vector<float> rest(63, 0.0F);
    const std::vector<float> rest_branch(63, 1.0F);
    const std::vector<float> rest_silence(63, 0.0F);
    std::vector<float> rest_output(63);
    part = mixer.process(rest, rest_branch, rest_silence, rest_output, 1);
    require(part.succeeded(), "phase-ramp second block failed");
    require_near(rest_output.back(), -1.0F, 1.0e-6F,
                 "phase inversion ramp did not reach negative polarity");
}

void test_invalid_buffers_and_no_allocations() {
    std::vector<float> input{0.25F, -0.25F};
    std::vector<float> a(2);
    std::vector<float> b(2);
    auto result = tabletfx::split_to_parallel(input, a, b, 3);
    require(result.status == tabletfx::ChannelMixStatus::UnsupportedChannelCount,
            "splitter accepted unsupported channels");
    result = tabletfx::split_to_parallel(input, std::span<float>(a.data(), 1), b, 2);
    require(result.status == tabletfx::ChannelMixStatus::BufferTooSmall,
            "splitter accepted a short branch buffer");

    tabletfx::ParallelMixer mixer(48000.0F, 0.0F);
    std::vector<float> output(2, 9.0F);
    const std::vector<float> short_branch{0.0F};
    result = mixer.process(input, short_branch, b, output, 2);
    require(result.status == tabletfx::ChannelMixStatus::BufferTooSmall,
            "mixer accepted a short branch buffer");
    require(output == std::vector<float>({9.0F, 9.0F}),
            "mixer modified output before validating input buffers");

    std::vector<float> overlap{0.25F, 0.5F, 0.75F, 1.0F};
    result = tabletfx::split_to_parallel(
        std::span<const float>(overlap.data(), 2),
        std::span<float>(overlap.data(), 2),
        std::span<float>(overlap.data() + 1, 2), 1);
    require(result.status == tabletfx::ChannelMixStatus::OverlappingBuffers,
            "splitter accepted overlapping branches");

    std::vector<float> dry(256, 0.1F);
    std::vector<float> branch_a(256, 0.2F);
    std::vector<float> branch_b(256, 0.3F);
    std::vector<float> output_buffer(256);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int i = 0; i < 64; ++i) {
        require(tabletfx::split_to_parallel(dry, branch_a, branch_b, 1).succeeded(),
                "repeated split failed");
        require(mixer.process(dry, branch_a, branch_b, output_buffer, 1).succeeded(),
                "repeated mix failed");
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "parallel routing allocated memory");
    require(mixer.memory_bytes() == sizeof(mixer), "parallel mixer memory reporting is incorrect");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_split_duplicates_mono_and_stereo();
        test_default_mix_preserves_dry_signal();
        test_branch_gain_balance_and_phase_inversion();
        test_mono_ignores_balance_and_branch_gain_is_bounded();
        test_gain_and_phase_changes_ramp_without_clicks();
        test_invalid_buffers_and_no_allocations();
        std::cout << "Parallel splitter/mixer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
