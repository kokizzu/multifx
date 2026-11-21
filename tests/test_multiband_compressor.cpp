#include "tabletfx/effects/multiband_compressor.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
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

tabletfx::MultibandCompressorParameters neutral_parameters() {
    tabletfx::MultibandCompressorParameters parameters;
    for (auto& band : parameters.bands) {
        band.ratio = 1.0F;
        band.level_db = 0.0F;
        band.mute = false;
    }
    return parameters;
}

float measure_sine_gain(tabletfx::MultibandCrossoverSlope slope,
                        float frequency_hz,
                        float low_mid_crossover_hz = 250.0F,
                        float mid_high_crossover_hz = 2500.0F) {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frame_count = 96000;
    constexpr std::size_t measure_frames = 24000;
    auto parameters = neutral_parameters();
    parameters.low_mid_crossover_hz = low_mid_crossover_hz;
    parameters.mid_high_crossover_hz = mid_high_crossover_hz;
    parameters.crossover_slope = slope;
    tabletfx::MultibandCompressor compressor(sample_rate, parameters);
    std::vector<float> samples(frame_count);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        samples[frame] = 0.3F * std::sin(
            2.0F * std::numbers::pi_v<float> * frequency_hz *
            static_cast<float>(frame) / sample_rate);
    }
    compressor.process(samples, 1);

    double input_power = 0.0;
    double output_power = 0.0;
    const std::size_t begin = frame_count - measure_frames;
    for (std::size_t frame = begin; frame < frame_count; ++frame) {
        const double input = 0.3 * std::sin(
            2.0 * std::numbers::pi * static_cast<double>(frequency_hz) *
            static_cast<double>(frame) / static_cast<double>(sample_rate));
        input_power += input * input;
        output_power += static_cast<double>(samples[frame]) * samples[frame];
    }
    return static_cast<float>(std::sqrt(output_power / input_power));
}

float measure_isolated_low_band_gain(tabletfx::MultibandCrossoverSlope slope,
                                     float frequency_hz) {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frame_count = 96000;
    constexpr std::size_t measure_frames = 24000;
    auto parameters = neutral_parameters();
    parameters.low_mid_crossover_hz = 500.0F;
    parameters.mid_high_crossover_hz = 10000.0F;
    parameters.crossover_slope = slope;
    parameters.bands[1].mute = true;
    parameters.bands[2].mute = true;
    tabletfx::MultibandCompressor compressor(sample_rate, parameters);
    std::vector<float> samples(frame_count);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        samples[frame] = 0.3F * std::sin(
            2.0F * std::numbers::pi_v<float> * frequency_hz *
            static_cast<float>(frame) / sample_rate);
    }
    compressor.process(samples, 1);

    double input_power = 0.0;
    double output_power = 0.0;
    const std::size_t begin = frame_count - measure_frames;
    for (std::size_t frame = begin; frame < frame_count; ++frame) {
        const double input = 0.3 * std::sin(
            2.0 * std::numbers::pi * static_cast<double>(frequency_hz) *
            static_cast<double>(frame) / static_cast<double>(sample_rate));
        input_power += input * input;
        output_power += static_cast<double>(samples[frame]) * samples[frame];
    }
    return static_cast<float>(std::sqrt(output_power / input_power));
}

void test_neutral_crossovers_recombine_without_a_large_level_dip() {
    constexpr std::array<float, 9> frequencies{
        45.0F, 120.0F, 250.0F, 600.0F, 1500.0F,
        2500.0F, 5000.0F, 10000.0F, 18000.0F};
    constexpr std::array slopes{tabletfx::MultibandCrossoverSlope::Db12,
                                tabletfx::MultibandCrossoverSlope::Db24};
    for (const auto slope : slopes) {
        for (const float frequency : frequencies) {
            const float gain = measure_sine_gain(slope, frequency);
            require(gain > 0.97F && gain < 1.03F,
                    "neutral three-band crossover altered the summed level");
        }
        for (const float frequency :
             std::array<float, 5>{700.0F, 1000.0F, 1200.0F, 1500.0F,
                                  2200.0F}) {
            const float gain = measure_sine_gain(slope, frequency, 1000.0F,
                                                 1500.0F);
            require(gain > 0.97F && gain < 1.03F,
                    "closely spaced neutral crossovers altered the summed level");
        }
    }
}

void test_crossover_slope_matches_the_selected_octave_rate() {
    const float twelve_db_low = measure_isolated_low_band_gain(
        tabletfx::MultibandCrossoverSlope::Db12, 2000.0F);
    const float twelve_db_high = measure_isolated_low_band_gain(
        tabletfx::MultibandCrossoverSlope::Db12, 4000.0F);
    const float twenty_four_db_low = measure_isolated_low_band_gain(
        tabletfx::MultibandCrossoverSlope::Db24, 2000.0F);
    const float twenty_four_db_high = measure_isolated_low_band_gain(
        tabletfx::MultibandCrossoverSlope::Db24, 4000.0F);
    const float twelve_db_octave_ratio = twelve_db_high / twelve_db_low;
    const float twenty_four_db_octave_ratio =
        twenty_four_db_high / twenty_four_db_low;
    require(twelve_db_octave_ratio > 0.20F &&
                twelve_db_octave_ratio < 0.34F,
            "Db12 crossover does not attenuate by about 12 dB per octave");
    require(twenty_four_db_octave_ratio > 0.04F &&
                twenty_four_db_octave_ratio < 0.10F,
            "Db24 crossover does not attenuate by about 24 dB per octave");
}

void test_low_band_hard_knee_ratio() {
    auto parameters = neutral_parameters();
    parameters.low_mid_crossover_hz = 800.0F;
    parameters.mid_high_crossover_hz = 5000.0F;
    auto& low = parameters.bands[0];
    low.threshold_db = -24.0F;
    low.ratio = 4.0F;
    low.knee_db = 0.0F;
    low.attack_ms = 0.1F;
    low.detector = tabletfx::MultibandDetector::Peak;
    tabletfx::MultibandCompressor compressor(48000.0F, parameters);

    constexpr std::size_t frame_count = 48000;
    std::vector<float> samples(frame_count);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        samples[frame] = 0.5F * std::sin(
            2.0F * std::numbers::pi_v<float> * 50.0F *
            static_cast<float>(frame) / 48000.0F);
    }
    compressor.process(samples, 1);

    double output_power = 0.0;
    for (std::size_t frame = frame_count - 12000; frame < frame_count; ++frame) {
        output_power += static_cast<double>(samples[frame]) * samples[frame];
    }
    const float output_rms = static_cast<float>(
        std::sqrt(output_power / 12000.0));
    require(output_rms > 0.07F && output_rms < 0.14F,
            "low-band compression did not approach the configured 4:1 transfer");
}

void test_stereo_link_and_band_level_mute() {
    auto parameters = neutral_parameters();
    parameters.low_mid_crossover_hz = 800.0F;
    parameters.mid_high_crossover_hz = 5000.0F;
    auto& low = parameters.bands[0];
    low.threshold_db = -24.0F;
    low.ratio = 4.0F;
    low.knee_db = 0.0F;
    low.attack_ms = 0.1F;
    low.release_ms = 20.0F;
    low.detector = tabletfx::MultibandDetector::Peak;
    tabletfx::MultibandCompressor compressor(48000.0F, parameters);

    constexpr std::size_t frame_count = 12000;
    std::vector<float> stereo(frame_count * 2);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        const float wave = std::sin(
            2.0F * std::numbers::pi_v<float> * 50.0F *
            static_cast<float>(frame) / 48000.0F);
        stereo[frame * 2] = 0.25F * wave;
        stereo[frame * 2 + 1] = 0.5F * wave;
    }
    compressor.process(stereo, 2);
    float maximum_relative_image_error = 0.0F;
    for (std::size_t frame = 1000; frame < frame_count; ++frame) {
        maximum_relative_image_error = std::max(
            maximum_relative_image_error,
            std::abs(stereo[frame * 2 + 1] - 2.0F * stereo[frame * 2]));
    }
    require(maximum_relative_image_error < 2.0e-4F,
            "stereo-linked band gain did not preserve the relative image");

    low.ratio = 1.0F;
    low.level_db = 6.0F;
    tabletfx::MultibandCompressor levelled(48000.0F, parameters);
    std::vector<float> low_samples(12000);
    for (std::size_t frame = 0; frame < low_samples.size(); ++frame) {
        low_samples[frame] = 0.25F * std::sin(
            2.0F * std::numbers::pi_v<float> * 50.0F *
            static_cast<float>(frame) / 48000.0F);
    }
    levelled.process(low_samples, 1);
    double level_power = 0.0;
    double source_power = 0.0;
    for (std::size_t frame = low_samples.size() - 2000;
         frame < low_samples.size(); ++frame) {
        level_power += static_cast<double>(low_samples[frame]) *
                       low_samples[frame];
        const double source = 0.25 * std::sin(
            2.0 * std::numbers::pi * 50.0 * static_cast<double>(frame) /
            48000.0);
        source_power += source * source;
    }
    require(std::abs(std::sqrt(level_power / source_power) - 1.995262F) < 0.02F,
            "per-band Level control did not apply its dB gain");

    low.mute = true;
    tabletfx::MultibandCompressor muted(48000.0F, parameters);
    std::vector<float> sustained_low(12000);
    for (std::size_t frame = 0; frame < sustained_low.size(); ++frame) {
        sustained_low[frame] = std::sin(
            2.0F * std::numbers::pi_v<float> * 50.0F *
            static_cast<float>(frame) / 48000.0F);
    }
    muted.process(sustained_low, 1);
    double residual_power = 0.0;
    for (std::size_t frame = sustained_low.size() - 2000;
         frame < sustained_low.size(); ++frame) {
        residual_power += static_cast<double>(sustained_low[frame]) *
                          sustained_low[frame];
    }
    require(std::sqrt(residual_power / 2000.0) < 0.01,
            "muting the low band did not remove a low-frequency sine");
}

void test_rms_and_peak_detector_modes_differ() {
    auto peak_parameters = neutral_parameters();
    peak_parameters.low_mid_crossover_hz = 800.0F;
    peak_parameters.mid_high_crossover_hz = 5000.0F;
    auto& peak = peak_parameters.bands[0];
    peak.threshold_db = -12.0F;
    peak.ratio = 8.0F;
    peak.knee_db = 0.0F;
    peak.attack_ms = 0.1F;
    peak.detector = tabletfx::MultibandDetector::Peak;
    auto rms_parameters = peak_parameters;
    rms_parameters.bands[0].detector = tabletfx::MultibandDetector::Rms;
    tabletfx::MultibandCompressor peak_compressor(48000.0F, peak_parameters);
    tabletfx::MultibandCompressor rms_compressor(48000.0F, rms_parameters);

    constexpr std::size_t frame_count = 24000;
    std::vector<float> peak_samples(frame_count);
    std::vector<float> rms_samples(frame_count);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        const float sample = 0.5F * std::sin(
            2.0F * std::numbers::pi_v<float> * 50.0F *
            static_cast<float>(frame) / 48000.0F);
        peak_samples[frame] = sample;
        rms_samples[frame] = sample;
    }
    peak_compressor.process(peak_samples, 1);
    rms_compressor.process(rms_samples, 1);
    double peak_power = 0.0;
    double rms_power = 0.0;
    for (std::size_t frame = frame_count - 8000; frame < frame_count; ++frame) {
        peak_power += static_cast<double>(peak_samples[frame]) * peak_samples[frame];
        rms_power += static_cast<double>(rms_samples[frame]) * rms_samples[frame];
    }
    require(std::sqrt(peak_power) < 0.9 * std::sqrt(rms_power),
            "Peak and RMS detectors did not produce their expected distinct levels");
}

void test_defaults_bounds_and_invalid_input_recovery() {
    tabletfx::MultibandCompressor compressor;
    const auto defaults = compressor.parameters();
    require(defaults.low_mid_crossover_hz == 250.0F &&
                defaults.mid_high_crossover_hz == 2500.0F &&
                defaults.crossover_slope ==
                    tabletfx::MultibandCrossoverSlope::Db24 &&
                defaults.bands[0].threshold_db == -24.0F &&
                defaults.bands[0].ratio == 2.0F &&
                defaults.bands[0].knee_db == 6.0F &&
                defaults.bands[0].attack_ms == 10.0F &&
                defaults.bands[0].release_ms == 100.0F &&
                defaults.bands[0].level_db == 0.0F,
            "project-authored compressor defaults changed unexpectedly");
    require(compressor.memory_bytes() == sizeof(compressor),
            "compressor state should remain fixed in-object");

    auto extreme = defaults;
    extreme.low_mid_crossover_hz = -1.0F;
    extreme.mid_high_crossover_hz = 100000.0F;
    extreme.bands[0].threshold_db = -100.0F;
    extreme.bands[0].ratio = 100.0F;
    extreme.bands[0].knee_db = 100.0F;
    extreme.bands[0].attack_ms = 0.0F;
    extreme.bands[0].release_ms = 0.0F;
    extreme.bands[0].level_db = 100.0F;
    extreme.bands[0].detector = static_cast<tabletfx::MultibandDetector>(99);
    extreme.crossover_slope =
        static_cast<tabletfx::MultibandCrossoverSlope>(99);
    compressor.configure(8000.0F, extreme);
    const auto bounded = compressor.parameters();
    require(bounded.low_mid_crossover_hz == 50.0F &&
                bounded.mid_high_crossover_hz <= 3200.0F &&
                bounded.bands[0].threshold_db == -60.0F &&
                bounded.bands[0].ratio == 20.0F &&
                bounded.bands[0].knee_db == 18.0F &&
                bounded.bands[0].attack_ms == 0.1F &&
                bounded.bands[0].release_ms == 5.0F &&
                bounded.bands[0].level_db == 12.0F &&
                bounded.bands[0].detector == tabletfx::MultibandDetector::Rms &&
                bounded.crossover_slope ==
                    tabletfx::MultibandCrossoverSlope::Db24,
            "project-authored parameter bounds were not applied");

    std::array<float, 5> bad_samples{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(), 0.25F};
    compressor.process(bad_samples, 1);
    for (const float sample : bad_samples) {
        require(std::isfinite(sample), "non-finite output escaped the processor");
    }
    require(bad_samples[0] == 0.0F && bad_samples[1] == 0.0F &&
                bad_samples[2] == 0.0F,
            "non-finite input frames were not silenced during recovery");

    std::array<float, 2> unsupported_channels{0.2F, -0.2F};
    compressor.process(unsupported_channels, 3);
    require(unsupported_channels[0] == 0.2F &&
                unsupported_channels[1] == -0.2F,
            "unsupported channel layouts should remain untouched");
}

void test_variable_blocks_and_process_does_not_allocate() {
    auto parameters = neutral_parameters();
    parameters.low_mid_crossover_hz = 300.0F;
    parameters.mid_high_crossover_hz = 3000.0F;
    tabletfx::MultibandCompressor whole(44100.0F, parameters);
    tabletfx::MultibandCompressor partitioned(44100.0F, parameters);
    std::vector<float> source(997 * 2);
    for (std::size_t frame = 0; frame < source.size() / 2; ++frame) {
        const float sample = 0.75F * std::sin(0.013F * static_cast<float>(frame));
        source[frame * 2] = sample;
        source[frame * 2 + 1] = -0.5F * sample;
    }
    auto contiguous = source;
    auto split = source;
    whole.process(contiguous, 2);

    constexpr std::array<std::size_t, 7> block_frames{1, 7, 31, 2, 113, 19, 5};
    std::size_t frame_offset = 0;
    std::size_t block_index = 0;
    while (frame_offset < split.size() / 2) {
        const std::size_t count = std::min(
            block_frames[block_index % block_frames.size()],
            split.size() / 2 - frame_offset);
        partitioned.process(
            std::span<float>(split.data() + frame_offset * 2, count * 2), 2);
        frame_offset += count;
        ++block_index;
    }
    for (std::size_t index = 0; index < split.size(); ++index) {
        require(std::abs(split[index] - contiguous[index]) < 1.0e-6F,
                "variable block partition changed compressor output");
    }

    std::array<float, 1> mono{0.1F};
    std::array<float, 8> stereo{};
    stereo.fill(0.2F);
    partitioned.process(mono, 1);
    partitioned.process(stereo, 2);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        partitioned.process(mono, 1);
        partitioned.process(stereo, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "process() allocated memory");
    for (const float sample : stereo) {
        require(std::isfinite(sample), "processor emitted non-finite audio");
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
        test_neutral_crossovers_recombine_without_a_large_level_dip();
        test_crossover_slope_matches_the_selected_octave_rate();
        test_low_band_hard_knee_ratio();
        test_stereo_link_and_band_level_mute();
        test_rms_and_peak_detector_modes_differ();
        test_defaults_bounds_and_invalid_input_recovery();
        test_variable_blocks_and_process_does_not_allocate();
        std::cout << "All TabletFX multiband compressor tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
