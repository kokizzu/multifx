#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

enum class ModulatedDelayWaveform : std::uint8_t {
    sine = 0,
    triangle = 1,
    random = 2,
    triggered_ramp = 3,
};

struct ModulatedDelayParameters {
    float time_ms = 350.0F;
    float feedback = 0.25F;
    float mix = 0.25F;
    float rate_hz = 0.8F;
    float depth_ms = 2.0F;
    ModulatedDelayWaveform waveform = ModulatedDelayWaveform::sine;
    float stereo_spread_degrees = 180.0F;
    float trigger_threshold_db = -24.0F;
};

/*
 * Project-authored modulated feedback delay. A free-running or input-triggered
 * low-frequency oscillator offsets the read time; the direct signal remains
 * immediate. Numeric bounds/defaults and the extra Random/Triggered Ramp modes
 * are project choices, not copied from any one manufacturer's algorithm.
 */
class ModulatedDelay final : public Effect {
public:
    explicit ModulatedDelay(float sample_rate = 48000.0F,
                            std::uint16_t channels = 1,
                            float max_delay_ms = 2000.0F,
                            ModulatedDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "modulated-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    [[nodiscard]] ModulatedDelayParameters parameters() const noexcept {
        return parameters_;
    }
    void configure(ModulatedDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumTimeMs = 1.0F;
    static constexpr float kMaximumTimeMs = 2000.0F;
    static constexpr float kMaximumFeedback = 0.95F;
    static constexpr float kMaximumRateHz = 10.0F;
    static constexpr float kMaximumDepthMs = 50.0F;
    static constexpr float kMinimumTriggerDb = -72.0F;
    static constexpr float kControlRampMs = 10.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float random_value(std::uint32_t& state) noexcept;
    [[nodiscard]] std::size_t samples_for_ms(float time_ms) const noexcept;
    [[nodiscard]] float read_fractional_delay(float delay_samples,
                                               std::uint16_t channel) const noexcept;
    [[nodiscard]] float read_age(std::size_t age,
                                 std::uint16_t channel) const noexcept;
    [[nodiscard]] float modulation_value(float phase) const noexcept;
    [[nodiscard]] float delay_samples_for_channel(std::uint16_t channel) const noexcept;
    void advance_ramps() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 1;
    float max_delay_ms_ = kMaximumTimeMs;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;
    ModulatedDelayParameters parameters_{};
    float trigger_amplitude_ = 0.0630957F;
    float current_time_samples_ = 0.0F;
    float target_time_samples_ = 0.0F;
    float time_step_ = 0.0F;
    float current_feedback_ = 0.25F;
    float feedback_step_ = 0.0F;
    float current_mix_ = 0.25F;
    float mix_step_ = 0.0F;
    float current_rate_hz_ = 0.8F;
    float rate_step_ = 0.0F;
    float current_depth_samples_ = 0.0F;
    float depth_step_ = 0.0F;
    float current_spread_degrees_ = 180.0F;
    float spread_step_ = 0.0F;
    std::size_t ramp_total_ = 1;
    std::size_t ramp_remaining_ = 0;
    float phase_ = 0.0F;
    float random_current_ = 0.0F;
    float random_next_ = 0.0F;
    std::uint32_t random_state_ = 0x6d2b79f5U;
    bool trigger_armed_ = true;
    bool has_triggered_ = false;
};

} // namespace tabletfx
