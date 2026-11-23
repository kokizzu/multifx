#include "tabletfx/effects/step_tremolo.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {

StepTremolo::StepTremolo(float sample_rate,
                         StepTremoloParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void StepTremolo::configure(float sample_rate,
                            StepTremoloParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;

    parameters.rate_hz = bounded(parameters.rate_hz, 4.0F,
                                 kMinimumRateHz, kMaximumRateHz);
    parameters.depth = bounded(parameters.depth, 0.75F, 0.0F, 1.0F);
    parameters.step_count = static_cast<std::uint8_t>(
        std::clamp<unsigned int>(parameters.step_count, 1U,
                                 static_cast<unsigned int>(kStepTremoloMaximumSteps)));
    for (float& level : parameters.levels) {
        level = bounded(level, 1.0F, 0.0F, 1.0F);
    }
    parameters.transition_ms = bounded(parameters.transition_ms, 4.0F,
                                        0.0F, kMaximumTransitionMs);
    parameters_ = parameters;

    phase_increment_ = parameters_.rate_hz / sample_rate_;
    prepare_transition();
    reset();
}

void StepTremolo::set_rate_hz(float rate_hz) noexcept {
    parameters_.rate_hz = bounded(rate_hz, parameters_.rate_hz,
                                  kMinimumRateHz, kMaximumRateHz);
    phase_increment_ = parameters_.rate_hz / sample_rate_;
    prepare_transition();
}

void StepTremolo::set_depth(float depth) noexcept {
    parameters_.depth = bounded(depth, parameters_.depth, 0.0F, 1.0F);
    begin_transition(current_step_);
}

void StepTremolo::set_step_count(std::uint8_t step_count) noexcept {
    parameters_.step_count = static_cast<std::uint8_t>(
        std::clamp<unsigned int>(step_count, 1U,
                                 static_cast<unsigned int>(kStepTremoloMaximumSteps)));
    prepare_transition();
    begin_transition(step_at_phase(phase_));
}

void StepTremolo::set_step_level(std::uint8_t step, float level) noexcept {
    if (step >= kStepTremoloMaximumSteps) {
        return;
    }
    parameters_.levels[step] = bounded(level, parameters_.levels[step], 0.0F, 1.0F);
    if (step == current_step_) {
        begin_transition(current_step_);
    }
}

void StepTremolo::set_transition_ms(float transition_ms) noexcept {
    parameters_.transition_ms = bounded(transition_ms,
                                        parameters_.transition_ms,
                                        0.0F, kMaximumTransitionMs);
    prepare_transition();
}

void StepTremolo::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::uint8_t next_step = step_at_phase(phase_);
        if (next_step != current_step_) {
            begin_transition(next_step);
        }
        if (ramp_remaining_ > 0) {
            current_gain_ += ramp_step_;
            --ramp_remaining_;
            if (ramp_remaining_ == 0) {
                current_gain_ = target_gain_;
                ramp_step_ = 0.0F;
            }
        }

        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            sample = std::isfinite(sample) ? sample * current_gain_ : 0.0F;
        }

        phase_ += phase_increment_;
        if (phase_ >= 1.0F) {
            phase_ -= 1.0F;
        }
    }
}

void StepTremolo::reset() noexcept {
    phase_ = 0.0F;
    current_step_ = 0;
    current_gain_ = gain_for_step(0);
    target_gain_ = current_gain_;
    ramp_step_ = 0.0F;
    ramp_remaining_ = 0;
}

float StepTremolo::bounded(float value, float fallback,
                           float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

std::uint8_t StepTremolo::step_at_phase(float phase) const noexcept {
    const float normalized = std::clamp(phase, 0.0F, 0.99999994F);
    const auto selected = static_cast<unsigned int>(
        normalized * static_cast<float>(parameters_.step_count));
    return static_cast<std::uint8_t>(std::min<unsigned int>(
        selected, static_cast<unsigned int>(parameters_.step_count - 1U)));
}

float StepTremolo::gain_for_step(std::uint8_t step) const noexcept {
    const float level = bounded(parameters_.levels[step], 1.0F, 0.0F, 1.0F);
    return 1.0F - parameters_.depth * (1.0F - level);
}

void StepTremolo::prepare_transition() noexcept {
    if (parameters_.transition_ms <= 0.0F || parameters_.rate_hz <= 0.0F) {
        transition_frames_ = 0;
        return;
    }

    const double requested = static_cast<double>(sample_rate_) *
                             static_cast<double>(parameters_.transition_ms) /
                             1000.0;
    const auto requested_frames = static_cast<std::size_t>(std::llround(requested));
    const double step_duration_frames = static_cast<double>(sample_rate_) /
                                       static_cast<double>(parameters_.rate_hz) /
                                       static_cast<double>(parameters_.step_count);
    const auto step_frames = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::floor(step_duration_frames)));
    transition_frames_ = std::min(requested_frames, step_frames);
}

void StepTremolo::begin_transition(std::uint8_t step) noexcept {
    current_step_ = step;
    target_gain_ = gain_for_step(step);
    if (transition_frames_ == 0) {
        current_gain_ = target_gain_;
        ramp_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }

    ramp_step_ = (target_gain_ - current_gain_) /
                 static_cast<float>(transition_frames_);
    ramp_remaining_ = transition_frames_;
}

} // namespace tabletfx
