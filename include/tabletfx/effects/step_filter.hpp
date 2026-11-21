#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/rhythmic_slicer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class StepFilterMode : std::uint8_t {
    LowPass,
    BandPass,
    HighPass,
    Notch,
};

struct StepFilterParameters {
    bool tempo_sync = true;
    float tempo_bpm = 120.0F;
    RhythmicDivision division = RhythmicDivision::Sixteenth;
    float rate_hz = 8.0F;
    StepFilterMode mode = StepFilterMode::LowPass;
    float minimum_cutoff_hz = 120.0F;
    float maximum_cutoff_hz = 6000.0F;
    float resonance_q = 0.707F;
    float depth = 1.0F;
    float shape = 0.15F;
    float smoothing_ms = 4.0F;
    float mix = 1.0F;
    float phase = 0.0F;
    std::array<float, 16> steps{
        0.0F, 0.72F, 0.18F, 0.88F, 0.32F, 0.62F, 0.08F, 0.96F,
        0.24F, 0.78F, 0.12F, 0.90F, 0.38F, 0.68F, 0.04F, 0.84F,
    };
};

/*
 * Project-authored 16-step resonant filter. The pattern values sweep a
 * logarithmic cutoff range; the selected filter response can be low-pass,
 * band-pass, high-pass, or notch. This is distinct from MOD-16's amplitude
 * gate. Parameters, defaults, interpolation, and numeric ranges are entirely
 * project-authored.
 *
 * Manufacturer source limits checked 2026-11-21:
 * - Fender Tone Master Pro release notes list Step Filter and Step Filter
 *   Delay, but do not publish individual controls or ranges:
 *   https://support.fender.com/hc/en-us/articles/46768129660315-Fender-Tone-Master-Pro-Firmware-and-Pro-Control-App-Updates
 * - The Line 6 Helix Stadium effects guide describes block Rate/Depth controls
 *   and says model-specific values are omitted; its public model list does not
 *   provide a step-filter parameter table:
 *   https://manuals.line6.com/en/helix-stadium/rev-d-v1.3/effect-blocks
 *   https://line6.com/helix-stadium-models/
 *
 * This implementation does not claim a Fender or Line 6 algorithm match.
 * It supports mono/stereo interleaved audio, uses fixed preallocated state,
 * performs no allocation in process(), and clamps its internal audio input to
 * +/-16 to prevent extreme finite samples from poisoning filter state.
 */
class StepFilter final : public Effect {
public:
    explicit StepFilter(float sample_rate = 48000.0F,
                        StepFilterParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "step-filter";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void set_parameters(StepFilterParameters parameters) noexcept;
    [[nodiscard]] StepFilterParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_step_rate_hz() const noexcept {
        return effective_step_rate_hz_;
    }
    [[nodiscard]] float current_cutoff_hz() const noexcept {
        return cutoff_from_position(smoothed_position_);
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumRateHz = 0.05F;
    static constexpr float kMaximumRateHz = 60.0F;
    static constexpr float kMinimumBpm = 30.0F;
    static constexpr float kMaximumBpm = 300.0F;
    static constexpr float kMinimumCutoffHz = 20.0F;
    static constexpr float kMaximumCutoffHz = 18000.0F;
    static constexpr float kMinimumQ = 0.5F;
    static constexpr float kMaximumQ = 12.0F;
    static constexpr float kMaximumSmoothingMs = 100.0F;
    static constexpr float kMaximumInputMagnitude = 16.0F;
    static constexpr std::size_t kCoefficientSteps = 256;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static StepFilterMode sanitize_mode(StepFilterMode mode) noexcept;
    [[nodiscard]] static RhythmicDivision sanitize_division(
        RhythmicDivision division) noexcept;
    [[nodiscard]] static float beats_per_step(RhythmicDivision division) noexcept;
    [[nodiscard]] float pattern_value(float cycle_phase) const noexcept;
    [[nodiscard]] float cutoff_from_position(float position) const noexcept;
    [[nodiscard]] float g_from_position(float position) const noexcept;
    void prepare_parameters() noexcept;
    void prepare_rate() noexcept;
    void prepare_coefficient_table() noexcept;
    [[nodiscard]] float filter_sample(float input, std::uint16_t channel,
                                      float g) noexcept;

    float sample_rate_ = 48000.0F;
    StepFilterParameters parameters_{};
    std::array<float, 2> integrator_1_{};
    std::array<float, 2> integrator_2_{};
    std::array<float, kCoefficientSteps + 1U> g_table_{};
    float resonance_k_ = 1.0F / 0.707F;
    float smoothing_alpha_ = 1.0F;
    float effective_step_rate_hz_ = 8.0F;
    float phase_increment_ = 8.0F / (48000.0F * 16.0F);
    float phase_ = 0.0F;
    float smoothed_position_ = 0.5F;
};

} // namespace tabletfx
