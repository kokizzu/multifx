#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class RhythmicDivision : std::uint8_t {
    Whole,
    Half,
    Quarter,
    DottedQuarter,
    QuarterTriplet,
    Eighth,
    DottedEighth,
    EighthTriplet,
    Sixteenth,
    DottedSixteenth,
    SixteenthTriplet,
    ThirtySecond,
    ThirtySecondTriplet,
};

struct RhythmicSlicerParameters {
    bool tempo_sync = true;
    float tempo_bpm = 120.0F;
    RhythmicDivision division = RhythmicDivision::Sixteenth;
    float rate_hz = 8.0F;
    float depth = 1.0F;
    // 0 holds each step; 1 eases toward the next step across the full step.
    float shape = 0.15F;
    float attack_ms = 1.0F;
    float release_ms = 1.0F;
    // Start position in the 16-step pattern, expressed as a cycle fraction.
    float phase = 0.0F;
    std::array<float, 16> steps{
        1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F,
        1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F,
    };
};

/*
 * MOD-16 is a project-authored 16-step amplitude gate, not a clone of any
 * listed manufacturer's algorithm. Its division names, numeric limits,
 * pattern values, interpolation shape, and attack/release smoothing are
 * project-authored. One shared envelope is applied to every input channel.
 *
 * Manufacturer documentation checked 2026-11-21:
 * - Line 6 Helix Stadium Models (current model list) identifies Bleat Chop
 *   Trem as mono/stereo and Pattern Tremolo as legacy:
 *   https://line6.com/helix-stadium-models/
 *   Stadium Effects manual, rev-d-v1.3, documents common Rate and Depth but
 *   says it omits individual model parameters; it does not publish pattern
 *   length, gate shape, or numeric ranges:
 *   https://manuals.line6.com/en/helix-stadium/rev-d-v1.3/effect-blocks
 * - Fender Tone Master Pro release notes identify Step Tremolo, Step Filter,
 *   Step Filter Delay, and Pitch Sequencer in firmware v1.8.45 (2026-06-03).
 *   The current support page lists v1.8.85 (2026-11-18); those notes do not
 *   specify the effects' individual controls or ranges:
 *   https://support.fender.com/hc/en-us/articles/46768129660315-Fender-Tone-Master-Pro-Firmware-and-Pro-Control-App-Updates
 * - HeadRush Prime's current effect list names Kill Seq and Splicer. The
 *   official downloads page lists firmware and User Guide v5.2; the manual's
 *   full text was not available to inspect, so no control/range claim is made:
 *   https://www.headrushfx.com/products/prime/
 *   https://www.headrushfx.com/downloads
 * - FM9 Owner's Manual, current as of firmware 3.x (August 2022), describes
 *   the Step-Sequencer as an internal controller with 2-32 stages, per-stage
 *   Value, stage count, Rate/Tempo, Run, and Damping. It is not documented
 *   there as an amplitude-gate audio block. The official downloads page lists
 *   firmware v12.0 for FM9 Original/Turbo (2026-07-07), so applicability of
 *   the older guide to that firmware is not established:
 *   https://www.fractalaudio.com/downloads/manuals/FM9/FM9-Owners-Manual.pdf
 *   https://www.fractalaudio.com/fm9-downloads/
 * - The Melofty Axom Max English user manual is available at the URL below;
 *   its rhythmic-model identity/control set was not independently verified
 *   for this implementation:
 *   https://cdn.rockboard.de/nos/data/erp/Artikell_PDF/Manual_PDF/Melofty/EN/MEL_AXOM_MAX_Manual_EN.pdf
 * - XRecer's official XR1 FAQ says its manual lists 111 effect models, but
 *   the public FAQ/product page does not identify a rhythmic model's controls
 *   or limits. No XR1 effect-specific settings are inferred:
 *   https://www.xrecer.com/pages/faq
 *   https://www.xrecer.com/products/xr1
 *
 * In particular, MOD-16 does not claim any device-specific stage count,
 * preset pattern, sync subdivision, stereo topology, or DSP/resource limit.
 */
class RhythmicSlicer final : public Effect {
public:
    explicit RhythmicSlicer(float sample_rate = 48000.0F,
                            RhythmicSlicerParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "mod-16";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   RhythmicSlicerParameters parameters) noexcept;
    void set_tempo_sync(bool enabled, float bpm,
                        RhythmicDivision division) noexcept;
    void set_rate_hz(float rate_hz) noexcept;
    void set_depth(float depth) noexcept;
    void set_shape(float shape) noexcept;
    void set_edge_times_ms(float attack_ms, float release_ms) noexcept;
    void set_phase(float cycle_fraction) noexcept;
    void set_steps(const std::array<float, 16>& steps) noexcept;

    [[nodiscard]] RhythmicSlicerParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_step_rate_hz() const noexcept {
        return effective_step_rate_hz_;
    }
    [[nodiscard]] float current_gain() const noexcept { return current_gain_; }

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
    static constexpr float kMinimumEdgeTimeMs = 0.0F;
    static constexpr float kMaximumEdgeTimeMs = 100.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float beats_per_step(RhythmicDivision division) noexcept;
    [[nodiscard]] static RhythmicDivision sanitize_division(
        RhythmicDivision division) noexcept;
    [[nodiscard]] float pattern_value(float cycle_phase) const noexcept;
    [[nodiscard]] float target_gain(float cycle_phase) const noexcept;
    void prepare_timing() noexcept;
    void prepare_edge_coefficients() noexcept;

    float sample_rate_ = 48000.0F;
    RhythmicSlicerParameters parameters_{};
    float effective_step_rate_hz_ = 8.0F;
    float phase_increment_ = 8.0F / (48000.0F * 16.0F);
    float phase_ = 0.0F;
    float attack_coefficient_ = 0.0F;
    float release_coefficient_ = 0.0F;
    float current_gain_ = 1.0F;
};

} // namespace tabletfx
