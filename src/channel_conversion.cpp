#include "tabletfx/channel_conversion.hpp"

#include <functional>
#include <limits>

namespace tabletfx {

ChannelConversionResult convert_channels(std::span<const float> input,
                                         std::uint16_t input_channels,
                                         std::span<float> output,
                                         std::uint16_t output_channels,
                                         MonoSumPolicy sum_policy) noexcept {
    if ((input_channels != 1 && input_channels != 2) ||
        (output_channels != 1 && output_channels != 2)) {
        return {ChannelConversionStatus::UnsupportedChannelCount, 0};
    }
    if (sum_policy != MonoSumPolicy::Average && sum_policy != MonoSumPolicy::EqualPower) {
        return {ChannelConversionStatus::InvalidSummingPolicy, 0};
    }
    if (input.size() % input_channels != 0) {
        return {ChannelConversionStatus::InvalidInputLength, 0};
    }

    const std::size_t frames = input.size() / input_channels;
    if (frames > std::numeric_limits<std::size_t>::max() / output_channels) {
        return {ChannelConversionStatus::OutputTooSmall, 0};
    }
    const std::size_t required_output = frames * output_channels;
    if (output.size() < required_output) {
        return {ChannelConversionStatus::OutputTooSmall, 0};
    }
    if (frames == 0) {
        return {ChannelConversionStatus::Success, 0};
    }

    const float* input_begin = input.data();
    const float* input_end = input_begin + input.size();
    float* output_begin = output.data();
    float* output_end = output_begin + required_output;
    const std::less<const float*> less;
    const bool overlaps = less(input_begin, output_end) && less(output_begin, input_end);
    const bool same_start = input_begin == output_begin;
    if (overlaps && !same_start) {
        return {ChannelConversionStatus::OverlappingBuffers, 0};
    }

    if (input_channels == output_channels && same_start) {
        return {ChannelConversionStatus::Success, frames};
    }

    if (input_channels == 1 && output_channels == 2) {
        // Expand backward when operating in-place so each mono sample is read
        // before its destination pair can overwrite it.
        for (std::size_t frame = frames; frame > 0; --frame) {
            const std::size_t source_index = frame - 1;
            const float sample = input[source_index];
            output[source_index * 2] = sample;
            output[source_index * 2 + 1] = sample;
        }
        return {ChannelConversionStatus::Success, frames};
    }

    if (input_channels == 2 && output_channels == 1) {
        constexpr float kAverage = 0.5F;
        constexpr float kEqualPower = 0.7071067811865475F;
        const float scale = sum_policy == MonoSumPolicy::Average ? kAverage : kEqualPower;
        // Compaction is safe in-place in the forward direction: each source
        // stereo frame is consumed before its destination can overlap it.
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const std::size_t source_index = frame * 2;
            output[frame] = (input[source_index] + input[source_index + 1]) * scale;
        }
        return {ChannelConversionStatus::Success, frames};
    }

    for (std::size_t sample = 0; sample < input.size(); ++sample) {
        output[sample] = input[sample];
    }
    return {ChannelConversionStatus::Success, frames};
}

} // namespace tabletfx
