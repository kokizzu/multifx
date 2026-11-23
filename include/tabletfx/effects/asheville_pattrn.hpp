#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

inline constexpr std::size_t project_asheville_band_count = 8U;
inline constexpr std::size_t project_asheville_pattern_count = 8U;

enum class AshevilleFrequencyBank : std::uint8_t {
    Bass,
    Mids,
};

struct ProjectAshevillePattrnParameters {
    AshevilleFrequencyBank bank = AshevilleFrequencyBank::Bass;
    // Project-authored pattern index, clamped to [0, 7]. It does not copy the
    // MF-105M's 24 factory patterns.
    std::uint8_t pattern = 0U;
    // Project-authored per-band gain range [0, 1].
    std::array<float, project_asheville_band_count> band_level{
        1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F};
    // Project-authored ranges: drive [0, 18] dB; output [-24, 12] dB.
    float drive_db = 0.0F;
    float output_db = 0.0F;
    // Project-authored range [0, 1].
    float mix = 0.5F;
    // Project-authored range [0.05, 10] full pattern cycles per second,
    // not step rate.
    float pattern_rate_hz = 0.75F;
    // Project-authored range [0.5, 12] for resonant band-pass bands. The first
    // BASS low-pass band instead uses a fixed project-authored Butterworth Q.
    float resonance_q = 3.0F;
    // Project-authored range [0, 1], mapped linearly to 1-40 ms attack and
    // 10-600 ms release. 0 is sharper/shorter; 1 is slower/longer.
    float envelope_shape = 0.5F;
    // Project-authored smoothing range [0, 1000] ms for parameter changes.
    float smoothing_ms = 10.0F;
};

// Project-authored eight-band filter bank with per-band sequenced
// envelopes and eight original eight-step patterns. Its parameter defaults,
// ranges, pattern count, and DSP are project choices. The name identifies the
// documented Stadium Filter model that motivated it; it does not claim Line 6
// or Moog DSP, parameter, pattern, or sound parity.
class ProjectAshevillePattrn final : public Effect {
public:
    explicit ProjectAshevillePattrn(
        float sample_rate = 48000.0F,
        ProjectAshevillePattrnParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "project-asheville-pattrn";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    [[nodiscard]] ProjectAshevillePattrnParameters parameters() const noexcept {
        return parameters_;
    }

    void configure(float sample_rate,
                   ProjectAshevillePattrnParameters parameters) noexcept;
    void set_parameters(ProjectAshevillePattrnParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct BiquadCoefficients {
        float b0 = 0.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    struct BiquadState {
        float z1 = 0.0F;
        float z2 = 0.0F;
    };

    [[nodiscard]] static ProjectAshevillePattrnParameters sanitize(
        ProjectAshevillePattrnParameters parameters) noexcept;
    [[nodiscard]] static float db_to_gain(float decibels) noexcept;
    [[nodiscard]] static float alpha_for_ms(float sample_rate,
                                            float milliseconds) noexcept;
    void update_filter_coefficients() noexcept;
    void update_envelope_targets() noexcept;
    void update_envelope_coefficients() noexcept;
    void retarget() noexcept;
    void advance_ramp() noexcept;
    void advance_sequence() noexcept;
    void advance_envelopes() noexcept;
    void snap_to_targets() noexcept;
    [[nodiscard]] float process_band(float input, std::uint16_t channel,
                                     std::size_t band) noexcept;

    float sample_rate_ = 48000.0F;
    ProjectAshevillePattrnParameters parameters_{};
    std::array<BiquadCoefficients, project_asheville_band_count>
        coefficients_{};
    std::array<std::array<BiquadState, project_asheville_band_count>, 2U>
        filter_state_{};
    std::array<float, project_asheville_band_count> envelope_{};
    std::array<float, project_asheville_band_count> envelope_target_{};
    std::array<float, project_asheville_band_count> band_level_{};
    std::array<float, project_asheville_band_count> band_level_target_{};
    std::array<float, project_asheville_band_count> band_level_step_{};
    float attack_alpha_ = 0.0F;
    float release_alpha_ = 0.0F;
    float drive_gain_ = 1.0F;
    float drive_gain_target_ = 1.0F;
    float drive_gain_step_ = 0.0F;
    float output_gain_ = 1.0F;
    float output_gain_target_ = 1.0F;
    float output_gain_step_ = 0.0F;
    float mix_ = 0.5F;
    float mix_target_ = 0.5F;
    float mix_step_ = 0.0F;
    float pattern_rate_hz_ = 0.75F;
    float pattern_rate_target_hz_ = 0.75F;
    float pattern_rate_step_ = 0.0F;
    float pattern_phase_ = 0.0F;
    std::uint8_t step_index_ = 0U;
    std::size_t ramp_remaining_ = 0U;
};

} // namespace tabletfx
