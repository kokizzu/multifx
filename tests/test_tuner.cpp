#include "tabletfx/tuner.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

tabletfx::TunerReading feed_tone(tabletfx::ChromaticTuner& tuner, float frequency,
                                 float sample_rate, std::size_t max_blocks,
                                 std::uint16_t channels = 1) {
    double phase = 0.0;
    constexpr std::size_t block_frames = 128;
    std::vector<float> block(block_frames * channels);
    for (std::size_t block_index = 0; block_index < max_blocks; ++block_index) {
        for (std::size_t frame = 0; frame < block_frames; ++frame) {
            const float sample = 0.35F * static_cast<float>(std::sin(phase));
            phase += 2.0 * 3.14159265358979323846 * frequency / sample_rate;
            if (phase >= 2.0 * 3.14159265358979323846) {
                phase -= 2.0 * 3.14159265358979323846;
            }
            block[frame * channels] = sample;
            if (channels == 2) block[frame * channels + 1] = sample;
        }
        tuner.process(block, channels);
        const auto reading = tuner.reading();
        if (reading.generation != 0) return reading;
    }
    return tuner.reading();
}

void test_a4_frequency_note_and_cents() {
    tabletfx::ChromaticTuner tuner;
    require(tuner.reference_a4_hz() == 440.0F,
            "tuner A4 default should be 440 Hz");
    require(tuner.memory_bytes() == sizeof(tuner),
            "tuner fixed-memory report is incorrect");
    require(tuner.memory_bytes() < 12000,
            "tuner state exceeds its 12 KiB memory budget");
    require(tuner.analysis_window_ms() > 80.0F &&
                tuner.analysis_window_ms() < 90.0F,
            "guitar pitch window duration is outside expected bounds");

    const auto reading = feed_tone(tuner, 440.0F, 48000.0F, 100);
    require(reading.valid, "440 Hz tone was not detected");
    require(reading.midi_note == 69, "440 Hz did not map to A4/MIDI 69");
    require(std::abs(reading.frequency_hz - 440.0F) < 1.0F,
            "440 Hz estimate is outside the 1 Hz tolerance");
    require(std::abs(reading.cents) < 4.0F,
            "440 Hz tune offset is not close to zero cents");
    require(reading.confidence >= 0.60F,
            "valid reading confidence fell below the acceptance threshold");
}

void test_low_e_and_reference_adjustment() {
    tabletfx::ChromaticTuner tuner;
    const auto low_e = feed_tone(tuner, 82.4069F, 48000.0F, 100);
    require(low_e.valid && low_e.midi_note == 40,
            "low E2 guitar fundamental was not identified");
    require(std::abs(low_e.frequency_hz - 82.4069F) < 1.0F,
            "low E2 estimate is outside the 1 Hz tolerance");

    tuner.reset();
    tuner.set_reference_a4_hz(432.0F);
    const auto alternate = feed_tone(tuner, 432.0F, 48000.0F, 100);
    require(alternate.valid && alternate.midi_note == 69,
            "adjustable A4 reference did not preserve the expected note");
    require(std::abs(alternate.cents) < 4.0F,
            "adjusted A4 reference did not center its tuning readout");
    tuner.set_reference_a4_hz(1000.0F);
    require(tuner.reference_a4_hz() == 480.0F,
            "A4 reference upper bound failed");
    tuner.set_reference_a4_hz(-1.0F);
    require(tuner.reference_a4_hz() == 400.0F,
            "A4 reference lower bound failed");
    tuner.set_reference_a4_hz(std::numeric_limits<float>::quiet_NaN());
    require(tuner.reference_a4_hz() == 400.0F,
            "non-finite reference should preserve the last valid value");
}

void test_lowest_guitar_range_pitch() {
    tabletfx::ChromaticTuner tuner;
    const auto reading = feed_tone(tuner, 55.0F, 48000.0F, 120);
    require(reading.valid, "55 Hz lower-range tone was not detected");
    require(std::abs(reading.frequency_hz - 55.0F) < 1.0F,
            "55 Hz lower-range estimate is outside the 1 Hz tolerance");

    tuner.reset();
    const auto high = feed_tone(tuner, 1500.0F, 48000.0F, 120);
    if (!high.valid || std::abs(high.frequency_hz - 1500.0F) >= 4.0F) {
        std::cerr << "1500 Hz diagnostic: valid=" << high.valid
                  << " frequency=" << high.frequency_hz
                  << " confidence=" << high.confidence << '\n';
    }
    require(high.valid, "1500 Hz upper-range tone was not detected");
    require(std::abs(high.frequency_hz - 1500.0F) < 4.0F,
            "1500 Hz upper-range estimate is outside the 4 Hz tolerance");
}

void test_silence_and_invalid_channel_handling() {
    tabletfx::ChromaticTuner tuner;
    std::array<float, 128> silence{};
    tuner.process(silence, 1);
    tuner.process(silence, 0);
    require(tuner.reading().generation == 0,
            "invalid channel count should not start or publish a reading");
    const auto reading = feed_tone(tuner, 0.0F, 48000.0F, 500);
    require(!reading.valid,
            "silence should not produce a pitch reading");
}

void test_audible_and_muted_modes_ramp() {
    tabletfx::ChromaticTuner tuner;
    std::array<float, 480> audio{};
    audio.fill(0.25F);
    tuner.process(audio, 1);
    require(audio.front() == 0.25F && audio.back() == 0.25F,
            "audible mode altered the monitored audio");

    tuner.set_muted_output(true);
    audio.fill(0.25F);
    tuner.process(audio, 1);
    require(audio.front() < 0.25F && audio.front() > 0.24F,
            "muted mode did not begin with a short click-free ramp");
    require(audio.back() == 0.0F,
            "muted mode did not reach silence after 10 ms");
    require(tuner.muted_output(), "mute setting was not retained");

    tuner.set_muted_output(false);
    audio.fill(0.25F);
    tuner.process(audio, 1);
    require(audio.front() > 0.0F && audio.front() < 0.01F,
            "audible restore did not fade in from silence");
    require(std::abs(audio.back() - 0.25F) < 1.0e-6F,
            "audible restore did not return to unity passthrough");
}

void test_stereo_and_no_audio_thread_allocations() {
    tabletfx::ChromaticTuner tuner;
    auto reading = feed_tone(tuner, 329.6276F, 48000.0F, 500, 2);
    require(reading.valid && reading.midi_note == 64,
            "stereo B3 signal was not identified");

    std::array<float, 256> block{};
    block.fill(0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int index = 0; index < 100; ++index) {
        tuner.process(block, 2);
        (void)tuner.reading();
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(before == after, "tuner allocated in process or reading");
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
        test_a4_frequency_note_and_cents();
        test_low_e_and_reference_adjustment();
        test_lowest_guitar_range_pitch();
        test_silence_and_invalid_channel_handling();
        test_audible_and_muted_modes_ramp();
        test_stereo_and_no_audio_thread_allocations();
        std::cout << "Chromatic tuner tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
