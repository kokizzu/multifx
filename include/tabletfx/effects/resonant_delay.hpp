#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

struct ResonantDelayParameters {
    float time_ms = 350.0F;
    float feedback = 0.35F;
    float center_hz = 800.0F;
    float resonance_q = 2.0F;
    float mix = 0.25F;
};

// Project-authored resonant-feedback delay. Each delayed repeat passes through
// a band-pass resonator before it is heard and fed back. This is not a model of
// a named commercial delay; its controls and ranges are project choices.
class ResonantDelay final : public Effect {
public:
    explicit ResonantDelay(float sample_rate = 48000.0F,
                           std::uint16_t channels = 1,
                           float max_delay_ms = 2000.0F,
                           ResonantDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "resonant-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + delay_buffer_.capacity() * sizeof(float);
    }
    [[nodiscard]] ResonantDelayParameters parameters() const noexcept {
        return parameters_;
    }
    void configure(ResonantDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct BiquadState {
        float input_1 = 0.0F;
        float input_2 = 0.0F;
        float output_1 = 0.0F;
        float output_2 = 0.0F;
    };

    void update_filter() noexcept;
    [[nodiscard]] float filter_sample(float input,
                                     std::uint16_t channel) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    float max_delay_ms_;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::size_t delay_frames_ = 1;
    std::vector<float> delay_buffer_;
    ResonantDelayParameters parameters_{};
    std::array<BiquadState, 2> filter_state_{};
    float b0_ = 0.0F;
    float b1_ = 0.0F;
    float b2_ = 0.0F;
    float a1_ = 0.0F;
    float a2_ = 0.0F;
};

} // namespace tabletfx
