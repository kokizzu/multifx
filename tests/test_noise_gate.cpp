#include "tabletfx/effects.hpp"

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
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

void test_defaults_and_parameter_limits() {
    tabletfx::NoiseGate gate(48000.0F);
    require(gate.id() == "noise-gate", "stable noise-gate ID changed");
    require(gate.memory_bytes() == sizeof(gate),
            "noise gate must report its directly owned state size");

    tabletfx::NoiseGateParameters parameters;
    require(parameters.threshold_db == -60.0F && parameters.attenuation_db == -72.0F &&
                parameters.hysteresis_db == 3.0F && parameters.attack_ms == 2.0F &&
                parameters.hold_ms == 20.0F && parameters.release_ms == 80.0F,
            "sane default gate settings changed unexpectedly");

    parameters.threshold_db = std::numeric_limits<float>::quiet_NaN();
    parameters.attenuation_db = 20.0F;
    parameters.hysteresis_db = 100.0F;
    parameters.attack_ms = -1.0F;
    parameters.hold_ms = 9000.0F;
    parameters.release_ms = std::numeric_limits<float>::infinity();
    gate.configure(48000.0F, parameters);
    std::array<float, 2> silence{0.0F, 0.0F};
    gate.process(silence, 2);
    require(std::isfinite(silence[0]) && std::isfinite(silence[1]),
            "bounded non-finite gate settings produced non-finite output");
}

void test_hysteresis_and_hold() {
    tabletfx::NoiseGateParameters parameters;
    parameters.threshold_db = -20.0F;
    parameters.attenuation_db = -60.0F;
    parameters.hysteresis_db = 3.0F;
    parameters.attack_ms = 0.0F;
    parameters.hold_ms = 3.0F;
    parameters.release_ms = 0.0F;
    tabletfx::NoiseGate gate(1000.0F, parameters);

    std::array<float, 1> above_open_threshold{0.2F};
    gate.process(above_open_threshold, 1);
    require(std::abs(above_open_threshold[0] - 0.2F) < 1.0e-6F,
            "signal above the opening threshold did not open the gate immediately");

    std::array<float, 1> between_thresholds{0.08F};
    gate.process(between_thresholds, 1);
    require(std::abs(between_thresholds[0] - 0.08F) < 1.0e-6F,
            "hysteresis did not keep the gate open between its thresholds");

    std::array<float, 3> hold_samples{0.04F, 0.04F, 0.04F};
    gate.process(hold_samples, 1);
    for (const float sample : hold_samples) {
        require(std::abs(sample - 0.04F) < 1.0e-6F,
                "gate closed before the configured hold period elapsed");
    }

    std::array<float, 1> after_hold{0.04F};
    gate.process(after_hold, 1);
    require(after_hold[0] < 0.001F,
            "gate did not begin closing after hold elapsed below the close threshold");

    gate.reset();
    std::array<float, 2> reset_output{0.04F, 0.2F};
    gate.process(std::span<float>(reset_output).first(1), 1);
    gate.process(std::span<float>(reset_output).subspan(1), 1);
    require(reset_output[0] < 0.001F && std::abs(reset_output[1] - 0.2F) < 1.0e-6F,
            "reset did not close the gate and clear its hold state");
}

void test_stereo_link_and_attack_release() {
    tabletfx::NoiseGateParameters parameters;
    parameters.threshold_db = -30.0F;
    parameters.attenuation_db = -72.0F;
    parameters.hysteresis_db = 4.0F;
    parameters.attack_ms = 4.0F;
    parameters.hold_ms = 0.0F;
    parameters.release_ms = 8.0F;
    tabletfx::NoiseGate gate(48000.0F, parameters);

    std::vector<float> attack(2048, 0.0F);
    for (std::size_t frame = 0; frame < attack.size() / 2; ++frame) {
        attack[frame * 2] = 0.5F;
        attack[frame * 2 + 1] = 0.125F;
    }
    gate.process(attack, 2);
    require(attack[0] < 0.5F && attack[attack.size() - 2] > 0.49F,
            "gate attack did not ramp from closed gain toward unity");
    for (std::size_t frame = 0; frame < attack.size() / 2; ++frame) {
        require(std::abs(attack[frame * 2 + 1] * 4.0F - attack[frame * 2]) < 1.0e-5F,
                "stereo-linked detection changed channel balance");
    }

    std::vector<float> silence(48000, 0.0F);
    gate.process(silence, 2);
    std::array<float, 2> closed_probe{0.01F, 0.005F};
    gate.process(closed_probe, 2);
    require(closed_probe[0] < 1.0e-4F && closed_probe[1] < 1.0e-4F,
            "gate release did not return the linked gate to attenuation");
}

void test_nonfinite_input_and_no_processing_allocations() {
    tabletfx::NoiseGate gate(48000.0F);
    std::vector<float> samples(256 * 2, 0.1F);
    samples[0] = std::numeric_limits<float>::quiet_NaN();
    samples[1] = std::numeric_limits<float>::infinity();
    gate.process(std::span<float>(samples).first(2), 2);
    require(samples[0] == 0.0F && samples[1] == 0.0F,
            "non-finite input was not replaced by finite silence");

    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int block = 0; block < 100; ++block) {
        gate.process(samples, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "noise gate allocated memory while processing audio");
    for (const float sample : samples) {
        require(std::isfinite(sample), "non-finite input contaminated later gate output");
    }
}

} // namespace

int main() {
    try {
        test_defaults_and_parameter_limits();
        test_hysteresis_and_hold();
        test_stereo_link_and_attack_release();
        test_nonfinite_input_and_no_processing_allocations();
        std::cout << "All noise-gate tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Noise-gate test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
