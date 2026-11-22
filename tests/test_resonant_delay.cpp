#include "tabletfx/effects/resonant_delay.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::atomic<bool> watch_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

float render_sine_rms(float frequency_hz) {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frame_count = 8192;
    tabletfx::ResonantDelayParameters parameters;
    parameters.time_ms = 1.0F;
    parameters.feedback = 0.0F;
    parameters.center_hz = 1000.0F;
    parameters.resonance_q = 8.0F;
    parameters.mix = 1.0F;
    tabletfx::ResonantDelay delay(sample_rate, 1, 2000.0F, parameters);
    std::vector<float> audio(frame_count);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        audio[frame] = 0.5F * std::sin(
            6.2831853071795864769F * frequency_hz *
            static_cast<float>(frame) / sample_rate);
    }
    delay.process(audio, 1);
    double sum = 0.0;
    for (std::size_t frame = 1024; frame < frame_count; ++frame) {
        sum += static_cast<double>(audio[frame]) * audio[frame];
    }
    return static_cast<float>(std::sqrt(
        sum / static_cast<double>(frame_count - 1024U)));
}

void test_delayed_wet_output_and_feedback_repeats() {
    tabletfx::ResonantDelayParameters parameters;
    parameters.time_ms = 2.0F;
    parameters.feedback = 0.6F;
    parameters.center_hz = 1000.0F;
    parameters.resonance_q = 2.0F;
    parameters.mix = 1.0F;
    tabletfx::ResonantDelay delay(48000.0F, 1, 2000.0F, parameters);
    std::vector<float> impulse(288, 0.0F);
    impulse[0] = 1.0F;
    delay.process(impulse, 1);
    require(impulse[0] == 0.0F,
            "100% wet Resonant Delay must not leak the dry input");
    const float first_echo = std::abs(impulse[96]);
    const float second_echo = std::abs(impulse[192]);
    require(first_echo > 1.0e-3F,
            "Resonant Delay must return a filtered repeat at 2 ms");
    require(second_echo > 1.0e-5F && second_echo < first_echo,
            "Resonant feedback must create a bounded decaying second repeat");
    require(delay.id() == "resonant-delay" && delay.memory_bytes() > sizeof(delay),
            "Resonant Delay identity or memory accounting is incorrect");
}

void test_resonance_selects_a_frequency_band() {
    const float centered = render_sine_rms(1000.0F);
    const float outside = render_sine_rms(100.0F);
    require(centered > outside * 4.0F,
            "resonant band-pass should emphasize its center-frequency region");
}

void test_bounds_reset_stereo_and_no_process_allocation() {
    tabletfx::ResonantDelay delay(48000.0F, 2);
    auto parameters = delay.parameters();
    parameters.time_ms = 9000.0F;
    parameters.feedback = 3.0F;
    parameters.center_hz = 90000.0F;
    parameters.resonance_q = 80.0F;
    parameters.mix = -1.0F;
    delay.configure(parameters);
    const auto bounded = delay.parameters();
    require(bounded.time_ms == 2000.0F && bounded.feedback == 0.95F &&
                bounded.center_hz <= 8000.0F && bounded.resonance_q == 12.0F &&
                bounded.mix == 0.0F,
            "project-authored Resonant Delay parameters must remain bounded");

    parameters = bounded;
    parameters.time_ms = 1.0F;
    parameters.feedback = 0.0F;
    parameters.center_hz = 1000.0F;
    parameters.resonance_q = 2.0F;
    parameters.mix = 1.0F;
    delay.configure(parameters);
    std::vector<float> stereo(512, 0.0F);
    stereo[0] = 0.75F;
    stereo[1] = -0.25F;
    allocation_count.store(0, std::memory_order_relaxed);
    watch_allocations.store(true, std::memory_order_relaxed);
    delay.process(stereo, 2);
    watch_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0,
            "Resonant Delay process() must not allocate");
    require(std::all_of(stereo.begin(), stereo.end(), [](float value) {
                return std::isfinite(value);
            }),
            "stereo Resonant Delay output must remain finite");
    delay.reset();
    std::vector<float> silence(256, 0.0F);
    delay.process(silence, 2);
    require(std::all_of(silence.begin(), silence.end(), [](float value) {
                return value == 0.0F;
            }),
            "reset must clear the delay ring and resonator state");
    std::vector<float> wrong_layout(32, 0.5F);
    delay.process(wrong_layout, 1);
    require(std::all_of(wrong_layout.begin(), wrong_layout.end(), [](float value) {
                return value == 0.5F;
            }),
            "channel mismatch must leave caller audio unchanged");
}

} // namespace

void* operator new(std::size_t size) {
    if (watch_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (watch_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
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
        test_delayed_wet_output_and_feedback_repeats();
        test_resonance_selects_a_frequency_band();
        test_bounds_reset_stereo_and_no_process_allocation();
        std::cout << "Resonant Delay tests passed\n";
    } catch (const std::exception& exception) {
        watch_allocations.store(false, std::memory_order_relaxed);
        std::cerr << "Resonant Delay tests failed: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
