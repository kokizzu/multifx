#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

struct HarmonicResonatorParameters {
    float root_frequency_hz = 110.0F;
    float decay_seconds = 1.2F;
    float brightness = 0.55F;
    float mix = 0.5F;
};

/*
 * Project-authored eight-mode harmonic resonator. Each mode is a band-pass
 * resonator at an integer multiple of Root Frequency. The input excites the
 * modes continuously; Decay controls the approximate -60 dB ring time, while
 * Brightness changes the relative harmonic weights. It is not a model of a
 * particular manufacturer's Resonator, Synth, or instrument simulator.
 *
 * The processor has fixed per-channel state and allocates nothing in
 * process(). Parameter changes ramp the resonator coefficients over 10 ms.
 */
class HarmonicResonator final : public Effect {
public:
    explicit HarmonicResonator(float sample_rate = 48000.0F,
                               HarmonicResonatorParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "harmonic-resonator";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void set_parameters(HarmonicResonatorParameters parameters) noexcept;
    [[nodiscard]] HarmonicResonatorParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kModes = 8;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumRootFrequencyHz = 20.0F;
    static constexpr float kMaximumRootFrequencyHz = 1000.0F;
    static constexpr float kMinimumDecaySeconds = 0.05F;
    static constexpr float kMaximumDecaySeconds = 8.0F;
    static constexpr float kMaximumInputMagnitude = 16.0F;
    static constexpr float kMaximumStateMagnitude = 1000000.0F;
    static constexpr float kParameterSmoothingMs = 10.0F;
    static constexpr float kMaximumQ = 5000.0F;

    struct Coefficients {
        float b0 = 0.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void prepare_coefficients() noexcept;
    void advance_coefficients() noexcept;

    float sample_rate_ = 48000.0F;
    HarmonicResonatorParameters parameters_{};
    std::array<std::array<float, kModes>, 2> state_1_{};
    std::array<std::array<float, kModes>, 2> state_2_{};
    std::array<float, kModes> weights_{};
    float weight_norm_ = 1.0F;
    std::array<Coefficients, kModes> coefficients_{};
    std::array<Coefficients, kModes> targets_{};
    std::array<Coefficients, kModes> steps_{};
    std::size_t ramp_remaining_ = 0;
    std::size_t ramp_total_ = 1;
};

} // namespace tabletfx
