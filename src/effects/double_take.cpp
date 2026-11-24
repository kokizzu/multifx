#include "tabletfx/effects/double_take.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr std::uint32_t kVoiceSeeds[4] = {
    0x5a17c9e3U, 0x31b6a92dU, 0x4f1bbcdcU, 0x6d2b79f5U};
constexpr double kFloatMaximum =
    static_cast<double>(std::numeric_limits<float>::max());

float finite_float(double value) noexcept {
    return static_cast<float>(std::clamp(value, -kFloatMaximum, kFloatMaximum));
}

} // namespace

DoubleTake::DoubleTake(float sample_rate, std::uint16_t channels,
                       DoubleTakeParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument(
            "Double Take requires 8–384 kHz and mono or stereo channels");
    }

    const double requested_frames = std::ceil(
        static_cast<double>(sample_rate_) * kMaximumDelayMs / 1000.0);
    const std::size_t frame_limit = storage_.max_size() / channels_;
    if (!std::isfinite(requested_frames) || requested_frames < 1.0 ||
        requested_frames > static_cast<double>(frame_limit - 2U)) {
        throw std::length_error("Double Take delay buffer is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(requested_frames) + 2U;
    storage_.assign(capacity_frames_ * channels_, 0.0F);

    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(
                static_cast<double>(sample_rate_) * kControlRampMs / 1000.0)));
    attack_coefficient_ = std::exp(-1.0F / (0.005F * sample_rate_));
    release_coefficient_ = std::exp(-1.0F / (0.100F * sample_rate_));
    noise_phase_step_ = 1.0F / (0.080F * sample_rate_);

    set_parameters(parameters);
    reset();
}

void DoubleTake::set_parameters(DoubleTakeParameters parameters) noexcept {
    parameters.doubles = static_cast<std::uint8_t>(
        std::clamp<unsigned>(parameters.doubles, 1U,
                             static_cast<unsigned>(kVoiceCount)));
    parameters.slop = bounded(parameters.slop, 0.25F, 0.0F, 1.0F);
    parameters.sensitivity = bounded(parameters.sensitivity, 0.55F, 0.0F, 1.0F);
    parameters.dry_level = bounded(parameters.dry_level, 0.85F, 0.0F, 1.0F);
    parameters.wet_level = bounded(parameters.wet_level, 0.15F, 0.0F, 1.0F);

    switch (parameters.source) {
    case DoubleTakeSource::mono:
    case DoubleTakeSource::true_stereo:
    case DoubleTakeSource::left_only:
    case DoubleTakeSource::right_only:
        break;
    default:
        parameters.source = DoubleTakeSource::mono;
        break;
    }
    if (channels_ == 1) {
        parameters.source = DoubleTakeSource::mono;
    }

    parameters_ = parameters;
    retarget_controls();

    std::array<float, kVoiceCount> pans{};
    switch (parameters_.doubles) {
    case 1:
        pans = {0.0F, 0.0F, 0.0F, 0.0F};
        break;
    case 2:
        pans = {-0.65F, 0.65F, 0.0F, 0.0F};
        break;
    case 3:
        pans = {-0.82F, 0.0F, 0.82F, 0.0F};
        break;
    default:
        pans = {-0.90F, -0.30F, 0.30F, 0.90F};
        break;
    }
    for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
        const bool active = voice < parameters_.doubles;
        const float target = active ? 1.0F / static_cast<float>(parameters_.doubles)
                                    : 0.0F;
        voice_weight_step_[voice] =
            (target - voice_weight_[voice]) / static_cast<float>(ramp_total_);
        pan_left_[voice] = equal_power_left(pans[voice]);
        pan_right_[voice] = equal_power_right(pans[voice]);
    }
    ramp_remaining_ = ramp_total_;
}

void DoubleTake::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t local_frame = 0; local_frame < frames; ++local_frame) {
        advance_controls();

        std::array<float, 2> input{};
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = local_frame * channels_ + channel;
            input[channel] = std::isfinite(interleaved[index])
                                 ? interleaved[index]
                                 : 0.0F;
            storage_[write_frame_ * channels_ + channel] = input[channel];
        }

        float detector_input = std::abs(input[0]);
        if (channels_ == 2) {
            switch (parameters_.source) {
            case DoubleTakeSource::mono:
                detector_input = std::abs(0.5F * input[0] + 0.5F * input[1]);
                break;
            case DoubleTakeSource::true_stereo:
                detector_input = std::max(std::abs(input[0]), std::abs(input[1]));
                break;
            case DoubleTakeSource::left_only:
                detector_input = std::abs(input[0]);
                break;
            case DoubleTakeSource::right_only:
                detector_input = std::abs(input[1]);
                break;
            }
        }
        const float detector_coefficient = detector_input > envelope_
                                               ? attack_coefficient_
                                               : release_coefficient_;
        envelope_ = detector_coefficient * envelope_ +
                    (1.0F - detector_coefficient) * detector_input;
        const float activity = dynamic_activity();

        double wet_left = 0.0;
        double wet_right = 0.0;
        double wet_mono = 0.0;
        for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
            const float weight = voice_weight_[voice];
            const float movement = current_slop_ * kMaximumSlopMs * activity *
                                   noise_value(voice);
            const float delay_ms = kBaseDelayMs +
                                   static_cast<float>(voice) * kVoiceSpacingMs +
                                   movement;
            const float delay_samples = std::clamp(
                delay_ms * sample_rate_ * 0.001F, 1.0F,
                static_cast<float>(capacity_frames_ - 2U));

            if (channels_ == 1) {
                const float voice_sample = read_delay(
                    write_frame_, delay_samples, 0);
                wet_mono += static_cast<double>(voice_sample) * weight;
                continue;
            }

            if (parameters_.source == DoubleTakeSource::true_stereo) {
                wet_left += static_cast<double>(read_delay(
                                write_frame_, delay_samples, 0)) * weight;
                wet_right += static_cast<double>(read_delay(
                                 write_frame_, delay_samples, 1)) * weight;
                continue;
            }

            float voice_sample = 0.0F;
            switch (parameters_.source) {
            case DoubleTakeSource::mono:
                voice_sample = 0.5F * read_delay(write_frame_, delay_samples, 0) +
                               0.5F * read_delay(write_frame_, delay_samples, 1);
                break;
            case DoubleTakeSource::left_only:
                voice_sample = read_delay(write_frame_, delay_samples, 0);
                break;
            case DoubleTakeSource::right_only:
                voice_sample = read_delay(write_frame_, delay_samples, 1);
                break;
            case DoubleTakeSource::true_stereo:
                break;
            }
            wet_left += static_cast<double>(voice_sample) * weight * pan_left_[voice];
            wet_right += static_cast<double>(voice_sample) * weight * pan_right_[voice];
        }

        if (channels_ == 1) {
            const double output = static_cast<double>(input[0]) * current_dry_level_ +
                                  wet_mono * current_wet_level_;
            interleaved[local_frame] = finite_float(output);
        } else {
            const double output_left =
                static_cast<double>(input[0]) * current_dry_level_ +
                wet_left * current_wet_level_;
            const double output_right =
                static_cast<double>(input[1]) * current_dry_level_ +
                wet_right * current_wet_level_;
            interleaved[local_frame * 2U] = finite_float(output_left);
            interleaved[local_frame * 2U + 1U] = finite_float(output_right);
        }

        ++write_frame_;
        if (write_frame_ == capacity_frames_) {
            write_frame_ = 0;
        }
        advance_noise();
    }
}

void DoubleTake::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
    envelope_ = 0.0F;

    const float initial_weights =
        1.0F / static_cast<float>(parameters_.doubles);
    for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
        voice_weight_[voice] = voice < parameters_.doubles ? initial_weights : 0.0F;
        voice_weight_step_[voice] = 0.0F;
        random_state_[voice] = kVoiceSeeds[voice];
        noise_current_[voice] = random_value(random_state_[voice]);
        noise_target_[voice] = random_value(random_state_[voice]);
        noise_phase_[voice] = 0.0F;
    }

    current_slop_ = parameters_.slop;
    current_sensitivity_ = parameters_.sensitivity;
    current_dry_level_ = parameters_.dry_level;
    current_wet_level_ = parameters_.wet_level;
    slop_step_ = 0.0F;
    sensitivity_step_ = 0.0F;
    dry_level_step_ = 0.0F;
    wet_level_step_ = 0.0F;
    ramp_remaining_ = 0;
}

float DoubleTake::bounded(float value, float fallback,
                          float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float DoubleTake::random_value(std::uint32_t& state) noexcept {
    state = state * 1664525U + 1013904223U;
    const float unit = static_cast<float>(state >> 8U) / 16777215.0F;
    return unit * 2.0F - 1.0F;
}

float DoubleTake::equal_power_left(float pan) noexcept {
    const float angle = (pan + 1.0F) * (kPi * 0.25F);
    return std::cos(angle);
}

float DoubleTake::equal_power_right(float pan) noexcept {
    const float angle = (pan + 1.0F) * (kPi * 0.25F);
    return std::sin(angle);
}

float DoubleTake::read_delay(std::size_t frame, float delay_samples,
                             std::uint16_t channel) const noexcept {
    const std::size_t whole_delay = static_cast<std::size_t>(delay_samples);
    const float fraction = delay_samples - static_cast<float>(whole_delay);
    std::size_t newer_frame = frame + capacity_frames_ - whole_delay;
    if (newer_frame >= capacity_frames_) {
        newer_frame -= capacity_frames_;
    }
    const std::size_t older_frame = newer_frame == 0
                                        ? capacity_frames_ - 1U
                                        : newer_frame - 1U;
    const float newer = storage_[newer_frame * channels_ + channel];
    const float older = storage_[older_frame * channels_ + channel];
    return finite_float(static_cast<double>(newer) * (1.0 - fraction) +
                        static_cast<double>(older) * fraction);
}

float DoubleTake::dynamic_activity() const noexcept {
    const float threshold = 0.01F + (1.0F - current_sensitivity_) * 0.50F;
    return std::clamp((envelope_ - threshold) / 0.25F, 0.0F, 1.0F);
}

float DoubleTake::noise_value(std::size_t voice) const noexcept {
    const float phase = noise_phase_[voice];
    const float smooth = phase * phase * (3.0F - 2.0F * phase);
    return noise_current_[voice] +
           (noise_target_[voice] - noise_current_[voice]) * smooth;
}

void DoubleTake::retarget_controls() noexcept {
    const float denominator = static_cast<float>(ramp_total_);
    slop_step_ = (parameters_.slop - current_slop_) / denominator;
    sensitivity_step_ =
        (parameters_.sensitivity - current_sensitivity_) / denominator;
    dry_level_step_ = (parameters_.dry_level - current_dry_level_) / denominator;
    wet_level_step_ = (parameters_.wet_level - current_wet_level_) / denominator;
}

void DoubleTake::advance_controls() noexcept {
    if (ramp_remaining_ == 0) {
        return;
    }
    current_slop_ += slop_step_;
    current_sensitivity_ += sensitivity_step_;
    current_dry_level_ += dry_level_step_;
    current_wet_level_ += wet_level_step_;
    for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
        voice_weight_[voice] += voice_weight_step_[voice];
    }
    --ramp_remaining_;
    if (ramp_remaining_ == 0) {
        current_slop_ = parameters_.slop;
        current_sensitivity_ = parameters_.sensitivity;
        current_dry_level_ = parameters_.dry_level;
        current_wet_level_ = parameters_.wet_level;
        const float weight = 1.0F / static_cast<float>(parameters_.doubles);
        for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
            voice_weight_[voice] = voice < parameters_.doubles ? weight : 0.0F;
            voice_weight_step_[voice] = 0.0F;
        }
        slop_step_ = 0.0F;
        sensitivity_step_ = 0.0F;
        dry_level_step_ = 0.0F;
        wet_level_step_ = 0.0F;
    }
}

void DoubleTake::advance_noise() noexcept {
    for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
        noise_phase_[voice] += noise_phase_step_;
        if (noise_phase_[voice] >= 1.0F) {
            noise_phase_[voice] -= 1.0F;
            noise_current_[voice] = noise_target_[voice];
            noise_target_[voice] = random_value(random_state_[voice]);
        }
    }
}

} // namespace tabletfx
