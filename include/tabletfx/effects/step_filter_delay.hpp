#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/delay_timing.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

enum class StepFilterDelayMode : std::uint8_t {
    LowPass,
    BandPass,
    HighPass,
    Notch,
};

struct StepFilterDelayParameters {
    float delay_time_ms = 350.0F;
    bool delay_tempo_sync = false;
    DelayDivision delay_division = DelayDivision::quarter;
    bool step_tempo_sync = true;
    float tempo_bpm = 120.0F;
    DelayDivision step_division = DelayDivision::sixteenth;
    float step_rate_hz = 8.0F;
    float feedback = 0.35F;
    float mix = 0.25F;
    StepFilterDelayMode filter_mode = StepFilterDelayMode::LowPass;
    float minimum_cutoff_hz = 120.0F;
    float maximum_cutoff_hz = 6000.0F;
    float resonance_q = 0.707F;
    float depth = 1.0F;
    float shape = 0.15F;
    float smoothing_ms = 4.0F;
    float step_phase = 0.0F;
    std::array<float, 16> steps{
        0.0F, 0.72F, 0.18F, 0.88F, 0.32F, 0.62F, 0.08F, 0.96F,
        0.24F, 0.78F, 0.12F, 0.90F, 0.38F, 0.68F, 0.04F, 0.84F,
    };
};

/*
 * Project-authored step-filter feedback delay prototype.
 *
 * Fender's official firmware notes place Step Filter Delay in the Delay
 * category beginning with v1.8.45 (3 June 2026). Fender describes it as an
 * original step delay for unusual rhythms and textures, but does not publish
 * controls, ranges, defaults, step count, filter topology, or stereo behavior.
 * The parameter set, 16-step pattern, resonant state-variable filter, tempo
 * behavior, feedback path, and dual-mono stereo handling below are therefore
 * project choices and do not claim Fender Tone Master Pro parity.
 *
 * Official references checked 23 November 2026:
 * - https://support.fender.com/hc/en-us/articles/46768129660315-Fender-Tone-Master-Pro-Firmware-and-Pro-Control-App-Updates
 *   (v1.8.45 adds the model; v1.8.58 fixes a possible crash in step effects;
 *   the page lists v1.8.85 as current.)
 * - https://www.fender.com/articles/instruments/the-latest-tone-master-pro-update-is-here
 *   (high-level description only.)
 *
 * The cutoff pattern advances as a shared clock for both channels. Delay and
 * filter state are independent per channel with no crossfeed. Processing uses
 * a buffer allocated in the constructor and does not allocate in process().
 */
class StepFilterDelay final : public Effect {
public:
    explicit StepFilterDelay(float sample_rate = 48000.0F,
                             std::uint16_t channels = 1,
                             float max_delay_ms = 2000.0F,
                             StepFilterDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "step-filter-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + delay_buffer_.capacity() * sizeof(float);
    }

    [[nodiscard]] StepFilterDelayParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_delay_time_ms() const noexcept {
        return effective_delay_time_ms_;
    }
    [[nodiscard]] float effective_step_rate_hz() const noexcept {
        return effective_step_rate_hz_;
    }
    [[nodiscard]] float current_cutoff_hz() const noexcept;

    void configure(StepFilterDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumDelayMs = 1.0F;
    static constexpr float kMaximumDelayMs = 2000.0F;
    static constexpr float kMinimumRateHz = 0.05F;
    static constexpr float kMaximumRateHz = 60.0F;
    static constexpr float kMinimumBpm = 30.0F;
    static constexpr float kMaximumBpm = 300.0F;
    static constexpr float kMinimumCutoffHz = 20.0F;
    static constexpr float kMaximumCutoffHz = 18000.0F;
    static constexpr float kMinimumQ = 0.5F;
    static constexpr float kMaximumQ = 12.0F;
    static constexpr float kMaximumSmoothingMs = 100.0F;
    static constexpr float kMaximumBufferSample = 64.0F;
    static constexpr float kMaximumFilterState = 1000000.0F;
    static constexpr std::size_t kCoefficientSteps = 256;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static DelayDivision sanitize_division(
        DelayDivision division) noexcept;
    [[nodiscard]] static StepFilterDelayMode sanitize_mode(
        StepFilterDelayMode mode) noexcept;
    [[nodiscard]] static float beats_per_division(
        DelayDivision division) noexcept;
    [[nodiscard]] float pattern_value(float cycle_phase) const noexcept;
    [[nodiscard]] float cutoff_from_position(float position) const noexcept;
    [[nodiscard]] float g_from_position(float position) const noexcept;
    [[nodiscard]] float filter_sample(float input, std::uint16_t channel,
                                      float g) noexcept;
    void prepare_step_clock() noexcept;
    void prepare_filter() noexcept;

    struct FilterState {
        float integrator_1 = 0.0F;
        float integrator_2 = 0.0F;
    };

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 1;
    float max_delay_ms_ = kMaximumDelayMs;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::size_t delay_frames_ = 1;
    std::vector<float> delay_buffer_;
    StepFilterDelayParameters parameters_{};
    std::array<FilterState, 2> filter_state_{};
    std::array<float, kCoefficientSteps + 1U> g_table_{};
    float effective_delay_time_ms_ = 350.0F;
    float effective_step_rate_hz_ = 8.0F;
    float phase_increment_ = 8.0F / (48000.0F * 16.0F);
    float phase_ = 0.0F;
    float smoothed_position_ = 0.5F;
    float resonance_k_ = 1.0F / 0.707F;
    float smoothing_alpha_ = 1.0F;
};

} // namespace tabletfx
