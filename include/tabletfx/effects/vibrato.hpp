#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tabletfx {

struct VibratoParameters {
    float rate_hz = 5.0F;
    float depth_ms = 0.35F;
    float mix = 1.0F;
    float smoothing_ms = 10.0F;
};

// Sine-LFO fractional-delay vibrato with a fixed 2 ms center delay.
// The bounded delay ring is allocated at construction; process() allocates
// nothing. At 100% wet, its tap moves from 0.5 to 3.5 ms at maximum depth.
class Vibrato final : public Effect {
public:
    explicit Vibrato(float sample_rate = 48000.0F, std::uint16_t channels = 2,
                     VibratoParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override { return "vibrato"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    void set_parameters(VibratoParameters parameters) noexcept;
    [[nodiscard]] VibratoParameters parameters() const noexcept { return parameters_; }

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kCenterDelayMs = 2.0F;
    static constexpr float kMinimumDelayMs = 0.5F;
    static constexpr float kMaximumDepthMs = 1.5F;
    static constexpr float kStorageDelayMs = 4.0F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void prepare_rate() noexcept;
    void retarget(bool immediate) noexcept;
    void advance_modulator() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;
    VibratoParameters parameters_{};
    float current_depth_ms_ = 0.35F;
    float current_mix_ = 1.0F;
    float target_depth_ms_ = 0.35F;
    float target_mix_ = 1.0F;
    float depth_step_ = 0.0F;
    float mix_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
    float sine_increment_ = 0.0F;
    float cosine_increment_ = 1.0F;
    float target_sine_increment_ = 0.0F;
    float target_cosine_increment_ = 1.0F;
    float sine_increment_step_ = 0.0F;
    float cosine_increment_step_ = 0.0F;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    std::uint32_t normalization_counter_ = 0;
};

} // namespace tabletfx
