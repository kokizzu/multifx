#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tabletfx {

struct BitCrusherParameters {
    // Project-authored resolution control. The Fractal Blocks Guide documents
    // reducing a 24-bit signal, but does not publish a control range/default.
    std::uint8_t bit_depth = 16;
    // Project-authored integer sample-rate division. A value of 1 keeps every
    // input frame; larger values intentionally hold samples and can alias.
    std::uint16_t sample_rate_divisor = 1;
    float mix = 1.0F;
};

// Project-authored bit-depth and sample-rate reduction. It is a clipped
// signed fixed-point quantizer followed by per-channel sample-and-hold; it
// does not claim to model Fractal or any specific converter. State is fixed,
// stereo channels are independent, and process() performs no allocation.
class BitCrusher final : public Effect {
public:
    explicit BitCrusher(float sample_rate = 48000.0F,
                        std::uint16_t channels = 2,
                        BitCrusherParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "bit-crusher";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void set_parameters(BitCrusherParameters parameters) noexcept;
    [[nodiscard]] BitCrusherParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_sample_rate_hz() const noexcept {
        return sample_rate_ / static_cast<float>(parameters_.sample_rate_divisor);
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr std::uint8_t kMinimumBitDepth = 2;
    static constexpr std::uint8_t kMaximumBitDepth = 24;
    static constexpr std::uint16_t kMinimumSampleRateDivisor = 1;
    static constexpr std::uint16_t kMaximumSampleRateDivisor = 128;

    [[nodiscard]] float quantize(float sample) const noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    BitCrusherParameters parameters_{};
    float quantization_scale_ = 32768.0F;
    std::array<float, 2> held_samples_{};
    std::uint16_t frames_until_update_ = 0;
};

} // namespace tabletfx
