#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/delay_timing.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace tabletfx {

enum class AutoSwellDelayTaper : std::uint8_t {
    slow,
    normal,
    fast,
};

struct AutoSwellDelayParameters {
    float time_ms = 350.0F;
    float feedback = 0.25F;
    float mix = 0.25F;
    DelayTimeMode time_mode = DelayTimeMode::milliseconds;
    DelayDivision tap_division = DelayDivision::quarter;
    float tempo_bpm = 120.0F;
    float swell_ms = 300.0F;
    float mod = 0.15F;
    float tone = 0.7F;
    float sense_db = -36.0F;
    AutoSwellDelayTaper taper = AutoSwellDelayTaper::normal;
    bool dry = false;
    bool spillover = false;
};

/*
 * Project-authored auto-swell delay. The incoming signal drives a linked
 * onset detector and a bounded swell envelope before entering the delay loop.
 * Dry selects whether the direct signal follows that swell as well. Mod is a
 * small, fixed-rate delay-time modulation and Tone filters repeats only.
 *
 * Fender's Tone Master Pro Model Guide v1.1.100 documents Auto-Swell Delay
 * controls named Time, Feedback, Mix, Tap Division, Swell, Mod, Tone, Sense,
 * Taper (slow/normal/fast), Dry, and Spillover, but supplies no numeric ranges
 * for this block. These ranges, taper curves, detector, filter, modulation,
 * and bypass behavior are project choices; this class does not claim Fender
 * algorithm, settings, or sound parity. Source checked 2026-11-22:
 * https://www.fmicassets.com/Damroot/Original/10018/OM_2274900000_TONE-MASTER-PRO-Model-Guide_English.pdf
 *
 * Construct with the maximum delay storage needed. configure() may be called
 * off the audio thread and never resizes storage. process() performs no
 * allocation. At 48 kHz, stereo, and the default 2 s maximum, the delay ring
 * occupies about 0.74 MiB.
 */
class AutoSwellDelay final : public Effect {
public:
    explicit AutoSwellDelay(float sample_rate = 48000.0F,
                            std::uint16_t channels = 1,
                            float max_delay_ms = 2000.0F,
                            AutoSwellDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "auto-swell-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }
    [[nodiscard]] AutoSwellDelayParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_time_ms() const noexcept {
        return static_cast<float>(static_cast<double>(target_delay_samples_) *
                                  1000.0 / static_cast<double>(sample_rate_));
    }
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }

    // All controls and bounds are project-authored. Delay memory remains fixed
    // at the constructor's maximum; changing Time does not allocate.
    void configure(AutoSwellDelayParameters parameters) noexcept;
    void set_enabled(bool enabled) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 192000.0F;
    static constexpr float kMinimumTimeMs = 1.0F;
    static constexpr float kMaximumTimeMs = 2000.0F;
    static constexpr float kMaximumFeedback = 0.95F;
    static constexpr float kMaximumSwellMs = 5000.0F;
    static constexpr float kMaximumSenseDb = 0.0F;
    static constexpr float kMinimumSenseDb = -72.0F;
    static constexpr float kMaximumMod = 1.0F;
    static constexpr float kMaximumModMs = 2.0F;
    static constexpr float kMaximumTone = 1.0F;
    static constexpr float kMinimumTone = 0.0F;
    static constexpr float kControlRampMs = 10.0F;
    static constexpr float kDetectorAttackMs = 1.0F;
    static constexpr float kDetectorReleaseMs = 45.0F;
    static constexpr float kSwellReleaseMs = 80.0F;
    static constexpr float kModRateHz = 0.25F;
    static constexpr float kTailSilenceThreshold = 1.0e-6F;
    static constexpr float kMaximumSignal = 64.0F;
    static constexpr std::size_t kTaperSteps = 256;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static AutoSwellDelayTaper valid_taper(
        AutoSwellDelayTaper taper) noexcept;
    [[nodiscard]] static float beats_for_division(
        DelayDivision division) noexcept;
    [[nodiscard]] static float one_pole_coefficient(float sample_rate,
                                                    float milliseconds) noexcept;
    [[nodiscard]] std::size_t samples_for_ms(float time_ms) const noexcept;
    [[nodiscard]] float current_delay_samples() const noexcept;
    [[nodiscard]] float read_fractional_delay(float delay_samples,
                                              std::uint16_t channel) const noexcept;
    [[nodiscard]] float read_age(std::size_t age,
                                 std::uint16_t channel) const noexcept;
    [[nodiscard]] float swell_gain() const noexcept;
    [[nodiscard]] float lookup_taper(float position) const noexcept;
    void build_taper_table() noexcept;
    void advance_ramps() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 1;
    float max_delay_ms_ = kMaximumTimeMs;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::size_t valid_frames_ = 0;
    std::vector<float> storage_;

    AutoSwellDelayParameters parameters_{};
    float target_delay_samples_ = 0.0F;
    float delay_transition_from_samples_ = 0.0F;
    std::size_t delay_transition_total_ = 1;
    std::size_t delay_transition_remaining_ = 0;

    float feedback_ = 0.0F;
    float feedback_step_ = 0.0F;
    float mix_ = 0.0F;
    float mix_step_ = 0.0F;
    float swell_step_ = 0.0F;
    float swell_step_step_ = 0.0F;
    float mod_depth_samples_ = 0.0F;
    float mod_depth_step_ = 0.0F;
    float tone_alpha_ = 0.0F;
    float tone_alpha_step_ = 0.0F;
    float target_tone_alpha_ = 0.0F;
    std::size_t ramp_total_ = 1;
    std::size_t ramp_remaining_ = 0;

    float sense_threshold_ = 0.0158489F;
    float detector_attack_coefficient_ = 0.0F;
    float detector_release_coefficient_ = 0.0F;
    float detector_envelope_ = 0.0F;
    float swell_phase_ = 0.0F;
    bool triggered_ = false;
    std::array<float, 2> tone_state_{};
    std::array<float, kTaperSteps + 1U> taper_table_{};

    float mod_phase_ = 0.0F;
    float mod_phase_step_ = 0.0F;
    std::size_t tail_quiet_frames_ = 0;
    bool enabled_ = true;
};

} // namespace tabletfx
