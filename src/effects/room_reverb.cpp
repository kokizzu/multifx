#include "tabletfx/effects.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr std::array<float, 4> kSmallRoomCombMs{29.7F, 33.1F, 37.9F, 41.1F};
constexpr std::array<float, 4> kLargeRoomCombMs{43.7F, 48.3F, 53.1F, 58.7F};
constexpr std::array<float, 2> kDiffuserMs{5.3F, 7.8F};
constexpr float kStereoOffsetMs = 0.83F;
constexpr float kDiffuserCoefficient = 0.68F;
constexpr float kParameterSmoothingMs = 20.0F;
constexpr float kMaxInternalSample = 64.0F;

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

RoomReverb::RoomReverb(float sample_rate, std::uint16_t channels,
                       RoomReverbParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
        sample_rate_ > 384000.0F) {
        throw std::invalid_argument("room reverb sample rate must be between 8 kHz and 384 kHz");
    }
    if (channels_ != 1 && channels_ != 2) {
        throw std::invalid_argument("room reverb supports mono or stereo only");
    }

    predelay_capacity_ = samples_for_ms(sample_rate_, 80.0F) + 2;
    std::size_t storage_samples = predelay_capacity_ * channels_;
    for (std::uint16_t channel = 0; channel < channels_; ++channel) {
        predelay_offset_[channel] = static_cast<std::size_t>(channel) * predelay_capacity_;
        const float stereo_offset = channel == 0 ? 0.0F : kStereoOffsetMs;
        for (std::size_t index = 0; index < kLargeRoomCombMs.size(); ++index) {
            auto& line = combs_[static_cast<std::size_t>(channel) * 4 + index];
            line.offset = storage_samples;
            line.length = samples_for_ms(sample_rate_, kLargeRoomCombMs[index] + stereo_offset) + 1;
            storage_samples += line.length;
        }
        for (std::size_t index = 0; index < kDiffuserMs.size(); ++index) {
            auto& line = diffusers_[static_cast<std::size_t>(channel) * 2 + index];
            line.offset = storage_samples;
            line.length = samples_for_ms(sample_rate_, kDiffuserMs[index] + stereo_offset) + 1;
            storage_samples += line.length;
        }
    }
    storage_.assign(storage_samples, 0.0F);
    configure_size(RoomSize::small);
    configure(parameters);
    // Preset construction starts at the selected values; it must not fade in
    // from the defaults or carry an artificial parameter ramp into the first block.
    decay_seconds_ = target_decay_seconds_;
    damping_alpha_ = target_damping_alpha_;
    pre_delay_samples_ = target_pre_delay_samples_;
    wet_mix_ = target_wet_mix_;
    wet_level_ = target_wet_level_;
    for (auto& line : combs_) {
        line.feedback = line.feedback_target;
        line.feedback_step = 0.0F;
    }
    parameter_ramp_remaining_ = 0;
}

std::size_t RoomReverb::memory_bytes() const noexcept {
    return sizeof(*this) + storage_.capacity() * sizeof(float);
}

void RoomReverb::configure_size(RoomSize size) noexcept {
    const bool large = size == RoomSize::large;
    const auto& delays = large ? kLargeRoomCombMs : kSmallRoomCombMs;
    for (std::uint16_t channel = 0; channel < channels_; ++channel) {
        const float stereo_offset = channel == 0 ? 0.0F : kStereoOffsetMs;
        for (std::size_t index = 0; index < delays.size(); ++index) {
            auto& line = combs_[static_cast<std::size_t>(channel) * 4 + index];
            line.length = samples_for_ms(sample_rate_, delays[index] + stereo_offset);
            line.index %= line.length;
        }
    }
}

void RoomReverb::update_targets() noexcept {
    const float target_damping_hz = std::clamp(parameters_.damping_hz, 200.0F,
                                                std::min(18000.0F, sample_rate_ * 0.45F));
    target_damping_alpha_ = std::exp(-6.28318530718F * target_damping_hz / sample_rate_);
    target_pre_delay_samples_ = parameters_.pre_delay_ms * sample_rate_ / 1000.0F;
    target_wet_mix_ = parameters_.mix;
    target_wet_level_ = std::pow(10.0F, parameters_.level_db / 20.0F);

    const float decay = parameters_.decay_seconds;
    for (std::uint16_t channel = 0; channel < channels_; ++channel) {
        for (std::size_t index = 0; index < 4; ++index) {
            auto& line = combs_[static_cast<std::size_t>(channel) * 4 + index];
            const float delay_seconds = static_cast<float>(line.length) / sample_rate_;
            line.feedback_target = std::min(
                0.995F, std::pow(10.0F, -3.0F * delay_seconds / decay));
        }
    }

    const double ramp_samples = static_cast<double>(sample_rate_) *
                                static_cast<double>(kParameterSmoothingMs) / 1000.0;
    parameter_ramp_total_ = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::llround(ramp_samples)));
    parameter_ramp_remaining_ = parameter_ramp_total_;
    const float ramp = static_cast<float>(parameter_ramp_total_);
    decay_step_ = (target_decay_seconds_ - decay_seconds_) / ramp;
    damping_step_ = (target_damping_alpha_ - damping_alpha_) / ramp;
    predelay_step_ = (target_pre_delay_samples_ - pre_delay_samples_) / ramp;
    mix_step_ = (target_wet_mix_ - wet_mix_) / ramp;
    level_step_ = (target_wet_level_ - wet_level_) / ramp;
    for (auto& line : combs_) {
        line.feedback_step = (line.feedback_target - line.feedback) / ramp;
    }
}

void RoomReverb::configure(RoomReverbParameters parameters) noexcept {
    if (parameters.size != RoomSize::small && parameters.size != RoomSize::large) {
        parameters.size = RoomSize::small;
    }
    parameters.decay_seconds = std::clamp(finite_or(parameters.decay_seconds, 0.8F),
                                          0.15F, 8.0F);
    parameters.pre_delay_ms = std::clamp(finite_or(parameters.pre_delay_ms, 10.0F),
                                         0.0F, 80.0F);
    const float maximum_damping_hz = std::min(18000.0F, sample_rate_ * 0.45F);
    parameters.damping_hz = std::clamp(finite_or(parameters.damping_hz, 6000.0F),
                                       200.0F, maximum_damping_hz);
    parameters.mix = std::clamp(finite_or(parameters.mix, 0.2F), 0.0F, 1.0F);
    parameters.level_db = std::clamp(finite_or(parameters.level_db, 0.0F), -24.0F, 12.0F);
    const bool size_changed = parameters.size != parameters_.size;
    parameters_ = parameters;
    if (size_changed) {
        configure_size(parameters_.size);
    }
    target_decay_seconds_ = parameters_.decay_seconds;
    update_targets();
}

void RoomReverb::advance_parameters() noexcept {
    if (parameter_ramp_remaining_ == 0) return;
    decay_seconds_ += decay_step_;
    damping_alpha_ += damping_step_;
    pre_delay_samples_ += predelay_step_;
    wet_mix_ += mix_step_;
    wet_level_ += level_step_;
    for (auto& line : combs_) line.feedback += line.feedback_step;
    --parameter_ramp_remaining_;
    if (parameter_ramp_remaining_ == 0) {
        decay_seconds_ = target_decay_seconds_;
        damping_alpha_ = target_damping_alpha_;
        pre_delay_samples_ = target_pre_delay_samples_;
        wet_mix_ = target_wet_mix_;
        wet_level_ = target_wet_level_;
        for (auto& line : combs_) line.feedback = line.feedback_target;
    }
}

float RoomReverb::read_predelay(std::uint16_t channel, float input) noexcept {
    const std::size_t offset = predelay_offset_[channel];
    const std::size_t index = predelay_index_[channel];
    storage_[offset + index] = input;
    const float delay = std::clamp(pre_delay_samples_, 0.0F,
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

float RoomReverb::process_comb(DelayLine& line, float input,
                               float damping_alpha) noexcept {
    const float delayed = storage_[line.offset + line.index];
    line.damping_state = damping_alpha * line.damping_state +
                         (1.0F - damping_alpha) * delayed;
    float write = input + line.feedback * line.damping_state;
    if (!std::isfinite(write)) write = 0.0F;
    write = std::clamp(write, -kMaxInternalSample, kMaxInternalSample);
    storage_[line.offset + line.index] = write;
    line.index = line.index + 1 == line.length ? 0 : line.index + 1;
    return delayed;
}

float RoomReverb::process_diffuser(DelayLine& line, float input) noexcept {
    const float delayed = storage_[line.offset + line.index];
    const float output = delayed - kDiffuserCoefficient * input;
    float write = input + kDiffuserCoefficient * output;
    if (!std::isfinite(write)) write = 0.0F;
    storage_[line.offset + line.index] =
        std::clamp(write, -kMaxInternalSample, kMaxInternalSample);
    line.index = line.index + 1 == line.length ? 0 : line.index + 1;
    return std::isfinite(output) ? output : 0.0F;
}

void RoomReverb::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;
    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        advance_parameters();
        const std::size_t frame_offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const std::size_t sample_index = frame_offset + channel;
            const float dry = std::isfinite(interleaved[sample_index])
                                  ? std::clamp(interleaved[sample_index], -8.0F, 8.0F)
                                  : 0.0F;
            const float input = read_predelay(channel, dry);
            float wet = 0.0F;
            const std::size_t comb_offset = static_cast<std::size_t>(channel) * 4;
            for (std::size_t index = 0; index < 4; ++index) {
                wet += process_comb(combs_[comb_offset + index], input, damping_alpha_);
            }
            wet *= 0.25F;
            const std::size_t diffuser_offset = static_cast<std::size_t>(channel) * 2;
            wet = process_diffuser(diffusers_[diffuser_offset], wet);
            wet = process_diffuser(diffusers_[diffuser_offset + 1], wet);
            const float output = dry * (1.0F - wet_mix_) +
                                 wet * wet_mix_ * wet_level_;
            interleaved[sample_index] = std::isfinite(output) ? output : 0.0F;
        }
    }
}

void RoomReverb::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    predelay_index_.fill(0);
    for (auto& line : combs_) {
        line.index = 0;
        line.damping_state = 0.0F;
        line.feedback = line.feedback_target;
        line.feedback_step = 0.0F;
    }
    for (auto& line : diffusers_) line.index = 0;
    decay_seconds_ = target_decay_seconds_;
    damping_alpha_ = target_damping_alpha_;
    pre_delay_samples_ = target_pre_delay_samples_;
    wet_mix_ = target_wet_mix_;
    wet_level_ = target_wet_level_;
    parameter_ramp_remaining_ = 0;
}

} // namespace tabletfx
