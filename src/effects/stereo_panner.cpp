#include "tabletfx/effects/stereo_panner.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTwoPi = 2.0F * kPi;

float safe_audio(float sample) noexcept {
    if (!std::isfinite(sample)) return 0.0F;
    return std::clamp(sample, -8.0F, 8.0F);
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

std::uint32_t hash_cycle(std::uint32_t value) noexcept {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

} // namespace

StereoPanner::StereoPanner(float sample_rate,
                           StereoPannerParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void StereoPanner::configure(float sample_rate,
                             StereoPannerParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    parameters_ = parameters;
    parameters_.rate_hz = bounded(parameters_.rate_hz, 0.5F,
                                  kMinimumRateHz, kMaximumRateHz);
    parameters_.width_percent = bounded(parameters_.width_percent, 100.0F,
                                         0.0F, 200.0F);
    parameters_.waveform = sanitize_waveform(parameters_.waveform);
    parameters_.tempo_bpm = bounded(parameters_.tempo_bpm, 120.0F,
                                    kMinimumBpm, kMaximumBpm);
    const auto division_value = static_cast<unsigned>(parameters.division);
    parameters_.division = division_value <= 15U
        ? static_cast<AutoPanDivision>(division_value)
        : AutoPanDivision::Quarter;
    parameters_.duty_cycle_percent = bounded(
        parameters_.duty_cycle_percent, 50.0F, 0.0F, 100.0F);
    parameters_.shape_percent = bounded(parameters_.shape_percent, 50.0F,
                                        0.0F, 100.0F);
    parameters_.phase_degrees = bounded(parameters_.phase_degrees, 180.0F,
                                         0.0F, 360.0F);
    parameters_.pan_center_percent = bounded(
        parameters_.pan_center_percent, 0.0F, -100.0F, 100.0F);
    parameters_.ducking_percent = bounded(parameters_.ducking_percent, 0.0F,
                                          0.0F, 100.0F);
    parameters_.level_db = bounded(parameters_.level_db, 0.0F, -24.0F, 12.0F);
    parameters_.balance_percent = bounded(
        parameters_.balance_percent, 0.0F, -100.0F, 100.0F);
    parameters_.smoothing_ms = bounded(parameters_.smoothing_ms, 10.0F,
                                        0.0F, 1000.0F);
    target_level_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    current_width_percent_ = parameters_.width_percent;
    current_pan_center_percent_ = parameters_.pan_center_percent;
    current_ducking_percent_ = parameters_.ducking_percent;
    current_level_gain_ = target_level_gain_;
    current_balance_percent_ = parameters_.balance_percent;
    const float smoothing_seconds = parameters_.smoothing_ms / 1000.0F;
    smoothing_coefficient_ = smoothing_seconds <= 0.0F
        ? 0.0F
        : std::exp(-1.0F / (sample_rate_ * smoothing_seconds));
    envelope_attack_coefficient_ = std::exp(-1.0F / (sample_rate_ * 0.005F));
    envelope_release_coefficient_ = std::exp(-1.0F / (sample_rate_ * 0.12F));
    prepare_oscillator();
    reset();
}

void StereoPanner::set_parameters(StereoPannerParameters parameters) noexcept {
    const float phase = phase_;
    const std::uint32_t random_cycle = random_cycle_;
    const float envelope = envelope_;
    const float current_width = current_width_percent_;
    const float current_center = current_pan_center_percent_;
    const float current_ducking = current_ducking_percent_;
    const float current_level = current_level_gain_;
    const float current_balance = current_balance_percent_;
    configure(sample_rate_, parameters);
    phase_ = phase;
    random_cycle_ = random_cycle;
    envelope_ = envelope;
    current_width_percent_ = current_width;
    current_pan_center_percent_ = current_center;
    current_ducking_percent_ = current_ducking;
    current_level_gain_ = current_level;
    current_balance_percent_ = current_balance;
}

float StereoPanner::bounded(float value, float fallback,
                            float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

StereoPannerWaveform StereoPanner::sanitize_waveform(
    StereoPannerWaveform waveform) noexcept {
    switch (waveform) {
    case StereoPannerWaveform::Sine:
    case StereoPannerWaveform::Triangle:
    case StereoPannerWaveform::Square:
    case StereoPannerWaveform::SawUp:
    case StereoPannerWaveform::SawDown:
    case StereoPannerWaveform::Random:
    case StereoPannerWaveform::Logarithmic:
    case StereoPannerWaveform::Exponential:
    case StereoPannerWaveform::Trapezoid:
    case StereoPannerWaveform::Astable:
        return waveform;
    }
    return StereoPannerWaveform::Sine;
}

void StereoPanner::prepare_oscillator() noexcept {
    effective_rate_hz_ = parameters_.tempo_sync
        ? (parameters_.tempo_bpm / 60.0F) / beats_per_cycle(parameters_.division)
        : parameters_.rate_hz;
    effective_rate_hz_ = std::clamp(effective_rate_hz_, kMinimumRateHz,
                                    std::min(kMaximumRateHz,
                                             sample_rate_ * 0.25F));
    phase_increment_ = effective_rate_hz_ / sample_rate_;
}

float StereoPanner::waveform_value(float phase) const noexcept {
    phase -= std::floor(phase);
    const float duty = std::clamp(parameters_.duty_cycle_percent / 100.0F,
                                  0.01F, 0.99F);
    float value = 0.0F;
    switch (parameters_.waveform) {
    case StereoPannerWaveform::Sine:
        value = std::sin(kTwoPi * phase);
        break;
    case StereoPannerWaveform::Triangle:
        value = phase < duty
            ? -1.0F + 2.0F * phase / duty
            : 1.0F - 2.0F * (phase - duty) / (1.0F - duty);
        break;
    case StereoPannerWaveform::Square:
        value = phase < duty ? 1.0F : -1.0F;
        break;
    case StereoPannerWaveform::SawUp:
        value = 2.0F * phase - 1.0F;
        break;
    case StereoPannerWaveform::SawDown:
        value = 1.0F - 2.0F * phase;
        break;
    case StereoPannerWaveform::Random: {
        const auto bucket = static_cast<std::uint32_t>(phase * 8.0F);
        std::uint32_t hash = hash_cycle(random_cycle_ * 8U + bucket);
        value = static_cast<float>(hash & 0xffffU) / 32767.5F - 1.0F;
        break;
    }
    case StereoPannerWaveform::Logarithmic:
        value = 2.0F * (std::log1p(9.0F * phase) / std::log(10.0F)) - 1.0F;
        break;
    case StereoPannerWaveform::Exponential:
        value = 2.0F * phase * phase - 1.0F;
        break;
    case StereoPannerWaveform::Trapezoid: {
        const float ramp = std::min(duty * 0.5F, 0.49F);
        const float fall_start = 1.0F - ramp;
        if (phase < ramp) value = -1.0F + 2.0F * phase / ramp;
        else if (phase < fall_start) value = 1.0F;
        else value = 1.0F - 2.0F * (phase - fall_start) / ramp;
        break;
    }
    case StereoPannerWaveform::Astable:
        value = phase < duty ? 1.0F : -1.0F;
        value *= 0.85F + 0.15F * std::sin(kTwoPi * phase);
        break;
    }
    return shape_waveform(std::clamp(value, -1.0F, 1.0F));
}

float StereoPanner::shape_waveform(float value) const noexcept {
    const float amount = parameters_.shape_percent / 100.0F;
    const float exponent = amount <= 0.5F
        ? 0.25F + amount * 1.5F
        : 1.0F + (amount - 0.5F) * 6.0F;
    const float magnitude = std::pow(std::abs(value), exponent);
    return std::copysign(magnitude, value);
}

void StereoPanner::advance_oscillator() noexcept {
    phase_ += phase_increment_;
    if (phase_ >= 1.0F) {
        phase_ -= 1.0F;
        ++random_cycle_;
    }
}

void StereoPanner::process(std::span<float> interleaved,
                           std::uint16_t channels) noexcept {
    if (channels == 0U) return;
    if (channels != 2U) {
        for (float& sample : interleaved) sample = safe_audio(sample);
        return;
    }

    for (std::size_t offset = 0; offset + 1U < interleaved.size(); offset += 2U) {
        current_width_percent_ = smoothing_coefficient_ * current_width_percent_ +
            (1.0F - smoothing_coefficient_) * parameters_.width_percent;
        current_pan_center_percent_ = smoothing_coefficient_ *
            current_pan_center_percent_ + (1.0F - smoothing_coefficient_) *
            parameters_.pan_center_percent;
        current_ducking_percent_ = smoothing_coefficient_ *
            current_ducking_percent_ + (1.0F - smoothing_coefficient_) *
            parameters_.ducking_percent;
        current_level_gain_ = smoothing_coefficient_ * current_level_gain_ +
            (1.0F - smoothing_coefficient_) * target_level_gain_;
        current_balance_percent_ = smoothing_coefficient_ *
            current_balance_percent_ + (1.0F - smoothing_coefficient_) *
            parameters_.balance_percent;

        const float left = safe_audio(interleaved[offset]);
        const float right = safe_audio(interleaved[offset + 1U]);
        const float mid = 0.5F * (left + right);
        const float side = 0.5F * (left - right);
        const float magnitude = std::abs(mid);
        const float envelope_coefficient = magnitude > envelope_
            ? envelope_attack_coefficient_ : envelope_release_coefficient_;
        envelope_ = envelope_coefficient * envelope_ +
            (1.0F - envelope_coefficient) * magnitude;

        const float phase_offset = parameters_.phase_degrees / 360.0F;
        const float left_wave = waveform_value(phase_);
        const float right_wave = waveform_value(phase_ + phase_offset);
        const float stereo_motion = 0.5F * (left_wave - right_wave);
        const float activity = std::clamp(envelope_ / (envelope_ + 0.05F),
                                          0.0F, 1.0F);
        const float duck_factor = 1.0F -
            (current_ducking_percent_ / 100.0F) * activity;
        const float motion = stereo_motion * duck_factor;
        const float width_scale = current_width_percent_ / 100.0F;
        const float center_offset = current_pan_center_percent_ / 200.0F;
        const float pan = std::clamp(0.5F + center_offset +
                                     0.5F * width_scale * motion,
                                     0.0F, 1.0F);
        const float panned_left = mid * std::sqrt(2.0F * (1.0F - pan));
        const float panned_right = mid * std::sqrt(2.0F * pan);
        const float wet_blend = std::clamp(width_scale, 0.0F, 1.0F);
        const float beyond_stereo = std::max(0.0F, width_scale - 1.0F);
        float out_left = left * (1.0F - wet_blend) +
                         panned_left * wet_blend + side * beyond_stereo * motion;
        float out_right = right * (1.0F - wet_blend) +
                          panned_right * wet_blend - side * beyond_stereo * motion;
        const float balance = current_balance_percent_ / 100.0F;
        const float left_gain = std::sqrt(std::clamp(1.0F - balance, 0.0F, 2.0F));
        const float right_gain = std::sqrt(std::clamp(1.0F + balance, 0.0F, 2.0F));
        interleaved[offset] = safe_audio(out_left * current_level_gain_ * left_gain);
        interleaved[offset + 1U] = safe_audio(out_right * current_level_gain_ * right_gain);
        advance_oscillator();
    }
    if (interleaved.size() % 2U != 0U) {
        interleaved.back() = safe_audio(interleaved.back());
    }
}

void StereoPanner::reset() noexcept {
    phase_ = 0.0F;
    random_cycle_ = 0U;
    envelope_ = 0.0F;
    current_width_percent_ = parameters_.width_percent;
    current_pan_center_percent_ = parameters_.pan_center_percent;
    current_ducking_percent_ = parameters_.ducking_percent;
    current_level_gain_ = target_level_gain_;
    current_balance_percent_ = parameters_.balance_percent;
}

} // namespace tabletfx
