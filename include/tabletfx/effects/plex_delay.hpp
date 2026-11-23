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
inline constexpr std::size_t plex_delay_input_diffusion_storage_limit_bytes =
    256U * 1024U;

enum class PlexDelayStackMode : std::uint8_t {
    off,
    stack,
    hold,
};

enum class PlexDelayInputSelect : std::uint8_t {
    sum_lr,
    left,
    right,
};

enum class PlexWetFilterType : std::uint8_t {
    bypass,
    low_pass,
    band_pass,
    high_pass,
    low_shelf,
    high_shelf,
    peaking,
    notch,
    all_pass,
};

enum class PlexFilterLfoType : std::uint8_t {
    sine,
    triangle,
    square,
    saw,
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
    float input_diffusion_mix = 0.0F;
    float input_diffusion_time_ms = 10.0F;
    float low_cut_hz = 20.0F;
    float high_cut_hz = 20000.0F;
    float pre_delay_ms = 0.0F;
    PlexDelayStackMode stack_mode = PlexDelayStackMode::off;
    PlexDelayInputSelect input_select = PlexDelayInputSelect::sum_lr;
    std::array<float, plex_delay_line_count> line_level{
        1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, plex_delay_line_count> line_pan{
        -0.85F, -0.6F, -0.35F, -0.1F, 0.1F, 0.35F, 0.6F, 0.85F};
    float input_gain_db = 0.0F;
    float mix = 0.25F;
    float level_db = 0.0F;
    float balance = 0.0F;
    PlexWetFilterType wet_filter_type = PlexWetFilterType::bypass;
    float wet_filter_frequency_hz = 2000.0F;
    float wet_filter_q = 0.707F;
    float wet_filter_gain_db = 0.0F;
    bool filter_lfo_enabled = false;
    PlexFilterLfoType filter_lfo_type = PlexFilterLfoType::sine;
    float filter_lfo_rate_hz = 0.5F;
    float filter_mod_frequency_hz = 2000.0F;
    bool envelope_enabled = false;
    float envelope_threshold_db = -70.0F;
    float envelope_attack_ms = 400.0F;
    float envelope_release_ms = 15.0F;
    float ducker_attenuation_db = 0.0F;
    float ducker_threshold_db = -30.0F;
    float ducker_release_ms = 200.0F;
    float stereo_spread = 1.0F;
};

/*
 * Project-authored eight-line feedback-matrix delay. The November 2026 Fractal
 * Audio Blocks Guide, pp. 87–90, documents an FM9 Plex Delay with 1–8 lines,
 * matrix diffusion, separate input Diffusion Mix/Time, low/high cuts, pre-delay,
 * Stack/Hold, per-line time/tempo/level/pan, master controls, a wet filter/LFO,
 * envelope follower, ducker, stereo spread, common mix controls, and spillover.
 * This implementation includes the matrix core, timing, controls, wet filter/LFO,
 * envelope send, ducker, stereo spread, input selection, and a project-authored
 * three-stage input diffuser.

 * Matrix topology, feedback/decay mapping, filter and diffuser design, bounds,
 * defaults, input selection/summing, and storage caps are project choices. The
 * matrix/pre-delay rings are capped at 1 MiB; input diffusion has a separate
 * 256 KiB fixed cap. The remaining Filter-block types, delay-line modulation,
 * Plex Detune/Shift/Verb and Shimmer Verb algorithms, Kill Dry, Bypass, Bypass
 * Mode, Global Mix, spillover, and exact Fractal settings/DSP/sound parity are
 * not implemented.
 * Plex Delay itself has no pitch effects; Plex Detune and Plex Shift are
 * separate types.
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
        return sizeof(*this) + storage_.capacity() * sizeof(float) +
               input_diffusion_storage_.capacity() * sizeof(float);
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
    struct BiquadCoefficients {
        float b0 = 1.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };
    struct BiquadState {
        float input_1 = 0.0F;
        float input_2 = 0.0F;
        float output_1 = 0.0F;
        float output_2 = 0.0F;
    };

    static constexpr std::size_t kRingCount = plex_delay_line_count + 1U;
    static constexpr std::size_t kInputDiffusionStages = 3U;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumDelayMs = 1.0F;
    static constexpr float kMaximumDelayMs = 2000.0F;
    static constexpr float kMaximumInputDiffusionTimeMs = 50.0F;
    static constexpr float kMaximumSignal = 8.0F;
    static constexpr std::size_t kFilterCoefficientUpdateFrames = 16U;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static bool valid_time_mode(DelayTimeMode mode) noexcept;
    [[nodiscard]] static bool valid_division(DelayDivision division) noexcept;
    [[nodiscard]] static bool valid_stack_mode(PlexDelayStackMode mode) noexcept;
    [[nodiscard]] static bool valid_input_select(
        PlexDelayInputSelect input_select) noexcept;
    [[nodiscard]] static bool valid_filter_type(PlexWetFilterType type) noexcept;
    [[nodiscard]] static bool valid_lfo_type(PlexFilterLfoType type) noexcept;
    [[nodiscard]] static float beats_for_division(DelayDivision division) noexcept;
    [[nodiscard]] std::size_t read_index(std::size_t delay,
                                         std::size_t ring) const noexcept;
    [[nodiscard]] float process_input_diffusion(float input) noexcept;
    void update_input_diffusion_times() noexcept;
    void clear_input_diffusion() noexcept;
    [[nodiscard]] float filter_feedback(float input,
                                        std::size_t line) noexcept;
    [[nodiscard]] BiquadCoefficients make_wet_filter(float frequency_hz) const noexcept;
    void update_wet_filter_target(float frequency_hz) noexcept;
    [[nodiscard]] float process_wet_filter(float input,
                                          std::size_t channel) noexcept;
    void advance_wet_filter() noexcept;
    [[nodiscard]] float lfo_value() const noexcept;
    [[nodiscard]] static float safe_signal(float value) noexcept;
    void update_times() noexcept;

    float sample_rate_ = 48000.0F;
    float requested_max_delay_ms_ = kMaximumDelayMs;
    std::size_t capacity_frames_ = 0U;
    std::size_t write_frame_ = 0U;
    std::vector<float> storage_;
    std::size_t input_diffusion_capacity_frames_ = 0U;
    std::size_t input_diffusion_write_frame_ = 0U;
    std::vector<float> input_diffusion_storage_;
    std::array<std::size_t, kInputDiffusionStages>
        input_diffusion_delay_frames_{};
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
    std::array<BiquadState, 2U> wet_filter_state_{};
    BiquadCoefficients wet_filter_coefficients_{};
    BiquadCoefficients wet_filter_target_coefficients_{};
    BiquadCoefficients wet_filter_coefficient_step_{};
    std::size_t wet_filter_ramp_remaining_ = 0U;
    std::size_t wet_filter_update_remaining_ = 0U;
    float filter_lfo_phase_ = 0.0F;
    float filter_lfo_phase_increment_ = 0.0F;
    float envelope_value_ = 0.0F;
    float envelope_threshold_linear_ = 0.0F;
    float envelope_attack_coefficient_ = 0.0F;
    float envelope_release_coefficient_ = 0.0F;
    float ducker_threshold_linear_ = 0.0F;
    float ducker_attenuation_linear_ = 1.0F;
    float ducker_attack_coefficient_ = 0.0F;
    float ducker_release_coefficient_ = 0.0F;
    float ducker_envelope_ = 0.0F;
    float ducker_gain_ = 1.0F;
};

} // namespace tabletfx
