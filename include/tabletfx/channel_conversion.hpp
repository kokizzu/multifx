#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class MonoSumPolicy : std::uint8_t {
    Average,
    EqualPower,
};

enum class ChannelConversionStatus : std::uint8_t {
    Success,
    UnsupportedChannelCount,
    InvalidInputLength,
    OutputTooSmall,
    InvalidSummingPolicy,
    OverlappingBuffers,
};

struct ChannelConversionResult {
    ChannelConversionStatus status = ChannelConversionStatus::Success;
    std::size_t frames = 0;

    [[nodiscard]] constexpr bool succeeded() const noexcept {
        return status == ChannelConversionStatus::Success;
    }
};

// Converts interleaved mono/stereo audio without allocation or persistent state.
// Mono input is duplicated to stereo. Stereo-to-mono uses the selected summing
// policy; Average is the conservative default. EqualPower can exceed full scale.
// Buffers may be disjoint or have the same starting address (in-place conversion).
// Partial overlap is rejected. Non-finite samples are preserved by arithmetic.
[[nodiscard]] ChannelConversionResult convert_channels(
    std::span<const float> input, std::uint16_t input_channels,
    std::span<float> output, std::uint16_t output_channels,
    MonoSumPolicy sum_policy = MonoSumPolicy::Average) noexcept;

} // namespace tabletfx
