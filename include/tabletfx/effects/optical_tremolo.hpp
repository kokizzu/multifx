#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

// Variant names reflect documented Fractal FM9-family labels only. This
// processor's response curves and controls are independent project choices.
enum class OpticalTremoloVariant : std::uint8_t {
    Optical1,
    Optical2,
    Neon,
};

enum class OpticalTremoloWaveform : std::uint8_t {
    Sine,
    Triangle,
    Square,
    SawUp,
    SawDown,
    Random,
    Logarithmic,
    Exponential,
    Trapezoid,
    Astable,
};

struct OpticalTremoloParameters {
    OpticalTremoloVariant variant = OpticalTremoloVariant::Optical1;
    float rate_hz = 4.5F;
    float depth = 0.5F;
    OpticalTremoloWaveform lfo = OpticalTremoloWaveform::Sine;
    bool tempo_sync = false;
    float tempo_bpm = 120.0F;
    // Quarter-note beats per modulation cycle; 1 is a quarter-note cycle.
    float beats_per_cycle = 1.0F;
    float duty_cycle_percent = 50.0F;
    // A project-authored waveform shape control, centered at 50%.
    float shape_percent = 50.0F;
    float phase_degrees = 0.0F;
    float start_phase_degrees = 0.0F;
    bool trigger_enabled = false;
    float trigger_threshold_dbfs = -36.0F;
    float ducking_percent = 0.0F;
    float mix = 1.0F;
    float level_db = 0.0F;
    float balance_percent = 0.0F;
};

/*
 * Project-authored, zero-latency optical-style amplitude modulation. Optical
 * 1 uses a linear light-response mapping; Optical 2 uses a more abrupt curve;
 * Neon uses a fixed pulse LFO. Those distinctions are deliberate project
 * interpretations of the source's qualitative descriptions, not Fractal
 * circuit, control, parameter, or sound parity. The photocell/LDR hardware is
 * not simulated.
 *
 * The documented block also has Harmonic Trem, Bias Trem, and Panner types;
 * they are not aliases for this amplitude-only processor. Their separate
 * signal paths remain separate effects. Numeric ranges/defaults, tempo
 * mapping, trigger and ducking behavior, waveform curves, and DSP are all
 * project-authored. The source guide does not publish numeric bounds/defaults.
 * The November 2026 FM9-family guide names ten LFO shapes; all ten labels are
 * available here with bounded project-authored mappings. Random is a seeded
 * cycle-rate sample-and-hold, and Neon deliberately ignores the selectable
 * LFO shape because its documented LFO is fixed. Triangle peak position,
 * square duty, saw direction, polynomial Log/Exponential bends, Trapezoid
 * plateau width, and alternating Astable duty are project interpretations;
 * none claims an exact vendor curve.
 *
 * Parameters are prepared in configure()/set_parameters(), outside the audio
 * loop. process() uses fixed state, incremental oscillator arithmetic, and no
 * allocation or per-sample trigonometric functions. It adds no fixed latency.
 */
class OpticalTremolo final : public Effect {
public:
    explicit OpticalTremolo(float sample_rate = 48000.0F,
                            OpticalTremoloParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "optical-tremolo";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   OpticalTremoloParameters parameters) noexcept;
    void set_parameters(OpticalTremoloParameters parameters) noexcept;
    [[nodiscard]] OpticalTremoloParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_rate_hz() const noexcept {
        return effective_rate_hz_;
    }
    [[nodiscard]] float envelope_level() const noexcept { return envelope_; }

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
    static constexpr float kMinimumTriggerDbfs = -90.0F;
    static constexpr float kMaximumTriggerDbfs = 0.0F;
    static constexpr float kMaximumInternalSample = 1000000.0F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;
    static constexpr float kControlSmoothingMs = 10.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static OpticalTremoloVariant sanitize_variant(
        OpticalTremoloVariant variant) noexcept;
    [[nodiscard]] static OpticalTremoloWaveform sanitize_lfo(
        OpticalTremoloWaveform waveform) noexcept;
    [[nodiscard]] static OpticalTremoloParameters sanitize_parameters(
        OpticalTremoloParameters parameters) noexcept;
    void prepare_oscillator() noexcept;
    void retarget_controls() noexcept;
    void advance_oscillator() noexcept;
    [[nodiscard]] float lfo_value(float phase) const noexcept;
    [[nodiscard]] float modulation_value(float phase) const noexcept;
    [[nodiscard]] float phase_with_offset(float phase) const noexcept;
    void advance_random_sample() noexcept;
    void advance_smoothing() noexcept;

    float sample_rate_ = 48000.0F;
    OpticalTremoloParameters parameters_{};
    float effective_rate_hz_ = 4.5F;
    float phase_increment_ = 4.5F / 48000.0F;
    float phase_ = 0.0F;
    float sine_increment_ = 0.0F;
    float cosine_increment_ = 1.0F;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    float phase_offset_sine_ = 0.0F;
    float phase_offset_cosine_ = 1.0F;
    float start_phase_cycles_ = 0.0F;
    float start_phase_sine_ = 0.0F;
    float start_phase_cosine_ = 1.0F;
    std::uint32_t cycle_count_ = 0U;
    std::uint32_t random_state_ = 0xA341316CU;
    float random_hold_ = 0.0F;
    std::uint32_t normalization_counter_ = 0U;

    float trigger_threshold_linear_ = 0.0F;
    float envelope_attack_coefficient_ = 0.0F;
    float envelope_release_coefficient_ = 0.0F;
    float envelope_ = 0.0F;
    bool trigger_active_ = true;

    float current_depth_ = 0.5F;
    float target_depth_ = 0.5F;
    float depth_step_ = 0.0F;
    float current_mix_ = 1.0F;
    float target_mix_ = 1.0F;
    float mix_step_ = 0.0F;
    float current_level_gain_ = 1.0F;
    float target_level_gain_ = 1.0F;
    float level_step_ = 0.0F;
    float current_balance_ = 0.0F;
    float target_balance_ = 0.0F;
    float balance_step_ = 0.0F;
    std::size_t smoothing_frames_remaining_ = 0U;
};

} // namespace tabletfx
