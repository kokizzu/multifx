#include "tabletfx/effects/auto_pan.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTwoPi = 2.0F * kPi;
constexpr float kSqrtTwo = 1.41421356237309504880F;

AutoPanWaveform sanitize_waveform(AutoPanWaveform waveform) noexcept {
    switch (waveform) {
    case AutoPanWaveform::Square:
    case AutoPanWaveform::Sine:
    case AutoPanWaveform::Triangle:
        return waveform;
    }
    return AutoPanWaveform::Sine;
}

AutoPanDivision sanitize_division(AutoPanDivision division) noexcept {
    switch (division) {
    case AutoPanDivision::Free:
    case AutoPanDivision::Whole:
    case AutoPanDivision::Half:
    case AutoPanDivision::DottedHalf:
    case AutoPanDivision::HalfTriplet:
    case AutoPanDivision::Quarter:
    case AutoPanDivision::DottedQuarter:
    case AutoPanDivision::QuarterTriplet:
    case AutoPanDivision::Eighth:
    case AutoPanDivision::DottedEighth:
    case AutoPanDivision::EighthTriplet:
    case AutoPanDivision::Sixteenth:
    case AutoPanDivision::DottedSixteenth:
    case AutoPanDivision::SixteenthTriplet:
    case AutoPanDivision::ThirtySecond:
    case AutoPanDivision::ThirtySecondTriplet:
        return division;
    }
    return AutoPanDivision::Quarter;
}

float beats_per_cycle(AutoPanDivision division) noexcept {
    switch (division) {
    case AutoPanDivision::Free: return 1.0F;
    case AutoPanDivision::Whole: return 4.0F;
    case AutoPanDivision::Half: return 2.0F;
    case AutoPanDivision::DottedHalf: return 3.0F;
    case AutoPanDivision::HalfTriplet: return 4.0F / 3.0F;
    case AutoPanDivision::Quarter: return 1.0F;
    case AutoPanDivision::DottedQuarter: return 1.5F;
    case AutoPanDivision::QuarterTriplet: return 2.0F / 3.0F;
    case AutoPanDivision::Eighth: return 0.5F;
    case AutoPanDivision::DottedEighth: return 0.75F;
    case AutoPanDivision::EighthTriplet: return 1.0F / 3.0F;
    case AutoPanDivision::Sixteenth: return 0.25F;
    case AutoPanDivision::DottedSixteenth: return 0.375F;
    case AutoPanDivision::SixteenthTriplet: return 1.0F / 6.0F;
    case AutoPanDivision::ThirtySecond: return 0.125F;
    case AutoPanDivision::ThirtySecondTriplet: return 1.0F / 12.0F;
    }
    return 1.0F;
}

float safe_audio(float sample) noexcept {
    if (!std::isfinite(sample)) return 0.0F;
    return std::clamp(sample, -8.0F, 8.0F);
}

} // namespace

AutoPan::AutoPan(float sample_rate, AutoPanParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void AutoPan::configure(float sample_rate,
                        AutoPanParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    parameters.rate_hz = bounded(parameters.rate_hz, 0.5F,
                                 kMinimumRateHz, kMaximumRateHz);
    parameters.depth = bounded(parameters.depth, 0.75F, 0.0F, 1.0F);
    parameters.waveform = sanitize_waveform(parameters.waveform);
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, 120.0F,
                                   kMinimumBpm, kMaximumBpm);
    parameters.division = sanitize_division(parameters.division);
    parameters.depth_smoothing_ms = bounded(parameters.depth_smoothing_ms,
                                              10.0F, 0.0F, 1000.0F);
    parameters_ = parameters;
    current_depth_ = parameters_.depth;
    target_depth_ = parameters_.depth;
    depth_ramp_step_ = 0.0F;
    depth_ramp_remaining_ = 0;
    prepare_oscillator();
    reset();
}

void AutoPan::set_rate_hz(float rate_hz) noexcept {
    parameters_.rate_hz = bounded(rate_hz, parameters_.rate_hz,
                                  kMinimumRateHz, kMaximumRateHz);
    if (!parameters_.tempo_sync) prepare_oscillator();
}

void AutoPan::set_depth(float depth) noexcept {
    parameters_.depth = bounded(depth, parameters_.depth, 0.0F, 1.0F);
    target_depth_ = parameters_.depth;
    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(parameters_.depth_smoothing_ms) /
                                    1000.0;
    const auto ramp_frames = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_frames == 0U) {
        current_depth_ = target_depth_;
        depth_ramp_step_ = 0.0F;
        depth_ramp_remaining_ = 0U;
        return;
    }
    depth_ramp_step_ = (target_depth_ - current_depth_) /
                       static_cast<float>(ramp_frames);
    depth_ramp_remaining_ = ramp_frames;
}

void AutoPan::set_depth_smoothing_ms(float smoothing_ms) noexcept {
    parameters_.depth_smoothing_ms = bounded(smoothing_ms, 10.0F,
                                             0.0F, 1000.0F);
}

void AutoPan::set_waveform(AutoPanWaveform waveform) noexcept {
    parameters_.waveform = sanitize_waveform(waveform);
}

void AutoPan::set_tempo_sync(bool enabled, float bpm,
                             AutoPanDivision division) noexcept {
    parameters_.tempo_sync = enabled;
    parameters_.tempo_bpm = bounded(bpm, parameters_.tempo_bpm,
                                    kMinimumBpm, kMaximumBpm);
    parameters_.division = sanitize_division(division);
    prepare_oscillator();
}

void AutoPan::process(std::span<float> interleaved,
                      std::uint16_t channels) noexcept {
    if (channels == 0U) return;

    const std::size_t frames = interleaved.size() / channels;
    if (channels != 2U) {
        for (float& sample : interleaved) sample = safe_audio(sample);
        return;
    }

    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (depth_ramp_remaining_ > 0U) {
            current_depth_ += depth_ramp_step_;
            --depth_ramp_remaining_;
            if (depth_ramp_remaining_ == 0U) {
                current_depth_ = target_depth_;
                depth_ramp_step_ = 0.0F;
            }
        }

        const std::size_t offset = frame * 2U;
        const float original_left = safe_audio(interleaved[offset]);
        const float original_right = safe_audio(interleaved[offset + 1U]);
        if (current_depth_ > 0.0F) {
            const float wave = waveform_value();
            const float pan = 0.5F + 0.5F * current_depth_ * wave;
            const float mid = 0.5F * (original_left + original_right);
            const float panned_left = mid * (kSqrtTwo * std::sqrt(1.0F - pan));
            const float panned_right = mid * (kSqrtTwo * std::sqrt(pan));
            interleaved[offset] = safe_audio(
                original_left + current_depth_ * (panned_left - original_left));
            interleaved[offset + 1U] = safe_audio(
                original_right + current_depth_ * (panned_right - original_right));
        } else {
            interleaved[offset] = original_left;
            interleaved[offset + 1U] = original_right;
        }

        advance_oscillator();
    }
    if (interleaved.size() % 2U != 0U) {
        interleaved.back() = safe_audio(interleaved.back());
    }
}

void AutoPan::reset() noexcept {
    phase_ = 0.0F;
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    normalization_counter_ = 0U;
    current_depth_ = parameters_.depth;
    target_depth_ = parameters_.depth;
    depth_ramp_step_ = 0.0F;
    depth_ramp_remaining_ = 0U;
}

float AutoPan::bounded(float value, float fallback,
                       float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

void AutoPan::prepare_oscillator() noexcept {
    effective_rate_hz_ = parameters_.tempo_sync
                             ? (parameters_.tempo_bpm / 60.0F) /
                                   beats_per_cycle(parameters_.division)
                             : parameters_.rate_hz;
    effective_rate_hz_ = std::clamp(effective_rate_hz_, kMinimumRateHz,
                                    std::min(kMaximumRateHz,
                                             sample_rate_ * 0.25F));
    phase_increment_ = effective_rate_hz_ / sample_rate_;
    const float radians = kTwoPi * phase_increment_;
    sine_increment_ = std::sin(radians);
}

float AutoPan::waveform_value() const noexcept {
    switch (parameters_.waveform) {
    case AutoPanWaveform::Square:
        return phase_ < 0.5F ? -1.0F : 1.0F;
    case AutoPanWaveform::Sine:
        return sine_phase_;
    case AutoPanWaveform::Triangle:
        return phase_ < 0.5F ? -1.0F + 4.0F * phase_
                             : 3.0F - 4.0F * phase_;
    }
    return sine_phase_;
}

void AutoPan::advance_oscillator() noexcept {
    phase_ += phase_increment_;
    if (phase_ >= 1.0F) phase_ -= 1.0F;

    const float next_sine = sine_phase_ + sine_increment_ * cosine_phase_;
    const float next_cosine = cosine_phase_ - sine_increment_ * sine_phase_;
    sine_phase_ = next_sine;
    cosine_phase_ = next_cosine;
    if (++normalization_counter_ == 4096U) {
        const float magnitude = std::sqrt(sine_phase_ * sine_phase_ +
                                          cosine_phase_ * cosine_phase_);
        if (magnitude > 0.0F && std::isfinite(magnitude)) {
            sine_phase_ /= magnitude;
            cosine_phase_ /= magnitude;
        } else {
            sine_phase_ = 0.0F;
            cosine_phase_ = 1.0F;
        }
        normalization_counter_ = 0U;
    }
}

} // namespace tabletfx
