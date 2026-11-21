#include "tabletfx/effects/bit_crusher.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx {

BitCrusher::BitCrusher(float sample_rate, std::uint16_t channels,
                       BitCrusherParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument(
            "BitCrusher requires 8–384 kHz and mono or stereo channels");
    }
    set_parameters(parameters);
    reset();
}

void BitCrusher::set_parameters(BitCrusherParameters parameters) noexcept {
    parameters.bit_depth = std::clamp(parameters.bit_depth,
                                      kMinimumBitDepth, kMaximumBitDepth);
    parameters.sample_rate_divisor = std::clamp(
        parameters.sample_rate_divisor, kMinimumSampleRateDivisor,
        kMaximumSampleRateDivisor);
    parameters.mix = std::isfinite(parameters.mix)
                         ? std::clamp(parameters.mix, 0.0F, 1.0F)
                         : parameters_.mix;

    if (parameters.sample_rate_divisor != parameters_.sample_rate_divisor) {
        // A changed hold period starts on a frame boundary and avoids carrying
        // an old held sample across an unrelated rate setting.
        frames_until_update_ = 0;
    }
    parameters_ = parameters;
    quantization_scale_ = std::ldexp(
        1.0F, static_cast<int>(parameters_.bit_depth) - 1);
}

void BitCrusher::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const bool update_held = frames_until_update_ == 0;
        const std::size_t offset = frame * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = offset + channel;
            const float input = interleaved[index];
            if (!std::isfinite(input)) {
                held_samples_[channel] = 0.0F;
                interleaved[index] = 0.0F;
                continue;
            }

            if (update_held) {
                held_samples_[channel] = input;
            }
            const float wet = quantize(held_samples_[channel]);
            const float output = input + (wet - input) * parameters_.mix;
            interleaved[index] = std::isfinite(output) ? output : 0.0F;
        }

        if (update_held) {
            frames_until_update_ = static_cast<std::uint16_t>(
                parameters_.sample_rate_divisor - 1U);
        } else {
            --frames_until_update_;
        }
    }
}

void BitCrusher::reset() noexcept {
    held_samples_ = {};
    frames_until_update_ = 0;
}

float BitCrusher::quantize(float sample) const noexcept {
    // Symmetric signed quantization over normalized full scale. Overs are
    // clamped like a fixed-point converter; the original dry path is retained
    // for the Mix control.
    const float bounded_sample = std::clamp(sample, -1.0F, 1.0F);
    const float minimum_code = -quantization_scale_;
    const float maximum_code = quantization_scale_ - 1.0F;
    const float quantized_code = std::clamp(
        std::round(bounded_sample * quantization_scale_),
        minimum_code, maximum_code);
    return quantized_code / quantization_scale_;
}

} // namespace tabletfx
