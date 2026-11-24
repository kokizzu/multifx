#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

// These are project-authored modulation choices. They are not a claim of
// compatibility with any manufacturer's oscillator implementation.
enum class BiasTremWaveform : std::uint8_t {
    Sine,
    Triangle,
    Square,
    SawUp,
    SawDown,
    SampleHold,
};

struct BiasTremParameters {
    float rate_hz = 4.5F;
    float depth = 0.35F;
    BiasTremWaveform waveform = BiasTremWaveform::Sine;
    bool tempo_sync = false;
    float tempo_bpm = 120.0F;
    float beats_per_cycle = 1.0F;

    // Project-authored input-trigger behavior: a rising input peak crossing
    // this threshold restarts the modulation phase when trigger_sync is on.
    bool trigger_sync = false;
    float trigger_threshold_db = -36.0F;

    // The detector reduces modulation depth as input level rises. bias_drive
    // controls a bounded, mild, LFO-varying polarity-asymmetric soft curve.
    float ducking = 0.0F;
    float bias_drive = 0.18F;

    float mix = 1.0F;
    float level_db = 0.0F;
    // Linear balance from -1 (left) through 0 (center) to +1 (right).
    float balance = 0.0F;
    bool bypass = false;
};

// Project-authored bias-style tremolo: a linked LFO modulates amplitude and a
// small, bounded asymmetric soft curve. Its qualitative inspiration is the
// tube-bias tremolo description in the Fractal Blocks Guide. It does not model
// or claim parity with Fractal DSP, settings, or sound.
//
// Controls are sanitized outside the audio loop. Processing uses fixed object
// state, allocates no memory, and has zero fixed latency. All channels share
// one modulation clock and one level detector; for stereo, balance attenuates
// the opposite side linearly. Non-finite input samples become silence.
class BiasTrem final : public Effect {
public:
    explicit BiasTrem(float sample_rate = 48000.0F,
                      BiasTremParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "fractal-bias-trem";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    [[nodiscard]] constexpr std::size_t latency_frames() const noexcept {
        return 0;
    }

    void configure(float sample_rate, BiasTremParameters parameters) noexcept;
    // Updates controls without resetting the running LFO phase.
    void set_parameters(BiasTremParameters parameters) noexcept;

    [[nodiscard]] BiasTremParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_rate_hz() const noexcept {
        return effective_rate_hz_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumRateHz = 0.05F;
    static constexpr float kMaximumRateHz = 20.0F;
    static constexpr float kMinimumBpm = 30.0F;
    static constexpr float kMaximumBpm = 300.0F;
    static constexpr float kMinimumBeatsPerCycle = 0.0625F;
    static constexpr float kMaximumBeatsPerCycle = 16.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static BiasTremWaveform sanitize_waveform(
        BiasTremWaveform waveform) noexcept;
    [[nodiscard]] static BiasTremParameters sanitize_parameters(
        BiasTremParameters parameters,
        const BiasTremParameters& fallback) noexcept;
    void prepare_oscillator() noexcept;
    void prepare_detector_and_output() noexcept;
    void reset_phase() noexcept;
    [[nodiscard]] float next_lfo_value() noexcept;
    [[nodiscard]] std::uint32_t next_random() noexcept;

    float sample_rate_ = 48000.0F;
    BiasTremParameters parameters_{};
    float effective_rate_hz_ = 4.5F;
    float phase_increment_ = 0.0F;
    float phase_ = 0.0F;
    float sine_increment_ = 0.0F;
    float cosine_increment_ = 1.0F;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    std::uint32_t normalization_counter_ = 0;
    std::uint32_t random_state_ = 0x6D2B79F5U;
    float sample_hold_value_ = -1.0F;

    float envelope_ = 0.0F;
    float attack_coefficient_ = 0.0F;
    float release_coefficient_ = 0.0F;
    float trigger_threshold_linear_ = 0.015848932F;
    bool trigger_was_above_ = false;

    float output_gain_ = 1.0F;
    float left_gain_ = 1.0F;
    float right_gain_ = 1.0F;
};

} // namespace tabletfx
