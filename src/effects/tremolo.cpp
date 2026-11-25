#include "tabletfx/effects/tremolo.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTwoPi = 2.0F * kPi;

TremoloWaveform sanitize_waveform(TremoloWaveform waveform) noexcept {
    switch (waveform) {
    case TremoloWaveform::Sine:
    case TremoloWaveform::Triangle:
    case TremoloWaveform::Square:
        return waveform;
    }
    return TremoloWaveform::Sine;
}

} // namespace

Tremolo::Tremolo(float sample_rate, TremoloParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void Tremolo::configure(float sample_rate,
                        TremoloParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    parameters.rate_hz = bounded(parameters.rate_hz, 5.0F,
                                 kMinimumRateHz, kMaximumRateHz);
    parameters.depth = bounded(parameters.depth, 0.5F, 0.0F, 1.0F);
    parameters.lfo_clip_percent = bounded(parameters.lfo_clip_percent, 0.0F,
                                          0.0F, 100.0F);
    parameters.duty_cycle_percent = bounded(
        parameters.duty_cycle_percent, 50.0F,
        kMinimumDutyCyclePercent, kMaximumDutyCyclePercent);
    parameters.waveform = sanitize_waveform(parameters.waveform);
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, 120.0F,
                                   kMinimumBpm, kMaximumBpm);
    parameters.beats_per_cycle = bounded(parameters.beats_per_cycle, 1.0F,
                                         kMinimumBeatsPerCycle,
                                         kMaximumBeatsPerCycle);
    parameters.depth_smoothing_ms = bounded(parameters.depth_smoothing_ms, 10.0F,
                                             0.0F, 1000.0F);
    parameters_ = parameters;
    current_depth_ = parameters_.depth;
    target_depth_ = parameters_.depth;
    depth_ramp_step_ = 0.0F;
    depth_ramp_remaining_ = 0;
    current_duty_cycle_percent_ = parameters_.duty_cycle_percent;
    target_duty_cycle_percent_ = parameters_.duty_cycle_percent;
    duty_cycle_ramp_step_ = 0.0F;
    duty_cycle_ramp_remaining_ = 0;
    prepare_oscillator();
    reset();
}

void Tremolo::set_rate_hz(float rate_hz) noexcept {
    parameters_.rate_hz = bounded(rate_hz, parameters_.rate_hz,
                                  kMinimumRateHz, kMaximumRateHz);
    if (!parameters_.tempo_sync) {
        prepare_oscillator();
    }
}

void Tremolo::set_depth(float depth) noexcept {
    parameters_.depth = bounded(depth, parameters_.depth, 0.0F, 1.0F);
    target_depth_ = parameters_.depth;
    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(parameters_.depth_smoothing_ms) /
                                    1000.0;
    const std::size_t ramp_frames = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_frames == 0) {
        current_depth_ = target_depth_;
        depth_ramp_step_ = 0.0F;
        depth_ramp_remaining_ = 0;
        return;
    }
    depth_ramp_step_ = (target_depth_ - current_depth_) /
                       static_cast<float>(ramp_frames);
    depth_ramp_remaining_ = ramp_frames;
}

void Tremolo::set_lfo_clip_percent(float clip_percent) noexcept {
    parameters_.lfo_clip_percent = bounded(clip_percent,
                                          parameters_.lfo_clip_percent,
                                          0.0F, 100.0F);
}

void Tremolo::set_duty_cycle_percent(float duty_cycle_percent) noexcept {
    parameters_.duty_cycle_percent = bounded(
        duty_cycle_percent, parameters_.duty_cycle_percent,
        kMinimumDutyCyclePercent, kMaximumDutyCyclePercent);
    target_duty_cycle_percent_ = parameters_.duty_cycle_percent;
    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(parameters_.depth_smoothing_ms) /
                                    1000.0;
    const std::size_t ramp_frames = static_cast<std::size_t>(
        std::llround(requested_frames));
    if (ramp_frames == 0) {
        current_duty_cycle_percent_ = target_duty_cycle_percent_;
        duty_cycle_ramp_step_ = 0.0F;
        duty_cycle_ramp_remaining_ = 0;
        return;
    }
    duty_cycle_ramp_step_ =
        (target_duty_cycle_percent_ - current_duty_cycle_percent_) /
        static_cast<float>(ramp_frames);
    duty_cycle_ramp_remaining_ = ramp_frames;
}

void Tremolo::set_depth_smoothing_ms(float smoothing_ms) noexcept {
    parameters_.depth_smoothing_ms = bounded(smoothing_ms, 10.0F,
                                             0.0F, 1000.0F);
}

void Tremolo::set_waveform(TremoloWaveform waveform) noexcept {
    const TremoloWaveform selected = sanitize_waveform(waveform);
    if (selected == TremoloWaveform::Sine &&
        parameters_.waveform != TremoloWaveform::Sine) {
        const float phase_radians = kTwoPi * phase_;
        sine_phase_ = std::sin(phase_radians);
        cosine_phase_ = std::cos(phase_radians);
    }
    parameters_.waveform = selected;
}

void Tremolo::set_tempo_sync(bool enabled, float bpm,
                             float beats_per_cycle) noexcept {
    parameters_.tempo_sync = enabled;
    parameters_.tempo_bpm = bounded(bpm, parameters_.tempo_bpm,
                                    kMinimumBpm, kMaximumBpm);
    parameters_.beats_per_cycle = bounded(beats_per_cycle,
                                           parameters_.beats_per_cycle,
                                           kMinimumBeatsPerCycle,
                                           kMaximumBeatsPerCycle);
    prepare_oscillator();
}

void Tremolo::process(std::span<float> interleaved,
                      std::uint16_t channels) noexcept {
    if (channels == 0) {
        return;
    }

    const bool lfo_clip_enabled = parameters_.lfo_clip_percent > 0.0F;
    const float lfo_clip_threshold =
        1.0F - 0.005F * parameters_.lfo_clip_percent;
    const float lfo_clip_normalization =
        lfo_clip_enabled ? 1.0F / lfo_clip_threshold : 1.0F;
    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (depth_ramp_remaining_ > 0) {
            current_depth_ += depth_ramp_step_;
            --depth_ramp_remaining_;
            if (depth_ramp_remaining_ == 0) {
                current_depth_ = target_depth_;
                depth_ramp_step_ = 0.0F;
            }
        }
        if (duty_cycle_ramp_remaining_ > 0) {
            current_duty_cycle_percent_ += duty_cycle_ramp_step_;
            --duty_cycle_ramp_remaining_;
            if (duty_cycle_ramp_remaining_ == 0) {
                current_duty_cycle_percent_ = target_duty_cycle_percent_;
                duty_cycle_ramp_step_ = 0.0F;
            }
        }
        float waveform_value = sine_phase_;
        if (parameters_.waveform == TremoloWaveform::Triangle) {
            waveform_value = phase_ < 0.5F
                                 ? -1.0F + 4.0F * phase_
                                 : 3.0F - 4.0F * phase_;
        } else if (parameters_.waveform == TremoloWaveform::Square) {
            waveform_value = phase_ < current_duty_cycle_percent_ / 100.0F
                                 ? 1.0F
                                 : -1.0F;
        }
        if (lfo_clip_enabled) {
            waveform_value = std::clamp(waveform_value, -lfo_clip_threshold,
                                        lfo_clip_threshold) *
                             lfo_clip_normalization;
        }

        const float gain = 1.0F - current_depth_ * (1.0F - waveform_value) * 0.5F;
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            sample = std::isfinite(sample) ? sample * gain : 0.0F;
        }

        phase_ += phase_increment_;
        if (phase_ >= 1.0F) {
            phase_ -= 1.0F;
        }

        if (parameters_.waveform == TremoloWaveform::Sine) {
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
        }
    }
}

void Tremolo::reset() noexcept {
    phase_ = 0.0F;
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    normalization_counter_ = 0;
    current_depth_ = target_depth_;
    depth_ramp_step_ = 0.0F;
    depth_ramp_remaining_ = 0;
    current_duty_cycle_percent_ = target_duty_cycle_percent_;
    duty_cycle_ramp_step_ = 0.0F;
    duty_cycle_ramp_remaining_ = 0;
}

float Tremolo::bounded(float value, float fallback,
                       float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

void Tremolo::prepare_oscillator() noexcept {
    const float rate = parameters_.tempo_sync
                           ? parameters_.tempo_bpm /
                                 (60.0F * parameters_.beats_per_cycle)
                           : parameters_.rate_hz;
    effective_rate_hz_ = std::clamp(rate, kMinimumRateHz, kMaximumRateHz);
    phase_increment_ = effective_rate_hz_ / sample_rate_;
    const float step_radians = kTwoPi * phase_increment_;
    sine_increment_ = std::sin(step_radians);
    cosine_increment_ = std::cos(step_radians);
    const float phase_radians = kTwoPi * phase_;
    sine_phase_ = std::sin(phase_radians);
    cosine_phase_ = std::cos(phase_radians);
    normalization_counter_ = 0;
}

} // namespace tabletfx
