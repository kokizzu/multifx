#include "tabletfx/level_meter.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace tabletfx {
namespace {

static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "LevelMeter requires lock-free 32-bit atomics");

float sanitize_threshold(float threshold) noexcept {
    if (!std::isfinite(threshold)) {
        return 0.999F;
    }
    return std::clamp(threshold, 0.01F, 1.0F);
}

std::uint32_t as_u32(std::size_t value) noexcept {
    return value > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(value);
}

} // namespace

LevelMeter::LevelMeter(float clip_threshold) noexcept
    : clip_threshold_(sanitize_threshold(clip_threshold)) {}

void LevelMeter::process(std::span<const float> interleaved,
                         std::uint16_t channels) noexcept {
    const std::size_t channel_count = channels;
    const std::size_t sample_count = channels == 0
                                         ? 0
                                         : interleaved.size() - (interleaved.size() % channel_count);
    const std::size_t frame_total = channels == 0 ? 0 : sample_count / channel_count;
    float peak = 0.0F;
    double square_sum = 0.0;
    std::size_t invalid = 0;
    bool clipped = false;

    for (std::size_t i = 0; i < sample_count; ++i) {
        const float sample = interleaved[i];
        if (!std::isfinite(sample)) {
            ++invalid;
            continue;
        }
        const float magnitude = std::abs(sample);
        peak = std::max(peak, magnitude);
        square_sum += static_cast<double>(sample) * static_cast<double>(sample);
        clipped = clipped || magnitude >= clip_threshold_;
    }

    const float rms = sample_count == 0
                          ? 0.0F
                          : static_cast<float>(std::sqrt(square_sum /
                                                         static_cast<double>(sample_count)));
    peak_bits_.store(std::bit_cast<std::uint32_t>(peak), std::memory_order_relaxed);
    rms_bits_.store(std::bit_cast<std::uint32_t>(rms), std::memory_order_relaxed);
    frames_.store(as_u32(frame_total), std::memory_order_relaxed);
    channels_.store(channels, std::memory_order_relaxed);
    invalid_samples_.store(as_u32(invalid), std::memory_order_relaxed);
    blocks_.fetch_add(1, std::memory_order_relaxed);
    flags_.fetch_and(~kBlockClipped, std::memory_order_relaxed);
    if (clipped) {
        clipped_blocks_.fetch_add(1, std::memory_order_relaxed);
        flags_.fetch_or(kBlockClipped | kClipLatched, std::memory_order_relaxed);
    }
}

float LevelMeter::peak() const noexcept {
    return std::bit_cast<float>(peak_bits_.load(std::memory_order_relaxed));
}

float LevelMeter::rms() const noexcept {
    return std::bit_cast<float>(rms_bits_.load(std::memory_order_relaxed));
}

std::uint32_t LevelMeter::invalid_samples() const noexcept {
    return invalid_samples_.load(std::memory_order_relaxed);
}

std::uint32_t LevelMeter::clip_flags() const noexcept {
    return flags_.load(std::memory_order_relaxed);
}

MeterReading LevelMeter::snapshot() const noexcept {
    const std::uint32_t flags = clip_flags();
    return {
        .peak = peak(),
        .rms = rms(),
        .frames = frames_.load(std::memory_order_relaxed),
        .channels = channels_.load(std::memory_order_relaxed),
        .invalid_samples = invalid_samples_.load(std::memory_order_relaxed),
        .blocks = blocks_.load(std::memory_order_relaxed),
        .clipped_blocks = clipped_blocks_.load(std::memory_order_relaxed),
        .block_clipped = (flags & kBlockClipped) != 0,
        .clip_latched = (flags & kClipLatched) != 0,
    };
}

void LevelMeter::clear_clip_latch() noexcept {
    flags_.fetch_and(~kClipLatched, std::memory_order_relaxed);
}

void LevelMeter::reset() noexcept {
    peak_bits_.store(0, std::memory_order_relaxed);
    rms_bits_.store(0, std::memory_order_relaxed);
    frames_.store(0, std::memory_order_relaxed);
    channels_.store(0, std::memory_order_relaxed);
    invalid_samples_.store(0, std::memory_order_relaxed);
    blocks_.store(0, std::memory_order_relaxed);
    clipped_blocks_.store(0, std::memory_order_relaxed);
    flags_.store(0, std::memory_order_relaxed);
}

} // namespace tabletfx
