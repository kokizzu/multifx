#include "tabletfx/effects/studio_pan_delay.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float actual, float expected, float tolerance = 2.0e-5F) {
    return std::abs(actual - expected) <= tolerance;
}

tabletfx::StudioPanDelayParameters impulse_parameters() {
    tabletfx::StudioPanDelayParameters parameters;
    parameters.delay_time_ms = 30.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.pan_depth = 0.0F;
    return parameters;
}

std::vector<float> render_constant_envelope(float level) {
    auto parameters = impulse_parameters();
    parameters.pan_depth = 1.0F;
    parameters.pan_mode = tabletfx::StudioDelayPanMode::Envelope;
    parameters.pan_rate_hz = 1.0F;
    tabletfx::StudioPanDelay delay(8000.0F, 60.0F, parameters);
    std::vector<float> audio(400U * 2U, level);
    delay.process(audio, 2U);
    return audio;
}

void test_defaults_and_project_bounds() {
    using tabletfx::StudioDelayPanMode;
    using tabletfx::StudioPanDelay;

    StudioPanDelay delay;
    const auto defaults = delay.parameters();
    require(delay.id() == "studio-delay" &&
                defaults.delay_time_ms == 350.0F &&
                defaults.feedback == 0.25F && defaults.mix == 0.25F &&
                defaults.pan_rate_hz == 0.5F && defaults.pan_depth == 0.75F &&
                defaults.pan_mode == StudioDelayPanMode::Sine &&
                !defaults.bypass,
            "safe project-authored delay defaults changed");
    require(near(delay.maximum_delay_ms(), 2000.0F) &&
                near(delay.effective_delay_ms(), 350.0F) &&
                delay.fixed_latency_frames() == 0U,
            "default buffer range, delay time, or latency is incorrect");

    delay.set_delay_time_ms(1.0F);
    require(delay.parameters().delay_time_ms == 30.0F,
            "delay time did not enforce the documented minimum span");
    delay.set_delay_time_ms(5000.0F);
    require(delay.parameters().delay_time_ms == 2000.0F,
            "delay time did not enforce the documented maximum span");
    delay.set_delay_time_ms(std::numeric_limits<float>::quiet_NaN());
    require(delay.parameters().delay_time_ms == 2000.0F,
            "non-finite delay time did not retain its last valid value");

    delay.set_feedback(-1.0F);
    require(delay.parameters().feedback == 0.0F,
            "negative feedback was not bounded to the project minimum");
    delay.set_feedback(10.0F);
    require(delay.parameters().feedback == 0.9F,
            "feedback was not bounded below the unstable unity loop");
    delay.set_feedback(std::numeric_limits<float>::infinity());
    require(delay.parameters().feedback == 0.9F,
            "non-finite feedback did not retain its last valid value");

    delay.set_mix(-1.0F);
    require(delay.parameters().mix == 0.0F,
            "Mix was not bounded to the project minimum");
    delay.set_mix(2.0F);
    require(delay.parameters().mix == 1.0F,
            "Mix was not bounded to the project maximum");
    delay.set_mix(std::numeric_limits<float>::quiet_NaN());
    require(delay.parameters().mix == 1.0F,
            "non-finite Mix did not retain its last valid value");

    delay.set_pan_rate_hz(0.0F);
    require(delay.parameters().pan_rate_hz == 0.05F,
            "Pan Rate did not enforce the project minimum");
    delay.set_pan_rate_hz(100.0F);
    require(delay.parameters().pan_rate_hz == 20.0F,
            "Pan Rate did not enforce the project maximum");
    delay.set_pan_depth(-1.0F);
    require(delay.parameters().pan_depth == 0.0F,
            "Pan Depth did not enforce its project minimum");
    delay.set_pan_depth(2.0F);
    require(delay.parameters().pan_depth == 1.0F,
            "Pan Depth did not enforce its project maximum");
    delay.set_pan_mode(static_cast<StudioDelayPanMode>(255U));
    require(delay.parameters().pan_mode == StudioDelayPanMode::Sine,
            "invalid Pan Mode did not fall back to Sine");

    auto shorter = impulse_parameters();
    StudioPanDelay capped(8000.0F, 60.0F, shorter);
    capped.set_delay_time_ms(2000.0F);
    require(near(capped.maximum_delay_ms(), 60.0F) &&
                near(capped.parameters().delay_time_ms, 60.0F) &&
                capped.memory_bytes() <= sizeof(capped) + 2U * 1024U * 1024U,
            "requested duration must remain bounded by fixed allocated storage");

    tabletfx::StudioPanDelay default_signal(8000.0F, 500.0F);
    std::vector<float> default_audio(3600U * 2U, 0.0F);
    default_audio[0] = 1.0F;
    default_audio[1] = 1.0F;
    default_signal.process(default_audio, 2U);
    require(std::abs(default_audio[2800U * 2U]) > 0.01F &&
                std::abs(default_audio[2800U * 2U + 1U]) > 0.01F,
            "safe default wet Mix did not make its configured echo audible");

    StudioPanDelay high_rate(384000.0F, 2000.0F, shorter);
    require(high_rate.maximum_delay_ms() < 700.0F &&
                high_rate.memory_bytes() <=
                    sizeof(high_rate) + 2U * 1024U * 1024U,
            "high-rate delay storage exceeded the fixed project memory cap");
}

void test_impulse_echo_uses_fixed_stereo_delay() {
    using tabletfx::StudioPanDelay;
    auto parameters = impulse_parameters();
    StudioPanDelay delay(8000.0F, 60.0F, parameters);
    constexpr std::size_t delay_frames = 240U;
    std::vector<float> audio(600U * 2U, 0.0F);
    audio[0] = 1.0F;
    audio[1] = 1.0F;
    delay.process(audio, 2U);

    require(std::all_of(audio.begin(), audio.begin() + delay_frames * 2U,
                        [](float sample) { return sample == 0.0F; }),
            "fully wet output leaked the dry impulse before its delay time");
    require(near(audio[delay_frames * 2U], 1.0F) &&
                near(audio[delay_frames * 2U + 1U], 1.0F),
            "stereo impulse did not return at the configured delay time");
    require(audio[delay_frames * 4U] == 0.0F &&
                audio[delay_frames * 4U + 1U] == 0.0F,
            "zero feedback produced an unexpected second repeat");
}

void test_bypassed_instance_defers_storage_until_control_activation() {
    using tabletfx::StudioPanDelay;
    auto parameters = impulse_parameters();
    parameters.bypass = true;
    StudioPanDelay delay(8000.0F, 60.0F, parameters);
    require(delay.memory_bytes() == sizeof(delay),
            "bypassed construction unexpectedly allocated delay storage");

    std::array<float, 8U> bypass_audio{0.1F, -0.3F, 0.7F, 0.2F,
                                       -0.4F, 0.05F, 0.2F, -0.1F};
    const auto original = bypass_audio;
    delay.process(bypass_audio, 2U);
    require(bypass_audio == original && delay.memory_bytes() == sizeof(delay),
            "unprepared bypass must pass audio without allocating");

    parameters.bypass = false;
    delay.configure(parameters);
    require(delay.memory_bytes() > sizeof(delay) &&
                !delay.parameters().bypass,
            "explicit active configuration did not prepare delay storage");

    std::vector<float> echo(600U * 2U, 0.0F);
    echo[0] = 1.0F;
    echo[1] = 1.0F;
    delay.process(echo, 2U);
    require(std::abs(echo[240U * 2U]) > 0.5F &&
                std::abs(echo[240U * 2U + 1U]) > 0.5F,
            "prepared delayed output was not audible at the configured time");
}

void test_feedback_decays_and_remains_bounded() {
    using tabletfx::StudioPanDelay;
    auto parameters = impulse_parameters();
    parameters.feedback = 0.5F;
    StudioPanDelay delay(8000.0F, 60.0F, parameters);
    constexpr std::size_t delay_frames = 240U;
    std::vector<float> audio((delay_frames * 7U) * 2U, 0.0F);
    audio[0] = 1.0F;
    audio[1] = 1.0F;
    delay.process(audio, 2U);

    for (std::size_t repeat = 1U; repeat <= 5U; ++repeat) {
        const float expected = std::pow(0.5F, static_cast<float>(repeat - 1U));
        const std::size_t offset = repeat * delay_frames * 2U;
        require(near(audio[offset], expected) &&
                    near(audio[offset + 1U], expected),
                "feedback repeats did not decay by the configured amount");
    }

    parameters.feedback = 4.0F;
    StudioPanDelay stable(8000.0F, 60.0F, parameters);
    require(stable.parameters().feedback == 0.9F,
            "large feedback was not clamped to the project stability bound");
    std::vector<float> long_tail(240U * 48U * 2U, 0.0F);
    long_tail[0] = 1.0F;
    long_tail[1] = 1.0F;
    stable.process(long_tail, 2U);
    require(std::all_of(long_tail.begin(), long_tail.end(), [](float sample) {
                return std::isfinite(sample) && std::abs(sample) <= 8.0F;
            }),
            "high bounded feedback produced non-finite or unbounded audio");
    require(std::abs(long_tail[240U * 2U]) >
                std::abs(long_tail[240U * 40U * 2U]),
            "bounded feedback tail did not decay over repeated echoes");
}

void test_sine_triangle_and_envelope_pan_the_wet_repeats() {
    using tabletfx::StudioDelayPanMode;
    using tabletfx::StudioPanDelay;

    auto parameters = impulse_parameters();
    parameters.feedback = 0.9F;
    parameters.pan_depth = 1.0F;
    parameters.pan_rate_hz = 1.0F;
    parameters.pan_mode = StudioDelayPanMode::Sine;
    StudioPanDelay sine(8000.0F, 60.0F, parameters);
    std::vector<float> sine_audio(600U * 2U, 0.0F);
    sine_audio[0] = 1.0F;
    sine_audio[1] = 1.0F;
    sine.process(sine_audio, 2U);
    const std::size_t first_echo = 240U * 2U;
    require(sine_audio[first_echo + 1U] > sine_audio[first_echo] + 0.15F,
            "Sine mode did not pan the first echo toward the right");

    parameters.pan_mode = StudioDelayPanMode::Triangle;
    StudioPanDelay triangle(8000.0F, 60.0F, parameters);
    std::vector<float> triangle_audio(600U * 2U, 0.0F);
    triangle_audio[0] = 1.0F;
    triangle_audio[1] = 1.0F;
    triangle.process(triangle_audio, 2U);
    require(triangle_audio[first_echo] > triangle_audio[first_echo + 1U] + 0.5F,
            "Triangle mode did not pan the first echo toward the left");

    const auto quiet = render_constant_envelope(0.1F);
    const auto loud = render_constant_envelope(0.8F);
    const std::size_t envelope_echo = 240U * 2U;
    require(quiet[envelope_echo + 1U] > quiet[envelope_echo] + 0.01F,
            "Envelope mode did not widen panning for quiet input");
    require(near(loud[envelope_echo], 0.8F, 1.0e-4F) &&
                near(loud[envelope_echo + 1U], 0.8F, 1.0e-4F),
            "Envelope mode did not reduce panning for louder input");
}

void test_mix_ramp_and_bypass_behavior() {
    using tabletfx::StudioPanDelay;
    auto parameters = impulse_parameters();
    parameters.mix = 0.5F;
    StudioPanDelay delay(8000.0F, 60.0F, parameters);

    std::array<float, 240U * 2U> fill{};
    fill.fill(0.2F);
    delay.process(fill, 2U);
    require(near(fill.back(), 0.1F),
            "pre-echo Mix should expose the expected dry contribution");

    delay.set_mix(0.0F);
    std::array<float, 80U * 2U> tail{};
    delay.process(tail, 2U);
    require(std::abs(tail[0] - 0.1F) < 0.02F,
            "Mix setter caused an abrupt output step");
    require(near(tail.back(), 0.0F) && near(delay.effective_mix(), 0.0F),
            "Mix ramp did not reach the dry endpoint");

    delay.set_bypass(true);
    std::array<float, 6U> bypass_audio{0.1F, -0.3F, 0.7F,
                                       0.2F, -0.4F, 0.05F};
    const auto original = bypass_audio;
    delay.process(bypass_audio, 2U);
    require(bypass_audio == original && delay.parameters().bypass,
            "bypass did not pass through valid stereo input unchanged");
    delay.set_bypass(false);
    require(!delay.parameters().bypass,
            "bypass could not be disabled");

    auto wet_parameters = impulse_parameters();
    wet_parameters.pan_depth = 0.0F;
    StudioPanDelay wet(8000.0F, 60.0F, wet_parameters);
    wet.set_bypass(true);
    std::vector<float> blocked_input(600U * 2U, 0.0F);
    blocked_input[0] = 1.0F;
    blocked_input[1] = 1.0F;
    wet.process(blocked_input, 2U);
    require(blocked_input[0] == 1.0F && blocked_input[1] == 1.0F &&
                blocked_input[240U * 2U] == 0.0F &&
                blocked_input[240U * 2U + 1U] == 0.0F,
            "bypass fed new input into the delay line or leaked a repeat");
}

void test_reset_finite_input_safety_and_allocation_free_processing() {
    using tabletfx::StudioDelayPanMode;
    using tabletfx::StudioPanDelay;

    auto parameters = impulse_parameters();
    parameters.feedback = 0.7F;
    parameters.pan_mode = StudioDelayPanMode::Envelope;
    StudioPanDelay delay(48000.0F, 100.0F, parameters);
    std::array<float, 128U> priming{};
    priming[0] = 0.5F;
    priming[1] = -0.25F;
    delay.process(priming, 2U);
    delay.reset();

    std::array<float, 128U> silence{};
    delay.process(silence, 2U);
    require(std::all_of(silence.begin(), silence.end(),
                        [](float sample) { return sample == 0.0F; }),
            "reset did not clear delay and envelope state");

    std::array<float, 8U> malformed{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 20.0F,
        0.25F, -0.25F, 0.5F, -0.5F};
    delay.process(malformed, 2U);
    require(std::all_of(malformed.begin(), malformed.end(), [](float sample) {
                return std::isfinite(sample) && std::abs(sample) <= 8.0F;
            }),
            "non-finite or excessive input produced invalid output");

    StudioPanDelay allocation_check(48000.0F, 100.0F, parameters);
    std::array<float, 512U> audio{};
    audio[0] = 0.25F;
    audio[1] = -0.25F;
    const std::size_t allocated_memory = allocation_check.memory_bytes();
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        allocation_check.process(audio, 2U);
    }
    count_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "delay process() allocated memory");
    require(allocation_check.memory_bytes() == allocated_memory &&
                allocation_check.fixed_latency_frames() == 0U,
            "fixed ring memory or zero-latency metadata changed during process");
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
        test_defaults_and_project_bounds();
        test_bypassed_instance_defers_storage_until_control_activation();
        test_impulse_echo_uses_fixed_stereo_delay();
        test_feedback_decays_and_remains_bounded();
        test_sine_triangle_and_envelope_pan_the_wet_repeats();
        test_mix_ramp_and_bypass_behavior();
        test_reset_finite_input_safety_and_allocation_free_processing();
        std::cout << "Studio Pan Delay tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        count_allocations.store(false, std::memory_order_relaxed);
        std::cerr << "Studio Pan Delay test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
