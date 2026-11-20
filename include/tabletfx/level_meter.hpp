#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

struct MeterReading {
    float peak = 0.0F;
    float rms = 0.0F;
    std::uint32_t frames = 0;
    std::uint32_t channels = 0;
    std::uint32_t invalid_samples = 0;
    std::uint32_t blocks = 0;
    std::uint32_t clipped_blocks = 0;
    bool block_clipped = false;
    bool clip_latched = false;
};

// Lock-free, allocation-free block level meter. Readings are individually
// atomic, so a concurrent UI snapshot can straddle two adjacent blocks.
class LevelMeter final {
public:
    explicit LevelMeter(float clip_threshold = 0.999F) noexcept;

    [[nodiscard]] std::size_t memory_bytes() const noexcept { return sizeof(*this); }
    void process(std::span<const float> interleaved, std::uint16_t channels) noexcept;
    [[nodiscard]] float peak() const noexcept;
    [[nodiscard]] float rms() const noexcept;
    [[nodiscard]] std::uint32_t invalid_samples() const noexcept;
    [[nodiscard]] std::uint32_t clip_flags() const noexcept;
    [[nodiscard]] MeterReading snapshot() const noexcept;
    void clear_clip_latch() noexcept;
    void reset() noexcept;

private:
    static constexpr std::uint32_t kBlockClipped = 1U << 0U;
    static constexpr std::uint32_t kClipLatched = 1U << 1U;

    float clip_threshold_;
    std::atomic<std::uint32_t> peak_bits_{0};
    std::atomic<std::uint32_t> rms_bits_{0};
    std::atomic<std::uint32_t> frames_{0};
    std::atomic<std::uint32_t> channels_{0};
    std::atomic<std::uint32_t> invalid_samples_{0};
    std::atomic<std::uint32_t> blocks_{0};
    std::atomic<std::uint32_t> clipped_blocks_{0};
    std::atomic<std::uint32_t> flags_{0};
};

} // namespace tabletfx
