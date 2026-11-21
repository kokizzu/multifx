#include "tabletfx/effects/high_gain_distortion.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {

HighGainDistortion::HighGainDistortion(float sample_rate,
                                       HighGainDistortionParameters parameters) noexcept {
    target_drive_ = bounded_drive(parameters.drive);
    target_pre_low_cut_hz_ = bounded_cutoff(parameters.pre_low_cut_hz, 100.0F, 20.0F, 1000.0F);
    target_pre_high_cut_hz_ = bounded_cutoff(parameters.pre_high_cut_hz, 9000.0F, 1000.0F,
                                              18000.0F);
    target_pre_high_cut_hz_ = std::max(target_pre_high_cut_hz_,
                                       std::min(18000.0F, target_pre_low_cut_hz_ * 1.5F));
    target_post_tone_hz_ = bounded_cutoff(parameters.post_tone_hz, 5200.0F, 500.0F, 18000.0F);
    target_level_db_ = bounded_level(parameters.level_db);
    target_level_gain_ = db_to_gain(target_level_db_);
    target_mix_ = bounded_mix(parameters.mix);
    configure_smoothing(sample_rate, parameters.smoothing_ms);
    reset();
}

void HighGainDistortion::configure(HighGainDistortionParameters parameters) noexcept {
    target_drive_ = bounded_drive(parameters.drive);
    target_pre_low_cut_hz_ = bounded_cutoff(parameters.pre_low_cut_hz, 100.0F, 20.0F, 1000.0F);
    target_pre_high_cut_hz_ = bounded_cutoff(parameters.pre_high_cut_hz, 9000.0F, 1000.0F,
                                              18000.0F);
    target_pre_high_cut_hz_ = std::max(target_pre_high_cut_hz_,
                                       std::min(18000.0F, target_pre_low_cut_hz_ * 1.5F));
    target_post_tone_hz_ = bounded_cutoff(parameters.post_tone_hz, 5200.0F, 500.0F, 18000.0F);
    target_level_db_ = bounded_level(parameters.level_db);
    target_level_gain_ = db_to_gain(target_level_db_);
    target_mix_ = bounded_mix(parameters.mix);
    configure_smoothing(sample_rate_, parameters.smoothing_ms);
    retarget();
}

void HighGainDistortion::set_drive(float drive) noexcept {
    target_drive_ = bounded_drive(drive);
    retarget();
}

void HighGainDistortion::set_pre_low_cut_hz(float frequency_hz) noexcept {
    target_pre_low_cut_hz_ = bounded_cutoff(frequency_hz, 100.0F, 20.0F, 1000.0F);
    target_pre_high_cut_hz_ = std::max(target_pre_high_cut_hz_,
                                       std::min(18000.0F, target_pre_low_cut_hz_ * 1.5F));
    retarget();
}

void HighGainDistortion::set_pre_high_cut_hz(float frequency_hz) noexcept {
    const float minimum = std::max(1000.0F, target_pre_low_cut_hz_ * 1.5F);
    target_pre_high_cut_hz_ = bounded_cutoff(frequency_hz, 9000.0F, minimum, 18000.0F);
    retarget();
}

void HighGainDistortion::set_post_tone_hz(float frequency_hz) noexcept {
    target_post_tone_hz_ = bounded_cutoff(frequency_hz, 5200.0F, 500.0F, 18000.0F);
    retarget();
}

void HighGainDistortion::set_level_db(float level_db) noexcept {
    target_level_db_ = bounded_level(level_db);
    target_level_gain_ = db_to_gain(target_level_db_);
    retarget();
}

void HighGainDistortion::set_mix(float mix) noexcept {
    target_mix_ = bounded_mix(mix);
    retarget();
}

void HighGainDistortion::configure_smoothing(float sample_rate, float smoothing_ms) noexcept {
    sample_rate_ = std::isfinite(sample_rate) && sample_rate >= 8000.0F &&
                           sample_rate <= 384000.0F
                       ? sample_rate
                       : 48000.0F;
    smoothing_ms_ = std::isfinite(smoothing_ms)
                        ? std::clamp(smoothing_ms, 0.0F, 1000.0F)
                        : 10.0F;
    retarget();
}

void HighGainDistortion::process(std::span<float> interleaved,
                                 std::uint16_t channels) noexcept {
    if (channels == 0 || channels > channel_state_.size()) return;

    const std::size_t channel_count = channels;
    const std::size_t frames = interleaved.size() / channel_count;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        advance_ramp();
        for (std::size_t channel = 0; channel < channel_count; ++channel) {
            float& sample = interleaved[frame * channel_count + channel];
            ChannelState& state = channel_state_[channel];
            if (!std::isfinite(sample)) {
                sample = 0.0F;
                state = {};
                continue;
            }

            const float dry = sample;
            const float bounded_input = std::clamp(dry, -16.0F, 16.0F);
            const float high_pass = pre_high_pass_alpha_ *
                                    (state.pre_high_pass_output + bounded_input -
                                     state.pre_high_pass_input);
            state.pre_high_pass_input = bounded_input;
            state.pre_high_pass_output = high_pass;

            state.pre_low_pass += pre_low_pass_alpha_ * (high_pass - state.pre_low_pass);
            const float driven = state.pre_low_pass * drive_gain_;
            const float saturated = soft_clip(driven);
            state.post_low_pass += post_low_pass_alpha_ * (saturated - state.post_low_pass);
            const float wet = state.post_low_pass;
            sample = (dry + (wet - dry) * mix_) * level_gain_;
        }
    }
}

void HighGainDistortion::reset() noexcept {
    channel_state_ = {};
    snap_to_targets();
    drive_gain_step_ = 0.0F;
    pre_high_pass_alpha_step_ = 0.0F;
    pre_low_pass_alpha_step_ = 0.0F;
    post_low_pass_alpha_step_ = 0.0F;
    level_gain_step_ = 0.0F;
    mix_step_ = 0.0F;
    ramp_remaining_ = 0;
}

void HighGainDistortion::retarget() noexcept {
    const float desired_drive_gain = 1.0F + 47.0F * target_drive_;
    const float desired_pre_high_pass_alpha =
        high_pass_alpha(target_pre_low_cut_hz_, sample_rate_);
    const float desired_pre_low_pass_alpha =
        low_pass_alpha(target_pre_high_cut_hz_, sample_rate_);
    const float desired_post_low_pass_alpha =
        low_pass_alpha(target_post_tone_hz_, sample_rate_);
    const std::size_t samples = ramp_length();
    if (samples == 0) {
        drive_gain_ = desired_drive_gain;
        pre_high_pass_alpha_ = desired_pre_high_pass_alpha;
        pre_low_pass_alpha_ = desired_pre_low_pass_alpha;
        post_low_pass_alpha_ = desired_post_low_pass_alpha;
        level_gain_ = target_level_gain_;
        mix_ = target_mix_;
        drive_gain_step_ = 0.0F;
        pre_high_pass_alpha_step_ = 0.0F;
        pre_low_pass_alpha_step_ = 0.0F;
        post_low_pass_alpha_step_ = 0.0F;
        level_gain_step_ = 0.0F;
        mix_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }

    const float divisor = static_cast<float>(samples);
    drive_gain_step_ = (desired_drive_gain - drive_gain_) / divisor;
    pre_high_pass_alpha_step_ =
        (desired_pre_high_pass_alpha - pre_high_pass_alpha_) / divisor;
    pre_low_pass_alpha_step_ = (desired_pre_low_pass_alpha - pre_low_pass_alpha_) / divisor;
    post_low_pass_alpha_step_ =
        (desired_post_low_pass_alpha - post_low_pass_alpha_) / divisor;
    level_gain_step_ = (target_level_gain_ - level_gain_) / divisor;
    mix_step_ = (target_mix_ - mix_) / divisor;
    ramp_remaining_ = samples;
}

void HighGainDistortion::advance_ramp() noexcept {
    if (ramp_remaining_ == 0) return;
    drive_gain_ += drive_gain_step_;
    pre_high_pass_alpha_ += pre_high_pass_alpha_step_;
    pre_low_pass_alpha_ += pre_low_pass_alpha_step_;
    post_low_pass_alpha_ += post_low_pass_alpha_step_;
    level_gain_ += level_gain_step_;
    mix_ += mix_step_;
    --ramp_remaining_;
    if (ramp_remaining_ == 0) snap_to_targets();
}

void HighGainDistortion::snap_to_targets() noexcept {
    drive_gain_ = 1.0F + 47.0F * target_drive_;
    pre_high_pass_alpha_ = high_pass_alpha(target_pre_low_cut_hz_, sample_rate_);
    pre_low_pass_alpha_ = low_pass_alpha(target_pre_high_cut_hz_, sample_rate_);
    post_low_pass_alpha_ = low_pass_alpha(target_post_tone_hz_, sample_rate_);
    level_gain_ = target_level_gain_;
    mix_ = target_mix_;
}

float HighGainDistortion::bounded_drive(float drive) noexcept {
    return std::isfinite(drive) ? std::clamp(drive, 0.0F, 1.0F) : 0.75F;
}

float HighGainDistortion::bounded_cutoff(float frequency_hz, float fallback_hz,
                                         float minimum_hz, float maximum_hz) noexcept {
    return std::isfinite(frequency_hz) ? std::clamp(frequency_hz, minimum_hz, maximum_hz)
                                       : fallback_hz;
}

float HighGainDistortion::bounded_level(float level_db) noexcept {
    return std::isfinite(level_db) ? std::clamp(level_db, -24.0F, 12.0F) : -9.0F;
}

float HighGainDistortion::bounded_mix(float mix) noexcept {
    return std::isfinite(mix) ? std::clamp(mix, 0.0F, 1.0F) : 1.0F;
}

float HighGainDistortion::db_to_gain(float level_db) noexcept {
    return std::pow(10.0F, level_db / 20.0F);
}

float HighGainDistortion::low_pass_alpha(float cutoff_hz, float sample_rate) noexcept {
    const float usable_cutoff = std::clamp(cutoff_hz, 1.0F, sample_rate * 0.45F);
    constexpr float two_pi = 6.28318530717958647692F;
    return 1.0F - std::exp(-two_pi * usable_cutoff / sample_rate);
}

float HighGainDistortion::high_pass_alpha(float cutoff_hz, float sample_rate) noexcept {
    const float usable_cutoff = std::clamp(cutoff_hz, 1.0F, sample_rate * 0.45F);
    constexpr float two_pi = 6.28318530717958647692F;
    return std::exp(-two_pi * usable_cutoff / sample_rate);
}

float HighGainDistortion::soft_clip(float sample) noexcept {
    const float magnitude = std::abs(sample);
    if (magnitude >= 3.0F) return std::copysign(1.0F, sample);
    const float squared = sample * sample;
    return sample * (27.0F + squared) / (27.0F + 9.0F * squared);
}

std::size_t HighGainDistortion::ramp_length() const noexcept {
    const double requested = std::ceil(static_cast<double>(sample_rate_) *
                                       static_cast<double>(smoothing_ms_) / 1000.0);
    return requested >= 1.0 ? static_cast<std::size_t>(requested) : 0;
}

} // namespace tabletfx
