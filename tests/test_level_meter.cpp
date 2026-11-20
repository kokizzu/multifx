#include "tabletfx/level_meter.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_near(float actual, float expected, float tolerance, const char* message) {
    if (std::abs(actual - expected) > tolerance) throw std::runtime_error(message);
}

void test_peak_rms_and_channel_layout() {
    tabletfx::LevelMeter meter;
    const std::array<float, 4> mono{0.5F, -0.5F, 0.25F, -0.25F};
    meter.process(mono, 1);
    const tabletfx::MeterReading reading = meter.snapshot();
    require_near(reading.peak, 0.5F, 1.0e-6F, "mono peak was not measured");
    require_near(reading.rms, std::sqrt(0.15625F), 1.0e-6F,
                 "mono RMS was not measured");
    require(reading.frames == 4 && reading.channels == 1,
            "mono block dimensions were not reported");
    require(!reading.block_clipped && !reading.clip_latched,
            "sub-threshold audio reported clipping");

    const std::array<float, 4> stereo{1.1F, -0.25F, 0.25F, 0.25F};
    meter.process(stereo, 2);
    const tabletfx::MeterReading stereo_reading = meter.snapshot();
    require_near(stereo_reading.peak, 1.1F, 1.0e-6F,
                 "stereo peak did not include both channels");
    require_near(stereo_reading.rms, std::sqrt(1.3975F / 4.0F), 1.0e-6F,
                 "stereo RMS was not measured across channels");
    require(stereo_reading.frames == 2 && stereo_reading.channels == 2,
            "stereo block dimensions were not reported");
    require(stereo_reading.block_clipped && stereo_reading.clip_latched &&
                stereo_reading.clipped_blocks == 1,
            "clipped block was not detected and latched");
    require(stereo_reading.blocks == 2, "block count was not advanced");

    const std::array<float, 2> quiet{0.1F, -0.1F};
    meter.process(quiet, 2);
    const tabletfx::MeterReading quiet_reading = meter.snapshot();
    require(!quiet_reading.block_clipped && quiet_reading.clip_latched,
            "clip latch did not persist independently of the current block");
    meter.clear_clip_latch();
    require(!meter.snapshot().clip_latched, "clip latch did not clear");
}

void test_non_finite_samples_and_invalid_layout() {
    tabletfx::LevelMeter meter;
    const std::array<float, 4> samples{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 0.25F, -0.25F};
    meter.process(samples, 2);
    const tabletfx::MeterReading reading = meter.snapshot();
    require_near(reading.peak, 0.25F, 1.0e-6F,
                 "non-finite values contaminated the measured peak");
    require_near(reading.rms, std::sqrt(0.03125F), 1.0e-6F,
                 "non-finite samples were not treated as silence in RMS");
    require(reading.invalid_samples == 2,
            "non-finite sample count was not reported for the block");

    meter.process(samples, 0);
    const tabletfx::MeterReading invalid_layout = meter.snapshot();
    require(invalid_layout.frames == 0 && invalid_layout.channels == 0 &&
                invalid_layout.peak == 0.0F && invalid_layout.rms == 0.0F,
            "zero-channel input was not handled as an empty block");
}

void test_reset_and_no_allocations() {
    tabletfx::LevelMeter meter;
    require(meter.memory_bytes() == sizeof(meter), "meter memory report changed");
    const std::array<float, 8> samples{0.2F, -0.4F, 0.6F, -0.8F,
                                       0.1F, -0.2F, 0.3F, -0.4F};
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int i = 0; i < 100; ++i) {
        meter.process(samples, 2);
        (void)meter.snapshot();
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "meter allocated during block measurement or snapshot");
    meter.reset();
    const tabletfx::MeterReading reset = meter.snapshot();
    require(reset.peak == 0.0F && reset.rms == 0.0F && reset.blocks == 0 &&
                reset.clipped_blocks == 0 && !reset.clip_latched,
            "meter reset did not clear measurements");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* pointer = std::malloc(size)) return pointer;
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
    try {
        test_peak_rms_and_channel_layout();
        test_non_finite_samples_and_invalid_layout();
        test_reset_and_no_allocations();
        std::cout << "All level-meter tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Level-meter test failure: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
