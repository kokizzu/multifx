#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class DualEngineDistortionMode : std::uint8_t {
    Blend,
    Series,
};

struct DualEngineDistortionParameters {
    // Project-authored normalized gain controls for the two independent paths.
    float gain_a = 0.25F;
    float tone_a = 0.7F;
    float gain_b = 0.6F;
    float tone_b = 0.5F;
    // Mixes A toward B in Blend mode; ignored in Series mode.
    float blend = 0.5F;
    DualEngineDistortionMode mode = DualEngineDistortionMode::Blend;
    bool bass_boost = false;
    float level_db = 0.0F;
    float smoothing_ms = 10.0F;
};

/*
 * Project-authored dual-engine distortion. The Fender Tone Master Pro Model
 * Guide v1.1.100 describes Pugilist Distortion with separate Gain A/Tone A and
 * Gain B/Tone B paths, Level, Blend, a blend-or-series Mode switch, and Bass
 * Boost. The guide does not publish numeric bounds, defaults, or DSP details.
 * All ranges, defaults, clipping, tone filtering, routing law, and bass-boost
 * behavior here are project choices; this is not Fender DSP, parameter, or
 * sound parity.
 *
 * Blend mode runs both paths in parallel and linearly mixes their outputs.
 * Series mode runs path A into path B; Blend is ignored in that mode. Each path
 * uses a project-authored soft clipper followed by a one-pole tone low-pass.
 * Bass Boost is a project-authored low-frequency pre-emphasis. Processing uses
 * fixed state and performs no allocations.
 */
class DualEngineDistortion final : public Effect {
public:
    explicit DualEngineDistortion(
        float sample_rate = 48000.0F,
        DualEngineDistortionParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "dual-engine-distortion";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   DualEngineDistortionParameters parameters) noexcept;
    void set_parameters(DualEngineDistortionParameters parameters) noexcept;

    [[nodiscard]] DualEngineDistortionParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct ChannelState {
        float bass_low = 0.0F;
        float tone_a = 0.0F;
        float tone_b = 0.0F;
    };

    [[nodiscard]] static DualEngineDistortionParameters sanitize(
        DualEngineDistortionParameters parameters) noexcept;
    [[nodiscard]] static float db_to_gain(float db) noexcept;
    [[nodiscard]] static float soft_clip(float sample) noexcept;
    [[nodiscard]] static float low_pass_alpha(float cutoff_hz,
                                              float sample_rate) noexcept;
    [[nodiscard]] float tone_alpha(float normalized_tone) const noexcept;
    [[nodiscard]] std::size_t ramp_length() const noexcept;
    void retarget() noexcept;
    void advance_ramp() noexcept;
    void snap_to_targets() noexcept;
    [[nodiscard]] float process_engine(float input, float drive_gain,
                                       float tone_coefficient,
                                       float& tone_state) const noexcept;

    float sample_rate_ = 48000.0F;
    DualEngineDistortionParameters parameters_{};
    float smoothing_ms_ = 10.0F;
    float bass_alpha_ = 0.0323F;

    float target_drive_gain_a_ = 3.25F;
    float target_tone_alpha_a_ = 0.0900F;
    float target_drive_gain_b_ = 11.2F;
    float target_tone_alpha_b_ = 0.792F;
    float target_blend_ = 0.5F;
    float target_level_gain_ = 1.0F;
    float target_bass_gain_ = 1.0F;

    float drive_gain_a_ = 3.25F;
    float drive_gain_a_step_ = 0.0F;
    float tone_alpha_a_ = 0.0900F;
    float tone_alpha_a_step_ = 0.0F;
    float drive_gain_b_ = 11.2F;
    float drive_gain_b_step_ = 0.0F;
    float tone_alpha_b_ = 0.792F;
    float tone_alpha_b_step_ = 0.0F;
    float blend_ = 0.5F;
    float blend_step_ = 0.0F;
    float level_gain_ = 1.0F;
    float level_gain_step_ = 0.0F;
    float bass_gain_ = 1.0F;
    float bass_gain_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0U;
    std::array<ChannelState, 2> channel_state_{};
};

} // namespace tabletfx
