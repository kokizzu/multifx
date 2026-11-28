#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/delay_timing.hpp"
#include "tabletfx/effects/formant_filter.hpp"
#include "tabletfx/effects/four_band_resonator.hpp"
#include "tabletfx/effects/resonator.hpp"
#include "tabletfx/effects/diatonic_harmonizer.hpp"
#include "tabletfx/effects/fractal_diatonic_harmonizer.hpp"
#include "tabletfx/effects/pitch_arpeggiator.hpp"
#include "tabletfx/effects/mono_guitar_synth.hpp"
#include "tabletfx/effects/note_generator.hpp"
#include "tabletfx/effects/octobot.hpp"
#include "tabletfx/effects/octave_fuzz.hpp"
#include "tabletfx/effects/octave_divider.hpp"
#include "tabletfx/effects/double_take.hpp"
#include "tabletfx/effects/eq5_graphic.hpp"
#include "tabletfx/effects/xr1_global_eq.hpp"
#include "tabletfx/effects/fractal_bias_trem.hpp"
#include "tabletfx/effects/vibratone.hpp"
#include "tabletfx/effects/optical_tremolo.hpp"
#include "tabletfx/effects/rock_box_dist.hpp"
#include "tabletfx/effects/rock_box_boost.hpp"
#include "tabletfx/effects/stereo_panner.hpp"
#include "tabletfx/effects/am_ring_mod.hpp"
#include "tabletfx/effects/tape_echo.hpp"
#include "tabletfx/effects/fractal_three_voice_synth.hpp"
#include "tabletfx/effects/enhancer.hpp"
#include "tabletfx/effects/resonant_delay.hpp"
#include "tabletfx/effects/pitch_feedback_delay.hpp"
#include "tabletfx/effects/feedback_generator.hpp"
#include "tabletfx/effects/shimmer_reverb.hpp"
#include "tabletfx/effects/grain_delay.hpp"
#include "tabletfx/effects/harmonic_resonator.hpp"
#include "tabletfx/effects/multiband_compressor.hpp"
#include "tabletfx/effects/bit_crusher.hpp"
#include "tabletfx/effects/harmonic_tremolo.hpp"
#include "tabletfx/effects/auto_swell_delay.hpp"
#include "tabletfx/effects/auto_pan.hpp"
#include "tabletfx/effects/megatap_delay.hpp"
#include "tabletfx/effects/de_esser.hpp"
#include "tabletfx/effects/pugilist.hpp"
#include "tabletfx/effects/vibe.hpp"
#include "tabletfx/effects/multi_delay.hpp"
#include "tabletfx/effects/plex_delay.hpp"
#include "tabletfx/effects/celestial_reverb.hpp"
#include "tabletfx/effects/cloud_reverb.hpp"
#include "tabletfx/effects/mercator_distortion.hpp"
#include "tabletfx/effects/porta_preamp.hpp"
#include "tabletfx/effects/pitch_sequence.hpp"
#include "tabletfx/effects/poly_sustain.hpp"
#include "tabletfx/effects/step_filter_delay.hpp"
#include "tabletfx/effects/step_tremolo.hpp"
#include "tabletfx/effects/stereo_filter.hpp"
#include "tabletfx/effects/volume_pan.hpp"
#include "tabletfx/effects/studio_pan_delay.hpp"
#include "tabletfx/effects/shuffling_looper.hpp"
#include "tabletfx/effects/ten_tap_delay.hpp"
#include "tabletfx/effects/dimension_chorus.hpp"
#include "tabletfx/effects/tone_master_pro_3d_chorus.hpp"
#include "tabletfx/effects/tone_master_modulated_large_hall.hpp"
#include "tabletfx/effects/pitch_ring_mod.hpp"
#include "tabletfx/effects/fractal_surround_delay.hpp"
#include "tabletfx/effects/poly_detune.hpp"
#include "tabletfx/effects/fractal_chromatic_harmonizer.hpp"
#include "tabletfx/effects/triple_double_chorus.hpp"
#include "tabletfx/effects/modulated_delay.hpp"
#include "tabletfx/effects/five_band_eq.hpp"
#include "tabletfx/effects/stadium_seven_band_peq.hpp"
#include "tabletfx/effects/looper.hpp"
#include "tabletfx/effects/deluxe_looper.hpp"
#include "tabletfx/effects/chromatic_pitch_delay.hpp"
#include "tabletfx/effects/mutant_filter.hpp"
#include "tabletfx/effects/phase_invert.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tabletfx {

class Gain final : public Effect {
public:
    explicit Gain(float gain_db = 0.0F, float sample_rate = 48000.0F,
                  float smoothing_ms = 10.0F) noexcept;
    [[nodiscard]] std::string_view id() const noexcept override { return "gain"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }
    void set_gain_db(float gain_db) noexcept;
    void set_gain_db_immediate(float gain_db) noexcept;
    [[nodiscard]] float gain_db() const noexcept { return gain_db_; }
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;
    void configure_smoothing(float sample_rate, float smoothing_ms) noexcept;

private:
    float gain_db_;
    float gain_;
    float target_gain_;
    float ramp_step_;
    float sample_rate_;
    float smoothing_ms_;
    std::size_t ramp_remaining_ = 0;
};

struct NoiseGateParameters {
    float threshold_db = -60.0F;
    float attenuation_db = -72.0F;
    float hysteresis_db = 3.0F;
    float attack_ms = 2.0F;
    float hold_ms = 20.0F;
    float release_ms = 80.0F;
};

class NoiseGate final : public Effect {
public:
    NoiseGate(float sample_rate, float threshold_db = -60.0F,
              float attenuation_db = -72.0F, float attack_ms = 2.0F,
              float release_ms = 80.0F) noexcept;
    NoiseGate(float sample_rate, NoiseGateParameters parameters) noexcept;
    [[nodiscard]] std::string_view id() const noexcept override { return "noise-gate"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }
    void configure(float sample_rate, float threshold_db, float attenuation_db,
                   float attack_ms, float release_ms) noexcept;
    void configure(float sample_rate, NoiseGateParameters parameters) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    float sample_rate_;
    float threshold_;
    float close_threshold_;
    float closed_gain_;
    float attack_coefficient_;
    float release_coefficient_;
    float envelope_ = 0.0F;
    float gain_ = 0.0F;
    std::size_t hold_duration_samples_ = 0;
    std::size_t hold_remaining_samples_ = 0;
    bool gate_open_ = false;
};

class SoftDrive final : public Effect {
public:
    explicit SoftDrive(float drive = 1.0F) noexcept;
    [[nodiscard]] std::string_view id() const noexcept override { return "soft-drive"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }
    void set_drive(float drive) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;

private:
    float drive_;
    float normalization_;
};

inline constexpr std::size_t maximum_delay_taps = 4;

class Delay final : public Effect {
public:
    Delay(float sample_rate, std::uint16_t channels, float max_delay_ms = 2000.0F,
          float delay_ms = 350.0F, float feedback = 0.25F, float mix = 0.25F,
          bool reserve_dual_line = false);
    [[nodiscard]] std::string_view id() const noexcept override { return "delay"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }
    [[nodiscard]] std::size_t delay_samples() const noexcept { return delay_samples_; }
    [[nodiscard]] float delay_time_ms() const noexcept {
        return static_cast<float>(static_cast<double>(delay_samples_) * 1000.0 /
                                  static_cast<double>(sample_rate_));
    }
    [[nodiscard]] float storage_peak() const noexcept;
    [[nodiscard]] float tap_sample(std::uint16_t channel) const noexcept;
    [[nodiscard]] std::size_t write_frame() const noexcept { return write_frame_; }
    [[nodiscard]] float feedback() const noexcept { return target_feedback_; }
    [[nodiscard]] float mix() const noexcept { return target_mix_; }
    [[nodiscard]] bool ping_pong_enabled() const noexcept { return target_ping_pong_; }
    [[nodiscard]] bool dual_enabled() const noexcept { return target_dual_; }
    [[nodiscard]] float tempo_bpm() const noexcept { return tempo_bpm_; }
    [[nodiscard]] DelayDivision division() const noexcept { return division_; }
    [[nodiscard]] DelayTimeMode time_mode() const noexcept { return time_mode_; }
    void configure(float delay_ms, float feedback, float mix) noexcept;
    // Project-authored Trails on bypass. Disabled by default; when enabled, the
    // existing delay line drains its stored repeats under dry input passthrough.
    void configure_trails_on_bypass(bool enabled) noexcept;
    // Project-authored BBD-style coloration: bandwidth-limited, mildly saturated
    // repeats. This reuses the existing delay ring and adds no processor stage.
    void configure_bbd_style(bool enabled) noexcept;
    // Project-authored BBD repeat mode. MOD uses gentler saturation and a
    // Project interpretation of Fractal Global Mix for this Delay's shared Mix:
    // effective Mix = clamp(base Mix + offset, 0, 1). The offset is normalized
    // wet-ratio units, bounded to +/-0.50 (50 percentage points). Disabled/zero
    // is identity. Fractal's exact equation is not documented; this is not parity.
    void configure_global_mix_offset(bool enabled, float offset) noexcept;
    // small repeat-path gain match at 0.5 linear level. This does not claim
    // HeadRush DSP, control-curve, or active/bypass level-matching parity.
    void configure_bbd_mode(bool mod) noexcept;
    // Project-authored tape-style echo with bounded saturation and speed modulation.
    void configure_tape_echo(bool enabled, float saturation, float wow_depth_ms,
                             float wow_rate_hz, float flutter_depth_ms,
                             float flutter_rate_hz) noexcept;
    // Stereo wet routing crossfades over 10 ms. A mono instance keeps normal
    // same-channel delay behavior regardless of this setting.
    void configure_ping_pong(bool enabled) noexcept;
    // Independent stereo L/R delay lines. A mono instance needs the optional
    // second line reserved at construction and mixes both wet taps to mono.
    void configure_dual(float delay_a_ms, float feedback_a, float mix_a,
                        float delay_b_ms, float feedback_b, float mix_b) noexcept;
    // Four mono-derived taps with independent time, level and equal-power pan.
    // Panning is audible in stereo; mono output sums the taps at center.
    void configure_multitap(bool enabled, float feedback, float mix) noexcept;
    void configure_multitap_tap(std::size_t tap, float delay_ms,
                                float level, float pan) noexcept;
    // Selects up to four project-authored tape playback heads using the existing
    // multitap ring; this is not a model of a specific tape machine. Bit i selects
    // a head at (i + 1) times the spacing; selected
    // heads share normalized level and alternate stereo pan. This controls head
    // routing only; tape coloration remains independently controlled above.
    void configure_tape_heads(bool enabled, float head_spacing_ms = 110.0F,
                              std::uint8_t selected_heads = 0x0FU) noexcept;
    // Project-authored repeat-path high-pass/low-pass approximation; it is not
    // a claim about Axom firmware or DSP. At 48 kHz, project control bounds are
    // 20–1000 Hz (low cut) and 2–20 kHz (high cut), with sample-rate limits at
    // lower rates. Non-finite controls use 120 Hz / 6 kHz. Disabled by default;
    // affects repeats written into the delay ring, not the dry path.
    void configure_repeat_filters(bool enabled, float low_cut_hz,
                                  float high_cut_hz) noexcept;
    void configure_tempo(float bpm, DelayDivision division, float feedback,
                         float mix) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    [[nodiscard]] std::size_t samples_for_ms(float delay_ms) const noexcept;
    [[nodiscard]] static float beats_for_division(DelayDivision division) noexcept;
    [[nodiscard]] float read_delay(std::size_t delay_samples,
                                   std::uint16_t channel) const noexcept;
    [[nodiscard]] float read_fractional_delay(float delay_samples,
                                              std::uint16_t channel) const noexcept;
    [[nodiscard]] float shape_delay_feedback(float value,
                                             std::uint16_t channel) noexcept;
    void set_delay_samples(std::size_t samples) noexcept;
    void set_dual_delay_samples(std::uint16_t line, std::size_t samples) noexcept;
    void retarget_parameters() noexcept;
    void advance_ramps() noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    std::size_t storage_channels_;
    bool dual_capable_;
    std::size_t delay_samples_;
    float transition_from_delay_samples_ = 1.0F;
    std::size_t delay_transition_remaining_ = 0;
    std::size_t delay_transition_total_ = 1;
    float feedback_;
    float target_feedback_;
    float feedback_step_ = 0.0F;
    float mix_;
    float target_mix_;
    float mix_step_ = 0.0F;
    std::size_t parameter_ramp_remaining_ = 0;
    std::size_t parameter_ramp_total_ = 1;
    float tempo_bpm_ = 120.0F;
    DelayDivision division_ = DelayDivision::quarter;
    DelayTimeMode time_mode_ = DelayTimeMode::milliseconds;
    std::size_t write_frame_ = 0;
    std::size_t capacity_frames_;
    std::vector<float> storage_;
    std::array<std::size_t, 2> dual_delay_samples_{1, 1};
    std::array<float, 2> dual_transition_from_delay_samples_{1.0F, 1.0F};
    std::array<std::size_t, 2> dual_delay_transition_remaining_{};
    std::array<float, 2> dual_feedback_{};
    std::array<float, 2> target_dual_feedback_{};
    std::array<float, 2> dual_feedback_step_{};
    std::array<float, 2> dual_mix_{};
    std::array<float, 2> target_dual_mix_{};
    std::array<float, 2> dual_mix_step_{};
    std::size_t dual_parameter_ramp_remaining_ = 0;
    std::array<std::size_t, maximum_delay_taps> multitap_delay_samples_{};
    std::array<float, maximum_delay_taps> multitap_transition_from_delay_samples_{};
    std::array<std::size_t, maximum_delay_taps> multitap_delay_transition_remaining_{};
    std::array<float, maximum_delay_taps> multitap_level_{};
    std::array<float, maximum_delay_taps> target_multitap_level_{};
    std::array<float, maximum_delay_taps> multitap_level_step_{};
    std::array<float, maximum_delay_taps> multitap_pan_left_{};
    std::array<float, maximum_delay_taps> target_multitap_pan_left_{};
    std::array<float, maximum_delay_taps> multitap_pan_left_step_{};
    std::array<float, maximum_delay_taps> multitap_pan_right_{};
    std::array<float, maximum_delay_taps> target_multitap_pan_right_{};
    std::array<float, maximum_delay_taps> multitap_pan_right_step_{};
    std::array<std::size_t, maximum_delay_taps> multitap_parameter_ramp_remaining_{};
    float bbd_mix_ = 0.0F;
    float target_bbd_mix_ = 0.0F;
    float bbd_mix_step_ = 0.0F;
    std::size_t bbd_ramp_remaining_ = 0;
    float bbd_filter_alpha_ = 0.5F;
    std::array<float, 2> bbd_filter_state_{};
    float bbd_mod_mix_ = 0.0F;
    float target_bbd_mod_mix_ = 0.0F;
    float bbd_mod_mix_step_ = 0.0F;
    std::size_t bbd_mode_ramp_remaining_ = 0;
    float tape_mix_ = 0.0F;
    float target_tape_mix_ = 0.0F;
    float tape_mix_step_ = 0.0F;
    std::size_t tape_ramp_remaining_ = 0;
    float tape_saturation_ = 0.45F;
    float target_tape_saturation_ = 0.45F;
    float tape_saturation_step_ = 0.0F;
    float tape_wow_depth_samples_ = 0.0F;
    float target_tape_wow_depth_samples_ = 0.0F;
    float tape_wow_depth_step_ = 0.0F;
    float tape_flutter_depth_samples_ = 0.0F;
    float target_tape_flutter_depth_samples_ = 0.0F;
    float tape_flutter_depth_step_ = 0.0F;
    float tape_wow_phase_increment_ = 0.0F;
    float target_tape_wow_phase_increment_ = 0.0F;
    float tape_wow_phase_step_ = 0.0F;
    float tape_flutter_phase_increment_ = 0.0F;
    float target_tape_flutter_phase_increment_ = 0.0F;
    float tape_flutter_phase_step_ = 0.0F;
    float tape_wow_phase_ = 0.0F;
    float tape_flutter_phase_ = 0.0F;
    float tape_modulation_samples_ = 0.0F;
    std::size_t tape_parameter_ramp_remaining_ = 0;
    float repeat_filter_mix_ = 0.0F;
    float target_repeat_filter_mix_ = 0.0F;
    float repeat_filter_mix_step_ = 0.0F;
    std::size_t repeat_filter_ramp_remaining_ = 0;
    float repeat_highpass_alpha_ = 0.0F;
    float target_repeat_highpass_alpha_ = 0.0F;
    float repeat_highpass_alpha_step_ = 0.0F;
    float repeat_lowpass_alpha_ = 1.0F;
    float target_repeat_lowpass_alpha_ = 1.0F;
    float repeat_lowpass_alpha_step_ = 0.0F;
    std::array<float, 2> repeat_highpass_previous_input_{};
    std::array<float, 2> repeat_highpass_previous_output_{};
    std::array<float, 2> repeat_lowpass_state_{};
    bool target_multitap_ = false;
    float multitap_route_mix_ = 0.0F;
    float multitap_route_step_ = 0.0F;
    std::size_t multitap_route_ramp_remaining_ = 0;
    bool target_ping_pong_ = false;
    float ping_pong_mix_ = 0.0F;
    float ping_pong_step_ = 0.0F;
    std::size_t ping_pong_ramp_remaining_ = 0;
    bool target_dual_ = false;
    float dual_routing_mix_ = 0.0F;
    float dual_routing_step_ = 0.0F;
    std::size_t dual_routing_ramp_remaining_ = 0;
    bool trails_on_bypass_ = false;
    bool bypassed_cleared_ = false;
};

struct FilteredDelayParameters {
    float time_ms = 350.0F;
    float feedback = 0.25F;
    float mix = 0.25F;
    float low_cut_hz = 120.0F;
    float high_cut_hz = 6000.0F;
    DelayTimeMode time_mode = DelayTimeMode::milliseconds;
    float tempo_bpm = 120.0F;
    DelayDivision division = DelayDivision::quarter;
};

// Project-authored static filtered-repeat delay. It reuses Delay's preallocated
// ring and filters only the signal re-entering that ring; dry audio is immediate.
class FilteredDelay final : public Effect {
public:
    explicit FilteredDelay(float sample_rate = 48000.0F,
                           std::uint16_t channels = 1,
                           float max_delay_ms = 2000.0F,
                           FilteredDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "filtered-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] FilteredDelayParameters parameters() const noexcept {
        return parameters_;
    }
    void configure(FilteredDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    float sample_rate_;
    float max_delay_ms_;
    Delay delay_;
    FilteredDelayParameters parameters_{};
};

enum class SweepDelayWaveform : std::uint8_t {
    sine,
    triangle,
    square,
};

enum class SweepDelayFilterType : std::uint8_t {
    lowpass,
    bandpass,
    highpass,
};

struct SweepDelayParameters {
    float time_ms = 350.0F;
    float feedback = 0.25F;
    float mix = 0.25F;
    float start_hz = 250.0F;
    float stop_hz = 6000.0F;
    float resonance_q = 1.2F;
    float rate_hz = 0.5F;
    SweepDelayWaveform waveform = SweepDelayWaveform::sine;
    float phase_degrees = 90.0F;
    SweepDelayFilterType filter_type = SweepDelayFilterType::bandpass;
    DelayTimeMode sweep_rate_mode = DelayTimeMode::milliseconds;
    float sweep_tempo_bpm = 120.0F;
    DelayDivision sweep_tempo_division = DelayDivision::quarter;
};

// Project-authored delay with an LFO-swept low-pass, band-pass, or high-pass
// filter on the wet output only. Its numeric controls and DSP are project
// choices; it does not claim to reproduce a commercial Sweep Delay algorithm.
class SweepDelay final : public Effect {
public:
    explicit SweepDelay(float sample_rate = 48000.0F,
                        std::uint16_t channels = 1,
                        float max_delay_ms = 2000.0F,
                        SweepDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "sweep-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] SweepDelayParameters parameters() const noexcept {
        return parameters_;
    }
    void configure(SweepDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct BiquadState {
        float input_1 = 0.0F;
        float input_2 = 0.0F;
        float output_1 = 0.0F;
        float output_2 = 0.0F;
    };

    void retarget_ramps() noexcept;
    void advance_ramps() noexcept;
    [[nodiscard]] float lfo_value(float phase) const noexcept;
    [[nodiscard]] float filter_wet_sample(float input, std::uint16_t channel,
                                          float cutoff_hz,
                                          float resonance_q,
                                          SweepDelayFilterType filter_type) noexcept;
    [[nodiscard]] static float rate_hz_for(
        const SweepDelayParameters& parameters) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    float max_delay_ms_;
    Delay delay_;
    SweepDelayParameters parameters_{};
    std::array<BiquadState, 2> filter_state_{};
    float phase_ = 0.0F;
    float current_mix_ = 0.25F;
    float mix_step_ = 0.0F;
    float current_start_hz_ = 250.0F;
    float start_step_ = 0.0F;
    float current_stop_hz_ = 6000.0F;
    float stop_step_ = 0.0F;
    float current_resonance_q_ = 1.2F;
    float resonance_step_ = 0.0F;
    float current_rate_hz_ = 0.5F;
    float rate_step_ = 0.0F;
    float current_phase_degrees_ = 90.0F;
    float phase_step_ = 0.0F;
    std::size_t ramp_total_ = 0;
    std::size_t ramp_remaining_ = 0;
};

enum class RoomSize : std::uint8_t {
    small,
    large,
};

struct RoomReverbParameters {
    RoomSize size = RoomSize::small;
    float decay_seconds = 0.8F;
    float pre_delay_ms = 10.0F;
    float damping_hz = 6000.0F;
    float mix = 0.2F;
    float level_db = 0.0F;
    bool spillover = false;
};

// Project-authored Schroeder-style room reverb. The dry path has no added
// latency; predelay applies only to the wet path. All storage is allocated at
// construction and process() performs no allocation. Call reset() to clear
// the complete reverb tail while retaining the selected parameters. Changing
// room size while a tail is active changes the comb-delay geometry immediately;
// that tail may briefly change texture.
class RoomReverb final : public Effect {
public:
    RoomReverb(float sample_rate, std::uint16_t channels,
               RoomReverbParameters parameters = {});
    [[nodiscard]] std::string_view id() const noexcept override { return "room-reverb"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] RoomReverbParameters parameters() const noexcept { return parameters_; }
    void configure(RoomReverbParameters parameters) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                         std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct DelayLine {
        std::size_t offset = 0;
        std::size_t length = 1;
        std::size_t index = 0;
        float damping_state = 0.0F;
        float feedback = 0.0F;
        float feedback_target = 0.0F;
        float feedback_step = 0.0F;
    };

    void configure_size(RoomSize size) noexcept;
    void update_targets() noexcept;
    void advance_parameters() noexcept;
    void process_internal(std::span<float> interleaved, bool bypassed) noexcept;
    [[nodiscard]] float read_predelay(std::uint16_t channel, float input) noexcept;
    [[nodiscard]] float process_comb(DelayLine& line, float input, float damping_alpha) noexcept;
    [[nodiscard]] float process_diffuser(DelayLine& line, float input) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    RoomReverbParameters parameters_;
    float decay_seconds_ = 0.8F;
    float target_decay_seconds_ = 0.8F;
    float damping_alpha_ = 0.0F;
    float target_damping_alpha_ = 0.0F;
    float pre_delay_samples_ = 0.0F;
    float target_pre_delay_samples_ = 0.0F;
    float wet_mix_ = 0.2F;
    float target_wet_mix_ = 0.2F;
    float wet_level_ = 1.0F;
    float target_wet_level_ = 1.0F;
    float decay_step_ = 0.0F;
    float damping_step_ = 0.0F;
    float predelay_step_ = 0.0F;
    float mix_step_ = 0.0F;
    float level_step_ = 0.0F;
    std::size_t parameter_ramp_remaining_ = 0;
    std::size_t parameter_ramp_total_ = 1;
    std::vector<float> storage_;
    std::array<std::size_t, 2> predelay_offset_{};
    std::size_t predelay_capacity_ = 1;
    std::array<std::size_t, 2> predelay_index_{};
    std::array<DelayLine, 8> combs_{};
    std::array<DelayLine, 4> diffusers_{};
    bool bypass_cleared_ = true;
};

} // namespace tabletfx
