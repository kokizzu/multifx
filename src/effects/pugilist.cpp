#include "tabletfx/effects/pugilist.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kTwoPi = 6.28318530717958647692F;
constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;

DualEngineDistortionMode sanitize_mode(
    DualEngineDistortionMode mode) noexcept {
    switch (mode) {
    case DualEngineDistortionMode::Blend:
    case DualEngineDistortionMode::Series:
        return mode;
    }
    return DualEngineDistortionMode::Blend;
}

} // namespace

DualEngineDistortion::DualEngineDistortion(
    float sample_rate, DualEngineDistortionParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void DualEngineDistortion::configure(
    float sample_rate,
    DualEngineDistortionParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    parameters_ = sanitize(parameters);
    smoothing_ms_ = parameters_.smoothing_ms;
    bass_alpha_ = low_pass_alpha(220.0F, sample_rate_);
    target_drive_gain_a_ = 1.0F + 9.0F * parameters_.gain_a;
    target_tone_alpha_a_ = tone_alpha(parameters_.tone_a);
    target_drive_gain_b_ = 1.0F + 9.0F * parameters_.gain_b;
    target_tone_alpha_b_ = tone_alpha(parameters_.tone_b);
    target_blend_ = parameters_.blend;
    target_level_gain_ = db_to_gain(parameters_.level_db);
    target_bass_gain_ = parameters_.bass_boost ? db_to_gain(6.0F) : 1.0F;
    snap_to_targets();
    reset();
}

void DualEngineDistortion::set_parameters(
    DualEngineDistortionParameters parameters) noexcept {
    parameters_ = sanitize(parameters);
    smoothing_ms_ = parameters_.smoothing_ms;
    target_drive_gain_a_ = 1.0F + 9.0F * parameters_.gain_a;
    target_tone_alpha_a_ = tone_alpha(parameters_.tone_a);
    target_drive_gain_b_ = 1.0F + 9.0F * parameters_.gain_b;
    target_tone_alpha_b_ = tone_alpha(parameters_.tone_b);
    target_blend_ = parameters_.blend;
    target_level_gain_ = db_to_gain(parameters_.level_db);
    target_bass_gain_ = parameters_.bass_boost ? db_to_gain(6.0F) : 1.0F;
    retarget();
}

void DualEngineDistortion::process(std::span<float> interleaved,
                                  std::uint16_t channels) noexcept {
    if (channels == 0U || channels > channel_state_.size()) return;

    const std::size_t channel_count = channels;
    for (std::size_t offset = 0; offset < interleaved.size();
         offset += channel_count) {
        advance_ramp();
        const std::size_t frame_channels =
            std::min(channel_count, interleaved.size() - offset);
        for (std::size_t channel = 0; channel < frame_channels; ++channel) {
            float& sample = interleaved[offset + channel];
            ChannelState& state = channel_state_[channel];
            if (!std::isfinite(sample)) {
                sample = 0.0F;
                state = {};
                continue;
            }

            const float input = std::clamp(sample, -16.0F, 16.0F);
            state.bass_low += bass_alpha_ * (input - state.bass_low);
            const float boosted = input + state.bass_low * (bass_gain_ - 1.0F);

            float output = 0.0F;
            if (parameters_.mode == DualEngineDistortionMode::Blend) {
                const float engine_a = process_engine(
                    boosted, drive_gain_a_, tone_alpha_a_, state.tone_a);
                const float engine_b = process_engine(
                    boosted, drive_gain_b_, tone_alpha_b_, state.tone_b);
                output = engine_a + (engine_b - engine_a) * blend_;
            } else {
                const float engine_a = process_engine(
                    boosted, drive_gain_a_, tone_alpha_a_, state.tone_a);
                output = process_engine(
                    engine_a, drive_gain_b_, tone_alpha_b_, state.tone_b);
            }

            sample = std::clamp(output * level_gain_, -8.0F, 8.0F);
        }
    }
}

void DualEngineDistortion::reset() noexcept {
    snap_to_targets();
    channel_state_.fill({});
    ramp_remaining_ = 0U;
}

DualEngineDistortionParameters DualEngineDistortion::sanitize(
    DualEngineDistortionParameters parameters) noexcept {
    parameters.gain_a = std::isfinite(parameters.gain_a)
                            ? std::clamp(parameters.gain_a, 0.0F, 1.0F)
                            : 0.25F;
    parameters.tone_a = std::isfinite(parameters.tone_a)
                            ? std::clamp(parameters.tone_a, 0.0F, 1.0F)
                            : 0.7F;
    parameters.gain_b = std::isfinite(parameters.gain_b)
                            ? std::clamp(parameters.gain_b, 0.0F, 1.0F)
                            : 0.6F;
    parameters.tone_b = std::isfinite(parameters.tone_b)
                            ? std::clamp(parameters.tone_b, 0.0F, 1.0F)
                            : 0.5F;
    parameters.blend = std::isfinite(parameters.blend)
                           ? std::clamp(parameters.blend, 0.0F, 1.0F)
                           : 0.5F;
    parameters.mode = sanitize_mode(parameters.mode);
    parameters.level_db = std::isfinite(parameters.level_db)
                              ? std::clamp(parameters.level_db, -24.0F, 12.0F)
                              : 0.0F;
    parameters.smoothing_ms = std::isfinite(parameters.smoothing_ms)
                                  ? std::clamp(parameters.smoothing_ms, 0.0F,
                                               1000.0F)
                                  : 10.0F;
    return parameters;
}

float DualEngineDistortion::db_to_gain(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float DualEngineDistortion::soft_clip(float sample) noexcept {
    const float magnitude = std::abs(sample);
    if (magnitude >= 3.0F) return std::copysign(1.0F, sample);
    const float squared = sample * sample;
    return sample * (27.0F + squared) / (27.0F + 9.0F * squared);
}

float DualEngineDistortion::low_pass_alpha(float cutoff_hz,
                                           float sample_rate) noexcept {
    const float cutoff = std::clamp(cutoff_hz, 10.0F,
                                    std::min(18000.0F, sample_rate * 0.45F));
    return 1.0F - std::exp(-kTwoPi * cutoff / sample_rate);
}

float DualEngineDistortion::tone_alpha(float normalized_tone) const noexcept {
    const float cutoff_hz = 600.0F + normalized_tone * 11400.0F;
    return low_pass_alpha(cutoff_hz, sample_rate_);
}

std::size_t DualEngineDistortion::ramp_length() const noexcept {
    const double requested = std::ceil(static_cast<double>(sample_rate_) *
                                       static_cast<double>(smoothing_ms_) /
                                       1000.0);
    return requested >= 1.0 ? static_cast<std::size_t>(requested) : 0U;
}

void DualEngineDistortion::retarget() noexcept {
    const std::size_t samples = ramp_length();
    if (samples == 0U) {
        snap_to_targets();
        ramp_remaining_ = 0U;
        return;
    }

    const float divisor = static_cast<float>(samples);
    drive_gain_a_step_ = (target_drive_gain_a_ - drive_gain_a_) / divisor;
    tone_alpha_a_step_ = (target_tone_alpha_a_ - tone_alpha_a_) / divisor;
    drive_gain_b_step_ = (target_drive_gain_b_ - drive_gain_b_) / divisor;
    tone_alpha_b_step_ = (target_tone_alpha_b_ - tone_alpha_b_) / divisor;
    blend_step_ = (target_blend_ - blend_) / divisor;
    level_gain_step_ = (target_level_gain_ - level_gain_) / divisor;
    bass_gain_step_ = (target_bass_gain_ - bass_gain_) / divisor;
    ramp_remaining_ = samples;
}

void DualEngineDistortion::advance_ramp() noexcept {
    if (ramp_remaining_ == 0U) return;
    drive_gain_a_ += drive_gain_a_step_;
    tone_alpha_a_ += tone_alpha_a_step_;
    drive_gain_b_ += drive_gain_b_step_;
    tone_alpha_b_ += tone_alpha_b_step_;
    blend_ += blend_step_;
    level_gain_ += level_gain_step_;
    bass_gain_ += bass_gain_step_;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) snap_to_targets();
}

void DualEngineDistortion::snap_to_targets() noexcept {
    drive_gain_a_ = target_drive_gain_a_;
    tone_alpha_a_ = target_tone_alpha_a_;
    drive_gain_b_ = target_drive_gain_b_;
    tone_alpha_b_ = target_tone_alpha_b_;
    blend_ = target_blend_;
    level_gain_ = target_level_gain_;
    bass_gain_ = target_bass_gain_;
    drive_gain_a_step_ = 0.0F;
    tone_alpha_a_step_ = 0.0F;
    drive_gain_b_step_ = 0.0F;
    tone_alpha_b_step_ = 0.0F;
    blend_step_ = 0.0F;
    level_gain_step_ = 0.0F;
    bass_gain_step_ = 0.0F;
}

float DualEngineDistortion::process_engine(float input, float drive_gain,
                                           float tone_coefficient,
                                           float& tone_state) const noexcept {
    const float clipped = soft_clip(input * drive_gain);
    tone_state += tone_coefficient * (clipped - tone_state);
    return tone_state;
}

} // namespace tabletfx
