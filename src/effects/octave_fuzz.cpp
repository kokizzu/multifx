#include "tabletfx/effects/octave_fuzz.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {

OctaveFuzz::OctaveFuzz(float sample_rate, std::uint16_t channels,
                       OctaveFuzzParameters parameters)
    : sample_rate_(sample_rate),
      channels_(channels),
      octave_shifter_(sample_rate, channels),
      alignment_delay_(kAlignmentDelayFrames * channels, 0.0F),
      octave_buffer_(kMaximumChunkFrames * channels, 0.0F),
      base_buffer_(kMaximumChunkFrames * channels, 0.0F) {
    if (!std::isfinite(sample_rate) || sample_rate < 8000.0F ||
        sample_rate > 384000.0F) {
        throw std::invalid_argument("Octave Fuzz sample rate is unsupported");
    }
    if (channels == 0U || channels > 2U) {
        throw std::invalid_argument("Octave Fuzz supports mono or stereo");
    }
    octave_shifter_.set_pitch_semitones(12.0F);
    configure(parameters);
    boost_ = target_boost_;
    volume_ = target_volume_;
    octave_mix_ = target_octave_mix_;
    parameter_ramp_remaining_ = 0U;
}

std::size_t OctaveFuzz::memory_bytes() const noexcept {
    return sizeof(*this) + alignment_delay_.capacity() * sizeof(float) +
           octave_buffer_.capacity() * sizeof(float) +
           base_buffer_.capacity() * sizeof(float) +
           octave_shifter_.memory_bytes() - sizeof(octave_shifter_);
}

float OctaveFuzz::algorithmic_latency_ms() const noexcept {
    return octave_shifter_.algorithmic_latency_ms();
}

float OctaveFuzz::bounded(float value, float fallback, float minimum,
                          float maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

float OctaveFuzz::finite_input(float sample) noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return std::clamp(sample, -kMaximumInputMagnitude,
                      kMaximumInputMagnitude);
}

void OctaveFuzz::configure(OctaveFuzzParameters requested) noexcept {
    parameters_.boost = bounded(requested.boost, 0.55F, 0.0F, 1.0F);
    parameters_.volume = bounded(requested.volume, 0.75F, 0.0F, 2.0F);
    parameters_.octave_switch = requested.octave_switch;

    target_boost_ = parameters_.boost;
    target_volume_ = parameters_.volume;
    target_octave_mix_ = parameters_.octave_switch ? 0.35F : 0.0F;
    parameter_ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(
                static_cast<double>(sample_rate_) *
                static_cast<double>(kParameterSmoothingMs) / 1000.0)));
    parameter_ramp_remaining_ = parameter_ramp_total_;
    boost_step_ = (target_boost_ - boost_) /
                  static_cast<float>(parameter_ramp_total_);
    volume_step_ = (target_volume_ - volume_) /
                   static_cast<float>(parameter_ramp_total_);
    octave_mix_step_ = (target_octave_mix_ - octave_mix_) /
                       static_cast<float>(parameter_ramp_total_);
}

void OctaveFuzz::advance_parameters() noexcept {
    if (parameter_ramp_remaining_ == 0U) {
        return;
    }
    boost_ += boost_step_;
    volume_ += volume_step_;
    octave_mix_ += octave_mix_step_;
    --parameter_ramp_remaining_;
    if (parameter_ramp_remaining_ == 0U) {
        boost_ = target_boost_;
        volume_ = target_volume_;
        octave_mix_ = target_octave_mix_;
    }
}

void OctaveFuzz::reset() noexcept {
    octave_shifter_.reset();
    std::fill(alignment_delay_.begin(), alignment_delay_.end(), 0.0F);
    std::fill(octave_buffer_.begin(), octave_buffer_.end(), 0.0F);
    std::fill(base_buffer_.begin(), base_buffer_.end(), 0.0F);
    alignment_write_frame_ = 0U;
    boost_ = target_boost_;
    volume_ = target_volume_;
    octave_mix_ = target_octave_mix_;
    parameter_ramp_remaining_ = 0U;
}

void OctaveFuzz::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U ||
        interleaved.size() % channels != 0U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    std::size_t frame_offset = 0U;
    while (frame_offset < frames) {
        const std::size_t chunk_frames = std::min(
            kMaximumChunkFrames, frames - frame_offset);
        const std::size_t chunk_samples = chunk_frames * channels_;

        for (std::size_t frame = 0U; frame < chunk_frames; ++frame) {
            advance_parameters();
            const float drive = 1.0F + 39.0F * boost_ * boost_;
            const std::size_t ring_frame = alignment_write_frame_;
            for (std::size_t channel = 0U; channel < channels_; ++channel) {
                const std::size_t sample_index = frame * channels_ + channel;
                const std::size_t source_index =
                    (frame_offset + frame) * channels_ + channel;
                const float input = finite_input(interleaved[source_index]);
                const float driven = input * drive;
                const float base = std::tanh(driven * 2.0F);
                const std::size_t delay_index = ring_frame * channels_ + channel;
                base_buffer_[sample_index] = alignment_delay_[delay_index];
                alignment_delay_[delay_index] = base;
                octave_buffer_[sample_index] = driven;
            }
            alignment_write_frame_ =
                (alignment_write_frame_ + 1U) % kAlignmentDelayFrames;
        }

        octave_shifter_.process(
            std::span<float>(octave_buffer_.data(), chunk_samples), channels_);
        for (std::size_t sample = 0U; sample < chunk_samples; ++sample) {
            const float upper = std::tanh(octave_buffer_[sample] * 6.0F);
            const float mixed = base_buffer_[sample] * (1.0F - octave_mix_) +
                                upper * octave_mix_;
            const float output = mixed * volume_;
            const std::size_t destination =
                frame_offset * channels_ + sample;
            interleaved[destination] =
                std::isfinite(output) ? output : 0.0F;
        }
        frame_offset += chunk_frames;
    }
}

}  // namespace tabletfx::effects
