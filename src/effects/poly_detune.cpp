#include "tabletfx/effects/poly_detune.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {

PolyDetune::PolyDetune(float sample_rate, std::uint16_t channels,
                       PolyDetuneParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument(
            "PolyDetune requires 8–384 kHz and mono or stereo channels");
    }

    const double frames = std::ceil(static_cast<double>(sample_rate_) *
                                    static_cast<double>(kStorageDelayMs) / 1000.0);
    if (frames > static_cast<double>(std::numeric_limits<std::size_t>::max() - 2)) {
        throw std::length_error("PolyDetune delay buffer is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(frames) + 2U;
    storage_.assign(capacity_frames_ * channels_, 0.0F);
    set_parameters(parameters);
    retarget(true);
}

void PolyDetune::set_parameters(PolyDetuneParameters parameters) noexcept {
    parameters.rate_hz = bounded(parameters.rate_hz, 0.25F, 0.05F, 2.0F);
    parameters.depth_ms = bounded(parameters.depth_ms, 0.75F,
                                  0.0F, kMaximumDepthMs);
    if (parameters.voice_count != 1U && parameters.voice_count != 2U) {
        parameters.voice_count = 2U;
    }
    parameters.width = bounded(parameters.width, 0.75F, 0.0F, 1.0F);
    parameters.mix = bounded(parameters.mix, 0.25F, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, 0.0F, -24.0F, 12.0F);
    parameters.smoothing_ms = bounded(parameters.smoothing_ms, 10.0F,
                                       0.0F, 1000.0F);
    parameters_ = parameters;

    target_depth_ms_ = parameters_.depth_ms;
    target_width_ = parameters_.width;
    target_mix_ = parameters_.mix;
    target_level_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    prepare_rate();
    retarget(false);
}

void PolyDetune::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    const float samples_per_ms = sample_rate_ * 0.001F;
    const float base_delay_frames = kBaseDelayMs * samples_per_ms;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (ramp_remaining_ > 0U) {
            current_depth_ms_ += depth_step_;
            current_width_ += width_step_;
            current_mix_ += mix_step_;
            current_level_gain_ += level_gain_step_;
            sine_increment_ += sine_increment_step_;
            cosine_increment_ += cosine_increment_step_;
            --ramp_remaining_;
            if (ramp_remaining_ == 0U) {
                current_depth_ms_ = target_depth_ms_;
                current_width_ = target_width_;
                current_mix_ = target_mix_;
                current_level_gain_ = target_level_gain_;
                sine_increment_ = target_sine_increment_;
                cosine_increment_ = target_cosine_increment_;
                depth_step_ = 0.0F;
                width_step_ = 0.0F;
                mix_step_ = 0.0F;
                level_gain_step_ = 0.0F;
                sine_increment_step_ = 0.0F;
                cosine_increment_step_ = 0.0F;
            }
        }

        const std::size_t input_offset = frame * channels_;
        const std::size_t write_offset = write_frame_ * channels_;
        const float left_input = std::clamp(
            std::isfinite(interleaved[input_offset]) ? interleaved[input_offset] : 0.0F,
            -kMaximumSignal, kMaximumSignal);
        storage_[write_offset] = left_input;

        const float modulation = sine_phase_ * current_width_;
        const float depth_frames = current_depth_ms_ * samples_per_ms;
        const float left_delay = std::clamp(
            base_delay_frames + depth_frames * modulation,
            1.0F, static_cast<float>(capacity_frames_ - 2U));
        const float left_wet = read_interpolated(0U, left_delay);

        if (channels_ == 1U) {
            float wet = left_wet;
            if (parameters_.voice_count == 2U) {
                const float right_delay = std::clamp(
                    base_delay_frames - depth_frames * modulation,
                    1.0F, static_cast<float>(capacity_frames_ - 2U));
                wet = 0.5F * (left_wet + read_interpolated(0U, right_delay));
            }
            const float output = ((1.0F - current_mix_) * left_input +
                                  current_mix_ * wet) * current_level_gain_;
            interleaved[input_offset] = std::clamp(
                std::isfinite(output) ? output : 0.0F,
                -kMaximumSignal, kMaximumSignal);
        } else {
            const float right_input = std::clamp(
                std::isfinite(interleaved[input_offset + 1U])
                    ? interleaved[input_offset + 1U]
                    : 0.0F,
                -kMaximumSignal, kMaximumSignal);
            storage_[write_offset + 1U] = right_input;
            const float right_delay = parameters_.voice_count == 2U
                ? std::clamp(base_delay_frames - depth_frames * modulation,
                             1.0F, static_cast<float>(capacity_frames_ - 2U))
                : left_delay;
            const float right_wet = read_interpolated(1U, right_delay);
            const float left_output =
                ((1.0F - current_mix_) * left_input + current_mix_ * left_wet) *
                current_level_gain_;
            const float right_output =
                ((1.0F - current_mix_) * right_input + current_mix_ * right_wet) *
                current_level_gain_;
            interleaved[input_offset] = std::clamp(
                std::isfinite(left_output) ? left_output : 0.0F,
                -kMaximumSignal, kMaximumSignal);
            interleaved[input_offset + 1U] = std::clamp(
                std::isfinite(right_output) ? right_output : 0.0F,
                -kMaximumSignal, kMaximumSignal);
        }

        ++write_frame_;
        if (write_frame_ == capacity_frames_) {
            write_frame_ = 0U;
        }
        advance_modulator();
    }
}

void PolyDetune::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0U;
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    normalization_counter_ = 0U;
    retarget(true);
}

float PolyDetune::bounded(float value, float fallback,
                          float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

void PolyDetune::prepare_rate() noexcept {
    const float step = kTwoPi * parameters_.rate_hz / sample_rate_;
    target_sine_increment_ = std::sin(step);
    target_cosine_increment_ = std::cos(step);
}

void PolyDetune::retarget(bool immediate) noexcept {
    if (immediate) {
        current_depth_ms_ = target_depth_ms_;
        current_width_ = target_width_;
        current_mix_ = target_mix_;
        current_level_gain_ = target_level_gain_;
        sine_increment_ = target_sine_increment_;
        cosine_increment_ = target_cosine_increment_;
        depth_step_ = 0.0F;
        width_step_ = 0.0F;
        mix_step_ = 0.0F;
        level_gain_step_ = 0.0F;
        sine_increment_step_ = 0.0F;
        cosine_increment_step_ = 0.0F;
        ramp_remaining_ = 0U;
        return;
    }

    const double requested_frames = static_cast<double>(sample_rate_) *
        static_cast<double>(parameters_.smoothing_ms) / 1000.0;
    ramp_remaining_ = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_remaining_ == 0U) {
        retarget(true);
        return;
    }
    const float denominator = static_cast<float>(ramp_remaining_);
    depth_step_ = (target_depth_ms_ - current_depth_ms_) / denominator;
    width_step_ = (target_width_ - current_width_) / denominator;
    mix_step_ = (target_mix_ - current_mix_) / denominator;
    level_gain_step_ = (target_level_gain_ - current_level_gain_) / denominator;
    sine_increment_step_ =
        (target_sine_increment_ - sine_increment_) / denominator;
    cosine_increment_step_ =
        (target_cosine_increment_ - cosine_increment_) / denominator;
}

void PolyDetune::advance_modulator() noexcept {
    const float old_sine = sine_phase_;
    sine_phase_ = old_sine * cosine_increment_ +
                  cosine_phase_ * sine_increment_;
    cosine_phase_ = cosine_phase_ * cosine_increment_ -
                    old_sine * sine_increment_;
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
        normalization_counter_ = 0U;
    }
}

float PolyDetune::read_interpolated(std::size_t channel,
                                    float delay_frames) const noexcept {
    const float bounded_delay = std::clamp(
        delay_frames, 1.0F, static_cast<float>(capacity_frames_ - 2U));
    const std::size_t whole_delay = static_cast<std::size_t>(bounded_delay);
    const float fraction = bounded_delay - static_cast<float>(whole_delay);
    std::size_t read_frame = write_frame_ + capacity_frames_ - whole_delay;
    if (read_frame >= capacity_frames_) {
        read_frame -= capacity_frames_;
    }
    const std::size_t older_frame = read_frame == 0U
                                        ? capacity_frames_ - 1U
                                        : read_frame - 1U;
    const float newer = storage_[read_frame * channels_ + channel];
    const float older = storage_[older_frame * channels_ + channel];
    return newer * (1.0F - fraction) + older * fraction;
}

} // namespace tabletfx
