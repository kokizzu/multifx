#include "tabletfx/effects/triple_double_chorus.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx::effects {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kMinSampleRate = 8000.0F;
constexpr float kMaxSampleRate = 96000.0F;
constexpr float kMinRateHz = 0.05F;
constexpr float kMaxRateHz = 8.0F;
constexpr float kMinDepthMs = 0.0F;
constexpr float kMaxFeedback = 0.8F;
constexpr float kMaxSparkle = 1.0F;
constexpr float kDryGain = 0.72F;
constexpr float kWetGain = 0.28F;
constexpr float kPanCenter = 0.70710678118F;
constexpr float kPanNorm = 1.0F + kPanCenter;
constexpr float kMaxInternalLevel = 16.0F;

float finite_clamp(float value, float fallback, float low, float high) noexcept {
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}

float safe_audio(float value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0F;
    }
    return std::clamp(value, -kMaxInternalLevel, kMaxInternalLevel);
}

}  // namespace

TripleDoubleChorus::TripleDoubleChorus(float sample_rate) noexcept
    : sample_rate_(finite_clamp(sample_rate, 48000.0F,
                                kMinSampleRate, kMaxSampleRate)) {
    sparkle_alpha_ = 1.0F - std::exp(-2.0F * kPi * 3200.0F / sample_rate_);
    sparkle_alpha_ = std::clamp(sparkle_alpha_, 0.0F, 1.0F);
    smoothing_ = 1.0F - std::exp(-1.0F / (0.020F * sample_rate_));
    resolve_parameters();
    current_ = target_;
    parameters_initialized_ = true;
}

void TripleDoubleChorus::configure(const TripleDoubleChorusConfig& requested) noexcept {
    config_ = requested;
    config_.rate_hz = finite_clamp(config_.rate_hz, 0.8F,
                                   kMinRateHz, kMaxRateHz);
    config_.left_depth_ms = finite_clamp(config_.left_depth_ms, 7.0F,
                                         kMinDepthMs, kMaxDepthMs);
    config_.center_depth_ms = finite_clamp(config_.center_depth_ms, 5.0F,
                                           kMinDepthMs, kMaxDepthMs);
    config_.right_depth_ms = finite_clamp(config_.right_depth_ms, 8.0F,
                                          kMinDepthMs, kMaxDepthMs);
    config_.feedback = finite_clamp(config_.feedback, 0.1F,
                                    -kMaxFeedback, kMaxFeedback);
    config_.sparkle = finite_clamp(config_.sparkle, 0.2F,
                                   0.0F, kMaxSparkle);
    resolve_parameters();
}

void TripleDoubleChorus::reset() noexcept {
    clear_state();
    phase_ = 0.0F;
    current_ = target_;
}

void TripleDoubleChorus::process(std::span<float> interleaved,
                                std::uint16_t channels) noexcept {
    if (channels == 0 || channels > 2) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        const float input_left = interleaved[offset];
        float output_left = 0.0F;
        float output_right = 0.0F;

        if (channels == 1) {
            process(&input_left, nullptr, &output_left, &output_right, 1);
            interleaved[offset] = config_.stereo_switch
                ? safe_audio(0.5F * (output_left + output_right))
                : output_left;
            continue;
        }

        const float input_right = interleaved[offset + 1];
        process(&input_left, &input_right, &output_left, &output_right, 1);
        interleaved[offset] = output_left;
        interleaved[offset + 1] = output_right;
    }
}

void TripleDoubleChorus::process(const float* input_left,
                                const float* input_right,
                                float* output_left,
                                float* output_right,
                                std::size_t frames) noexcept {
    if (output_left == nullptr) {
        return;
    }

    constexpr std::array<float, kVoiceCount> phase_offset = {
        0.0F, 1.0F / 3.0F, 2.0F / 3.0F
    };
    constexpr std::array<float, kVoiceCount> pan_left = {
        1.0F, kPanCenter, 0.0F
    };
    constexpr std::array<float, kVoiceCount> pan_right = {
        0.0F, kPanCenter, 1.0F
    };

    for (std::size_t frame = 0; frame < frames; ++frame) {
        float input[2] = {
            input_left == nullptr ? 0.0F : safe_audio(input_left[frame]),
            input_right == nullptr
                ? (input_left == nullptr ? 0.0F : safe_audio(input_left[frame]))
                : safe_audio(input_right[frame]),
        };

        current_.rate_hz += smoothing_ * (target_.rate_hz - current_.rate_hz);
        current_.left_depth_ms += smoothing_ *
            (target_.left_depth_ms - current_.left_depth_ms);
        current_.center_depth_ms += smoothing_ *
            (target_.center_depth_ms - current_.center_depth_ms);
        current_.right_depth_ms += smoothing_ *
            (target_.right_depth_ms - current_.right_depth_ms);
        current_.feedback += smoothing_ *
            (target_.feedback - current_.feedback);
        current_.sparkle += smoothing_ *
            (target_.sparkle - current_.sparkle);

        const float depths[kVoiceCount] = {
            current_.left_depth_ms,
            current_.center_depth_ms,
            current_.right_depth_ms,
        };
        float voices[2][kVoiceCount] = {};
        for (std::size_t channel = 0; channel < 2; ++channel) {
            for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
                float lfo = phase_ + phase_offset[voice];
                if (lfo >= 1.0F) {
                    lfo -= 1.0F;
                }
                const float modulation = 0.5F +
                    0.5F * std::sin(2.0F * kPi * lfo);
                const float depth_samples = depths[voice] *
                    sample_rate_ / 1000.0F;
                const float delay_samples = kBaseDelayMs * sample_rate_ / 1000.0F +
                    depth_samples * modulation;
                const float delayed = read_delay(channel, voice, delay_samples);
                float& low = sparkle_lowpass_[channel][voice];
                low += sparkle_alpha_ * (delayed - low);
                if (!std::isfinite(low)) {
                    low = 0.0F;
                }
                const float high = delayed - low;
                voices[channel][voice] = safe_audio(
                    low + high * (1.0F + current_.sparkle));

                const float next = safe_audio(input[channel] +
                                              current_.feedback * delayed);
                delay_[channel][voice][write_position_] = next;
            }
        }

        const auto panned_left = [&](std::size_t channel) noexcept {
            return (pan_left[0] * voices[channel][0] +
                    pan_left[1] * voices[channel][1] +
                    pan_left[2] * voices[channel][2]) / kPanNorm;
        };
        const auto panned_right = [&](std::size_t channel) noexcept {
            return (pan_right[0] * voices[channel][0] +
                    pan_right[1] * voices[channel][1] +
                    pan_right[2] * voices[channel][2]) / kPanNorm;
        };

        float result_left = 0.0F;
        float result_right = 0.0F;
        if (config_.stereo_switch) {
            const float wet_left = 0.8F * panned_left(0) +
                                   0.2F * panned_left(1);
            const float wet_right = 0.2F * panned_right(0) +
                                    0.8F * panned_right(1);
            result_left = kDryGain * input[0] + kWetGain * wet_left;
            result_right = kDryGain * input[1] + kWetGain * wet_right;
        } else {
            const float dry_mono = 0.5F * (input[0] + input[1]);
            const float wet_mono = 0.25F * (panned_left(0) + panned_right(0) +
                                            panned_left(1) + panned_right(1));
            result_left = result_right = kDryGain * dry_mono + kWetGain * wet_mono;
        }

        output_left[frame] = safe_audio(result_left);
        if (output_right != nullptr) {
            output_right[frame] = safe_audio(result_right);
        }

        phase_ += current_.rate_hz / sample_rate_;
        if (phase_ >= 1.0F) {
            phase_ -= 1.0F;
        }
        write_position_ = (write_position_ + 1) % kDelaySize;
    }
}

void TripleDoubleChorus::resolve_parameters() noexcept {
    if (config_.manual_switch) {
        target_ = {
            config_.rate_hz,
            config_.left_depth_ms,
            config_.center_depth_ms,
            config_.right_depth_ms,
            config_.feedback,
            config_.sparkle,
        };
    } else if (config_.preset_switch) {
        // Project-authored Wide preset; the guide does not publish presets.
        target_ = {0.55F, 18.0F, 12.0F, 18.0F, 0.25F, 0.45F};
    } else {
        // Project-authored Classic preset; the guide does not publish presets.
        target_ = {0.8F, 7.0F, 5.0F, 8.0F, 0.1F, 0.2F};
    }
    if (!parameters_initialized_) {
        current_ = target_;
    }
}

void TripleDoubleChorus::clear_state() noexcept {
    for (auto& channel : delay_) {
        for (auto& voice : channel) {
            voice.fill(0.0F);
        }
    }
    for (auto& channel : sparkle_lowpass_) {
        channel.fill(0.0F);
    }
    write_position_ = 0;
}

float TripleDoubleChorus::read_delay(std::size_t channel,
                                    std::size_t voice,
                                    float delay_samples) const noexcept {
    delay_samples = std::clamp(delay_samples, 1.0F,
                               static_cast<float>(kDelaySize - 2));
    const auto whole = static_cast<std::size_t>(delay_samples);
    const float fraction = delay_samples - static_cast<float>(whole);
    const std::size_t first =
        (write_position_ + kDelaySize - (whole % kDelaySize)) % kDelaySize;
    const std::size_t second = (first + kDelaySize - 1) % kDelaySize;
    const float a = delay_[channel][voice][first];
    const float b = delay_[channel][voice][second];
    const float value = a + fraction * (b - a);
    return std::isfinite(value) ? value : 0.0F;
}

}  // namespace tabletfx::effects
