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
    constexpr std::size_t attack_frames = 4800U;
    constexpr std::size_t release_begin = 24000U;
    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::EnvelopeFilter;
    parameters.mode = tabletfx::StereoFilterMode::Lowpass;
    parameters.frequency_hz = 300.0F;
    parameters.mod_frequency_hz = 6000.0F;
    parameters.sensitivity_db = -36.0F;
    parameters.attack_time_ms = 5.0F;
    parameters.release_time_ms = 80.0F;
    parameters.lfo_enabled = true;

    tabletfx::StereoFilter effect(sample_rate, parameters);
    auto held_note = tone(sample_rate, 2500.0F, frames);
    for (float& sample : held_note) sample *= 0.8F;
    effect.process(held_note, 1U);
    const float attack_response = rms(
        std::span<const float>(held_note.data(), attack_frames), 0U) / 0.8F;
    const float sustained_response = rms(held_note, release_begin) / 0.8F;
    require(attack_response > sustained_response * 2.0F,
            "Envelope Filter attack/tail responses were " +
                std::to_string(attack_response) + " / " +
                std::to_string(sustained_response));
}

void test_ladder_filter_controls() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 24000U;
    constexpr std::size_t warmup = 6000U;
    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::LadderFilter;
    parameters.frequency_hz = 1000.0F;
    parameters.ladder_resonance = 0.0F;
    parameters.ladder_drive = 0.0F;
    parameters.ladder_bass_comp = 0.0F;

    tabletfx::StereoFilter default_order(sample_rate, parameters);
    require(default_order.parameters().ladder_order == 4U,
            "Ladder Filter project default is not four-pole");

    parameters.ladder_order = 2U;
    parameters.ladder_resonance = -1.0F;
    parameters.ladder_drive = 2.0F;
    parameters.ladder_bass_comp = 4.0F;
    parameters.mode = tabletfx::StereoFilterMode::Highpass;
    tabletfx::StereoFilter sanitized(sample_rate, parameters);
    const auto bounded = sanitized.parameters();
    require(bounded.ladder_order == 2U && bounded.ladder_resonance == 0.0F &&
                bounded.ladder_drive == 1.0F &&
                bounded.ladder_bass_comp == 1.0F &&
                bounded.mode == tabletfx::StereoFilterMode::Lowpass,
            "Ladder Filter project controls or unsupported mode were not sanitized");

    parameters.ladder_order = 4U;
    parameters.ladder_resonance = 0.0F;
    parameters.ladder_drive = 0.0F;
    parameters.ladder_bass_comp = 0.0F;
    parameters.mode = tabletfx::StereoFilterMode::Lowpass;
    tabletfx::StereoFilter lowpass_low(sample_rate, parameters);
    auto low_tone = tone(sample_rate, 250.0F, frames);
    lowpass_low.process(low_tone, 1U);
    tabletfx::StereoFilter lowpass_high(sample_rate, parameters);
    auto high_tone = tone(sample_rate, 8000.0F, frames);
    lowpass_high.process(high_tone, 1U);
    require(rms(low_tone, warmup) > rms(high_tone, warmup) * 5.0F,
            "four-pole Ladder low-pass did not reject a high tone");

    parameters.ladder_order = 2U;
    tabletfx::StereoFilter two_pole(sample_rate, parameters);
    auto two_pole_audio = tone(sample_rate, 4000.0F, frames);
    two_pole.process(two_pole_audio, 1U);
    parameters.ladder_order = 4U;
    tabletfx::StereoFilter four_pole(sample_rate, parameters);
    auto four_pole_audio = tone(sample_rate, 4000.0F, frames);
    four_pole.process(four_pole_audio, 1U);
    require(rms(four_pole_audio, warmup) < rms(two_pole_audio, warmup) * 0.65F,
            "four-pole Ladder order did not steepen the low-pass response");

    parameters.mode = tabletfx::StereoFilterMode::Bandpass;
    tabletfx::StereoFilter bandpass_low(sample_rate, parameters);
    auto band_low = tone(sample_rate, 100.0F, frames);
    bandpass_low.process(band_low, 1U);
    tabletfx::StereoFilter bandpass_mid(sample_rate, parameters);
    auto band_mid = tone(sample_rate, 1000.0F, frames);
    bandpass_mid.process(band_mid, 1U);
    tabletfx::StereoFilter bandpass_high(sample_rate, parameters);
    auto band_high = tone(sample_rate, 8000.0F, frames);
    bandpass_high.process(band_high, 1U);
    require(rms(band_mid, warmup) > rms(band_low, warmup) * 1.5F &&
                rms(band_mid, warmup) > rms(band_high, warmup) * 1.2F,
            "Ladder band-pass did not emphasize its center region");

    parameters.mode = tabletfx::StereoFilterMode::Lowpass;
    const auto render = [&](tabletfx::StereoFilterParameters selected,
                            float frequency, float amplitude) {
        tabletfx::StereoFilter effect(sample_rate, selected);
        auto audio = tone(sample_rate, frequency, frames);
        for (float& sample : audio) sample *= amplitude / 0.25F;
        effect.process(audio, 1U);
        return audio;
    };
    const auto difference = [](std::span<const float> left,
                               std::span<const float> right) {
        double sum = 0.0;
        for (std::size_t index = 0U; index < left.size(); ++index) {
            sum += std::abs(static_cast<double>(left[index]) - right[index]);
        }
        return sum / static_cast<double>(left.size());
    };

    auto unresonant = render(parameters, 1000.0F, 0.25F);
    parameters.ladder_resonance = 0.8F;
    auto resonant = render(parameters, 1000.0F, 0.25F);
    require(difference(unresonant, resonant) > 0.01,
            "Ladder resonance did not change the cutoff response");

    parameters.ladder_resonance = 0.0F;
    parameters.ladder_drive = 1.0F;
    auto driven = render(parameters, 700.0F, 0.75F);
    parameters.ladder_drive = 0.0F;
    auto clean = render(parameters, 700.0F, 0.75F);
    require(difference(driven, clean) > 0.005,
            "Ladder Drive did not change the saturation response");

    parameters.frequency_hz = 1000.0F;
    parameters.ladder_resonance = 0.8F;
    parameters.ladder_drive = 0.0F;
    parameters.ladder_bass_comp = 0.0F;
    auto uncompensated_bass = render(parameters, 60.0F, 0.25F);
    parameters.ladder_bass_comp = 1.0F;
    auto compensated_bass = render(parameters, 60.0F, 0.25F);
    require(difference(uncompensated_bass, compensated_bass) > 0.005,
            "Ladder Bass Compensation did not restore low-frequency output");

    parameters.mode = tabletfx::StereoFilterMode::Bandpass;
    auto band_without_comp = render(parameters, 60.0F, 0.25F);
    parameters.ladder_bass_comp = 0.0F;
    auto band_with_zero_comp = render(parameters, 60.0F, 0.25F);
    require(difference(band_without_comp, band_with_zero_comp) < 1.0e-7,
            "Bass Compensation unexpectedly altered Ladder band-pass mode");

    parameters.mode = tabletfx::StereoFilterMode::Lowpass;
    parameters.ladder_bass_comp = 0.0F;
    parameters.frequency_hz = 500.0F;
    parameters.mod_frequency_hz = 5000.0F;
    parameters.lfo_enabled = true;
    parameters.lfo_shape = tabletfx::StereoFilterLfoShape::Square;
    parameters.lfo_rate_hz = 1.0F;
    parameters.lfo_duty_percent = 50.0F;
    parameters.lfo_quantize_steps = 2U;
    tabletfx::StereoFilter lfo_filter(sample_rate, parameters);
    auto lfo_audio = tone(sample_rate, 1500.0F, 96000U);
    lfo_filter.process(lfo_audio, 1U);
    parameters.lfo_enabled = false;
    tabletfx::StereoFilter static_filter(sample_rate, parameters);
    auto static_audio = tone(sample_rate, 1500.0F, 96000U);
    static_filter.process(static_audio, 1U);
    require(rms(lfo_audio, warmup) > rms(static_audio, warmup) * 1.5F,
            "LFO did not sweep the Ladder cutoff");

    parameters.lfo_enabled = false;
    parameters.low_cut_hz = 400.0F;
    parameters.high_cut_hz = 5000.0F;
    tabletfx::StereoFilter cut_filter(sample_rate, parameters);
    std::vector<float> cut_audio(frames, 0.1F);
    cut_filter.process(cut_audio, 1U);
    for (float sample : cut_audio) {
        require(std::isfinite(sample), "Ladder cut filters produced invalid audio");
    }

    parameters.low_cut_hz = 0.0F;
    parameters.high_cut_hz = 0.0F;
    parameters.ladder_resonance = 1.0F;
    tabletfx::StereoFilter realtime_filter(sample_rate, parameters);
    std::array<float, 128> realtime_audio{};
    realtime_audio.fill(0.1F);
    allocation_count = 0U;
    count_allocations = true;
    realtime_filter.process(realtime_audio, 2U);
    count_allocations = false;
    require(allocation_count == 0U,
            "Ladder Filter allocated during process()");
    for (float sample : realtime_audio) {
        require(std::isfinite(sample), "maximum Ladder resonance became non-finite");
    }
    realtime_filter.reset();
    realtime_filter.process(realtime_audio, 2U);
    for (float sample : realtime_audio) {
        require(std::isfinite(sample), "Ladder reset left invalid processor state");
    }
}

void test_touch_ladder_uses_input_envelope_instead_of_lfo() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 48000U;
    constexpr std::size_t warmup = 12000U;
    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::TouchLadderFilter;
    parameters.mode = tabletfx::StereoFilterMode::Highpass;
    parameters.frequency_hz = 20.0F;
    parameters.mod_frequency_hz = 20000.0F;
    parameters.ladder_order = 2U;
    parameters.ladder_resonance = 0.2F;
    parameters.ladder_drive = 0.1F;
    parameters.ladder_bass_comp = 0.4F;
    parameters.lfo_enabled = true;

    tabletfx::StereoFilter sanitized(sample_rate, parameters);
    const auto bounded = sanitized.parameters();
    require(bounded.type == tabletfx::StereoFilterType::TouchLadderFilter &&
                bounded.mode == tabletfx::StereoFilterMode::Lowpass &&
                bounded.frequency_hz == 100.0F &&
                bounded.mod_frequency_hz == 10000.0F &&
                bounded.ladder_order == 2U &&
                bounded.ladder_resonance == 0.2F &&
                bounded.ladder_drive == 0.1F &&
                bounded.ladder_bass_comp == 0.4F,
            "Touch-Ladder did not preserve the shared Ladder controls or sanitize its sweep");

    parameters.mode = tabletfx::StereoFilterMode::Lowpass;
    parameters.frequency_hz = 300.0F;
    parameters.mod_frequency_hz = 6000.0F;
    parameters.ladder_order = 4U;
    parameters.ladder_resonance = 0.0F;
    parameters.ladder_drive = 0.0F;
    parameters.ladder_bass_comp = 0.0F;
    parameters.sensitivity_db = -36.0F;
    parameters.attack_time_ms = 5.0F;
    parameters.release_time_ms = 80.0F;

    const auto render = [&](float input_gain, bool lfo_enabled) {
        parameters.lfo_enabled = lfo_enabled;
        tabletfx::StereoFilter effect(sample_rate, parameters);
        auto audio = tone(sample_rate, 2500.0F, frames);
        for (float& sample : audio) sample *= input_gain;
        effect.process(audio, 1U);
        return rms(audio, warmup) / input_gain;
    };

    const float soft_pick = render(0.01F, false);
    const float hard_pick = render(0.8F, false);
    require(hard_pick > soft_pick * 2.0F,
            "Touch-Ladder cutoff did not open farther for a stronger block-input level");

    const float hard_with_lfo_enabled = render(0.8F, true);
    require(std::abs(hard_pick - hard_with_lfo_enabled) < 1.0e-7F,
            "Touch-Ladder followed the LFO toggle instead of its input envelope");

    parameters.lfo_enabled = false;
    parameters.mode = tabletfx::StereoFilterMode::Bandpass;
    tabletfx::StereoFilter bandpass(sample_rate, parameters);
    auto band_audio = tone(sample_rate, 2500.0F, frames);
    for (float& sample : band_audio) sample *= 0.8F;
    bandpass.process(band_audio, 1U);
    for (float sample : band_audio) {
        require(std::isfinite(sample),
                "Touch-Ladder band-pass mode produced non-finite audio");
    }
    require(std::abs(rms(band_audio, warmup) - hard_pick * 0.8F) > 1.0e-4F,
            "Touch-Ladder band-pass mode did not use the shared Ladder response");
}

void test_touch_wah_tracks_sustained_volume_and_natural_fade() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 48000U;
    constexpr std::size_t warmup = 12000U;
    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::TouchWahFilter;
    parameters.mode = tabletfx::StereoFilterMode::Lowpass;
    parameters.frequency_hz = 300.0F;
    parameters.mod_frequency_hz = 6000.0F;
    parameters.sensitivity_db = -36.0F;
    parameters.attack_time_ms = 5.0F;
    parameters.release_time_ms = 80.0F;
    parameters.lfo_enabled = true;

    tabletfx::StereoFilter touch_wah(sample_rate, parameters);
    auto strong_sustain = tone(sample_rate, 2500.0F, frames);
    for (float& sample : strong_sustain) sample *= 0.8F;
    touch_wah.process(strong_sustain, 1U);
    const float sustained_response = rms(strong_sustain, warmup) / 0.8F;

    auto no_lfo_parameters = parameters;
    no_lfo_parameters.lfo_enabled = false;
    tabletfx::StereoFilter no_lfo(sample_rate, no_lfo_parameters);
    auto same_sustain = tone(sample_rate, 2500.0F, frames);
    for (float& sample : same_sustain) sample *= 0.8F;
    no_lfo.process(same_sustain, 1U);
    double lfo_difference = 0.0;
    for (std::size_t index = 0U; index < frames; ++index) {
        const double delta = static_cast<double>(strong_sustain[index]) -
                             same_sustain[index];
        lfo_difference += delta * delta;
    }
    require(std::sqrt(lfo_difference / static_cast<double>(frames)) < 1.0e-7,
            "Touch-Wah unexpectedly followed the LFO toggle");

    auto natural_fade = tone(sample_rate, 2500.0F, frames);
    for (float& sample : natural_fade) sample *= 0.01F;
    touch_wah.process(natural_fade, 1U);
    const float faded_response = rms(natural_fade, warmup) / 0.01F;
    require(sustained_response > faded_response * 2.0F,
            "Touch-Wah did not follow sustained input volume through a natural fade");

    parameters.mode = tabletfx::StereoFilterMode::Highpass;
    tabletfx::StereoFilter highpass(sample_rate, parameters);
    require(highpass.parameters().mode == tabletfx::StereoFilterMode::Highpass,
            "Touch-Wah incorrectly sanitized its documented high-pass mode");
    auto low_tone = tone(sample_rate, 100.0F, frames);
    for (float& sample : low_tone) sample *= 0.8F;
    auto highpass_audio = low_tone;
    highpass.process(highpass_audio, 1U);
    parameters.mode = tabletfx::StereoFilterMode::Lowpass;
    tabletfx::StereoFilter lowpass(sample_rate, parameters);
    lowpass.process(low_tone, 1U);
    require(rms(low_tone, warmup) > rms(highpass_audio, warmup) * 5.0F,
            "Touch-Wah low-pass and high-pass modes did not select distinct responses");
}

void test_touch_wah_mix_crossfades_dry_and_project_bandpass() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 4096U;
    for (const std::uint16_t channels : std::array<std::uint16_t, 2>{1U, 2U}) {
        std::vector<float> dry_input(frames * channels);
        for (std::size_t frame = 0U; frame < frames; ++frame) {
            dry_input[frame * channels] = 0.22F * std::sin(
                2.0F * 3.14159265358979323846F * 430.0F *
                static_cast<float>(frame) / sample_rate);
            if (channels == 2U) {
                dry_input[frame * channels + 1U] = 0.17F * std::sin(
                    2.0F * 3.14159265358979323846F * 1730.0F *
                    static_cast<float>(frame) / sample_rate + 0.3F);
            }
        }
        auto wet_input = dry_input;
        auto midpoint_input = dry_input;
        auto dry_reference = dry_input;
        auto wet_reference = dry_input;

        tabletfx::StereoFilterParameters parameters;
        parameters.type = tabletfx::StereoFilterType::TouchWahFilter;
        parameters.mode = tabletfx::StereoFilterMode::Mix;
        parameters.frequency_hz = 350.0F;
        parameters.mod_frequency_hz = 6000.0F;
        parameters.sensitivity_db = -36.0F;
        parameters.attack_time_ms = 5.0F;
        parameters.release_time_ms = 80.0F;
        parameters.mix_percent = 0.0F;
        tabletfx::StereoFilter dry(sample_rate, parameters);
        dry.process(dry_input, channels);
        for (std::size_t index = 0U; index < dry_input.size(); ++index) {
            require(std::abs(dry_input[index] - dry_reference[index]) < 1.0e-7F,
                    "Touch-Wah MIX at zero percent changed the dry signal");
        }

        parameters.mix_percent = 100.0F;
        tabletfx::StereoFilter full_mix(sample_rate, parameters);
        full_mix.process(wet_input, channels);
        parameters.mode = tabletfx::StereoFilterMode::Bandpass;
        tabletfx::StereoFilter bandpass(sample_rate, parameters);
        bandpass.process(wet_reference, channels);
        for (std::size_t index = 0U; index < wet_input.size(); ++index) {
            require(std::abs(wet_input[index] - wet_reference[index]) < 1.0e-7F,
                    "Touch-Wah MIX at 100 percent differs from its wet path");
        }

        parameters.mode = tabletfx::StereoFilterMode::Mix;
        parameters.mix_percent = 50.0F;
        tabletfx::StereoFilter midpoint(sample_rate, parameters);
        midpoint.process(midpoint_input, channels);
        for (std::size_t index = 0U; index < midpoint_input.size(); ++index) {
            const float expected = 0.5F * (dry_reference[index] +
                                           wet_reference[index]);
            require(std::abs(midpoint_input[index] - expected) < 1.0e-7F,
                    "Touch-Wah MIX midpoint was not a linear dry/filter blend");
        }
    }
}

void test_touch_wah_mix_sanitization_reset_smoothing_and_blocks() {
    constexpr float sample_rate = 48000.0F;
    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::TouchWahFilter;
    parameters.mode = tabletfx::StereoFilterMode::Mix;
    parameters.frequency_hz = 350.0F;
    parameters.mod_frequency_hz = 6000.0F;
    parameters.sensitivity_db = -36.0F;
    parameters.mix_percent = std::numeric_limits<float>::quiet_NaN();
    tabletfx::StereoFilter sanitized(sample_rate, parameters);
    require(sanitized.parameters().mode == tabletfx::StereoFilterMode::Mix &&
                sanitized.parameters().mix_percent == 50.0F,
            "Touch-Wah MIX default or non-finite fallback was incorrect");

    parameters.mix_percent = std::numeric_limits<float>::infinity();
    sanitized.set_parameters(parameters);
    require(sanitized.parameters().mix_percent == 50.0F,
            "Touch-Wah MIX did not replace infinite blend values with its default");
    parameters.mix_percent = -std::numeric_limits<float>::infinity();
    sanitized.set_parameters(parameters);
    require(sanitized.parameters().mix_percent == 50.0F,
            "Touch-Wah MIX did not replace negative infinity with its default");
    parameters.mix_percent = -10.0F;
    sanitized.set_parameters(parameters);
    require(sanitized.parameters().mix_percent == 0.0F,
            "Touch-Wah MIX did not clamp its lower bound");
    parameters.mix_percent = 110.0F;
    sanitized.set_parameters(parameters);
    require(sanitized.parameters().mix_percent == 100.0F,
            "Touch-Wah MIX did not clamp its upper bound");

    parameters.mix_percent = 0.0F;
    tabletfx::StereoFilter ramping(sample_rate, parameters);
    tabletfx::StereoFilter chunked(sample_rate, parameters);
    std::vector<float> audio(4097U);
    for (std::size_t index = 0U; index < audio.size(); ++index) {
        audio[index] = 0.2F * std::sin(
            2.0F * 3.14159265358979323846F * 1200.0F *
            static_cast<float>(index) / sample_rate);
    }
    auto ramping_audio = audio;
    auto chunked_audio = audio;
    constexpr std::size_t change_frame = 97U;
    ramping.process(std::span<float>(ramping_audio.data(), change_frame), 1U);
    chunked.process(std::span<float>(chunked_audio.data(), change_frame), 1U);
    parameters.mix_percent = 100.0F;
    ramping.set_parameters(parameters);
    chunked.set_parameters(parameters);
    ramping.process(std::span<float>(ramping_audio.data() + change_frame,
                                     ramping_audio.size() - change_frame), 1U);
    std::size_t offset = change_frame;
    const std::array<std::size_t, 7> chunks{3U, 17U, 1U, 31U, 9U, 64U, 5U};
    std::size_t chunk_index = 0U;
    while (offset < chunked_audio.size()) {
        const std::size_t count = std::min(
            chunks[chunk_index % chunks.size()], chunked_audio.size() - offset);
        chunked.process(std::span<float>(chunked_audio.data() + offset, count),
                        1U);
        offset += count;
        ++chunk_index;
    }
    for (std::size_t index = 0U; index < ramping_audio.size(); ++index) {
        require(std::abs(ramping_audio[index] - chunked_audio[index]) < 1.0e-7F,
                "Touch-Wah MIX ramp changed with irregular process block sizes");
    }

    parameters.mix_percent = 0.0F;
    tabletfx::StereoFilter reset_filter(sample_rate, parameters);
    std::array<float, 64> warmup{};
    warmup.fill(0.2F);
    reset_filter.process(warmup, 1U);
    parameters.mix_percent = 100.0F;
    reset_filter.set_parameters(parameters);
    reset_filter.process(warmup, 1U);
    reset_filter.reset();
    tabletfx::StereoFilter fresh_full_mix(sample_rate, parameters);
    std::array<float, 64> after_reset{};
    std::array<float, 64> fresh_output{};
    after_reset.fill(0.2F);
    fresh_output = after_reset;
    reset_filter.process(after_reset, 1U);
    fresh_full_mix.process(fresh_output, 1U);
    for (std::size_t index = 0U; index < after_reset.size(); ++index) {
        require(std::abs(after_reset[index] - fresh_output[index]) < 1.0e-7F,
                "Touch-Wah MIX reset did not restore its target blend and filter state");
    }

    std::array<float, 256> allocation_audio{};
    allocation_audio.fill(0.1F);
    allocation_count = 0U;
    count_allocations = true;
    reset_filter.process(allocation_audio, 1U);
    count_allocations = false;
    require(allocation_count == 0U,
            "Touch-Wah MIX allocated during process()");
}

void test_physical_detector_uses_raw_chain_input_and_falls_back_safely() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 8192U;
    const std::array<std::uint16_t, 2> channel_counts{1U, 2U};
    const std::array<tabletfx::StereoFilterType, 2> filter_types{
        tabletfx::StereoFilterType::TouchWahFilter,
        tabletfx::StereoFilterType::TouchLadderFilter};

    for (const auto type : filter_types) {
        for (const std::uint16_t channels : channel_counts) {
            std::vector<float> block_signal(frames * channels);
            std::vector<float> raw_signal(frames * channels);
            for (std::size_t frame = 0U; frame < frames; ++frame) {
                for (std::uint16_t channel = 0U; channel < channels; ++channel) {
                    const float frequency = channel == 0U ? 2400.0F : 3700.0F;
                    const std::size_t sample_index = frame * channels + channel;
                    const float phase = 2.0F * 3.14159265358979323846F *
                        frequency * static_cast<float>(frame) / sample_rate;
                    block_signal[sample_index] = 0.12F * std::sin(phase);
                    raw_signal[sample_index] = 0.8F * std::sin(phase);
                }
            }

            auto block_output = block_signal;
            auto physical_output = block_signal;
            auto fallback_output = block_signal;
            tabletfx::StereoFilterParameters parameters;
            parameters.type = type;
            parameters.frequency_hz = 300.0F;
            parameters.mod_frequency_hz = 6000.0F;
            parameters.sensitivity_db = -36.0F;
            parameters.attack_time_ms = 2.0F;
            parameters.release_time_ms = 80.0F;
            parameters.mode = tabletfx::StereoFilterMode::Lowpass;
            parameters.detector_source =
                tabletfx::StereoFilterDetectorSource::BlockInput;
            tabletfx::StereoFilter block_filter(sample_rate, parameters);
            block_filter.process(block_output, channels);

            parameters.detector_source =
                tabletfx::StereoFilterDetectorSource::PhysicalInput;
            tabletfx::StereoFilter physical_filter(sample_rate, parameters);
            physical_filter.process_with_detector(physical_output, raw_signal,
                                                  channels);
            tabletfx::StereoFilter fallback_filter(sample_rate, parameters);
            fallback_filter.process(fallback_output, channels);

            double physical_difference = 0.0;
            for (std::size_t sample = 0U; sample < block_output.size(); ++sample) {
                physical_difference += std::abs(
                    static_cast<double>(block_output[sample]) -
                    physical_output[sample]);
                require(std::abs(block_output[sample] - fallback_output[sample]) <
                            1.0e-7F,
                        "Physical detector without a raw span did not fall back to block input");
            }
            physical_difference /= static_cast<double>(block_output.size());
            require(physical_difference > 0.002,
                    "Physical detector did not follow the original raw chain input");
            require(block_filter.memory_bytes() == physical_filter.memory_bytes(),
                    "Detector source selection changed filter buffer memory");
        }
    }
}

void test_physical_detector_is_block_invariant_and_allocation_free() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 4093U;
    const std::array<std::uint16_t, 2> channel_counts{1U, 2U};
    const std::array<tabletfx::StereoFilterType, 2> filter_types{
        tabletfx::StereoFilterType::TouchWahFilter,
        tabletfx::StereoFilterType::TouchLadderFilter};
    const std::array<std::size_t, 7> chunks{3U, 17U, 1U, 31U, 9U, 64U, 5U};

    for (const auto type : filter_types) {
        for (const std::uint16_t channels : channel_counts) {
            std::vector<float> audio(frames * channels);
            std::vector<float> raw_input(frames * channels);
            for (std::size_t frame = 0U; frame < frames; ++frame) {
                for (std::uint16_t channel = 0U; channel < channels; ++channel) {
                    const std::size_t sample_index = frame * channels + channel;
                    const float frequency = channel == 0U ? 350.0F : 1200.0F;
                    const float phase = 2.0F * 3.14159265358979323846F *
                        frequency * static_cast<float>(frame) / sample_rate;
                    audio[sample_index] = 0.15F * std::sin(phase);
                    const float envelope = frame < frames / 2U ? 0.85F : 0.08F;
                    raw_input[sample_index] = envelope * std::sin(phase + 0.2F);
                }
            }
            auto contiguous = audio;
            auto chunked = audio;
            tabletfx::StereoFilterParameters parameters;
            parameters.type = type;
            parameters.mode = tabletfx::StereoFilterMode::Lowpass;
            parameters.frequency_hz = 250.0F;
            parameters.mod_frequency_hz = 7000.0F;
            parameters.sensitivity_db = -36.0F;
            parameters.attack_time_ms = 4.0F;
            parameters.release_time_ms = 60.0F;
            parameters.detector_source =
                tabletfx::StereoFilterDetectorSource::PhysicalInput;
            tabletfx::StereoFilter one_block(sample_rate, parameters);
            tabletfx::StereoFilter irregular_blocks(sample_rate, parameters);
            one_block.process_with_detector(contiguous, raw_input, channels);

            std::size_t offset_frames = 0U;
            std::size_t chunk_index = 0U;
            while (offset_frames < frames) {
                const std::size_t count = std::min(
                    chunks[chunk_index % chunks.size()], frames - offset_frames);
                const std::size_t sample_offset = offset_frames * channels;
                const std::size_t sample_count = count * channels;
                irregular_blocks.process_with_detector(
                    std::span<float>(chunked.data() + sample_offset, sample_count),
                    std::span<const float>(raw_input.data() + sample_offset,
                                           sample_count),
                    channels);
                offset_frames += count;
                ++chunk_index;
            }
            for (std::size_t sample = 0U; sample < contiguous.size(); ++sample) {
                require(std::abs(contiguous[sample] - chunked[sample]) < 1.0e-7F,
                        "Physical detector changed with irregular process blocks");
            }
        }
    }

    tabletfx::StereoFilterParameters parameters;
    parameters.type = tabletfx::StereoFilterType::TouchWahFilter;
    parameters.detector_source =
        tabletfx::StereoFilterDetectorSource::PhysicalInput;
    tabletfx::StereoFilter filter(sample_rate, parameters);
    std::array<float, 128> audio{};
    std::array<float, 128> raw_input{};
    audio.fill(0.1F);
    raw_input.fill(0.8F);
    allocation_count = 0U;
    count_allocations = true;
    filter.process_with_detector(audio, raw_input, 1U);
    count_allocations = false;
    require(allocation_count == 0U,
            "Physical detector path allocated during process()");

    parameters.detector_source =
        static_cast<tabletfx::StereoFilterDetectorSource>(255U);
    filter.set_parameters(parameters);
    require(filter.parameters().detector_source ==
                tabletfx::StereoFilterDetectorSource::BlockInput,
            "Invalid detector source was not sanitized to block input");
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
        test_ladder_filter_controls();
        test_touch_ladder_uses_input_envelope_instead_of_lfo();
        test_touch_wah_tracks_sustained_volume_and_natural_fade();
        test_touch_wah_mix_crossfades_dry_and_project_bandpass();
        test_touch_wah_mix_sanitization_reset_smoothing_and_blocks();
        test_physical_detector_uses_raw_chain_input_and_falls_back_safely();
        test_physical_detector_is_block_invariant_and_allocation_free();
        test_sanitization_reset_and_no_process_allocation();
        std::cout << "Stereo Filter tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        count_allocations = false;
        std::cerr << "Stereo Filter test failure: " << exception.what() << '\n';
        return 1;
    }
}
