#include "tabletfx/effects/manual_thru_zero_flanger.hpp"

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

void test_documented_controls_and_project_bounds() {
    tabletfx::ManualThruZeroFlanger flanger;
    const auto defaults = flanger.parameters();
    require(flanger.id() == "manual-thru-zero-flanger",
            "manual through-zero flanger ID changed");
    require(defaults.minimum_time_ms == 0.34F && defaults.maximum_time_ms == 8.0F &&
                defaults.manual == 5.0F && defaults.dry_delay_position == 0.5F &&
                defaults.feedback_percent == 0.0F &&
                defaults.phase_reverse == tabletfx::FlangerPhaseReverse::Both &&
                defaults.mix == 0.5F && defaults.smoothing_ms == 10.0F,
            "project-authored manual through-zero defaults changed");
    require(flanger.current_wet_time_ms() == flanger.current_dry_time_ms(),
            "default Manual and Dry Delay positions should meet at the zero point");
    require(flanger.maximum_path_latency_frames() == 384U,
            "reported 8 ms maximum path delay is inaccurate at 48 kHz");

    tabletfx::ManualThruZeroFlangerParameters parameters;
    parameters.minimum_time_ms = -1.0F;
    parameters.maximum_time_ms = 200.0F;
    parameters.manual = 100.0F;
    parameters.dry_delay_position = -2.0F;
    parameters.feedback_percent = 200.0F;
    parameters.phase_reverse = static_cast<tabletfx::FlangerPhaseReverse>(99);
    parameters.mix = -1.0F;
    parameters.smoothing_ms = 2000.0F;
    flanger.set_parameters(parameters);
    const auto bounded = flanger.parameters();
    require(bounded.minimum_time_ms == 0.34F && bounded.maximum_time_ms == 20.0F &&
                bounded.manual == 10.0F && bounded.dry_delay_position == 0.0F &&
                bounded.feedback_percent == 100.0F &&
                bounded.phase_reverse == tabletfx::FlangerPhaseReverse::Both &&
                bounded.mix == 0.0F && bounded.smoothing_ms == 1000.0F,
            "manual through-zero parameter bounds or invalid enum fallback changed");

    parameters = {};
    parameters.minimum_time_ms = 19.999F;
    parameters.maximum_time_ms = 0.34F;
    parameters.manual = std::numeric_limits<float>::quiet_NaN();
    parameters.dry_delay_position = std::numeric_limits<float>::infinity();
    parameters.feedback_percent = -std::numeric_limits<float>::infinity();
    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    parameters.smoothing_ms = std::numeric_limits<float>::quiet_NaN();
    flanger.set_parameters(parameters);
    const auto finite = flanger.parameters();
    require(finite.minimum_time_ms == 19.99F && finite.maximum_time_ms == 20.0F &&
                finite.manual == 5.0F && finite.dry_delay_position == 0.5F &&
                finite.feedback_percent == 0.0F && finite.mix == 0.5F &&
                finite.smoothing_ms == 10.0F,
            "invalid through-zero parameters did not fall back to bounded values");
}

void test_manual_sweep_crosses_delayed_dry_path() {
    tabletfx::ManualThruZeroFlangerParameters parameters;
    parameters.minimum_time_ms = 2.0F;
    parameters.maximum_time_ms = 6.0F;
    parameters.manual = 0.0F;
    parameters.dry_delay_position = 0.5F;
    parameters.feedback_percent = 0.0F;
    parameters.phase_reverse = tabletfx::FlangerPhaseReverse::Both;
    parameters.mix = 0.5F;
    parameters.smoothing_ms = 0.0F;

    tabletfx::ManualThruZeroFlanger wet_leads(8000.0F, 1, parameters);
    std::array<float, 96> lead_impulse{};
    lead_impulse[0] = 1.0F;
    wet_leads.process(lead_impulse, 1);
    require(std::abs(lead_impulse[16] + 0.5F) < 1.0e-6F &&
                std::abs(lead_impulse[32] - 0.5F) < 1.0e-6F,
            "Manual endpoint should put the wet tap before the delayed dry tap");

    parameters.manual = 10.0F;
    tabletfx::ManualThruZeroFlanger wet_trails(8000.0F, 1, parameters);
    std::array<float, 96> trail_impulse{};
    trail_impulse[0] = 1.0F;
    wet_trails.process(trail_impulse, 1);
    require(std::abs(trail_impulse[32] - 0.5F) < 1.0e-6F &&
                std::abs(trail_impulse[48] + 0.5F) < 1.0e-6F,
            "Manual endpoint should put the wet tap after the delayed dry tap");
}

void test_zero_point_cancellation_and_phase_choices() {
    tabletfx::ManualThruZeroFlangerParameters parameters;
    parameters.minimum_time_ms = 2.0F;
    parameters.maximum_time_ms = 6.0F;
    parameters.manual = 5.0F;
    parameters.dry_delay_position = 0.5F;
    parameters.feedback_percent = 0.0F;
    parameters.phase_reverse = tabletfx::FlangerPhaseReverse::Both;
    parameters.mix = 0.5F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::ManualThruZeroFlanger cancelling(8000.0F, 1, parameters);
    std::array<float, 256> steady{};
    steady.fill(0.25F);
    cancelling.process(steady, 1);
    require(std::all_of(steady.begin() + 64, steady.end(),
                        [](float value) { return std::abs(value) < 1.0e-7F; }),
            "matched delayed paths with opposite phase should cancel at the zero point");

    parameters.phase_reverse = tabletfx::FlangerPhaseReverse::None;
    tabletfx::ManualThruZeroFlanger reinforcing(8000.0F, 1, parameters);
    steady.fill(0.25F);
    reinforcing.process(steady, 1);
    require(std::all_of(steady.begin() + 64, steady.end(),
                        [](float value) { return std::abs(value - 0.25F) < 1.0e-7F; }),
            "matched delayed paths without phase reversal should reinforce");
}

void test_feedback_is_bounded_and_polarity_controlled() {
    for (const float polarity : {100.0F, -100.0F}) {
        tabletfx::ManualThruZeroFlangerParameters parameters;
        parameters.minimum_time_ms = 2.0F;
        parameters.maximum_time_ms = 6.0F;
        parameters.manual = 0.0F;
        parameters.dry_delay_position = 0.5F;
        parameters.feedback_percent = polarity;
        parameters.phase_reverse = tabletfx::FlangerPhaseReverse::None;
        parameters.mix = 1.0F;
        parameters.smoothing_ms = 0.0F;
        tabletfx::ManualThruZeroFlanger flanger(8000.0F, 1, parameters);
        std::array<float, 80> impulse{};
        impulse[0] = 1.0F;
        flanger.process(impulse, 1);
        const float expected_repeat = polarity > 0.0F ? 0.9F : -0.9F;
        require(std::abs(impulse[16] - 1.0F) < 1.0e-6F &&
                    std::abs(impulse[32] - expected_repeat) < 1.0e-6F,
                "feedback polarity or project stability cap changed");
    }
}

void test_stereo_isolation_smoothing_and_reset() {
    tabletfx::ManualThruZeroFlanger whole(48000.0F, 2);
    tabletfx::ManualThruZeroFlanger chunks(48000.0F, 2);
    std::vector<float> input(8192);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = 0.3F * std::sin(2.0F * 3.14159265358979323846F *
                                   330.0F * static_cast<float>(i / 2) / 48000.0F);
    }
    auto whole_output = input;
    auto chunks_output = input;
    whole.process(whole_output, 2);
    for (std::size_t offset = 0; offset < chunks_output.size();) {
        const std::size_t count = std::min<std::size_t>(318, chunks_output.size() - offset);
        chunks.process(std::span<float>(chunks_output.data() + offset, count), 2);
        offset += count;
    }
    require(whole_output == chunks_output,
            "manual through-zero output changed with block partitioning");

    tabletfx::ManualThruZeroFlangerParameters changed;
    changed.minimum_time_ms = 1.0F;
    changed.maximum_time_ms = 12.0F;
    changed.manual = 2.0F;
    changed.dry_delay_position = 0.8F;
    changed.feedback_percent = -55.0F;
    changed.phase_reverse = tabletfx::FlangerPhaseReverse::Right;
    changed.mix = 0.8F;
    whole.set_parameters(changed);
    chunks.set_parameters(changed);
    std::array<float, 128> change_a{};
    change_a.fill(0.2F);
    auto change_b = change_a;
    whole.process(change_a, 2);
    chunks.process(change_b, 2);
    require(change_a == change_b,
            "smoothed parameter changes differ across block partitions");

    whole.reset();
    tabletfx::ManualThruZeroFlanger fresh(48000.0F, 2, changed);
    std::array<float, 512> after_reset{};
    after_reset[0] = 1.0F;
    auto from_fresh = after_reset;
    whole.process(after_reset, 2);
    fresh.process(from_fresh, 2);
    require(after_reset == from_fresh,
            "reset manual through-zero state differs from a fresh instance");

    tabletfx::ManualThruZeroFlanger stereo(8000.0F, 2);
    std::array<float, 96> left_only{};
    left_only[0] = 1.0F;
    stereo.process(left_only, 2);
    for (std::size_t i = 1; i < left_only.size(); i += 2) {
        require(std::abs(left_only[i]) < 1.0e-7F,
                "manual through-zero left input leaked to the right channel");
    }
}

void test_finite_recovery_layout_and_no_process_allocations() {
    tabletfx::ManualThruZeroFlanger mono(48000.0F, 1);
    std::array<float, 5> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.25F, -0.5F};
    mono.process(invalid, 1);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "manual through-zero emitted non-finite output");

    std::array<float, 4> wrong_layout{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before = wrong_layout;
    mono.process(wrong_layout, 2);
    require(wrong_layout == before,
            "manual through-zero modified a mismatched channel layout");

    std::array<float, 512> block{};
    block.fill(0.25F);
    const std::size_t before_allocations = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        mono.process(block, 1);
    }
    const std::size_t after_allocations = allocation_count.load(std::memory_order_relaxed);
    require(before_allocations == after_allocations,
            "manual through-zero process() allocated memory");
}

void test_invalid_construction() {
    bool bad_rate = false;
    try { tabletfx::ManualThruZeroFlanger invalid(100.0F, 1); }
    catch (const std::invalid_argument&) { bad_rate = true; }
    require(bad_rate, "manual through-zero accepted an unsupported sample rate");

    bool bad_channels = false;
    try { tabletfx::ManualThruZeroFlanger invalid(48000.0F, 3); }
    catch (const std::invalid_argument&) { bad_channels = true; }
    require(bad_channels, "manual through-zero accepted an unsupported channel count");
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
        test_documented_controls_and_project_bounds();
        test_manual_sweep_crosses_delayed_dry_path();
        test_zero_point_cancellation_and_phase_choices();
        test_feedback_is_bounded_and_polarity_controlled();
        test_stereo_isolation_smoothing_and_reset();
        test_finite_recovery_layout_and_no_process_allocations();
        test_invalid_construction();
        std::cout << "All TabletFX manual through-zero flanger tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
