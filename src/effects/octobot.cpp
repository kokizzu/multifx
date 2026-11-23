#include "tabletfx/effects/octobot.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {
namespace {

constexpr float kMaximumInputMagnitude = 1000000.0F;

}  // namespace

Octobot::Octobot(float sample_rate, std::uint16_t channels,
                 OctobotParameters parameters)
    : sample_rate_(sample_rate),
      channels_(channels),
      down_shifter_(sample_rate, channels),
      up_shifter_(sample_rate, channels),
      alignment_delay_(kAlignmentDelayFrames * channels, 0.0F),
      direct_buffer_(kMaximumChunkFrames * channels, 0.0F),
      down_buffer_(kMaximumChunkFrames * channels, 0.0F),
      sizzle_buffer_(kMaximumChunkFrames * channels, 0.0F) {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        throw std::invalid_argument("Octobot sample rate is unsupported");
    }
    if (channels == 0U || channels > 2U) {
        throw std::invalid_argument("Octobot supports mono or stereo");
    }
    down_shifter_.set_pitch_semitones(-12.0F);
    up_shifter_.set_pitch_semitones(12.0F);
    configure(parameters);
    // Constructor values take effect immediately; later edits use a short ramp.
    direct_ = target_direct_;
    down_ = target_down_;
    sizzle_ = target_sizzle_;
    parameter_ramp_remaining_ = 0U;
}

std::size_t Octobot::memory_bytes() const noexcept {
    return sizeof(*this) +
           (alignment_delay_.capacity() + direct_buffer_.capacity() +
            down_buffer_.capacity() + sizzle_buffer_.capacity()) * sizeof(float) +
           down_shifter_.memory_bytes() - sizeof(down_shifter_) +
           up_shifter_.memory_bytes() - sizeof(up_shifter_);
}

float Octobot::algorithmic_latency_ms() const noexcept {
    return down_shifter_.algorithmic_latency_ms();
}

float Octobot::bounded(float value, float fallback,
                       float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

float Octobot::finite_input(float sample) noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return std::clamp(sample, -kMaximumInputMagnitude,
                      kMaximumInputMagnitude);
}

void Octobot::configure(OctobotParameters requested) noexcept {
    parameters_.direct = bounded(requested.direct, 0.65F, 0.0F, 1.0F);
    parameters_.down = bounded(requested.down, 0.20F, 0.0F, 1.0F);
    parameters_.sizzle = bounded(requested.sizzle, 0.25F, 0.0F, 1.0F);

    target_direct_ = parameters_.direct;
    target_down_ = parameters_.down;
    target_sizzle_ = parameters_.sizzle;
    parameter_ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(
                static_cast<double>(sample_rate_) *
                static_cast<double>(kParameterSmoothingMs) / 1000.0)));
    parameter_ramp_remaining_ = parameter_ramp_total_;
    direct_step_ = (target_direct_ - direct_) /
                   static_cast<float>(parameter_ramp_total_);
    down_step_ = (target_down_ - down_) /
                 static_cast<float>(parameter_ramp_total_);
    sizzle_step_ = (target_sizzle_ - sizzle_) /
                   static_cast<float>(parameter_ramp_total_);
}

void Octobot::advance_parameters() noexcept {
    if (parameter_ramp_remaining_ == 0U) {
        return;
    }
    direct_ += direct_step_;
    down_ += down_step_;
    sizzle_ += sizzle_step_;
    --parameter_ramp_remaining_;
    if (parameter_ramp_remaining_ == 0U) {
        direct_ = target_direct_;
        down_ = target_down_;
        sizzle_ = target_sizzle_;
    }
}

void Octobot::reset() noexcept {
    down_shifter_.reset();
    up_shifter_.reset();
    std::fill(alignment_delay_.begin(), alignment_delay_.end(), 0.0F);
    std::fill(direct_buffer_.begin(), direct_buffer_.end(), 0.0F);
    std::fill(down_buffer_.begin(), down_buffer_.end(), 0.0F);
    std::fill(sizzle_buffer_.begin(), sizzle_buffer_.end(), 0.0F);
    alignment_write_frame_ = 0U;
    direct_ = target_direct_;
    down_ = target_down_;
    sizzle_ = target_sizzle_;
    parameter_ramp_remaining_ = 0U;
}

void Octobot::process(std::span<float> interleaved,
                      std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U || channels > 2U) {
        return;
    }

    const std::size_t total_frames = interleaved.size() / channels;
    std::size_t frame_offset = 0U;
    while (frame_offset < total_frames) {
        const std::size_t chunk_frames = std::min(
            kMaximumChunkFrames, total_frames - frame_offset);
        const std::size_t chunk_samples = chunk_frames * channels;
        const std::size_t sample_offset = frame_offset * channels;

        for (std::size_t frame = 0U; frame < chunk_frames; ++frame) {
            const std::size_t delayed_frame = alignment_write_frame_;
            for (std::uint16_t channel = 0U; channel < channels; ++channel) {
                const std::size_t sample_index = frame * channels + channel;
                const float input = finite_input(
                    interleaved[sample_offset + sample_index]);
                const std::size_t delay_index = delayed_frame * channels + channel;
                direct_buffer_[sample_index] = alignment_delay_[delay_index];
                alignment_delay_[delay_index] = input;
                down_buffer_[sample_index] = input;
                sizzle_buffer_[sample_index] = input;
            }
            alignment_write_frame_ =
                (alignment_write_frame_ + 1U) % kAlignmentDelayFrames;
        }

        down_shifter_.process(
            std::span<float>(down_buffer_.data(), chunk_samples), channels);
        up_shifter_.process(
            std::span<float>(sizzle_buffer_.data(), chunk_samples), channels);

        for (std::size_t frame = 0U; frame < chunk_frames; ++frame) {
            advance_parameters();
            for (std::uint16_t channel = 0U; channel < channels; ++channel) {
                const std::size_t sample_index = frame * channels + channel;
                const float down_sample = finite_input(down_buffer_[sample_index]);
                const float up_sample = finite_input(sizzle_buffer_[sample_index]);
                const float sizzle_sample = std::tanh(kSizzleDrive * up_sample);
                const float output = direct_buffer_[sample_index] * direct_ +
                                     down_sample * down_ +
                                     sizzle_sample * sizzle_;
                interleaved[sample_offset + sample_index] =
                    std::isfinite(output) ? output : 0.0F;
            }
        }
        frame_offset += chunk_frames;
    }
}

}  // namespace tabletfx::effects
