#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

struct ChorusParameters {
    float rate_hz = 0.8F;
    float base_delay_ms = 15.0F;
    float depth_ms = 7.0F;
    float mix = 0.3F;
    float smoothing_ms = 10.0F;
};

// Low-cost sine-LFO modulated delay chorus. The wet path is interpolated
// between adjacent samples; stereo uses opposite LFO phase for width. Delay
// storage is allocated at construction, while process() is allocation-free.
class Chorus final : public Effect {
public:
    explicit Chorus(float sample_rate = 48000.0F, std::uint16_t channels = 2,
                    ChorusParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override { return "chorus"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    void set_parameters(ChorusParameters parameters) noexcept;
    [[nodiscard]] ChorusParameters parameters() const noexcept { return parameters_; }
    [[nodiscard]] float current_mix() const noexcept { return current_mix_; }

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMaximumDelayMs = 30.0F;
    static constexpr float kStorageDelayMs = 32.0F;
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
    ChorusParameters parameters_{};
    float current_base_delay_ms_ = 15.0F;
    float current_depth_ms_ = 7.0F;
    float current_mix_ = 0.3F;
    float target_base_delay_ms_ = 15.0F;
    float target_depth_ms_ = 7.0F;
    float target_mix_ = 0.3F;
    float base_delay_step_ = 0.0F;
    float depth_step_ = 0.0F;
    float mix_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
    float sine_increment_ = 0.0F;
    float cosine_increment_ = 1.0F;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    std::uint32_t normalization_counter_ = 0;
};

} // namespace tabletfx
