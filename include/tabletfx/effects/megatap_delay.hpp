#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

inline constexpr std::size_t megatap_delay_max_taps = 64;
inline constexpr std::size_t megatap_delay_storage_limit_bytes = 1024U * 1024U;

enum class MegatapAmplitudeShape : std::uint8_t {
    constant,
    increasing,
    decreasing,
    sine,
    alternating,
};

enum class MegatapPanShape : std::uint8_t {
    constant,
    increasing,
    decreasing,
    sine,
};

enum class MegatapInputSelect : std::uint8_t {
    sum,
    left,
    right,
};

struct MegatapDelayParameters {
    float delay_time_ms = 1000.0F;
    std::uint8_t tap_count = 16;
    float time_alpha = 1.0F;
    float time_randomize_percent = 0.0F;
    MegatapAmplitudeShape amplitude_shape = MegatapAmplitudeShape::decreasing;
    float amplitude_alpha = 1.0F;
    float amplitude_randomize_percent = 0.0F;
    MegatapPanShape pan_shape = MegatapPanShape::increasing;
    // In Constant pan mode this is the pan position; otherwise it is shape depth.
    float pan_alpha_percent = 100.0F;
    float stereo_spread_percent = 0.0F;
    float predelay_ms = 0.0F;
    float feedback = 0.0F;
    std::uint8_t feedback_tap = 16;
    float low_cut_hz = 20.0F;
    float high_cut_hz = 20000.0F;
    float diffusion_mix = 0.0F;
    float diffusion_time_ms = 10.0F;
    float diffusion_lfo_rate_hz = 0.5F;
    float diffusion_lfo_depth_ms = 0.0F;
    float envelope_threshold_db = -70.0F;
    float envelope_attack_ms = 400.0F;
    float envelope_release_ms = 15.0F;
    float input_gain_db = 0.0F;
    float mix = 0.25F;
    float level_db = 0.0F;
    MegatapInputSelect input_select = MegatapInputSelect::sum;
    bool envelope_enabled = false;
    std::uint32_t random_seed = 0x4D475450U;
};

/*
 * Project-authored dense multi-tap delay. Fractal's June 2023 Blocks Guide,
 * pp. 52–53, documents the FM9 Megatap as a four-second, 64-tap line with
 * controls for delay time, tap count, predelay, feedback/feedback tap, time
 * shaping/randomization, amplitude shaping/randomization, tap panning/stereo
 * spread, diffusion and its LFO, and an input envelope follower. The guide
 * gives a one-second maximum for Predelay and a 64-tap FM9 limit; most other
 * numeric ranges and defaults are not stated. The class models the documented
 * delay/tap controls, time/amplitude shaping and randomization, pan/spread,
 * feedback selection, wet tone cuts, diffusion, and input envelope concepts.
 * It does not expose Fractal's separate Bypass Mode, Balance, Global Mix, or
 * spillover controls.
 *
 * This class implements a related general-purpose delay, not Fractal DSP,
 * presets, parameter curves, or sound/control parity. The documented four-
 * second line and named controls are vendor facts; the project's 1 MiB ring
 * cap, effective maximum delay, control bounds/defaults, random sequence,
 * four-stage all-pass diffuser, filter topology, panning law, and input
 * envelope mapping are project choices. With the default five-second storage
 * request, the mono ring can hold four seconds of taps plus one second of
 * predelay at 48 kHz; higher sample rates may reduce that span under the fixed
 * memory cap.
 *
 * The mono delay ring intentionally accepts mono or stereo input and can
 * select left, right, or an average sum for the effect path. The dry signal
 * preserves its original channels. Processing is preallocated and performs
 * no allocations. The official FM9 downloads page checked 2026-11-22 lists
 * firmware v12.0 dated 2026-07-07 for FM9 Original and FM9 Turbo; it does not
 * separately name FM9 Mark II Turbo. Source documents:
 * https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
 * https://www.fractalaudio.com/fm9-downloads/
 */
class MegatapDelay final : public Effect {
public:
    explicit MegatapDelay(float sample_rate = 48000.0F,
                          float max_storage_ms = 5000.0F,
                          MegatapDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "megatap-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }
    [[nodiscard]] MegatapDelayParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float maximum_delay_ms() const noexcept;
    [[nodiscard]] std::size_t tap_delay_samples(std::size_t tap) const noexcept;
    [[nodiscard]] float tap_gain(std::size_t tap) const noexcept;
    [[nodiscard]] float tap_pan(std::size_t tap) const noexcept;

    // Bounds are project-authored and setting controls never resizes storage.
    void set_parameters(MegatapDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMaximumDelayMs = 4000.0F;
    static constexpr float kMaximumPredelayMs = 1000.0F;
    static constexpr float kMaximumTimeAlpha = 4.0F;
    static constexpr float kMinimumTimeAlpha = 0.25F;
    static constexpr float kMaximumAmplitudeAlpha = 4.0F;
    static constexpr float kMinimumAmplitudeAlpha = 0.25F;
    static constexpr float kMaximumDiffusionMs = 10.0F;
    static constexpr float kMaximumDiffusionModDepthMs = 2.0F;
    static constexpr std::size_t kDiffusionLineSamples = 4096U;
    static constexpr std::size_t kDiffusionStages = 4U;
    static constexpr float kMaximumSignal = 8.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static bool valid_amplitude_shape(
        MegatapAmplitudeShape shape) noexcept;
    [[nodiscard]] static bool valid_pan_shape(MegatapPanShape shape) noexcept;
    [[nodiscard]] static bool valid_input_select(
        MegatapInputSelect input_select) noexcept;
    [[nodiscard]] static float one_pole_coefficient(
        float sample_rate, float milliseconds) noexcept;
    [[nodiscard]] float read_delay(std::size_t delay_samples) const noexcept;
    [[nodiscard]] float diffuse(float input) noexcept;
    [[nodiscard]] float filter_tap(float input, std::size_t tap) noexcept;
    [[nodiscard]] float envelope_gain(float input) noexcept;
    [[nodiscard]] static float safe_signal(float value) noexcept;
    void rebuild_taps() noexcept;
    void rebuild_diffuser() noexcept;

    float sample_rate_ = 48000.0F;
    float requested_max_storage_ms_ = 5000.0F;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;

    MegatapDelayParameters parameters_{};
    std::array<std::size_t, megatap_delay_max_taps> tap_delay_samples_{};
    std::array<float, megatap_delay_max_taps> tap_gains_{};
    std::array<float, megatap_delay_max_taps> tap_pans_{};
    std::array<float, megatap_delay_max_taps> tap_left_gains_{};
    std::array<float, megatap_delay_max_taps> tap_right_gains_{};
    std::array<float, megatap_delay_max_taps> low_cut_input_{};
    std::array<float, megatap_delay_max_taps> low_cut_output_{};
    std::array<float, megatap_delay_max_taps> high_cut_output_{};
    float low_cut_coefficient_ = 0.0F;
    float high_cut_coefficient_ = 1.0F;
    float input_gain_linear_ = 1.0F;
    float output_level_linear_ = 1.0F;
    float low_cut_alpha_ = 0.0F;
    float envelope_threshold_ = 0.000316228F;
    float envelope_attack_coefficient_ = 0.0F;
    float envelope_release_coefficient_ = 0.0F;
    float envelope_ = 0.0F;
    float diffusion_lfo_phase_ = 0.0F;
    float diffusion_lfo_phase_step_ = 0.0F;
    float diffusion_mod_depth_samples_ = 0.0F;
    std::array<std::array<float, kDiffusionLineSamples>, kDiffusionStages>
        diffusion_lines_{};
    std::array<std::size_t, kDiffusionStages> diffusion_write_frames_{};
    std::array<std::size_t, kDiffusionStages> diffusion_delay_samples_{};
};

} // namespace tabletfx
