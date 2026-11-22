#include "tabletfx/effects/cloud_reverb.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kTwoPi = 6.28318530718F;
constexpr float kInvSqrtEight = 0.35355339059F;
constexpr float kInternalLimit = 64.0F;
constexpr std::array<float, 8> kDelayMs{
    23.1F, 28.3F, 31.9F, 37.7F, 41.3F, 47.9F, 53.1F, 59.7F};
constexpr std::array<float, 8> kLfoOffsets{
    0.0F, 0.31F, 0.79F, 1.47F, 2.08F, 2.81F, 3.74F, 4.92F};
constexpr std::array<float, 8> kInputSigns{
    1.0F, -1.0F, 1.0F, -1.0F, -1.0F, 1.0F, -1.0F, 1.0F};
constexpr std::array<float, 8> kOutputSigns{
    1.0F, 1.0F, -1.0F, -1.0F, 1.0F, -1.0F, -1.0F, 1.0F};
constexpr std::array<float, 2> kDiffuserMs{2.7F, 4.1F};
constexpr std::array<float, 2> kChannelDelayOffsetMs{0.0F, 0.43F};

float checked_sample_rate(float sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate < 8000.0F ||
        sample_rate > 384000.0F) {
        throw std::invalid_argument(
            "Cloud Reverb sample rate must be between 8 kHz and 384 kHz");
    }
    return sample_rate;
}

std::uint16_t checked_channels(std::uint16_t channels) {
    if (channels != 1 && channels != 2) {
        throw std::invalid_argument(
            "Cloud Reverb supports mono or stereo only");
    }
    return channels;
}

std::size_t samples_for_ms(float sample_rate, float milliseconds) noexcept {
    const double samples = static_cast<double>(sample_rate) *
                           static_cast<double>(milliseconds) / 1000.0;
    return std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(samples)));
}

float finite_or(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}

float finite_sample(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, -16.0F, 16.0F) : 0.0F;
}

} // namespace

CloudReverb::CloudReverb(float sample_rate, CloudReverbParameters parameters)
    : CloudReverb(sample_rate, std::uint16_t{2}, parameters) {}

CloudReverb::CloudReverb(float sample_rate, std::uint16_t channels,
                         CloudReverbParameters parameters)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)) {
    std::size_t storage_samples = 0;
    for (std::size_t channel = 0; channel < channels_; ++channel) {
        for (std::size_t index = 0; index < kDiffuserCount; ++index) {
            auto& line = diffusers_[channel * kDiffuserCount + index];
            line.offset = storage_samples;
            line.length = samples_for_ms(sample_rate_,
                                          kDiffuserMs[index] +
                                              kChannelDelayOffsetMs[channel]) + 1U;
            line.coefficient = index == 0 ? 0.71F : -0.63F;
            storage_samples += line.length;
        }
        for (std::size_t index = 0; index < kLinesPerChannel; ++index) {
            auto& line = lines_[channel * kLinesPerChannel + index];
            const float nominal_ms = kDelayMs[index] +
                                     kChannelDelayOffsetMs[channel];
            line.offset = storage_samples;
            line.nominal_delay_samples = static_cast<float>(
                samples_for_ms(sample_rate_, nominal_ms));
            line.length = samples_for_ms(
                              sample_rate_, nominal_ms +
                                                kMaximumModulationDepthMs + 1.0F) +
                          2U;
            storage_samples += line.length;
        }
    }
    storage_.assign(storage_samples, 0.0F);
    const double ramp_samples = static_cast<double>(sample_rate_) *
                                static_cast<double>(kControlSmoothingMs) / 1000.0;
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(ramp_samples)));
    configure(parameters);
    settle_parameters();
}

std::size_t CloudReverb::memory_bytes() const noexcept {
    return sizeof(*this) + storage_.capacity() * sizeof(float);
}

void CloudReverb::configure(CloudReverbParameters parameters) noexcept {
    const CloudReverbParameters defaults{};
    const float maximum_cut_hz = std::min(20000.0F, sample_rate_ * 0.45F);
    parameters.mix = std::clamp(finite_or(parameters.mix, defaults.mix),
                                0.0F, 1.0F);
    parameters.tone = std::clamp(finite_or(parameters.tone, defaults.tone),
                                 0.0F, 1.0F);
    parameters.decay_seconds = std::clamp(
        finite_or(parameters.decay_seconds, defaults.decay_seconds), 0.2F, 20.0F);
    parameters.diffusion = std::clamp(
        finite_or(parameters.diffusion, defaults.diffusion), 0.0F, 1.0F);
    parameters.high_damp_hz = std::clamp(
        finite_or(parameters.high_damp_hz, defaults.high_damp_hz),
        1000.0F, maximum_cut_hz);
    parameters.low_damp_hz = std::clamp(
        finite_or(parameters.low_damp_hz, defaults.low_damp_hz), 20.0F, 1000.0F);
    parameters.modulation_rate_hz = std::clamp(
        finite_or(parameters.modulation_rate_hz, defaults.modulation_rate_hz),
        0.05F, 8.0F);
    parameters.modulation_depth = std::clamp(
        finite_or(parameters.modulation_depth, defaults.modulation_depth),
        0.0F, 1.0F);

    const float tone_cut_hz = 500.0F * std::pow(
        maximum_cut_hz / 500.0F, parameters.tone);
    const float target_tone_alpha = std::exp(
        -kTwoPi * tone_cut_hz / sample_rate_);
    const float target_high_damp_alpha = std::exp(
        -kTwoPi * parameters.high_damp_hz / sample_rate_);
    const float target_low_damp_alpha = std::exp(
        -kTwoPi * parameters.low_damp_hz / sample_rate_);
    const float target_depth_samples = parameters.modulation_depth *
        kMaximumModulationDepthMs * sample_rate_ / 1000.0F;
    const float target_phase_increment =
        kTwoPi * parameters.modulation_rate_hz / sample_rate_;

    parameters_ = parameters;
    target_tone_alpha_ = target_tone_alpha;
    target_high_damp_alpha_ = target_high_damp_alpha;
    target_low_damp_alpha_ = target_low_damp_alpha;
    const float ramp = static_cast<float>(ramp_total_);
    mix_step_ = (parameters_.mix - mix_) / ramp;
    tone_alpha_step_ = (target_tone_alpha_ - tone_alpha_) / ramp;
    diffusion_step_ = (parameters_.diffusion - diffusion_) / ramp;
    high_damp_alpha_step_ = (target_high_damp_alpha_ - high_damp_alpha_) / ramp;
    low_damp_alpha_step_ = (target_low_damp_alpha_ - low_damp_alpha_) / ramp;
    modulation_depth_step_ =
        (target_depth_samples - modulation_depth_samples_) / ramp;
    modulation_phase_increment_step_ =
        (target_phase_increment - modulation_phase_increment_) / ramp;
    for (auto& line : lines_) {
        const float delay_seconds = line.nominal_delay_samples / sample_rate_;
        line.feedback_target = std::min(
            0.995F, std::pow(10.0F,
                             -3.0F * delay_seconds / parameters_.decay_seconds));
        line.feedback_step =
            (line.feedback_target - line.feedback) / ramp;
    }
    ramp_remaining_ = ramp_total_;
}

void CloudReverb::advance_parameters() noexcept {
    if (ramp_remaining_ == 0) return;
    mix_ += mix_step_;
    tone_alpha_ += tone_alpha_step_;
    diffusion_ += diffusion_step_;
    high_damp_alpha_ += high_damp_alpha_step_;
    low_damp_alpha_ += low_damp_alpha_step_;
    modulation_depth_samples_ += modulation_depth_step_;
    modulation_phase_increment_ += modulation_phase_increment_step_;
    for (auto& line : lines_) line.feedback += line.feedback_step;
    --ramp_remaining_;
    if (ramp_remaining_ == 0) settle_parameters();
}

void CloudReverb::settle_parameters() noexcept {
    mix_ = parameters_.mix;
    tone_alpha_ = target_tone_alpha_;
    diffusion_ = parameters_.diffusion;
    high_damp_alpha_ = target_high_damp_alpha_;
    low_damp_alpha_ = target_low_damp_alpha_;
    modulation_depth_samples_ = parameters_.modulation_depth *
        kMaximumModulationDepthMs * sample_rate_ / 1000.0F;
    modulation_phase_increment_ =
        kTwoPi * parameters_.modulation_rate_hz / sample_rate_;
    for (auto& line : lines_) {
        line.feedback = line.feedback_target;
        line.feedback_step = 0.0F;
    }
    ramp_remaining_ = 0;
}

float CloudReverb::process_diffuser(DiffuserLine& line,
                                    float input) noexcept {
    const std::size_t index = line.offset + line.index;
    const float delayed = storage_[index];
    const float output = delayed - line.coefficient * input;
    storage_[index] = std::clamp(input + line.coefficient * output,
                                 -kInternalLimit, kInternalLimit);
    ++line.index;
    if (line.index == line.length) line.index = 0;
    return finite_sample(input * (1.0F - diffusion_) + output * diffusion_);
}

float CloudReverb::read_delay(const DelayLine& line,
                              float delay_samples) const noexcept {
    const float bounded_delay = std::clamp(
        delay_samples, 1.0F, static_cast<float>(line.length - 2U));
    float position = static_cast<float>(line.index) - bounded_delay;
    if (position < 0.0F) position += static_cast<float>(line.length);
    const std::size_t first = static_cast<std::size_t>(position);
    const std::size_t second = first + 1U == line.length ? 0U : first + 1U;
    const float fraction = position - static_cast<float>(first);
    const float first_sample = storage_[line.offset + first];
    const float second_sample = storage_[line.offset + second];
    return first_sample + fraction * (second_sample - first_sample);
}

std::array<float, 2> CloudReverb::process_frame(float left,
                                               float right) noexcept {
    advance_parameters();
    left = finite_sample(left);
    right = finite_sample(right);
    const float diffuse_left = process_diffuser(diffusers_[0], left);
    const float diffuse_left_final = process_diffuser(diffusers_[1], diffuse_left);
    float diffuse_right_final = 0.0F;
    if (channels_ == 2) {
        const float diffuse_right = process_diffuser(diffusers_[2], right);
        diffuse_right_final = process_diffuser(diffusers_[3], diffuse_right);
    }
    const std::array<float, 2> input{diffuse_left_final, diffuse_right_final};
    std::array<float, kLineCount> delayed{};
    std::array<float, 2> sums{};
    std::array<float, 2> outputs{};

    const std::size_t active_line_count =
        static_cast<std::size_t>(channels_) * kLinesPerChannel;
    for (std::size_t index = 0; index < active_line_count; ++index) {
        auto& line = lines_[index];
        const std::size_t channel = index / kLinesPerChannel;
        const std::size_t line_index = index % kLinesPerChannel;
        const float phase = modulation_phase_ + kLfoOffsets[line_index] +
                            (channel == 0 ? 0.0F : 0.47F);
        const float read_delay_samples = line.nominal_delay_samples +
            modulation_depth_samples_ * std::sin(phase);
        const float raw = read_delay(line, read_delay_samples);
        const float high_damped = (1.0F - high_damp_alpha_) * raw +
                                  high_damp_alpha_ * line.high_damp_state;
        line.high_damp_state = std::clamp(high_damped,
                                          -kInternalLimit, kInternalLimit);
        const float low_damped = low_damp_alpha_ *
            (line.low_damp_previous_output + line.high_damp_state -
             line.low_damp_previous_input);
        line.low_damp_previous_input = line.high_damp_state;
        line.low_damp_previous_output = std::clamp(low_damped,
                                                   -kInternalLimit,
                                                   kInternalLimit);
        delayed[index] = line.low_damp_previous_output;
        sums[channel] += delayed[index];
        outputs[channel] += delayed[index] * kOutputSigns[line_index];
    }

    for (std::size_t index = 0; index < active_line_count; ++index) {
        auto& line = lines_[index];
        const std::size_t channel = index / kLinesPerChannel;
        const std::size_t line_index = index % kLinesPerChannel;
        const float householder = delayed[index] - 0.25F * sums[channel];
        const float injected = input[channel] * kInputSigns[line_index] *
                               kInvSqrtEight;
        storage_[line.offset + line.index] = std::clamp(
            injected + householder * line.feedback,
            -kInternalLimit, kInternalLimit);
        ++line.index;
        if (line.index == line.length) line.index = 0;
    }

    modulation_phase_ += modulation_phase_increment_;
    if (modulation_phase_ >= kTwoPi) modulation_phase_ -= kTwoPi;
    for (std::size_t channel = 0; channel < channels_; ++channel) {
        const float wet = outputs[channel] * kInvSqrtEight;
        tone_state_[channel] = std::clamp(
            (1.0F - tone_alpha_) * wet + tone_alpha_ * tone_state_[channel],
            -kInternalLimit, kInternalLimit);
    }
    return {tone_state_[0], tone_state_[1]};
}

void CloudReverb::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels != channels_) return;
    bypass_cleared_ = false;
    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels_;
        const float left = finite_sample(interleaved[offset]);
        const float right = channels_ == 2
            ? finite_sample(interleaved[offset + 1U])
            : 0.0F;
        const auto wet = process_frame(left, right);
        interleaved[offset] = finite_sample(
            left * (1.0F - mix_) + wet[0] * mix_);
        if (channels_ == 2) {
            interleaved[offset + 1U] = finite_sample(
                right * (1.0F - mix_) + wet[1] * mix_);
        }
    }
}

void CloudReverb::process_bypassed(std::span<float> interleaved,
                                   std::uint16_t channels) noexcept {
    if (channels != channels_) return;
    if (!parameters_.spillover) {
        if (!bypass_cleared_) clear_tail_state();
        bypass_cleared_ = true;
        return;
    }
    bypass_cleared_ = false;
    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels_;
        const float left = finite_sample(interleaved[offset]);
        const float right = channels_ == 2
            ? finite_sample(interleaved[offset + 1U])
            : 0.0F;
        const auto wet = process_frame(0.0F, 0.0F);
        interleaved[offset] = finite_sample(left + wet[0] * mix_);
        if (channels_ == 2) {
            interleaved[offset + 1U] = finite_sample(right + wet[1] * mix_);
        }
    }
}

void CloudReverb::clear_tail_state() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    for (auto& line : lines_) {
        line.index = 0;
        line.high_damp_state = 0.0F;
        line.low_damp_previous_input = 0.0F;
        line.low_damp_previous_output = 0.0F;
    }
    for (auto& line : diffusers_) line.index = 0;
    tone_state_.fill(0.0F);
    modulation_phase_ = 0.0F;
}

void CloudReverb::reset() noexcept {
    clear_tail_state();
    settle_parameters();
    bypass_cleared_ = false;
}

} // namespace tabletfx
