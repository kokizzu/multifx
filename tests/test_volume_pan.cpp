#include "tabletfx/effects/volume_pan.hpp"

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

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float actual, float expected, float tolerance = 1.0e-4F) {
    return std::abs(actual - expected) <= tolerance;
}

void test_defaults_identity_and_endpoints() {
    tabletfx::VolumePan effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "volume-pan", "stable effect ID changed");
    require(effect.memory_bytes() == sizeof(effect),
            "memory report is inaccurate");
    require(defaults.volume == 1.0F && defaults.minimum_db == -96.0F &&
                defaults.maximum_db == 0.0F &&
                defaults.taper == tabletfx::VolumePanTaper::Linear &&
                defaults.balance_percent == 0.0F &&
                defaults.input_select == tabletfx::VolumePanInputSelect::Stereo &&
                defaults.pan_left_percent == -100.0F &&
                defaults.pan_right_percent == 100.0F &&
                defaults.output_level_db == 0.0F && !defaults.bypassed &&
                defaults.bypass_mode == tabletfx::VolumePanBypassMode::Pass,
            "project-authored defaults changed");

    std::array<float, 4> stereo{0.25F, -0.5F, 0.75F, -1.0F};
    effect.process(stereo, 2);
    require(stereo == std::array<float, 4>{0.25F, -0.5F, 0.75F, -1.0F},
            "default hard-panned stereo routing should be unity-through");

    auto parameters = defaults;
    parameters.volume = 0.0F;
    effect.set_parameters(parameters);
    std::array<float, 2> heel{1.0F, 1.0F};
    effect.process(heel, 2);
    require(std::abs(heel[0]) < 2.0e-5F && std::abs(heel[1]) < 2.0e-5F,
            "heel position should reach the configured minimum endpoint");
}

void test_tapers_and_independent_output_level() {
    tabletfx::VolumePanParameters parameters;
    parameters.minimum_db = -96.0F;
    parameters.maximum_db = 0.0F;
    parameters.volume = 0.5F;
    parameters.taper = tabletfx::VolumePanTaper::Log30A;
    tabletfx::VolumePan log30(parameters);
    parameters.taper = tabletfx::VolumePanTaper::Log5A;
    tabletfx::VolumePan log5(parameters);
    parameters.taper = tabletfx::VolumePanTaper::STaper;
    tabletfx::VolumePan s_taper(parameters);
    parameters.taper = tabletfx::VolumePanTaper::Linear;
    tabletfx::VolumePan linear(parameters);

    const auto center_gain = [](tabletfx::VolumePan& effect) {
        std::array<float, 2> audio{1.0F, 0.0F};
        effect.process(audio, 2);
        return std::hypot(audio[0], audio[1]);
    };
    const float g30 = center_gain(log30);
    const float g5 = center_gain(log5);
    const float gs = center_gain(s_taper);
    const float gl = center_gain(linear);
    // Linear and S-Taper are both 0.5 gain at the midpoint. The authored
    // -5 dB taper is about 0.562 gain there, so it sits above both.
    require(g30 < gl && near(gs, gl) && gl < g5,
            "named taper approximations do not match their midpoint levels");

    parameters.volume = 1.0F;
    parameters.minimum_db = 0.0F;
    parameters.maximum_db = 0.0F;
    parameters.output_level_db = 6.0F;
    parameters.taper = tabletfx::VolumePanTaper::Linear;
    tabletfx::VolumePan output_level(parameters);
    std::array<float, 2> audio{0.25F, -0.25F};
    output_level.process(audio, 2);
    const float expected = 0.25F * std::pow(10.0F, 6.0F / 20.0F);
    require(near(audio[0], expected) && near(audio[1], -expected),
            "Output Level did not act independently of the Volume control");
}

void test_pan_input_select_and_balance() {
    tabletfx::VolumePanParameters parameters;
    parameters.minimum_db = 0.0F;
    parameters.maximum_db = 0.0F;
    parameters.pan_left_percent = 0.0F;
    parameters.pan_right_percent = 100.0F;
    tabletfx::VolumePan centered_left(parameters);
    std::array<float, 2> audio{1.0F, 0.25F};
    centered_left.process(audio, 2);
    require(near(audio[0], std::sqrt(0.5F)) &&
                near(audio[1], std::sqrt(0.5F) + 0.25F),
            "independent left/right pan controls did not route the channels");

    parameters.pan_left_percent = -100.0F;
    parameters.pan_right_percent = 100.0F;
    parameters.input_select = tabletfx::VolumePanInputSelect::RightOnly;
    tabletfx::VolumePan right_only(parameters);
    audio = {3.0F, -0.5F};
    right_only.process(audio, 2);
    require(near(audio[0], 0.0F) && near(audio[1], -0.5F),
            "Right Only input selection did not preserve the selected source");

    parameters.input_select = tabletfx::VolumePanInputSelect::Stereo;
    parameters.balance_percent = -100.0F;
    tabletfx::VolumePan left_balance(parameters);
    audio = {0.5F, 0.75F};
    left_balance.process(audio, 2);
    require(near(audio[0], 0.5F) && near(audio[1], 0.0F),
            "hard-left balance did not attenuate the right output");

    parameters.balance_percent = 100.0F;
    tabletfx::VolumePan right_balance(parameters);
    audio = {0.5F, 0.75F};
    right_balance.process(audio, 2);
    require(near(audio[0], 0.0F) && near(audio[1], 0.75F),
            "hard-right balance did not attenuate the left output");
}

void test_bypass_bounds_channels_and_no_allocations() {
    tabletfx::VolumePanParameters parameters;
    parameters.minimum_db = 0.0F;
    parameters.maximum_db = 0.0F;
    parameters.volume = 0.0F;
    parameters.bypassed = true;
    parameters.bypass_mode = tabletfx::VolumePanBypassMode::Pass;
    tabletfx::VolumePan effect(parameters);
    std::array<float, 4> audio{0.2F, -0.4F, 0.6F, -0.8F};
    effect.process(audio, 2);
    require(audio == std::array<float, 4>{0.2F, -0.4F, 0.6F, -0.8F},
            "Pass bypass should preserve the input");

    parameters.bypass_mode = tabletfx::VolumePanBypassMode::Mute;
    effect.set_parameters(parameters);
    effect.process(audio, 2);
    require(audio == std::array<float, 4>{0.0F, 0.0F, 0.0F, 0.0F},
            "Mute bypass should zero the input");

    parameters.bypassed = false;
    parameters.volume = std::numeric_limits<float>::quiet_NaN();
    parameters.minimum_db = 20.0F;
    parameters.maximum_db = -200.0F;
    parameters.output_level_db = 100.0F;
    parameters.pan_left_percent = -200.0F;
    parameters.pan_right_percent = 200.0F;
    parameters.balance_percent = 200.0F;
    parameters.taper = static_cast<tabletfx::VolumePanTaper>(255U);
    parameters.input_select = static_cast<tabletfx::VolumePanInputSelect>(255U);
    parameters.bypass_mode = static_cast<tabletfx::VolumePanBypassMode>(255U);
    effect.set_parameters(parameters);
    const auto sanitized = effect.parameters();
    require(sanitized.volume == 1.0F && sanitized.minimum_db == -96.0F &&
                sanitized.maximum_db == 12.0F &&
                sanitized.output_level_db == 12.0F &&
                sanitized.pan_left_percent == -100.0F &&
                sanitized.pan_right_percent == 100.0F &&
                sanitized.balance_percent == 100.0F &&
                sanitized.taper == tabletfx::VolumePanTaper::Linear &&
                sanitized.input_select == tabletfx::VolumePanInputSelect::Stereo &&
                sanitized.bypass_mode == tabletfx::VolumePanBypassMode::Pass,
            "invalid controls were not bounded or sanitized");

    parameters = sanitized;
    parameters.minimum_db = 0.0F;
    parameters.maximum_db = 0.0F;
    parameters.pan_left_percent = -100.0F;
    parameters.pan_right_percent = 100.0F;
    parameters.balance_percent = 0.0F;
    parameters.output_level_db = 0.0F;
    effect.set_parameters(parameters);
    std::array<float, 3> three_channels{0.25F, -0.5F, 0.75F};
    effect.process(three_channels, 3);
    require(near(three_channels[0], 0.25F) &&
                near(three_channels[1], -0.5F) &&
                near(three_channels[2], 0.75F),
            "additional channels should receive the shared unity gain");

    allocation_count = 0U;
    count_allocations = true;
    for (int iteration = 0; iteration < 100; ++iteration) {
        effect.process(three_channels, 3);
    }
    count_allocations = false;
    require(allocation_count == 0U,
            "Volume/Pan allocated memory inside process()");

    std::array<float, 3> mono{0.25F, -0.5F, 0.75F};
    effect.process(mono, 1);
    require(mono == std::array<float, 3>{0.25F, -0.5F, 0.75F},
            "mono channel processing changed the unity-gain signal");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_identity_and_endpoints();
        test_tapers_and_independent_output_level();
        test_pan_input_select_and_balance();
        test_bypass_bounds_channels_and_no_allocations();
        std::cout << "Volume/Pan tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        count_allocations = false;
        std::cerr << "Volume/Pan test failure: " << exception.what() << '\n';
        return 1;
    }
}
