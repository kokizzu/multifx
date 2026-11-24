#include "tabletfx/effects/optical_tremolo.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

float wrap_cycles(float cycles) noexcept {
    cycles -= std::floor(cycles);
    return cycles < 0.0F ? cycles + 1.0F : cycles;
}

} // namespace

OpticalTremolo::OpticalTremolo(
    float sample_rate, OpticalTremoloParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

float OpticalTremolo::bounded(float value, float fallback,
                              float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

OpticalTremoloVariant OpticalTremolo::sanitize_variant(
    OpticalTremoloVariant variant) noexcept {
    switch (variant) {
    case OpticalTremoloVariant::Optical1:
    case OpticalTremoloVariant::Optical2:
    case OpticalTremoloVariant::Neon:
        return variant;
    }
    return OpticalTremoloVariant::Optical1;
}

OpticalTremoloWaveform OpticalTremolo::sanitize_lfo(
    OpticalTremoloWaveform waveform) noexcept {
    switch (waveform) {
    case OpticalTremoloWaveform::Sine:
    case OpticalTremoloWaveform::Triangle:
    case OpticalTremoloWaveform::Square:
    case OpticalTremoloWaveform::SawUp:
    case OpticalTremoloWaveform::SawDown:
    case OpticalTremoloWaveform::Random:
    case OpticalTremoloWaveform::Logarithmic:
    case OpticalTremoloWaveform::Exponential:
    case OpticalTremoloWaveform::Trapezoid:
    case OpticalTremoloWaveform::Astable:
        return waveform;
    }
    return OpticalTremoloWaveform::Sine;
}

OpticalTremoloParameters OpticalTremolo::sanitize_parameters(
    OpticalTremoloParameters parameters) noexcept {
    parameters.variant = sanitize_variant(parameters.variant);
    parameters.rate_hz = bounded(parameters.rate_hz, 4.5F,
                                 kMinimumRateHz, kMaximumRateHz);
    parameters.depth = bounded(parameters.depth, 0.5F, 0.0F, 1.0F);
    parameters.lfo = sanitize_lfo(parameters.lfo);
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, 120.0F,
                                   kMinimumBpm, kMaximumBpm);
    parameters.beats_per_cycle = bounded(parameters.beats_per_cycle, 1.0F,
                                         kMinimumBeatsPerCycle,
                                         kMaximumBeatsPerCycle);
    parameters.duty_cycle_percent = bounded(parameters.duty_cycle_percent,
                                             50.0F, 5.0F, 95.0F);
    parameters.shape_percent = bounded(parameters.shape_percent, 50.0F,
                                         0.0F, 100.0F);
    parameters.phase_degrees = bounded(parameters.phase_degrees, 0.0F,
                                        0.0F, 360.0F);
    parameters.start_phase_degrees = bounded(
        parameters.start_phase_degrees, 0.0F, 0.0F, 360.0F);
    parameters.trigger_threshold_dbfs = bounded(
        parameters.trigger_threshold_dbfs, -36.0F,
        kMinimumTriggerDbfs, kMaximumTriggerDbfs);
    parameters.ducking_percent = bounded(parameters.ducking_percent, 0.0F,
                                         0.0F, 100.0F);
    parameters.mix = bounded(parameters.mix, 1.0F, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, 0.0F, -24.0F, 12.0F);
    parameters.balance_percent = bounded(parameters.balance_percent,
                                          0.0F, -100.0F, 100.0F);
    return parameters;
}

void OpticalTremolo::configure(
    float sample_rate, OpticalTremoloParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    parameters_ = sanitize_parameters(parameters);
    prepare_oscillator();

    const float attack_seconds = 0.005F;
    const float release_seconds = 0.080F;
    envelope_attack_coefficient_ = std::exp(
        -1.0F / (sample_rate_ * attack_seconds));
    envelope_release_coefficient_ = std::exp(
        -1.0F / (sample_rate_ * release_seconds));
    trigger_threshold_linear_ = parameters_.trigger_enabled
        ? std::pow(10.0F, parameters_.trigger_threshold_dbfs / 20.0F)
        : 0.0F;

    current_depth_ = target_depth_ = parameters_.depth;
    current_mix_ = target_mix_ = parameters_.mix;
    current_level_gain_ = target_level_gain_ = std::pow(
        10.0F, parameters_.level_db / 20.0F);
    current_balance_ = target_balance_ = parameters_.balance_percent;
    depth_step_ = mix_step_ = level_step_ = balance_step_ = 0.0F;
    smoothing_frames_remaining_ = 0U;
    reset();
}

void OpticalTremolo::set_parameters(
    OpticalTremoloParameters parameters) noexcept {
    const bool was_trigger_enabled = parameters_.trigger_enabled;
    parameters_ = sanitize_parameters(parameters);
    prepare_oscillator();
    trigger_threshold_linear_ = parameters_.trigger_enabled
        ? std::pow(10.0F, parameters_.trigger_threshold_dbfs / 20.0F)
        : 0.0F;
    envelope_attack_coefficient_ = std::exp(
        -1.0F / (sample_rate_ * 0.005F));
    envelope_release_coefficient_ = std::exp(
        -1.0F / (sample_rate_ * 0.080F));

    if (!parameters_.trigger_enabled) {
        trigger_active_ = true;
    } else if (!was_trigger_enabled || envelope_ < trigger_threshold_linear_) {
        trigger_active_ = false;
    }
    retarget_controls();
}

void OpticalTremolo::prepare_oscillator() noexcept {
    float requested_rate = parameters_.rate_hz;
    if (parameters_.tempo_sync) {
        requested_rate = (parameters_.tempo_bpm / 60.0F) /
                         parameters_.beats_per_cycle;
    }
    effective_rate_hz_ = std::clamp(requested_rate, kMinimumRateHz,
                                    kMaximumRateHz);
    phase_increment_ = effective_rate_hz_ / sample_rate_;
    const float increment_radians = kTwoPi * phase_increment_;
    sine_increment_ = std::sin(increment_radians);
    cosine_increment_ = std::cos(increment_radians);

    const float offset_radians = kTwoPi * parameters_.phase_degrees / 360.0F;
    phase_offset_sine_ = std::sin(offset_radians);
    phase_offset_cosine_ = std::cos(offset_radians);

    start_phase_cycles_ = wrap_cycles(
        parameters_.start_phase_degrees / 360.0F);
    const float start_radians = kTwoPi * start_phase_cycles_;
    start_phase_sine_ = std::sin(start_radians);
    start_phase_cosine_ = std::cos(start_radians);
    phase_ = wrap_cycles(phase_);
    const float phase_radians = kTwoPi * phase_;
    sine_phase_ = std::sin(phase_radians);
    cosine_phase_ = std::cos(phase_radians);
}

void OpticalTremolo::retarget_controls() noexcept {
    target_depth_ = parameters_.depth;
    target_mix_ = parameters_.mix;
    target_level_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    target_balance_ = parameters_.balance_percent;

    const std::size_t frames = static_cast<std::size_t>(std::llround(
        sample_rate_ * kControlSmoothingMs / 1000.0F));
    if (frames == 0U) {
        current_depth_ = target_depth_;
        current_mix_ = target_mix_;
        current_level_gain_ = target_level_gain_;
        current_balance_ = target_balance_;
        depth_step_ = mix_step_ = level_step_ = balance_step_ = 0.0F;
        smoothing_frames_remaining_ = 0U;
        return;
    }

    const float frame_count = static_cast<float>(frames);
    depth_step_ = (target_depth_ - current_depth_) / frame_count;
    mix_step_ = (target_mix_ - current_mix_) / frame_count;
    level_step_ = (target_level_gain_ - current_level_gain_) / frame_count;
    balance_step_ = (target_balance_ - current_balance_) / frame_count;
    smoothing_frames_remaining_ = frames;
}

float OpticalTremolo::phase_with_offset(float phase) const noexcept {
    float shifted = phase + parameters_.phase_degrees / 360.0F;
    if (shifted >= 1.0F) {
        shifted -= 1.0F;
    }
    return shifted;
}

float OpticalTremolo::lfo_value(float phase) const noexcept {
    float value = 0.0F;

    if (parameters_.variant == OpticalTremoloVariant::Neon) {
        // A fixed-width pulse is a project-authored interpretation of the
        // guide's fixed-LFO Neon description; the ordinary LFO controls do not
        // change this waveform.
        const float shifted_phase = phase_with_offset(phase);
        value = shifted_phase < 0.38F ? 1.0F : -1.0F;
    } else {
        const float shifted_phase = phase_with_offset(phase);
        const float shape = parameters_.shape_percent / 100.0F;
        const auto curve = [shape](float unipolar) noexcept {
            const float curvature = 2.0F * shape - 1.0F;
            unipolar += curvature * unipolar * (1.0F - unipolar) *
                        (2.0F * unipolar - 1.0F);
            return std::clamp(unipolar, 0.0F, 1.0F);
        };

        switch (parameters_.lfo) {
        case OpticalTremoloWaveform::Sine: {
            // Quadrature recurrence avoids a trigonometric call per sample.
            value = sine_phase_ * phase_offset_cosine_ +
                    cosine_phase_ * phase_offset_sine_;
            const float unipolar = curve(0.5F * (value + 1.0F));
            value = 2.0F * unipolar - 1.0F;
            break;
        }
        case OpticalTremoloWaveform::Triangle: {
            const float peak = 0.1F + 0.8F * shape;
            const float unipolar = shifted_phase < peak
                ? shifted_phase / peak
                : 1.0F - (shifted_phase - peak) / (1.0F - peak);
            value = 2.0F * curve(unipolar) - 1.0F;
            break;
        }
        case OpticalTremoloWaveform::Square:
            value = shifted_phase < parameters_.duty_cycle_percent / 100.0F
                        ? 1.0F : -1.0F;
            break;
        case OpticalTremoloWaveform::SawUp:
            value = 2.0F * curve(shifted_phase) - 1.0F;
            break;
        case OpticalTremoloWaveform::SawDown:
            value = 1.0F - 2.0F * curve(shifted_phase);
            break;
        case OpticalTremoloWaveform::Random:
            // Updated only at the cycle boundary; bounded held value is
            // deterministic and resettable.
            value = random_hold_;
            break;
        case OpticalTremoloWaveform::Logarithmic: {
            const float logarithmic = shifted_phase * (2.0F - shifted_phase);
            const float unipolar = shifted_phase +
                                   shape * (logarithmic - shifted_phase);
            value = 2.0F * unipolar - 1.0F;
            break;
        }
        case OpticalTremoloWaveform::Exponential: {
            const float exponential = shifted_phase * shifted_phase;
            const float unipolar = shifted_phase +
                                   shape * (exponential - shifted_phase);
            value = 2.0F * unipolar - 1.0F;
            break;
        }
        case OpticalTremoloWaveform::Trapezoid: {
            const float plateau = parameters_.duty_cycle_percent / 100.0F;
            const float ramp = 0.5F * (1.0F - plateau);
            float unipolar = 1.0F;
            if (shifted_phase < ramp) {
                unipolar = curve(shifted_phase / ramp);
            } else if (shifted_phase >= ramp + plateau) {
                unipolar = curve(1.0F -
                    (shifted_phase - ramp - plateau) / ramp);
            }
            value = 2.0F * unipolar - 1.0F;
            break;
        }
        case OpticalTremoloWaveform::Astable: {
            const float duty = parameters_.duty_cycle_percent / 100.0F;
            const float cycle_duty = (cycle_count_ & 1U) == 0U
                ? duty : 1.0F - duty;
            value = shifted_phase < cycle_duty ? 1.0F : -1.0F;
            break;
        }
        }
    }
    return std::clamp(value, -1.0F, 1.0F);
}

float OpticalTremolo::modulation_value(float phase) const noexcept {
    float normalized = 0.5F * (lfo_value(phase) + 1.0F);
    if (parameters_.variant == OpticalTremoloVariant::Optical2) {
        // More abrupt transition than Optical 1: project-authored smoothstep.
        normalized = normalized * normalized * (3.0F - 2.0F * normalized);
    }
    return std::clamp(normalized, 0.0F, 1.0F);
}

void OpticalTremolo::advance_oscillator() noexcept {
    phase_ += phase_increment_;
    if (phase_ >= 1.0F) {
        phase_ -= 1.0F;
        ++cycle_count_;
        if (parameters_.lfo == OpticalTremoloWaveform::Random &&
            parameters_.variant != OpticalTremoloVariant::Neon) {
            advance_random_sample();
        }
    }
    if (parameters_.lfo == OpticalTremoloWaveform::Sine &&
        parameters_.variant != OpticalTremoloVariant::Neon) {
        const float old_sine = sine_phase_;
        sine_phase_ = old_sine * cosine_increment_ +
                      cosine_phase_ * sine_increment_;
        cosine_phase_ = cosine_phase_ * cosine_increment_ -
                        old_sine * sine_increment_;
        if (++normalization_counter_ == 4096U) {
            const float magnitude_squared = sine_phase_ * sine_phase_ +
                                            cosine_phase_ * cosine_phase_;
            if (std::isfinite(magnitude_squared) && magnitude_squared > 0.0F) {
                const float normalization = 1.0F / std::sqrt(magnitude_squared);
                sine_phase_ *= normalization;
                cosine_phase_ *= normalization;
            } else {
                phase_ = start_phase_cycles_;
                sine_phase_ = start_phase_sine_;
                cosine_phase_ = start_phase_cosine_;
            }
            normalization_counter_ = 0U;
        }
    }
}

void OpticalTremolo::advance_random_sample() noexcept {
    std::uint32_t value = random_state_;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    random_state_ = value == 0U ? 0xA341316CU : value;
    const std::uint32_t mantissa = random_state_ >> 8U;
    const float unit = static_cast<float>(mantissa) / 16777215.0F;
    random_hold_ = 2.0F * unit - 1.0F;
}

void OpticalTremolo::advance_smoothing() noexcept {
    if (smoothing_frames_remaining_ == 0U) {
        return;
    }
    current_depth_ += depth_step_;
    current_mix_ += mix_step_;
    current_level_gain_ += level_step_;
    current_balance_ += balance_step_;
    --smoothing_frames_remaining_;
    if (smoothing_frames_remaining_ == 0U) {
        current_depth_ = target_depth_;
        current_mix_ = target_mix_;
        current_level_gain_ = target_level_gain_;
        current_balance_ = target_balance_;
        depth_step_ = mix_step_ = level_step_ = balance_step_ = 0.0F;
    }
}

void OpticalTremolo::process(std::span<float> interleaved,
                             std::uint16_t channels) noexcept {
    if (channels == 0U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        advance_smoothing();
        const std::size_t offset = frame * channels;
        float peak = 0.0F;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const float input = interleaved[offset + channel];
            if (std::isfinite(input)) {
                peak = std::max(peak, std::abs(std::clamp(
                    input, -kMaximumInternalSample, kMaximumInternalSample)));
            }
        }
        const float coefficient = peak > envelope_
            ? envelope_attack_coefficient_ : envelope_release_coefficient_;
        envelope_ = coefficient * envelope_ + (1.0F - coefficient) * peak;
        if (!std::isfinite(envelope_)) {
            envelope_ = 0.0F;
        }

        if (parameters_.trigger_enabled) {
            const bool was_active = trigger_active_;
            trigger_active_ = envelope_ >= trigger_threshold_linear_;
            if (!was_active && trigger_active_) {
                phase_ = start_phase_cycles_;
                sine_phase_ = start_phase_sine_;
                cosine_phase_ = start_phase_cosine_;
                normalization_counter_ = 0U;
                cycle_count_ = 0U;
                random_state_ = 0xA341316CU;
                advance_random_sample();
            }
        } else {
            trigger_active_ = true;
        }

        const bool modulation_running = !parameters_.trigger_enabled ||
                                        trigger_active_;
        const float lfo = modulation_running ? modulation_value(phase_) : 1.0F;
        const float duck = 1.0F -
            (parameters_.ducking_percent / 100.0F) *
            std::clamp(envelope_, 0.0F, 1.0F);
        const float depth = current_depth_ * duck;
        const float tremolo_gain = 1.0F - depth * (1.0F - lfo);
        const float active_mix = modulation_running ? current_mix_ : 0.0F;

        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const std::size_t index = offset + channel;
            float input = interleaved[index];
            if (!std::isfinite(input)) {
                input = 0.0F;
            }
            input = std::clamp(input, -kMaximumInternalSample,
                               kMaximumInternalSample);

            float wet = input * tremolo_gain * current_level_gain_;
            if (channels >= 2U && channel == 0U && current_balance_ > 0.0F) {
                wet *= 1.0F - current_balance_ / 100.0F;
            } else if (channels >= 2U && channel == 1U && current_balance_ < 0.0F) {
                wet *= 1.0F + current_balance_ / 100.0F;
            }
            interleaved[index] = input * (1.0F - active_mix) +
                                 wet * active_mix;
        }

        if (modulation_running) {
            advance_oscillator();
        }
    }
}

void OpticalTremolo::reset() noexcept {
    phase_ = start_phase_cycles_;
    sine_phase_ = start_phase_sine_;
    cosine_phase_ = start_phase_cosine_;
    normalization_counter_ = 0U;
    cycle_count_ = 0U;
    random_state_ = 0xA341316CU;
    advance_random_sample();
    envelope_ = 0.0F;
    trigger_active_ = !parameters_.trigger_enabled;
    current_depth_ = target_depth_;
    current_mix_ = target_mix_;
    current_level_gain_ = target_level_gain_;
    current_balance_ = target_balance_;
    depth_step_ = mix_step_ = level_step_ = balance_step_ = 0.0F;
    smoothing_frames_remaining_ = 0U;
}

} // namespace tabletfx
