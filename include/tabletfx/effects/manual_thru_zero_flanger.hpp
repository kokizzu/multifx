#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

// Evidence basis: Fractal Audio Systems, Blocks Guide (November 2026), Flanger
// Block, pp. 48–50: Manual 0–10, feedback −100…+100, overall Min/Max Time
// extrema 0.340–20 ms, adjustable Dry Delay, and wet phase reversal on one or
// both channels. The guide says Type changes auto-seed Min/Max Time, but does
// not publish this type's exact seeds or the Dry Delay range. This processor
// uses its own bounded control law and defaults; it does not claim Fractal DSP
// or preset parity.
// https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
enum class FlangerPhaseReverse : std::uint8_t {
    None,
    Left,
    Right,
    Both,
};

enum class ThruZeroMode : std::uint8_t {
    Off,
    Auto,
    Manual,
};

enum class ThruZeroLfoWaveform : std::uint8_t {
    Sine,
    Triangle,
};

struct ManualThruZeroFlangerParameters {
    // Delay endpoints follow the documented FM9-family extrema (0.340–20 ms).
    // The exact Manual TZF seeds are not published; defaults and the 0.010 ms
    // minimum span are project choices. Vendor extrema do not imply that each
    // model uses these exact values.
    float minimum_time_ms = 0.34F;
    float maximum_time_ms = 8.0F;
    // Manual is a documented 0–10 position control (not milliseconds); it
    // moves the wet delay between minimum_time_ms and maximum_time_ms.
    float manual = 5.0F;
    // Vendor documentation says Dry Delay moves the cancellation point, but
    // does not give a range here. This project uses a normalized 0–1 position
    // within the same delay span so the wet tap can cross the delayed dry tap.
    float dry_delay_position = 0.5F;
    // UI scale follows the documented -100…+100 feedback control. Internally
    // this project maps it to a stable feedback gain no greater than 0.90.
    float feedback_percent = 0.0F;
    // Project-authored per-channel selector for the documented Phase Reverse
    // capability; exact control labels and mode combinations are not asserted.
    FlangerPhaseReverse phase_reverse = FlangerPhaseReverse::Both;
    ThruZeroMode mode = ThruZeroMode::Manual;
    // Auto/Off LFO controls use project-authored bounds and mapping because the
    // FM9-family guide does not publish numeric ranges/defaults for them.
    float lfo_rate_hz = 0.25F;
    float lfo_depth = 5.0F;
    ThruZeroLfoWaveform lfo_waveform = ThruZeroLfoWaveform::Sine;
    float lfo_phase_degrees = 180.0F;
    // These are project-authored linear wet/dry and smoothing controls; their
    // ranges are not presented as Fractal parameter ranges.
    float mix = 0.5F;
    float smoothing_ms = 10.0F;
};

// Project-authored Off/Auto/Manual through-zero flanger. Auto sweeps the wet
// delay with an LFO; Manual controls its position by hand. Dry Delay moves the
// cancellation point. The LFO mapping, numeric bounds/defaults, and DSP are
// project choices, not Fractal settings or sound parity. Off uses an immediate
// dry path; Auto/Manual delay both paths by their selected taps (0.340–20 ms).
// maximum_path_latency_frames() reports the configured upper bound for graph
// budgeting. The processor does not compensate other graph paths. Construction
// allocates delay storage; process() is allocation-free.
class ManualThruZeroFlanger final : public Effect {
public:
    explicit ManualThruZeroFlanger(
        float sample_rate = 48000.0F, std::uint16_t channels = 2,
        ManualThruZeroFlangerParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "manual-thru-zero-flanger";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) +
               (wet_storage_.capacity() + dry_storage_.capacity()) * sizeof(float);
    }

    void set_parameters(ManualThruZeroFlangerParameters parameters) noexcept;
    [[nodiscard]] ManualThruZeroFlangerParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float current_wet_time_ms() const noexcept;
    [[nodiscard]] float current_dry_time_ms() const noexcept;
    [[nodiscard]] std::size_t maximum_path_latency_frames() const noexcept;

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumTimeMs = 0.340F;
    static constexpr float kMaximumTimeMs = 20.0F;
    static constexpr float kMinimumTimeSpanMs = 0.010F;
    static constexpr float kMaximumFeedbackGain = 0.90F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] float read_delay(const std::vector<float>& storage,
                                   std::size_t write_frame,
                                   std::uint16_t channel,
                                   float delay_frames) const noexcept;
    [[nodiscard]] static float lfo_value(ThruZeroLfoWaveform waveform,
                                         float phase_cycles) noexcept;
    void retarget(bool immediate) noexcept;
    void advance_ramps() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> wet_storage_;
    std::vector<float> dry_storage_;
    ManualThruZeroFlangerParameters parameters_{};

    float current_minimum_time_ms_ = 0.34F;
    float current_maximum_time_ms_ = 8.0F;
    float current_manual_ = 5.0F;
    float current_dry_delay_position_ = 0.5F;
    float current_feedback_percent_ = 0.0F;
    float current_lfo_rate_hz_ = 0.25F;
    float current_lfo_depth_ = 5.0F;
    float current_lfo_phase_degrees_ = 180.0F;
    float current_thru_zero_mix_ = 1.0F;
    float current_lfo_mix_ = 0.0F;
    float current_mix_ = 0.5F;
    float target_minimum_time_ms_ = 0.34F;
    float target_maximum_time_ms_ = 8.0F;
    float target_manual_ = 5.0F;
    float target_dry_delay_position_ = 0.5F;
    float target_feedback_percent_ = 0.0F;
    float target_lfo_rate_hz_ = 0.25F;
    float target_lfo_depth_ = 5.0F;
    float target_lfo_phase_degrees_ = 180.0F;
    float target_thru_zero_mix_ = 1.0F;
    float target_lfo_mix_ = 0.0F;
    float target_mix_ = 0.5F;
    float minimum_time_step_ = 0.0F;
    float maximum_time_step_ = 0.0F;
    float manual_step_ = 0.0F;
    float dry_delay_position_step_ = 0.0F;
    float feedback_percent_step_ = 0.0F;
    float lfo_rate_step_ = 0.0F;
    float lfo_depth_step_ = 0.0F;
    float lfo_phase_step_ = 0.0F;
    float thru_zero_mix_step_ = 0.0F;
    float lfo_mix_step_ = 0.0F;
    float mix_step_ = 0.0F;
    float lfo_phase_cycles_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
};

} // namespace tabletfx
