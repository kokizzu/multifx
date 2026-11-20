#include "tabletfx/effects.hpp"
#include "tabletfx/wav.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
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

void test_gain() {
    tabletfx::Gain gain(-6.0F);
    std::vector<float> samples{1.0F, -1.0F, 0.5F, -0.5F};
    gain.process(samples, 2);
    const float expected = std::pow(10.0F, -6.0F / 20.0F);
    require(std::abs(samples[0] - expected) < 1.0e-5F, "gain does not match dB value");
    require(std::abs(samples[1] + expected) < 1.0e-5F, "gain stereo channel mismatch");
}

void test_gate_and_drive_are_finite() {
    tabletfx::NoiseGate gate(48000.0F, -45.0F, -72.0F, 1.0F, 50.0F);
    tabletfx::SoftDrive drive(4.0F);
    std::vector<float> samples(4096);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<float>(std::sin(static_cast<double>(i) * 0.07) * 0.25);
    }
    gate.process(samples, 2);
    drive.process(samples, 2);
    for (float sample : samples) {
        require(std::isfinite(sample), "processor emitted a non-finite sample");
    }
}

void test_delay_impulse_position() {
    tabletfx::Delay delay(1000.0F, 1, 1000.0F, 2.0F, 0.0F, 1.0F);
    std::vector<float> samples(8, 0.0F);
    samples[0] = 1.0F;
    delay.process(samples, 1);
    require(std::abs(samples[2] - 1.0F) < 1.0e-6F, "delay impulse appeared at the wrong sample");
    for (std::size_t i : {std::size_t{0}, std::size_t{1}, std::size_t{3}, std::size_t{4}}) {
        require(std::abs(samples[i]) < 1.0e-6F, "unexpected delay output sample");
    }
}

void test_effect_processing_allocates_nothing() {
    tabletfx::Gain gain(3.0F);
    tabletfx::NoiseGate gate(48000.0F, -48.0F);
    tabletfx::SoftDrive drive(3.0F);
    tabletfx::Delay delay(48000.0F, 2, 2000.0F);
    require(delay.memory_bytes() >= sizeof(delay) + 96000U * 2U * sizeof(float),
            "delay memory estimate must include its owned delay line");
    std::vector<float> samples(256 * 2, 0.1F);
    const auto before = allocation_count.load(std::memory_order_relaxed);
    gain.process(samples, 2);
    gate.process(samples, 2);
    drive.process(samples, 2);
    delay.process(samples, 2);
    const auto after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "an effect allocated memory during process()");
}

void test_wav_float_round_trip() {
    const auto nonce = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / ("tabletfx-" + nonce + ".wav");
    const tabletfx::AudioBuffer input{48000, 2, {0.0F, 1.0F, -1.0F, 0.25F, -0.25F, 0.75F}};
    tabletfx::write_wav_float32(path, input);
    const tabletfx::AudioBuffer output = tabletfx::read_wav(path);
    std::filesystem::remove(path);
    require(output.sample_rate == input.sample_rate, "WAV sample rate did not round-trip");
    require(output.channels == input.channels, "WAV channel count did not round-trip");
    require(output.interleaved.size() == input.interleaved.size(), "WAV frame count did not round-trip");
    for (std::size_t i = 0; i < input.interleaved.size(); ++i) {
        require(output.interleaved[i] == input.interleaved[i], "WAV float sample changed during round-trip");
    }
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
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
        test_gain();
        test_gate_and_drive_are_finite();
        test_delay_impulse_position();
        test_effect_processing_allocates_nothing();
        test_wav_float_round_trip();
        std::cout << "All TabletFX offline DSP tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
