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
inline constexpr std::size_t multi_delay_time_lfo_count = 2U;
inline constexpr std::size_t multi_delay_storage_limit_bytes = 512U * 1024U;

enum class MultiDelayTopology : std::uint32_t {
    quad_parallel,
    quad_series,
    quad_diffusor,
    quad_tap,
    quad_tap_band,
    quad_tape,
};

// Preserve Sine/Triangle/Square raw IDs 0–2. The guide also names Saw Up,
// Saw Down, Random, Log, Exponential, Trapezoid, and Astable; the exact shape
// transfer functions below remain project-authored.
enum class MultiDelayLfoWaveform : std::uint32_t {
    sine = 0,
    triangle = 1,
    square = 2,
    saw_up = 3,
    saw_down = 4,
    random = 5,
    log = 6,
    exponential = 7,
    trapezoid = 8,
    astable = 9,
};

struct MultiDelayTimeLfoParameters {
    MultiDelayLfoWaveform waveform = MultiDelayLfoWaveform::sine;
    // Project range 0–180 degrees; this is the phase offset for tap 2.
    float phase_degrees = 90.0F;
    // Project-authored free-rate range 0.05–20 Hz.
    float rate_hz = 0.5F;
    // False is the Fractal-style NONE/free-rate setting; true derives rate from
    // shared BPM and this division (up to a project-derived 80 Hz). NONE is
    // not a DelayDivision enum value.
    bool tempo_sync = false;
    DelayDivision division = DelayDivision::quarter;
    // Project-authored peak delay-time deviation in milliseconds, 0–20 ms.
    // Zero disables modulation and preserves the legacy Multi-Delay output.
    float depth_ms = 0.0F;
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
    // Project-selected 0.8–1.2x range; 1.0x is normal motor speed.
    float motor_speed = 1.0F;
    // Project-authored linear tap gains and equal-power pan positions.
    std::array<float, multi_delay_line_count> level{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, multi_delay_line_count> pan{0.1F, 0.35F, 0.65F, 0.9F};
    float low_cut_hz = 20.0F;
    float high_cut_hz = 20000.0F;
    float input_gain_db = 0.0F;
    float mix = 0.25F;
    float level_db = 0.0F;
    // Trailing fields preserve existing aggregate/config field order. Zero
    // depth keeps all older presets and callers sonically unchanged.
    std::array<MultiDelayTimeLfoParameters, multi_delay_time_lfo_count> time_lfo{};
    // Project-only opt-in for an additive percentage-point offset to Mix.
    // The base Mix remains stored independently, and the option is neutral off.
    bool global_mix_enabled = false;
    float global_mix_offset = 0.0F;
};

/*
 * Project-authored four-line delay. Fractal's November 2026 Blocks Guide,
 * printed pp. 62–68, lists the FM9 Multi-Delay core types Quad-Tap, Quad
 * Parallel, Quad-Series, Quad Diffusor, Quad Tape, and Quad Tap Band. This
 * class offers limited project-authored analogues: Quad Parallel-like
 * independent lines with self/cyclic cross-feedback links 1->2, 2->3, 3->4,
 * 4->1; Quad Series-like cumulative tap times with one selected output tap
 * feeding one selected delay-line input; a four-stage all-pass diffusor; four
 * independent Quad-Tap-like delay lines; Quad Tap Band-like output filtering
 * outside the feedback loop; and four Quad-Tape-like delay heads. The tape
 * mode's project-authored Motor Speed control scales echo spacing and slews
 * fractional read heads so stored echoes bend pitch as speed changes. Per-line
 * Time/Tempo applies to each mode. Per-line Feedback is available in Parallel,
 * Quad-Tap, Quad Tap Band, and Quad Tape; Series and Diffusor use one shared
 * feedback control each. The documented 1–2,000 ms delay-time span informs the
 * project upper bound. Negative feedback supports polarity inversion where
 * the guide documents it.
 *
 * Project choices are explicit: the lines use a summed mono effect input,
 * each delay is clamped to 1 ms minimum, tempo is shared at 20–300 BPM, the
 * available divisions come from this project, parallel self/cross feedback
 * is limited to ±0.45 per link, and series feedback is limited to ±0.90.
 * Series send/return selectors are zero-based in code (lines 1–4 in UI);
 * level is linear −2…+2 and pan is 0…1. Diffusor Master Time is 0.1…2 and its
 * Master Feedback value is 0…0.90, applied equally to all four stages. Quad
 * Tape Motor Speed is clamped to 0.8…1.2 (1.0 normal), a project-selected
 * range because the guide gives no numeric limit for this control. A 512-KiB
 * ring-storage cap may shorten the available time below 2,000 ms.
 * Project-authored one-pole low/high cuts, input gain, mix, and output level
 * are included. The manual does not publish numeric ranges for most controls;
 * these project limits are not Fractal settings.
 *
 * The tape interpolation and speed bounds are project-authored, not Fractal's
 * proprietary tape model. Per-tap tape coloration, the guide's two-second
 * diffuser DSP, per-stage stereo filters/spread, individual BPF/comb/ring-mod/
 * chorus processing, ducker, envelope follower, and full EQ page are not
 * implemented. The guide names two LFOs and ten waveform types and describes
 * Phase at Tap 2 with Tap 3/4 inversions. This implementation applies each LFO
 * to all four taps using offsets [0, phase, 180, phase+180] and sums both
 * contributions; that routing, the waveform transfer functions, phase bounds
 * (0–180 degrees), free-rate bounds (0.05–20 Hz), the sync ceiling derived
 * from the project's tempo/division range (80 Hz), 0–20 ms peak depth units,
 * and fractional interpolation are project-authored, not Fractal parity.
 * This class does not reproduce Fractal DSP, presets,
 * controls, or sound. Mono/stereo
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
    static constexpr float kMinimumTimeLfoRateHz = 0.05F;
    static constexpr float kMaximumTimeLfoRateHz = 20.0F;
    // Project-derived ceiling: 300 BPM at a 64th-note division is 80 Hz.
    static constexpr float kMaximumTimeLfoSyncRateHz = 80.0F;
    static constexpr float kMinimumTimeLfoPhaseDegrees = 0.0F;
    static constexpr float kMaximumTimeLfoPhaseDegrees = 180.0F;
    static constexpr float kMaximumTimeLfoDepthMs = 20.0F;
    static constexpr float kMaximumLinkFeedback = 0.45F;
    static constexpr float kMinimumMotorSpeed = 0.8F;
    static constexpr float kMaximumMotorSpeed = 1.2F;
    static constexpr float kMaximumReadHeadSlewPerSample = 0.2F;
    static constexpr float kMaximumLevel = 2.0F;
    static constexpr float kMaximumSignal = 8.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static bool valid_time_mode(DelayTimeMode mode) noexcept;
    [[nodiscard]] static bool valid_division(DelayDivision division) noexcept;
    [[nodiscard]] static float beats_for_division(DelayDivision division) noexcept;
    [[nodiscard]] static bool valid_lfo_waveform(
        MultiDelayLfoWaveform waveform) noexcept;
    [[nodiscard]] std::size_t read_frame(std::size_t delay,
                                         std::size_t line) const noexcept;
    [[nodiscard]] float read_interpolated(float delay,
                                          std::size_t line) const noexcept;
    [[nodiscard]] float filter_line(float input, std::size_t line) noexcept;
    [[nodiscard]] static float safe_signal(float value) noexcept;
    void update_line_times(bool initialize_tape_heads = false) noexcept;
    void prepare_time_lfos() noexcept;
    void reset_time_lfos() noexcept;
    [[nodiscard]] float time_lfo_waveform_value(std::size_t lfo,
                                                std::size_t line) const noexcept;
    [[nodiscard]] float time_lfo_offset_frames(std::size_t line) const noexcept;
    void advance_time_lfos() noexcept;

    float sample_rate_ = 48000.0F;
    float requested_max_delay_ms_ = kMaximumDelayMs;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;

    MultiDelayParameters parameters_{};
    std::array<std::size_t, multi_delay_line_count> delay_frames_{};
    std::array<float, multi_delay_line_count> read_delay_frames_{};
    float read_head_slew_per_sample_ = kMaximumReadHeadSlewPerSample;
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
    std::array<float, multi_delay_time_lfo_count> time_lfo_depth_frames_{};
    std::array<float, multi_delay_time_lfo_count> time_lfo_phase_increment_{};
    std::array<float, multi_delay_time_lfo_count> time_lfo_sine_increment_{};
    std::array<float, multi_delay_time_lfo_count> time_lfo_cosine_increment_{};
    std::array<float, multi_delay_time_lfo_count> time_lfo_base_phase_{};
    std::array<std::array<float, multi_delay_line_count>,
               multi_delay_time_lfo_count> time_lfo_tap_phase_{};
    std::array<std::array<float, multi_delay_line_count>,
               multi_delay_time_lfo_count> time_lfo_tap_sine_{};
    std::array<std::array<float, multi_delay_line_count>,
               multi_delay_time_lfo_count> time_lfo_tap_cosine_{};
    std::array<std::array<std::uint32_t, multi_delay_line_count>,
               multi_delay_time_lfo_count> time_lfo_random_state_{};
    std::array<std::array<float, multi_delay_line_count>,
               multi_delay_time_lfo_count> time_lfo_random_value_{};
    std::array<std::uint32_t, multi_delay_time_lfo_count>
        time_lfo_normalization_counter_{};
    bool time_lfo_active_ = false;
};

} // namespace tabletfx
