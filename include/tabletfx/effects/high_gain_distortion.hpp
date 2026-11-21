#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

struct HighGainDistortionParameters {
    float drive = 0.75F;
    float pre_low_cut_hz = 100.0F;
    float pre_high_cut_hz = 9000.0F;
    float post_tone_hz = 5200.0F;
    float level_db = -9.0F;
    float mix = 1.0F;
    float smoothing_ms = 10.0F;
};

// A generic high-gain waveshaper with pre-distortion low/high cuts and a
// post-distortion tone low-pass. It is not a commercial circuit emulation and
// has zero fixed algorithmic latency. It makes no oversampling or
// aliasing-performance claim.
class HighGainDistortion final : public Effect {
public:
    explicit HighGainDistortion(float sample_rate = 48000.0F,
                                HighGainDistortionParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "high-gain-distortion";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }
    [[nodiscard]] float drive() const noexcept { return target_drive_; }
    [[nodiscard]] float pre_low_cut_hz() const noexcept { return target_pre_low_cut_hz_; }
    [[nodiscard]] float pre_high_cut_hz() const noexcept { return target_pre_high_cut_hz_; }
    [[nodiscard]] float post_tone_hz() const noexcept { return target_post_tone_hz_; }
    [[nodiscard]] float level_db() const noexcept { return target_level_db_; }
    [[nodiscard]] float mix() const noexcept { return target_mix_; }

    void configure(HighGainDistortionParameters parameters) noexcept;
    void set_drive(float drive) noexcept;
    void set_pre_low_cut_hz(float frequency_hz) noexcept;
    void set_pre_high_cut_hz(float frequency_hz) noexcept;
    void set_post_tone_hz(float frequency_hz) noexcept;
    void set_level_db(float level_db) noexcept;
    void set_mix(float mix) noexcept;
    void configure_smoothing(float sample_rate, float smoothing_ms) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct ChannelState {
        float pre_high_pass_input = 0.0F;
        float pre_high_pass_output = 0.0F;
        float pre_low_pass = 0.0F;
        float post_low_pass = 0.0F;
    };

    void retarget() noexcept;
    void advance_ramp() noexcept;
    void snap_to_targets() noexcept;
    [[nodiscard]] static float bounded_drive(float drive) noexcept;
    [[nodiscard]] static float bounded_cutoff(float frequency_hz, float fallback_hz,
                                             float minimum_hz, float maximum_hz) noexcept;
    [[nodiscard]] static float bounded_level(float level_db) noexcept;
    [[nodiscard]] static float bounded_mix(float mix) noexcept;
    [[nodiscard]] static float db_to_gain(float level_db) noexcept;
    [[nodiscard]] static float low_pass_alpha(float cutoff_hz, float sample_rate) noexcept;
    [[nodiscard]] static float high_pass_alpha(float cutoff_hz, float sample_rate) noexcept;
    [[nodiscard]] static float soft_clip(float sample) noexcept;
    [[nodiscard]] std::size_t ramp_length() const noexcept;

    float sample_rate_ = 48000.0F;
    float smoothing_ms_ = 10.0F;
    float target_drive_ = 0.75F;
    float target_pre_low_cut_hz_ = 100.0F;
    float target_pre_high_cut_hz_ = 9000.0F;
    float target_post_tone_hz_ = 5200.0F;
    float target_level_db_ = -9.0F;
    float target_level_gain_ = 0.3548134F;
    float target_mix_ = 1.0F;
    float drive_gain_ = 36.25F;
    float drive_gain_step_ = 0.0F;
    float pre_high_pass_alpha_ = 0.987F;
    float pre_high_pass_alpha_step_ = 0.0F;
    float pre_low_pass_alpha_ = 0.607F;
    float pre_low_pass_alpha_step_ = 0.0F;
    float post_low_pass_alpha_ = 0.494F;
    float post_low_pass_alpha_step_ = 0.0F;
    float level_gain_ = 0.3548134F;
    float level_gain_step_ = 0.0F;
    float mix_ = 1.0F;
    float mix_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
    std::array<ChannelState, 2> channel_state_{};
};

} // namespace tabletfx
