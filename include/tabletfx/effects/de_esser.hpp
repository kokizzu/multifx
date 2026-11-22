#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx::effects {

// Project-authored controls for a split-band de-esser. The HeadRush Prime
// product page lists a De-Esser, but its public product/support material did
// not expose the model's controls, ranges, or DSP. These values and behavior
// do not claim HeadRush parameter or sound parity.
struct DeEsserParameters {
    float detector_frequency_hz = 6500.0F; // [1500, min(12000, 0.45 * Fs)] Hz.
    float detector_q = 1.5F;              // [0.5, 8].
    float threshold_dbfs = -30.0F;         // [-72, -6] dBFS.
    float max_reduction_db = 12.0F;        // [0, 18] dB.
    float attack_ms = 2.0F;                // [0, 50] ms.
    float release_ms = 80.0F;              // [5, 500] ms.
};

/*
 * Fixed-state, project-authored split-band de-esser. A band-pass detector
 * follows the absolute filtered signal with attack/release smoothing. Above
 * threshold, the effect attenuates that band and subtracts the difference
 * from the input. Attenuation ramps linearly with detector amplitude from
 * threshold through +18 dB, then reaches max_reduction_db. Other signal bands
 * are left substantially unchanged.
 *
 * Controls, bounds, defaults, detector response, and DSP are project choices;
 * they are not documented HeadRush Prime settings. Configure while the audio
 * callback is stopped. Processing supports the constructor's mono or stereo
 * channel count and does not allocate memory.
 */
class DeEsser final : public tabletfx::Effect {
public:
    static constexpr float kMinimumDetectorFrequencyHz = 1500.0F;
    static constexpr float kMaximumDetectorFrequencyHz = 12000.0F;
    static constexpr float kMinimumDetectorQ = 0.5F;
    static constexpr float kMaximumDetectorQ = 8.0F;
    static constexpr float kMinimumThresholdDbfs = -72.0F;
    static constexpr float kMaximumThresholdDbfs = -6.0F;
    static constexpr float kMinimumReductionDb = 0.0F;
    static constexpr float kMaximumReductionDb = 18.0F;
    static constexpr float kMinimumAttackMs = 0.0F;
    static constexpr float kMaximumAttackMs = 50.0F;
    static constexpr float kMinimumReleaseMs = 5.0F;
    static constexpr float kMaximumReleaseMs = 500.0F;

    explicit DeEsser(float sample_rate = 48000.0F,
                     std::uint16_t channels = 2,
                     DeEsserParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "de-esser";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] const DeEsserParameters& parameters() const noexcept {
        return parameters_;
    }

    void configure(const DeEsserParameters& parameters) noexcept;
    void reset() noexcept override;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

private:
    struct BiquadCoefficients {
        float b0 = 0.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    struct ChannelState {
        float z1 = 0.0F;
        float z2 = 0.0F;
        float detector_envelope = 0.0F;
    };

    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 192000.0F;
    static constexpr float kMaximumInputMagnitude = 1000000.0F;
    static constexpr float kDetectorReleaseDb = 18.0F;
    static constexpr float kMaximumThresholdAmplitudeRatio = 7.94328235F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float time_coefficient(float milliseconds,
                                                float sample_rate) noexcept;
    [[nodiscard]] static float finite_input(float sample) noexcept;
    void rebuild_coefficients() noexcept;
    void reset_channel(std::uint16_t channel) noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    DeEsserParameters parameters_{};
    BiquadCoefficients detector_coefficients_{};
    std::array<ChannelState, 2> states_{};
    float threshold_amplitude_ = 0.03162278F;
    float maximum_gain_ = 0.25118864F;
    float attack_coefficient_ = 0.0F;
    float release_coefficient_ = 0.0F;
};

} // namespace tabletfx::effects
