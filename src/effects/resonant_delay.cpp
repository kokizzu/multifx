#include "tabletfx/effects/resonant_delay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {

ResonantDelay::ResonantDelay(float sample_rate, std::uint16_t channels,
                             float max_delay_ms,
                             ResonantDelayParameters parameters)
    : sample_rate_(sample_rate), channels_(channels),
      max_delay_ms_(max_delay_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
        sample_rate_ > 384000.0F || channels_ == 0 || channels_ > 2 ||
        !std::isfinite(max_delay_ms_) || max_delay_ms_ < 1.0F ||
        max_delay_ms_ > 2000.0F) {
        throw std::invalid_argument(
            "Resonant Delay requires 8–384 kHz, mono or stereo, and 1–2000 ms capacity");
    }

    const double maximum_frames = std::ceil(
        static_cast<double>(sample_rate_) *
        static_cast<double>(max_delay_ms_) / 1000.0);
    const std::size_t maximum_size = delay_buffer_.max_size() / channels_;
    if (!std::isfinite(maximum_frames) || maximum_frames < 1.0 ||
        maximum_frames >= static_cast<double>(maximum_size)) {
        throw std::length_error("Resonant Delay buffer is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(maximum_frames) + 1U;
    delay_buffer_.assign(capacity_frames_ * channels_, 0.0F);
    configure(parameters);
    reset();
}

void ResonantDelay::configure(ResonantDelayParameters parameters) noexcept {
    const float maximum_time = std::min(2000.0F, max_delay_ms_);
    parameters.time_ms = std::isfinite(parameters.time_ms)
        ? std::clamp(parameters.time_ms, 1.0F, maximum_time)
        : 350.0F;
    parameters.feedback = std::isfinite(parameters.feedback)
        ? std::clamp(parameters.feedback, 0.0F, 0.95F)
        : 0.35F;
    const float maximum_center = std::max(50.0F,
        std::min(8000.0F, sample_rate_ * 0.45F));
    parameters.center_hz = std::isfinite(parameters.center_hz)
        ? std::clamp(parameters.center_hz, 50.0F, maximum_center)
        : std::min(800.0F, maximum_center);
    parameters.resonance_q = std::isfinite(parameters.resonance_q)
        ? std::clamp(parameters.resonance_q, 0.5F, 12.0F)
        : 2.0F;
    parameters.mix = std::isfinite(parameters.mix)
        ? std::clamp(parameters.mix, 0.0F, 1.0F)
        : 0.25F;
    parameters_ = parameters;

    const double requested_frames = std::round(
        static_cast<double>(sample_rate_) *
        static_cast<double>(parameters_.time_ms) / 1000.0);
    delay_frames_ = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::max(1.0, requested_frames)), 1U,
        capacity_frames_ - 1U);
    update_filter();
}

void ResonantDelay::update_filter() noexcept {
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    const double omega = kTwoPi * static_cast<double>(parameters_.center_hz) /
                         static_cast<double>(sample_rate_);
    const double sine = std::sin(omega);
    const double cosine = std::cos(omega);
    const double alpha = sine /
        (2.0 * static_cast<double>(parameters_.resonance_q));
    const double inverse = 1.0 / (1.0 + alpha);
    b0_ = static_cast<float>(alpha * inverse);
    b1_ = 0.0F;
    b2_ = -b0_;
    a1_ = static_cast<float>(-2.0 * cosine * inverse);
    a2_ = static_cast<float>((1.0 - alpha) * inverse);
}

float ResonantDelay::filter_sample(float input,
                                   std::uint16_t channel) noexcept {
    auto& state = filter_state_[channel];
    const float output = b0_ * input + b1_ * state.input_1 +
        b2_ * state.input_2 - a1_ * state.output_1 -
        a2_ * state.output_2;
    state.input_2 = state.input_1;
    state.input_1 = input;
    state.output_2 = state.output_1;
    state.output_1 = std::isfinite(output) ? output : 0.0F;
    return state.output_1;
}

void ResonantDelay::process(std::span<float> interleaved,
                            std::uint16_t channels) noexcept {
    if (channels != channels_) return;
    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t read_frame = write_frame_ >= delay_frames_
            ? write_frame_ - delay_frames_
            : write_frame_ + capacity_frames_ - delay_frames_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = frame * channels_ + channel;
            const std::size_t buffer_index = read_frame * channels_ + channel;
            const float dry = std::isfinite(interleaved[index])
                ? interleaved[index]
                : 0.0F;
            const float delayed = delay_buffer_[buffer_index];
            const float wet = filter_sample(delayed, channel);
            delay_buffer_[write_frame_ * channels_ + channel] =
                dry + parameters_.feedback * wet;
            interleaved[index] = (1.0F - parameters_.mix) * dry +
                                 parameters_.mix * wet;
        }
        ++write_frame_;
        if (write_frame_ == capacity_frames_) write_frame_ = 0;
    }
}

void ResonantDelay::reset() noexcept {
    std::fill(delay_buffer_.begin(), delay_buffer_.end(), 0.0F);
    filter_state_ = {};
    write_frame_ = 0;
}

} // namespace tabletfx
