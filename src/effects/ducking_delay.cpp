#include "tabletfx/effects/ducking_delay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr double kMaximumFloat =
    static_cast<double>(std::numeric_limits<float>::max());

float bounded_sample(double value) noexcept {
    return static_cast<float>(std::clamp(value, -kMaximumFloat, kMaximumFloat));
}

float one_pole_coefficient(float sample_rate, float milliseconds) noexcept {
    const double time_constant = std::max(
        1.0, static_cast<double>(sample_rate) * static_cast<double>(milliseconds) / 1000.0);
    return static_cast<float>(1.0 - std::exp(-1.0 / time_constant));
}

} // namespace

DuckingDelay::DuckingDelay(float sample_rate, std::uint16_t channels,
                           float max_delay_ms,
                           DuckingDelayParameters parameters)
    : sample_rate_(sample_rate), channels_(channels), max_delay_ms_(max_delay_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2 ||
        !std::isfinite(max_delay_ms_) || max_delay_ms_ < kMinimumTimeMs ||
        max_delay_ms_ > 6000.0F) {
        throw std::invalid_argument(
            "Ducking Delay requires a supported sample rate, mono/stereo layout, and maximum time in [1, 6000] ms");
    }

    const double maximum_delay_samples = std::ceil(
        static_cast<double>(sample_rate_) * static_cast<double>(max_delay_ms_) / 1000.0);
    if (!std::isfinite(maximum_delay_samples) || maximum_delay_samples < 1.0 ||
        maximum_delay_samples > static_cast<double>(storage_.max_size() / channels_ - 2U)) {
        throw std::length_error("Ducking Delay buffer size is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(maximum_delay_samples) + 2U;
    storage_.assign(capacity_frames_ * channels_, 0.0F);
    parameter_ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::ceil(
                static_cast<double>(sample_rate_) * kControlRampMs / 1000.0)));
    delay_transition_total_ = parameter_ramp_total_;
    duck_attack_coefficient_ = 1.0F;

    configure(parameters);
    current_feedback_ = parameters_.feedback;
    current_mix_ = parameters_.mix;
    parameter_ramp_remaining_ = 0;
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    reset();
}

void DuckingDelay::configure(DuckingDelayParameters parameters) noexcept {
    parameters.time_ms = bounded(parameters.time_ms, 350.0F,
                                 kMinimumTimeMs,
                                 std::min(kMaximumTimeMs, max_delay_ms_));
    if (parameters.time_mode != DuckingDelayTimeMode::milliseconds &&
        parameters.time_mode != DuckingDelayTimeMode::tempo_sync) {
        parameters.time_mode = DuckingDelayTimeMode::milliseconds;
    }
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, 120.0F,
                                   kMinimumTempoBpm, kMaximumTempoBpm);
    switch (parameters.tap_division) {
    case DuckingDelayTapDivision::whole:
    case DuckingDelayTapDivision::dotted_half:
    case DuckingDelayTapDivision::half:
    case DuckingDelayTapDivision::dotted_quarter:
    case DuckingDelayTapDivision::quarter:
    case DuckingDelayTapDivision::quarter_triplet:
    case DuckingDelayTapDivision::dotted_eighth:
    case DuckingDelayTapDivision::eighth:
    case DuckingDelayTapDivision::eighth_triplet:
    case DuckingDelayTapDivision::dotted_sixteenth:
    case DuckingDelayTapDivision::sixteenth:
    case DuckingDelayTapDivision::sixteenth_triplet:
    case DuckingDelayTapDivision::thirty_second:
    case DuckingDelayTapDivision::sixty_fourth:
        break;
    default:
        parameters.tap_division = DuckingDelayTapDivision::quarter;
        break;
    }

    float selected_time_ms = parameters.time_ms;
    if (parameters.time_mode == DuckingDelayTimeMode::tempo_sync) {
        selected_time_ms = 60000.0F / parameters.tempo_bpm *
                           beats_for_division(parameters.tap_division);
    }
    selected_time_ms = bounded(selected_time_ms, parameters.time_ms,
                                kMinimumTimeMs,
                                std::min(kMaximumTimeMs, max_delay_ms_));
    const float new_delay = static_cast<float>(samples_for_ms(selected_time_ms));
    if (target_delay_samples_ == 0.0F) {
        target_delay_samples_ = new_delay;
        delay_transition_from_samples_ = new_delay;
    } else if (std::abs(new_delay - target_delay_samples_) > 1.0e-5F) {
        delay_transition_from_samples_ = current_delay_samples();
        target_delay_samples_ = new_delay;
        delay_transition_remaining_ = delay_transition_total_;
    }

    parameters.feedback = bounded(parameters.feedback, 0.25F,
                                  0.0F, kMaximumFeedback);
    parameters.mix = bounded(parameters.mix, 0.25F, 0.0F, 1.0F);
    parameters.threshold_db = bounded(parameters.threshold_db, -30.0F,
                                      kMinimumThresholdDb, 0.0F);
    parameters.attenuation_db = bounded(parameters.attenuation_db, 12.0F,
                                        0.0F, kMaximumAttenuationDb);
    parameters.release_ms = bounded(parameters.release_ms, 250.0F,
                                    kMinimumReleaseMs, kMaximumReleaseMs);

    const float low_cut_max = std::max(
        20.0F, std::min(1000.0F, sample_rate_ * 0.15F));
    const float high_cut_max = std::max(
        20.0F, std::min(20000.0F, sample_rate_ * 0.45F));
    parameters.low_cut_hz = bounded(parameters.low_cut_hz, 0.0F,
                                    0.0F, low_cut_max);
    parameters.high_cut_hz = bounded(parameters.high_cut_hz, 0.0F,
                                     0.0F, high_cut_max);
    if (parameters.low_cut_hz > 0.0F) {
        parameters.low_cut_hz = std::clamp(parameters.low_cut_hz,
                                           20.0F, low_cut_max);
    }
    if (parameters.high_cut_hz > 0.0F) {
        parameters.high_cut_hz = std::clamp(parameters.high_cut_hz,
                                            20.0F, high_cut_max);
    }

    const float previous_low_cut = parameters_.low_cut_hz;
    const float previous_high_cut = parameters_.high_cut_hz;
    repeat_highpass_alpha_ = parameters.low_cut_hz > 0.0F
        ? std::exp(-6.2831853071795864769F * parameters.low_cut_hz / sample_rate_)
        : 1.0F;
    repeat_lowpass_alpha_ = parameters.high_cut_hz > 0.0F
        ? 1.0F - std::exp(-6.2831853071795864769F *
                          parameters.high_cut_hz / sample_rate_)
        : 1.0F;
    if (previous_low_cut != parameters.low_cut_hz ||
        previous_high_cut != parameters.high_cut_hz) {
        repeat_highpass_previous_input_.fill(0.0F);
        repeat_highpass_previous_output_.fill(0.0F);
        repeat_lowpass_state_.fill(0.0F);
    }

    threshold_amplitude_ = std::pow(10.0F, parameters.threshold_db / 20.0F);
    attenuation_gain_ = std::pow(10.0F, -parameters.attenuation_db / 20.0F);
    release_coefficient_ = one_pole_coefficient(sample_rate_, parameters.release_ms);
    feedback_step_ = (parameters.feedback - current_feedback_) /
                     static_cast<float>(parameter_ramp_total_);
    mix_step_ = (parameters.mix - current_mix_) /
                static_cast<float>(parameter_ramp_total_);
    parameters_ = parameters;
    parameter_ramp_remaining_ = parameter_ramp_total_;
}

void DuckingDelay::process_audio(std::span<float> interleaved,
                                 bool bypassed) noexcept {
    const std::size_t frames = interleaved.size() / channels_;
    std::array<float, 2> original{};
    std::array<float, 2> delayed{};

    for (std::size_t frame = 0; frame < frames; ++frame) {
        float input_peak = 0.0F;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = frame * channels_ + channel;
            original[channel] = std::isfinite(interleaved[index])
                ? interleaved[index]
                : 0.0F;
            input_peak = std::max(input_peak, std::abs(original[channel]));
            delayed[channel] = read_fractional_delay(
                current_delay_samples(), channel);
        }

        const bool should_duck = !bypassed && parameters_.attenuation_db > 0.0F &&
                                 input_peak > threshold_amplitude_;
        const float target_gain = should_duck ? attenuation_gain_ : 1.0F;
        const float coefficient = target_gain < duck_gain_
            ? duck_attack_coefficient_
            : release_coefficient_;
        duck_gain_ += coefficient * (target_gain - duck_gain_);

        const std::size_t write_offset = write_frame_ * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const double wet = static_cast<double>(delayed[channel]) * duck_gain_;
            const double output = bypassed
                ? static_cast<double>(original[channel]) +
                      wet * static_cast<double>(current_mix_)
                : static_cast<double>(original[channel]) *
                      static_cast<double>(1.0F - current_mix_) +
                      wet * static_cast<double>(current_mix_);
            interleaved[frame * channels_ + channel] = bounded_sample(output);
            const float filtered_repeat = filter_repeat_sample(
                delayed[channel], channel);
            const double recorded = (bypassed ? 0.0 :
                                     static_cast<double>(original[channel])) +
                                    static_cast<double>(filtered_repeat) * current_feedback_;
            storage_[write_offset + channel] = bounded_sample(recorded);
        }

        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
        advance_ramps();
    }
}

void DuckingDelay::process(std::span<float> interleaved,
                           std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;
    storage_cleared_ = false;
    process_audio(interleaved, false);
}

void DuckingDelay::process_bypassed(std::span<float> interleaved,
                                    std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;
    if (!parameters_.spillover) {
        if (!storage_cleared_) reset();
        return;
    }
    storage_cleared_ = false;
    process_audio(interleaved, true);
}

void DuckingDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
    storage_cleared_ = true;
    duck_gain_ = 1.0F;
    current_feedback_ = parameters_.feedback;
    current_mix_ = parameters_.mix;
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    parameter_ramp_remaining_ = 0;
    delay_transition_from_samples_ = target_delay_samples_;
    delay_transition_remaining_ = 0;
    repeat_highpass_previous_input_.fill(0.0F);
    repeat_highpass_previous_output_.fill(0.0F);
    repeat_lowpass_state_.fill(0.0F);
}

float DuckingDelay::bounded(float value, float fallback,
                            float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float DuckingDelay::beats_for_division(
    DuckingDelayTapDivision division) noexcept {
    switch (division) {
    case DuckingDelayTapDivision::whole: return 4.0F;
    case DuckingDelayTapDivision::dotted_half: return 3.0F;
    case DuckingDelayTapDivision::half: return 2.0F;
    case DuckingDelayTapDivision::dotted_quarter: return 1.5F;
    case DuckingDelayTapDivision::quarter: return 1.0F;
    case DuckingDelayTapDivision::quarter_triplet: return 2.0F / 3.0F;
    case DuckingDelayTapDivision::dotted_eighth: return 0.75F;
    case DuckingDelayTapDivision::eighth: return 0.5F;
    case DuckingDelayTapDivision::eighth_triplet: return 1.0F / 3.0F;
    case DuckingDelayTapDivision::dotted_sixteenth: return 0.375F;
    case DuckingDelayTapDivision::sixteenth: return 0.25F;
    case DuckingDelayTapDivision::sixteenth_triplet: return 1.0F / 6.0F;
    case DuckingDelayTapDivision::thirty_second: return 0.125F;
    case DuckingDelayTapDivision::sixty_fourth: return 0.0625F;
    }
    return 1.0F;
}

std::size_t DuckingDelay::samples_for_ms(float time_ms) const noexcept {
    const double samples = std::round(
        static_cast<double>(sample_rate_) * static_cast<double>(time_ms) / 1000.0);
    const double maximum = static_cast<double>(capacity_frames_ - 2U);
    return static_cast<std::size_t>(std::clamp(samples, 1.0, maximum));
}

float DuckingDelay::current_delay_samples() const noexcept {
    if (delay_transition_remaining_ == 0U) return target_delay_samples_;
    const float progress = 1.0F -
        static_cast<float>(delay_transition_remaining_) /
            static_cast<float>(delay_transition_total_);
    return delay_transition_from_samples_ +
           (target_delay_samples_ - delay_transition_from_samples_) * progress;
}

float DuckingDelay::filter_repeat_sample(float sample,
                                         std::uint16_t channel) noexcept {
    if (channel >= channels_) return sample;
    float filtered = sample;
    if (parameters_.low_cut_hz > 0.0F) {
        const float highpass = repeat_highpass_alpha_ *
            (repeat_highpass_previous_output_[channel] + filtered -
             repeat_highpass_previous_input_[channel]);
        repeat_highpass_previous_input_[channel] = filtered;
        repeat_highpass_previous_output_[channel] = highpass;
        filtered = highpass;
    }
    if (parameters_.high_cut_hz > 0.0F) {
        repeat_lowpass_state_[channel] += repeat_lowpass_alpha_ *
            (filtered - repeat_lowpass_state_[channel]);
        filtered = repeat_lowpass_state_[channel];
    }
    return filtered;
}

float DuckingDelay::read_fractional_delay(float delay_samples,
                                          std::uint16_t channel) const noexcept {
    const float bounded_delay = std::clamp(
        delay_samples, 1.0F, static_cast<float>(capacity_frames_ - 2U));
    const auto whole = static_cast<std::size_t>(std::floor(bounded_delay));
    const float fraction = bounded_delay - static_cast<float>(whole);
    const float recent = read_age(whole, channel);
    const float previous = read_age(whole + 1U, channel);
    return recent + (previous - recent) * fraction;
}

float DuckingDelay::read_age(std::size_t age,
                             std::uint16_t channel) const noexcept {
    if (age == 0U || age >= capacity_frames_ || channel >= channels_) return 0.0F;
    const std::size_t read_frame =
        (write_frame_ + capacity_frames_ - age) % capacity_frames_;
    return storage_[read_frame * channels_ + channel];
}

void DuckingDelay::advance_ramps() noexcept {
    if (parameter_ramp_remaining_ != 0U) {
        --parameter_ramp_remaining_;
        if (parameter_ramp_remaining_ == 0U) {
            current_feedback_ = parameters_.feedback;
            current_mix_ = parameters_.mix;
        } else {
            current_feedback_ += feedback_step_;
            current_mix_ += mix_step_;
        }
    }
    if (delay_transition_remaining_ != 0U) {
        --delay_transition_remaining_;
        if (delay_transition_remaining_ == 0U) {
            delay_transition_from_samples_ = target_delay_samples_;
        }
    }
}

} // namespace tabletfx
