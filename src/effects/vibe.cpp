#include "tabletfx/effects/vibe.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

float beats_per_vibe_cycle(VibeNoteDivision division) noexcept {
    switch (division) {
    case VibeNoteDivision::Whole: return 4.0F;
    case VibeNoteDivision::DottedHalf: return 3.0F;
    case VibeNoteDivision::Half: return 2.0F;
    case VibeNoteDivision::DottedQuarter: return 1.5F;
    case VibeNoteDivision::Quarter: return 1.0F;
    case VibeNoteDivision::QuarterTriplet: return 2.0F / 3.0F;
    case VibeNoteDivision::DottedEighth: return 0.75F;
    case VibeNoteDivision::Eighth: return 0.5F;
    case VibeNoteDivision::EighthTriplet: return 1.0F / 3.0F;
    case VibeNoteDivision::DottedSixteenth: return 0.375F;
    case VibeNoteDivision::Sixteenth: return 0.25F;
    case VibeNoteDivision::SixteenthTriplet: return 1.0F / 6.0F;
    }
    return 1.0F;
}

} // namespace

Vibe::Vibe(float sample_rate, std::uint16_t channels,
           VibeParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument(
            "Vibe requires 8–384 kHz and mono or stereo channels");
    }
    set_parameters(parameters);
    retarget(true);
}

void Vibe::set_parameters(VibeParameters parameters) noexcept {
    const std::uint8_t previous_stages = parameters_.stages;
    parameters.rate_hz = bounded(parameters.rate_hz, 1.5F,
                                 kMinimumRateHz, kMaximumRateHz);
    parameters.depth = bounded(parameters.depth, 0.7F, 0.0F, 1.0F);
    parameters.trem = bounded(parameters.trem, 0.2F, 0.0F, 1.0F);
    if (parameters.stages != 1 && parameters.stages != 2) {
        parameters.stages = 2;
    }
    parameters.spread = bounded(parameters.spread, 0.5F, 0.0F, 1.0F);
    parameters.level = bounded(parameters.level, 1.0F, 0.0F, 2.0F);
    if (static_cast<std::uint8_t>(parameters.rate_mode) >
        static_cast<std::uint8_t>(VibeRateMode::TempoNote)) {
        parameters.rate_mode = VibeRateMode::Free;
    }
    if (static_cast<std::uint8_t>(parameters.note_division) >
        static_cast<std::uint8_t>(VibeNoteDivision::SixteenthTriplet)) {
        parameters.note_division = VibeNoteDivision::Quarter;
    }
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, 120.0F, 20.0F, 300.0F);
    if (parameters.stages != previous_stages) {
        stages_ = {};
    }

    parameters_ = parameters;
    effective_rate_hz_ = parameters_.rate_hz;
    if (parameters_.rate_mode == VibeRateMode::TempoNote) {
        const float beats = beats_per_vibe_cycle(parameters_.note_division);
        const float tempo_rate = parameters_.tempo_bpm / (60.0F * beats);
        effective_rate_hz_ = bounded(tempo_rate, 2.0F,
                                     kMinimumTempoRateHz,
                                     kMaximumTempoRateHz);
    }
    target_depth_ = parameters_.depth;
    target_trem_ = parameters_.trem;
    target_level_ = parameters_.level;

    const float spread_radians = kPi * parameters_.spread;
    target_spread_cosine_ = std::cos(spread_radians);
    target_spread_sine_ = std::sin(spread_radians);

    const float lfo_step = kTwoPi * effective_rate_hz_ / sample_rate_;
    target_sine_increment_ = std::sin(lfo_step);
    target_cosine_increment_ = std::cos(lfo_step);
    retarget(false);
}

void Vibe::process(std::span<float> interleaved,
                   std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (ramp_remaining_ > 0) {
            current_depth_ += depth_step_;
            current_trem_ += trem_step_;
            current_spread_cosine_ += spread_cosine_step_;
            current_spread_sine_ += spread_sine_step_;
            current_level_ += level_step_;
            current_sine_increment_ += sine_increment_step_;
            current_cosine_increment_ += cosine_increment_step_;
            --ramp_remaining_;
            if (ramp_remaining_ == 0) {
                current_depth_ = target_depth_;
                current_trem_ = target_trem_;
                current_spread_cosine_ = target_spread_cosine_;
                current_spread_sine_ = target_spread_sine_;
                current_level_ = target_level_;
                current_sine_increment_ = target_sine_increment_;
                current_cosine_increment_ = target_cosine_increment_;
                depth_step_ = 0.0F;
                trem_step_ = 0.0F;
                spread_cosine_step_ = 0.0F;
                spread_sine_step_ = 0.0F;
                level_step_ = 0.0F;
                sine_increment_step_ = 0.0F;
                cosine_increment_step_ = 0.0F;
            }
        }

        const float coefficient = std::clamp(
            0.5F + 0.45F * current_depth_ * sine_phase_, 0.05F, 0.95F);
        const std::size_t input_offset = frame * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = input_offset + channel;
            const float sample = std::isfinite(interleaved[index])
                                     ? std::clamp(interleaved[index],
                                                  -kMaximumInternalSample,
                                                  kMaximumInternalSample)
                                     : 0.0F;
            float phase_shifted = sample;
            for (std::size_t stage = 0; stage < parameters_.stages; ++stage) {
                StageState& state = stages_[channel][stage];
                const float output = coefficient * phase_shifted +
                                     state.previous_input -
                                     coefficient * state.previous_output;
                state.previous_input = phase_shifted;
                state.previous_output = std::isfinite(output)
                                           ? std::clamp(output,
                                                        -kMaximumInternalSample,
                                                        kMaximumInternalSample)
                                           : 0.0F;
                phase_shifted = state.previous_output;
            }

            const float phase_mix = (1.0F - current_depth_) * sample +
                                    current_depth_ * phase_shifted;
            float trem_lfo = sine_phase_;
            if (channels_ == 2 && channel == 1) {
                trem_lfo = sine_phase_ * current_spread_cosine_ +
                           cosine_phase_ * current_spread_sine_;
            }
            const float trem_gain =
                1.0F - 0.5F * current_trem_ * (1.0F - trem_lfo);
            const float output = phase_mix * trem_gain * current_level_;
            interleaved[index] = std::isfinite(output)
                                     ? std::clamp(output,
                                                  -kMaximumInternalSample,
                                                  kMaximumInternalSample)
                                     : 0.0F;
        }
        advance_lfo();
    }
}

void Vibe::reset() noexcept {
    stages_ = {};
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    normalization_counter_ = 0;
    retarget(true);
}

float Vibe::bounded(float value, float fallback,
                    float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum)
                                : fallback;
}

void Vibe::retarget(bool immediate) noexcept {
    if (immediate) {
        current_depth_ = target_depth_;
        current_trem_ = target_trem_;
        current_spread_cosine_ = target_spread_cosine_;
        current_spread_sine_ = target_spread_sine_;
        current_level_ = target_level_;
        current_sine_increment_ = target_sine_increment_;
        current_cosine_increment_ = target_cosine_increment_;
        depth_step_ = 0.0F;
        trem_step_ = 0.0F;
        spread_cosine_step_ = 0.0F;
        spread_sine_step_ = 0.0F;
        level_step_ = 0.0F;
        sine_increment_step_ = 0.0F;
        cosine_increment_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }

    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(kControlRampMs) / 1000.0;
    ramp_remaining_ = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_remaining_ == 0) {
        retarget(true);
        return;
    }

    const float denominator = static_cast<float>(ramp_remaining_);
    depth_step_ = (target_depth_ - current_depth_) / denominator;
    trem_step_ = (target_trem_ - current_trem_) / denominator;
    spread_cosine_step_ =
        (target_spread_cosine_ - current_spread_cosine_) / denominator;
    spread_sine_step_ =
        (target_spread_sine_ - current_spread_sine_) / denominator;
    level_step_ = (target_level_ - current_level_) / denominator;
    sine_increment_step_ =
        (target_sine_increment_ - current_sine_increment_) / denominator;
    cosine_increment_step_ =
        (target_cosine_increment_ - current_cosine_increment_) / denominator;
}

void Vibe::advance_lfo() noexcept {
    const float old_sine = sine_phase_;
    sine_phase_ = old_sine * current_cosine_increment_ +
                  cosine_phase_ * current_sine_increment_;
    cosine_phase_ = cosine_phase_ * current_cosine_increment_ -
                    old_sine * current_sine_increment_;
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
