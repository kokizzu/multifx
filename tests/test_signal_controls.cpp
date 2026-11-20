#include "tabletfx/signal_controls.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <span>
#include <stdexcept>
#include <string>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void require_near(float actual, float expected, float tolerance,
                  const std::string& message) {
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance,
            message);
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

void test_input_mute_ramps_without_touching_source() {
    tabletfx::SignalControls controls(8000.0F, 0.25F);
    std::array<float, 4> input{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> output{};
    controls.set_input_muted(true);
    controls.process_input(input, output, 1);
    require_near(output[0], 0.5F, 1.0e-6F, "input mute did not ramp down smoothly");
    require_near(output[1], 0.0F, 1.0e-6F, "input mute did not reach silence");
    require_near(output[2], 0.0F, 1.0e-6F, "input mute silence was not held");
    require(input[0] == 1.0F && input[3] == 1.0F,
            "input control modified its source buffer");
    controls.set_input_muted(false);
    controls.process_input(input, output, 1);
    require_near(output[0], 0.5F, 1.0e-6F, "input unmute did not ramp up smoothly");
    require_near(output[1], 1.0F, 1.0e-6F, "input unmute did not restore unity");
}

void test_independent_blocks_and_global_bypass_crossfade() {
    tabletfx::SignalControls controls(8000.0F, 0.25F);
    std::array<float, 4> dry{0.0F, 0.0F, 0.0F, 0.0F};
    std::array<float, 4> wet{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> output{};
    controls.set_global_bypassed(true);
    controls.process_output(dry, wet, output, 1);
    require_near(output[0], 0.5F, 1.0e-6F,
                 "global bypass did not crossfade rather than click");
    require_near(output[1], 0.0F, 1.0e-6F,
                 "global bypass did not select the dry path");
    require(controls.global_bypassed(), "global bypass state was not retained");
    controls.set_global_bypassed(false);
    controls.process_output(dry, wet, output, 1);
    require_near(output[0], 0.5F, 1.0e-6F,
                 "global bypass re-enable did not crossfade back");
    require_near(output[1], 1.0F, 1.0e-6F,
                 "global bypass re-enable did not restore the wet path");
    controls.set_mix(0.25F);
    controls.process_output(dry, wet, output, 1);
    require_near(output[1], 0.25F, 1.0e-6F,
                 "independent dry/wet setting was not restored after bypass");
}

void test_panic_is_immediate_and_release_fades_in() {
    tabletfx::SignalControls controls(8000.0F, 0.5F);
    std::array<float, 4> dry{0.25F, 0.25F, 0.25F, 0.25F};
    std::array<float, 4> wet{0.75F, 0.75F, 0.75F, 0.75F};
    std::array<float, 4> output{};
    controls.set_panic_muted(true);
    controls.process_output(dry, wet, output, 1);
    require(controls.panic_muted(), "panic state was not retained");
    for (float sample : output) require(sample == 0.0F, "panic mute was not immediate");
    controls.set_panic_muted(false);
    controls.process_output(dry, wet, output, 1);
    require_near(output[0], 0.1875F, 1.0e-6F,
                 "panic release did not begin with a controlled fade-in");
    require_near(output[3], 0.75F, 1.0e-6F,
                 "panic release did not restore the processed output");
}

void test_stereo_linking_bounds_reset_and_no_allocations() {
    tabletfx::SignalControls controls(48000.0F, 10.0F);
    require(controls.memory_bytes() == sizeof(controls),
            "signal-controls memory reporting changed");
    controls.set_mix(2.0F);
    require(controls.mix() == 1.0F, "wet mix did not clamp to its upper bound");
    controls.set_mix(-1.0F);
    require(controls.mix() == 0.0F, "wet mix did not clamp to its lower bound");
    controls.set_mix(1.0F);
    controls.reset();

    std::array<float, 16> dry{};
    std::array<float, 16> wet{};
    std::array<float, 16> output{};
    for (std::size_t i = 0; i < wet.size(); ++i) wet[i] = 0.25F;
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int i = 0; i < 100; ++i) {
        controls.process_output(dry, wet, output, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "signal controls allocated during audio processing");
    for (float sample : output) require_near(sample, 0.25F, 1.0e-6F,
                                             "stereo output channels were not processed");
}

} // namespace

int main() {
    try {
        test_input_mute_ramps_without_touching_source();
        test_independent_blocks_and_global_bypass_crossfade();
        test_panic_is_immediate_and_release_fades_in();
        test_stereo_linking_bounds_reset_and_no_allocations();
        std::cout << "All signal-controls tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Signal-controls test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
