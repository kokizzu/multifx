#include "tabletfx/effects/flanger.hpp"

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

void test_defaults_bounds_and_memory() {
    tabletfx::Flanger flanger;
    const auto defaults = flanger.parameters();
    require(flanger.id() == "flanger", "flanger stable ID changed");
    require(defaults.rate_hz == 0.25F && defaults.base_delay_ms == 2.0F &&
                defaults.depth_ms == 1.0F && defaults.feedback == 0.35F &&
                defaults.feedback_polarity == tabletfx::FlangerFeedbackPolarity::Positive &&
                defaults.mix == 0.35F &&
                defaults.right_channel_phase_degrees == 180.0F &&
                defaults.smoothing_ms == 10.0F,
            "sane flanger defaults changed");
    require(flanger.memory_bytes() == sizeof(flanger) + 1156U * sizeof(float),
            "stereo flanger storage estimate is inaccurate at 48 kHz");

    tabletfx::FlangerParameters parameters;
    parameters.rate_hz = 100.0F;
    parameters.base_delay_ms = 10.0F;
    parameters.depth_ms = 10.0F;
    parameters.feedback = 2.0F;
    parameters.feedback_polarity = static_cast<tabletfx::FlangerFeedbackPolarity>(99);
    parameters.mix = -1.0F;
    parameters.right_channel_phase_degrees = 720.0F;
    parameters.smoothing_ms = 2000.0F;
    flanger.set_parameters(parameters);
    const auto bounds = flanger.parameters();
    require(bounds.rate_hz == 8.0F && bounds.base_delay_ms == 10.0F &&
                bounds.depth_ms == 2.0F && bounds.feedback == 0.9F &&
                bounds.feedback_polarity == tabletfx::FlangerFeedbackPolarity::Positive &&
                bounds.mix == 0.0F &&
                bounds.right_channel_phase_degrees == 360.0F &&
                bounds.smoothing_ms == 1000.0F,
            "flanger bounds or invalid polarity fallback changed");

    parameters = {};
    parameters.rate_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.base_delay_ms = std::numeric_limits<float>::infinity();
    parameters.depth_ms = std::numeric_limits<float>::quiet_NaN();
    parameters.feedback = std::numeric_limits<float>::infinity();
    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    parameters.right_channel_phase_degrees =
        std::numeric_limits<float>::quiet_NaN();
    parameters.smoothing_ms = std::numeric_limits<float>::quiet_NaN();
    flanger.set_parameters(parameters);
    const auto finite = flanger.parameters();
    require(finite.rate_hz == 0.25F && finite.base_delay_ms == 2.0F &&
                finite.depth_ms == 1.0F && finite.feedback == 0.35F &&
                finite.mix == 0.35F &&
                finite.right_channel_phase_degrees == 180.0F &&
                finite.smoothing_ms == 10.0F,
            "non-finite flanger parameters did not use safe defaults");
}

void test_stereo_phase_relationship() {
    tabletfx::FlangerParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.base_delay_ms = 2.0F;
    parameters.depth_ms = 1.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;

    std::vector<float> input(8192U * 2U);
    for (std::size_t frame = 0; frame < input.size() / 2U; ++frame) {
        const float sample = 0.4F * std::sin(
            2.0F * 3.14159265358979323846F * 440.0F *
            static_cast<float>(frame) / 48000.0F);
        input[frame * 2U] = sample;
        input[frame * 2U + 1U] = sample;
    }

    auto in_phase_output = input;
    parameters.right_channel_phase_degrees = 0.0F;
    tabletfx::Flanger in_phase(48000.0F, 2, parameters);
    in_phase.process(in_phase_output, 2);
    for (std::size_t frame = 0; frame < in_phase_output.size() / 2U; ++frame) {
        require(std::abs(in_phase_output[frame * 2U] -
                         in_phase_output[frame * 2U + 1U]) < 1.0e-7F,
                "zero L/R phase did not produce matching stereo modulation");
    }

    auto opposite_phase_output = input;
    parameters.right_channel_phase_degrees = 180.0F;
    tabletfx::Flanger opposite_phase(48000.0F, 2, parameters);
    opposite_phase.process(opposite_phase_output, 2);
    double difference_energy = 0.0;
    for (std::size_t frame = 0; frame < opposite_phase_output.size() / 2U; ++frame) {
        const double difference = opposite_phase_output[frame * 2U] -
                                  opposite_phase_output[frame * 2U + 1U];
        difference_energy += difference * difference;
    }
    require(std::sqrt(difference_energy /
                      static_cast<double>(opposite_phase_output.size() / 2U)) > 0.005,
            "default opposite-phase modulation did not separate stereo channels");
}

void test_delay_feedback_and_polarity() {
    tabletfx::FlangerParameters parameters;
    parameters.base_delay_ms = 8.0F;
    parameters.depth_ms = 0.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::Flanger dry_only(8000.0F, 1, parameters);
    std::array<float, 96> impulse{};
    impulse[0] = 1.0F;
    dry_only.process(impulse, 1);
    require(std::all_of(impulse.begin(), impulse.begin() + 64,
                        [](float value) { return std::abs(value) < 1.0e-7F; }) &&
                std::abs(impulse[64] - 1.0F) < 1.0e-6F,
            "flanger wet impulse missed the configured 64-frame delay");

    for (const auto polarity : {tabletfx::FlangerFeedbackPolarity::Positive,
                                tabletfx::FlangerFeedbackPolarity::Negative}) {
        parameters.feedback = 0.5F;
        parameters.feedback_polarity = polarity;
        tabletfx::Flanger with_feedback(8000.0F, 1, parameters);
        std::array<float, 192> repeats{};
        repeats[0] = 1.0F;
        with_feedback.process(repeats, 1);
        const float expected = polarity == tabletfx::FlangerFeedbackPolarity::Positive
                                   ? 0.5F : -0.5F;
        require(std::abs(repeats[64] - 1.0F) < 1.0e-6F &&
                    std::abs(repeats[128] - expected) < 1.0e-6F,
                "flanger feedback polarity or repeat level is incorrect");
    }
}

void test_stereo_isolation_mix_and_modulation() {
    tabletfx::FlangerParameters parameters;
    parameters.base_delay_ms = 2.0F;
    parameters.depth_ms = 0.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::Flanger stereo(8000.0F, 2, parameters);
    std::array<float, 96> stereo_impulse{};
    stereo_impulse[0] = 1.0F;
    stereo.process(stereo_impulse, 2);
    require(std::abs(stereo_impulse[32] - 1.0F) < 1.0e-6F,
            "stereo flanger left delay is incorrect");
    for (std::size_t i = 1; i < stereo_impulse.size(); i += 2) {
        require(std::abs(stereo_impulse[i]) < 1.0e-7F,
                "stereo flanger leaked the left input into the right channel");
    }

    parameters.mix = 0.0F;
    tabletfx::Flanger dry(8000.0F, 1, parameters);
    std::array<float, 128> dry_input{};
    for (std::size_t i = 0; i < dry_input.size(); ++i) {
        dry_input[i] = static_cast<float>(static_cast<int>(i % 17) - 8) / 8.0F;
    }
    const auto expected_dry = dry_input;
    dry.process(dry_input, 1);
    require(dry_input == expected_dry, "zero-mix flanger changed the dry signal");

    parameters.rate_hz = 1.0F;
    parameters.base_delay_ms = 8.0F;
    parameters.depth_ms = 4.0F;
    parameters.mix = 1.0F;
    tabletfx::Flanger modulated(8000.0F, 1, parameters);
    tabletfx::Flanger fixed(8000.0F, 1, tabletfx::FlangerParameters{
        .rate_hz = 1.0F, .base_delay_ms = 8.0F, .depth_ms = 0.0F,
        .feedback = 0.0F, .feedback_polarity = tabletfx::FlangerFeedbackPolarity::Positive,
        .mix = 1.0F, .smoothing_ms = 0.0F});
    std::vector<float> input(8000);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = 0.5F * std::sin(2.0F * 3.14159265358979323846F *
                                   410.0F * static_cast<float>(i) / 8000.0F);
    }
    auto moving = input;
    auto stationary = input;
    modulated.process(moving, 1);
    fixed.process(stationary, 1);
    double difference_energy = 0.0;
    for (std::size_t i = 128; i < moving.size(); ++i) {
        const double difference = static_cast<double>(moving[i] - stationary[i]);
        difference_energy += difference * difference;
    }
    require(std::sqrt(difference_energy / static_cast<double>(moving.size() - 128)) > 0.01,
            "flanger modulation did not vary the short-delay comb response");

    std::array<float, 4> wrong_layout{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before = wrong_layout;
    stereo.process(wrong_layout, 1);
    require(wrong_layout == before, "flanger modified a mismatched channel layout");
}

void test_smoothing_block_invariance_and_reset() {
    tabletfx::Flanger whole(48000.0F, 2);
    tabletfx::Flanger chunks(48000.0F, 2);
    std::vector<float> input(4096);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = 0.3F * std::sin(2.0F * 3.14159265358979323846F *
                                   330.0F * static_cast<float>(i / 2) / 48000.0F);
    }
    auto a = input;
    auto b = input;
    whole.process(a, 2);
    for (std::size_t offset = 0; offset < b.size();) {
        const std::size_t count = std::min<std::size_t>(318, b.size() - offset);
        chunks.process(std::span<float>(b.data() + offset, count), 2);
        offset += count;
    }
    require(a == b, "flanger output changed with block partitioning");

    tabletfx::FlangerParameters changed;
    changed.rate_hz = 2.0F;
    changed.base_delay_ms = 4.0F;
    changed.depth_ms = 2.0F;
    changed.feedback = 0.7F;
    changed.feedback_polarity = tabletfx::FlangerFeedbackPolarity::Negative;
    changed.mix = 0.8F;
    changed.right_channel_phase_degrees = 45.0F;
    whole.set_parameters(changed);
    chunks.set_parameters(changed);
    std::array<float, 128> change_a{};
    change_a.fill(0.2F);
    auto change_b = change_a;
    whole.process(change_a, 2);
    chunks.process(change_b, 2);
    require(change_a == change_b, "smoothed flanger retarget differs across blocks");

    whole.reset();
    tabletfx::Flanger fresh(48000.0F, 2, changed);
    std::array<float, 256> after_reset{};
    after_reset[0] = 1.0F;
    auto from_fresh = after_reset;
    whole.process(after_reset, 2);
    fresh.process(from_fresh, 2);
    require(after_reset == from_fresh, "flanger reset does not match fresh state");
}

void test_invalid_samples_and_no_process_allocations() {
    tabletfx::Flanger mono(48000.0F, 1);
    std::array<float, 5> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.25F, -0.5F};
    mono.process(invalid, 1);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F && invalid[2] == 0.0F,
            "flanger did not replace non-finite input with silence");

    std::array<float, 4> no_channels{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before = no_channels;
    mono.process(no_channels, 0);
    require(no_channels == before, "zero-channel process should be a no-op");

    std::array<float, 512> block{};
    block.fill(0.25F);
    tabletfx::Flanger stereo;
    const std::size_t before_allocations = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) stereo.process(block, 2);
    const std::size_t after_allocations = allocation_count.load(std::memory_order_relaxed);
    require(before_allocations == after_allocations, "flanger allocated during process()");
    require(std::all_of(block.begin(), block.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "flanger emitted a non-finite sample");
}

void test_invalid_construction() {
    bool bad_rate = false;
    try { tabletfx::Flanger invalid(100.0F, 1); }
    catch (const std::invalid_argument&) { bad_rate = true; }
    require(bad_rate, "flanger accepted an unsupported sample rate");

    bool bad_channels = false;
    try { tabletfx::Flanger invalid(48000.0F, 3); }
    catch (const std::invalid_argument&) { bad_channels = true; }
    require(bad_channels, "flanger accepted an unsupported channel count");
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
        test_defaults_bounds_and_memory();
        test_stereo_phase_relationship();
        test_delay_feedback_and_polarity();
        test_stereo_isolation_mix_and_modulation();
        test_smoothing_block_invariance_and_reset();
        test_invalid_samples_and_no_process_allocations();
        test_invalid_construction();
        std::cout << "All TabletFX flanger tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
