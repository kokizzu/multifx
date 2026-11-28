#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

enum class StudioDelayPanMode : std::uint8_t {
    Sine,
    Triangle,
    Envelope,
};

struct StudioPanDelayParameters {
    // Time/feedback/Mix defaults and Feedback bounds are project-authored.
    float delay_time_ms = 350.0F;
    float feedback = 0.25F; // project range 0..0.9
    float mix = 0.25F;      // project range 0..1, linear dry/wet

    // Fender documents Pan Rate, Pan Depth, and these three modes, but not
    // numeric ranges or defaults. These ranges/defaults are project choices.
    float pan_rate_hz = 0.5F; // project range 0.05..20 Hz
    float pan_depth = 0.75F;  // project range 0..1 excursion
    StudioDelayPanMode pan_mode = StudioDelayPanMode::Sine;

    // Project-authored hard bypass. It passes dry audio and stops new input
    // entering the delay line while feedback tails decay silently.
    bool bypass = false;
};

/*
 * Project-authored stereo delay informed by the Studio Delay controls in the
 * Fender Tone Master Pro Model Guide v1.1.100 / Rev. A, printed p. 50. That
 * guide lists Time, Tap Division, Feedback, Mix, Low Cut, High Cut, Mod Rate,
 * Mod Depth, Mod Mode, Pan Rate, Pan Depth, Pan Mode (Sine/Tri/Env),
 * and Spillover. Fender firmware v1.6.34 later documents Studio Delay Time
 * from 30 to 2000 ms. This implementation uses that documented current time
 * span; its 350 ms default, Feedback range/default, Mix default, Pan Rate/Depth
 * bounds/defaults, envelope interpretation, panning law, and smoothing are
 * project-authored choices, not Fender defaults or DSP.
 *
 * The implemented subset is a fixed-time stereo feedback delay with wet-repeat
 * panning. The project does not implement Tap Division/tempo sync, Low/High Cut,
 * delay-time Mod controls, or Spillover, and does not reproduce Fender DSP or
 * sound. The guide does not define Envelope behavior; here it means a smoothed
 * input-level detector reduces Pan Depth as input level rises. Mono input is
 * accepted but cannot produce stereo panning. The dry path has zero fixed
 * latency. An active instance allocates its ring during construction; a
 * bypassed instance defers allocation until prepare() or activation. process()
 * performs no allocation.
 *
 * Sources:
 * https://www.fmicassets.com/Damroot/Original/10018/
 * OM_2274900000_TONE-MASTER-PRO-Model-Guide_English.pdf
 * https://support.fender.com/hc/en-us/articles/46768129660315-
 * Fender-Tone-Master-Pro-Firmware-and-Pro-Control-App-Updates
 */
class StudioPanDelay final : public Effect {
public:
    explicit StudioPanDelay(float sample_rate = 48000.0F,
                            StudioPanDelayParameters parameters = {});
    StudioPanDelay(float sample_rate, float max_delay_ms,
                   StudioPanDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "studio-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }
    [[nodiscard]] std::size_t fixed_latency_frames() const noexcept {
        return 0U;
    }
    [[nodiscard]] float maximum_delay_ms() const noexcept;
    [[nodiscard]] float effective_delay_ms() const noexcept;
    [[nodiscard]] float effective_feedback() const noexcept {
        return current_feedback_;
    }
    [[nodiscard]] float effective_mix() const noexcept {
        return current_mix_;
    }
    [[nodiscard]] StudioPanDelayParameters parameters() const noexcept {
        return parameters_;
    }

    void configure(StudioPanDelayParameters parameters) noexcept;
    // Allocate and clear the fixed ring before enabling processing. Safe to
    // call repeatedly; never allocates from process().
    [[nodiscard]] bool prepare() noexcept;
    void set_delay_time_ms(float delay_time_ms) noexcept;
    void set_feedback(float feedback) noexcept;
    void set_mix(float mix) noexcept;
    void set_pan_rate_hz(float rate_hz) noexcept;
    void set_pan_depth(float depth) noexcept;
    void set_pan_mode(StudioDelayPanMode mode) noexcept;
    void set_bypass(bool bypass) noexcept;

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumDelayMs = 30.0F;
    static constexpr float kMaximumDelayMs = 2000.0F;
    static constexpr float kMinimumFeedback = 0.0F;
    static constexpr float kMaximumFeedback = 0.9F;
    static constexpr float kMinimumRateHz = 0.05F;
    static constexpr float kMaximumRateHz = 20.0F;
    static constexpr float kDefaultDelayMs = 350.0F;
    static constexpr float kDefaultFeedback = 0.25F;
    static constexpr float kDefaultMix = 0.25F;
    static constexpr float kParameterRampMs = 10.0F;
    static constexpr float kEnvelopeAttackMs = 8.0F;
    static constexpr float kEnvelopeReleaseMs = 120.0F;
    static constexpr float kEnvelopeFullScale = 0.5F;
    static constexpr float kMaximumSignal = 8.0F;
    static constexpr std::size_t kMaximumStorageBytes = 2U * 1024U * 1024U;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static StudioDelayPanMode sanitize_mode(
        StudioDelayPanMode mode) noexcept;
    [[nodiscard]] static float safe_signal(float sample) noexcept;
    [[nodiscard]] static std::array<float, 3U> mode_weights(
        StudioDelayPanMode mode) noexcept;
    [[nodiscard]] float waveform_value() const noexcept;
    [[nodiscard]] float delay_frames_for_ms(float milliseconds) const noexcept;
    [[nodiscard]] std::size_t control_ramp_frames() const noexcept;
    [[nodiscard]] float read_delayed(float delay_frames,
                                     std::size_t channel) const noexcept;
    void update_envelope_coefficients() noexcept;
    void prepare_oscillator() noexcept;
    void start_delay_ramp(float target) noexcept;
    void start_feedback_ramp(float target) noexcept;
    void start_mix_ramp(float target) noexcept;
    void start_depth_ramp(float target) noexcept;
    void start_mode_ramp(StudioDelayPanMode mode) noexcept;
    void advance_controls() noexcept;
    void advance_oscillator() noexcept;

    float sample_rate_ = 48000.0F;
    float requested_max_delay_ms_ = kMaximumDelayMs;
    std::size_t capacity_frames_ = 0U;
    std::size_t write_frame_ = 0U;
    std::vector<float> storage_;
    StudioPanDelayParameters parameters_{};

    float current_delay_frames_ = 0.0F;
    float target_delay_frames_ = 0.0F;
    float delay_frames_step_ = 0.0F;
    std::size_t delay_ramp_remaining_ = 0U;
    float current_feedback_ = 0.25F;
    float target_feedback_ = 0.25F;
    float feedback_step_ = 0.0F;
    std::size_t feedback_ramp_remaining_ = 0U;
    float current_mix_ = kDefaultMix;
    float target_mix_ = kDefaultMix;
    float mix_step_ = 0.0F;
    std::size_t mix_ramp_remaining_ = 0U;

    float effective_pan_rate_hz_ = 0.5F;
    float phase_increment_ = 0.0F;
    float phase_ = 0.0F;
    float sine_increment_ = 0.0F;
    float cosine_increment_ = 1.0F;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    float envelope_level_ = 0.0F;
    float attack_coefficient_ = 0.0F;
    float release_coefficient_ = 0.0F;
    float current_depth_ = 0.75F;
    float depth_target_ = 0.75F;
    float depth_step_ = 0.0F;
    std::size_t depth_ramp_remaining_ = 0U;
    std::array<float, 3U> mode_weights_{1.0F, 0.0F, 0.0F};
    std::array<float, 3U> mode_steps_{};
    std::array<float, 3U> mode_targets_{1.0F, 0.0F, 0.0F};
    std::size_t mode_ramp_remaining_ = 0U;
    std::uint32_t normalization_counter_ = 0U;
    bool initialized_ = false;
};

} // namespace tabletfx
