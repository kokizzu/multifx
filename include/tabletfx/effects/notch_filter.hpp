#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

struct NotchFilterParameters {
    float frequency_hz = 1000.0F;
    float q = 4.0F;
    float mix = 1.0F;
};

/*
 * Project-authored static notch filter, distinct from StepFilter's moving
 * Notch response. Defaults, bounds, smoothing, and implementation are not an
 * emulation of a commercial effect.
 *
 * Manufacturer evidence checked 2026-11-21:
 * - Fender Tone Master Pro firmware v1.7.53 names Notch Filter but gives no
 *   model-specific controls or numeric ranges:
 *   https://support.fender.com/hc/en-us/articles/46768129660315-Fender-Tone-Master-Pro-Firmware-and-Pro-Control-App-Updates
 * - Fractal's currently linked Blocks Guide lists Notch as a Filter type and
 *   generic Frequency/Q controls, without numeric ranges. The guide is an
 *   older platform reference; FM9 downloads lists firmware v12.0 for FM9
 *   Original/Turbo, and does not name Mark II Turbo separately:
 *   https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
 *   https://www.fractalaudio.com/fm9-downloads/
 * - Line 6's Helix Stadium catalogue has no explicitly named Notch Filter:
 *   https://line6.com/helix-stadium-models/
 *
 * The processor uses fixed per-channel state and allocates nothing in
 * process(). A 10 ms parameter ramp interpolates stable state-variable
 * coefficients and wet mix when controls change.
 */
class NotchFilter final : public Effect {
public:
    explicit NotchFilter(float sample_rate = 48000.0F,
                         NotchFilterParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "notch-filter";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void set_parameters(NotchFilterParameters parameters) noexcept;
    [[nodiscard]] NotchFilterParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumFrequencyHz = 20.0F;
    static constexpr float kMaximumFrequencyHz = 18000.0F;
    static constexpr float kMinimumQ = 0.5F;
    static constexpr float kMaximumQ = 12.0F;
    static constexpr float kMaximumInputMagnitude = 16.0F;
    static constexpr float kMaximumStateMagnitude = 1000000.0F;
    static constexpr float kParameterSmoothingMs = 10.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void advance_parameters() noexcept;
    [[nodiscard]] float filter_sample(float input, std::uint16_t channel,
                                     float g, float k) noexcept;

    float sample_rate_ = 48000.0F;
    NotchFilterParameters parameters_{};
    std::array<float, 2> integrator_1_{};
    std::array<float, 2> integrator_2_{};
    float g_ = 0.0F;
    float target_g_ = 0.0F;
    float g_step_ = 0.0F;
    float k_ = 0.25F;
    float target_k_ = 0.25F;
    float k_step_ = 0.0F;
    float mix_ = 1.0F;
    float target_mix_ = 1.0F;
    float mix_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
    std::size_t ramp_total_ = 1;
};

} // namespace tabletfx
