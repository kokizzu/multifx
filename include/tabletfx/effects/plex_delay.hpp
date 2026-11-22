#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/delay_timing.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

inline constexpr std::size_t plex_delay_line_count = 8U;
inline constexpr std::size_t plex_delay_storage_limit_bytes = 1024U * 1024U;

enum class PlexDelayStackMode : std::uint8_t {
    off,
    stack,
    hold,
};

struct PlexDelayParameters {
    std::uint8_t active_lines = 8U;
    std::array<float, plex_delay_line_count> time_ms{
        45.0F, 70.0F, 110.0F, 150.0F, 220.0F, 300.0F, 450.0F, 600.0F};
    std::array<DelayTimeMode, plex_delay_line_count> time_mode{};
    std::array<DelayDivision, plex_delay_line_count> division{
        DelayDivision::sixteenth, DelayDivision::eighth_triplet,
        DelayDivision::eighth, DelayDivision::dotted_eighth,
        DelayDivision::quarter, DelayDivision::dotted_quarter,
        DelayDivision::half, DelayDivision::whole};
    float tempo_bpm = 120.0F;
    float master_time = 1.0F;
    float master_level = 1.0F;
    float master_pan = 1.0F;
    float decay_seconds = 3.0F;
    float diffusion = 0.5F;
    float low_cut_hz = 20.0F;
    float high_cut_hz = 20000.0F;
    float pre_delay_ms = 0.0F;
    PlexDelayStackMode stack_mode = PlexDelayStackMode::off;
    std::array<float, plex_delay_line_count> line_level{
        1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, plex_delay_line_count> line_pan{
        -0.85F, -0.6F, -0.35F, -0.1F, 0.1F, 0.35F, 0.6F, 0.85F};
    float input_gain_db = 0.0F;
    float mix = 0.25F;
    float level_db = 0.0F;
    float balance = 0.0F;
};

/*
 * Project-authored eight-line feedback-matrix delay. The November 2026 Fractal
 * Audio Blocks Guide, pp. 88–90, documents an FM9 Plex Delay with 1–8 lines,
 * decay, diffusion, low/high cuts, pre-delay, Stack/Hold, per-line time/tempo,
 * level/pan, master scaling, a wet filter and LFO, an input envelope follower,
 * a ducker, stereo spread, common mix controls, and spillover. This implementation currently
 * covers the matrix-delay core, timing, line levels/pans, master controls,
 * decay, diffusion, cuts, pre-delay, and project-authored Stack/Hold behavior.
 *
 * Matrix topology, feedback/decay mapping, filter design, bounds, defaults,
 * input summing, and the 1 MiB total ring-storage cap are project choices.
 * The cap is divided among eight delay lines and one pre-delay ring, so the
 * maximum per-ring time depends on sample rate. Wet filter and LFO, the envelope
 * follower, ducker, stereo spread, granular pitch variants, presets, spillover,
 * and exact Fractal settings/DSP/sound parity are not implemented. Plex Delay
 * itself has no pitch effects; Plex Detune and Plex Shift are separate types.
 * The guide
 * names FM9 without distinguishing the Mark II Turbo hardware revision; the
 * FM9 downloads page currently lists v12.0 for Original/Turbo without a
 * separate Mark II Turbo entry. Do not treat this as an installed-firmware or
 * exact-model parity claim.
 *
 * Source revision: November 2026.
 * Source: https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
 * Firmware scope: https://www.fractalaudio.com/fm9-downloads/
 */
class PlexDelay final : public Effect {
public:
    explicit PlexDelay(float sample_rate = 48000.0F,
                       float max_delay_ms = 2000.0F,
                       PlexDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "plex-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }
    [[nodiscard]] PlexDelayParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float maximum_delay_ms() const noexcept;
    [[nodiscard]] std::size_t delay_samples(std::size_t line) const noexcept;
    [[nodiscard]] std::size_t pre_delay_samples() const noexcept {
        return pre_delay_frames_;
    }

    void configure(PlexDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kRingCount = plex_delay_line_count + 1U;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumDelayMs = 1.0F;
    static constexpr float kMaximumDelayMs = 2000.0F;
    static constexpr float kMaximumSignal = 8.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static bool valid_time_mode(DelayTimeMode mode) noexcept;
    [[nodiscard]] static bool valid_division(DelayDivision division) noexcept;
    [[nodiscard]] static bool valid_stack_mode(PlexDelayStackMode mode) noexcept;
    [[nodiscard]] static float beats_for_division(DelayDivision division) noexcept;
    [[nodiscard]] std::size_t read_index(std::size_t delay,
                                         std::size_t ring) const noexcept;
    [[nodiscard]] float filter_feedback(float input,
                                        std::size_t line) noexcept;
    [[nodiscard]] static float safe_signal(float value) noexcept;
    void update_times() noexcept;

    float sample_rate_ = 48000.0F;
    float requested_max_delay_ms_ = kMaximumDelayMs;
    std::size_t capacity_frames_ = 0U;
    std::size_t write_frame_ = 0U;
    std::vector<float> storage_;
    PlexDelayParameters parameters_{};
    std::array<std::size_t, plex_delay_line_count> delay_frames_{};
    std::size_t pre_delay_frames_ = 0U;
    std::array<float, plex_delay_line_count> feedback_gains_{};
    std::array<float, plex_delay_line_count> output_left_gains_{};
    std::array<float, plex_delay_line_count> output_right_gains_{};
    float input_gain_linear_ = 1.0F;
    float output_level_linear_ = 1.0F;
    float low_cut_alpha_ = 0.0F;
    float high_cut_coefficient_ = 1.0F;
    bool low_cut_enabled_ = false;
    std::array<float, plex_delay_line_count> low_cut_input_{};
    std::array<float, plex_delay_line_count> low_cut_output_{};
    std::array<float, plex_delay_line_count> high_cut_output_{};
};

} // namespace tabletfx
