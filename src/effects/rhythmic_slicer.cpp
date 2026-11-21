#include "tabletfx/effects/rhythmic_slicer.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kDefaultTempoBpm = 120.0F;
constexpr float kDefaultRateHz = 8.0F;
constexpr float kDefaultAttackMs = 1.0F;
constexpr float kDefaultReleaseMs = 1.0F;

} // namespace

RhythmicSlicer::RhythmicSlicer(float sample_rate,
                               RhythmicSlicerParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void RhythmicSlicer::configure(float sample_rate,
                               RhythmicSlicerParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : kDefaultSampleRate;

    parameters.tempo_bpm = bounded(parameters.tempo_bpm, kDefaultTempoBpm,
                                   kMinimumBpm, kMaximumBpm);
    parameters.division = sanitize_division(parameters.division);
    parameters.rate_hz = bounded(parameters.rate_hz, kDefaultRateHz,
                                 kMinimumRateHz, kMaximumRateHz);
    parameters.depth = bounded(parameters.depth, 1.0F, 0.0F, 1.0F);
    parameters.shape = bounded(parameters.shape, 0.15F, 0.0F, 1.0F);
    parameters.attack_ms = bounded(parameters.attack_ms, kDefaultAttackMs,
                                   kMinimumEdgeTimeMs, kMaximumEdgeTimeMs);
    parameters.release_ms = bounded(parameters.release_ms, kDefaultReleaseMs,
                                    kMinimumEdgeTimeMs, kMaximumEdgeTimeMs);
    parameters.phase = bounded(parameters.phase, 0.0F, 0.0F,
                               0.99999994F);
    for (float& step : parameters.steps) {
        step = bounded(step, 1.0F, 0.0F, 1.0F);
    }

    parameters_ = parameters;
    prepare_timing();
    prepare_edge_coefficients();
    reset();
}

void RhythmicSlicer::set_tempo_sync(bool enabled, float bpm,
                                    RhythmicDivision division) noexcept {
    parameters_.tempo_sync = enabled;
    parameters_.tempo_bpm = bounded(bpm, parameters_.tempo_bpm,
                                    kMinimumBpm, kMaximumBpm);
    parameters_.division = sanitize_division(division);
    prepare_timing();
}

void RhythmicSlicer::set_rate_hz(float rate_hz) noexcept {
    parameters_.rate_hz = bounded(rate_hz, parameters_.rate_hz,
                                  kMinimumRateHz, kMaximumRateHz);
    if (!parameters_.tempo_sync) {
        prepare_timing();
    }
}

void RhythmicSlicer::set_depth(float depth) noexcept {
    parameters_.depth = bounded(depth, parameters_.depth, 0.0F, 1.0F);
}

void RhythmicSlicer::set_shape(float shape) noexcept {
    parameters_.shape = bounded(shape, parameters_.shape, 0.0F, 1.0F);
}

void RhythmicSlicer::set_edge_times_ms(float attack_ms,
                                       float release_ms) noexcept {
    parameters_.attack_ms = bounded(attack_ms, parameters_.attack_ms,
                                    kMinimumEdgeTimeMs, kMaximumEdgeTimeMs);
    parameters_.release_ms = bounded(release_ms, parameters_.release_ms,
                                     kMinimumEdgeTimeMs, kMaximumEdgeTimeMs);
    prepare_edge_coefficients();
}

void RhythmicSlicer::set_phase(float cycle_fraction) noexcept {
    parameters_.phase = bounded(cycle_fraction, parameters_.phase, 0.0F,
                                0.99999994F);
    phase_ = parameters_.phase;
    current_gain_ = target_gain(phase_);
}

void RhythmicSlicer::set_steps(const std::array<float, 16>& steps) noexcept {
    for (std::size_t index = 0; index < steps.size(); ++index) {
        parameters_.steps[index] = bounded(steps[index],
                                           parameters_.steps[index],
                                           0.0F, 1.0F);
    }
}

void RhythmicSlicer::process(std::span<float> interleaved,
                             std::uint16_t channels) noexcept {
    if (channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float target = target_gain(phase_);
        if (target > current_gain_) {
            current_gain_ = target + (current_gain_ - target) * attack_coefficient_;
        } else if (target < current_gain_) {
            current_gain_ = target + (current_gain_ - target) * release_coefficient_;
        }
        current_gain_ = std::clamp(current_gain_, 0.0F, 1.0F);

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

void RhythmicSlicer::reset() noexcept {
    phase_ = parameters_.phase;
    current_gain_ = target_gain(phase_);
}

float RhythmicSlicer::bounded(float value, float fallback,
                              float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float RhythmicSlicer::beats_per_step(RhythmicDivision division) noexcept {
    switch (division) {
    case RhythmicDivision::Whole: return 4.0F;
    case RhythmicDivision::Half: return 2.0F;
    case RhythmicDivision::Quarter: return 1.0F;
    case RhythmicDivision::DottedQuarter: return 1.5F;
    case RhythmicDivision::QuarterTriplet: return 2.0F / 3.0F;
    case RhythmicDivision::Eighth: return 0.5F;
    case RhythmicDivision::DottedEighth: return 0.75F;
    case RhythmicDivision::EighthTriplet: return 1.0F / 3.0F;
    case RhythmicDivision::Sixteenth: return 0.25F;
    case RhythmicDivision::DottedSixteenth: return 0.375F;
    case RhythmicDivision::SixteenthTriplet: return 1.0F / 6.0F;
    case RhythmicDivision::ThirtySecond: return 0.125F;
    case RhythmicDivision::ThirtySecondTriplet: return 1.0F / 12.0F;
    }
    return 0.25F;
}

RhythmicDivision RhythmicSlicer::sanitize_division(
    RhythmicDivision division) noexcept {
    switch (division) {
    case RhythmicDivision::Whole:
    case RhythmicDivision::Half:
    case RhythmicDivision::Quarter:
    case RhythmicDivision::DottedQuarter:
    case RhythmicDivision::QuarterTriplet:
    case RhythmicDivision::Eighth:
    case RhythmicDivision::DottedEighth:
    case RhythmicDivision::EighthTriplet:
    case RhythmicDivision::Sixteenth:
    case RhythmicDivision::DottedSixteenth:
    case RhythmicDivision::SixteenthTriplet:
    case RhythmicDivision::ThirtySecond:
    case RhythmicDivision::ThirtySecondTriplet:
        return division;
    }
    return RhythmicDivision::Sixteenth;
}

float RhythmicSlicer::pattern_value(float cycle_phase) const noexcept {
    float position = cycle_phase * 16.0F;
    const float nearby_step = std::round(position);
    if (std::abs(position - nearby_step) < 1.0e-5F) {
        position = nearby_step;
    }
    const auto index = static_cast<std::size_t>(position);
    const std::size_t current_index = std::min(index, parameters_.steps.size() - 1U);
    const float fraction = position - static_cast<float>(current_index);
    const float current = parameters_.steps[current_index];
    const float shape = parameters_.shape;
    if (shape <= 0.0F || fraction <= 1.0F - shape) {
        return current;
    }

    const std::size_t next_index = (current_index + 1U) % parameters_.steps.size();
    const float transition_position =
        std::clamp((fraction - (1.0F - shape)) / shape, 0.0F, 1.0F);
    const float eased = transition_position * transition_position *
                        (3.0F - 2.0F * transition_position);
    return current + (parameters_.steps[next_index] - current) * eased;
}

float RhythmicSlicer::target_gain(float cycle_phase) const noexcept {
    const float value = pattern_value(cycle_phase);
    return 1.0F - parameters_.depth * (1.0F - value);
}

void RhythmicSlicer::prepare_timing() noexcept {
    const float rate = parameters_.tempo_sync
                           ? parameters_.tempo_bpm /
                                 (60.0F * beats_per_step(parameters_.division))
                           : parameters_.rate_hz;
    effective_step_rate_hz_ =
        std::clamp(rate, kMinimumRateHz, kMaximumRateHz);
    phase_increment_ = effective_step_rate_hz_ / (sample_rate_ * 16.0F);
}

void RhythmicSlicer::prepare_edge_coefficients() noexcept {
    const float attack_seconds = parameters_.attack_ms * 0.001F;
    const float release_seconds = parameters_.release_ms * 0.001F;
    attack_coefficient_ = attack_seconds <= 0.0F
                              ? 0.0F
                              : std::exp(-1.0F / (attack_seconds * sample_rate_));
    release_coefficient_ = release_seconds <= 0.0F
                               ? 0.0F
                               : std::exp(-1.0F / (release_seconds * sample_rate_));
}

} // namespace tabletfx
