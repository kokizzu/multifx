#include "tabletfx/effects/shimmer_reverb.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kMaximumInternalSample = 16.0F;

float checked_sample_rate(float sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate < 8000.0F ||
        sample_rate > 384000.0F) {
        throw std::invalid_argument(
            "ShimmerReverb sample rate must be between 8 kHz and 384 kHz");
    }
    return sample_rate;
}

std::uint16_t checked_channels(std::uint16_t channels) {
    if (channels != 1 && channels != 2) {
        throw std::invalid_argument("ShimmerReverb supports mono or stereo only");
    }
    return channels;
}

float finite_sample(float value) noexcept {
    return std::isfinite(value)
        ? std::clamp(value, -kMaximumInternalSample, kMaximumInternalSample)
        : 0.0F;
}

} // namespace

ShimmerReverb::ShimmerReverb(float sample_rate, std::uint16_t channels,
                             ShimmerReverbParameters parameters)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)), reverb_(sample_rate_, channels_),
      pitch_shifter_(sample_rate_, channels_) {
    const double ramp_samples = static_cast<double>(sample_rate_) *
                                static_cast<double>(kControlSmoothingMs) / 1000.0;
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(ramp_samples)));
    configure(parameters);
    reset();
}

std::size_t ShimmerReverb::memory_bytes() const noexcept {
    return sizeof(*this) + reverb_.memory_bytes() - sizeof(reverb_) +
           pitch_shifter_.memory_bytes() - sizeof(pitch_shifter_);
}

void ShimmerReverb::configure(ShimmerReverbParameters parameters) noexcept {
    const ShimmerReverbParameters defaults{};
    const float maximum_damping_hz = std::min(18000.0F, sample_rate_ * 0.45F);
    const float maximum_high_pass_hz = std::min(1500.0F, sample_rate_ * 0.2F);
    parameters.decay_seconds = bounded(parameters.decay_seconds,
                                       defaults.decay_seconds, 0.2F, 12.0F);
    parameters.damping_hz = bounded(parameters.damping_hz,
                                    defaults.damping_hz, 200.0F,
                                    maximum_damping_hz);
    parameters.diffusion = bounded(parameters.diffusion,
                                   defaults.diffusion, 0.0F, 1.0F);
    parameters.feedback = bounded(parameters.feedback,
                                   defaults.feedback, 0.0F,
                                   kMaximumFeedback);
    parameters.high_pass_hz = bounded(parameters.high_pass_hz,
                                       defaults.high_pass_hz, 20.0F,
                                       maximum_high_pass_hz);
    parameters.pitch_semitones = bounded(parameters.pitch_semitones,
                                         defaults.pitch_semitones, 1.0F,
                                         12.0F);
    parameters.shimmer_intensity = bounded(parameters.shimmer_intensity,
                                           defaults.shimmer_intensity, 0.0F,
                                           1.0F);
    parameters.mix = bounded(parameters.mix, defaults.mix, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, defaults.level_db,
                                  -24.0F, 12.0F);

    const float previous_feedback = feedback_;
    const float previous_high_pass_alpha = high_pass_alpha_;
    const float previous_intensity = shimmer_intensity_;
    const float previous_mix = mix_;
    const float previous_level = level_;

    parameters_ = parameters;

    PlateReverbParameters reverb_parameters;
    reverb_parameters.decay_seconds = parameters_.decay_seconds;
    reverb_parameters.pre_delay_ms = 0.0F;
    reverb_parameters.damping_hz = parameters_.damping_hz;
    reverb_parameters.diffusion = parameters_.diffusion;
    reverb_parameters.balance = 1.0F;
    reverb_parameters.mix = 1.0F;
    reverb_parameters.level_db = 0.0F;
    reverb_.configure(reverb_parameters);
    pitch_shifter_.set_pitch_semitones(parameters_.pitch_semitones);

    const float target_feedback = parameters_.feedback;
    target_high_pass_alpha_ = std::exp(
        -6.28318530718F * parameters_.high_pass_hz / sample_rate_);
    const float target_intensity = parameters_.shimmer_intensity;
    const float target_mix = parameters_.mix;
    target_level_ = std::pow(10.0F, parameters_.level_db / 20.0F);

    const float ramp = static_cast<float>(ramp_total_);
    feedback_step_ = (target_feedback - previous_feedback) / ramp;
    high_pass_step_ = (target_high_pass_alpha_ - previous_high_pass_alpha) / ramp;
    shimmer_intensity_step_ = (target_intensity - previous_intensity) / ramp;
    mix_step_ = (target_mix - previous_mix) / ramp;
    level_step_ = (target_level_ - previous_level) / ramp;
    ramp_remaining_ = ramp_total_;
}

void ShimmerReverb::process(std::span<float> interleaved,
                            std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    std::size_t block_offset = 0;
    while (block_offset < frames) {
        const std::size_t block_frames = std::min(
            kMaxProcessFrames, frames - block_offset);
        const std::size_t block_samples = block_frames * channels_;

        for (std::size_t frame = 0; frame < block_frames; ++frame) {
            advance_controls();
            high_pass_alpha_buffer_[frame] = high_pass_alpha_;
            shimmer_intensity_buffer_[frame] = shimmer_intensity_;
            mix_buffer_[frame] = mix_;
            level_buffer_[frame] = level_;
            const std::size_t source_offset = (block_offset + frame) * channels_;
            const std::size_t scratch_offset = frame * channels_;
            const std::size_t ring_frame =
                (feedback_frame_ + frame) % kFeedbackDelayFrames;
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                const std::size_t source_index = source_offset + channel;
                const std::size_t scratch_index = scratch_offset + channel;
                const float dry = std::isfinite(interleaved[source_index])
                    ? std::clamp(interleaved[source_index], -8.0F, 8.0F)
                    : 0.0F;
                const std::size_t ring_index = ring_frame * channels_ + channel;
                dry_buffer_[scratch_index] = dry;
                wet_buffer_[scratch_index] = std::clamp(
                    dry + feedback_ring_[ring_index] * feedback_, -8.0F, 8.0F);
            }
        }

        reverb_.process(std::span<float>(wet_buffer_.data(), block_samples),
                        channels_);
        std::copy_n(wet_buffer_.begin(), block_samples, pitched_buffer_.begin());
        pitch_shifter_.process(
            std::span<float>(pitched_buffer_.data(), block_samples), channels_);

        for (std::size_t frame = 0; frame < block_frames; ++frame) {
            const std::size_t output_offset = (block_offset + frame) * channels_;
            const std::size_t scratch_offset = frame * channels_;
            const std::size_t ring_frame =
                (feedback_frame_ + frame) % kFeedbackDelayFrames;
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                const std::size_t index = scratch_offset + channel;
                const float wet_sample = finite_sample(wet_buffer_[index]);
                const float pitched_sample = finite_sample(pitched_buffer_[index]);
                const float high_pass = high_pass_alpha_buffer_[frame] *
                    (feedback_output_previous_[channel] + pitched_sample -
                     feedback_input_previous_[channel]);
                feedback_input_previous_[channel] = pitched_sample;
                feedback_output_previous_[channel] = finite_sample(high_pass);
                feedback_ring_[ring_frame * channels_ + channel] =
                    feedback_output_previous_[channel];
                const float blended_wet =
                    wet_sample * (1.0F - shimmer_intensity_buffer_[frame]) +
                    pitched_sample * shimmer_intensity_buffer_[frame];
                const float output = dry_buffer_[index] * (1.0F - mix_buffer_[frame]) +
                                     blended_wet * mix_buffer_[frame] * level_buffer_[frame];
                interleaved[output_offset + channel] = finite_sample(output);
            }
        }
        feedback_frame_ = (feedback_frame_ + block_frames) % kFeedbackDelayFrames;
        block_offset += block_frames;
    }
}

void ShimmerReverb::reset() noexcept {
    reverb_.reset();
    pitch_shifter_.reset();
    feedback_ring_.fill(0.0F);
    feedback_frame_ = 0;
    feedback_input_previous_.fill(0.0F);
    feedback_output_previous_.fill(0.0F);
    feedback_ = parameters_.feedback;
    high_pass_alpha_ = target_high_pass_alpha_;
    shimmer_intensity_ = parameters_.shimmer_intensity;
    mix_ = parameters_.mix;
    level_ = target_level_;
    feedback_step_ = 0.0F;
    high_pass_step_ = 0.0F;
    shimmer_intensity_step_ = 0.0F;
    mix_step_ = 0.0F;
    level_step_ = 0.0F;
    ramp_remaining_ = 0;
}

float ShimmerReverb::bounded(float value, float fallback,
                             float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

void ShimmerReverb::advance_controls() noexcept {
    if (ramp_remaining_ == 0U) return;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) {
        feedback_ = parameters_.feedback;
        high_pass_alpha_ = target_high_pass_alpha_;
        shimmer_intensity_ = parameters_.shimmer_intensity;
        mix_ = parameters_.mix;
        level_ = target_level_;
        feedback_step_ = 0.0F;
        high_pass_step_ = 0.0F;
        shimmer_intensity_step_ = 0.0F;
        mix_step_ = 0.0F;
        level_step_ = 0.0F;
        return;
    }
    feedback_ += feedback_step_;
    high_pass_alpha_ += high_pass_step_;
    shimmer_intensity_ += shimmer_intensity_step_;
    mix_ += mix_step_;
    level_ += level_step_;
}

} // namespace tabletfx
