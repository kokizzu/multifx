#include "tabletfx/effects/fractal_surround_delay.hpp"

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

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float actual, float expected, float tolerance = 1.0e-4F) {
    return std::abs(actual - expected) <= tolerance;
}

std::array<float, 2> second_stereo_echo(float rotation_percent) {
    tabletfx::FractalSurroundDelayParameters parameters;
    parameters.time_ms = 5.0F;
    parameters.feedback_left_percent = 100.0F;
    parameters.feedback_right_percent = 100.0F;
    parameters.rotation_percent = rotation_percent;
    parameters.mix_percent = 100.0F;
    parameters.width_percent = 100.0F;
    tabletfx::FractalSurroundDelay delay(8000.0F, parameters);
    std::array<float, 162> audio{}; // 81 stereo frames: two 40-frame echoes.
    audio[0] = 1.0F;
    audio[1] = 1.0F;
    delay.process(audio, 2U);
    return {audio[160], audio[161]};
}

void test_defaults_memory_and_time_relationships() {
    tabletfx::FractalSurroundDelay delay;
    const auto parameters = delay.parameters();
    require(delay.id() == "fractal-surround-delay",
            "Fractal Surround Delay picker ID changed");
    require(parameters.time_ms == 375.0F &&
                parameters.master_time_percent == 100.0F &&
                parameters.right_time_ratio_percent == 100.0F &&
                parameters.feedback_left_percent == 35.0F &&
                parameters.feedback_right_percent == 35.0F &&
                parameters.master_feedback_percent == 100.0F &&
                parameters.rotation_percent == 50.0F &&
                parameters.width_percent == 100.0F &&
                parameters.mix_percent == 25.0F && parameters.level_db == 0.0F,
            "project-authored delay defaults changed");
    require(delay.maximum_delay_frames() == 18000U &&
                delay.maximum_supported_delay_frames() == 192000U,
            "default storage should match 375 ms while supported echoes cap at four seconds");
    const std::size_t expected_ring_bytes = 18000U * 2U * sizeof(float);
    require(delay.memory_bytes() >= sizeof(delay) + expected_ring_bytes &&
                delay.memory_bytes() <= sizeof(delay) + expected_ring_bytes + 4096U,
            "delay memory report should include only the currently required stereo ring");
}

void test_stereo_time_and_ratio_bounds() {
    tabletfx::FractalSurroundDelayParameters parameters;
    parameters.time_ms = 100.0F;
    parameters.master_time_percent = 200.0F;
    parameters.right_time_ratio_percent = 50.0F;
    tabletfx::FractalSurroundDelay delay(8000.0F, parameters);
    require(delay.delay_left_frames() == 1600U &&
                delay.delay_right_frames() == 800U,
            "Master Time and right-channel ratio should scale independent delays");

    parameters.time_ms = 500.0F;
    parameters.master_time_percent = 400.0F;
    parameters.right_time_ratio_percent = 200.0F;
    delay.set_parameters(parameters);
    require(delay.delay_left_frames() == 16000U &&
                delay.delay_right_frames() == delay.maximum_delay_frames(),
            "project storage should grow to the longer selected right-channel time");
}

void test_project_authored_bounds_and_non_finite_controls() {
    tabletfx::FractalSurroundDelayParameters parameters;
    parameters.time_ms = std::numeric_limits<float>::quiet_NaN();
    parameters.master_time_percent = -std::numeric_limits<float>::infinity();
    parameters.right_time_ratio_percent = -100.0F;
    parameters.feedback_left_percent = std::numeric_limits<float>::infinity();
    parameters.feedback_right_percent = -10.0F;
    parameters.master_feedback_percent = 300.0F;
    parameters.rotation_percent = 300.0F;
    parameters.width_percent = -500.0F;
    parameters.mix_percent = 110.0F;
    parameters.level_db = -100.0F;
    tabletfx::FractalSurroundDelay delay(48000.0F, parameters);

    const auto safe = delay.parameters();
    require(safe.time_ms == 375.0F && safe.master_time_percent == 100.0F,
            "non-finite delay controls should use the documented project defaults");
    require(safe.right_time_ratio_percent == 25.0F &&
                safe.feedback_left_percent == 35.0F &&
                safe.feedback_right_percent == 0.0F &&
                safe.master_feedback_percent == 200.0F,
            "ratio and feedback controls escaped project bounds");
    require(safe.rotation_percent == 200.0F && safe.width_percent == -100.0F &&
                safe.mix_percent == 100.0F && safe.level_db == -24.0F,
            "rotation, width, mix, or level escaped project bounds");
}

void test_mono_echo_feedback_and_dry_path() {
    tabletfx::FractalSurroundDelayParameters parameters;
    parameters.time_ms = 5.0F;
    parameters.feedback_left_percent = 50.0F;
    parameters.feedback_right_percent = 50.0F;
    parameters.rotation_percent = 100.0F; // Mono has no stereo field to rotate.
    parameters.mix_percent = 100.0F;
    tabletfx::FractalSurroundDelay delay(8000.0F, parameters);
    std::array<float, 81> audio{};
    audio[0] = 1.0F;
    delay.process(audio, 1U);
    require(near(audio[0], 0.0F) && near(audio[40], 1.0F) &&
                near(audio[80], 0.5F),
            "mono delay should retain a dry-free first echo and stable feedback repeat");

    tabletfx::FractalSurroundDelayParameters dry_parameters;
    dry_parameters.time_ms = 5.0F;
    dry_parameters.mix_percent = 0.0F;
    tabletfx::FractalSurroundDelay dry(8000.0F, dry_parameters);
    std::array<float, 2> dry_audio{0.375F, -0.25F};
    dry.process(dry_audio, 1U);
    require(near(dry_audio[0], 0.375F) && near(dry_audio[1], -0.25F),
            "zero wet mix must preserve the direct path without added delay");
}

void test_stereo_rotation_direction_and_repeat_mapping() {
    const auto clockwise = second_stereo_echo(50.0F);
    const auto counterclockwise = second_stereo_echo(-50.0F);
    require(near(clockwise[0], -0.98F) && near(clockwise[1], 0.98F),
            "positive rotation should send the second echo toward the project right side");
    require(near(counterclockwise[0], 0.98F) && near(counterclockwise[1], -0.98F),
            "negative rotation should reverse the second echo direction");

    const auto half_turn = second_stereo_echo(100.0F);
    require(near(half_turn[0], -0.98F) && near(half_turn[1], -0.98F),
            "the project angle scale should make 100 percent a 180 degree repeat rotation");
}

void test_delay_state_is_continuous_across_blocks_and_control_changes() {
    tabletfx::FractalSurroundDelayParameters parameters;
    parameters.time_ms = 20.0F;
    parameters.feedback_left_percent = 40.0F;
    parameters.feedback_right_percent = 55.0F;
    parameters.right_time_ratio_percent = 125.0F;
    parameters.rotation_percent = 35.0F;
    parameters.mix_percent = 60.0F;
    tabletfx::FractalSurroundDelay whole(8000.0F, parameters);
    tabletfx::FractalSurroundDelay split(8000.0F, parameters);
    std::array<float, 2048> whole_audio{};
    std::array<float, 2048> split_audio{};
    for (std::size_t i = 0U; i < whole_audio.size(); ++i) {
        whole_audio[i] = std::sin(static_cast<float>(i) * 0.031F) * 0.25F;
    }
    split_audio = whole_audio;
    whole.process(whole_audio, 2U);

    constexpr std::array<std::size_t, 6> block_frames{1U, 37U, 11U, 83U, 5U, 29U};
    std::size_t frame_offset = 0U;
    std::size_t block_index = 0U;
    while (frame_offset < split_audio.size() / 2U) {
        const std::size_t count = std::min(
            block_frames[block_index % block_frames.size()],
            split_audio.size() / 2U - frame_offset);
        split.process(std::span<float>(split_audio).subspan(
                          frame_offset * 2U, count * 2U),
                      2U);
        frame_offset += count;
        ++block_index;
    }
    for (std::size_t i = 0U; i < whole_audio.size(); ++i) {
        require(near(whole_audio[i], split_audio[i]),
                "delay output must be invariant to callback partitioning");
    }

    auto continuity_parameters = parameters;
    continuity_parameters.time_ms = 10.0F;
    continuity_parameters.right_time_ratio_percent = 100.0F;
    continuity_parameters.mix_percent = 100.0F;
    continuity_parameters.feedback_left_percent = 0.0F;
    continuity_parameters.feedback_right_percent = 0.0F;
    tabletfx::FractalSurroundDelay continuity(8000.0F, continuity_parameters);
    std::array<float, 2> impulse{1.0F, 1.0F};
    continuity.process(impulse, 2U);
    auto updated = continuity_parameters;
    updated.width_percent = 50.0F;
    continuity.set_parameters(updated);
    std::array<float, 158> silence{};
    continuity.process(silence, 2U);
    std::array<float, 2> first_echo{};
    continuity.process(first_echo, 2U);
    require(near(first_echo[0], 1.0F) && near(first_echo[1], 1.0F),
            "changing controls must preserve pending echo history");
}

void test_delay_change_ramps_without_clearing_state() {
    tabletfx::FractalSurroundDelayParameters parameters;
    parameters.time_ms = 10.0F;
    parameters.mix_percent = 100.0F;
    parameters.feedback_left_percent = 0.0F;
    parameters.feedback_right_percent = 0.0F;
    tabletfx::FractalSurroundDelay delay(8000.0F, parameters);
    std::array<float, 2> impulse{1.0F, 1.0F};
    delay.process(impulse, 2U);
    parameters.time_ms = 20.0F;
    delay.set_parameters(parameters);
    std::array<float, 318> silence{};
    delay.process(silence, 2U);
    std::array<float, 2> echo{};
    delay.process(echo, 2U);
    require(std::isfinite(echo[0]) && std::isfinite(echo[1]) &&
                (std::abs(echo[0]) > 0.1F || std::abs(echo[1]) > 0.1F),
            "time changes should ramp the read head and retain existing delay state");
}

void test_reset_clears_tails_and_preserves_selected_parameters() {
    tabletfx::FractalSurroundDelayParameters parameters;
    parameters.time_ms = 5.0F;
    parameters.feedback_left_percent = 50.0F;
    parameters.feedback_right_percent = 50.0F;
    parameters.mix_percent = 100.0F;
    tabletfx::FractalSurroundDelay delay(8000.0F, parameters);
    std::array<float, 2> impulse{1.0F, 1.0F};
    delay.process(impulse, 2U);
    delay.reset();
    require(delay.parameters().time_ms == parameters.time_ms &&
                delay.parameters().feedback_left_percent ==
                    parameters.feedback_left_percent,
            "reset should keep the selected controls");
    std::array<float, 100> silence{};
    delay.process(silence, 2U);
    require(std::all_of(silence.begin(), silence.end(),
                        [](float sample) { return near(sample, 0.0F); }),
            "reset must clear all pending echoes");
}

void test_non_finite_audio_is_sanitized_before_feedback() {
    tabletfx::FractalSurroundDelayParameters parameters;
    parameters.time_ms = 5.0F;
    parameters.feedback_left_percent = 100.0F;
    parameters.feedback_right_percent = 100.0F;
    parameters.master_feedback_percent = 200.0F;
    parameters.rotation_percent = 200.0F;
    parameters.mix_percent = 100.0F;
    tabletfx::FractalSurroundDelay delay(8000.0F, parameters);
    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(), 0.25F, -0.25F, 0.0F};
    delay.process(invalid, 2U);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "non-finite or extreme input must not poison the stereo delay output");

    std::array<float, 2000> tail{};
    delay.process(tail, 2U);
    require(std::all_of(tail.begin(), tail.end(), [](float sample) {
                return std::isfinite(sample) && std::abs(sample) <= 8.0F;
            }),
            "feedback safety limit must keep later echoes finite and bounded");
}

void test_invalid_channel_layouts_and_process_allocations() {
    tabletfx::FractalSurroundDelay delay;
    std::array<float, 2> untouched{0.25F, -0.5F};
    delay.process(untouched, 0U);
    require(untouched == std::array<float, 2>{0.25F, -0.5F},
            "zero-channel processing must leave the buffer untouched");

    std::array<float, 3> unsupported{
        0.25F, std::numeric_limits<float>::quiet_NaN(), -0.5F};
    delay.process(unsupported, 3U);
    require(std::isfinite(unsupported[0]) && std::isfinite(unsupported[1]) &&
                std::isfinite(unsupported[2]),
            "unsupported layouts should remain passthrough and sanitize invalid audio");

    std::array<float, 2048> audio{};
    audio.fill(0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        delay.process(audio, 2U);
    }
    count_allocations.store(false, std::memory_order_relaxed);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "delay processing must not allocate memory");
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
        test_defaults_memory_and_time_relationships();
        test_stereo_time_and_ratio_bounds();
        test_project_authored_bounds_and_non_finite_controls();
        test_mono_echo_feedback_and_dry_path();
        test_stereo_rotation_direction_and_repeat_mapping();
        test_delay_state_is_continuous_across_blocks_and_control_changes();
        test_delay_change_ramps_without_clearing_state();
        test_reset_clears_tails_and_preserves_selected_parameters();
        test_non_finite_audio_is_sanitized_before_feedback();
        test_invalid_channel_layouts_and_process_allocations();
        std::cout << "All Fractal-inspired Surround Delay tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Fractal-inspired Surround Delay test failure: "
                  << error.what() << '\n';
        return 1;
    }
    return 0;
}
