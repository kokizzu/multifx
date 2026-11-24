#include "tabletfx/effects/vibratone.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kHysteresisRatio = 0.75F;

float bounded_db_to_gain(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

} // namespace

Vibratone::Vibratone(float sample_rate, std::uint16_t channels,
                     VibratoneParameters parameters)
    : sample_rate_(sample_rate), channels_(channels), rotary_(sample_rate, channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument(
            "Vibratone requires 8–384 kHz and mono or stereo channels");
    }
    attack_coefficient_ = 1.0F - std::exp(-1.0F / (sample_rate_ * 0.020F));
    release_coefficient_ = 1.0F - std::exp(-1.0F / (sample_rate_ * 0.250F));
    set_parameters(parameters);
    reset();
}

std::size_t Vibratone::memory_bytes() const noexcept {
    return sizeof(*this) + (rotary_.memory_bytes() - sizeof(RotarySpeaker));
}

float Vibratone::bounded(float value, float fallback, float minimum,
                         float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

void Vibratone::prepare_tone_filter() noexcept {
    // Exponential 800 Hz–20 kHz project range. At maximum Tone, skip filtering.
    if (parameters_.tone >= 0.9999F) {
        tone_alpha_ = 1.0F;
        return;
    }
    const float cutoff_hz = 800.0F * std::pow(25.0F, parameters_.tone);
    const float coefficient = 1.0F - std::exp(-2.0F * kPi * cutoff_hz / sample_rate_);
    tone_alpha_ = std::clamp(coefficient, 0.0F, 1.0F);
}

void Vibratone::apply_rotor_speed(bool fast) noexcept {
    if (applied_speed_fast_ == fast) {
        return;
    }
    RotarySpeakerParameters rotary_parameters = rotary_.parameters();
    const float rate = parameters_.fast_speed_hz;
    rotary_parameters.rate_hz = rate;
    rotary_parameters.slow_speed_multiplier =
        std::clamp(parameters_.slow_speed_hz / rate, 0.02F, 0.5F);
    rotary_parameters.speed = fast ? RotarySpeed::fast : RotarySpeed::slow;
    rotary_parameters.drive = parameters_.drive;
    rotary_parameters.low_time_constant_ms = parameters_.ramp_ms;
    rotary_parameters.hi_time_constant_ms = parameters_.ramp_ms;
    rotary_.set_parameters(rotary_parameters);
    applied_speed_fast_ = fast;
}

void Vibratone::set_parameters(VibratoneParameters parameters) noexcept {
    parameters.drive = bounded(parameters.drive, parameters_.drive, 0.0F, 1.0F);
    parameters.tone = bounded(parameters.tone, parameters_.tone, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, parameters_.level_db,
                                   -24.0F, 12.0F);
    parameters.fast_speed_hz = bounded(parameters.fast_speed_hz,
                                        parameters_.fast_speed_hz,
                                        1.0F, kMaximumSpeedHz);
    parameters.slow_speed_hz = bounded(parameters.slow_speed_hz,
                                       parameters_.slow_speed_hz,
                                       kMinimumSpeedHz, 5.0F);
    parameters.slow_speed_hz = std::min(parameters.slow_speed_hz,
                                        parameters.fast_speed_hz * 0.5F);
    parameters.ramp_ms = bounded(parameters.ramp_ms, parameters_.ramp_ms,
                                 kMinimumRampMs, kMaximumRampMs);
    parameters.sensitivity = bounded(parameters.sensitivity,
                                     parameters_.sensitivity, 0.0F, 1.0F);
    parameters_ = parameters;

    output_gain_ = bounded_db_to_gain(parameters_.level_db);
    const float threshold_db = -5.0F - 40.0F * parameters_.sensitivity;
    sensitivity_threshold_ = bounded_db_to_gain(threshold_db);
    prepare_tone_filter();

    RotarySpeakerParameters rotary_parameters = rotary_.parameters();
    rotary_parameters.rate_hz = parameters_.fast_speed_hz;
    rotary_parameters.slow_speed_multiplier = std::clamp(
        parameters_.slow_speed_hz / parameters_.fast_speed_hz, 0.02F, 0.5F);
    rotary_parameters.drive = parameters_.drive;
    rotary_parameters.low_time_constant_ms = parameters_.ramp_ms;
    rotary_parameters.hi_time_constant_ms = parameters_.ramp_ms;
    const bool requested_fast = parameters_.dynamics_switch
                                    ? applied_speed_fast_
                                    : parameters_.speed_fast;
    rotary_parameters.speed = requested_fast ? RotarySpeed::fast
                                              : RotarySpeed::slow;
    rotary_.set_parameters(rotary_parameters);
    applied_speed_fast_ = requested_fast;
}

void Vibratone::process(std::span<float> interleaved,
                        std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) {
        return;
    }

    if (parameters_.dynamics_switch) {
        const std::size_t frames = interleaved.size() / channels_;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const std::size_t offset = frame * channels_;
            float magnitude = 0.0F;
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                const float sample = std::isfinite(interleaved[offset + channel])
                                         ? interleaved[offset + channel]
                                         : 0.0F;
                magnitude = std::max(magnitude, std::abs(sample));
            }
            const float coefficient = magnitude > envelope_
                                          ? attack_coefficient_
                                          : release_coefficient_;
            envelope_ += coefficient * (magnitude - envelope_);
        }

        bool fast = applied_speed_fast_;
        if (!fast && envelope_ >= sensitivity_threshold_) {
            fast = true;
        } else if (fast && envelope_ < sensitivity_threshold_ * kHysteresisRatio) {
            fast = false;
        }
        apply_rotor_speed(fast);
    } else {
        envelope_ = 0.0F;
        apply_rotor_speed(parameters_.speed_fast);
    }

    rotary_.process(interleaved, channels_);
    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            float sample = interleaved[offset + channel];
            if (tone_alpha_ < 1.0F) {
                tone_state_[channel] += tone_alpha_ * (sample - tone_state_[channel]);
                sample = tone_state_[channel];
            } else {
                tone_state_[channel] = sample;
            }
            sample *= output_gain_;
            interleaved[offset + channel] = std::isfinite(sample) ? sample : 0.0F;
        }
    }
}

void Vibratone::reset() noexcept {
    rotary_.reset();
    tone_state_.fill(0.0F);
    envelope_ = 0.0F;
    applied_speed_fast_ = false;
    RotarySpeakerParameters rotary_parameters = rotary_.parameters();
    rotary_parameters.rate_hz = parameters_.fast_speed_hz;
    rotary_parameters.slow_speed_multiplier = std::clamp(
        parameters_.slow_speed_hz / parameters_.fast_speed_hz, 0.02F, 0.5F);
    rotary_parameters.speed = RotarySpeed::slow;
    rotary_parameters.drive = parameters_.drive;
    rotary_parameters.low_time_constant_ms = parameters_.ramp_ms;
    rotary_parameters.hi_time_constant_ms = parameters_.ramp_ms;
    rotary_.set_parameters(rotary_parameters);
    if (!parameters_.dynamics_switch) {
        apply_rotor_speed(parameters_.speed_fast);
    }
}

} // namespace tabletfx
