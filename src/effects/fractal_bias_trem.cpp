#include "tabletfx/effects/fractal_bias_trem.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTwoPi = 2.0F * kPi;
constexpr float kOutputLimit = 4.0F;
constexpr float kMaximumDriveCoefficient = 0.24F;
constexpr float kMaximumAsymmetry = 0.45F;
constexpr float kDetectorAttackSeconds = 0.001F;
constexpr float kDetectorReleaseSeconds = 0.12F;

float one_pole_coefficient(float sample_rate, float time_seconds) noexcept {
    return 1.0F - std::exp(-1.0F / (sample_rate * time_seconds));
}

float db_to_linear(float decibels) noexcept {
    return std::pow(10.0F, decibels / 20.0F);
}

} // namespace

BiasTrem::BiasTrem(float sample_rate, BiasTremParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void BiasTrem::configure(float sample_rate,
                         BiasTremParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    parameters_ = sanitize_parameters(parameters, BiasTremParameters{});
    prepare_oscillator();
    prepare_detector_and_output();
    reset();
}

void BiasTrem::set_parameters(BiasTremParameters parameters) noexcept {
    parameters_ = sanitize_parameters(parameters, parameters_);
    prepare_oscillator();
    prepare_detector_and_output();
}

void BiasTrem::process(std::span<float> interleaved,
                       std::uint16_t channels) noexcept {
    if (channels == 0 || parameters_.bypass) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        float input_peak = 0.0F;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const float input = interleaved[offset + channel];
            if (std::isfinite(input)) {
                input_peak = std::max(input_peak, std::abs(input));
            } else {
                interleaved[offset + channel] = 0.0F;
            }
        }

        if (parameters_.trigger_sync) {
            const bool is_above = input_peak >= trigger_threshold_linear_;
            if (is_above && !trigger_was_above_) {
                reset_phase();
            }
            trigger_was_above_ = is_above;
        }

        const float detector_coefficient =
            input_peak > envelope_ ? attack_coefficient_ : release_coefficient_;
        envelope_ += detector_coefficient * (input_peak - envelope_);

        const float lfo = next_lfo_value();
        const float lfo_amount = (lfo + 1.0F) * 0.5F;
        const float detector_level = std::clamp(envelope_, 0.0F, 1.0F);
        const float effective_depth = parameters_.depth *
                                      (1.0F - parameters_.ducking * detector_level);
        const float modulation_gain =
            1.0F - effective_depth * (1.0F - lfo_amount);

        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            const float dry = std::isfinite(sample) ? sample : 0.0F;
            const float modulated = dry * modulation_gain;

            float wet = modulated;
            if (parameters_.bias_drive > 0.0F) {
                const float base_drive =
                    parameters_.bias_drive * kMaximumDriveCoefficient;
                const float polarity_bias = lfo * kMaximumAsymmetry;
                const float drive = base_drive *
                                    (modulated >= 0.0F
                                         ? 1.0F + polarity_bias
                                         : 1.0F - polarity_bias);
                wet = modulated / (1.0F + drive * std::abs(modulated));
            }

            const float mixed = dry + (wet - dry) * parameters_.mix;
            float channel_gain = output_gain_;
            if (channels >= 2) {
                if (channel == 0) {
                    channel_gain *= left_gain_;
                } else if (channel == 1) {
                    channel_gain *= right_gain_;
                }
            }
            const float output = mixed * channel_gain;
            sample = std::isfinite(output)
                         ? std::clamp(output, -kOutputLimit, kOutputLimit)
                         : 0.0F;
        }
    }
}

void BiasTrem::reset() noexcept {
    reset_phase();
    random_state_ = 0x6D2B79F5U;
    sample_hold_value_ = -1.0F;
    envelope_ = 0.0F;
    trigger_was_above_ = false;
}

float BiasTrem::bounded(float value, float fallback,
                        float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

BiasTremWaveform BiasTrem::sanitize_waveform(
    BiasTremWaveform waveform) noexcept {
    switch (waveform) {
    case BiasTremWaveform::Sine:
    case BiasTremWaveform::Triangle:
    case BiasTremWaveform::Square:
    case BiasTremWaveform::SawUp:
    case BiasTremWaveform::SawDown:
    case BiasTremWaveform::SampleHold:
        return waveform;
    }
    return BiasTremWaveform::Sine;
}

BiasTremParameters BiasTrem::sanitize_parameters(
    BiasTremParameters parameters,
    const BiasTremParameters& fallback) noexcept {
    parameters.rate_hz = bounded(parameters.rate_hz, fallback.rate_hz,
                                 kMinimumRateHz, kMaximumRateHz);
    parameters.depth = bounded(parameters.depth, fallback.depth, 0.0F, 1.0F);
    parameters.waveform = sanitize_waveform(parameters.waveform);
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, fallback.tempo_bpm,
                                   kMinimumBpm, kMaximumBpm);
    parameters.beats_per_cycle = bounded(
        parameters.beats_per_cycle, fallback.beats_per_cycle,
        kMinimumBeatsPerCycle, kMaximumBeatsPerCycle);
    parameters.trigger_threshold_db = bounded(
        parameters.trigger_threshold_db, fallback.trigger_threshold_db,
        -80.0F, 0.0F);
    parameters.ducking = bounded(parameters.ducking, fallback.ducking,
                                 0.0F, 1.0F);
    parameters.bias_drive = bounded(parameters.bias_drive, fallback.bias_drive,
                                    0.0F, 1.0F);
    parameters.mix = bounded(parameters.mix, fallback.mix, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, fallback.level_db,
                                   -24.0F, 12.0F);
    parameters.balance = bounded(parameters.balance, fallback.balance,
                                  -1.0F, 1.0F);
    return parameters;
}

void BiasTrem::prepare_oscillator() noexcept {
    const float requested_rate = parameters_.tempo_sync
                                     ? parameters_.tempo_bpm /
                                           (60.0F * parameters_.beats_per_cycle)
                                     : parameters_.rate_hz;
    effective_rate_hz_ = std::clamp(requested_rate,
                                    kMinimumRateHz, kMaximumRateHz);
    phase_increment_ = effective_rate_hz_ / sample_rate_;
    const float step_radians = kTwoPi * phase_increment_;
    sine_increment_ = std::sin(step_radians);
    cosine_increment_ = std::cos(step_radians);
}

void BiasTrem::prepare_detector_and_output() noexcept {
    attack_coefficient_ = one_pole_coefficient(sample_rate_,
                                               kDetectorAttackSeconds);
    release_coefficient_ = one_pole_coefficient(sample_rate_,
                                                kDetectorReleaseSeconds);
    trigger_threshold_linear_ = db_to_linear(parameters_.trigger_threshold_db);
    output_gain_ = db_to_linear(parameters_.level_db);
    left_gain_ = parameters_.balance > 0.0F
                     ? 1.0F - parameters_.balance
                     : 1.0F;
    right_gain_ = parameters_.balance < 0.0F
                      ? 1.0F + parameters_.balance
                      : 1.0F;
}

void BiasTrem::reset_phase() noexcept {
    phase_ = 0.0F;
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    normalization_counter_ = 0;
}

float BiasTrem::next_lfo_value() noexcept {
    float value = sine_phase_;
    switch (parameters_.waveform) {
    case BiasTremWaveform::Sine:
        value = sine_phase_;
        break;
    case BiasTremWaveform::Triangle:
        value = phase_ < 0.5F ? -1.0F + 4.0F * phase_
                              : 3.0F - 4.0F * phase_;
        break;
    case BiasTremWaveform::Square:
        value = phase_ < 0.5F ? 1.0F : -1.0F;
        break;
    case BiasTremWaveform::SawUp:
        value = 2.0F * phase_ - 1.0F;
        break;
    case BiasTremWaveform::SawDown:
        value = 1.0F - 2.0F * phase_;
        break;
    case BiasTremWaveform::SampleHold:
        value = sample_hold_value_;
        break;
    }

    phase_ += phase_increment_;
    if (phase_ >= 1.0F) {
        phase_ -= 1.0F;
        if (parameters_.waveform == BiasTremWaveform::SampleHold) {
            const std::uint32_t random = next_random();
            sample_hold_value_ =
                static_cast<float>(random >> 8U) * (2.0F / 16777215.0F) - 1.0F;
        }
    }

    const float old_sine = sine_phase_;
    sine_phase_ = old_sine * cosine_increment_ +
                  cosine_phase_ * sine_increment_;
    cosine_phase_ = cosine_phase_ * cosine_increment_ -
                    old_sine * sine_increment_;
    if (++normalization_counter_ == 4096U) {
        const float magnitude_squared = sine_phase_ * sine_phase_ +
                                        cosine_phase_ * cosine_phase_;
        if (std::isfinite(magnitude_squared) && magnitude_squared > 0.0F) {
            const float inverse_magnitude = 1.0F / std::sqrt(magnitude_squared);
            sine_phase_ *= inverse_magnitude;
            cosine_phase_ *= inverse_magnitude;
        } else {
            sine_phase_ = 0.0F;
            cosine_phase_ = 1.0F;
        }
        normalization_counter_ = 0;
    }
    // The recursive sine oscillator can drift by a few float ulps between
    // periodic normalizations. Keep the documented modulation gain bounded.
    return std::clamp(value, -1.0F, 1.0F);
}

std::uint32_t BiasTrem::next_random() noexcept {
    random_state_ ^= random_state_ << 13U;
    random_state_ ^= random_state_ >> 17U;
    random_state_ ^= random_state_ << 5U;
    return random_state_;
}

} // namespace tabletfx
