#include "tabletfx/effects/stereo_filter.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> tone(float sample_rate, float frequency,
                        std::size_t frames) {
    std::vector<float> audio(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        audio[frame] = 0.25F * std::sin(
            2.0F * 3.14159265358979323846F * frequency *
            static_cast<float>(frame) / sample_rate);
    }
    return audio;
}

float rms(std::span<const float> audio, std::size_t begin) {
    double sum = 0.0;
    for (std::size_t index = begin; index < audio.size(); ++index) {
        const double sample = audio[index];
        sum += sample * sample;
    }
    return begin >= audio.size() ? 0.0F : static_cast<float>(std::sqrt(
        sum / static_cast<double>(audio.size() - begin)));
}

void test_defaults_bounds_and_memory() {
    tabletfx::StereoFilter filter;
    const auto defaults = filter.parameters();
    require(filter.id() == "stereo-filter", "stable effect ID changed");
    require(defaults.type == tabletfx::StereoFilterType::Lowpass &&
                defaults.frequency_hz == 1000.0F && defaults.order == 2U &&
                defaults.q == 0.707F && defaults.gain_db == 0.0F &&
                defaults.low_cut_hz == 0.0F && defaults.high_cut_hz == 0.0F &&
                !defaults.lfo_enabled,
            "project-authored defaults changed");
    require(filter.memory_bytes() >= sizeof(filter),
            "delay-line storage is missing from the memory report");

    tabletfx::StereoFilterParameters parameters;
    parameters.type = static_cast<tabletfx::StereoFilterType>(255U);
    parameters.frequency_hz = 100000.0F;
    parameters.order = 3U;
    parameters.q = -2.0F;
    parameters.gain_db = 50.0F;
    parameters.low_cut_hz = -5.0F;
    parameters.high_cut_hz = 30000.0F;
    parameters.pan_left_percent = -200.0F;
    parameters.pan_right_percent = 200.0F;
    parameters.phase_invert = static_cast<tabletfx::StereoFilterPhaseInvert>(9U);
    parameters.delay_time_ms = 200.0F;
    parameters.depth = 2.0F;
    parameters.feedback = 2.0F;
    parameters.level_db = 20.0F;
    parameters.balance_percent = -200.0F;
    parameters.lfo_shape = static_cast<tabletfx::StereoFilterLfoShape>(9U);
    parameters.lfo_rate_hz = 100.0F;
    parameters.lfo_duty_percent = 0.0F;
    parameters.mod_frequency_hz = 30000.0F;
    parameters.lfo_quantize_steps = 90U;
    filter.set_parameters(parameters);
    const auto actual = filter.parameters();
    require(actual.type == tabletfx::StereoFilterType::Lowpass &&
                actual.frequency_hz == 20000.0F && actual.order == 2U &&
                actual.q == 0.1F && actual.gain_db == 24.0F &&
                actual.low_cut_hz == 0.0F && actual.high_cut_hz == 20000.0F &&
                actual.pan_left_percent == -100.0F &&
                actual.pan_right_percent == 100.0F &&
                actual.phase_invert == tabletfx::StereoFilterPhaseInvert::None &&
                actual.delay_time_ms == 50.0F && actual.depth == 1.0F &&
                actual.feedback == 0.95F && actual.level_db == 12.0F &&
                actual.balance_percent == -100.0F &&
                actual.lfo_shape == tabletfx::StereoFilterLfoShape::Sine &&
                actual.lfo_rate_hz == 20.0F &&
                actual.lfo_duty_percent == 1.0F &&
                actual.mod_frequency_hz == 20000.0F &&
                actual.lfo_quantize_steps == 32U,
            "project-authored controls were not bounded or sanitized");

    parameters.frequency_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.q = std::numeric_limits<float>::infinity();
    parameters.level_db = -std::numeric_limits<float>::infinity();
    filter.set_parameters(parameters);
    require(filter.parameters().frequency_hz == 1000.0F &&
                filter.parameters().q == 0.707F &&
                filter.parameters().level_db == 0.0F,
            "non-finite controls did not fall back to project defaults");
}

void test_lowpass_highpass_and_fourth_order() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 48000U;
    constexpr std::size_t warmup = 12000U;

    auto low_frequency = tone(sample_rate, 250.0F, frames);
    auto high_frequency = tone(sample_rate, 8000.0F, frames);
    tabletfx::StereoFilter lowpass(sample_rate);
    lowpass.process(low_frequency, 1U);
    lowpass.process(high_frequency, 1U);
    const float low_pass_low_rms = rms(low_frequency, warmup);
    const float low_pass_high_rms = rms(high_frequency, warmup);
    require(low_pass_low_rms > low_pass_high_rms * 5.0F,
            "lowpass did not reject a high test tone");

    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::Highpass;
    parameters.frequency_hz = 1000.0F;
    parameters.q = 0.707F;
    tabletfx::StereoFilter highpass(sample_rate, parameters);
    auto highpass_low = tone(sample_rate, 100.0F, frames);
    auto highpass_high = tone(sample_rate, 5000.0F, frames);
    highpass.process(highpass_low, 1U);
    highpass.process(highpass_high, 1U);
    require(rms(highpass_high, warmup) > rms(highpass_low, warmup) * 5.0F,
            "highpass did not reject a low test tone");

    parameters.type = tabletfx::StereoFilterType::Lowpass;
    parameters.frequency_hz = 1000.0F;
    parameters.order = 2U;
    tabletfx::StereoFilter two_pole(sample_rate, parameters);
    parameters.order = 4U;
    tabletfx::StereoFilter four_pole(sample_rate, parameters);
    auto two_pole_audio = tone(sample_rate, 4000.0F, frames);
    auto four_pole_audio = two_pole_audio;
    two_pole.process(two_pole_audio, 1U);
    four_pole.process(four_pole_audio, 1U);
    require(rms(four_pole_audio, warmup) < rms(two_pole_audio, warmup) * 0.65F,
            "four-pole cascade did not steepen the lowpass response");
}

void test_comb_and_allpass_delay_controls() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t delayed_sample = 480U;
    std::vector<float> impulse(1000U, 0.0F);
    impulse[0] = 1.0F;

    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::FeedForwardComb;
    parameters.delay_time_ms = 10.0F;
    parameters.depth = 0.5F;
    tabletfx::StereoFilter feedforward(sample_rate, parameters);
    auto output = impulse;
    feedforward.process(output, 1U);
    require(std::abs(output[0] - 1.0F) < 1.0e-6F &&
                std::abs(output[delayed_sample] - 0.5F) < 1.0e-6F,
            "feed-forward comb did not produce a delayed tap");

    parameters.type = tabletfx::StereoFilterType::FeedbackComb;
    parameters.feedback = 0.5F;
    tabletfx::StereoFilter feedback(sample_rate, parameters);
    output = impulse;
    feedback.process(output, 1U);
    require(std::abs(output[0] - 1.0F) < 1.0e-6F &&
                std::abs(output[delayed_sample] - 0.5F) < 1.0e-6F &&
                std::abs(output[delayed_sample * 2U] - 0.25F) < 1.0e-6F,
            "feedback comb did not regenerate bounded repeats");

    parameters.type = tabletfx::StereoFilterType::Allpass;
    parameters.feedback = 0.6F;
    tabletfx::StereoFilter allpass(sample_rate, parameters);
    output = impulse;
    allpass.process(output, 1U);
    for (float sample : output) {
        require(std::isfinite(sample) && std::abs(sample) <= 8.0F,
                "allpass delay feedback became invalid or unbounded");
    }
    require(std::abs(output[delayed_sample]) > 0.1F,
            "allpass did not use its documented delay/feedback control roles");
}

void test_stereo_pan_phase_balance_and_mono() {
    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::Null;
    tabletfx::StereoFilter filter(48000.0F, parameters);
    std::array<float, 4> stereo{1.0F, 0.0F, 0.25F, -0.5F};
    filter.process(stereo, 2U);
    require(std::abs(stereo[0] - 1.0F) < 1.0e-5F &&
                std::abs(stereo[1]) < 1.0e-5F &&
                std::abs(stereo[2] - 0.25F) < 1.0e-5F &&
                std::abs(stereo[3] + 0.5F) < 1.0e-5F,
            "default independent stereo pans changed the channels");

    parameters.phase_invert = tabletfx::StereoFilterPhaseInvert::Left;
    parameters.pan_left_percent = 100.0F;
    parameters.pan_right_percent = 100.0F;
    filter.set_parameters(parameters);
    stereo = {1.0F, 0.0F, 0.0F, 0.0F};
    filter.process(stereo, 2U);
    require(std::abs(stereo[0]) < 1.0e-5F &&
                std::abs(stereo[1] + 1.0F) < 1.0e-5F,
            "left phase inversion or right panning failed");

    parameters.phase_invert = tabletfx::StereoFilterPhaseInvert::None;
    parameters.balance_percent = 100.0F;
    parameters.pan_left_percent = -100.0F;
    parameters.pan_right_percent = 100.0F;
    filter.set_parameters(parameters);
    stereo = {1.0F, 1.0F};
    filter.process(stereo, 2U);
    require(std::abs(stereo[0]) < 1.0e-5F &&
                std::abs(stereo[1] - 1.0F) < 1.0e-5F,
            "stereo output balance did not attenuate the opposite side");

    parameters.pan_left_percent = 100.0F;
    parameters.pan_right_percent = -100.0F;
    parameters.balance_percent = 0.0F;
    filter.set_parameters(parameters);
    std::array<float, 2> mono{0.25F, -0.5F};
    filter.process(mono, 1U);
    require(std::abs(mono[0] - 0.25F) < 1.0e-5F &&
                std::abs(mono[1] + 0.5F) < 1.0e-5F,
            "mono filtering should not apply stereo pan routing");
}

void test_lfo_and_optional_cut_filters() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 96000U;
    constexpr std::size_t warmup = 12000U;
    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::Lowpass;
    parameters.frequency_hz = 500.0F;
    parameters.mod_frequency_hz = 5000.0F;
    parameters.lfo_enabled = true;
    parameters.lfo_shape = tabletfx::StereoFilterLfoShape::Square;
    parameters.lfo_rate_hz = 1.0F;
    parameters.lfo_duty_percent = 50.0F;
    parameters.lfo_quantize_steps = 2U;
    tabletfx::StereoFilter modulated(sample_rate, parameters);
    auto changing = tone(sample_rate, 1500.0F, frames);
    modulated.process(changing, 1U);

    parameters.lfo_enabled = false;
    tabletfx::StereoFilter static_filter(sample_rate, parameters);
    auto static_audio = tone(sample_rate, 1500.0F, frames);
    static_filter.process(static_audio, 1U);
    require(rms(changing, warmup) > rms(static_audio, warmup) * 1.5F,
            "LFO did not sweep from the base cutoff toward Mod Frequency");

    parameters.low_cut_hz = 800.0F;
    parameters.high_cut_hz = 2500.0F;
    tabletfx::StereoFilter band_limited(sample_rate, parameters);
    std::vector<float> broadband(frames, 0.1F);
    band_limited.process(broadband, 1U);
    for (float sample : broadband) {
        require(std::isfinite(sample), "optional first-order cuts produced invalid audio");
    }
}

void test_auto_wah_automatically_sweeps_filter_mode() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 96000U;
    constexpr std::size_t warmup = 12000U;
    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::AutoWah;
    parameters.mode = tabletfx::StereoFilterMode::Bandpass;
    parameters.frequency_hz = 350.0F;
    parameters.mod_frequency_hz = 6000.0F;
    parameters.q = 4.0F;
    parameters.lfo_enabled = false;
    parameters.lfo_shape = tabletfx::StereoFilterLfoShape::Triangle;
    parameters.lfo_rate_hz = 2.0F;
    parameters.sweep_shape = 1.0F;
    tabletfx::StereoFilter auto_wah(sample_rate, parameters);
    auto swept_audio = tone(sample_rate, 2500.0F, frames);
    auto_wah.process(swept_audio, 1U);

    parameters.type = tabletfx::StereoFilterType::Bandpass;
    tabletfx::StereoFilter static_filter(sample_rate, parameters);
    auto static_audio = tone(sample_rate, 2500.0F, frames);
    static_filter.process(static_audio, 1U);
    require(rms(swept_audio, warmup) > rms(static_audio, warmup) * 1.5F,
            "Auto-Wah did not sweep automatically when the LFO toggle was off");

    parameters.type = tabletfx::StereoFilterType::AutoWah;
    parameters.lfo_tempo_sync = true;
    parameters.lfo_tempo_bpm = 120.0F;
    parameters.lfo_beats_per_cycle = 1.0F;
    tabletfx::StereoFilter tempo_synced(sample_rate, parameters);
    auto tempo_audio = tone(sample_rate, 2500.0F, frames);
    tempo_synced.process(tempo_audio, 1U);
    double tempo_error = 0.0;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const double delta = static_cast<double>(swept_audio[frame]) -
                             static_cast<double>(tempo_audio[frame]);
        tempo_error += delta * delta;
    }
    require(std::sqrt(tempo_error / static_cast<double>(frames)) < 1.0e-6,
            "Auto-Wah tempo sync did not match its equivalent 2 Hz rate");

    parameters.type = tabletfx::StereoFilterType::AutoWah;
    parameters.lfo_tempo_sync = false;
    parameters.sweep_shape = 3.0F;
    tabletfx::StereoFilter reshaped(sample_rate, parameters);
    auto reshaped_audio = tone(sample_rate, 2500.0F, frames);
    reshaped.process(reshaped_audio, 1U);
    double difference = 0.0;
    for (std::size_t frame = warmup; frame < frames; ++frame) {
        const double delta = static_cast<double>(swept_audio[frame]) -
                             static_cast<double>(reshaped_audio[frame]);
        difference += delta * delta;
    }
    require(std::sqrt(difference / static_cast<double>(frames - warmup)) >
                1.0e-4,
            "Auto-Wah Sweep Shape did not change the project-authored motion");
}

void test_envelope_filter_tracks_input_level_without_lfo() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 48000U;
    constexpr std::size_t warmup = 12000U;
    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::EnvelopeFilter;
    parameters.mode = tabletfx::StereoFilterMode::Bandpass;
    parameters.frequency_hz = 300.0F;
    parameters.mod_frequency_hz = 6000.0F;
    parameters.q = 4.0F;
    parameters.sensitivity_db = -36.0F;
    parameters.attack_time_ms = 5.0F;
    parameters.release_time_ms = 80.0F;
    parameters.lfo_enabled = true;

    const auto render = [&](float input_gain) {
        tabletfx::StereoFilter effect(sample_rate, parameters);
        auto audio = tone(sample_rate, 2500.0F, frames);
        for (float& sample : audio) sample *= input_gain;
        effect.process(audio, 1U);
        return rms(audio, warmup) / input_gain;
    };

    const float soft_gain = render(0.01F);
    const float hard_gain = render(0.8F);
    require(hard_gain > soft_gain * 2.0F,
            "Envelope Filter did not open farther for a stronger pick attack");
}

void test_sanitization_reset_and_no_process_allocation() {
    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::Null;
    tabletfx::StereoFilter filter(48000.0F, parameters);
    std::array<float, 5> malformed{
        0.25F, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -12.0F, 0.5F};
    filter.process(malformed, 1U);
    require(malformed[0] == 0.25F && malformed[1] == 0.0F &&
                malformed[2] == 0.0F && malformed[3] >= -8.0F &&
                malformed[4] == 0.5F,
            "invalid input samples were not safely bounded");

    std::array<float, 128> audio{};
    audio.fill(0.1F);
    allocation_count = 0U;
    count_allocations = true;
    filter.process(audio, 2U);
    count_allocations = false;
    require(allocation_count == 0U,
            "stereo filter allocated during process()");

    filter.reset();
    std::array<float, 2> impulse{1.0F, 0.0F};
    filter.process(impulse, 2U);
    require(std::isfinite(impulse[0]) && std::isfinite(impulse[1]),
            "reset left invalid processor state");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load()) ++allocation_count;
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_bounds_and_memory();
        test_lowpass_highpass_and_fourth_order();
        test_comb_and_allpass_delay_controls();
        test_stereo_pan_phase_balance_and_mono();
        test_lfo_and_optional_cut_filters();
        test_auto_wah_automatically_sweeps_filter_mode();
        test_envelope_filter_tracks_input_level_without_lfo();
        test_sanitization_reset_and_no_process_allocation();
        std::cout << "Stereo Filter tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        count_allocations = false;
        std::cerr << "Stereo Filter test failure: " << exception.what() << '\n';
        return 1;
    }
}
