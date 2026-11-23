#include "tabletfx/effects/stereo_crossover.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

float channel_rms(const std::vector<float>& samples, std::size_t first_frame,
                  std::size_t last_frame, std::size_t channel,
                  std::size_t channels) {
    double sum = 0.0;
    for (std::size_t frame = first_frame; frame < last_frame; ++frame) {
        const double sample = samples[frame * channels + channel];
        sum += sample * sample;
    }
    return static_cast<float>(std::sqrt(
        sum / static_cast<double>(last_frame - first_frame)));
}

std::vector<float> render_stereo_sine(float frequency_hz,
                                      std::size_t frames = 48000) {
    constexpr float sample_rate = 48000.0F;
    std::vector<float> samples(frames * 2, 0.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        samples[frame * 2] = std::sin(
            2.0F * 3.14159265358979323846F * frequency_hz *
            static_cast<float>(frame) / sample_rate);
    }
    return samples;
}

void test_lr4_split_recombines_near_unity() {
    constexpr float frequencies[]{100.0F, 500.0F, 5000.0F};
    for (const float frequency : frequencies) {
        auto samples = render_stereo_sine(frequency);
        tabletfx::StereoCrossover crossover;
        crossover.process(samples, 2);
        const float input_rms = 0.70710678F;
        const float output_rms = channel_rms(samples, 4096, 45000, 0, 2);
        require(output_rms > input_rms * 0.97F &&
                    output_rms < input_rms * 1.03F,
                "default Linkwitz-Riley bands should sum near unity");
        require(channel_rms(samples, 4096, 45000, 1, 2) < 1.0e-4F,
                "default left input bands should remain routed left");
    }
}

void test_low_and_high_levels_select_their_bands() {
    tabletfx::StereoCrossoverParameters low_only;
    low_only.high_level_left_db = -60.0F;
    auto low_tone = render_stereo_sine(100.0F);
    tabletfx::StereoCrossover low_crossover(48000.0F, low_only);
    low_crossover.process(low_tone, 2);

    auto high_tone_low_only = render_stereo_sine(5000.0F);
    tabletfx::StereoCrossover high_tone_low_crossover(48000.0F, low_only);
    high_tone_low_crossover.process(high_tone_low_only, 2);

    tabletfx::StereoCrossoverParameters high_only;
    high_only.low_level_left_db = -60.0F;
    auto high_tone = render_stereo_sine(5000.0F);
    tabletfx::StereoCrossover high_crossover(48000.0F, high_only);
    high_crossover.process(high_tone, 2);

    auto low_tone_high_only = render_stereo_sine(100.0F);
    tabletfx::StereoCrossover low_tone_high_crossover(48000.0F, high_only);
    low_tone_high_crossover.process(low_tone_high_only, 2);

    const float low_level_output = channel_rms(low_tone, 4096, 45000, 0, 2);
    const float low_band_rejection = channel_rms(
        high_tone_low_only, 4096, 45000, 0, 2);
    const float high_level_output = channel_rms(
        high_tone, 4096, 45000, 0, 2);
    const float high_band_rejection = channel_rms(
        low_tone_high_only, 4096, 45000, 0, 2);
    require(low_level_output > 0.65F,
            "low-band level should retain a signal below the crossover");
    require(low_band_rejection < 0.01F,
            "muting the high band should reject a high-frequency input");
    require(high_level_output > 0.65F,
            "high-band level should retain a signal above the crossover");
    require(high_band_rejection < 0.01F,
            "muting the low band should reject a low-frequency input");
}

void test_band_pan_and_right_input_are_independent() {
    tabletfx::StereoCrossoverParameters parameters;
    parameters.low_pan_left = 1.0F;
    parameters.high_level_left_db = -60.0F;
    auto samples = render_stereo_sine(100.0F);
    tabletfx::StereoCrossover crossover(48000.0F, parameters);
    crossover.process(samples, 2);
    require(channel_rms(samples, 4096, 45000, 0, 2) < 1.0e-4F,
            "left low band should leave the left output when panned right");
    require(channel_rms(samples, 4096, 45000, 1, 2) > 0.65F,
            "left low band should reach the right output when panned right");

    tabletfx::StereoCrossoverParameters right_only;
    right_only.low_level_left_db = -60.0F;
    right_only.high_level_left_db = -60.0F;
    auto right = render_stereo_sine(100.0F);
    for (std::size_t frame = 0; frame < right.size() / 2; ++frame) {
        right[frame * 2] = 0.0F;
        right[frame * 2 + 1] = std::sin(
            2.0F * 3.14159265358979323846F * 100.0F *
            static_cast<float>(frame) / 48000.0F);
    }
    tabletfx::StereoCrossover right_crossover(48000.0F, right_only);
    right_crossover.process(right, 2);
    require(channel_rms(right, 4096, 45000, 1, 2) > 0.65F &&
                channel_rms(right, 4096, 45000, 0, 2) < 1.0e-4F,
            "right input levels and default pan should remain isolated right");
}

void test_frequency_multiplier_and_bounds() {
    tabletfx::StereoCrossoverParameters parameters;
    parameters.frequency_hz = 500.0F;
    parameters.frequency_multiplier =
        tabletfx::CrossoverFrequencyMultiplier::x10;
    tabletfx::StereoCrossover crossover(48000.0F, parameters);
    require(crossover.effective_frequency_hz() == 5000.0F,
            "x10 frequency multiplier should shift the crossover cutoff");

    parameters.frequency_hz = 2000.0F;
    parameters.frequency_multiplier =
        tabletfx::CrossoverFrequencyMultiplier::x10;
    tabletfx::StereoCrossover low_rate(8000.0F, parameters);
    require(low_rate.effective_frequency_hz() <= 3600.0F &&
                low_rate.parameters().frequency_hz <= 360.0F,
            "crossover frequency must stay below the sample-rate guard band");

    auto low_frequency_tone = render_stereo_sine(1000.0F);
    tabletfx::StereoCrossoverParameters low_cut_parameters;
    low_cut_parameters.high_level_left_db = -60.0F;
    tabletfx::StereoCrossover low_cut(48000.0F, low_cut_parameters);
    low_cut.process(low_frequency_tone, 2);
    const float lower_cut_output = channel_rms(
        low_frequency_tone, 4096, 45000, 0, 2);

    auto high_frequency_tone = render_stereo_sine(1000.0F);
    low_cut_parameters.frequency_hz = 500.0F;
    low_cut_parameters.frequency_multiplier =
        tabletfx::CrossoverFrequencyMultiplier::x10;
    tabletfx::StereoCrossover high_cut(48000.0F, low_cut_parameters);
    high_cut.process(high_frequency_tone, 2);
    const float higher_cut_output = channel_rms(
        high_frequency_tone, 4096, 45000, 0, 2);
    require(higher_cut_output > lower_cut_output,
            "raising the crossover should move a test tone into the low band");
}

void test_mono_reset_nonfinite_and_parameter_clamping() {
    tabletfx::StereoCrossover crossover;
    auto parameters = crossover.parameters();
    parameters.frequency_hz = 9000.0F;
    parameters.low_level_left_db = 40.0F;
    parameters.low_pan_left = 2.0F;
    parameters.balance = -3.0F;
    crossover.configure(parameters);
    require(crossover.parameters().frequency_hz == 2000.0F &&
                crossover.parameters().low_level_left_db == 12.0F &&
                crossover.parameters().low_pan_left == 1.0F &&
                crossover.parameters().balance == -1.0F,
            "project-authored controls should clamp to safe bounds");

    std::vector<float> mono(1024, 0.25F);
    mono[17] = std::numeric_limits<float>::quiet_NaN();
    crossover.process(mono, 1);
    require(std::all_of(mono.begin(), mono.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "mono processing must sanitize non-finite samples");

    std::vector<float> impulse(2048, 0.0F);
    impulse[0] = 1.0F;
    crossover.reset();
    crossover.process(impulse, 1);
    crossover.reset();
    std::vector<float> silence(256, 0.0F);
    crossover.process(silence, 1);
    require(std::all_of(silence.begin(), silence.end(), [](float sample) {
                return sample == 0.0F;
            }),
            "reset should clear all crossover filter states");
    require(crossover.id() == "stereo-crossover" &&
                crossover.memory_bytes() == sizeof(crossover),
            "crossover identity or fixed-state memory accounting is incorrect");
}

} // namespace

int main() {
    try {
        test_lr4_split_recombines_near_unity();
        test_low_and_high_levels_select_their_bands();
        test_band_pan_and_right_input_are_independent();
        test_frequency_multiplier_and_bounds();
        test_mono_reset_nonfinite_and_parameter_clamping();
    } catch (const std::exception& error) {
        std::cerr << "Stereo crossover test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
