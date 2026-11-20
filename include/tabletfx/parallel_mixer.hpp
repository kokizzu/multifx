#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class ChannelMixStatus : std::uint8_t {
    Success,
    UnsupportedChannelCount,
    InvalidInputLength,
    BufferTooSmall,
    OverlappingBuffers,
};

struct ChannelMixResult {
    ChannelMixStatus status = ChannelMixStatus::Success;
    std::size_t frames = 0;

    [[nodiscard]] constexpr bool succeeded() const noexcept {
        return status == ChannelMixStatus::Success;
    }
};

struct ParallelMixParameters {
    float dry_gain = 1.0F;
    float branch_a_gain = 0.0F;
    float branch_a_balance = 0.0F;
    bool branch_a_inverted = false;
    float branch_b_gain = 0.0F;
    float branch_b_balance = 0.0F;
    bool branch_b_inverted = false;
};

// Duplicates an interleaved mono/stereo input into two preallocated branch buffers.
// Output buffers must be distinct; exact in-place input-to-branch copies are safe.
[[nodiscard]] ChannelMixResult split_to_parallel(
    std::span<const float> input, std::span<float> branch_a,
    std::span<float> branch_b, std::uint16_t channels) noexcept;

// Sums dry audio and two independently processed branch buffers without allocation.
// Stereo balance is linear: positive values attenuate left; negative values attenuate
// right. Mono input ignores balance. Output is not clipped or normalized.
class ParallelMixer final {
public:
    explicit ParallelMixer(float sample_rate = 48000.0F,
                           float smoothing_ms = 10.0F) noexcept;

    void configure_smoothing(float sample_rate, float smoothing_ms) noexcept;
    void set_parameters(ParallelMixParameters parameters) noexcept;
    void set_parameters_immediate(ParallelMixParameters parameters) noexcept;
    [[nodiscard]] ChannelMixResult process(
        std::span<const float> dry, std::span<const float> branch_a,
        std::span<const float> branch_b, std::span<float> output,
        std::uint16_t channels) noexcept;
    void reset() noexcept;
    [[nodiscard]] std::size_t memory_bytes() const noexcept { return sizeof(*this); }

private:
    [[nodiscard]] static std::array<float, 7> coefficients(
        ParallelMixParameters parameters) noexcept;
    void apply_immediately(const std::array<float, 7>& coefficients) noexcept;

    float sample_rate_ = 48000.0F;
    float smoothing_ms_ = 10.0F;
    std::array<float, 7> current_{};
    std::array<float, 7> target_{};
    std::array<float, 7> step_{};
    std::size_t ramp_remaining_ = 0;
    std::size_t ramp_frames_ = 480;
};

} // namespace tabletfx
