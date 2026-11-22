#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

inline constexpr std::size_t ten_tap_delay_max_taps = 10;
inline constexpr std::size_t ten_tap_delay_storage_limit_bytes = 1024U * 1024U;

enum class TenTapPanShape : std::uint8_t {
    constant,
    increasing,
    decreasing,
    sine,
};

enum class TenTapDivision : std::uint8_t {
    free,
    whole,
    half,
    dotted_half,
    half_triplet,
    quarter,
    dotted_quarter,
    quarter_triplet,
    eighth,
    dotted_eighth,
    eighth_triplet,
    sixteenth,
    dotted_sixteenth,
    sixteenth_triplet,
    thirty_second,
    thirty_second_triplet,
};

struct TenTapDelayParameters {
    std::uint8_t tap_count = 4;
    float delay_time_ms = 220.0F;
    float tempo_bpm = 120.0F;
    TenTapDivision division = TenTapDivision::free;
    float decay_percent = 30.0F;
    float shuffle_percent = 0.0F;
    float spread_percent = 50.0F;
    // Ratio sets left tap spacing as a percentage of right tap spacing.
    float ratio_percent = 100.0F;
    TenTapPanShape pan_shape = TenTapPanShape::increasing;
    float pan_alpha_percent = 100.0F;
    float low_cut_hz = 20.0F;
    float high_cut_hz = 20000.0F;
    float mix = 0.25F;
    float level_db = 0.0F;
    bool mono = false;
    std::array<float, ten_tap_delay_max_taps> tap_level_db{};
};

struct RhythmTapDelayParameters {
    TenTapDelayParameters common{};
    TenTapDivision quantize = TenTapDivision::free;
    float feedback_percent = 25.0F;
    // Each value is the interval from the preceding repeat. Values are in
    // milliseconds when quantize is free, or in quantized divs otherwise.
    std::array<float, ten_tap_delay_max_taps> tap_time_values{
        220.0F, 220.0F, 220.0F, 220.0F, 220.0F,
        220.0F, 220.0F, 220.0F, 220.0F, 220.0F};
};

/*
 * Project-authored Ten-Tap Delay. The November 2026 Fractal Audio Blocks Guide
 * documents Ten-Tap Delay controls for the FM9 family: 1–10 taps, delay time /
 * tempo, decay, shuffle, stereo spread and ratio, pan shape/alpha, low/high cut,
 * and per-tap level (-80 to +20 dB). This class implements the named controls as
 * a general echo effect; it does not reproduce Fractal's algorithm, control
 * curves, sound, preset behavior, or device firmware.
 *
 * Bounds, defaults, delay-line length, tempo subdivisions, shuffle amount,
 * pan-curve math, filter slopes, stereo law, and mono behavior are project-owned
 * where the guide does not publish them. The documented tap-level range and
 * maximum tap count are retained. A one-MiB ring-buffer cap further limits the
 * available delay at high sample rates. Processing uses preallocated storage
 * and performs no allocations.
 *
 * Rhythm Tap Delay is a separate Fractal type implemented below. FM9 family
 * documentation applies to the FM9 family; applicability to the specifically
 * named FM9 Mk II Turbo is inferred from Fractal's family material, not stated
 * as a separate firmware compatibility line.
 *
 * Source: https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
 * Guide revision: November 2026. The FM9 downloads page lists v12.0 (2026-07-07)
 * for FM9 Original and FM9 Turbo, but does not separately name the FM9 Mk II
 * Turbo; installed device firmware is unknown.
 */
class TenTapDelay : public Effect {
public:
    explicit TenTapDelay(float sample_rate = 48000.0F,
                         std::uint16_t channels = 2,
                         float max_delay_ms = 2000.0F,
                         TenTapDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "ten-tap-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    void set_parameters(TenTapDelayParameters parameters) noexcept;
    [[nodiscard]] TenTapDelayParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] std::size_t tap_delay_samples(std::size_t tap,
                                                 std::uint16_t channel = 1) const noexcept;
    [[nodiscard]] float effective_tap_pan(std::size_t tap) const noexcept;
    [[nodiscard]] float maximum_delay_ms() const noexcept;
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

protected:
    void configure_rhythm_pattern(
        TenTapDivision quantize, float feedback_percent,
        const std::array<float, ten_tap_delay_max_taps>& tap_time_values) noexcept;
    [[nodiscard]] std::size_t delay_storage_bytes() const noexcept {
        return storage_.capacity() * sizeof(float);
    }

private:
    friend class RhythmTapDelay;

    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumDelayMs = 1.0F;
    static constexpr float kMaximumDelayMs = 2000.0F;
    static constexpr float kMaximumRequestedDelayMs = 20000.0F;
    static constexpr float kMinimumLevelDb = -80.0F;
    static constexpr float kMaximumLevelDb = 20.0F;
    static constexpr float kMaximumSignal = 8.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                        float minimum, float maximum) noexcept;
    [[nodiscard]] static float division_beats(TenTapDivision division) noexcept;
    [[nodiscard]] static bool valid_division(TenTapDivision division) noexcept;
    [[nodiscard]] static bool valid_pan_shape(TenTapPanShape shape) noexcept;
    [[nodiscard]] float tap_interval_ms() const noexcept;
    [[nodiscard]] float shaped_pan(std::size_t tap,
                                   std::size_t tap_count) const noexcept;
    [[nodiscard]] float read_delay(std::size_t delay_frames,
                                   std::uint16_t channel) const noexcept;
    [[nodiscard]] float filter_wet(float sample, std::uint16_t channel) noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    float requested_max_delay_ms_ = kMaximumDelayMs;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;
    TenTapDelayParameters parameters_{};
    std::array<std::array<std::size_t, 2>, ten_tap_delay_max_taps> delay_frames_{};
    std::array<float, ten_tap_delay_max_taps> level_gains_{};
    std::array<float, ten_tap_delay_max_taps> effective_pans_{};
    std::array<float, 2> low_cut_input_{};
    std::array<float, 2> low_cut_output_{};
    std::array<float, 2> high_cut_output_{};
    float low_cut_coefficient_ = 0.0F;
    float high_cut_coefficient_ = 1.0F;
    bool rhythm_mode_ = false;
    TenTapDivision rhythm_quantize_ = TenTapDivision::free;
    float rhythm_feedback_percent_ = 0.0F;
    std::array<float, ten_tap_delay_max_taps> rhythm_tap_time_values_{};
};

/*
 * Project-authored Rhythm Tap Delay sharing the Ten-Tap delay-line, filtering,
 * panning, and level path. The November 2026 Fractal guide documents custom
 * inter-tap milliseconds or quantized divs, Learn-by-tapping, per-tap levels,
 * and final-repeat feedback that regenerates the full pattern. Learn interaction
 * is provided by the browser UI; values here are already converted to ms/divs.
 *
 * Feedback ceiling, defaults, interval bounds, interpolation, exact feedback
 * placement, buffer cap, and DSP are project choices because Fractal does not
 * publish their numeric bounds or implementation. Pattern length is exposed as
 * a project control; the guide documents Tap Time 1–10 but excludes a Number of
 * Taps parameter. No Fractal sound, preset, or algorithm parity is claimed.
 *
 * Source: https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
 * Guide revision: November 2026; firmware applicability follows the qualified
 * FM9-family note on TenTapDelay above.
 */
class RhythmTapDelay final : public TenTapDelay {
public:
    explicit RhythmTapDelay(float sample_rate = 48000.0F,
                            std::uint16_t channels = 2,
                            RhythmTapDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "rhythm-tap-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + delay_storage_bytes();
    }

    void set_parameters(RhythmTapDelayParameters parameters) noexcept;
    [[nodiscard]] RhythmTapDelayParameters parameters() const noexcept {
        return parameters_;
    }

private:
    RhythmTapDelayParameters parameters_{};
};

} // namespace tabletfx
