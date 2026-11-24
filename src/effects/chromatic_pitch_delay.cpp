#include "tabletfx/effects/chromatic_pitch_delay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kDefaultTimeMs = 350.0F;
constexpr float kDefaultTempoBpm = 120.0F;
constexpr float kDefaultFeedback = 0.25F;
constexpr float kDefaultMix = 0.25F;
constexpr float kDefaultTone = 0.65F;
constexpr float kMaximumInternalSample = 16.0F;
constexpr float kPi = 3.14159265358979323846F;

float checked_sample_rate(float value) {
    if (!std::isfinite(value) || value < 8000.0F || value > 384000.0F) {
        throw std::invalid_argument(
            "ChromaticPitchDelay sample rate must be 8–384 kHz");
    }
    return value;
}

std::uint16_t checked_channels(std::uint16_t value) {
    if (value == 0 || value > 2) {
        throw std::invalid_argument(
            "ChromaticPitchDelay supports one or two channels");
    }
    return value;
}

float checked_max_delay(float value) {
    if (!std::isfinite(value) || value < 10.0F || value > 2000.0F) {
        throw std::invalid_argument(
            "ChromaticPitchDelay maximum time must be in [10, 2000] ms");
    }
    return value;
}

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

ChromaticPitchDelay::ChromaticPitchDelay(
    float sample_rate, std::uint16_t channels, float max_delay_ms,
    ChromaticPitchDelayParameters parameters)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)),
      max_delay_ms_(checked_max_delay(max_delay_ms)),
      pitch_shifter_(sample_rate_, channels_, 24.0F) {
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
        throw std::length_error("ChromaticPitchDelay buffer size is too large");
    }

    capacity_frames_ = static_cast<std::size_t>(maximum_delay_samples) + 2U;
    storage_.assign(capacity_frames_ * channels_, 0.0F);
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::ceil(
                static_cast<double>(sample_rate_) * kControlRampMs / 1000.0)));

    configure(parameters);
    reset();
}

std::size_t ChromaticPitchDelay::memory_bytes() const noexcept {
    return sizeof(*this) + storage_.capacity() * sizeof(float) +
           pitch_shifter_.memory_bytes() - sizeof(pitch_shifter_);
}

float ChromaticPitchDelay::effective_time_ms() const noexcept {
    if (parameters_.time_mode == ChromaticPitchDelayTimeMode::TempoSync) {
        return bounded(60000.0F / parameters_.tempo_bpm *
                           division_beats(parameters_.tap_division),
                       kDefaultTimeMs, kMinimumTimeMs,
                       std::min(kMaximumTimeMs, max_delay_ms_));
    }
    return parameters_.time_ms;
}

void ChromaticPitchDelay::configure(
    ChromaticPitchDelayParameters parameters) noexcept {
    parameters.pitch_semitones = bounded(parameters.pitch_semitones, 0.0F,
                                         -24.0F, 24.0F);
    parameters.time_ms = bounded(
        parameters.time_ms,
        std::clamp(kDefaultTimeMs, kMinimumTimeMs,
                   std::min(kMaximumTimeMs, max_delay_ms_)),
        kMinimumTimeMs, std::min(kMaximumTimeMs, max_delay_ms_));
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, kDefaultTempoBpm,
                                   20.0F, 300.0F);
    if (parameters.time_mode != ChromaticPitchDelayTimeMode::Milliseconds &&
        parameters.time_mode != ChromaticPitchDelayTimeMode::TempoSync) {
        parameters.time_mode = ChromaticPitchDelayTimeMode::Milliseconds;
    }
    if (static_cast<std::uint8_t>(parameters.tap_division) >
        static_cast<std::uint8_t>(ChromaticPitchDelayTapDivision::SixtyFourth)) {
        parameters.tap_division = ChromaticPitchDelayTapDivision::Quarter;
    }
    parameters.feedback = bounded(parameters.feedback, kDefaultFeedback,
                                  0.0F, kMaximumFeedback);
    parameters.tone = bounded(parameters.tone, kDefaultTone, 0.0F, 1.0F);
    parameters.mix = bounded(parameters.mix, kDefaultMix, 0.0F, 1.0F);

    parameters_ = parameters;
    pitch_shifter_.set_pitch_semitones(parameters_.pitch_semitones);

    const float effective_time = effective_time_ms();
    const float new_time_samples =
        static_cast<float>(samples_for_ms(effective_time));
    target_time_samples_ = new_time_samples;
    time_step_ = (new_time_samples - current_time_samples_) /
                 static_cast<float>(ramp_total_);
    feedback_step_ = (parameters_.feedback - current_feedback_) /
                     static_cast<float>(ramp_total_);
    mix_step_ = (parameters_.mix - current_mix_) /
                static_cast<float>(ramp_total_);
    target_tone_coefficient_ = tone_coefficient(parameters_.tone);
    tone_coefficient_step_ =
        (target_tone_coefficient_ - current_tone_coefficient_) /
        static_cast<float>(ramp_total_);
    ramp_remaining_ = ramp_total_;
}

void ChromaticPitchDelay::process(std::span<float> interleaved,
                                  std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;
    bypassed_ = false;
    process_internal(interleaved, true, false);
}

void ChromaticPitchDelay::process_bypassed(
    std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;
    if (!parameters_.spillover) {
        if (!bypassed_) reset();
        bypassed_ = true;
        for (float& sample : interleaved) sample = finite_bounded_sample(sample);
        return;
    }
    bypassed_ = true;
    process_internal(interleaved, false, true);
}

void ChromaticPitchDelay::process_internal(std::span<float> interleaved,
                                           bool record_input,
                                           bool bypassed) noexcept {
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

        pitch_shifter_.process(std::span<float>(shifted.data(), channels_),
                               channels_);

        const std::size_t write_offset = write_frame_ * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const float shifted_sample = finite_bounded_sample(shifted[channel]);
            filter_state_[channel] += current_tone_coefficient_ *
                                      (shifted_sample - filter_state_[channel]);
            const float wet = finite_bounded_sample(filter_state_[channel]);
            const double output = bypassed
                ? static_cast<double>(original[channel]) +
                      static_cast<double>(wet) * current_mix_
                : static_cast<double>(original[channel]) * (1.0 - current_mix_) +
                      static_cast<double>(wet) * current_mix_;
            const double recorded = (record_input
                                         ? static_cast<double>(original[channel])
                                         : 0.0) +
                                    static_cast<double>(wet) * current_feedback_;
            interleaved[frame * channels_ + channel] =
                finite_bounded_sample(output);
            storage_[write_offset + channel] = finite_bounded_sample(recorded);
        }

        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
        advance_ramps();
    }
}

void ChromaticPitchDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
    pitch_shifter_.reset();
    std::fill(std::begin(filter_state_), std::end(filter_state_), 0.0F);
    current_time_samples_ = target_time_samples_;
    current_feedback_ = parameters_.feedback;
    current_mix_ = parameters_.mix;
    current_tone_coefficient_ = target_tone_coefficient_;
    time_step_ = 0.0F;
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    tone_coefficient_step_ = 0.0F;
    ramp_remaining_ = 0;
    bypassed_ = false;
}

float ChromaticPitchDelay::bounded(float value, float fallback,
                                   float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float ChromaticPitchDelay::division_beats(
    ChromaticPitchDelayTapDivision division) noexcept {
    switch (division) {
    case ChromaticPitchDelayTapDivision::Whole: return 4.0F;
    case ChromaticPitchDelayTapDivision::DottedHalf: return 3.0F;
    case ChromaticPitchDelayTapDivision::Half: return 2.0F;
    case ChromaticPitchDelayTapDivision::DottedQuarter: return 1.5F;
    case ChromaticPitchDelayTapDivision::Quarter: return 1.0F;
    case ChromaticPitchDelayTapDivision::QuarterTriplet: return 2.0F / 3.0F;
    case ChromaticPitchDelayTapDivision::DottedEighth: return 0.75F;
    case ChromaticPitchDelayTapDivision::Eighth: return 0.5F;
    case ChromaticPitchDelayTapDivision::EighthTriplet: return 1.0F / 3.0F;
    case ChromaticPitchDelayTapDivision::DottedSixteenth: return 0.375F;
    case ChromaticPitchDelayTapDivision::Sixteenth: return 0.25F;
    case ChromaticPitchDelayTapDivision::SixteenthTriplet: return 1.0F / 6.0F;
    case ChromaticPitchDelayTapDivision::ThirtySecond: return 0.125F;
    case ChromaticPitchDelayTapDivision::SixtyFourth: return 0.0625F;
    }
    return 1.0F;
}

float ChromaticPitchDelay::tone_coefficient(float tone) const noexcept {
    const float cutoff = std::min(
        kMinimumToneCutoffHz *
            std::exp(std::log(kMaximumToneCutoffHz / kMinimumToneCutoffHz) * tone),
        0.45F * sample_rate_);
    return 1.0F - std::exp(-2.0F * kPi * cutoff / sample_rate_);
}

std::size_t ChromaticPitchDelay::samples_for_ms(float time_ms) const noexcept {
    const double nominal_samples = std::round(
        static_cast<double>(sample_rate_) * static_cast<double>(time_ms) / 1000.0);
    const double maximum = static_cast<double>(capacity_frames_ - 2U);
    return static_cast<std::size_t>(std::clamp(nominal_samples, 1.0, maximum));
}

float ChromaticPitchDelay::read_fractional_delay(
    float delay_samples, std::uint16_t channel) const noexcept {
    const float bounded_delay = std::clamp(
        delay_samples, 1.0F, static_cast<float>(capacity_frames_ - 2U));
    const auto whole = static_cast<std::size_t>(std::floor(bounded_delay));
    const float fraction = bounded_delay - static_cast<float>(whole);
    const float recent = read_age(whole, channel);
    const float previous = read_age(whole + 1U, channel);
    return recent + (previous - recent) * fraction;
}

float ChromaticPitchDelay::read_age(std::size_t age,
                                    std::uint16_t channel) const noexcept {
    if (age == 0U || age >= capacity_frames_ || channel >= channels_) return 0.0F;
    const std::size_t read_frame =
        (write_frame_ + capacity_frames_ - age) % capacity_frames_;
    return storage_[read_frame * channels_ + channel];
}

void ChromaticPitchDelay::advance_ramps() noexcept {
    if (ramp_remaining_ == 0U) return;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) {
        current_time_samples_ = target_time_samples_;
        current_feedback_ = parameters_.feedback;
        current_mix_ = parameters_.mix;
        current_tone_coefficient_ = target_tone_coefficient_;
    } else {
        current_time_samples_ += time_step_;
        current_feedback_ += feedback_step_;
        current_mix_ += mix_step_;
        current_tone_coefficient_ += tone_coefficient_step_;
    }
}

} // namespace tabletfx
