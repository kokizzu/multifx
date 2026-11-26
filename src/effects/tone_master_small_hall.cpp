#include "tabletfx/effects/tone_master_small_hall.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kTwoPi = 6.28318530718F;

float finite_or(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}

float gain_from_db(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

CloudReverbParameters make_internal_parameters(
    float sample_rate, ToneMasterSmallHallParameters parameters) noexcept {
    const ToneMasterSmallHallParameters defaults{};
    CloudReverbParameters internal;
    internal.mix = 1.0F;
    internal.decay_seconds = std::clamp(
        finite_or(parameters.decay_seconds, defaults.decay_seconds),
        0.25F, 10.0F);
    internal.diffusion = std::clamp(
        finite_or(parameters.diffuse, defaults.diffuse), 0.0F, 1.0F);
    internal.high_damp_hz = std::min(20000.0F, sample_rate * 0.45F);
    internal.low_damp_hz = 20.0F;
    internal.modulation_rate_hz = std::clamp(
        finite_or(parameters.rate_hz, defaults.rate_hz), 0.05F, 6.0F);
    internal.modulation_depth = std::clamp(
        finite_or(parameters.depth, defaults.depth), 0.0F, 1.0F);
    internal.spillover = parameters.spillover;
    return internal;
}

} // namespace

ToneMasterSmallHallReverb::ToneMasterSmallHallReverb(
    float sample_rate, std::uint16_t channels,
    ToneMasterSmallHallParameters parameters)
    : sample_rate_(sample_rate), channels_(channels),
      reverb_(sample_rate, channels,
              make_internal_parameters(sample_rate, parameters)) {
    const float low_frequency =
        std::min(kLowShelfFrequencyHz, sample_rate_ * 0.4F);
    const float high_frequency =
        std::min(kHighShelfFrequencyHz, sample_rate_ * 0.4F);
    low_shelf_alpha_ = std::exp(-kTwoPi * low_frequency / sample_rate_);
    high_shelf_alpha_ = std::exp(-kTwoPi * high_frequency / sample_rate_);
    const double ramp_samples = static_cast<double>(sample_rate_) *
        static_cast<double>(kControlSmoothingMs) / 1000.0;
    input_ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(ramp_samples)));
    output_ramp_total_ = input_ramp_total_;
    configure(parameters);

    // A newly constructed preset begins at its stored values, without an
    // unnecessary control ramp from the struct's defaults.
    dwell_gain_ = dwell_gain_target_;
    level_ = level_target_;
    low_shelf_gain_ = low_shelf_gain_target_;
    high_shelf_gain_ = high_shelf_gain_target_;
    input_ramp_remaining_ = 0U;
    output_ramp_remaining_ = 0U;
}

std::size_t ToneMasterSmallHallReverb::memory_bytes() const noexcept {
    return sizeof(*this) + reverb_.memory_bytes() - sizeof(reverb_);
}

void ToneMasterSmallHallReverb::configure(
    ToneMasterSmallHallParameters parameters) noexcept {
    const ToneMasterSmallHallParameters defaults{};
    parameters.level = std::clamp(
        finite_or(parameters.level, defaults.level), 0.0F, 1.0F);
    parameters.decay_seconds = std::clamp(
        finite_or(parameters.decay_seconds, defaults.decay_seconds),
        0.25F, 10.0F);
    parameters.dwell_db = std::clamp(
        finite_or(parameters.dwell_db, defaults.dwell_db), -12.0F, 12.0F);
    parameters.diffuse = std::clamp(
        finite_or(parameters.diffuse, defaults.diffuse), 0.0F, 1.0F);
    parameters.rate_hz = std::clamp(
        finite_or(parameters.rate_hz, defaults.rate_hz), 0.05F, 6.0F);
    parameters.depth = std::clamp(
        finite_or(parameters.depth, defaults.depth), 0.0F, 1.0F);
    parameters.high_cut_gain_db = std::clamp(
        finite_or(parameters.high_cut_gain_db,
                  defaults.high_cut_gain_db), -12.0F, 12.0F);
    parameters.low_cut_gain_db = std::clamp(
        finite_or(parameters.low_cut_gain_db,
                  defaults.low_cut_gain_db), -12.0F, 12.0F);
    parameters_ = parameters;

    CloudReverbParameters internal;
    internal.mix = 1.0F;
    internal.decay_seconds = parameters_.decay_seconds;
    internal.diffusion = parameters_.diffuse;
    // Keep the feedback network's damping nearly neutral. The named High/Low
    // Cut controls are implemented separately as output shelf gains below.
    internal.high_damp_hz = std::min(20000.0F, sample_rate_ * 0.45F);
    internal.low_damp_hz = 20.0F;
    internal.modulation_rate_hz = parameters_.rate_hz;
    internal.modulation_depth = parameters_.depth;
    internal.spillover = parameters_.spillover;
    reverb_.configure(internal);

    dwell_gain_target_ = gain_from_db(parameters_.dwell_db);
    dwell_gain_step_ = (dwell_gain_target_ - dwell_gain_) /
        static_cast<float>(input_ramp_total_);
    input_ramp_remaining_ = input_ramp_total_;

    level_target_ = parameters_.level;
    low_shelf_gain_target_ = gain_from_db(parameters_.low_cut_gain_db);
    high_shelf_gain_target_ = gain_from_db(parameters_.high_cut_gain_db);
    const float output_ramp = static_cast<float>(output_ramp_total_);
    level_step_ = (level_target_ - level_) / output_ramp;
    low_shelf_gain_step_ =
        (low_shelf_gain_target_ - low_shelf_gain_) / output_ramp;
    high_shelf_gain_step_ =
        (high_shelf_gain_target_ - high_shelf_gain_) / output_ramp;
    output_ramp_remaining_ = output_ramp_total_;
}

void ToneMasterSmallHallReverb::advance_input_controls() noexcept {
    if (input_ramp_remaining_ == 0U) return;
    dwell_gain_ += dwell_gain_step_;
    --input_ramp_remaining_;
    if (input_ramp_remaining_ == 0U) dwell_gain_ = dwell_gain_target_;
}

void ToneMasterSmallHallReverb::advance_output_controls() noexcept {
    if (output_ramp_remaining_ == 0U) return;
    level_ += level_step_;
    low_shelf_gain_ += low_shelf_gain_step_;
    high_shelf_gain_ += high_shelf_gain_step_;
    --output_ramp_remaining_;
    if (output_ramp_remaining_ == 0U) {
        level_ = level_target_;
        low_shelf_gain_ = low_shelf_gain_target_;
        high_shelf_gain_ = high_shelf_gain_target_;
    }
}

float ToneMasterSmallHallReverb::filter_wet(
    float sample, std::uint16_t channel) noexcept {
    float low = (1.0F - low_shelf_alpha_) * sample +
                low_shelf_alpha_ * low_shelf_state_[channel];
    low_shelf_state_[channel] = safe_sample(low);
    low = sample + (low_shelf_gain_ - 1.0F) * low;

    const float high_lowpass =
        (1.0F - high_shelf_alpha_) * low +
        high_shelf_alpha_ * high_shelf_lowpass_state_[channel];
    high_shelf_lowpass_state_[channel] = safe_sample(high_lowpass);
    const float high = low + (high_shelf_gain_ - 1.0F) *
        (low - high_lowpass);
    return safe_sample(high);
}

float ToneMasterSmallHallReverb::safe_sample(float sample) noexcept {
    return std::isfinite(sample) ? std::clamp(sample, -8.0F, 8.0F) : 0.0F;
}

void ToneMasterSmallHallReverb::process(
    std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U) return;
    const std::size_t frame_count = interleaved.size() / channels_;
    for (std::size_t first_frame = 0U; first_frame < frame_count;
         first_frame += kMaximumChunkFrames) {
        const std::size_t frames = std::min(
            kMaximumChunkFrames, frame_count - first_frame);
        const std::size_t samples = frames * channels_;
        const std::size_t sample_offset = first_frame * channels_;
        auto chunk = interleaved.subspan(sample_offset, samples);

        for (std::size_t frame = 0U; frame < frames; ++frame) {
            advance_input_controls();
            for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
                const std::size_t local = frame * channels_ + channel;
                const float dry = safe_sample(chunk[local]);
                dry_buffer_[local] = dry;
                chunk[local] = safe_sample(dry * dwell_gain_);
            }
        }
        reverb_.process(chunk, channels_);

        for (std::size_t frame = 0U; frame < frames; ++frame) {
            advance_output_controls();
            for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
                const std::size_t local = frame * channels_ + channel;
                const float wet = filter_wet(chunk[local], channel);
                chunk[local] = safe_sample(
                    dry_buffer_[local] * (1.0F - level_) + wet * level_);
            }
        }
    }
}

void ToneMasterSmallHallReverb::process_bypassed(
    std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U) return;
    if (!parameters_.spillover) {
        reverb_.process_bypassed(interleaved, channels_);
        low_shelf_state_.fill(0.0F);
        high_shelf_lowpass_state_.fill(0.0F);
        return;
    }

    const std::size_t frame_count = interleaved.size() / channels_;
    for (std::size_t first_frame = 0U; first_frame < frame_count;
         first_frame += kMaximumChunkFrames) {
        const std::size_t frames = std::min(
            kMaximumChunkFrames, frame_count - first_frame);
        const std::size_t samples = frames * channels_;
        const std::size_t sample_offset = first_frame * channels_;
        auto chunk = interleaved.subspan(sample_offset, samples);
        for (std::size_t local = 0U; local < samples; ++local) {
            dry_buffer_[local] = safe_sample(chunk[local]);
            chunk[local] = 0.0F;
        }

        reverb_.process_bypassed(chunk, channels_);
        for (std::size_t frame = 0U; frame < frames; ++frame) {
            advance_output_controls();
            for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
                const std::size_t local = frame * channels_ + channel;
                const float tail = filter_wet(chunk[local], channel) * level_;
                chunk[local] = safe_sample(dry_buffer_[local] + tail);
            }
        }
    }
}

void ToneMasterSmallHallReverb::reset() noexcept {
    reverb_.reset();
    low_shelf_state_.fill(0.0F);
    high_shelf_lowpass_state_.fill(0.0F);
    dwell_gain_ = dwell_gain_target_;
    level_ = level_target_;
    low_shelf_gain_ = low_shelf_gain_target_;
    high_shelf_gain_ = high_shelf_gain_target_;
    input_ramp_remaining_ = 0U;
    output_ramp_remaining_ = 0U;
}

} // namespace tabletfx
