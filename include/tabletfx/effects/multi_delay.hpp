#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/delay_timing.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

inline constexpr std::size_t multi_delay_line_count = 4U;
inline constexpr std::size_t multi_delay_storage_limit_bytes = 512U * 1024U;

enum class MultiDelayTopology : std::uint32_t {
    quad_parallel,
    quad_series,
    quad_diffusor,
};

struct MultiDelayParameters {
    MultiDelayTopology topology = MultiDelayTopology::quad_parallel;
    std::array<float, multi_delay_line_count> time_ms{
        250.0F, 375.0F, 500.0F, 750.0F};
    std::array<DelayTimeMode, multi_delay_line_count> time_mode{
        DelayTimeMode::milliseconds, DelayTimeMode::milliseconds,
        DelayTimeMode::milliseconds, DelayTimeMode::milliseconds};
    std::array<DelayDivision, multi_delay_line_count> division{
        DelayDivision::quarter, DelayDivision::dotted_quarter,
        DelayDivision::half, DelayDivision::whole};
    float tempo_bpm = 120.0F;
    // A line feeds itself; cross_feedback[i] routes line i to line (i + 1) % 4.
    std::array<float, multi_delay_line_count> feedback{};
    std::array<float, multi_delay_line_count> cross_feedback{};
    // Series topology sends one tap back to a selected delay-line input.
    // Selectors are zero-based in C++ and map to documented lines 1–4 in UI.
    float series_feedback = 0.0F;
    std::uint32_t series_feedback_send = 3U;
    std::uint32_t series_feedback_return = 0U;
    float master_time = 1.0F;
    float master_feedback = 0.5F;
    // Project-authored linear tap gains and equal-power pan positions.
    std::array<float, multi_delay_line_count> level{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, multi_delay_line_count> pan{0.1F, 0.35F, 0.65F, 0.9F};
    float low_cut_hz = 20.0F;
    float high_cut_hz = 20000.0F;
    float input_gain_db = 0.0F;
    float mix = 0.25F;
    float level_db = 0.0F;
};

/*
 * Project-authored four-line delay. Fractal's November 2026 Blocks Guide,
 * printed pp. 62–68, lists the FM9 Multi-Delay core types Quad-Tap, Quad
 * Parallel, Quad-Series, Quad Diffusor, Quad Tape, and Quad Tap Band. This
 * class offers three limited topologies: a Quad-Parallel-like pattern with
 * independent lines, self feedback, and cyclic cross-feedback links 1->2,
 * 2->3, 3->4, 4->1; a Quad-Series-like pattern with cumulative tap times and
 * one selected output tap feeding one selected delay-line input; and a
 * four-stage cascaded all-pass diffuser with project-authored Master Time and
 * Master Feedback controls. Per-line Time/Tempo is represented in each
 * topology; per-line Feedback is only used in Parallel, while Series and
 * Diffusor use one block-level feedback control each. The documented
 * 1–2000 ms delay-time span informs the project upper bound. Negative
 * feedback supports polarity inversion as described by the guide where it is
 * documented.
 *
 * Project choices are explicit: the lines use a summed mono effect input,
 * each delay is clamped to 1 ms minimum, tempo is shared at 20–300 BPM, the
 * available divisions come from this project, parallel self/cross feedback
 * is limited to ±0.45 per link, and series feedback is limited to ±0.90.
 * Series send/return selectors are zero-based in code (lines 1–4 in UI);
 * level is linear −2…+2 and pan is 0…1. Diffusor Master Time is 0.1…2 and its
 * Master Feedback value is 0…0.90, applied equally to all four stages. A
 * 512-KiB ring-storage cap may shorten the available time below 2000 ms.
 * Project-authored one-pole low/high cuts, input gain, mix, and output level
 * are included. The manual does not publish numeric ranges for most controls;
 * these project limits are not Fractal settings.
 *
 * Quad-Tap, Quad Tape, Quad Tap Band, the guide's two-second diffuser DSP,
 * per-stage stereo filters/spread, individual BPF/comb/ring-mod/chorus/tape
 * processing, time LFOs, ducker, envelope follower, and full EQ page are not implemented. This class
 * does not reproduce Fractal DSP, presets, controls, or sound. Mono/stereo
 * input is accepted; the wet path is generated
 * from a mono average and can be panned in stereo. Processing uses a
 * preallocated ring and performs no allocations.
 *
 * The November 2026 guide names FM9 but is family-level, and the guide itself
 * says product features can vary. Fractal's downloads page, checked
 * 2026-11-22, lists firmware v12.0 dated 2026-07-07 for “FM9 Original and FM9
 * Turbo”; it does not separately name FM9 Mark II Turbo. This source
 * supports an FM9-family comparison, not a separate Mark II firmware claim.
 * Sources:
 * https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
 * https://www.fractalaudio.com/fm9-downloads/
 */
class MultiDelay final : public Effect {
public:
    explicit MultiDelay(float sample_rate = 48000.0F,
                        float max_delay_ms = 2000.0F,
                        MultiDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "multi-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }
    [[nodiscard]] MultiDelayParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float maximum_delay_ms() const noexcept;
    [[nodiscard]] std::size_t delay_samples(std::size_t line) const noexcept;

    // Storage remains fixed after construction; all bounds are project-owned.
    void configure(MultiDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumDelayMs = 1.0F;
    static constexpr float kMaximumDelayMs = 2000.0F;
    static constexpr float kMinimumTempoBpm = 20.0F;
    static constexpr float kMaximumTempoBpm = 300.0F;
    static constexpr float kMaximumLinkFeedback = 0.45F;
    static constexpr float kMaximumLevel = 2.0F;
    static constexpr float kMaximumSignal = 8.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static bool valid_time_mode(DelayTimeMode mode) noexcept;
    [[nodiscard]] static bool valid_division(DelayDivision division) noexcept;
    [[nodiscard]] static float beats_for_division(DelayDivision division) noexcept;
    [[nodiscard]] std::size_t read_frame(std::size_t delay,
                                         std::size_t line) const noexcept;
    [[nodiscard]] float filter_line(float input, std::size_t line) noexcept;
    [[nodiscard]] static float safe_signal(float value) noexcept;
    void update_line_times() noexcept;

    float sample_rate_ = 48000.0F;
    float requested_max_delay_ms_ = kMaximumDelayMs;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;

    MultiDelayParameters parameters_{};
    std::array<std::size_t, multi_delay_line_count> delay_frames_{};
    std::array<float, multi_delay_line_count> line_gains_{};
    std::array<float, multi_delay_line_count> left_gains_{};
    std::array<float, multi_delay_line_count> right_gains_{};
    std::array<float, multi_delay_line_count> low_cut_input_{};
    std::array<float, multi_delay_line_count> low_cut_output_{};
    std::array<float, multi_delay_line_count> high_cut_output_{};
    float low_cut_alpha_ = 0.0F;
    bool low_cut_enabled_ = false;
    float high_cut_coefficient_ = 0.0F;
    float input_gain_linear_ = 1.0F;
    float output_level_linear_ = 1.0F;
};

} // namespace tabletfx
