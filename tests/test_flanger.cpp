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

tabletfx::FlangerParameters filter_mode_parameters() {
    tabletfx::FlangerParameters parameters;
    parameters.mode = tabletfx::FlangerMode::Filter;
    parameters.rate_hz = 0.05F;
    parameters.tempo_sync = true;
    parameters.tempo_bpm = 300.0F;
    parameters.tap_division = tabletfx::DelayDivision::whole;
    parameters.base_delay_ms = 10.0F;
    parameters.depth_ms = 4.5F;
    parameters.feedback = 0.9F;
    parameters.feedback_polarity = tabletfx::FlangerFeedbackPolarity::Negative;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    parameters.color = 0.0F;
    return parameters;
}

void test_defaults_bounds_and_memory() {
    tabletfx::Flanger flanger;
    const auto defaults = flanger.parameters();
    require(flanger.id() == "flanger", "flanger stable ID changed");
    require(defaults.mode == tabletfx::FlangerMode::Flanger &&
                defaults.color == 0.35F,
            "default Flanger mode or project Color default changed");
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
    parameters.color = 3.0F;
    flanger.set_parameters(parameters);
    const auto bounds = flanger.parameters();
    require(bounds.rate_hz == 8.0F && bounds.base_delay_ms == 10.0F &&
                bounds.depth_ms == 2.0F && bounds.feedback == 0.9F &&
                bounds.feedback_polarity == tabletfx::FlangerFeedbackPolarity::Positive &&
                bounds.mix == 0.0F &&
                bounds.right_channel_phase_degrees == 360.0F &&
                bounds.smoothing_ms == 1000.0F && bounds.color == 0.9F,
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
    parameters.color = std::numeric_limits<float>::quiet_NaN();
    parameters.mode = static_cast<tabletfx::FlangerMode>(99);
    flanger.set_parameters(parameters);
    const auto finite = flanger.parameters();
    require(finite.rate_hz == 0.25F && finite.base_delay_ms == 2.0F &&
                finite.depth_ms == 1.0F && finite.feedback == 0.35F &&
                finite.mix == 0.35F &&
                finite.right_channel_phase_degrees == 180.0F &&
                finite.smoothing_ms == 10.0F &&
                finite.mode == tabletfx::FlangerMode::Flanger &&
                finite.color == 0.35F,
            "non-finite flanger parameters did not use safe defaults");
}

void test_filter_mode_legacy_identity_and_shared_memory() {
    tabletfx::Flanger defaults(48000.0F, 2);
    tabletfx::FlangerParameters explicit_parameters;
    explicit_parameters.mode = tabletfx::FlangerMode::Flanger;
    tabletfx::Flanger explicit_flanger(48000.0F, 2, explicit_parameters);
    require(defaults.memory_bytes() == explicit_flanger.memory_bytes(),
            "Flanger mode allocated storage beyond the existing delay ring");

    std::vector<float> input(8192U);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = 0.3F * std::sin(2.0F * 3.14159265358979323846F *
                                   311.0F * static_cast<float>(i / 2U) / 48000.0F);
    }
    auto default_output = input;
    auto explicit_output = input;
    defaults.process(default_output, 2);
    explicit_flanger.process(explicit_output, 2);
    require(default_output == explicit_output,
            "default mode no longer matches explicit legacy Flanger output");

    tabletfx::FlangerParameters filter_parameters = filter_mode_parameters();
    tabletfx::Flanger filter(48000.0F, 2, filter_parameters);
    require(filter.memory_bytes() == defaults.memory_bytes(),
            "Filter mode did not reuse the existing delay ring");
}

void test_filter_mode_rate_tunes_fixed_comb_and_ignores_sync() {
    auto low_rate_parameters = filter_mode_parameters();
    low_rate_parameters.rate_hz = 0.05F;
    tabletfx::Flanger low_rate(8000.0F, 1, low_rate_parameters);
    std::array<float, 128> low_impulse{};
    low_impulse[0] = 1.0F;
    low_rate.process(low_impulse, 1);
    require(std::abs(low_impulse[4] - 1.0F) < 1.0e-6F,
            "Filter-mode minimum Rate did not select the project 0.5 ms comb delay");
    for (std::size_t i = 0; i < low_impulse.size(); ++i) {
        const float expected = i == 4U ? 1.0F : 0.0F;
        require(std::abs(low_impulse[i] - expected) < 1.0e-6F,
                "Filter mode unexpectedly swept its fixed comb delay");
    }

    auto high_rate_parameters = filter_mode_parameters();
    high_rate_parameters.rate_hz = 8.0F;
    tabletfx::Flanger high_rate(8000.0F, 1, high_rate_parameters);
    std::array<float, 128> high_impulse{};
    high_impulse[0] = 1.0F;
    high_rate.process(high_impulse, 1);
    require(std::abs(high_impulse[80] - 1.0F) < 1.0e-6F,
            "Filter-mode maximum Rate did not select the project 10 ms comb delay");

    auto unsynced_parameters = filter_mode_parameters();
    unsynced_parameters.rate_hz = 1.25F;
    unsynced_parameters.tempo_sync = false;
    unsynced_parameters.tap_division = tabletfx::DelayDivision::quarter;
    tabletfx::Flanger unsynced(8000.0F, 1, unsynced_parameters);
    auto other_sync_parameters = unsynced_parameters;
    other_sync_parameters.tempo_sync = true;
    other_sync_parameters.tempo_bpm = 37.0F;
    other_sync_parameters.tap_division = tabletfx::DelayDivision::sixty_fourth;
    tabletfx::Flanger other_sync(8000.0F, 1, other_sync_parameters);
    std::array<float, 512> sync_a{};
    sync_a[0] = 1.0F;
    auto sync_b = sync_a;
    unsynced.process(sync_a, 1);
    other_sync.process(sync_b, 1);
    require(sync_a == sync_b,
            "Filter mode used tempo sync or Tap Division for its static Rate mapping");

    auto varied_modulation = unsynced_parameters;
    varied_modulation.depth_ms = 0.0F;
    tabletfx::Flanger no_modulation_depth(8000.0F, 1, varied_modulation);
    auto high_modulation_depth = unsynced_parameters;
    high_modulation_depth.depth_ms = 4.5F;
    tabletfx::Flanger ignored_modulation_depth(8000.0F, 1,
                                              high_modulation_depth);
    std::array<float, 512> depth_a{};
    depth_a[0] = 1.0F;
    auto depth_b = depth_a;
    no_modulation_depth.process(depth_a, 1);
    ignored_modulation_depth.process(depth_b, 1);
    require(depth_a == depth_b,
            "Filter mode used modulation depth despite its fixed comb response");
}

void test_filter_mode_depth_color_and_stability() {
    auto dry_parameters = filter_mode_parameters();
    dry_parameters.mix = 0.0F;
    tabletfx::Flanger dry(8000.0F, 1, dry_parameters);
    std::array<float, 128> dry_input{};
    dry_input[0] = 1.0F;
    dry_input[19] = -0.25F;
    const auto expected_dry = dry_input;
    dry.process(dry_input, 1);
    require(dry_input == expected_dry,
            "Filter-mode project wet mix at zero did not preserve the dry signal");

    auto neutral_color = filter_mode_parameters();
    neutral_color.mix = 1.0F;
    neutral_color.color = 0.0F;
    tabletfx::Flanger no_color(8000.0F, 1, neutral_color);
    auto colored = neutral_color;
    colored.color = 0.7F;
    tabletfx::Flanger color(8000.0F, 1, colored);
    std::array<float, 128> no_color_impulse{};
    no_color_impulse[0] = 1.0F;
    auto color_impulse = no_color_impulse;
    no_color.process(no_color_impulse, 1);
    color.process(color_impulse, 1);
    require(std::abs(no_color_impulse[8]) < 1.0e-7F &&
                std::abs(color_impulse[8] - 0.7F) < 1.0e-6F,
            "Filter-mode Color did not control bounded comb feedback intensity");

    auto resonant_parameters = filter_mode_parameters();
    resonant_parameters.rate_hz = 0.05F;
    resonant_parameters.color = 0.9F;
    std::vector<float> resonant_input(32000U);
    for (std::size_t i = 0; i < resonant_input.size(); ++i) {
        resonant_input[i] = 0.1F * std::sin(
            2.0F * 3.14159265358979323846F * 2000.0F *
            static_cast<float>(i) / 8000.0F);
    }
    tabletfx::Flanger resonant(8000.0F, 1, resonant_parameters);
    resonant.process(resonant_input, 1);
    require(std::all_of(resonant_input.begin(), resonant_input.end(),
                        [](float sample) {
                            return std::isfinite(sample) && std::abs(sample) < 2.0F;
                        }),
            "Filter-mode high Color became unstable or emitted non-finite audio");
}

void test_filter_mode_stereo_isolation_and_block_continuity() {
    auto parameters = filter_mode_parameters();
    parameters.rate_hz = 0.75F;
    parameters.mix = 0.8F;
    parameters.color = 0.6F;
    tabletfx::Flanger stereo(8000.0F, 2, parameters);
    std::array<float, 1024> stereo_impulse{};
    stereo_impulse[0] = 1.0F;
    stereo.process(stereo_impulse, 2);
    for (std::size_t i = 1; i < stereo_impulse.size(); i += 2U) {
        require(std::abs(stereo_impulse[i]) < 1.0e-7F,
                "Filter-mode feedback leaked the left channel into the right");
    }

    std::vector<float> source(4096U);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = 0.2F * std::sin(2.0F * 3.14159265358979323846F *
                                    337.0F * static_cast<float>(i / 2U) /
                                    48000.0F);
    }
    auto whole_output = source;
    auto partitioned_output = source;
    tabletfx::Flanger whole(48000.0F, 2, parameters);
    tabletfx::Flanger partitioned(48000.0F, 2, parameters);
    whole.process(whole_output, 2);
    std::size_t offset = 0U;
    for (const std::size_t block_frames : {1U, 17U, 128U, 257U, 64U, 511U}) {
        const std::size_t remaining_frames =
            partitioned_output.size() / 2U - offset;
        const std::size_t frames = std::min(block_frames, remaining_frames);
        partitioned.process(
            std::span<float>(partitioned_output.data() + offset * 2U,
                             frames * 2U),
            2);
        offset += frames;
        if (offset == partitioned_output.size() / 2U) break;
    }
    if (offset < partitioned_output.size() / 2U) {
        partitioned.process(
            std::span<float>(partitioned_output.data() + offset * 2U,
                             partitioned_output.size() - offset * 2U),
            2);
    }
    require(whole_output == partitioned_output,
            "Filter-mode fixed-comb output depends on block partitioning");
}

void test_tempo_sync_divisions() {
    constexpr std::array<float, 14> expected_rates{
        0.5F, 2.0F / 3.0F, 1.0F, 4.0F / 3.0F, 2.0F, 3.0F,
        8.0F / 3.0F, 4.0F, 6.0F, 16.0F / 3.0F, 8.0F, 12.0F,
        16.0F, 32.0F};
    for (std::size_t index = 0; index < expected_rates.size(); ++index) {
        tabletfx::FlangerParameters parameters;
        parameters.tempo_sync = true;
        parameters.tempo_bpm = 120.0F;
        parameters.tap_division = static_cast<tabletfx::DelayDivision>(index);
        tabletfx::Flanger flanger(48000.0F, 1, parameters);
        require(std::abs(flanger.effective_rate_hz() - expected_rates[index]) < 1.0e-5F,
                "tempo-synced Flanger division has the wrong rate");
    }

    tabletfx::FlangerParameters free_rate;
    free_rate.rate_hz = 0.75F;
    free_rate.tempo_bpm = 240.0F;
    free_rate.tap_division = tabletfx::DelayDivision::sixteenth;
    tabletfx::Flanger unsynced(48000.0F, 1, free_rate);
    require(std::abs(unsynced.effective_rate_hz() - 0.75F) < 1.0e-6F,
            "free Flanger rate changed when tempo sync is disabled");

    free_rate.tempo_sync = true;
    free_rate.tempo_bpm = std::numeric_limits<float>::quiet_NaN();
    free_rate.tap_division = static_cast<tabletfx::DelayDivision>(255);
    tabletfx::Flanger invalid(48000.0F, 1, free_rate);
    require(invalid.parameters().tempo_bpm == 120.0F &&
                invalid.parameters().tap_division == tabletfx::DelayDivision::quarter &&
                std::abs(invalid.effective_rate_hz() - 2.0F) < 1.0e-6F,
            "invalid Flanger tempo controls did not use safe defaults");
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

    auto filter_parameters = filter_mode_parameters();
    filter_parameters.color = 0.7F;
    tabletfx::Flanger filter(48000.0F, 2, filter_parameters);
    std::array<float, 512> filter_block{};
    filter_block.fill(0.25F);
    const std::size_t before_filter_allocations =
        allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        filter.process(filter_block, 2);
    }
    const std::size_t after_filter_allocations =
        allocation_count.load(std::memory_order_relaxed);
    require(before_filter_allocations == after_filter_allocations,
            "Filter mode allocated during process()");
    require(std::all_of(filter_block.begin(), filter_block.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "Filter mode emitted a non-finite sample");
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
        test_filter_mode_legacy_identity_and_shared_memory();
        test_filter_mode_rate_tunes_fixed_comb_and_ignores_sync();
        test_filter_mode_depth_color_and_stability();
        test_filter_mode_stereo_isolation_and_block_continuity();
        test_tempo_sync_divisions();
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
