#include "tabletfx/effects/celestial_reverb.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kTwoPi = 6.28318530718F;
constexpr float kThreeDbDecay = 0.001F;
constexpr float kInvSqrtTwo = 0.70710678118F;

float checked_sample_rate(float sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate < 8000.0F ||
        sample_rate > 384000.0F) {
        throw std::invalid_argument(
            "CelestialReverb sample rate must be between 8 kHz and 384 kHz");
    }
    return sample_rate;
}

std::uint16_t checked_channels(std::uint16_t channels) {
    if (channels != 1 && channels != 2) {
        throw std::invalid_argument(
            "CelestialReverb supports mono or stereo only");
    }
    return channels;
}

float finite_sample(float value) noexcept {
    return std::isfinite(value)
        ? std::clamp(value, -16.0F, 16.0F)
        : 0.0F;
}

} // namespace

CelestialReverb::CelestialReverb(float sample_rate, std::uint16_t channels,
                                 CelestialReverbParameters parameters)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)),
      reverb_(sample_rate_, channels_),
      pitch_1_(sample_rate_, channels_),
      pitch_2_(sample_rate_, channels_) {
    const double ramp_samples = static_cast<double>(sample_rate_) *
        static_cast<double>(kControlSmoothingMs) / 1000.0;
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(ramp_samples)));
    configure(parameters);
    reset();
}

std::size_t CelestialReverb::memory_bytes() const noexcept {
    return sizeof(*this) + reverb_.memory_bytes() - sizeof(reverb_) +
           pitch_1_.memory_bytes() - sizeof(pitch_1_) +
           pitch_2_.memory_bytes() - sizeof(pitch_2_);
}

void CelestialReverb::configure(CelestialReverbParameters parameters) noexcept {
    const CelestialReverbParameters defaults{};
    const float maximum_cut_hz = std::min(20000.0F, sample_rate_ * 0.45F);
    parameters.mix = bounded(parameters.mix, defaults.mix, 0.0F, 1.0F);
    parameters.decay_seconds = bounded(parameters.decay_seconds,
                                       defaults.decay_seconds, 0.2F, 12.0F);
    parameters.pitch_1_semitones = bounded(parameters.pitch_1_semitones,
                                           defaults.pitch_1_semitones,
                                           -12.0F, 12.0F);
    parameters.pitch_2_semitones = bounded(parameters.pitch_2_semitones,
                                           defaults.pitch_2_semitones,
                                           -12.0F, 12.0F);
    parameters.pitch_decay_seconds = bounded(parameters.pitch_decay_seconds,
                                             defaults.pitch_decay_seconds,
                                             0.2F, 12.0F);
    parameters.high_cut_hz = bounded(parameters.high_cut_hz,
                                     defaults.high_cut_hz, 200.0F,
                                     maximum_cut_hz);
    parameters.blend = bounded(parameters.blend, defaults.blend, 0.0F, 1.0F);

    const float previous_mix = mix_;
    const float previous_blend = blend_;
    const float previous_alpha = high_cut_alpha_;
    parameters_ = parameters;

    PlateReverbParameters plate_parameters;
    plate_parameters.decay_seconds = parameters_.decay_seconds;
    plate_parameters.pre_delay_ms = 0.0F;
    plate_parameters.damping_hz = maximum_cut_hz;
    plate_parameters.diffusion = 0.85F;
    plate_parameters.balance = 1.0F;
    plate_parameters.mix = 1.0F;
    plate_parameters.level_db = 0.0F;
    reverb_.configure(plate_parameters);
    pitch_1_.set_pitch_semitones(parameters_.pitch_1_semitones);
    pitch_2_.set_pitch_semitones(parameters_.pitch_2_semitones);

    // A 60 dB return decay over the selected Pitch Decay time is applied at
    // each 128-frame return. This is a project mapping, not Fender's equation.
    const float round_trip = static_cast<float>(kFeedbackDelayFrames) /
                             (sample_rate_ * parameters_.pitch_decay_seconds);
    feedback_gain_ = std::pow(kThreeDbDecay, round_trip);
    target_high_cut_alpha_ = std::exp(
        -kTwoPi * parameters_.high_cut_hz / sample_rate_);

    const float ramp = static_cast<float>(ramp_total_);
    mix_step_ = (parameters_.mix - previous_mix) / ramp;
    blend_step_ = (parameters_.blend - previous_blend) / ramp;
    high_cut_alpha_step_ =
        (target_high_cut_alpha_ - previous_alpha) / ramp;
    ramp_remaining_ = ramp_total_;
}

void CelestialReverb::process(std::span<float> interleaved,
                              std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;
    bypass_cleared_ = false;

    const std::size_t frames = interleaved.size() / channels_;
    std::size_t offset = 0;
    while (offset < frames) {
        const std::size_t block_frames = std::min(
            kMaxProcessFrames, frames - offset);
        const std::size_t block_samples = block_frames * channels_;
        process_block(interleaved.subspan(offset * channels_, block_samples),
                      block_frames, false);
        offset += block_frames;
    }
}

void CelestialReverb::process_bypassed(std::span<float> interleaved,
                                       std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;
    if (!parameters_.spillover) {
        if (!bypass_cleared_) clear_tail_state();
        bypass_cleared_ = true;
        return;
    }
    bypass_cleared_ = false;

    const std::size_t frames = interleaved.size() / channels_;
    std::size_t offset = 0;
    while (offset < frames) {
        const std::size_t block_frames = std::min(
            kMaxProcessFrames, frames - offset);
        const std::size_t block_samples = block_frames * channels_;
        process_block(interleaved.subspan(offset * channels_, block_samples),
                      block_frames, true);
        offset += block_frames;
    }
}

void CelestialReverb::process_block(std::span<float> interleaved,
                                    std::size_t frames,
                                    bool bypassed) noexcept {
    const std::size_t samples = frames * channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t scratch = frame * channels_;
        const std::size_t ring_frame = (feedback_frame_ + frame) %
                                       kFeedbackDelayFrames;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = scratch + channel;
            const float input = finite_sample(interleaved[index]);
            dry_buffer_[index] = input;
            const float return_1 = feedback_ring_[ring_frame * 2U + channel];
            const float return_2 = feedback_ring_[
                (kFeedbackDelayFrames + ring_frame) * 2U + channel];
            const float reverb_input = bypassed
                ? return_1 + return_2
                : input + return_1 + return_2;
            wet_buffer_[index] = std::clamp(reverb_input, -8.0F, 8.0F);
        }
    }

    reverb_.process(std::span<float>(wet_buffer_.data(), samples), channels_);
    std::copy_n(wet_buffer_.begin(), samples, pitch_1_buffer_.begin());
    std::copy_n(wet_buffer_.begin(), samples, pitch_2_buffer_.begin());
    pitch_1_.process(std::span<float>(pitch_1_buffer_.data(), samples), channels_);
    pitch_2_.process(std::span<float>(pitch_2_buffer_.data(), samples), channels_);

    for (std::size_t frame = 0; frame < frames; ++frame) {
        advance_controls();
        const std::size_t scratch = frame * channels_;
        const std::size_t ring_frame = (feedback_frame_ + frame) %
                                       kFeedbackDelayFrames;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = scratch + channel;
            const float unshifted = finite_sample(wet_buffer_[index]);
            const float shifted_1 = finite_sample(pitch_1_buffer_[index]);
            const float shifted_2 = finite_sample(pitch_2_buffer_[index]);
            const std::size_t channel_slot = channel;

            float& unshifted_state = high_cut_state_[channel_slot];
            float& pitch_1_state = high_cut_state_[2U + channel_slot];
            float& pitch_2_state = high_cut_state_[4U + channel_slot];
            const float high_cut_gain = 1.0F - high_cut_alpha_;
            unshifted_state = finite_sample(
                high_cut_gain * unshifted + high_cut_alpha_ * unshifted_state);
            pitch_1_state = finite_sample(
                high_cut_gain * shifted_1 + high_cut_alpha_ * pitch_1_state);
            pitch_2_state = finite_sample(
                high_cut_gain * shifted_2 + high_cut_alpha_ * pitch_2_state);

            feedback_ring_[ring_frame * 2U + channel] =
                pitch_1_state * feedback_gain_;
            feedback_ring_[(kFeedbackDelayFrames + ring_frame) * 2U + channel] =
                pitch_2_state * feedback_gain_;

            const float pitched =
                (pitch_1_state + pitch_2_state) * kInvSqrtTwo;
            const float wet = unshifted_state * (1.0F - blend_) +
                              pitched * blend_;
            const float output = bypassed
                ? dry_buffer_[index] + wet * mix_
                : dry_buffer_[index] * (1.0F - mix_) + wet * mix_;
            interleaved[index] = finite_sample(output);
        }
    }
    feedback_frame_ = (feedback_frame_ + frames) % kFeedbackDelayFrames;
}

void CelestialReverb::clear_tail_state() noexcept {
    reverb_.reset();
    pitch_1_.reset();
    pitch_2_.reset();
    feedback_ring_.fill(0.0F);
    high_cut_state_.fill(0.0F);
    feedback_frame_ = 0;
}

void CelestialReverb::reset() noexcept {
    clear_tail_state();
    high_cut_alpha_ = target_high_cut_alpha_;
    mix_ = parameters_.mix;
    blend_ = parameters_.blend;
    high_cut_alpha_step_ = 0.0F;
    mix_step_ = 0.0F;
    blend_step_ = 0.0F;
    ramp_remaining_ = 0;
    bypass_cleared_ = false;
}

float CelestialReverb::bounded(float value, float fallback,
                               float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

void CelestialReverb::advance_controls() noexcept {
    if (ramp_remaining_ == 0U) return;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) {
        mix_ = parameters_.mix;
        blend_ = parameters_.blend;
        high_cut_alpha_ = target_high_cut_alpha_;
        mix_step_ = 0.0F;
        blend_step_ = 0.0F;
        high_cut_alpha_step_ = 0.0F;
        return;
    }
    mix_ += mix_step_;
    blend_ += blend_step_;
    high_cut_alpha_ += high_cut_alpha_step_;
}

} // namespace tabletfx
