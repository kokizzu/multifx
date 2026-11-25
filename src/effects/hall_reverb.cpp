#include "tabletfx/effects/hall_reverb.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr std::array<float, 6> kHallCombMs{47.3F, 51.7F, 57.1F,
                                           61.3F, 67.9F, 71.9F};
constexpr std::array<float, 2> kHallDiffuserMs{8.9F, 13.8F};
constexpr float kStereoDelayOffsetMs = 0.61F;
constexpr float kDiffuserCoefficient = 0.72F;
constexpr float kParameterSmoothingMs = 20.0F;
constexpr float kMaxInternalSample = 64.0F;
constexpr float kPiTimesTwo = 6.28318530718F;

std::size_t samples_for_ms(float sample_rate, float milliseconds) noexcept {
    const double samples = static_cast<double>(sample_rate) *
                           static_cast<double>(milliseconds) / 1000.0;
    return std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(samples)));
}

float finite_or(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}

std::size_t wrap_subtract(std::size_t index, std::size_t amount,
                          std::size_t capacity) noexcept {
    amount %= capacity;
    return index >= amount ? index - amount : capacity - (amount - index);
}

} // namespace

HallReverb::HallReverb(float sample_rate, std::uint16_t channels,
                       HallReverbParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
        sample_rate_ > 192000.0F) {
        throw std::invalid_argument("Hall reverb sample rate must be between 8 kHz and 192 kHz");
    }
    if (channels_ != 1 && channels_ != 2) {
        throw std::invalid_argument("Hall reverb supports mono or stereo only");
    }

    predelay_capacity_ = samples_for_ms(sample_rate_, 120.0F) + 2;
    std::size_t storage_samples = predelay_capacity_ * channels_;
    for (std::uint16_t channel = 0; channel < channels_; ++channel) {
        predelay_offset_[channel] = static_cast<std::size_t>(channel) * predelay_capacity_;
        const float stereo_offset = channel == 0 ? 0.0F : kStereoDelayOffsetMs;
        for (std::size_t index = 0; index < kHallCombMs.size(); ++index) {
            auto& line = combs_[static_cast<std::size_t>(channel) * kHallCombMs.size() + index];
            line.offset = storage_samples;
            line.length = samples_for_ms(sample_rate_, kHallCombMs[index] + stereo_offset);
            storage_samples += line.length;
        }
        for (std::size_t index = 0; index < kHallDiffuserMs.size(); ++index) {
            auto& line = diffusers_[static_cast<std::size_t>(channel) * kHallDiffuserMs.size() + index];
            line.offset = storage_samples;
            line.length = samples_for_ms(sample_rate_, kHallDiffuserMs[index] + stereo_offset);
            storage_samples += line.length;
        }
    }
    storage_.assign(storage_samples, 0.0F);
    configure(parameters);

    // Presets start at their saved settings without a parameter transition.
    highpass_alpha_ = target_highpass_alpha_;
    highcut_alpha_ = target_highcut_alpha_;
    predelay_samples_ = target_predelay_samples_;
    mix_ = target_mix_;
    level_ = target_level_;
    for (auto& line : combs_) {
        line.feedback = line.feedback_target;
        line.feedback_step = 0.0F;
    }
    parameter_ramp_remaining_ = 0;
}

std::size_t HallReverb::memory_bytes() const noexcept {
    return sizeof(*this) + storage_.capacity() * sizeof(float);
}

void HallReverb::configure(HallReverbParameters parameters) noexcept {
    parameters.decay_seconds = std::clamp(finite_or(parameters.decay_seconds, 3.2F),
                                          0.5F, 12.0F);
    parameters.pre_delay_ms = std::clamp(finite_or(parameters.pre_delay_ms, 32.0F),
                                         0.0F, 120.0F);
    parameters.low_cut_hz = std::clamp(
        finite_or(parameters.low_cut_hz, 100.0F), 20.0F,
        std::min(1500.0F, sample_rate_ * 0.2F));
    parameters.damping_hz = std::clamp(
        finite_or(parameters.damping_hz, 9000.0F), 200.0F,
        std::min(18000.0F, sample_rate_ * 0.45F));
    parameters.mix = std::clamp(finite_or(parameters.mix, 0.25F), 0.0F, 1.0F);
    parameters.level_db = std::clamp(finite_or(parameters.level_db, 0.0F), -24.0F, 12.0F);
    parameters_ = parameters;
    target_decay_seconds_ = parameters_.decay_seconds;
    update_targets();
}

void HallReverb::update_targets() noexcept {
    target_highpass_alpha_ =
        std::exp(-kPiTimesTwo * parameters_.low_cut_hz / sample_rate_);
    target_highcut_alpha_ =
        std::exp(-kPiTimesTwo * parameters_.damping_hz / sample_rate_);
    target_predelay_samples_ = parameters_.pre_delay_ms * sample_rate_ / 1000.0F;
    target_mix_ = parameters_.mix;
    target_level_ = std::pow(10.0F, parameters_.level_db / 20.0F);

    for (std::uint16_t channel = 0; channel < channels_; ++channel) {
        for (std::size_t index = 0; index < kHallCombMs.size(); ++index) {
            auto& line = combs_[static_cast<std::size_t>(channel) * kHallCombMs.size() + index];
            const float delay_seconds = static_cast<float>(line.length) / sample_rate_;
            line.feedback_target = std::min(
                0.995F, std::pow(10.0F, -3.0F * delay_seconds / target_decay_seconds_));
        }
    }

    const double ramp_samples = static_cast<double>(sample_rate_) *
                                static_cast<double>(kParameterSmoothingMs) / 1000.0;
    parameter_ramp_total_ = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::llround(ramp_samples)));
    parameter_ramp_remaining_ = parameter_ramp_total_;
    const float ramp = static_cast<float>(parameter_ramp_total_);
    highpass_step_ = (target_highpass_alpha_ - highpass_alpha_) / ramp;
    highcut_step_ = (target_highcut_alpha_ - highcut_alpha_) / ramp;
    predelay_step_ = (target_predelay_samples_ - predelay_samples_) / ramp;
    mix_step_ = (target_mix_ - mix_) / ramp;
    level_step_ = (target_level_ - level_) / ramp;
    for (auto& line : combs_) {
        line.feedback_step = (line.feedback_target - line.feedback) / ramp;
    }
}

void HallReverb::advance_parameters() noexcept {
    if (parameter_ramp_remaining_ == 0) return;
    highpass_alpha_ += highpass_step_;
    highcut_alpha_ += highcut_step_;
    predelay_samples_ += predelay_step_;
    mix_ += mix_step_;
    level_ += level_step_;
    for (auto& line : combs_) line.feedback += line.feedback_step;
    --parameter_ramp_remaining_;
    if (parameter_ramp_remaining_ == 0) {
        highpass_alpha_ = target_highpass_alpha_;
        highcut_alpha_ = target_highcut_alpha_;
        predelay_samples_ = target_predelay_samples_;
        mix_ = target_mix_;
        level_ = target_level_;
        for (auto& line : combs_) line.feedback = line.feedback_target;
    }
}

float HallReverb::read_predelay(std::uint16_t channel, float input) noexcept {
    const std::size_t offset = predelay_offset_[channel];
    const std::size_t index = predelay_index_[channel];
    storage_[offset + index] = input;
    const float delay = std::clamp(predelay_samples_, 0.0F,
                                   static_cast<float>(predelay_capacity_ - 2));
    const auto whole = static_cast<std::size_t>(delay);
    const float fraction = delay - static_cast<float>(whole);
    const std::size_t newer = wrap_subtract(index, whole, predelay_capacity_);
    const std::size_t older = wrap_subtract(index, whole + 1, predelay_capacity_);
    const float result = storage_[offset + newer] * (1.0F - fraction) +
                         storage_[offset + older] * fraction;
    predelay_index_[channel] = index + 1 == predelay_capacity_ ? 0 : index + 1;
    return result;
}

float HallReverb::process_comb(DelayLine& line, float input) noexcept {
    const float delayed = storage_[line.offset + line.index];
    line.damping_state = highcut_alpha_ * line.damping_state +
                         (1.0F - highcut_alpha_) * delayed;
    float write = input + line.feedback * line.damping_state;
    if (!std::isfinite(write)) write = 0.0F;
    write = std::clamp(write, -kMaxInternalSample, kMaxInternalSample);
    storage_[line.offset + line.index] = write;
    line.index = line.index + 1 == line.length ? 0 : line.index + 1;
    return delayed;
}

float HallReverb::process_diffuser(DelayLine& line, float input) noexcept {
    const float delayed = storage_[line.offset + line.index];
    const float output = delayed - kDiffuserCoefficient * input;
    float write = input + kDiffuserCoefficient * output;
    if (!std::isfinite(write)) write = 0.0F;
    storage_[line.offset + line.index] =
        std::clamp(write, -kMaxInternalSample, kMaxInternalSample);
    line.index = line.index + 1 == line.length ? 0 : line.index + 1;
    return std::isfinite(output) ? output : 0.0F;
}

void HallReverb::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    if (channels == 0 || channels != channels_) return;
    bypass_cleared_ = false;
    process_internal(interleaved, false);
}

void HallReverb::process_bypassed(std::span<float> interleaved,
                                  std::uint16_t channels) noexcept {
    if (channels == 0 || channels != channels_) return;
    if (!parameters_.spillover) {
        if (!bypass_cleared_) reset();
        bypass_cleared_ = true;
        return;
    }
    bypass_cleared_ = false;
    process_internal(interleaved, true);
}

void HallReverb::process_internal(std::span<float> interleaved,
                                  bool bypassed) noexcept {
    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        advance_parameters();
        const std::size_t frame_offset = frame * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t sample_index = frame_offset + channel;
            const float passthrough = std::isfinite(interleaved[sample_index])
                                          ? interleaved[sample_index]
                                          : 0.0F;
            const float dry = bypassed ? 0.0F : std::clamp(passthrough, -8.0F, 8.0F);
            const float highpassed = highpass_alpha_ *
                                     (highpass_previous_[channel] + dry -
                                      input_previous_[channel]);
            input_previous_[channel] = dry;
            highpass_previous_[channel] = std::isfinite(highpassed) ? highpassed : 0.0F;
            const float input = read_predelay(channel, highpass_previous_[channel]);

            float wet = 0.0F;
            const std::size_t comb_offset = static_cast<std::size_t>(channel) * kHallCombMs.size();
            for (std::size_t index = 0; index < kHallCombMs.size(); ++index) {
                wet += process_comb(combs_[comb_offset + index], input);
            }
            wet /= static_cast<float>(kHallCombMs.size());
            const std::size_t diffuser_offset =
                static_cast<std::size_t>(channel) * kHallDiffuserMs.size();
            wet = process_diffuser(diffusers_[diffuser_offset], wet);
            wet = process_diffuser(diffusers_[diffuser_offset + 1], wet);

            const float output = bypassed ? passthrough + wet * mix_ * level_
                                          : dry * (1.0F - mix_) + wet * mix_ * level_;
            interleaved[sample_index] = std::isfinite(output) ? output : 0.0F;
        }
    }
}

void HallReverb::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    predelay_index_.fill(0);
    input_previous_.fill(0.0F);
    highpass_previous_.fill(0.0F);
    for (auto& line : combs_) {
        line.index = 0;
        line.damping_state = 0.0F;
        line.feedback = line.feedback_target;
        line.feedback_step = 0.0F;
    }
    for (auto& line : diffusers_) line.index = 0;
    highpass_alpha_ = target_highpass_alpha_;
    highcut_alpha_ = target_highcut_alpha_;
    predelay_samples_ = target_predelay_samples_;
    mix_ = target_mix_;
    level_ = target_level_;
    parameter_ramp_remaining_ = 0;
    bypass_cleared_ = true;
}

} // namespace tabletfx
