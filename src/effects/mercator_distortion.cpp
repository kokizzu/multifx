#include "tabletfx/effects/mercator_distortion.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kTwoPi = 6.28318530717958647692F;
constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kStageOneGain = 1.5F;
constexpr float kInterstageGainRange = 6.0F;
constexpr float kBrightBoostDb = 6.0F;
constexpr float kBrightCutoffHz = 1600.0F;
constexpr float kBassSplitHz = 180.0F;
constexpr float kTrebleSplitHz = 1800.0F;

} // namespace

MercatorDistortion::MercatorDistortion(
    float sample_rate, MercatorDistParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void MercatorDistortion::configure(
    float sample_rate,
    MercatorDistParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    bright_alpha_ = low_pass_alpha(kBrightCutoffHz, sample_rate_);
    tone_low_alpha_ = low_pass_alpha(kBassSplitHz, sample_rate_);
    tone_high_alpha_ = low_pass_alpha(kTrebleSplitHz, sample_rate_);
    parameters_ = sanitize(parameters);
    smoothing_ms_ = parameters_.smoothing_ms;
    target_drive_gain_ = 1.0F + kInterstageGainRange * parameters_.drive;
    target_drive_2_gain_ = 1.0F + kInterstageGainRange * parameters_.drive_2;
    target_bass_gain_ = db_to_gain(parameters_.bass_db);
    target_mid_gain_ = db_to_gain(parameters_.mid_db);
    target_treble_gain_ = db_to_gain(parameters_.treble_db);
    target_bright_1_gain_ = parameters_.bright_1 ? db_to_gain(kBrightBoostDb)
                                                 : 1.0F;
    target_bright_2_gain_ = parameters_.bright_2 ? db_to_gain(kBrightBoostDb)
                                                 : 1.0F;
    target_level_gain_ = db_to_gain(parameters_.level_db);
    snap_to_targets();
    reset();
}

void MercatorDistortion::set_parameters(
    MercatorDistParameters parameters) noexcept {
    parameters_ = sanitize(parameters);
    smoothing_ms_ = parameters_.smoothing_ms;
    target_drive_gain_ = 1.0F + kInterstageGainRange * parameters_.drive;
    target_drive_2_gain_ = 1.0F + kInterstageGainRange * parameters_.drive_2;
    target_bass_gain_ = db_to_gain(parameters_.bass_db);
    target_mid_gain_ = db_to_gain(parameters_.mid_db);
    target_treble_gain_ = db_to_gain(parameters_.treble_db);
    target_bright_1_gain_ = parameters_.bright_1 ? db_to_gain(kBrightBoostDb)
                                                 : 1.0F;
    target_bright_2_gain_ = parameters_.bright_2 ? db_to_gain(kBrightBoostDb)
                                                 : 1.0F;
    target_level_gain_ = db_to_gain(parameters_.level_db);
    retarget();
}

void MercatorDistortion::process(std::span<float> interleaved,
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

            const float stage_one = soft_clip(input * kStageOneGain);
            state.bright_1_low +=
                bright_alpha_ * (stage_one - state.bright_1_low);
            const float bright_one =
                stage_one + (stage_one - state.bright_1_low) *
                                (bright_1_gain_ - 1.0F);

            const float stage_two = soft_clip(bright_one * drive_gain_);
            state.bright_2_low +=
                bright_alpha_ * (stage_two - state.bright_2_low);
            const float bright_two =
                stage_two + (stage_two - state.bright_2_low) *
                                (bright_2_gain_ - 1.0F);

            const float stage_three = soft_clip(bright_two * drive_2_gain_);
            state.tone_low += tone_low_alpha_ * (stage_three - state.tone_low);
            state.tone_high +=
                tone_high_alpha_ * (stage_three - state.tone_high);

            const float bass_band = state.tone_low;
            const float mid_band = state.tone_high - state.tone_low;
            const float treble_band = stage_three - state.tone_high;
            const float equalized = bass_band * bass_gain_ +
                                    mid_band * mid_gain_ +
                                    treble_band * treble_gain_;
            sample = std::clamp(equalized * level_gain_, -8.0F, 8.0F);
        }
    }
}

void MercatorDistortion::reset() noexcept {
    snap_to_targets();
    channel_state_.fill({});
    ramp_remaining_ = 0U;
}

MercatorDistParameters MercatorDistortion::sanitize(
    MercatorDistParameters parameters) noexcept {
    parameters.drive = std::isfinite(parameters.drive)
                           ? std::clamp(parameters.drive, 0.0F, 1.0F)
                           : 0.45F;
    parameters.drive_2 = std::isfinite(parameters.drive_2)
                             ? std::clamp(parameters.drive_2, 0.0F, 1.0F)
                             : 0.45F;
    parameters.bass_db = std::isfinite(parameters.bass_db)
                             ? std::clamp(parameters.bass_db, -12.0F, 12.0F)
                             : 0.0F;
    parameters.mid_db = std::isfinite(parameters.mid_db)
                            ? std::clamp(parameters.mid_db, -12.0F, 12.0F)
                            : 0.0F;
    parameters.treble_db = std::isfinite(parameters.treble_db)
                               ? std::clamp(parameters.treble_db, -12.0F, 12.0F)
                               : 0.0F;
    parameters.level_db = std::isfinite(parameters.level_db)
                              ? std::clamp(parameters.level_db, -24.0F, 12.0F)
                              : 0.0F;
    parameters.smoothing_ms = std::isfinite(parameters.smoothing_ms)
                                  ? std::clamp(parameters.smoothing_ms, 0.0F,
                                               1000.0F)
                                  : 10.0F;
    return parameters;
}

float MercatorDistortion::db_to_gain(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float MercatorDistortion::soft_clip(float sample) noexcept {
    const float magnitude = std::abs(sample);
    if (magnitude >= 3.0F) return std::copysign(1.0F, sample);
    const float squared = sample * sample;
    return sample * (27.0F + squared) / (27.0F + 9.0F * squared);
}

float MercatorDistortion::low_pass_alpha(float cutoff_hz,
                                         float sample_rate) noexcept {
    const float cutoff = std::clamp(cutoff_hz, 10.0F,
                                    std::min(18000.0F, sample_rate * 0.45F));
    return 1.0F - std::exp(-kTwoPi * cutoff / sample_rate);
}

std::size_t MercatorDistortion::ramp_length() const noexcept {
    const double requested = std::ceil(static_cast<double>(sample_rate_) *
                                       static_cast<double>(smoothing_ms_) /
                                       1000.0);
    return requested >= 1.0 ? static_cast<std::size_t>(requested) : 0U;
}

void MercatorDistortion::retarget() noexcept {
    const std::size_t samples = ramp_length();
    if (samples == 0U) {
        snap_to_targets();
        ramp_remaining_ = 0U;
        return;
    }

    const float divisor = static_cast<float>(samples);
    drive_gain_step_ = (target_drive_gain_ - drive_gain_) / divisor;
    drive_2_gain_step_ = (target_drive_2_gain_ - drive_2_gain_) / divisor;
    bass_gain_step_ = (target_bass_gain_ - bass_gain_) / divisor;
    mid_gain_step_ = (target_mid_gain_ - mid_gain_) / divisor;
    treble_gain_step_ = (target_treble_gain_ - treble_gain_) / divisor;
    bright_1_gain_step_ = (target_bright_1_gain_ - bright_1_gain_) / divisor;
    bright_2_gain_step_ = (target_bright_2_gain_ - bright_2_gain_) / divisor;
    level_gain_step_ = (target_level_gain_ - level_gain_) / divisor;
    ramp_remaining_ = samples;
}

void MercatorDistortion::advance_ramp() noexcept {
    if (ramp_remaining_ == 0U) return;
    drive_gain_ += drive_gain_step_;
    drive_2_gain_ += drive_2_gain_step_;
    bass_gain_ += bass_gain_step_;
    mid_gain_ += mid_gain_step_;
    treble_gain_ += treble_gain_step_;
    bright_1_gain_ += bright_1_gain_step_;
    bright_2_gain_ += bright_2_gain_step_;
    level_gain_ += level_gain_step_;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) snap_to_targets();
}

void MercatorDistortion::snap_to_targets() noexcept {
    drive_gain_ = target_drive_gain_;
    drive_2_gain_ = target_drive_2_gain_;
    bass_gain_ = target_bass_gain_;
    mid_gain_ = target_mid_gain_;
    treble_gain_ = target_treble_gain_;
    bright_1_gain_ = target_bright_1_gain_;
    bright_2_gain_ = target_bright_2_gain_;
    level_gain_ = target_level_gain_;
    drive_gain_step_ = 0.0F;
    drive_2_gain_step_ = 0.0F;
    bass_gain_step_ = 0.0F;
    mid_gain_step_ = 0.0F;
    treble_gain_step_ = 0.0F;
    bright_1_gain_step_ = 0.0F;
    bright_2_gain_step_ = 0.0F;
    level_gain_step_ = 0.0F;
}

} // namespace tabletfx
