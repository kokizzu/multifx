#include "tabletfx/effects/porta_preamp.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kBassShelfHz = 220.0F;
constexpr float kTrebleShelfHz = 3000.0F;

} // namespace

PortaPreamp::PortaPreamp(float sample_rate,
                         PortaPreampParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void PortaPreamp::configure(float sample_rate,
                            PortaPreampParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    parameters_ = sanitize(parameters);
    bass_alpha_ = one_pole_alpha(kBassShelfHz, sample_rate_);
    treble_alpha_ = one_pole_alpha(kTrebleShelfHz, sample_rate_);
    target_gain_1_ = db_to_gain(parameters_.gain_1_db);
    target_gain_2_ = db_to_gain(parameters_.gain_2_db);
    target_bass_gain_ = db_to_gain(parameters_.bass_db);
    target_treble_gain_ = db_to_gain(parameters_.treble_db);
    target_level_gain_ = db_to_gain(parameters_.level_db);
    snap_to_targets();
    reset();
}

void PortaPreamp::set_parameters(PortaPreampParameters parameters) noexcept {
    parameters_ = sanitize(parameters);
    target_gain_1_ = db_to_gain(parameters_.gain_1_db);
    target_gain_2_ = db_to_gain(parameters_.gain_2_db);
    target_bass_gain_ = db_to_gain(parameters_.bass_db);
    target_treble_gain_ = db_to_gain(parameters_.treble_db);
    target_level_gain_ = db_to_gain(parameters_.level_db);
    retarget();
}

void PortaPreamp::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels == 0U || channels > channel_state_.size()) return;

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

            const float input = std::clamp(sample, -16.0F, 16.0F);
            const float stage_1 = soft_clip(input * gain_1_);
            const float stage_2 = soft_clip(stage_1 * gain_2_);

            state.bass_low += bass_alpha_ * (stage_2 - state.bass_low);
            state.treble_low += treble_alpha_ * (stage_2 - state.treble_low);
            const float high = stage_2 - state.treble_low;
            const float shaped = stage_2 +
                state.bass_low * (bass_gain_ - 1.0F) +
                high * (treble_gain_ - 1.0F);
            sample = std::clamp(shaped * level_gain_, -8.0F, 8.0F);
        }
    }
}

void PortaPreamp::reset() noexcept {
    channel_state_.fill({});
    snap_to_targets();
    ramp_remaining_ = 0U;
}

PortaPreampParameters PortaPreamp::sanitize(
    PortaPreampParameters parameters) noexcept {
    const auto bounded = [](float value, float fallback,
                            float minimum, float maximum) noexcept {
        if (!std::isfinite(value)) value = fallback;
        return std::clamp(value, minimum, maximum);
    };
    parameters.gain_1_db = bounded(parameters.gain_1_db, 6.0F, -12.0F, 24.0F);
    parameters.gain_2_db = bounded(parameters.gain_2_db, 3.0F, -12.0F, 24.0F);
    parameters.bass_db = bounded(parameters.bass_db, 0.0F, -12.0F, 12.0F);
    parameters.treble_db = bounded(parameters.treble_db, 0.0F, -12.0F, 12.0F);
    parameters.level_db = bounded(parameters.level_db, 0.0F, -24.0F, 12.0F);
    parameters.smoothing_ms = bounded(parameters.smoothing_ms, 10.0F, 0.0F, 1000.0F);
    return parameters;
}

float PortaPreamp::db_to_gain(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float PortaPreamp::one_pole_alpha(float cutoff_hz,
                                  float sample_rate) noexcept {
    return 1.0F - std::exp(-2.0F * kPi * cutoff_hz / sample_rate);
}

float PortaPreamp::soft_clip(float sample) noexcept {
    const float magnitude = std::abs(sample);
    if (magnitude <= 0.5F) return sample;
    const float clipped = 0.5F + 0.5F * std::tanh(2.0F * (magnitude - 0.5F));
    return std::copysign(clipped, sample);
}

std::size_t PortaPreamp::ramp_length() const noexcept {
    const double samples = std::ceil(
        static_cast<double>(sample_rate_) *
        static_cast<double>(parameters_.smoothing_ms) / 1000.0);
    return samples <= 0.0 ? 0U : static_cast<std::size_t>(samples);
}

void PortaPreamp::retarget() noexcept {
    const std::size_t samples = ramp_length();
    if (samples == 0U) {
        snap_to_targets();
        return;
    }

    const float divisor = static_cast<float>(samples);
    gain_1_step_ = (target_gain_1_ - gain_1_) / divisor;
    gain_2_step_ = (target_gain_2_ - gain_2_) / divisor;
    bass_gain_step_ = (target_bass_gain_ - bass_gain_) / divisor;
    treble_gain_step_ = (target_treble_gain_ - treble_gain_) / divisor;
    level_gain_step_ = (target_level_gain_ - level_gain_) / divisor;
    ramp_remaining_ = samples;
}

void PortaPreamp::advance_ramp() noexcept {
    if (ramp_remaining_ == 0U) return;
    gain_1_ += gain_1_step_;
    gain_2_ += gain_2_step_;
    bass_gain_ += bass_gain_step_;
    treble_gain_ += treble_gain_step_;
    level_gain_ += level_gain_step_;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) snap_to_targets();
}

void PortaPreamp::snap_to_targets() noexcept {
    gain_1_ = target_gain_1_;
    gain_1_step_ = 0.0F;
    gain_2_ = target_gain_2_;
    gain_2_step_ = 0.0F;
    bass_gain_ = target_bass_gain_;
    bass_gain_step_ = 0.0F;
    treble_gain_ = target_treble_gain_;
    treble_gain_step_ = 0.0F;
    level_gain_ = target_level_gain_;
    level_gain_step_ = 0.0F;
    ramp_remaining_ = 0U;
}

} // namespace tabletfx
