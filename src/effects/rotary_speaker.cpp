#include "tabletfx/effects/rotary_speaker.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

// Bound only the signal entering the rotary model. A 16x linear ceiling leaves
// 24 dB of float headroom while keeping crossover, sum, and mid/side math finite
// for hostile finite inputs such as FLT_MAX.
constexpr float kMaximumProcessingSample = 16.0F;

} // namespace

RotarySpeaker::RotarySpeaker(float sample_rate, std::uint16_t channels,
                             RotarySpeakerParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument(
            "RotarySpeaker requires 8–384 kHz and mono or stereo channels");
    }

    const double requested_frames =
        static_cast<double>(sample_rate_) * kMaximumRotorDelayMs / 1000.0;
    delay_capacity_frames_ = static_cast<std::size_t>(std::ceil(requested_frames)) + 2U;
    rotor_delay_buffer_.assign(delay_capacity_frames_ * channels_, 0.0F);
    set_parameters(parameters);
    reset();
}

std::size_t RotarySpeaker::memory_bytes() const noexcept {
    return sizeof(*this) + rotor_delay_buffer_.capacity() * sizeof(float);
}

void RotarySpeaker::set_parameters(RotarySpeakerParameters parameters) noexcept {
    parameters.rate_hz = bounded(parameters.rate_hz, parameters_.rate_hz,
                                 kMinimumRateHz, kMaximumRateHz);
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, parameters_.tempo_bpm,
                                   kMinimumTempoBpm, kMaximumTempoBpm);
    parameters.beats_per_rotation = bounded(
        parameters.beats_per_rotation, parameters_.beats_per_rotation,
        kMinimumBeatsPerRotation, kMaximumBeatsPerRotation);
    parameters.speed = sanitize_speed(parameters.speed);
    parameters.slow_speed_multiplier = bounded(
        parameters.slow_speed_multiplier, parameters_.slow_speed_multiplier,
        0.02F, 0.5F);
    parameters.drive = bounded(parameters.drive, parameters_.drive, 0.0F, 1.0F);
    parameters.hi_depth = bounded(parameters.hi_depth, parameters_.hi_depth,
                                  0.0F, 1.0F);
    parameters.high_level = bounded(parameters.high_level, parameters_.high_level,
                                     0.0F, 2.0F);
    parameters.low_depth = bounded(parameters.low_depth, parameters_.low_depth,
                                   0.0F, 1.0F);
    parameters.rotor_length = bounded(parameters.rotor_length,
                                      parameters_.rotor_length, 0.0F, 1.0F);
    parameters.low_rate_multi = bounded(parameters.low_rate_multi,
                                        parameters_.low_rate_multi, 0.0F, 1.0F);
    parameters.low_time_constant_ms = bounded(
        parameters.low_time_constant_ms, parameters_.low_time_constant_ms,
        kMinimumTimeConstantMs, kMaximumTimeConstantMs);
    parameters.hi_time_constant_ms = bounded(
        parameters.hi_time_constant_ms, parameters_.hi_time_constant_ms,
        kMinimumTimeConstantMs, kMaximumTimeConstantMs);
    parameters.low_mic_spacing = bounded(
        parameters.low_mic_spacing, parameters_.low_mic_spacing, 0.0F, 1.0F);
    parameters.high_mic_spacing = bounded(
        parameters.high_mic_spacing, parameters_.high_mic_spacing, 0.0F, 1.0F);
    parameters.mic_distance = bounded(parameters.mic_distance,
                                      parameters_.mic_distance, 0.0F, 1.0F);
    parameters.stereo_spread = bounded(parameters.stereo_spread,
                                       parameters_.stereo_spread, -2.0F, 2.0F);
    parameters.mix = bounded(parameters.mix, parameters_.mix, 0.0F, 1.0F);
    parameters.level = bounded(parameters.level, parameters_.level, 0.0F, 2.0F);
    parameters.balance = bounded(parameters.balance, parameters_.balance,
                                 -1.0F, 1.0F);
    parameters.input_select = sanitize_input_select(parameters.input_select);
    parameters_ = parameters;
    prepare_parameters();
}

void RotarySpeaker::process(std::span<float> interleaved,
                            std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        std::array<float, 2> input{};
        std::array<float, 2> low_band{};
        std::array<float, 2> high_band{};
        std::array<float, 2> wet{};
        const std::size_t offset = frame * channels_;

        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const float sample = std::isfinite(interleaved[offset + channel])
                                     ? std::clamp(interleaved[offset + channel],
                                                  -kMaximumProcessingSample,
                                                  kMaximumProcessingSample)
                                     : 0.0F;
            input[channel] = process_input(sample);
        }

        if (channels_ == 2) {
            const float left = input[0];
            const float right = input[1];
            switch (parameters_.input_select) {
            case RotaryInputSelect::stereo:
                break;
            case RotaryInputSelect::left:
                input[1] = left;
                break;
            case RotaryInputSelect::right:
                input[0] = right;
                break;
            case RotaryInputSelect::sum: {
                const float sum = 0.5F * (left + right);
                input[0] = sum;
                input[1] = sum;
                break;
            }
            }
        }

        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            crossover_state_[channel] += crossover_alpha_ *
                (input[channel] - crossover_state_[channel]);
            low_band[channel] = crossover_state_[channel];
            high_band[channel] = input[channel] - low_band[channel];
            rotor_delay_buffer_[delay_write_frame_ * channels_ + channel] =
                high_band[channel];
        }

        const float distance_scale = 1.0F - 0.5F * parameters_.mic_distance;
        const float modulation_depth_scale =
            1.0F - 0.4F * parameters_.mic_distance;
        const float delay_depth_samples = parameters_.rotor_length *
            (kMaximumRotorDelayMs * sample_rate_ / 1000.0F) * distance_scale;

        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const float low_cosine =
                phase_cosine_for_channel(low_phase_cycles_, channel, false);
            const float high_sine =
                phase_sine_for_channel(hi_phase_cycles_, channel, true);
            const float high_cosine =
                phase_cosine_for_channel(hi_phase_cycles_, channel, true);
            const float low_modulation = 0.5F * (1.0F + low_cosine);
            const float high_modulation = 0.5F * (1.0F + high_cosine);
            const float low_depth = parameters_.low_depth * modulation_depth_scale;
            const float high_depth = parameters_.hi_depth * modulation_depth_scale;
            const float low_gain = 1.0F - low_depth * (1.0F - low_modulation);
            const float high_gain = 1.0F - high_depth * (1.0F - high_modulation);
            const float delay_samples = delay_depth_samples *
                                       (0.5F + 0.5F * high_sine);
            const float delayed_high = read_rotor_delay(channel, delay_samples);
            const float high_distance_gain = 1.0F - 0.2F * parameters_.mic_distance;
            wet[channel] = low_band[channel] * low_gain +
                           delayed_high * high_gain * parameters_.high_level *
                               high_distance_gain;
        }

        if (channels_ == 2) {
            const float mid = 0.5F * (wet[0] + wet[1]);
            const float side = 0.5F * (wet[0] - wet[1]) * parameters_.stereo_spread;
            wet[0] = mid + side;
            wet[1] = mid - side;
            if (parameters_.balance > 0.0F) {
                wet[0] *= 1.0F - parameters_.balance;
            } else if (parameters_.balance < 0.0F) {
                wet[1] *= 1.0F + parameters_.balance;
            }
        }

        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const float dry = std::isfinite(interleaved[offset + channel])
                                  ? interleaved[offset + channel]
                                  : 0.0F;
            const float effected = dry +
                (wet[channel] * parameters_.level - dry) * parameters_.mix;
            const float output = parameters_.bypass ? dry : effected;
            interleaved[offset + channel] = std::isfinite(output) ? output : 0.0F;
        }

        delay_write_frame_ = (delay_write_frame_ + 1U) % delay_capacity_frames_;
        update_motor();
    }
}

void RotarySpeaker::reset() noexcept {
    std::fill(rotor_delay_buffer_.begin(), rotor_delay_buffer_.end(), 0.0F);
    crossover_state_ = {};
    delay_write_frame_ = 0;
    low_phase_cycles_ = 0.0F;
    hi_phase_cycles_ = 0.0F;
    current_low_rate_hz_ = 0.0F;
    current_hi_rate_hz_ = 0.0F;
}

float RotarySpeaker::bounded(float value, float fallback,
                             float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float RotarySpeaker::sine_from_cycles(float cycles) noexcept {
    float angle = cycles * kTwoPi;
    if (angle > kPi) {
        angle -= kTwoPi;
    }
    // Fast, low-error sine approximation for the two free-running rotors.
    const float absolute = std::abs(angle);
    float value = 1.273239545F * angle - 0.405284735F * angle * absolute;
    value = 0.225F * (value * std::abs(value) - value) + value;
    return value;
}

float RotarySpeaker::cosine_from_cycles(float cycles) noexcept {
    float shifted = cycles + 0.25F;
    if (shifted >= 1.0F) {
        shifted -= 1.0F;
    }
    return sine_from_cycles(shifted);
}

RotarySpeed RotarySpeaker::sanitize_speed(RotarySpeed speed) noexcept {
    switch (speed) {
    case RotarySpeed::slow:
    case RotarySpeed::fast:
        return speed;
    }
    return RotarySpeed::slow;
}

RotaryInputSelect RotarySpeaker::sanitize_input_select(
    RotaryInputSelect input_select) noexcept {
    switch (input_select) {
    case RotaryInputSelect::stereo:
    case RotaryInputSelect::left:
    case RotaryInputSelect::right:
    case RotaryInputSelect::sum:
        return input_select;
    }
    return RotaryInputSelect::stereo;
}

float RotarySpeaker::rate_target_hz() const noexcept {
    const float base_rate = parameters_.tempo_sync
        ? parameters_.tempo_bpm / (60.0F * parameters_.beats_per_rotation)
        : parameters_.rate_hz;
    const float mode_rate = parameters_.speed == RotarySpeed::slow
        ? base_rate * parameters_.slow_speed_multiplier
        : base_rate;
    return std::clamp(mode_rate, kMinimumRateHz, kMaximumRateHz);
}

void RotarySpeaker::prepare_parameters() noexcept {
    const float base_rate = rate_target_hz();
    target_hi_rate_hz_ = base_rate;
    target_low_rate_hz_ = std::clamp(base_rate * parameters_.low_rate_multi,
                                     0.0F, kMaximumRateHz);

    const float low_time_seconds = parameters_.low_time_constant_ms / 1000.0F;
    const float hi_time_seconds = parameters_.hi_time_constant_ms / 1000.0F;
    low_acceleration_coefficient_ =
        1.0F - std::exp(-1.0F / (low_time_seconds * sample_rate_));
    hi_acceleration_coefficient_ =
        1.0F - std::exp(-1.0F / (hi_time_seconds * sample_rate_));
    crossover_alpha_ = 1.0F -
        std::exp(-kTwoPi * kCrossoverHz / sample_rate_);
    drive_gain_ = 1.0F + 4.0F * parameters_.drive;
    drive_normalization_ = std::tanh(drive_gain_);

    for (std::uint16_t channel = 0; channel < 2; ++channel) {
        const float direction = channel == 0 ? -1.0F : 1.0F;
        const float low_offset_radians =
            direction * 0.25F * parameters_.low_mic_spacing * kTwoPi;
        const float high_offset_radians =
            direction * 0.25F * parameters_.high_mic_spacing * kTwoPi;
        low_mic_offset_sine_[channel] = std::sin(low_offset_radians);
        low_mic_offset_cosine_[channel] = std::cos(low_offset_radians);
        high_mic_offset_sine_[channel] = std::sin(high_offset_radians);
        high_mic_offset_cosine_[channel] = std::cos(high_offset_radians);
    }
}

void RotarySpeaker::update_motor() noexcept {
    current_hi_rate_hz_ += hi_acceleration_coefficient_ *
        (target_hi_rate_hz_ - current_hi_rate_hz_);
    current_low_rate_hz_ += low_acceleration_coefficient_ *
        (target_low_rate_hz_ - current_low_rate_hz_);

    if (std::abs(target_hi_rate_hz_ - current_hi_rate_hz_) < 1.0e-6F) {
        current_hi_rate_hz_ = target_hi_rate_hz_;
    }
    if (std::abs(target_low_rate_hz_ - current_low_rate_hz_) < 1.0e-6F) {
        current_low_rate_hz_ = target_low_rate_hz_;
    }

    advance_phase(hi_phase_cycles_, current_hi_rate_hz_);
    advance_phase(low_phase_cycles_, current_low_rate_hz_);
}

float RotarySpeaker::process_input(float input) const noexcept {
    if (parameters_.drive <= 0.0F) {
        return input;
    }
    return drive_normalization_ > 0.0F
        ? std::tanh(input * drive_gain_) / drive_normalization_
        : input;
}

float RotarySpeaker::read_rotor_delay(std::uint16_t channel,
                                     float delay_samples) const noexcept {
    const float bounded_delay = std::clamp(
        delay_samples, 0.0F,
        static_cast<float>(delay_capacity_frames_ - 2U));
    const std::size_t whole_samples = static_cast<std::size_t>(bounded_delay);
    const float fraction = bounded_delay - static_cast<float>(whole_samples);
    const std::size_t newer_frame =
        (delay_write_frame_ + delay_capacity_frames_ - whole_samples) %
        delay_capacity_frames_;
    const std::size_t older_frame =
        (newer_frame + delay_capacity_frames_ - 1U) % delay_capacity_frames_;
    const float newer = rotor_delay_buffer_[newer_frame * channels_ + channel];
    const float older = rotor_delay_buffer_[older_frame * channels_ + channel];
    return newer + (older - newer) * fraction;
}

float RotarySpeaker::phase_sine_for_channel(float phase,
                                           std::uint16_t channel,
                                           bool high_rotor) const noexcept {
    const auto& sine = high_rotor ? high_mic_offset_sine_ : low_mic_offset_sine_;
    const auto& cosine = high_rotor ? high_mic_offset_cosine_ : low_mic_offset_cosine_;
    const float offset_sine = sine[channel];
    const float offset_cosine = cosine[channel];
    return sine_from_cycles(phase) * offset_cosine +
           cosine_from_cycles(phase) * offset_sine;
}

float RotarySpeaker::phase_cosine_for_channel(float phase,
                                              std::uint16_t channel,
                                              bool high_rotor) const noexcept {
    const auto& sine = high_rotor ? high_mic_offset_sine_ : low_mic_offset_sine_;
    const auto& cosine = high_rotor ? high_mic_offset_cosine_ : low_mic_offset_cosine_;
    const float offset_sine = sine[channel];
    const float offset_cosine = cosine[channel];
    return cosine_from_cycles(phase) * offset_cosine -
           sine_from_cycles(phase) * offset_sine;
}

void RotarySpeaker::advance_phase(float& phase, float rate_hz) const noexcept {
    phase += rate_hz / sample_rate_;
    if (phase >= 1.0F) {
        phase -= 1.0F;
    }
}

} // namespace tabletfx
