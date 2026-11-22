#include "tabletfx/effects/de_esser.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>

namespace {

std::atomic<std::size_t> allocation_count{0U};
std::atomic<bool> track_allocations{false};

constexpr float kSampleRate = 48000.0F;
constexpr double kPi = 3.14159265358979323846;

void fill_sine(std::span<float> samples, std::uint16_t channels,
               std::uint16_t active_channel, double frequency_hz,
               double amplitude) {
    const std::size_t frames = samples.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        samples[frame * channels + active_channel] = static_cast<float>(
            amplitude * std::sin(2.0 * kPi * frequency_hz *
                                 static_cast<double>(frame) / kSampleRate));
    }
}

double rms(std::span<const float> samples, std::uint16_t channels,
           std::uint16_t channel, std::size_t start_frame) {
    const std::size_t frames = samples.size() / channels;
    double sum = 0.0;
    std::size_t count = 0U;
    for (std::size_t frame = start_frame; frame < frames; ++frame) {
        const double value = samples[frame * channels + channel];
        sum += value * value;
        ++count;
    }
    return count == 0U ? 0.0 : std::sqrt(sum / static_cast<double>(count));
}

void require_finite(std::span<const float> samples) {
    assert(std::all_of(samples.begin(), samples.end(), [](float sample) {
        return std::isfinite(sample);
    }));
}

void test_defaults_bounds_and_invalid_parameters() {
    using namespace tabletfx::effects;
    DeEsser effect;
    assert(effect.id() == "de-esser");
    assert(effect.sample_rate() == kSampleRate);
    assert(effect.channels() == 2U);
    assert(effect.memory_bytes() == sizeof(effect));
    assert(effect.parameters().detector_frequency_hz == 6500.0F);
    assert(effect.parameters().detector_q == 1.5F);
    assert(effect.parameters().threshold_dbfs == -30.0F);
    assert(effect.parameters().max_reduction_db == 12.0F);
    assert(effect.parameters().attack_ms == 2.0F);
    assert(effect.parameters().release_ms == 80.0F);

    auto parameters = effect.parameters();
    parameters.detector_frequency_hz = -10.0F;
    parameters.detector_q = 100.0F;
    parameters.threshold_dbfs = -100.0F;
    parameters.max_reduction_db = 100.0F;
    parameters.attack_ms = -1.0F;
    parameters.release_ms = 1000.0F;
    effect.configure(parameters);
    assert(effect.parameters().detector_frequency_hz ==
           DeEsser::kMinimumDetectorFrequencyHz);
    assert(effect.parameters().detector_q == DeEsser::kMaximumDetectorQ);
    assert(effect.parameters().threshold_dbfs == DeEsser::kMinimumThresholdDbfs);
    assert(effect.parameters().max_reduction_db == DeEsser::kMaximumReductionDb);
    assert(effect.parameters().attack_ms == DeEsser::kMinimumAttackMs);
    assert(effect.parameters().release_ms == DeEsser::kMaximumReleaseMs);

    parameters.detector_frequency_hz = 100000.0F;
    parameters.detector_q = 0.01F;
    parameters.threshold_dbfs = 10.0F;
    parameters.max_reduction_db = -1.0F;
    parameters.attack_ms = 100.0F;
    parameters.release_ms = 0.0F;
    effect.configure(parameters);
    assert(effect.parameters().detector_frequency_hz ==
           DeEsser::kMaximumDetectorFrequencyHz);
    assert(effect.parameters().detector_q == DeEsser::kMinimumDetectorQ);
    assert(effect.parameters().threshold_dbfs == DeEsser::kMaximumThresholdDbfs);
    assert(effect.parameters().max_reduction_db == DeEsser::kMinimumReductionDb);
    assert(effect.parameters().attack_ms == DeEsser::kMaximumAttackMs);
    assert(effect.parameters().release_ms == DeEsser::kMinimumReleaseMs);

    parameters.detector_frequency_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.detector_q = std::numeric_limits<float>::infinity();
    parameters.threshold_dbfs = std::numeric_limits<float>::quiet_NaN();
    parameters.max_reduction_db = std::numeric_limits<float>::infinity();
    parameters.attack_ms = std::numeric_limits<float>::quiet_NaN();
    parameters.release_ms = std::numeric_limits<float>::infinity();
    effect.configure(parameters);
    assert(effect.parameters().detector_frequency_hz == 6500.0F);
    assert(effect.parameters().detector_q == 1.5F);
    assert(effect.parameters().threshold_dbfs == -30.0F);
    assert(effect.parameters().max_reduction_db == 12.0F);
    assert(effect.parameters().attack_ms == 2.0F);
    assert(effect.parameters().release_ms == 80.0F);

    DeEsser low_rate(8000.0F, 1U);
    auto low_rate_parameters = low_rate.parameters();
    low_rate_parameters.detector_frequency_hz = 12000.0F;
    low_rate.configure(low_rate_parameters);
    assert(low_rate.parameters().detector_frequency_hz == 3600.0F);
    low_rate_parameters.detector_frequency_hz =
        std::numeric_limits<float>::quiet_NaN();
    low_rate.configure(low_rate_parameters);
    assert(low_rate.parameters().detector_frequency_hz == 3600.0F);

    bool rejected_channels = false;
    try {
        DeEsser unsupported(kSampleRate, 3U);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    assert(rejected_channels);

    bool rejected_sample_rate = false;
    try {
        DeEsser unsupported(7999.0F, 1U);
    } catch (const std::invalid_argument&) {
        rejected_sample_rate = true;
    }
    assert(rejected_sample_rate);
}

void test_reduces_sibilance_band_and_retains_low_frequencies() {
    using namespace tabletfx::effects;
    constexpr std::size_t frames = 16384U;
    constexpr std::size_t analysis_start = 8192U;
    std::array<float, frames> high{};
    std::array<float, frames> low{};
    fill_sine(high, 1U, 0U, 6500.0, 0.25);
    fill_sine(low, 1U, 0U, 180.0, 0.25);
    const auto high_original = high;
    const auto low_original = low;

    DeEsserParameters parameters;
    parameters.threshold_dbfs = -36.0F;
    parameters.max_reduction_db = 18.0F;
    DeEsser effect(kSampleRate, 1U, parameters);
    effect.process(high, 1U);
    effect.reset();
    effect.process(low, 1U);
    require_finite(high);
    require_finite(low);

    const double high_ratio = rms(high, 1U, 0U, analysis_start) /
                              rms(high_original, 1U, 0U, analysis_start);
    const double low_ratio = rms(low, 1U, 0U, analysis_start) /
                             rms(low_original, 1U, 0U, analysis_start);
    assert(high_ratio < 0.5);
    assert(low_ratio > 0.95);
}

void test_stereo_channels_are_independent() {
    using namespace tabletfx::effects;
    constexpr std::size_t frames = 16384U;
    constexpr std::size_t analysis_start = 8192U;
    std::array<float, frames * 2U> stereo{};
    fill_sine(stereo, 2U, 0U, 6500.0, 0.25);
    fill_sine(stereo, 2U, 1U, 180.0, 0.25);
    const auto original = stereo;

    DeEsserParameters parameters;
    parameters.threshold_dbfs = -36.0F;
    parameters.max_reduction_db = 18.0F;
    DeEsser effect(kSampleRate, 2U, parameters);
    effect.process(stereo, 2U);
    require_finite(stereo);
    const double high_ratio = rms(stereo, 2U, 0U, analysis_start) /
                              rms(original, 2U, 0U, analysis_start);
    const double low_ratio = rms(stereo, 2U, 1U, analysis_start) /
                             rms(original, 2U, 1U, analysis_start);
    assert(high_ratio < 0.5);
    assert(low_ratio > 0.95);
}

void test_reset_invalid_samples_and_channel_mismatch() {
    using namespace tabletfx::effects;
    DeEsser effect(kSampleRate, 1U);
    std::array<float, 3U> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
    };
    effect.process(invalid, 1U);
    assert(std::all_of(invalid.begin(), invalid.end(), [](float sample) {
        return sample == 0.0F;
    }));

    std::array<float, 3U> huge{
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        0.25F,
    };
    effect.process(huge, 1U);
    require_finite(huge);

    std::array<float, 8U> wrong_layout{};
    wrong_layout.fill(0.2F);
    const auto before = wrong_layout;
    effect.process(wrong_layout, 2U);
    assert(wrong_layout == before);

    // A stale detector envelope must not attenuate a quiet signal after reset.
    std::array<float, 4096U> loud{};
    fill_sine(loud, 1U, 0U, 6500.0, 0.25);
    effect.process(loud, 1U);
    effect.reset();
    std::array<float, 4096U> quiet{};
    fill_sine(quiet, 1U, 0U, 6500.0, 0.0001);
    const auto quiet_original = quiet;
    effect.process(quiet, 1U);
    require_finite(quiet);
    const double quiet_ratio = rms(quiet, 1U, 0U, 1024U) /
                               rms(quiet_original, 1U, 0U, 1024U);
    assert(quiet_ratio > 0.99);
}

void test_processing_does_not_allocate() {
    using namespace tabletfx::effects;
    DeEsser effect(kSampleRate, 2U);
    std::array<float, 2048U> stereo{};
    fill_sine(stereo, 2U, 0U, 6500.0, 0.2);
    fill_sine(stereo, 2U, 1U, 180.0, 0.2);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    track_allocations.store(true, std::memory_order_relaxed);
    effect.process(stereo, 2U);
    track_allocations.store(false, std::memory_order_relaxed);
    assert(allocation_count.load(std::memory_order_relaxed) == before);
}

} // namespace

void* operator new(std::size_t size) {
    if (track_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (track_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    test_defaults_bounds_and_invalid_parameters();
    test_reduces_sibilance_band_and_retains_low_frequencies();
    test_stereo_channels_are_independent();
    test_reset_invalid_samples_and_channel_mismatch();
    test_processing_does_not_allocate();
}
