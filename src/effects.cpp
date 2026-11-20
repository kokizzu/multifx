#include "tabletfx/effects.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kMinGainDb = -96.0F;
constexpr float kMaxGainDb = 24.0F;

float decibels_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float time_coefficient(float milliseconds, float sample_rate) noexcept {
    if (milliseconds <= 0.0F || sample_rate <= 0.0F) {
        return 0.0F;
    }
    const float seconds = milliseconds * 0.001F;
    return std::exp(-1.0F / (seconds * sample_rate));
}

} // namespace

Gain::Gain(float gain_db, float sample_rate, float smoothing_ms) noexcept
    : gain_db_(0.0F), gain_(1.0F), target_gain_(1.0F), ramp_step_(0.0F),
      sample_rate_(48000.0F), smoothing_ms_(10.0F) {
    configure_smoothing(sample_rate, smoothing_ms);
    set_gain_db_immediate(gain_db);
}

void Gain::set_gain_db(float gain_db) noexcept {
    if (std::isfinite(gain_db)) {
        gain_db_ = std::clamp(gain_db, kMinGainDb, kMaxGainDb);
    }
    target_gain_ = decibels_to_linear(gain_db_);
    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(smoothing_ms_) / 1000.0;
    const std::size_t ramp_frames = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_frames == 0) {
        gain_ = target_gain_;
        ramp_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }
    ramp_step_ = (target_gain_ - gain_) / static_cast<float>(ramp_frames);
    ramp_remaining_ = ramp_frames;
}

void Gain::set_gain_db_immediate(float gain_db) noexcept {
    if (std::isfinite(gain_db)) {
        gain_db_ = std::clamp(gain_db, kMinGainDb, kMaxGainDb);
    }
    target_gain_ = decibels_to_linear(gain_db_);
    gain_ = target_gain_;
    ramp_step_ = 0.0F;
    ramp_remaining_ = 0;
}

void Gain::process(std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels == 0) {
        return;
    }
    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (ramp_remaining_ > 0) {
            gain_ += ramp_step_;
            --ramp_remaining_;
            if (ramp_remaining_ == 0) {
                gain_ = target_gain_;
                ramp_step_ = 0.0F;
            }
        }
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            interleaved[offset + channel] *= gain_;
        }
    }
}

void Gain::reset() noexcept {
    gain_ = target_gain_;
    ramp_step_ = 0.0F;
    ramp_remaining_ = 0;
}

void Gain::configure_smoothing(float sample_rate, float smoothing_ms) noexcept {
    sample_rate_ = std::isfinite(sample_rate) && sample_rate >= 8000.0F &&
                           sample_rate <= 384000.0F
                       ? sample_rate
                       : 48000.0F;
    smoothing_ms_ = std::isfinite(smoothing_ms)
                        ? std::clamp(smoothing_ms, 0.0F, 1000.0F)
                        : 10.0F;
}

NoiseGate::NoiseGate(float sample_rate, float threshold_db, float attenuation_db,
                     float attack_ms, float release_ms) noexcept
    : sample_rate_(sample_rate), threshold_(0.0F), close_threshold_(0.0F),
      closed_gain_(0.0F),
      attack_coefficient_(0.0F), release_coefficient_(0.0F) {
    configure(sample_rate, threshold_db, attenuation_db, attack_ms, release_ms);
}

NoiseGate::NoiseGate(float sample_rate, NoiseGateParameters parameters) noexcept
    : sample_rate_(sample_rate), threshold_(0.0F), close_threshold_(0.0F),
      closed_gain_(0.0F), attack_coefficient_(0.0F), release_coefficient_(0.0F) {
    configure(sample_rate, parameters);
}

void NoiseGate::configure(float sample_rate, float threshold_db, float attenuation_db,
                          float attack_ms, float release_ms) noexcept {
    NoiseGateParameters parameters;
    parameters.threshold_db = threshold_db;
    parameters.attenuation_db = attenuation_db;
    parameters.attack_ms = attack_ms;
    parameters.release_ms = release_ms;
    configure(sample_rate, parameters);
}

void NoiseGate::configure(float sample_rate, NoiseGateParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) ? std::clamp(sample_rate, 1.0F, 384000.0F)
                                             : 48000.0F;
    const float threshold_db = std::isfinite(parameters.threshold_db)
                                   ? std::clamp(parameters.threshold_db, -120.0F, 0.0F)
                                   : -60.0F;
    const float attenuation_db = std::isfinite(parameters.attenuation_db)
                                     ? std::clamp(parameters.attenuation_db, -96.0F, 0.0F)
                                     : -72.0F;
    const float hysteresis_db = std::isfinite(parameters.hysteresis_db)
                                    ? std::clamp(parameters.hysteresis_db, 0.0F, 24.0F)
                                    : 3.0F;
    const float attack_ms = std::isfinite(parameters.attack_ms)
                                ? std::clamp(parameters.attack_ms, 0.0F, 2000.0F)
                                : 2.0F;
    const float hold_ms = std::isfinite(parameters.hold_ms)
                              ? std::clamp(parameters.hold_ms, 0.0F, 5000.0F)
                              : 20.0F;
    const float release_ms = std::isfinite(parameters.release_ms)
                                 ? std::clamp(parameters.release_ms, 0.0F, 5000.0F)
                                 : 80.0F;

    threshold_ = decibels_to_linear(threshold_db);
    close_threshold_ = decibels_to_linear(threshold_db - hysteresis_db);
    closed_gain_ = decibels_to_linear(attenuation_db);
    attack_coefficient_ = time_coefficient(attack_ms, sample_rate_);
    release_coefficient_ = time_coefficient(release_ms, sample_rate_);
    const double hold_samples = static_cast<double>(sample_rate_) *
                                static_cast<double>(hold_ms) / 1000.0;
    hold_duration_samples_ = static_cast<std::size_t>(std::llround(hold_samples));
    hold_remaining_samples_ = std::min(hold_remaining_samples_, hold_duration_samples_);
}

void NoiseGate::process(std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels == 0) {
        return;
    }
    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        float peak = 0.0F;
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const float sample = interleaved[offset + channel];
            if (std::isfinite(sample)) {
                peak = std::max(peak, std::abs(sample));
            }
        }
        const float envelope_coefficient = peak > envelope_ ? attack_coefficient_ : release_coefficient_;
        envelope_ = envelope_coefficient * envelope_ + (1.0F - envelope_coefficient) * peak;
        if (!gate_open_) {
            if (envelope_ >= threshold_) {
                gate_open_ = true;
                hold_remaining_samples_ = hold_duration_samples_;
            }
        } else if (envelope_ >= close_threshold_) {
            hold_remaining_samples_ = hold_duration_samples_;
        } else if (hold_remaining_samples_ > 0) {
            --hold_remaining_samples_;
        } else {
            gate_open_ = false;
        }

        const float target_gain = gate_open_ ? 1.0F : closed_gain_;
        const float gain_coefficient = target_gain > gain_ ? attack_coefficient_ : release_coefficient_;
        gain_ = gain_coefficient * gain_ + (1.0F - gain_coefficient) * target_gain;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const std::size_t index = offset + channel;
            interleaved[index] = std::isfinite(interleaved[index])
                                     ? interleaved[index] * gain_
                                     : 0.0F;
        }
    }
}

void NoiseGate::reset() noexcept {
    envelope_ = 0.0F;
    gain_ = closed_gain_;
    hold_remaining_samples_ = 0;
    gate_open_ = false;
}

SoftDrive::SoftDrive(float drive) noexcept : drive_(1.0F), normalization_(1.0F) {
    set_drive(drive);
}

void SoftDrive::set_drive(float drive) noexcept {
    drive_ = std::clamp(drive, 1.0F, 20.0F);
    normalization_ = std::tanh(drive_);
}

void SoftDrive::process(std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels == 0 || drive_ <= 1.0001F) {
        return;
    }
    for (float& sample : interleaved) {
        sample = std::tanh(sample * drive_) / normalization_;
    }
}

Delay::Delay(float sample_rate, std::uint16_t channels, float max_delay_ms,
             float delay_ms, float feedback, float mix)
    : sample_rate_(sample_rate), channels_(channels), delay_samples_(1), feedback_(0.0F),
      mix_(0.0F), write_frame_(0), capacity_frames_(0) {
    if (sample_rate <= 0.0F || channels == 0 || max_delay_ms <= 0.0F) {
        throw std::invalid_argument("Delay requires a positive sample rate, channel count, and maximum delay");
    }
    const double maximum_frames = std::ceil(static_cast<double>(sample_rate_) * max_delay_ms / 1000.0);
    if (maximum_frames > static_cast<double>(std::numeric_limits<std::size_t>::max() - 1)) {
        throw std::length_error("Delay buffer size is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(maximum_frames) + 1;
    storage_.assign(capacity_frames_ * channels_, 0.0F);
    configure(delay_ms, feedback, mix);
}

void Delay::configure(float delay_ms, float feedback, float mix) noexcept {
    const float maximum_ms = static_cast<float>(capacity_frames_ - 1) * 1000.0F / sample_rate_;
    const float bounded_ms = std::clamp(delay_ms, 1.0F, maximum_ms);
    delay_samples_ = std::clamp(static_cast<std::size_t>(std::llround(bounded_ms * sample_rate_ / 1000.0F)),
                                std::size_t{1}, capacity_frames_ - 1);
    feedback_ = std::clamp(feedback, 0.0F, 0.95F);
    mix_ = std::clamp(mix, 0.0F, 1.0F);
}

void Delay::process(std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0 || mix_ <= 0.0F) {
        return;
    }
    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t read_frame = (write_frame_ + capacity_frames_ - delay_samples_) % capacity_frames_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t input_index = frame * channels_ + channel;
            const std::size_t delay_index = read_frame * channels_ + channel;
            const std::size_t write_index = write_frame_ * channels_ + channel;
            const float input = interleaved[input_index];
            const float delayed = storage_[delay_index];
            interleaved[input_index] = input * (1.0F - mix_) + delayed * mix_;
            storage_[write_index] = input + delayed * feedback_;
        }
        write_frame_ = (write_frame_ + 1) % capacity_frames_;
    }
}

void Delay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
}

} // namespace tabletfx
