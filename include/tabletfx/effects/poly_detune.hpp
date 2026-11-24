#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

struct PolyDetuneParameters {
    // These controls describe the project's short, modulated-delay effect.
    // They are not documented Line 6 Poly Detune parameter values.
    float rate_hz = 0.25F;
    float depth_ms = 0.75F;
    std::uint8_t voice_count = 2;
    float width = 0.75F;
    float mix = 0.25F;
    float level_db = 0.0F;
    float smoothing_ms = 10.0F;
};

// Project-authored polyphonic detune-inspired micro-delay. It processes the
// complete waveform without note tracking, using one shared short ring buffer
// and up to two time-modulated taps. This is not a static pitch shifter and is
// not a recreation of Line 6 DSP. The direct path is immediate; the wet taps
// are delayed. Storage is allocated at construction and process() allocates no
// memory or evaluates trigonometric functions per sample.
class PolyDetune final : public Effect {
public:
    explicit PolyDetune(float sample_rate = 48000.0F,
                        std::uint16_t channels = 2,
                        PolyDetuneParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "poly-detune";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    void set_parameters(PolyDetuneParameters parameters) noexcept;
    [[nodiscard]] PolyDetuneParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float current_mix() const noexcept { return current_mix_; }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kBaseDelayMs = 5.0F;
    static constexpr float kMaximumDepthMs = 2.0F;
    static constexpr float kStorageDelayMs = 9.0F;
    static constexpr float kMaximumSignal = 8.0F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void prepare_rate() noexcept;
    void retarget(bool immediate) noexcept;
    void advance_modulator() noexcept;
    [[nodiscard]] float read_interpolated(std::size_t channel,
                                          float delay_frames) const noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;
    PolyDetuneParameters parameters_{};

    float current_depth_ms_ = 0.75F;
    float current_width_ = 0.75F;
    float current_mix_ = 0.25F;
    float current_level_gain_ = 1.0F;
    float target_depth_ms_ = 0.75F;
    float target_width_ = 0.75F;
    float target_mix_ = 0.25F;
    float target_level_gain_ = 1.0F;
    float depth_step_ = 0.0F;
    float width_step_ = 0.0F;
    float mix_step_ = 0.0F;
    float level_gain_step_ = 0.0F;
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
