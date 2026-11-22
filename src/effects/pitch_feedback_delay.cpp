#include "tabletfx/effects/pitch_feedback_delay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kDefaultTimeMs = 350.0F;
constexpr float kDefaultFeedback = 0.35F;
constexpr float kDefaultMix = 0.25F;
constexpr float kDefaultPitchSemitones = 7.0F;
constexpr float kMaximumInternalSample = 16.0F;

float finite_bounded_sample(float value) noexcept {
    return std::isfinite(value)
        ? std::clamp(value, -kMaximumInternalSample, kMaximumInternalSample)
        : 0.0F;
}

float finite_bounded_sample(double value) noexcept {
    if (!std::isfinite(value)) return 0.0F;
    return static_cast<float>(std::clamp(
        value, -static_cast<double>(kMaximumInternalSample),
        static_cast<double>(kMaximumInternalSample)));
}

} // namespace

PitchFeedbackDelay::PitchFeedbackDelay(float sample_rate, std::uint16_t channels,
                                       float max_delay_ms,
                                       PitchFeedbackDelayParameters parameters)
    : sample_rate_(sample_rate), channels_(channels),
      max_delay_ms_(max_delay_ms), pitch_shifter_(sample_rate, channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
        sample_rate_ > 384000.0F || channels_ == 0 || channels_ > 2 ||
        !std::isfinite(max_delay_ms_) || max_delay_ms_ < kMinimumTimeMs ||
        max_delay_ms_ > kMaximumTimeMs) {
        throw std::invalid_argument(
            "Pitch Feedback Delay requires a supported sample rate, mono/stereo layout, and maximum time in [40, 6000] ms");
    }

    shifter_latency_samples_ = static_cast<std::size_t>(std::lround(
        static_cast<double>(pitch_shifter_.algorithmic_latency_ms()) *
        static_cast<double>(sample_rate_) / 1000.0));
    const double maximum_delay_samples = std::ceil(
        static_cast<double>(sample_rate_) * static_cast<double>(max_delay_ms_) /
        1000.0);
    const std::size_t sample_capacity_limit = storage_.max_size() / channels_;
    if (!std::isfinite(maximum_delay_samples) || maximum_delay_samples < 1.0 ||
        sample_capacity_limit < 2U ||
        maximum_delay_samples > static_cast<double>(sample_capacity_limit - 2U)) {
        throw std::length_error("Pitch Feedback Delay buffer size is too large");
    }

    capacity_frames_ = static_cast<std::size_t>(maximum_delay_samples) + 2U;
    storage_.assign(capacity_frames_ * channels_, 0.0F);
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::ceil(
                static_cast<double>(sample_rate_) * kControlRampMs / 1000.0)));

    configure(parameters);
    reset();
}

std::size_t PitchFeedbackDelay::memory_bytes() const noexcept {
    return sizeof(*this) + storage_.capacity() * sizeof(float) +
           pitch_shifter_.memory_bytes() - sizeof(pitch_shifter_);
}

void PitchFeedbackDelay::configure(PitchFeedbackDelayParameters parameters) noexcept {
    const float maximum_time_ms = std::min(kMaximumTimeMs, max_delay_ms_);
    parameters.time_ms = bounded(
        parameters.time_ms,
        std::clamp(kDefaultTimeMs, kMinimumTimeMs, maximum_time_ms),
        kMinimumTimeMs, maximum_time_ms);
    parameters.feedback = bounded(parameters.feedback, kDefaultFeedback,
                                  0.0F, kMaximumFeedback);
    parameters.mix = bounded(parameters.mix, kDefaultMix, 0.0F, 1.0F);
    parameters.pitch_semitones = bounded(parameters.pitch_semitones,
                                         kDefaultPitchSemitones,
                                         -12.0F, 12.0F);

    const float new_time_samples = static_cast<float>(samples_for_ms(parameters.time_ms));
    target_time_samples_ = new_time_samples;
    time_step_ = (new_time_samples - current_time_samples_) /
                 static_cast<float>(ramp_total_);
    feedback_step_ = (parameters.feedback - current_feedback_) /
                     static_cast<float>(ramp_total_);
    mix_step_ = (parameters.mix - current_mix_) /
                static_cast<float>(ramp_total_);
    parameters_ = parameters;
    pitch_shifter_.set_pitch_semitones(parameters_.pitch_semitones);
    ramp_remaining_ = ramp_total_;
}

void PitchFeedbackDelay::process(std::span<float> interleaved,
                                 std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    std::array<float, 2> original{};
    std::array<float, 2> shifted{};

    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float delay_samples = std::clamp(
            current_time_samples_ - static_cast<float>(shifter_latency_samples_),
            1.0F, static_cast<float>(capacity_frames_ - 2U));

        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = frame * channels_ + channel;
            original[channel] = finite_bounded_sample(interleaved[index]);
            shifted[channel] = read_fractional_delay(delay_samples, channel);
        }

        pitch_shifter_.process(
            std::span<float>(shifted.data(), channels_), channels_);

        const std::size_t write_offset = write_frame_ * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            shifted[channel] = finite_bounded_sample(shifted[channel]);
            const double output =
                static_cast<double>(original[channel]) * (1.0 - current_mix_) +
                static_cast<double>(shifted[channel]) * current_mix_;
            const double recorded = static_cast<double>(original[channel]) +
                                    static_cast<double>(shifted[channel]) *
                                        current_feedback_;
            interleaved[frame * channels_ + channel] =
                finite_bounded_sample(output);
            storage_[write_offset + channel] = finite_bounded_sample(recorded);
        }

        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
        advance_ramps();
    }
}

void PitchFeedbackDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
    pitch_shifter_.reset();
    current_time_samples_ = target_time_samples_;
    current_feedback_ = parameters_.feedback;
    current_mix_ = parameters_.mix;
    time_step_ = 0.0F;
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    ramp_remaining_ = 0;
}

float PitchFeedbackDelay::bounded(float value, float fallback,
                                  float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

std::size_t PitchFeedbackDelay::samples_for_ms(float time_ms) const noexcept {
    const double nominal_samples = std::round(
        static_cast<double>(sample_rate_) * static_cast<double>(time_ms) / 1000.0);
    const double maximum = static_cast<double>(capacity_frames_ - 2U);
    return static_cast<std::size_t>(std::clamp(nominal_samples, 1.0, maximum));
}

float PitchFeedbackDelay::read_fractional_delay(float delay_samples,
                                                std::uint16_t channel) const noexcept {
    const float bounded_delay = std::clamp(
        delay_samples, 1.0F, static_cast<float>(capacity_frames_ - 2U));
    const auto whole = static_cast<std::size_t>(std::floor(bounded_delay));
    const float fraction = bounded_delay - static_cast<float>(whole);
    const float recent = read_age(whole, channel);
    const float previous = read_age(whole + 1U, channel);
    return recent + (previous - recent) * fraction;
}

float PitchFeedbackDelay::read_age(std::size_t age,
                                   std::uint16_t channel) const noexcept {
    if (age == 0U || age >= capacity_frames_ || channel >= channels_) return 0.0F;
    const std::size_t read_frame =
        (write_frame_ + capacity_frames_ - age) % capacity_frames_;
    return storage_[read_frame * channels_ + channel];
}

void PitchFeedbackDelay::advance_ramps() noexcept {
    if (ramp_remaining_ == 0U) return;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) {
        current_time_samples_ = target_time_samples_;
        current_feedback_ = parameters_.feedback;
        current_mix_ = parameters_.mix;
    } else {
        current_time_samples_ += time_step_;
        current_feedback_ += feedback_step_;
        current_mix_ += mix_step_;
    }
}

} // namespace tabletfx
