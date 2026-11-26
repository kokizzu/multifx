#include "tabletfx/effects/tone_master_modulated_large_hall.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

// Project-selected longer delays for the Large Hall character; these are not
// copied from Fender's undocumented internal algorithm or parameter values.
constexpr std::array<float, 4U> kLargeHallDelayMs{
    55.1F, 63.7F, 72.3F, 82.9F};
constexpr std::array<float, 2U> kLargeHallDiffuserMs{7.3F, 13.1F};
constexpr std::array<float, 4U> kLinePhaseOffsets{
    0.0F, 1.57079632679F, 3.14159265359F, 4.71238898038F};
constexpr std::array<float, 4U> kInputSigns{1.0F, -1.0F, 1.0F, -1.0F};
constexpr float kStereoDelayOffsetMs = 0.71F;
constexpr float kStereoPhaseOffset = 0.43F;
constexpr float kDiffuserMinimumCoefficient = 0.18F;
constexpr float kDiffuserCoefficientRange = 0.60F;
constexpr float kTwoPi = 6.28318530718F;

float finite_or(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}

float gain_from_db(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

std::size_t samples_for_ms(float sample_rate, float milliseconds) noexcept {
    const double samples = static_cast<double>(sample_rate) *
                           static_cast<double>(milliseconds) / 1000.0;
    return std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(samples)));
}

std::size_t wrap_subtract(std::size_t index, std::size_t amount,
                          std::size_t capacity) noexcept {
    amount %= capacity;
    return index >= amount ? index - amount : capacity - (amount - index);
}

} // namespace

ToneMasterModulatedLargeHallReverb::ToneMasterModulatedLargeHallReverb(
    float sample_rate, std::uint16_t channels,
    ToneMasterModulatedLargeHallParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
        sample_rate_ > 192000.0F) {
        throw std::invalid_argument(
            "Modulated Large Hall sample rate must be between 8 kHz and 192 kHz");
    }
    if (channels_ != 1U && channels_ != 2U) {
        throw std::invalid_argument(
            "Modulated Large Hall supports mono or stereo only");
    }

    const std::size_t modulation_margin =
        samples_for_ms(sample_rate_, kMaximumModulationMs) + 2U;
    std::size_t storage_samples = 0U;
    for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
        const float stereo_delay = channel == 0U ? 0.0F : kStereoDelayOffsetMs;
        for (std::size_t index = 0U;
             index < kFeedbackLinesPerChannel; ++index) {
            auto& line = feedback_lines_[
                static_cast<std::size_t>(channel) *
                    kFeedbackLinesPerChannel + index];
            line.offset = storage_samples;
            line.nominal_delay_samples = static_cast<float>(samples_for_ms(
                sample_rate_, kLargeHallDelayMs[index] + stereo_delay));
            line.length = static_cast<std::size_t>(
                line.nominal_delay_samples) + modulation_margin;
            storage_samples += line.length;
        }
        const float diffuser_offset = channel == 0U ? 0.0F : 0.37F;
        for (std::size_t index = 0U;
             index < kDiffusersPerChannel; ++index) {
            auto& line = diffusers_[
                static_cast<std::size_t>(channel) * kDiffusersPerChannel + index];
            line.offset = storage_samples;
            line.length = samples_for_ms(
                sample_rate_, kLargeHallDiffuserMs[index] + diffuser_offset);
            storage_samples += line.length;
        }
    }
    storage_.assign(storage_samples, 0.0F);

    const float low_shelf_frequency = std::min(240.0F, sample_rate_ * 0.15F);
    const float high_shelf_frequency = std::min(6500.0F, sample_rate_ * 0.40F);
    low_shelf_alpha_ = std::exp(-kTwoPi * low_shelf_frequency / sample_rate_);
    high_shelf_alpha_ = std::exp(-kTwoPi * high_shelf_frequency / sample_rate_);
    const float damping_frequency = std::min(12000.0F, sample_rate_ * 0.40F);
    damping_alpha_ = std::exp(-kTwoPi * damping_frequency / sample_rate_);
    parameter_ramp_total_ = std::max<std::size_t>(
        1U, samples_for_ms(sample_rate_, kControlSmoothingMs));

    configure(parameters);
    settle_parameters();
}

std::size_t ToneMasterModulatedLargeHallReverb::memory_bytes() const noexcept {
    return sizeof(*this) + storage_.capacity() * sizeof(float);
}

void ToneMasterModulatedLargeHallReverb::configure(
    ToneMasterModulatedLargeHallParameters parameters) noexcept {
    const ToneMasterModulatedLargeHallParameters defaults{};
    parameters.level = std::clamp(
        finite_or(parameters.level, defaults.level), 0.0F, 1.0F);
    parameters.decay_seconds = std::clamp(
        finite_or(parameters.decay_seconds, defaults.decay_seconds),
        0.6F, 18.0F);
    parameters.dwell = std::clamp(
        finite_or(parameters.dwell, defaults.dwell), 0.0F, 2.0F);
    parameters.diffuse = std::clamp(
        finite_or(parameters.diffuse, defaults.diffuse), 0.0F, 1.0F);
    parameters.rate_hz = std::clamp(
        finite_or(parameters.rate_hz, defaults.rate_hz), 0.05F, 4.0F);
    parameters.depth = std::clamp(
        finite_or(parameters.depth, defaults.depth), 0.0F, 1.0F);
    parameters.high_cut_gain_db = std::clamp(
        finite_or(parameters.high_cut_gain_db,
                  defaults.high_cut_gain_db), -12.0F, 6.0F);
    parameters.low_cut_gain_db = std::clamp(
        finite_or(parameters.low_cut_gain_db,
                  defaults.low_cut_gain_db), -12.0F, 6.0F);
    parameters_ = parameters;
    update_targets();
}

void ToneMasterModulatedLargeHallReverb::update_targets() noexcept {
    level_target_ = parameters_.level;
    dwell_target_ = parameters_.dwell;
    diffuse_target_ = parameters_.diffuse;
    modulation_depth_target_samples_ = parameters_.depth *
        sample_rate_ * kMaximumModulationMs / 1000.0F;
    modulation_phase_increment_target_ =
        kTwoPi * parameters_.rate_hz / sample_rate_;
    low_shelf_gain_target_ = gain_from_db(parameters_.low_cut_gain_db);
    high_shelf_gain_target_ = gain_from_db(parameters_.high_cut_gain_db);

    for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
        for (std::size_t index = 0U;
             index < kFeedbackLinesPerChannel; ++index) {
            auto& line = feedback_lines_[
                static_cast<std::size_t>(channel) *
                    kFeedbackLinesPerChannel + index];
            const float delay_seconds =
                line.nominal_delay_samples / sample_rate_;
            line.feedback_target = std::min(
                0.995F,
                std::pow(10.0F, -3.0F * delay_seconds /
                                    parameters_.decay_seconds));
        }
    }

    const float ramp = static_cast<float>(parameter_ramp_total_);
    level_step_ = (level_target_ - level_) / ramp;
    dwell_step_ = (dwell_target_ - dwell_) / ramp;
    diffuse_step_ = (diffuse_target_ - diffuse_) / ramp;
    modulation_depth_step_ =
        (modulation_depth_target_samples_ - modulation_depth_samples_) / ramp;
    modulation_phase_increment_step_ =
        (modulation_phase_increment_target_ - modulation_phase_increment_) / ramp;
    low_shelf_gain_step_ =
        (low_shelf_gain_target_ - low_shelf_gain_) / ramp;
    high_shelf_gain_step_ =
        (high_shelf_gain_target_ - high_shelf_gain_) / ramp;
    for (auto& line : feedback_lines_) {
        line.feedback_step =
            (line.feedback_target - line.feedback) / ramp;
    }
    parameter_ramp_remaining_ = parameter_ramp_total_;
}

void ToneMasterModulatedLargeHallReverb::advance_parameters() noexcept {
    if (parameter_ramp_remaining_ == 0U) return;
    level_ += level_step_;
    dwell_ += dwell_step_;
    diffuse_ += diffuse_step_;
    modulation_depth_samples_ += modulation_depth_step_;
    modulation_phase_increment_ += modulation_phase_increment_step_;
    low_shelf_gain_ += low_shelf_gain_step_;
    high_shelf_gain_ += high_shelf_gain_step_;
    for (auto& line : feedback_lines_) line.feedback += line.feedback_step;
    --parameter_ramp_remaining_;
    if (parameter_ramp_remaining_ == 0U) settle_parameters();
}

void ToneMasterModulatedLargeHallReverb::settle_parameters() noexcept {
    level_ = level_target_;
    dwell_ = dwell_target_;
    diffuse_ = diffuse_target_;
    modulation_depth_samples_ = modulation_depth_target_samples_;
    modulation_phase_increment_ = modulation_phase_increment_target_;
    low_shelf_gain_ = low_shelf_gain_target_;
    high_shelf_gain_ = high_shelf_gain_target_;
    for (auto& line : feedback_lines_) line.feedback = line.feedback_target;
    parameter_ramp_remaining_ = 0U;
}

float ToneMasterModulatedLargeHallReverb::process_diffuser(
    DiffuserLine& line, float input) noexcept {
    const float delayed = storage_[line.offset + line.index];
    const float coefficient = kDiffuserMinimumCoefficient +
        kDiffuserCoefficientRange * diffuse_;
    const float output = delayed - coefficient * input;
    const float write = input + coefficient * output;
    storage_[line.offset + line.index] = safe_sample(write);
    line.index = line.index + 1U == line.length ? 0U : line.index + 1U;
    return safe_sample(output);
}

float ToneMasterModulatedLargeHallReverb::read_delay(
    const DelayLine& line, float delay_samples) const noexcept {
    const float bounded_delay = std::clamp(
        delay_samples, 1.0F, static_cast<float>(line.length - 2U));
    const auto whole = static_cast<std::size_t>(bounded_delay);
    const float fraction = bounded_delay - static_cast<float>(whole);
    const std::size_t newer =
        wrap_subtract(line.index, whole, line.length);
    const std::size_t older =
        wrap_subtract(line.index, whole + 1U, line.length);
    return storage_[line.offset + newer] * (1.0F - fraction) +
           storage_[line.offset + older] * fraction;
}

float ToneMasterModulatedLargeHallReverb::process_channel(
    float input, std::uint16_t channel) noexcept {
    const float first_diffuse = process_diffuser(
        diffusers_[static_cast<std::size_t>(channel) * kDiffusersPerChannel],
        input);
    const float diffused_input = process_diffuser(
        diffusers_[static_cast<std::size_t>(channel) * kDiffusersPerChannel + 1U],
        first_diffuse);

    std::array<float, kFeedbackLinesPerChannel> delayed{};
    std::array<float, kFeedbackLinesPerChannel> mixed{};
    const float channel_phase = modulation_phase_[channel] +
        (channel == 0U ? 0.0F : kStereoPhaseOffset);
    for (std::size_t index = 0U;
         index < kFeedbackLinesPerChannel; ++index) {
        auto& line = feedback_lines_[
            static_cast<std::size_t>(channel) *
                kFeedbackLinesPerChannel + index];
        const float phase = channel_phase + kLinePhaseOffsets[index];
        const float delay = line.nominal_delay_samples +
            modulation_depth_samples_ * std::sin(phase);
        const float raw = read_delay(line, delay);
        line.damping_state = damping_alpha_ * line.damping_state +
            (1.0F - damping_alpha_) * raw;
        delayed[index] = safe_sample(line.damping_state);
    }

    const float half = 0.5F;
    mixed[0] = half * (delayed[0] + delayed[1] + delayed[2] + delayed[3]);
    mixed[1] = half * (delayed[0] - delayed[1] + delayed[2] - delayed[3]);
    mixed[2] = half * (delayed[0] + delayed[1] - delayed[2] - delayed[3]);
    mixed[3] = half * (delayed[0] - delayed[1] - delayed[2] + delayed[3]);

    for (std::size_t index = 0U;
         index < kFeedbackLinesPerChannel; ++index) {
        auto& line = feedback_lines_[
            static_cast<std::size_t>(channel) *
                kFeedbackLinesPerChannel + index];
        const float write = diffused_input * kInputSigns[index] * 0.25F +
                            mixed[index] * line.feedback;
        storage_[line.offset + line.index] = safe_sample(write);
        line.index = line.index + 1U == line.length ? 0U : line.index + 1U;
    }

    const float output = channel == 0U
        ? 0.25F * (delayed[0] + delayed[1] - delayed[2] - delayed[3])
        : 0.25F * (delayed[0] - delayed[1] + delayed[2] - delayed[3]);
    return safe_sample(output);
}

float ToneMasterModulatedLargeHallReverb::filter_wet(
    float sample, std::uint16_t channel) noexcept {
    const float low = (1.0F - low_shelf_alpha_) * sample +
        low_shelf_alpha_ * low_shelf_state_[channel];
    low_shelf_state_[channel] = safe_sample(low);
    const float low_shelved = sample + (low_shelf_gain_ - 1.0F) * low;

    const float high_lowpass = (1.0F - high_shelf_alpha_) * low_shelved +
        high_shelf_alpha_ * high_shelf_lowpass_state_[channel];
    high_shelf_lowpass_state_[channel] = safe_sample(high_lowpass);
    const float high_shelved = low_shelved +
        (high_shelf_gain_ - 1.0F) * (low_shelved - high_lowpass);
    return safe_sample(high_shelved);
}

float ToneMasterModulatedLargeHallReverb::safe_sample(float sample) noexcept {
    return std::isfinite(sample)
        ? std::clamp(sample, -kInternalLimit, kInternalLimit)
        : 0.0F;
}

void ToneMasterModulatedLargeHallReverb::process(
    std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels == 0U || channels != channels_) return;
    bypass_cleared_ = false;
    process_internal(interleaved, false);
}

void ToneMasterModulatedLargeHallReverb::process_bypassed(
    std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels == 0U || channels != channels_) return;
    if (!parameters_.spillover) {
        if (!bypass_cleared_) clear_tail_state();
        bypass_cleared_ = true;
        return;
    }
    bypass_cleared_ = false;
    process_internal(interleaved, true);
}

void ToneMasterModulatedLargeHallReverb::process_internal(
    std::span<float> interleaved, bool bypassed) noexcept {
    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        advance_parameters();
        const std::size_t offset = frame * channels_;
        const float dry_left = safe_sample(interleaved[offset]);
        const float send_left = bypassed ? 0.0F : dry_left * dwell_;
        const float wet_left = process_channel(safe_sample(send_left), 0U);
        float wet_right = wet_left;
        if (channels_ == 2U) {
            const float dry_right = safe_sample(interleaved[offset + 1U]);
            const float send_right = bypassed ? 0.0F : dry_right * dwell_;
            wet_right = process_channel(safe_sample(send_right), 1U);
            const float cross_left = 0.94F * wet_left + 0.06F * wet_right;
            const float cross_right = 0.94F * wet_right + 0.06F * wet_left;
            wet_right = cross_right;
            interleaved[offset] = safe_sample(
                bypassed ? dry_left + filter_wet(cross_left, 0U) * level_
                         : dry_left * (1.0F - level_) +
                               filter_wet(cross_left, 0U) * level_);
            interleaved[offset + 1U] = safe_sample(
                bypassed ? dry_right + filter_wet(wet_right, 1U) * level_
                         : dry_right * (1.0F - level_) +
                               filter_wet(wet_right, 1U) * level_);
        } else {
            const float wet = filter_wet(wet_left, 0U) * level_;
            interleaved[offset] = safe_sample(
                bypassed ? dry_left + wet
                         : dry_left * (1.0F - level_) + wet);
        }

        for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
            modulation_phase_[channel] += modulation_phase_increment_;
            if (modulation_phase_[channel] >= kTwoPi) {
                modulation_phase_[channel] -= kTwoPi;
            }
        }
    }
}

void ToneMasterModulatedLargeHallReverb::clear_tail_state() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    for (auto& line : feedback_lines_) {
        line.index = 0U;
        line.damping_state = 0.0F;
    }
    for (auto& line : diffusers_) line.index = 0U;
    low_shelf_state_.fill(0.0F);
    high_shelf_lowpass_state_.fill(0.0F);
    reset_modulation_phase();
}

void ToneMasterModulatedLargeHallReverb::reset() noexcept {
    clear_tail_state();
    settle_parameters();
    bypass_cleared_ = false;
}

void ToneMasterModulatedLargeHallReverb::reset_modulation_phase() noexcept {
    modulation_phase_.fill(0.0F);
}

} // namespace tabletfx
