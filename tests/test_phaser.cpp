#include "tabletfx/effects/phaser.hpp"

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

std::vector<float> make_tone(float frequency_hz, std::size_t frames,
                             std::uint32_t sample_rate = 48000) {
    std::vector<float> samples(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        samples[frame] = 0.5F * std::sin(
            2.0F * 3.14159265358979323846F * frequency_hz *
            static_cast<float>(frame) / static_cast<float>(sample_rate));
    }
    return samples;
}

float rms(std::span<const float> samples) {
    double energy = 0.0;
    for (float sample : samples) energy += static_cast<double>(sample) * sample;
    return static_cast<float>(std::sqrt(energy / static_cast<double>(samples.size())));
}

void test_defaults_bounds_and_stage_choices() {
    tabletfx::Phaser phaser;
    const auto defaults = phaser.parameters();
    require(phaser.id() == "phaser", "phaser stable ID changed");
    require(defaults.rate_hz == 0.5F && !defaults.tempo_sync &&
                defaults.tempo_bpm == 120.0F &&
                defaults.tap_division == tabletfx::DelayDivision::quarter &&
                defaults.minimum_hz == 250.0F &&
                defaults.maximum_hz == 1600.0F && defaults.feedback == 0.2F &&
                defaults.stages == 4 && defaults.mix == 0.5F &&
                defaults.smoothing_ms == 10.0F,
            "sane phaser defaults changed");
    require(phaser.memory_bytes() == sizeof(phaser),
            "phaser fixed-state memory estimate is inaccurate");

    tabletfx::PhaserParameters parameters;
    parameters.rate_hz = 20.0F;
    parameters.minimum_hz = 1800.0F;
    parameters.maximum_hz = 800.0F;
    parameters.feedback = 2.0F;
    parameters.stages = 7;
    parameters.mix = -1.0F;
    parameters.smoothing_ms = 2000.0F;
    phaser.set_parameters(parameters);
    const auto bounds = phaser.parameters();
    require(bounds.rate_hz == 8.0F && bounds.minimum_hz == 800.0F &&
                bounds.maximum_hz == 1800.0F && bounds.feedback == 0.7F &&
                bounds.stages == 4 && bounds.mix == 0.0F &&
                bounds.smoothing_ms == 1000.0F,
            "phaser parameter bounds or stage fallback changed");

    parameters.stages = 6;
    parameters.feedback = -0.7F;
    phaser.set_parameters(parameters);
    require(phaser.parameters().stages == 6 && phaser.parameters().feedback == -0.7F,
            "phaser rejected a supported stage count or negative feedback");
    parameters.stages = 8;
    phaser.set_parameters(parameters);
    require(phaser.parameters().stages == 8, "phaser rejected eight stages");

    parameters = {};
    parameters.rate_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.minimum_hz = std::numeric_limits<float>::infinity();
    parameters.maximum_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.feedback = std::numeric_limits<float>::quiet_NaN();
    parameters.mix = std::numeric_limits<float>::infinity();
    parameters.smoothing_ms = std::numeric_limits<float>::quiet_NaN();
    phaser.set_parameters(parameters);
    const auto finite = phaser.parameters();
    require(finite.rate_hz == 0.5F && finite.minimum_hz == 250.0F &&
                finite.maximum_hz == 1600.0F && finite.feedback == 0.2F &&
                finite.mix == 0.5F && finite.smoothing_ms == 10.0F,
            "non-finite phaser parameters did not use safe defaults");
}

void test_tempo_sync_divisions() {
    constexpr std::array<float, 14> expected_rates{
        0.5F, 2.0F / 3.0F, 1.0F, 4.0F / 3.0F, 2.0F, 3.0F,
        8.0F / 3.0F, 4.0F, 6.0F, 16.0F / 3.0F, 8.0F, 12.0F,
        16.0F, 32.0F,
    };
    tabletfx::PhaserParameters parameters;
    parameters.tempo_sync = true;
    parameters.tempo_bpm = 120.0F;
    tabletfx::Phaser phaser(48000.0F, 1, parameters);
    for (std::size_t index = 0; index < expected_rates.size(); ++index) {
        parameters.tap_division = static_cast<tabletfx::DelayDivision>(index);
        phaser.set_parameters(parameters);
        require(std::abs(phaser.effective_rate_hz() - expected_rates[index]) < 1.0e-5F,
                "Phaser Tap Division does not map to the expected 120 BPM rate");
    }

    parameters.tempo_bpm = 300.0F;
    parameters.tap_division = tabletfx::DelayDivision::sixty_fourth;
    phaser.set_parameters(parameters);
    require(phaser.effective_rate_hz() == 80.0F,
            "Phaser tempo-synced LFO exceeded its project safety cap");

    parameters.tempo_bpm = std::numeric_limits<float>::quiet_NaN();
    parameters.tap_division = static_cast<tabletfx::DelayDivision>(255);
    phaser.set_parameters(parameters);
    require(phaser.parameters().tempo_bpm == 120.0F &&
                phaser.parameters().tap_division == tabletfx::DelayDivision::quarter &&
                phaser.effective_rate_hz() == 2.0F,
            "Phaser tempo controls did not use safe invalid-value fallbacks");

    parameters.tempo_sync = false;
    parameters.rate_hz = 0.5F;
    phaser.set_parameters(parameters);
    require(phaser.effective_rate_hz() == 0.5F,
            "Phaser free-rate mode changed when tempo sync is disabled");
}

void test_notch_and_selectable_stages() {
    tabletfx::PhaserParameters parameters;
    parameters.rate_hz = 0.5F;
    parameters.minimum_hz = 1000.0F;
    parameters.maximum_hz = 1000.0F;
    parameters.feedback = 0.0F;
    parameters.stages = 4;
    parameters.mix = 0.5F;
    parameters.smoothing_ms = 0.0F;

    auto four_stage_tone = make_tone(500.0F, 8192);
    const auto dry_reference = make_tone(500.0F, 7168);
    tabletfx::Phaser four_stage(48000.0F, 1, parameters);
    four_stage.process(four_stage_tone, 1);
    const float output_ratio = rms(std::span<const float>(four_stage_tone).subspan(1024)) /
                               rms(dry_reference);
    require(output_ratio < 0.4F,
            "four-stage phaser did not create a dry/wet cancellation notch");

    std::array<float, 1024> four{}, six{}, eight{};
    four[0] = six[0] = eight[0] = 1.0F;
    tabletfx::Phaser four_effect(48000.0F, 1, parameters);
    parameters.stages = 6;
    tabletfx::Phaser six_effect(48000.0F, 1, parameters);
    parameters.stages = 8;
    tabletfx::Phaser eight_effect(48000.0F, 1, parameters);
    four_effect.process(four, 1);
    six_effect.process(six, 1);
    eight_effect.process(eight, 1);
    require(four != six && six != eight && four != eight,
            "selectable 4/6/8-stage phasers did not produce distinct responses");

    parameters.stages = 8;
    parameters.mix = 1.0F;
    tabletfx::Phaser switching(48000.0F, 1, parameters);
    std::array<float, 1024> prior_tail{};
    prior_tail[0] = 1.0F;
    switching.process(prior_tail, 1);
    parameters.stages = 4;
    switching.set_parameters(parameters);
    std::array<float, 128> cleared_tail{};
    switching.process(cleared_tail, 1);
    require(std::all_of(cleared_tail.begin(), cleared_tail.end(),
                        [](float sample) { return sample == 0.0F; }),
            "phaser stage-count change retained stale all-pass history");
}

void test_dry_mix_stereo_and_modulation() {
    tabletfx::PhaserParameters parameters;
    parameters.minimum_hz = 500.0F;
    parameters.maximum_hz = 1800.0F;
    parameters.feedback = 0.4F;
    parameters.mix = 0.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::Phaser dry(48000.0F, 1, parameters);
    std::array<float, 256> dry_signal{};
    for (std::size_t i = 0; i < dry_signal.size(); ++i) {
        dry_signal[i] = static_cast<float>(static_cast<int>(i % 23) - 11) / 11.0F;
    }
    const auto expected_dry = dry_signal;
    dry.process(dry_signal, 1);
    require(dry_signal == expected_dry, "zero-mix phaser changed the dry signal");

    parameters.mix = 1.0F;
    tabletfx::Phaser stereo(48000.0F, 2, parameters);
    std::array<float, 512> stereo_impulse{};
    stereo_impulse[0] = 1.0F;
    stereo.process(stereo_impulse, 2);
    for (std::size_t i = 1; i < stereo_impulse.size(); i += 2) {
        require(std::abs(stereo_impulse[i]) < 1.0e-7F,
                "stereo phaser leaked the left input into the right channel");
    }

    parameters.rate_hz = 1.0F;
    parameters.minimum_hz = 250.0F;
    parameters.maximum_hz = 1600.0F;
    tabletfx::Phaser moving(48000.0F, 1, parameters);
    parameters.minimum_hz = 900.0F;
    parameters.maximum_hz = 900.0F;
    tabletfx::Phaser fixed(48000.0F, 1, parameters);
    auto input = make_tone(500.0F, 12000);
    auto modulated = input;
    auto stationary = input;
    moving.process(modulated, 1);
    fixed.process(stationary, 1);
    double difference_energy = 0.0;
    for (std::size_t i = 256; i < modulated.size(); ++i) {
        const double difference = static_cast<double>(modulated[i] - stationary[i]);
        difference_energy += difference * difference;
    }
    require(std::sqrt(difference_energy / static_cast<double>(modulated.size() - 256)) > 0.01,
            "phaser LFO did not move its notch response");
}

void test_smoothing_blocks_reset_and_feedback_stability() {
    tabletfx::Phaser whole(48000.0F, 2);
    tabletfx::Phaser chunks(48000.0F, 2);
    std::vector<float> input(4096);
    for (std::size_t frame = 0; frame < input.size() / 2; ++frame) {
        const float value = 0.3F * std::sin(
            2.0F * 3.14159265358979323846F * 330.0F *
            static_cast<float>(frame) / 48000.0F);
        input[frame * 2] = value;
        input[frame * 2 + 1] = -value * 0.5F;
    }
    auto a = input;
    auto b = input;
    whole.process(a, 2);
    for (std::size_t offset = 0; offset < b.size();) {
        const std::size_t count = std::min<std::size_t>(318, b.size() - offset);
        chunks.process(std::span<float>(b.data() + offset, count), 2);
        offset += count;
    }
    require(a == b, "phaser output changed with block partitioning");

    tabletfx::PhaserParameters changed;
    changed.rate_hz = 2.0F;
    changed.minimum_hz = 350.0F;
    changed.maximum_hz = 2400.0F;
    changed.feedback = -0.65F;
    changed.stages = 8;
    changed.mix = 0.8F;
    whole.set_parameters(changed);
    chunks.set_parameters(changed);
    std::array<float, 256> changed_a{};
    changed_a.fill(0.2F);
    auto changed_b = changed_a;
    whole.process(changed_a, 2);
    chunks.process(changed_b, 2);
    require(changed_a == changed_b, "smoothed phaser retarget differs across blocks");

    std::array<float, 512> impulse{};
    impulse[0] = 1.0F;
    auto reset_output = impulse;
    whole.reset();
    tabletfx::Phaser fresh(48000.0F, 2, changed);
    auto fresh_output = impulse;
    whole.process(reset_output, 2);
    fresh.process(fresh_output, 2);
    require(reset_output == fresh_output, "phaser reset does not match fresh state");

    changed.feedback = 0.7F;
    tabletfx::Phaser bounded_feedback(48000.0F, 1, changed);
    std::vector<float> feedback_impulse(48000, 0.0F);
    feedback_impulse[0] = 1.0F;
    bounded_feedback.process(feedback_impulse, 1);
    require(std::all_of(feedback_impulse.begin(), feedback_impulse.end(),
                        [](float sample) { return std::isfinite(sample) &&
                                                   std::abs(sample) < 8.0F; }),
            "bounded phaser feedback became unstable");
}

void test_no_allocations_invalid_samples_and_layout() {
    tabletfx::Phaser mono(48000.0F, 1);
    std::array<float, 5> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.25F, -0.5F};
    mono.process(invalid, 1);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F && invalid[2] == 0.0F,
            "phaser did not replace non-finite input with silence");

    std::array<float, 4> wrong_layout{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before = wrong_layout;
    mono.process(wrong_layout, 2);
    require(wrong_layout == before, "phaser modified a mismatched channel layout");

    std::array<float, 512> block{};
    block.fill(0.25F);
    tabletfx::Phaser stereo;
    const std::size_t before_allocations = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) stereo.process(block, 2);
    const std::size_t after_allocations = allocation_count.load(std::memory_order_relaxed);
    require(before_allocations == after_allocations, "phaser allocated during process()");
    require(std::all_of(block.begin(), block.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "phaser emitted a non-finite output");
}

void test_invalid_construction() {
    bool bad_rate = false;
    try { tabletfx::Phaser invalid(100.0F, 1); }
    catch (const std::invalid_argument&) { bad_rate = true; }
    require(bad_rate, "phaser accepted an unsupported sample rate");

    bool bad_channels = false;
    try { tabletfx::Phaser invalid(48000.0F, 3); }
    catch (const std::invalid_argument&) { bad_channels = true; }
    require(bad_channels, "phaser accepted an unsupported channel count");
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
        test_defaults_bounds_and_stage_choices();
        test_tempo_sync_divisions();
        test_notch_and_selectable_stages();
        test_dry_mix_stereo_and_modulation();
        test_smoothing_blocks_reset_and_feedback_stability();
        test_no_allocations_invalid_samples_and_layout();
        test_invalid_construction();
        std::cout << "Phaser tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Phaser test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
