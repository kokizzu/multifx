#include "tabletfx/effects/plate_reverb.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr std::array<float, 8> kFeedbackDelayMs{
    17.13F, 21.79F, 26.47F, 29.73F, 33.19F, 37.47F, 41.11F, 43.73F};
constexpr std::array<float, 4> kDiffuserDelayMs{1.11F, 1.37F, 1.79F, 2.13F};
constexpr std::array<float, 8> kInputSigns{1.0F, -1.0F, 1.0F, 1.0F,
                                            -1.0F, 1.0F, -1.0F, -1.0F};
constexpr std::array<float, 8> kOutputSigns{1.0F, -1.0F, -1.0F, 1.0F,
                                             1.0F, -1.0F, 1.0F, -1.0F};
constexpr float kNormalizedLineGain = 0.35355339059F;
constexpr float kParameterSmoothingMs = 20.0F;
constexpr float kMaxInternalSample = 64.0F;
// Keep long feedback tails consistent across native and WebAssembly libm builds.
constexpr float kFeedbackCoefficientScale = 65536.0F;

float stable_coefficient(float value) noexcept {
    return std::round(value * kFeedbackCoefficientScale) /
           kFeedbackCoefficientScale;
}

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

PlateReverb::PlateReverb(float sample_rate, std::uint16_t channels,
                         PlateReverbParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
        sample_rate_ > 384000.0F) {
        throw std::invalid_argument("plate reverb sample rate must be between 8 kHz and 384 kHz");
    }
    if (channels_ != 1 && channels_ != 2) {
        throw std::invalid_argument("plate reverb supports mono or stereo only");
    }

    predelay_capacity_ = samples_for_ms(sample_rate_, 120.0F) + 2;
    std::size_t storage_samples = predelay_capacity_ * channels_;
    for (std::uint16_t channel = 0; channel < channels_; ++channel) {
        predelay_offset_[channel] = static_cast<std::size_t>(channel) * predelay_capacity_;
        const float channel_offset_ms = channel == 0 ? 0.0F : 0.37F;
        for (std::size_t index = 0; index < kDiffuserDelayMs.size(); ++index) {
            auto& line = diffusers_[static_cast<std::size_t>(channel) * kDiffuserCount + index];
            const float offset_ms = channel_offset_ms + static_cast<float>(index) * 0.035F;
            line.offset = storage_samples;
            line.length = samples_for_ms(sample_rate_, kDiffuserDelayMs[index] + offset_ms) + 1;
            storage_samples += line.length;
        }
        for (std::size_t index = 0; index < kFeedbackDelayMs.size(); ++index) {
            auto& line = feedback_lines_[static_cast<std::size_t>(channel) * kFeedbackLineCount + index];
            const float offset_ms = channel_offset_ms + static_cast<float>(index) * 0.047F;
            line.offset = storage_samples;
            line.length = samples_for_ms(sample_rate_, kFeedbackDelayMs[index] + offset_ms) + 1;
            storage_samples += line.length;
        }
    }
    storage_.assign(storage_samples, 0.0F);
    configure(parameters);
    settle_parameters();
}

std::size_t PlateReverb::memory_bytes() const noexcept {
    return sizeof(*this) + storage_.capacity() * sizeof(float);
}

void PlateReverb::configure(PlateReverbParameters parameters) noexcept {
    const PlateReverbParameters defaults{};
    parameters.decay_seconds = std::clamp(finite_or(parameters.decay_seconds,
                                                   defaults.decay_seconds),
                                          0.2F, 12.0F);
    parameters.pre_delay_ms = std::clamp(finite_or(parameters.pre_delay_ms,
                                                   defaults.pre_delay_ms),
                                          0.0F, 120.0F);
    const float maximum_damping_hz = std::min(18000.0F, sample_rate_ * 0.45F);
    parameters.damping_hz = std::clamp(finite_or(parameters.damping_hz,
                                                 defaults.damping_hz),
                                        200.0F, maximum_damping_hz);
    parameters.diffusion = std::clamp(finite_or(parameters.diffusion,
                                                defaults.diffusion),
                                       0.0F, 1.0F);
    parameters.balance = std::clamp(finite_or(parameters.balance,
                                              defaults.balance),
                                     0.0F, 1.0F);
    parameters.mix = std::clamp(finite_or(parameters.mix, defaults.mix), 0.0F, 1.0F);
    parameters.level_db = std::clamp(finite_or(parameters.level_db,
                                               defaults.level_db),
                                      -24.0F, 12.0F);
    parameters_ = parameters;
    update_targets();
}

void PlateReverb::update_targets() noexcept {
    target_damping_alpha_ = stable_coefficient(
        std::exp(-6.28318530718F * parameters_.damping_hz / sample_rate_));
    target_pre_delay_samples_ = parameters_.pre_delay_ms * sample_rate_ / 1000.0F;
    target_diffusion_ = parameters_.diffusion;
    target_balance_ = parameters_.balance;
    target_mix_ = parameters_.mix;
    target_level_ = std::pow(10.0F, parameters_.level_db / 20.0F);

    for (std::uint16_t channel = 0; channel < channels_; ++channel) {
        for (std::size_t index = 0; index < kFeedbackLineCount; ++index) {
            auto& line = feedback_lines_[static_cast<std::size_t>(channel) * kFeedbackLineCount + index];
            const float delay_seconds = static_cast<float>(line.length) / sample_rate_;
            const float target = std::min(
                0.995F, std::pow(10.0F, -3.0F * delay_seconds / parameters_.decay_seconds));
            line.feedback_target = stable_coefficient(target);
        }
    }

    const double ramp_samples = static_cast<double>(sample_rate_) *
                                static_cast<double>(kParameterSmoothingMs) / 1000.0;
    parameter_ramp_total_ = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::llround(ramp_samples)));
    parameter_ramp_remaining_ = parameter_ramp_total_;
    const float ramp = static_cast<float>(parameter_ramp_total_);
    damping_step_ = (target_damping_alpha_ - damping_alpha_) / ramp;
    pre_delay_step_ = (target_pre_delay_samples_ - pre_delay_samples_) / ramp;
    diffusion_step_ = (target_diffusion_ - diffusion_) / ramp;
    balance_step_ = (target_balance_ - balance_) / ramp;
    mix_step_ = (target_mix_ - mix_) / ramp;
    level_step_ = (target_level_ - level_) / ramp;
    for (auto& line : feedback_lines_) {
        line.feedback_step = (line.feedback_target - line.feedback) / ramp;
    }
}

void PlateReverb::advance_parameters() noexcept {
    if (parameter_ramp_remaining_ == 0) return;
    damping_alpha_ += damping_step_;
    pre_delay_samples_ += pre_delay_step_;
    diffusion_ += diffusion_step_;
    balance_ += balance_step_;
    mix_ += mix_step_;
    level_ += level_step_;
    for (auto& line : feedback_lines_) line.feedback += line.feedback_step;
    --parameter_ramp_remaining_;
    if (parameter_ramp_remaining_ == 0) settle_parameters();
}

void PlateReverb::settle_parameters() noexcept {
    damping_alpha_ = target_damping_alpha_;
    pre_delay_samples_ = target_pre_delay_samples_;
    diffusion_ = target_diffusion_;
    balance_ = target_balance_;
    mix_ = target_mix_;
    level_ = target_level_;
    for (auto& line : feedback_lines_) {
        line.feedback = line.feedback_target;
        line.feedback_step = 0.0F;
    }
    parameter_ramp_remaining_ = 0;
}

float PlateReverb::read_predelay(std::uint16_t channel, float input) noexcept {
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

float PlateReverb::process_diffuser(DelayLine& line, float input,
                                    float diffusion) noexcept {
    const float delayed = storage_[line.offset + line.index];
    const float coefficient = 0.35F + 0.55F * diffusion;
    const float allpass = delayed - coefficient * input;
    float write = input + coefficient * allpass;
    if (!std::isfinite(write)) write = 0.0F;
    storage_[line.offset + line.index] = std::clamp(write, -kMaxInternalSample,
                                                    kMaxInternalSample);
    line.index = line.index + 1 == line.length ? 0 : line.index + 1;
    const float output = input + diffusion * (allpass - input);
    return std::isfinite(output) ? output : 0.0F;
}

void PlateReverb::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;
    bypass_cleared_ = false;
    process_internal(interleaved, false);
}

void PlateReverb::process_bypassed(std::span<float> interleaved,
                                   std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;
    if (!parameters_.spillover) {
        if (!bypass_cleared_) reset();
        bypass_cleared_ = true;
        return;
    }
    bypass_cleared_ = false;
    process_internal(interleaved, true);
}

void PlateReverb::process_internal(std::span<float> interleaved,
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
            const float wet_input = read_predelay(channel, dry);
            float diffused = wet_input;
            const std::size_t diffuser_offset = static_cast<std::size_t>(channel) * kDiffuserCount;
            for (std::size_t index = 0; index < kDiffuserCount; ++index) {
                diffused = process_diffuser(diffusers_[diffuser_offset + index],
                                             diffused, diffusion_);
            }

            std::array<float, kFeedbackLineCount> filtered{};
            float feedback_sum = 0.0F;
            const std::size_t line_offset = static_cast<std::size_t>(channel) * kFeedbackLineCount;
            for (std::size_t index = 0; index < kFeedbackLineCount; ++index) {
                auto& line = feedback_lines_[line_offset + index];
                const float delayed = storage_[line.offset + line.index];
                line.damping_state = damping_alpha_ * line.damping_state +
                                     (1.0F - damping_alpha_) * delayed;
                filtered[index] = line.damping_state;
                feedback_sum += filtered[index];
            }

            float late = 0.0F;
            for (std::size_t index = 0; index < kFeedbackLineCount; ++index) {
                auto& line = feedback_lines_[line_offset + index];
                const float householder = filtered[index] - 0.25F * feedback_sum;
                float write = diffused * kInputSigns[index] * kNormalizedLineGain +
                              line.feedback * householder;
                if (!std::isfinite(write)) write = 0.0F;
                storage_[line.offset + line.index] = std::clamp(
                    write, -kMaxInternalSample, kMaxInternalSample);
                line.index = line.index + 1 == line.length ? 0 : line.index + 1;
                late += filtered[index] * kOutputSigns[index] * kNormalizedLineGain;
            }

            const float early = (diffused - wet_input) * 0.35F;
            const float wet = early * (1.0F - balance_) + late * balance_;
            const float output = bypassed ? passthrough + wet * mix_ * level_
                                          : dry * (1.0F - mix_) + wet * mix_ * level_;
            interleaved[sample_index] = std::isfinite(output) ? output : 0.0F;
        }
    }
}

void PlateReverb::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    predelay_index_.fill(0);
    for (auto& line : feedback_lines_) {
        line.index = 0;
        line.damping_state = 0.0F;
    }
    for (auto& line : diffusers_) line.index = 0;
    settle_parameters();
    bypass_cleared_ = true;
}

} // namespace tabletfx
