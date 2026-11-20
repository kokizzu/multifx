#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

// Real-time chain controls for input mute, dry/wet bypass, and panic mute.
// The caller owns all buffers; processing performs no allocation or locking.
class SignalControls final {
public:
    explicit SignalControls(float sample_rate = 48000.0F,
                            float ramp_ms = 10.0F) noexcept;

    [[nodiscard]] std::size_t memory_bytes() const noexcept { return sizeof(*this); }
    [[nodiscard]] bool input_muted() const noexcept { return input_muted_; }
    [[nodiscard]] bool global_bypassed() const noexcept { return global_bypassed_; }
    [[nodiscard]] bool panic_muted() const noexcept { return panic_muted_; }
    [[nodiscard]] float mix() const noexcept { return mix_target_; }

    void set_input_muted(bool muted) noexcept;
    void set_global_bypassed(bool bypassed) noexcept;
    void set_panic_muted(bool muted) noexcept;
    void set_mix(float wet_mix) noexcept;
    void process_input(std::span<const float> input, std::span<float> output,
                       std::uint16_t channels) noexcept;
    void process_output(std::span<const float> dry, std::span<const float> wet,
                        std::span<float> output, std::uint16_t channels) noexcept;
    void reset() noexcept;

private:
    [[nodiscard]] std::size_t ramp_frames() const noexcept;
    static void advance(float& current, float target, float step,
                        std::size_t& remaining) noexcept;
    void retarget_mix() noexcept;

    float sample_rate_ = 48000.0F;
    float ramp_ms_ = 10.0F;
    float input_gain_ = 1.0F;
    float input_gain_target_ = 1.0F;
    float input_gain_step_ = 0.0F;
    std::size_t input_gain_remaining_ = 0;
    float mix_target_ = 1.0F;
    float effective_mix_target_ = 1.0F;
    float mix_ = 1.0F;
    float mix_step_ = 0.0F;
    std::size_t mix_remaining_ = 0;
    float panic_gain_ = 1.0F;
    float panic_gain_step_ = 0.0F;
    std::size_t panic_gain_remaining_ = 0;
    bool input_muted_ = false;
    bool global_bypassed_ = false;
    bool panic_muted_ = false;
};

} // namespace tabletfx
