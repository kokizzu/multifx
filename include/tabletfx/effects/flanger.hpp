#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

enum class FlangerFeedbackPolarity : std::uint8_t {
    Positive,
    Negative,
};

struct FlangerParameters {
    float rate_hz = 0.25F;
    float base_delay_ms = 2.0F;
    float depth_ms = 1.0F;
    float feedback = 0.35F;
    FlangerFeedbackPolarity feedback_polarity = FlangerFeedbackPolarity::Positive;
    float mix = 0.35F;
    float right_channel_phase_degrees = 180.0F;
    float smoothing_ms = 10.0F;
};

// Short sine-LFO modulated delay flanger with bounded signed feedback.
// Delay storage is allocated at construction; process() is allocation-free.
class Flanger final : public Effect {
public:
    explicit Flanger(float sample_rate = 48000.0F, std::uint16_t channels = 2,
                     FlangerParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override { return "flanger"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    void set_parameters(FlangerParameters parameters) noexcept;
    [[nodiscard]] FlangerParameters parameters() const noexcept { return parameters_; }
    [[nodiscard]] float current_feedback() const noexcept { return current_feedback_; }

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumDelayMs = 0.5F;
    static constexpr float kMaximumDelayMs = 10.0F;
    static constexpr float kStorageDelayMs = 12.0F;
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
    FlangerParameters parameters_{};
    float current_base_delay_ms_ = 2.0F;
    float current_depth_ms_ = 1.0F;
    float current_feedback_ = 0.35F;
    float current_mix_ = 0.35F;
    float right_phase_sine_ = 0.0F;
    float right_phase_cosine_ = -1.0F;
    float target_right_phase_sine_ = 0.0F;
    float target_right_phase_cosine_ = -1.0F;
    float right_phase_sine_step_ = 0.0F;
    float right_phase_cosine_step_ = 1.0F;
    float target_base_delay_ms_ = 2.0F;
    float target_depth_ms_ = 1.0F;
    float target_feedback_ = 0.35F;
    float target_mix_ = 0.35F;
    float base_delay_step_ = 0.0F;
    float depth_step_ = 0.0F;
    float feedback_step_ = 0.0F;
    float mix_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
    float sine_increment_ = 0.0F;
    float cosine_increment_ = 1.0F;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    std::uint32_t normalization_counter_ = 0;
};

} // namespace tabletfx
