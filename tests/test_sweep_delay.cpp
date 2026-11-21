#include "tabletfx/effects.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void test_delay_time_dry_path_and_zero_mix() {
    tabletfx::SweepDelayParameters parameters;
    parameters.time_ms = 4.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.start_hz = 250.0F;
    parameters.stop_hz = 250.0F;
    tabletfx::SweepDelay delay(8000.0F, 1, 100.0F, parameters);

    std::array<float, 64> impulse{};
    impulse[0] = 0.8F;
    delay.process(impulse, 1);
    require(std::all_of(impulse.begin(), impulse.begin() + 32,
                        [](float sample) { return sample == 0.0F; }),
            "100% wet Sweep Delay must not leak dry input before its delay time");
    require(impulse[32] > 0.0F && impulse[32] < 0.8F,
            "Sweep Delay must produce its filtered repeat at the configured time");
    require(delay.id() == "sweep-delay" && delay.memory_bytes() > sizeof(delay),
            "Sweep Delay identity or memory accounting is incorrect");

    parameters.mix = 0.25F;
    tabletfx::SweepDelay mixed(8000.0F, 1, 100.0F, parameters);
    std::array<float, 64> dry_and_wet{};
    dry_and_wet[0] = 0.8F;
    mixed.process(dry_and_wet, 1);
    require(std::abs(dry_and_wet[0] - 0.6F) < 1.0e-6F,
            "Sweep Delay must retain an immediate 75% dry path at 25% mix");

    parameters.mix = 0.0F;
    tabletfx::SweepDelay bypassed(8000.0F, 1, 100.0F, parameters);
    std::array<float, 64> unchanged{};
    unchanged[0] = 0.2F;
    unchanged[17] = -0.4F;
    const auto original = unchanged;
    bypassed.process(unchanged, 1);
    require(unchanged == original,
            "zero-mix Sweep Delay must pass the direct signal through unchanged");
}

std::vector<float> render_stereo_sweep(tabletfx::SweepDelayWaveform waveform,
                                       float phase_degrees) {
    constexpr float sample_rate = 8000.0F;
    constexpr std::size_t frames = 4096;
    tabletfx::SweepDelayParameters parameters;
    parameters.time_ms = 2.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.start_hz = 120.0F;
    parameters.stop_hz = 3200.0F;
    parameters.resonance_q = 1.1F;
    parameters.rate_hz = 5.0F;
    parameters.waveform = waveform;
    parameters.phase_degrees = phase_degrees;
    tabletfx::SweepDelay delay(sample_rate, 2, 100.0F, parameters);

    std::vector<float> audio(frames * 2U);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float sample = 0.5F * std::sin(
            6.2831853071795864769F * 1000.0F *
            static_cast<float>(frame) / sample_rate);
        audio[frame * 2U] = sample;
        audio[frame * 2U + 1U] = sample;
    }
    delay.process(audio, 2);
    return audio;
}

void test_waveforms_and_stereo_phase_difference() {
    const auto sine = render_stereo_sweep(tabletfx::SweepDelayWaveform::sine,
                                          180.0F);
    const auto triangle = render_stereo_sweep(
        tabletfx::SweepDelayWaveform::triangle, 180.0F);
    const auto square = render_stereo_sweep(
        tabletfx::SweepDelayWaveform::square, 180.0F);
    require(sine.size() == triangle.size() && triangle.size() == square.size(),
            "Sweep Delay waveform renders have inconsistent lengths");
    double stereo_difference = 0.0;
    double sine_triangle_difference = 0.0;
    double triangle_square_difference = 0.0;
    for (std::size_t frame = 64; frame < sine.size() / 2U; ++frame) {
        stereo_difference += std::abs(
            static_cast<double>(sine[frame * 2U]) -
            static_cast<double>(sine[frame * 2U + 1U]));
        sine_triangle_difference += std::abs(
            static_cast<double>(sine[frame * 2U]) -
            static_cast<double>(triangle[frame * 2U]));
        triangle_square_difference += std::abs(
            static_cast<double>(triangle[frame * 2U]) -
            static_cast<double>(square[frame * 2U]));
    }
    require(stereo_difference > 1.0,
            "stereo phase offset must produce different left/right filter sweeps");
    require(sine_triangle_difference > 1.0 && triangle_square_difference > 1.0,
            "project-defined Sweep Delay waveforms must produce distinct sweeps");
    require(std::all_of(sine.begin(), sine.end(), [](float sample) {
                return std::isfinite(sample);
            }) &&
            std::all_of(triangle.begin(), triangle.end(), [](float sample) {
                return std::isfinite(sample);
            }) &&
            std::all_of(square.begin(), square.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "Sweep Delay waveform output must remain finite");
}

void test_controls_are_bounded_nonfinite_audio_and_reset() {
    tabletfx::SweepDelay delay;
    auto parameters = delay.parameters();
    parameters.time_ms = std::numeric_limits<float>::quiet_NaN();
    parameters.feedback = 3.0F;
    parameters.mix = -1.0F;
    parameters.start_hz = -50.0F;
    parameters.stop_hz = 100000.0F;
    parameters.resonance_q = 50.0F;
    parameters.rate_hz = std::numeric_limits<float>::infinity();
    parameters.phase_degrees = 500.0F;
    parameters.waveform = static_cast<tabletfx::SweepDelayWaveform>(255U);
    delay.configure(parameters);
    const auto bounded = delay.parameters();
    require(bounded.time_ms == 350.0F && bounded.feedback == 0.95F &&
                bounded.mix == 0.0F && bounded.start_hz == 20.0F &&
                bounded.stop_hz <= 18000.0F && bounded.resonance_q == 12.0F &&
                bounded.rate_hz == 0.5F && bounded.phase_degrees == 360.0F &&
                bounded.waveform == tabletfx::SweepDelayWaveform::sine,
            "Sweep Delay must bound controls and replace non-finite values safely");

    auto playable = bounded;
    playable.time_ms = 4.0F;
    playable.feedback = 0.0F;
    playable.mix = 1.0F;
    tabletfx::SweepDelay resettable(8000.0F, 2, 100.0F, playable);
    std::array<float, 256> with_invalid{};
    with_invalid[0] = 0.4F;
    with_invalid[1] = -0.2F;
    with_invalid[2] = std::numeric_limits<float>::quiet_NaN();
    with_invalid[3] = std::numeric_limits<float>::infinity();
    resettable.process(with_invalid, 2);
    require(std::all_of(with_invalid.begin(), with_invalid.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "Sweep Delay must replace non-finite input with finite output");

    resettable.reset();
    std::array<float, 128> silence{};
    resettable.process(silence, 2);
    require(std::all_of(silence.begin(), silence.end(), [](float sample) {
                return sample == 0.0F;
            }),
            "reset must clear delay history and swept-filter state");
    require(resettable.parameters().time_ms == playable.time_ms &&
                resettable.parameters().mix == playable.mix,
            "reset must preserve selected Sweep Delay parameters");
}

void test_process_does_not_allocate() {
    tabletfx::SweepDelay delay;
    std::array<float, 512> block{};
    block.fill(0.25F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        delay.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "Sweep Delay allocated memory during process()");
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
        test_delay_time_dry_path_and_zero_mix();
        test_waveforms_and_stereo_phase_difference();
        test_controls_are_bounded_nonfinite_audio_and_reset();
        test_process_does_not_allocate();
        std::cout << "Sweep Delay tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Sweep Delay test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
