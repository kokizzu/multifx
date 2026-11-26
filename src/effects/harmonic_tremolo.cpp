#include "tabletfx/effects/harmonic_tremolo.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTwoPi = 2.0F * kPi;

} // namespace

HarmonicTremolo::HarmonicTremolo(
    float sample_rate, HarmonicTremoloParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void HarmonicTremolo::configure(
    float sample_rate, HarmonicTremoloParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;

    parameters.rate_hz = bounded(parameters.rate_hz, 3.0F,
                                  kMinimumRateHz, kMaximumRateHz);
    parameters.depth = bounded(parameters.depth, 0.65F, 0.0F, 1.0F);
    parameters.crossover_hz = bounded(parameters.crossover_hz, 700.0F,
                                      kMinimumCrossoverHz,
                                      maximum_crossover_hz());
    parameters.mode = sanitize_mode(parameters.mode);
    parameters.waveform = sanitize_waveform(parameters.waveform);
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, 120.0F,
                                   kMinimumBpm, kMaximumBpm);
    parameters.beats_per_cycle = bounded(parameters.beats_per_cycle, 1.0F,
                                         kMinimumBeatsPerCycle,
                                         kMaximumBeatsPerCycle);
    parameters.smoothing_ms = bounded(parameters.smoothing_ms, 10.0F,
                                      0.0F, kMaximumSmoothingMs);
    parameters_ = parameters;

    current_depth_ = parameters_.depth;
    target_depth_ = parameters_.depth;
    depth_step_ = 0.0F;
    depth_frames_remaining_ = 0;

    current_alpha_ = alpha_for_crossover(effective_crossover_hz());
    target_alpha_ = current_alpha_;
    alpha_step_ = 0.0F;
    alpha_frames_remaining_ = 0;

    prepare_oscillator();
    reset();
}

void HarmonicTremolo::set_rate_hz(float rate_hz) noexcept {
    parameters_.rate_hz = bounded(rate_hz, parameters_.rate_hz,
                                  kMinimumRateHz, kMaximumRateHz);
    if (!parameters_.tempo_sync) {
        prepare_oscillator();
    }
}

void HarmonicTremolo::set_depth(float depth) noexcept {
    retarget_depth(bounded(depth, parameters_.depth, 0.0F, 1.0F));
}

void HarmonicTremolo::set_crossover_hz(float frequency_hz) noexcept {
    parameters_.crossover_hz = bounded(
        frequency_hz, parameters_.crossover_hz, kMinimumCrossoverHz,
        maximum_crossover_hz());
    retarget_crossover(effective_crossover_hz());
}

void HarmonicTremolo::set_mode(HarmonicTremoloMode mode) noexcept {
    const HarmonicTremoloMode selected = sanitize_mode(mode);
    if (selected == parameters_.mode) {
        return;
    }
    parameters_.mode = selected;
    retarget_crossover(effective_crossover_hz());
}

void HarmonicTremolo::set_waveform(
    HarmonicTremoloWaveform waveform) noexcept {
    const HarmonicTremoloWaveform selected = sanitize_waveform(waveform);
    if (selected == HarmonicTremoloWaveform::Sine &&
        parameters_.waveform != HarmonicTremoloWaveform::Sine) {
        synchronize_sine_phase();
    }
    parameters_.waveform = selected;
}

void HarmonicTremolo::set_tempo_sync(bool enabled, float bpm,
                                    float beats_per_cycle) noexcept {
    parameters_.tempo_sync = enabled;
    parameters_.tempo_bpm = bounded(bpm, parameters_.tempo_bpm,
                                    kMinimumBpm, kMaximumBpm);
    parameters_.beats_per_cycle = bounded(
        beats_per_cycle, parameters_.beats_per_cycle,
        kMinimumBeatsPerCycle, kMaximumBeatsPerCycle);
    prepare_oscillator();
}

void HarmonicTremolo::set_stereo_alternation(bool enabled) noexcept {
    parameters_.stereo_alternation = enabled;
}

void HarmonicTremolo::set_smoothing_ms(float smoothing_ms) noexcept {
    parameters_.smoothing_ms = bounded(smoothing_ms, 10.0F,
                                       0.0F, kMaximumSmoothingMs);
}

float HarmonicTremolo::current_crossover_hz() const noexcept {
    if (current_alpha_ <= 0.0F) {
        return 0.0F;
    }
    if (current_alpha_ >= 0.82842714F) {
        return sample_rate_ * 0.5F;
    }
    const float d = (current_alpha_ * current_alpha_) /
                    (2.0F * (1.0F - current_alpha_));
    return sample_rate_ *
           std::acos(std::clamp(1.0F - d, -1.0F, 1.0F)) / kTwoPi;
}

void HarmonicTremolo::process(std::span<float> interleaved,
                              std::uint16_t channels) noexcept {
    if (channels == 0 || channels > 2) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (depth_frames_remaining_ > 0) {
            current_depth_ += depth_step_;
            --depth_frames_remaining_;
            if (depth_frames_remaining_ == 0) {
                current_depth_ = target_depth_;
                depth_step_ = 0.0F;
            }
        }
        if (alpha_frames_remaining_ > 0) {
            current_alpha_ += alpha_step_;
            --alpha_frames_remaining_;
            if (alpha_frames_remaining_ == 0) {
                current_alpha_ = target_alpha_;
                alpha_step_ = 0.0F;
            }
        }

        const float lfo = modulation_value();
        const float low_gain =
            1.0F - current_depth_ * (1.0F - lfo) * 0.5F;
        const float high_gain =
            1.0F - current_depth_ * (1.0F + lfo) * 0.5F;
        const std::size_t offset = frame * channels;

        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const std::size_t index = offset + channel;
            const float input = interleaved[index];
            if (!std::isfinite(input)) {
                low_state_[channel] = 0.0F;
                interleaved[index] = 0.0F;
                continue;
            }

            const float bounded_input = std::clamp(
                input, -kMaximumInternalSample, kMaximumInternalSample);
            float low = low_state_[channel] +
                        current_alpha_ * (bounded_input - low_state_[channel]);
            if (!std::isfinite(low)) {
                low = 0.0F;
            }
            low = std::clamp(low, -kMaximumInternalSample,
                             kMaximumInternalSample);
            low_state_[channel] = low;
            const float high = bounded_input - low;

            if (current_depth_ == 0.0F) {
                interleaved[index] = bounded_input;
                continue;
            }

            const bool swap_bands = channels == 2 && channel == 1 &&
                                    parameters_.stereo_alternation;
            const float low_channel_gain = swap_bands ? high_gain : low_gain;
            const float high_channel_gain = swap_bands ? low_gain : high_gain;
            const float output = low * low_channel_gain +
                                 high * high_channel_gain;
            interleaved[index] = std::isfinite(output)
                                     ? std::clamp(output,
                                                  -kMaximumInternalSample,
                                                  kMaximumInternalSample)
                                     : 0.0F;
        }

        advance_state();
    }
}

void HarmonicTremolo::reset() noexcept {
    low_state_.fill(0.0F);
    phase_ = 0.0F;
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    normalization_counter_ = 0;
    current_depth_ = target_depth_;
    depth_step_ = 0.0F;
    depth_frames_remaining_ = 0;
    current_alpha_ = target_alpha_;
    alpha_step_ = 0.0F;
    alpha_frames_remaining_ = 0;
}

float HarmonicTremolo::bounded(float value, float fallback,
                              float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum)
                                : fallback;
}

HarmonicTremoloWaveform HarmonicTremolo::sanitize_waveform(
    HarmonicTremoloWaveform waveform) noexcept {
    switch (waveform) {
    case HarmonicTremoloWaveform::Sine:
    case HarmonicTremoloWaveform::Triangle:
    case HarmonicTremoloWaveform::Square:
        return waveform;
    }
    return HarmonicTremoloWaveform::Sine;
}

HarmonicTremoloMode HarmonicTremolo::sanitize_mode(
    HarmonicTremoloMode mode) noexcept {
    switch (mode) {
    case HarmonicTremoloMode::SixG4:
    case HarmonicTremoloMode::SixG4A:
        return mode;
    }
    return HarmonicTremoloMode::SixG4;
}

float HarmonicTremolo::maximum_crossover_hz() const noexcept {
    return std::min(kMaximumCrossoverHz, sample_rate_ * 0.45F);
}

float HarmonicTremolo::effective_crossover_hz() const noexcept {
    const float mode_scale =
        parameters_.mode == HarmonicTremoloMode::SixG4A ? 1.25F : 1.0F;
    return std::clamp(parameters_.crossover_hz * mode_scale,
                      kMinimumCrossoverHz, maximum_crossover_hz());
}

float HarmonicTremolo::alpha_for_crossover(float frequency_hz) const noexcept {
    const float frequency = std::clamp(frequency_hz, kMinimumCrossoverHz,
                                       maximum_crossover_hz());
    const float radians = kTwoPi * frequency / sample_rate_;
    const float d = 1.0F - std::cos(radians);
    return std::sqrt(d * d + 2.0F * d) - d;
}

float HarmonicTremolo::modulation_value() const noexcept {
    switch (parameters_.waveform) {
    case HarmonicTremoloWaveform::Sine:
        return sine_phase_;
    case HarmonicTremoloWaveform::Triangle:
        return phase_ < 0.5F ? -1.0F + 4.0F * phase_
                             : 3.0F - 4.0F * phase_;
    case HarmonicTremoloWaveform::Square:
        return phase_ < 0.5F ? 1.0F : -1.0F;
    }
    return sine_phase_;
}

void HarmonicTremolo::prepare_oscillator() noexcept {
    const float rate = parameters_.tempo_sync
                           ? parameters_.tempo_bpm /
                                 (60.0F * parameters_.beats_per_cycle)
                           : parameters_.rate_hz;
    effective_rate_hz_ = std::clamp(rate, kMinimumRateHz, kMaximumRateHz);
    phase_increment_ = effective_rate_hz_ / sample_rate_;
    const float step = kTwoPi * phase_increment_;
    sine_increment_ = std::sin(step);
    cosine_increment_ = std::cos(step);
    synchronize_sine_phase();
}

void HarmonicTremolo::synchronize_sine_phase() noexcept {
    const float radians = kTwoPi * phase_;
    sine_phase_ = std::sin(radians);
    cosine_phase_ = std::cos(radians);
    normalization_counter_ = 0;
}

void HarmonicTremolo::retarget_depth(float depth) noexcept {
    parameters_.depth = depth;
    target_depth_ = depth;
    const auto frames = static_cast<std::size_t>(std::llround(
        static_cast<double>(sample_rate_) *
        static_cast<double>(parameters_.smoothing_ms) / 1000.0));
    if (frames == 0) {
        current_depth_ = target_depth_;
        depth_step_ = 0.0F;
        depth_frames_remaining_ = 0;
        return;
    }
    depth_step_ = (target_depth_ - current_depth_) /
                  static_cast<float>(frames);
    depth_frames_remaining_ = frames;
}

void HarmonicTremolo::retarget_crossover(float frequency_hz) noexcept {
    target_alpha_ = alpha_for_crossover(frequency_hz);
    const auto frames = static_cast<std::size_t>(std::llround(
        static_cast<double>(sample_rate_) *
        static_cast<double>(parameters_.smoothing_ms) / 1000.0));
    if (frames == 0) {
        current_alpha_ = target_alpha_;
        alpha_step_ = 0.0F;
        alpha_frames_remaining_ = 0;
        return;
    }
    alpha_step_ = (target_alpha_ - current_alpha_) /
                  static_cast<float>(frames);
    alpha_frames_remaining_ = frames;
}

void HarmonicTremolo::advance_state() noexcept {
    phase_ += phase_increment_;
    if (phase_ >= 1.0F) {
        phase_ -= 1.0F;
    }

    if (parameters_.waveform != HarmonicTremoloWaveform::Sine) {
        return;
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
}

} // namespace tabletfx
