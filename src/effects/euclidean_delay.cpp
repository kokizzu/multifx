#include "tabletfx/effects/euclidean_delay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {

EuclideanDelay::EuclideanDelay(float sample_rate,
                               EuclideanDelayParameters parameters)
    : sample_rate_(sample_rate) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate) {
        throw std::invalid_argument(
            "Euclidean Delay requires a sample rate between 8 and 384 kHz");
    }

    constexpr std::size_t maximum_stereo_frames =
        kMaximumStorageBytes / (2U * sizeof(float));
    const double requested_frames = std::ceil(
        static_cast<double>(sample_rate_) * kMaximumStepTimeMs / 1000.0) + 2.0;
    const std::size_t requested_capacity =
        static_cast<std::size_t>(requested_frames);
    capacity_frames_ = std::min(requested_capacity, maximum_stereo_frames);
    if (capacity_frames_ < 4U ||
        capacity_frames_ > storage_.max_size() / 2U) {
        throw std::length_error("Euclidean Delay ring capacity is invalid");
    }
    storage_.assign(capacity_frames_ * 2U, 0.0F);
    if (storage_.capacity() > kMaximumStorageBytes / sizeof(float)) {
        throw std::length_error("Euclidean Delay ring exceeded its memory limit");
    }

    configure(parameters);
    reset();
}

float EuclideanDelay::maximum_step_time_ms() const noexcept {
    if (capacity_frames_ <= 2U) return 0.0F;
    return static_cast<float>(capacity_frames_ - 2U) * 1000.0F / sample_rate_;
}

void EuclideanDelay::configure(EuclideanDelayParameters parameters) noexcept {
    const float maximum_time =
        std::min(kMaximumStepTimeMs, maximum_step_time_ms());
    parameters.step_time_ms = bounded(parameters.step_time_ms,
                                      parameters_.step_time_ms,
                                      kMinimumStepTimeMs, maximum_time);
    parameters.steps = std::clamp(parameters.steps, kMinimumSteps,
                                  kMaximumSteps);
    parameters.pulses = std::min(parameters.pulses, parameters.steps);
    parameters.rotation = static_cast<std::uint8_t>(
        parameters.rotation % parameters.steps);
    parameters.feedback = bounded(parameters.feedback, parameters_.feedback,
                                  0.0F, kMaximumFeedback);
    parameters.mix = bounded(parameters.mix, parameters_.mix, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, parameters_.level_db,
                                   kMinimumLevelDb, kMaximumLevelDb);
    const float maximum_cut_hz = std::min(20000.0F, sample_rate_ * 0.45F);
    parameters.low_cut_hz = bounded(
        parameters.low_cut_hz, parameters_.low_cut_hz, 20.0F,
        std::min(2000.0F, maximum_cut_hz));
    parameters.high_cut_hz = bounded(
        parameters.high_cut_hz, parameters_.high_cut_hz, 1000.0F,
        maximum_cut_hz);

    const float target_delay = std::clamp(
        parameters.step_time_ms * sample_rate_ / 1000.0F, 1.0F,
        static_cast<float>(capacity_frames_ - 2U));
    const float target_phase_increment = 1.0F / target_delay;
    const float target_level_gain =
        std::pow(10.0F, parameters.level_db / 20.0F);
    const bool pattern_changed =
        parameters.steps != parameters_.steps ||
        parameters.pulses != parameters_.pulses ||
        parameters.rotation != parameters_.rotation;

    const bool parameters_changed =
        parameters.step_time_ms != parameters_.step_time_ms ||
        parameters.steps != parameters_.steps ||
        parameters.pulses != parameters_.pulses ||
        parameters.rotation != parameters_.rotation ||
        parameters.feedback != parameters_.feedback ||
        parameters.mix != parameters_.mix ||
        parameters.level_db != parameters_.level_db ||
        parameters.low_cut_hz != parameters_.low_cut_hz ||
        parameters.high_cut_hz != parameters_.high_cut_hz ||
        parameters.trails != parameters_.trails;
    if (!parameters_changed && initialized_) return;

    parameters_ = parameters;
    constexpr float kTwoPi = 6.2831853071795864769F;
    low_cut_enabled_ = parameters_.low_cut_hz > 20.0F;
    high_cut_enabled_ = parameters_.high_cut_hz < maximum_cut_hz;
    low_cut_alpha_ = std::exp(
        -kTwoPi * parameters_.low_cut_hz / sample_rate_);
    high_cut_alpha_ = std::exp(
        -kTwoPi * parameters_.high_cut_hz / sample_rate_);
    if (!initialized_) {
        current_delay_frames_ = target_delay;
        target_delay_frames_ = target_delay;
        current_phase_increment_ = target_phase_increment;
        target_phase_increment_ = target_phase_increment;
        current_feedback_ = parameters.feedback;
        current_mix_ = parameters.mix;
        current_level_gain_ = target_level_gain;
        initialized_ = true;
        restart_pattern();
        return;
    }

    target_delay_frames_ = target_delay;
    target_phase_increment_ = target_phase_increment;
    start_ramp(target_delay_frames_, current_delay_frames_,
               delay_frames_step_, delay_ramp_remaining_);
    start_ramp(target_phase_increment_, current_phase_increment_,
               phase_increment_step_, phase_increment_ramp_remaining_);
    start_ramp(parameters.feedback, current_feedback_, feedback_step_,
               feedback_ramp_remaining_);
    start_ramp(parameters.mix, current_mix_, mix_step_, mix_ramp_remaining_);
    start_ramp(target_level_gain, current_level_gain_, level_gain_step_,
               level_ramp_remaining_);
    if (pattern_changed) restart_pattern();
}

void EuclideanDelay::process(std::span<float> interleaved,
                             std::uint16_t channels) noexcept {
    if ((channels != 1U && channels != 2U) || interleaved.empty() ||
        capacity_frames_ == 0U) {
        return;
    }

    const std::size_t channel_count = static_cast<std::size_t>(channels);
    const std::size_t frames = interleaved.size() / channel_count;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        advance_pattern();
        // A step boundary can coincide with the first sample returned by the
        // delay line. Apply the newly selected gate ramp on that sample so an
        // exact-boundary pulse is not lost before its fade-in begins.
        advance_ramps();
        const std::size_t offset = frame * channel_count;
        for (std::size_t channel = 0U; channel < channel_count; ++channel) {
            const float dry = safe_sample(interleaved[offset + channel]);
            const float delayed = read_delay(channel, current_delay_frames_);
            float filtered = delayed;
            if (low_cut_enabled_) {
                const float output = low_cut_alpha_ *
                    (low_cut_previous_output_[channel] + filtered -
                     low_cut_previous_input_[channel]);
                low_cut_previous_input_[channel] = filtered;
                low_cut_previous_output_[channel] = output;
                filtered = output;
            }
            if (high_cut_enabled_) {
                const float output = (1.0F - high_cut_alpha_) * filtered +
                    high_cut_alpha_ * high_cut_state_[channel];
                high_cut_state_[channel] = output;
                filtered = output;
            }
            const float wet = filtered * gate_gain_;
            const float mixed = dry + (wet - dry) * current_mix_;
            interleaved[offset + channel] = safe_sample(
                mixed * current_level_gain_);

            storage_[write_frame_ * 2U + channel] = safe_sample(
                dry + filtered * current_feedback_);
        }
        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
    }
}

void EuclideanDelay::process_bypassed(std::span<float> interleaved,
                                      std::uint16_t channels) noexcept {
    if (!parameters_.trails || (channels != 1U && channels != 2U) ||
        interleaved.empty() || capacity_frames_ == 0U) {
        return;
    }

    const std::size_t channel_count = static_cast<std::size_t>(channels);
    const std::size_t frames = interleaved.size() / channel_count;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        advance_pattern();
        advance_ramps();
        const std::size_t offset = frame * channel_count;
        for (std::size_t channel = 0U; channel < channel_count; ++channel) {
            const float dry = safe_sample(interleaved[offset + channel]);
            float filtered = read_delay(channel, current_delay_frames_);
            if (low_cut_enabled_) {
                const float output = low_cut_alpha_ *
                    (low_cut_previous_output_[channel] + filtered -
                     low_cut_previous_input_[channel]);
                low_cut_previous_input_[channel] = filtered;
                low_cut_previous_output_[channel] = output;
                filtered = output;
            }
            if (high_cut_enabled_) {
                const float output = (1.0F - high_cut_alpha_) * filtered +
                    high_cut_alpha_ * high_cut_state_[channel];
                high_cut_state_[channel] = output;
                filtered = output;
            }
            const float wet = filtered * gate_gain_;
            const float wet_output = wet * current_mix_ * current_level_gain_;
            interleaved[offset + channel] = safe_sample(dry + wet_output);
            storage_[write_frame_ * 2U + channel] = safe_sample(
                filtered * current_feedback_);
        }
        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
    }
}

void EuclideanDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0U;
    phase_ = 0.0F;
    current_delay_frames_ = target_delay_frames_;
    delay_frames_step_ = 0.0F;
    delay_ramp_remaining_ = 0U;
    current_phase_increment_ = target_phase_increment_;
    phase_increment_step_ = 0.0F;
    phase_increment_ramp_remaining_ = 0U;
    current_feedback_ = parameters_.feedback;
    low_cut_previous_input_.fill(0.0F);
    low_cut_previous_output_.fill(0.0F);
    high_cut_state_.fill(0.0F);
    feedback_step_ = 0.0F;
    feedback_ramp_remaining_ = 0U;
    current_mix_ = parameters_.mix;
    mix_step_ = 0.0F;
    mix_ramp_remaining_ = 0U;
    current_level_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    level_gain_step_ = 0.0F;
    level_ramp_remaining_ = 0U;
    restart_pattern();
}

float EuclideanDelay::bounded(float value, float fallback, float minimum,
                              float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

float EuclideanDelay::safe_sample(float sample) noexcept {
    if (!std::isfinite(sample)) return 0.0F;
    return std::clamp(sample, -kMaximumSampleMagnitude,
                      kMaximumSampleMagnitude);
}

bool EuclideanDelay::euclidean_hit(std::uint8_t step, std::uint8_t steps,
                                   std::uint8_t pulses,
                                   std::uint8_t rotation) noexcept {
    if (steps == 0U || pulses == 0U) return false;
    const unsigned int pattern_step =
        (static_cast<unsigned int>(step) + steps - rotation) % steps;
    return (pattern_step * pulses) % steps < pulses;
}

std::size_t EuclideanDelay::ring_index_behind(std::size_t index,
                                               std::size_t frames) const noexcept {
    frames %= capacity_frames_;
    return (index + capacity_frames_ - frames) % capacity_frames_;
}

float EuclideanDelay::read_delay(std::size_t channel,
                                 float delay_frames) const noexcept {
    const float bounded_delay = std::clamp(
        delay_frames, 1.0F, static_cast<float>(capacity_frames_ - 2U));
    const float whole_frames_float = std::floor(bounded_delay);
    const std::size_t whole_frames =
        static_cast<std::size_t>(whole_frames_float);
    const float fraction = bounded_delay - whole_frames_float;
    const std::size_t newer_index = ring_index_behind(write_frame_, whole_frames);
    const std::size_t older_index =
        ring_index_behind(write_frame_, whole_frames + 1U);
    const float newer = safe_sample(storage_[newer_index * 2U + channel]);
    const float older = safe_sample(storage_[older_index * 2U + channel]);
    return safe_sample(newer + (older - newer) * fraction);
}

std::size_t EuclideanDelay::control_ramp_frames() const noexcept {
    return std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(
                sample_rate_ * kParameterRampMs / 1000.0F)));
}

void EuclideanDelay::restart_pattern() noexcept {
    phase_ = 0.0F;
    current_step_ = static_cast<std::uint8_t>(parameters_.steps - 1U);
    gate_gain_ = 0.0F;
    gate_target_ = 0.0F;
    gate_step_ = 0.0F;
    gate_ramp_remaining_ = 0U;
}

void EuclideanDelay::start_ramp(float target, float& current, float& step,
                                std::size_t& remaining) noexcept {
    remaining = control_ramp_frames();
    step = (target - current) / static_cast<float>(remaining);
    if (target == current) {
        step = 0.0F;
        remaining = 0U;
    }
}

void EuclideanDelay::advance_ramps() noexcept {
    if (delay_ramp_remaining_ > 0U) {
        current_delay_frames_ += delay_frames_step_;
        if (--delay_ramp_remaining_ == 0U) {
            current_delay_frames_ = target_delay_frames_;
        }
    }
    if (phase_increment_ramp_remaining_ > 0U) {
        current_phase_increment_ += phase_increment_step_;
        if (--phase_increment_ramp_remaining_ == 0U) {
            current_phase_increment_ = target_phase_increment_;
        }
    }
    if (feedback_ramp_remaining_ > 0U) {
        current_feedback_ += feedback_step_;
        if (--feedback_ramp_remaining_ == 0U) {
            current_feedback_ = parameters_.feedback;
        }
    }
    if (mix_ramp_remaining_ > 0U) {
        current_mix_ += mix_step_;
        if (--mix_ramp_remaining_ == 0U) current_mix_ = parameters_.mix;
    }
    if (level_ramp_remaining_ > 0U) {
        current_level_gain_ += level_gain_step_;
        if (--level_ramp_remaining_ == 0U) {
            current_level_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
        }
    }
    if (gate_ramp_remaining_ > 0U) {
        gate_gain_ += gate_step_;
        if (--gate_ramp_remaining_ == 0U) gate_gain_ = gate_target_;
    }
}

void EuclideanDelay::advance_pattern() noexcept {
    phase_ += current_phase_increment_;
    if (phase_ < 1.0F) return;

    phase_ -= 1.0F;
    current_step_ = static_cast<std::uint8_t>(
        (static_cast<unsigned int>(current_step_) + 1U) % parameters_.steps);
    gate_target_ = euclidean_hit(current_step_, parameters_.steps,
                                 parameters_.pulses, parameters_.rotation)
                       ? 1.0F
                       : 0.0F;
    gate_ramp_remaining_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(
                sample_rate_ * kGateRampMs / 1000.0F)));
    gate_step_ = (gate_target_ - gate_gain_) /
                 static_cast<float>(gate_ramp_remaining_);
}

} // namespace tabletfx
