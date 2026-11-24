#include "tabletfx/effects/vibratone.hpp"

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

void test_defaults_and_bounds() {
    tabletfx::Vibratone effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "vibratone" && defaults.tone == 0.8F &&
                defaults.fast_speed_hz == 6.5F &&
                defaults.slow_speed_hz == 0.85F &&
                defaults.speed_fast == false && defaults.ramp_ms == 650.0F &&
                defaults.sensitivity == 0.5F && !defaults.dynamics_switch,
            "project-authored Vibratone defaults changed");
    require(effect.memory_bytes() > sizeof(effect),
            "rotor storage is missing from Vibratone memory accounting");

    auto parameters = defaults;
    parameters.drive = 2.0F;
    parameters.tone = -1.0F;
    parameters.level_db = 20.0F;
    parameters.fast_speed_hz = 100.0F;
    parameters.slow_speed_hz = 8.0F;
    parameters.ramp_ms = 1.0F;
    parameters.sensitivity = 2.0F;
    effect.set_parameters(parameters);
    const auto bounded = effect.parameters();
    require(bounded.drive == 1.0F && bounded.tone == 0.0F &&
                bounded.level_db == 12.0F && bounded.fast_speed_hz == 10.0F &&
                bounded.slow_speed_hz == 5.0F && bounded.ramp_ms == 50.0F &&
                bounded.sensitivity == 1.0F,
            "Vibratone project-authored parameter bounds changed");

    parameters = defaults;
    parameters.fast_speed_hz = 4.0F;
    parameters.slow_speed_hz = 3.0F;
    parameters.speed_fast = true;
    effect.set_parameters(parameters);
    require(effect.parameters().slow_speed_hz == 2.0F &&
                std::abs(effect.target_speed_hz() - 4.0F) < 1.0e-6F,
            "Vibratone should bound slow speed below the fast speed");
}

void test_speed_switch_and_automatic_dynamics() {
    tabletfx::Vibratone effect(48000.0F, 1);
    std::vector<float> block(128, 0.0F);
    auto parameters = effect.parameters();
    parameters.speed_fast = true;
    effect.set_parameters(parameters);
    require(std::abs(effect.target_speed_hz() - parameters.fast_speed_hz) < 1.0e-6F,
            "manual speed switch did not select Fast");
    parameters.speed_fast = false;
    parameters.dynamics_switch = true;
    parameters.sensitivity = 0.5F;
    effect.set_parameters(parameters);
    std::fill(block.begin(), block.end(), 0.5F);
    for (int i = 0; i < 12; ++i) effect.process(block, 1);
    require(effect.input_envelope() > 0.1F &&
                std::abs(effect.target_speed_hz() - parameters.fast_speed_hz) < 1.0e-6F,
            "dynamics switch did not ramp toward Fast above the project threshold");
    std::fill(block.begin(), block.end(), 0.0F);
    for (int i = 0; i < 2400; ++i) effect.process(block, 1);
    require(effect.input_envelope() < 0.001F &&
                std::abs(effect.target_speed_hz() - parameters.slow_speed_hz) < 1.0e-4F,
            "dynamics switch did not return to Slow after the input faded");
}

void test_tone_and_output_level() {
    const auto render = [](float tone, float level_db) {
        tabletfx::VibratoneParameters parameters;
        parameters.tone = tone;
        parameters.level_db = level_db;
        parameters.speed_fast = true;
        parameters.ramp_ms = 50.0F;
        parameters.drive = 0.0F;
        tabletfx::Vibratone effect(48000.0F, 1, parameters);
        std::vector<float> audio(48000);
        for (std::size_t frame = 0; frame < audio.size(); ++frame) {
            audio[frame] = 0.2F * std::sin(2.0F * 3.14159265358979323846F *
                                           9000.0F * static_cast<float>(frame) /
                                           48000.0F);
        }
        effect.process(audio, 1);
        double energy = 0.0;
        for (std::size_t i = 12000; i < audio.size(); ++i) {
            energy += static_cast<double>(audio[i]) * audio[i];
        }
        return std::sqrt(energy / static_cast<double>(audio.size() - 12000U));
    };
    const double dark = render(0.0F, 0.0F);
    const double bright = render(1.0F, 0.0F);
    const double plus_six_db = render(1.0F, 6.0F);
    require(dark < bright * 0.5,
            "Tone control did not reduce upper-frequency energy");
    require(std::abs(plus_six_db / bright - 1.995) < 0.03,
            "project output level control did not apply its dB gain");
}

void test_reset_finite_output_and_irregular_blocks() {
    tabletfx::VibratoneParameters parameters;
    parameters.dynamics_switch = true;
    parameters.tone = 0.2F;
    parameters.speed_fast = true;
    tabletfx::Vibratone effect(44100.0F, 2, parameters);
    std::array<float, 18> audio{0.4F, -0.3F, 0.2F, -0.1F, 0.3F, -0.2F,
                                0.1F, -0.4F, 0.2F, -0.3F, 0.4F, -0.1F,
                                0.2F, -0.3F, 0.1F, -0.2F, 0.3F, -0.4F};
    effect.process(std::span<float>(audio.data(), 6), 2);
    effect.process(std::span<float>(audio.data() + 6, 12), 2);
    require(std::all_of(audio.begin(), audio.end(), [](float value) {
                return std::isfinite(value);
            }), "Vibratone produced non-finite output");
    effect.reset();
    require(effect.input_envelope() == 0.0F && effect.current_speed_hz() == 0.0F,
            "Vibratone reset did not clear envelope and rotor state");
    std::array<float, 4> hostile{std::numeric_limits<float>::max(),
                                  -std::numeric_limits<float>::max(),
                                  std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::quiet_NaN()};
    effect.process(hostile, 2);
    require(std::all_of(hostile.begin(), hostile.end(), [](float value) {
                return std::isfinite(value);
            }), "Vibratone failed finite recovery on hostile input");
}

void test_no_process_allocation() {
    tabletfx::VibratoneParameters parameters;
    parameters.dynamics_switch = true;
    tabletfx::Vibratone effect(48000.0F, 2, parameters);
    std::array<float, 256> audio{};
    audio.fill(0.5F);
    allocation_count = 0;
    effect.process(audio, 2);
    require(allocation_count.load() == 0,
            "process allocs");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_and_bounds();
        test_speed_switch_and_automatic_dynamics();
        test_tone_and_output_level();
        test_reset_finite_output_and_irregular_blocks();
        test_no_process_allocation();
        std::cout << "Vibratone tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Vibratone test failure: " << error.what() << '\n';
        return 1;
    }
}
