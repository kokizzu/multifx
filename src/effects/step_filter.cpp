#include "tabletfx/effects/step_filter.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kDefaultTempoBpm = 120.0F;
constexpr float kDefaultRateHz = 8.0F;
constexpr float kPi = 3.14159265358979323846F;
constexpr float kMaximumStateMagnitude = 1000000.0F;

} // namespace

StepFilter::StepFilter(float sample_rate,
                       StepFilterParameters parameters) noexcept {
    const bool valid_rate = std::isfinite(sample_rate) &&
                            sample_rate >= kMinimumSampleRate &&
                            sample_rate <= kMaximumSampleRate;
    sample_rate_ = valid_rate ? sample_rate : kDefaultSampleRate;
    set_parameters(parameters);
    reset();
}

void StepFilter::set_parameters(StepFilterParameters parameters) noexcept {
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, kDefaultTempoBpm,
                                   kMinimumBpm, kMaximumBpm);
    parameters.division = sanitize_division(parameters.division);
    parameters.rate_hz = bounded(parameters.rate_hz, kDefaultRateHz,
                                 kMinimumRateHz, kMaximumRateHz);
    parameters.mode = sanitize_mode(parameters.mode);
    const float cutoff_limit = std::min(kMaximumCutoffHz, sample_rate_ * 0.40F);
    parameters.minimum_cutoff_hz = bounded(parameters.minimum_cutoff_hz,
                                            120.0F, kMinimumCutoffHz,
                                            cutoff_limit);
    parameters.maximum_cutoff_hz = bounded(parameters.maximum_cutoff_hz,
                                            6000.0F,
                                            parameters.minimum_cutoff_hz,
                                            cutoff_limit);
    if (parameters.maximum_cutoff_hz < parameters.minimum_cutoff_hz) {
        parameters.maximum_cutoff_hz = parameters.minimum_cutoff_hz;
    }
    parameters.resonance_q = bounded(parameters.resonance_q, 0.707F,
                                     kMinimumQ, kMaximumQ);
    parameters.depth = bounded(parameters.depth, 1.0F, 0.0F, 1.0F);
    parameters.shape = bounded(parameters.shape, 0.15F, 0.0F, 1.0F);
    parameters.smoothing_ms = bounded(parameters.smoothing_ms, 4.0F,
                                      0.0F, kMaximumSmoothingMs);
    parameters.mix = bounded(parameters.mix, 1.0F, 0.0F, 1.0F);
    parameters.phase = bounded(parameters.phase, 0.0F, 0.0F,
                               0.99999994F);
    for (float& step : parameters.steps) {
        step = bounded(step, 0.5F, 0.0F, 1.0F);
    }

    parameters_ = parameters;
    prepare_parameters();
}

void StepFilter::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    if (channels == 0 || channels > 2) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float pattern = pattern_value(phase_);
        const float target = 0.5F + parameters_.depth * (pattern - 0.5F);
        smoothed_position_ +=
            (target - smoothed_position_) * smoothing_alpha_;
        smoothed_position_ = std::clamp(smoothed_position_, 0.0F, 1.0F);
        const float g = g_from_position(smoothed_position_);

        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            if (!std::isfinite(sample)) {
                integrator_1_[channel] = 0.0F;
                integrator_2_[channel] = 0.0F;
                sample = 0.0F;
                continue;
            }

            const float original = sample;
            const float dry = std::clamp(original, -kMaximumInputMagnitude,
                                         kMaximumInputMagnitude);
            const float wet = filter_sample(dry, channel, g);
            if (!std::isfinite(wet)) {
                integrator_1_[channel] = 0.0F;
                integrator_2_[channel] = 0.0F;
                sample = 0.0F;
            } else if (parameters_.mix <= 0.0F) {
                sample = original;
            } else {
                sample = dry + (wet - dry) * parameters_.mix;
            }
        }

        phase_ += phase_increment_;
        if (phase_ >= 1.0F) {
            phase_ -= 1.0F;
        }
    }
}

void StepFilter::reset() noexcept {
    integrator_1_.fill(0.0F);
    integrator_2_.fill(0.0F);
    phase_ = parameters_.phase;
    smoothed_position_ = 0.5F + parameters_.depth *
                                      (pattern_value(phase_) - 0.5F);
}

float StepFilter::bounded(float value, float fallback,
                          float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

StepFilterMode StepFilter::sanitize_mode(StepFilterMode mode) noexcept {
    switch (mode) {
    case StepFilterMode::LowPass:
    case StepFilterMode::BandPass:
    case StepFilterMode::HighPass:
    case StepFilterMode::Notch:
        return mode;
    }
    return StepFilterMode::LowPass;
}

RhythmicDivision StepFilter::sanitize_division(
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

float StepFilter::beats_per_step(RhythmicDivision division) noexcept {
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

float StepFilter::pattern_value(float cycle_phase) const noexcept {
    float position = std::clamp(cycle_phase, 0.0F, 0.99999994F) * 16.0F;
    const float nearby_step = std::round(position);
    if (std::abs(position - nearby_step) < 1.0e-5F) {
        position = nearby_step;
    }
    const auto index = static_cast<std::size_t>(position);
    const std::size_t current_index =
        std::min(index, parameters_.steps.size() - 1U);
    const float fraction = position - static_cast<float>(current_index);
    const float current = parameters_.steps[current_index];
    const float shape = parameters_.shape;
    if (shape <= 0.0F || fraction <= 1.0F - shape) {
        return current;
    }

    const std::size_t next_index = (current_index + 1U) % parameters_.steps.size();
    const float transition =
        std::clamp((fraction - (1.0F - shape)) / shape, 0.0F, 1.0F);
    const float eased = transition * transition * (3.0F - 2.0F * transition);
    return current + (parameters_.steps[next_index] - current) * eased;
}

float StepFilter::cutoff_from_position(float position) const noexcept {
    const float log_min = std::log(parameters_.minimum_cutoff_hz);
    const float log_max = std::log(parameters_.maximum_cutoff_hz);
    return std::exp(log_min + std::clamp(position, 0.0F, 1.0F) *
                                 (log_max - log_min));
}

float StepFilter::g_from_position(float position) const noexcept {
    const float table_position = std::clamp(position, 0.0F, 1.0F) *
                                 static_cast<float>(kCoefficientSteps);
    const auto index = static_cast<std::size_t>(table_position);
    if (index >= kCoefficientSteps) {
        return g_table_[kCoefficientSteps];
    }
    const float fraction = table_position - static_cast<float>(index);
    const float lower = g_table_[index];
    const float upper = g_table_[index + 1U];
    return lower + (upper - lower) * fraction;
}

void StepFilter::prepare_parameters() noexcept {
    prepare_rate();
    resonance_k_ = 1.0F / parameters_.resonance_q;
    const float smoothing_seconds = parameters_.smoothing_ms * 0.001F;
    smoothing_alpha_ = smoothing_seconds <= 0.0F
                           ? 1.0F
                           : static_cast<float>(1.0 - std::exp(
                                 -1.0 / (static_cast<double>(smoothing_seconds) *
                                         sample_rate_)));
    prepare_coefficient_table();
}

void StepFilter::prepare_rate() noexcept {
    const float rate = parameters_.tempo_sync
                           ? parameters_.tempo_bpm /
                                 (60.0F * beats_per_step(parameters_.division))
                           : parameters_.rate_hz;
    effective_step_rate_hz_ = std::clamp(rate, kMinimumRateHz, kMaximumRateHz);
    phase_increment_ = effective_step_rate_hz_ / (sample_rate_ * 16.0F);
}

void StepFilter::prepare_coefficient_table() noexcept {
    const float log_min = std::log(parameters_.minimum_cutoff_hz);
    const float log_max = std::log(parameters_.maximum_cutoff_hz);
    for (std::size_t index = 0; index <= kCoefficientSteps; ++index) {
        const float position = static_cast<float>(index) /
                               static_cast<float>(kCoefficientSteps);
        const float cutoff = std::exp(log_min + position * (log_max - log_min));
        const float normalized = std::min(cutoff / sample_rate_, 0.40F);
        g_table_[index] = std::tan(kPi * normalized);
    }
}

float StepFilter::filter_sample(float input, std::uint16_t channel,
                                float g) noexcept {
    const float denominator = 1.0F + g * (g + resonance_k_);
    const float a1 = 1.0F / denominator;
    const float v3 = input - integrator_2_[channel];
    const float band = (integrator_1_[channel] + g * v3) * a1;
    const float low = integrator_2_[channel] + g * band;
    integrator_1_[channel] = 2.0F * band - integrator_1_[channel];
    integrator_2_[channel] = 2.0F * low - integrator_2_[channel];

    if (!std::isfinite(integrator_1_[channel]) ||
        !std::isfinite(integrator_2_[channel]) ||
        std::abs(integrator_1_[channel]) > kMaximumStateMagnitude ||
        std::abs(integrator_2_[channel]) > kMaximumStateMagnitude) {
        integrator_1_[channel] = 0.0F;
        integrator_2_[channel] = 0.0F;
        return 0.0F;
    }

    const float high = input - resonance_k_ * band - low;
    switch (parameters_.mode) {
    case StepFilterMode::LowPass: return low;
    case StepFilterMode::BandPass: return band;
    case StepFilterMode::HighPass: return high;
    case StepFilterMode::Notch: return low + high;
    }
    return low;
}

} // namespace tabletfx
