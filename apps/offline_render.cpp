#include "tabletfx/effects.hpp"
#include "tabletfx/channel_conversion.hpp"
#include "tabletfx/parallel_mixer.hpp"
#include "tabletfx/tuner.hpp"
#include "tabletfx/effects/clean_boost.hpp"
#include "tabletfx/effects/chorus.hpp"
#include "tabletfx/effects/compressor.hpp"
#include "tabletfx/effects/envelope_wah.hpp"
#include "tabletfx/effects/flanger.hpp"
#include "tabletfx/effects/filters.hpp"
#include "tabletfx/effects/high_gain_distortion.hpp"
#include "tabletfx/effects/mid_overdrive.hpp"
#include "tabletfx/effects/overdrive.hpp"
#include "tabletfx/effects/round_fuzz.hpp"
#include "tabletfx/effects/peak_limiter.hpp"
#include "tabletfx/effects/pedal_compressor.hpp"
#include "tabletfx/effects/phaser.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"
#include "tabletfx/effects/tremolo.hpp"
#include "tabletfx/effects/three_band_eq.hpp"
#include "tabletfx/effects/vibrato.hpp"
#include "tabletfx/effects/volume_pedal.hpp"
#include "tabletfx/effects/wah.hpp"
#include "tabletfx/level_meter.hpp"
#include "tabletfx/signal_controls.hpp"
#include "tabletfx/wav.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    float gain_db = 0.0F;
    float output_gain_db = 0.0F;
    std::uint16_t output_channels = 0;
    tabletfx::MonoSumPolicy mono_sum_policy = tabletfx::MonoSumPolicy::Average;
    bool volume_pedal_enabled = false;
    tabletfx::VolumePedalParameters volume_pedal{};
    bool limiter_enabled = false;
    float limiter_ceiling_db = -1.0F;
    float limiter_release_ms = 80.0F;
    bool wah_enabled = false;
    tabletfx::WahParameters wah{};
    bool envelope_wah_enabled = false;
    tabletfx::EnvelopeWahParameters envelope_wah{};
    bool clean_boost_enabled = false;
    float clean_boost_db = 0.0F;
    bool overdrive_enabled = false;
    tabletfx::TransparentOverdriveParameters overdrive{};
    bool mid_overdrive_enabled = false;
    tabletfx::MidFocusedOverdriveParameters mid_overdrive{};
    bool high_gain_distortion_enabled = false;
    tabletfx::HighGainDistortionParameters high_gain_distortion{};
    bool round_fuzz_enabled = false;
    tabletfx::RoundFuzzParameters round_fuzz{};
    bool tremolo_enabled = false;
    tabletfx::TremoloParameters tremolo{};
    bool chorus_enabled = false;
    tabletfx::ChorusParameters chorus{};
    bool flanger_enabled = false;
    tabletfx::FlangerParameters flanger{};
    bool phaser_enabled = false;
    tabletfx::PhaserParameters phaser{};
    bool vibrato_enabled = false;
    tabletfx::VibratoParameters vibrato{};
    float high_pass_hz = 0.0F;
    float low_pass_hz = 0.0F;
    bool noise_gate_enabled = false;
    tabletfx::NoiseGateParameters noise_gate{};
    float drive = 1.0F;
    float delay_ms = 0.0F;
    float delay_feedback = 0.25F;
    float delay_mix = 0.0F;
    bool delay_tempo_enabled = false;
    float delay_tempo_bpm = 120.0F;
    tabletfx::DelayDivision delay_division = tabletfx::DelayDivision::quarter;
    bool delay_ping_pong = false;
    bool delay_dual_enabled = false;
    bool delay_bbd_style = false;
    float delay_dual_a_ms = 350.0F;
    float delay_dual_a_feedback = 0.25F;
    float delay_dual_a_mix = 0.25F;
    float delay_dual_b_ms = 525.0F;
    float delay_dual_b_feedback = 0.25F;
    float delay_dual_b_mix = 0.25F;
    bool delay_multitap_enabled = false;
    std::array<float, tabletfx::maximum_delay_taps> delay_multitap_ms{
        250.0F, 500.0F, 750.0F, 1000.0F};
    std::array<float, tabletfx::maximum_delay_taps> delay_multitap_level{
        0.65F, 0.45F, 0.30F, 0.20F};
    std::array<float, tabletfx::maximum_delay_taps> delay_multitap_pan{
        -0.55F, 0.55F, -0.25F, 0.25F};
    std::size_t delay_multitap_custom_count = 0;
    bool pitch_enabled = false;
    float pitch_semitones = 0.0F;
    bool compressor_enabled = false;
    tabletfx::CompressorParameters compressor{};
    bool pedal_compressor_enabled = false;
    tabletfx::PedalCompressorParameters pedal_compressor{};
    bool eq_enabled = false;
    float eq_band1_frequency_hz = 120.0F;
    float eq_band1_gain_db = 0.0F;
    float eq_band1_q = 0.707F;
    float eq_band2_frequency_hz = 800.0F;
    float eq_band2_gain_db = 0.0F;
    float eq_band2_q = 0.707F;
    float eq_band3_frequency_hz = 5000.0F;
    float eq_band3_gain_db = 0.0F;
    float eq_band3_q = 0.707F;
};

float parse_float(std::string_view name, const char* raw) {
    std::size_t parsed = 0;
    const std::string text(raw);
    const float value = std::stof(text, &parsed);
    if (parsed != text.size() || !std::isfinite(value)) {
        throw std::invalid_argument("Invalid numeric argument for " + std::string(name) + ": " + text);
    }
    return value;
}

tabletfx::DelayDivision parse_delay_division(std::string_view value) {
    if (value == "whole") return tabletfx::DelayDivision::whole;
    if (value == "dotted-half") return tabletfx::DelayDivision::dotted_half;
    if (value == "half") return tabletfx::DelayDivision::half;
    if (value == "dotted-quarter") return tabletfx::DelayDivision::dotted_quarter;
    if (value == "quarter") return tabletfx::DelayDivision::quarter;
    if (value == "quarter-triplet") return tabletfx::DelayDivision::quarter_triplet;
    if (value == "dotted-eighth") return tabletfx::DelayDivision::dotted_eighth;
    if (value == "eighth") return tabletfx::DelayDivision::eighth;
    if (value == "eighth-triplet") return tabletfx::DelayDivision::eighth_triplet;
    if (value == "dotted-sixteenth") return tabletfx::DelayDivision::dotted_sixteenth;
    if (value == "sixteenth") return tabletfx::DelayDivision::sixteenth;
    if (value == "sixteenth-triplet") return tabletfx::DelayDivision::sixteenth_triplet;
    throw std::invalid_argument("Unsupported delay division: " + std::string(value));
}

Options parse_options(int argc, char** argv, int first) {
    Options options;
    for (int i = first; i < argc; ++i) {
        const std::string_view key(argv[i]);
        if (key == "--delay-multitap") {
            options.delay_multitap_enabled = true;
            continue;
        }
        if (key == "--delay-bbd") {
            options.delay_bbd_style = true;
            continue;
        }
        if (i + 1 >= argc) {
            throw std::invalid_argument("Missing value for " + std::string(key));
        }
        if (key == "--output-channels") {
            const std::string_view value(argv[++i]);
            if (value == "preserve") options.output_channels = 0;
            else if (value == "mono" || value == "1") options.output_channels = 1;
            else if (value == "stereo" || value == "2") options.output_channels = 2;
            else throw std::invalid_argument("Output channels must be preserve, mono, or stereo");
            continue;
        }
        if (key == "--delay-preset") {
            const std::string_view value(argv[++i]);
            if (value == "digital") {
                options.delay_ms = 350.0F;
                options.delay_feedback = 0.25F;
                options.delay_mix = 0.25F;
                options.delay_tempo_enabled = false;
                options.delay_bbd_style = false;
            } else if (value == "slapback") {
                options.delay_ms = 110.0F;
                options.delay_feedback = 0.12F;
                options.delay_mix = 0.25F;
                options.delay_tempo_enabled = false;
                options.delay_bbd_style = false;
            } else if (value == "bbd") {
                options.delay_ms = 280.0F;
                options.delay_feedback = 0.28F;
                options.delay_mix = 0.25F;
                options.delay_tempo_enabled = false;
                options.delay_bbd_style = true;
            } else {
                throw std::invalid_argument("Delay preset must be digital, slapback, or bbd");
            }
            continue;
        }
        if (key == "--delay-tap") {
            if (i + 3 >= argc) {
                throw std::invalid_argument("--delay-tap requires time-ms level pan");
            }
            if (options.delay_multitap_custom_count >= tabletfx::maximum_delay_taps) {
                throw std::invalid_argument("At most four --delay-tap entries are supported");
            }
            const std::size_t tap = options.delay_multitap_custom_count++;
            options.delay_multitap_ms[tap] = parse_float("--delay-tap time", argv[i + 1]);
            options.delay_multitap_level[tap] = parse_float("--delay-tap level", argv[i + 2]);
            options.delay_multitap_pan[tap] = parse_float("--delay-tap pan", argv[i + 3]);
            options.delay_multitap_enabled = true;
            i += 3;
            continue;
        }
        if (key == "--delay-ping-pong") {
            options.delay_ping_pong = true;
            continue;
        }
        if (key == "--delay-dual") {
            options.delay_dual_enabled = true;
            continue;
        }
        if (key == "--delay-division") {
            options.delay_division = parse_delay_division(argv[++i]);
            options.delay_tempo_enabled = true;
            continue;
        }
        if (key == "--mono-sum") {
            const std::string_view value(argv[++i]);
            if (value == "average") options.mono_sum_policy = tabletfx::MonoSumPolicy::Average;
            else if (value == "equal-power") options.mono_sum_policy = tabletfx::MonoSumPolicy::EqualPower;
            else throw std::invalid_argument("Mono sum policy must be average or equal-power");
            continue;
        }
        if (key == "--flanger-feedback-polarity") {
            const std::string_view value(argv[++i]);
            if (value == "positive") {
                options.flanger.feedback_polarity = tabletfx::FlangerFeedbackPolarity::Positive;
            } else if (value == "negative") {
                options.flanger.feedback_polarity = tabletfx::FlangerFeedbackPolarity::Negative;
            } else {
                throw std::invalid_argument("Flanger feedback polarity must be positive or negative");
            }
            options.flanger_enabled = true;
            continue;
        }
        if (key == "--phaser-stages") {
            const std::string_view value(argv[++i]);
            if (value == "4") options.phaser.stages = 4;
            else if (value == "6") options.phaser.stages = 6;
            else if (value == "8") options.phaser.stages = 8;
            else throw std::invalid_argument("Phaser stages must be 4, 6, or 8");
            options.phaser_enabled = true;
            continue;
        }
        if (key == "--volume-curve") {
            const std::string_view value(argv[++i]);
            if (value == "linear") {
                options.volume_pedal.taper = tabletfx::VolumePedalTaper::LinearAmplitude;
            } else if (value == "audio") {
                options.volume_pedal.taper = tabletfx::VolumePedalTaper::AudioTaperDb;
            } else {
                throw std::invalid_argument("Volume curve must be 'linear' or 'audio'");
            }
            options.volume_pedal_enabled = true;
            continue;
        }
        if (key == "--tremolo-waveform") {
            const std::string_view value(argv[++i]);
            if (value == "sine") {
                options.tremolo.waveform = tabletfx::TremoloWaveform::Sine;
            } else if (value == "triangle") {
                options.tremolo.waveform = tabletfx::TremoloWaveform::Triangle;
            } else if (value == "square") {
                options.tremolo.waveform = tabletfx::TremoloWaveform::Square;
            } else {
                throw std::invalid_argument("Tremolo waveform must be sine, triangle, or square");
            }
            options.tremolo_enabled = true;
            continue;
        }
        if (key == "--tremolo-sync") {
            const std::string_view value(argv[++i]);
            if (value == "on") {
                options.tremolo.tempo_sync = true;
            } else if (value == "off") {
                options.tremolo.tempo_sync = false;
            } else {
                throw std::invalid_argument("Tremolo sync must be 'on' or 'off'");
            }
            options.tremolo_enabled = true;
            continue;
        }
        if (key == "--touch-wah-direction") {
            const std::string_view value(argv[++i]);
            if (value == "up") {
                options.envelope_wah.direction = tabletfx::EnvelopeWahDirection::Up;
            } else if (value == "down") {
                options.envelope_wah.direction = tabletfx::EnvelopeWahDirection::Down;
            } else {
                throw std::invalid_argument("Touch-wah direction must be 'up' or 'down'");
            }
            options.envelope_wah_enabled = true;
            continue;
        }
        const float value = parse_float(key, argv[++i]);
        if (key == "--gain-db") options.gain_db = value;
        else if (key == "--output-gain-db") options.output_gain_db = value;
        else if (key == "--volume-position") {
            options.volume_pedal.position = value;
            options.volume_pedal_enabled = true;
        }
        else if (key == "--volume-min-db") {
            options.volume_pedal.minimum_db = value;
            options.volume_pedal_enabled = true;
        }
        else if (key == "--volume-max-db") {
            options.volume_pedal.maximum_db = value;
            options.volume_pedal_enabled = true;
        }
        else if (key == "--limiter-ceiling-db") {
            options.limiter_ceiling_db = value;
            options.limiter_enabled = true;
        }
        else if (key == "--limiter-release-ms") {
            options.limiter_release_ms = value;
            options.limiter_enabled = true;
        }
        else if (key == "--wah-position") {
            options.wah.position = value;
            options.wah_enabled = true;
        }
        else if (key == "--wah-min-hz") {
            options.wah.minimum_hz = value;
            options.wah_enabled = true;
        }
        else if (key == "--wah-max-hz") {
            options.wah.maximum_hz = value;
            options.wah_enabled = true;
        }
        else if (key == "--wah-q") {
            options.wah.resonance_q = value;
            options.wah_enabled = true;
        }
        else if (key == "--wah-level-db") {
            options.wah.level_db = value;
            options.wah_enabled = true;
        }
        else if (key == "--touch-wah-sensitivity") {
            options.envelope_wah.sensitivity = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-min-hz") {
            options.envelope_wah.minimum_hz = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-max-hz") {
            options.envelope_wah.maximum_hz = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-q") {
            options.envelope_wah.resonance_q = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-level-db") {
            options.envelope_wah.level_db = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-attack-ms") {
            options.envelope_wah.attack_ms = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-release-ms") {
            options.envelope_wah.release_ms = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-smoothing-ms") {
            options.envelope_wah.smoothing_ms = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--tremolo-rate-hz") {
            options.tremolo.rate_hz = value;
            options.tremolo_enabled = true;
        }
        else if (key == "--tremolo-depth") {
            options.tremolo.depth = value;
            options.tremolo_enabled = true;
        }
        else if (key == "--tremolo-tempo-bpm") {
            options.tremolo.tempo_bpm = value;
            options.tremolo_enabled = true;
        }
        else if (key == "--tremolo-beats-per-cycle") {
            options.tremolo.beats_per_cycle = value;
            options.tremolo_enabled = true;
        }
        else if (key == "--chorus-rate-hz") {
            options.chorus.rate_hz = value;
            options.chorus_enabled = true;
        }
        else if (key == "--chorus-delay-ms") {
            options.chorus.base_delay_ms = value;
            options.chorus_enabled = true;
        }
        else if (key == "--chorus-depth-ms") {
            options.chorus.depth_ms = value;
            options.chorus_enabled = true;
        }
        else if (key == "--chorus-mix") {
            options.chorus.mix = value;
            options.chorus_enabled = true;
        }
        else if (key == "--flanger-rate-hz") {
            options.flanger.rate_hz = value;
            options.flanger_enabled = true;
        }
        else if (key == "--flanger-delay-ms") {
            options.flanger.base_delay_ms = value;
            options.flanger_enabled = true;
        }
        else if (key == "--flanger-depth-ms") {
            options.flanger.depth_ms = value;
            options.flanger_enabled = true;
        }
        else if (key == "--flanger-feedback") {
            options.flanger.feedback = value;
            options.flanger_enabled = true;
        }
        else if (key == "--flanger-mix") {
            options.flanger.mix = value;
            options.flanger_enabled = true;
        }
        else if (key == "--phaser-rate-hz") {
            options.phaser.rate_hz = value;
            options.phaser_enabled = true;
        }
        else if (key == "--phaser-min-hz") {
            options.phaser.minimum_hz = value;
            options.phaser_enabled = true;
        }
        else if (key == "--phaser-max-hz") {
            options.phaser.maximum_hz = value;
            options.phaser_enabled = true;
        }
        else if (key == "--phaser-feedback") {
            options.phaser.feedback = value;
            options.phaser_enabled = true;
        }
        else if (key == "--phaser-mix") {
            options.phaser.mix = value;
            options.phaser_enabled = true;
        }
        else if (key == "--vibrato-rate-hz") {
            options.vibrato.rate_hz = value;
            options.vibrato_enabled = true;
        }
        else if (key == "--vibrato-depth-ms") {
            options.vibrato.depth_ms = value;
            options.vibrato_enabled = true;
        }
        else if (key == "--vibrato-mix") {
            options.vibrato.mix = value;
            options.vibrato_enabled = true;
        }
        else if (key == "--overdrive-drive") {
            options.overdrive.drive = value;
            options.overdrive_enabled = true;
        }
        else if (key == "--overdrive-level-db") {
            options.overdrive.level_db = value;
            options.overdrive_enabled = true;
        }
        else if (key == "--overdrive-mix") {
            options.overdrive.mix = value;
            options.overdrive_enabled = true;
        }
        else if (key == "--mid-overdrive-drive") {
            options.mid_overdrive.drive = value;
            options.mid_overdrive_enabled = true;
        }
        else if (key == "--mid-overdrive-boost-db") {
            options.mid_overdrive.mid_boost_db = value;
            options.mid_overdrive_enabled = true;
        }
        else if (key == "--mid-overdrive-level-db") {
            options.mid_overdrive.level_db = value;
            options.mid_overdrive_enabled = true;
        }
        else if (key == "--mid-overdrive-mix") {
            options.mid_overdrive.mix = value;
            options.mid_overdrive_enabled = true;
        }
        else if (key == "--high-gain-drive") {
            options.high_gain_distortion.drive = value;
            options.high_gain_distortion_enabled = true;
        }
        else if (key == "--high-gain-pre-low-cut-hz") {
            options.high_gain_distortion.pre_low_cut_hz = value;
            options.high_gain_distortion_enabled = true;
        }
        else if (key == "--high-gain-pre-high-cut-hz") {
            options.high_gain_distortion.pre_high_cut_hz = value;
            options.high_gain_distortion_enabled = true;
        }
        else if (key == "--high-gain-post-tone-hz") {
            options.high_gain_distortion.post_tone_hz = value;
            options.high_gain_distortion_enabled = true;
        }
        else if (key == "--high-gain-level-db") {
            options.high_gain_distortion.level_db = value;
            options.high_gain_distortion_enabled = true;
        }
        else if (key == "--high-gain-mix") {
            options.high_gain_distortion.mix = value;
            options.high_gain_distortion_enabled = true;
        }
        else if (key == "--round-fuzz") {
            options.round_fuzz.fuzz = value;
            options.round_fuzz_enabled = true;
        }
        else if (key == "--round-fuzz-level-db") {
            options.round_fuzz.level_db = value;
            options.round_fuzz_enabled = true;
        }
        else if (key == "--round-fuzz-mix") {
            options.round_fuzz.mix = value;
            options.round_fuzz_enabled = true;
        }
        else if (key == "--high-pass-hz") options.high_pass_hz = value;
        else if (key == "--low-pass-hz") options.low_pass_hz = value;
        else if (key == "--gate-threshold-db") {
            options.noise_gate.threshold_db = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--gate-attenuation-db") {
            options.noise_gate.attenuation_db = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--gate-hysteresis-db") {
            options.noise_gate.hysteresis_db = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--gate-attack-ms") {
            options.noise_gate.attack_ms = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--gate-hold-ms") {
            options.noise_gate.hold_ms = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--gate-release-ms") {
            options.noise_gate.release_ms = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--drive") options.drive = value;
        else if (key == "--boost-db") {
            options.clean_boost_db = value;
            options.clean_boost_enabled = true;
        }
        else if (key == "--delay-ms") options.delay_ms = value;
        else if (key == "--delay-sync-bpm") {
            options.delay_tempo_bpm = value;
            options.delay_tempo_enabled = true;
        }
        else if (key == "--delay-feedback") options.delay_feedback = value;
        else if (key == "--delay-mix") options.delay_mix = value;
        else if (key == "--delay-dual-a-ms") {
            options.delay_dual_a_ms = value;
            options.delay_dual_enabled = true;
        }
        else if (key == "--delay-dual-a-feedback") {
            options.delay_dual_a_feedback = value;
            options.delay_dual_enabled = true;
        }
        else if (key == "--delay-dual-a-mix") {
            options.delay_dual_a_mix = value;
            options.delay_dual_enabled = true;
        }
        else if (key == "--delay-dual-b-ms") {
            options.delay_dual_b_ms = value;
            options.delay_dual_enabled = true;
        }
        else if (key == "--delay-dual-b-feedback") {
            options.delay_dual_b_feedback = value;
            options.delay_dual_enabled = true;
        }
        else if (key == "--delay-dual-b-mix") {
            options.delay_dual_b_mix = value;
            options.delay_dual_enabled = true;
        }
        else if (key == "--pitch-semitones") {
            options.pitch_semitones = value;
            options.pitch_enabled = true;
        }
        else if (key == "--compressor-threshold-db") {
            options.compressor.threshold_db = value;
            options.compressor_enabled = true;
        }
        else if (key == "--compressor-ratio") {
            options.compressor.ratio = value;
            options.compressor_enabled = true;
        }
        else if (key == "--compressor-knee-db") {
            options.compressor.knee_db = value;
            options.compressor_enabled = true;
        }
        else if (key == "--compressor-attack-ms") {
            options.compressor.attack_ms = value;
            options.compressor_enabled = true;
        }
        else if (key == "--compressor-release-ms") {
            options.compressor.release_ms = value;
            options.compressor_enabled = true;
        }
        else if (key == "--compressor-makeup-db") {
            options.compressor.makeup_db = value;
            options.compressor_enabled = true;
        }
        else if (key == "--pedal-compressor-sensitivity") {
            options.pedal_compressor.sensitivity = value;
            options.pedal_compressor_enabled = true;
        }
        else if (key == "--pedal-compressor-sustain") {
            options.pedal_compressor.sustain = value;
            options.pedal_compressor_enabled = true;
        }
        else if (key == "--pedal-compressor-attack") {
            options.pedal_compressor.attack = value;
            options.pedal_compressor_enabled = true;
        }
        else if (key == "--pedal-compressor-level-db") {
            options.pedal_compressor.level_db = value;
            options.pedal_compressor_enabled = true;
        }
        else if (key == "--eq-band1-frequency-hz") {
            options.eq_band1_frequency_hz = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band1-gain-db") {
            options.eq_band1_gain_db = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band1-q") {
            options.eq_band1_q = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band2-frequency-hz") {
            options.eq_band2_frequency_hz = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band2-gain-db") {
            options.eq_band2_gain_db = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band2-q") {
            options.eq_band2_q = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band3-frequency-hz") {
            options.eq_band3_frequency_hz = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band3-gain-db") {
            options.eq_band3_gain_db = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band3-q") {
            options.eq_band3_q = value;
            options.eq_enabled = true;
        }
        else throw std::invalid_argument("Unknown option: " + std::string(key));
    }
    return options;
}

std::vector<std::unique_ptr<tabletfx::Effect>> make_effects(const Options& options,
                                                            std::uint32_t sample_rate,
                                                            std::uint16_t channels) {
    std::vector<std::unique_ptr<tabletfx::Effect>> effects;
    if (std::abs(options.gain_db) > 0.0001F) {
        effects.push_back(std::make_unique<tabletfx::Gain>(
            options.gain_db, static_cast<float>(sample_rate)));
    }
    if (options.volume_pedal_enabled) {
        effects.push_back(std::make_unique<tabletfx::VolumePedal>(
            static_cast<float>(sample_rate), options.volume_pedal));
    }
    if (options.high_pass_hz > 0.0F) {
        effects.push_back(std::make_unique<tabletfx::HighPassFilter>(
            static_cast<float>(sample_rate), channels, options.high_pass_hz));
    }
    if (options.low_pass_hz > 0.0F) {
        effects.push_back(std::make_unique<tabletfx::LowPassFilter>(
            static_cast<float>(sample_rate), channels, options.low_pass_hz));
    }
    if (options.noise_gate_enabled) {
        effects.push_back(std::make_unique<tabletfx::NoiseGate>(
            static_cast<float>(sample_rate), options.noise_gate));
    }
    if (options.pedal_compressor_enabled) {
        effects.push_back(std::make_unique<tabletfx::PedalCompressor>(
            static_cast<float>(sample_rate), options.pedal_compressor));
    }
    if (options.compressor_enabled) {
        effects.push_back(std::make_unique<tabletfx::Compressor>(
            static_cast<float>(sample_rate), options.compressor));
    }
    if (options.wah_enabled) {
        effects.push_back(std::make_unique<tabletfx::Wah>(
            static_cast<float>(sample_rate), channels, options.wah));
    }
    if (options.envelope_wah_enabled) {
        effects.push_back(std::make_unique<tabletfx::EnvelopeWah>(
            static_cast<float>(sample_rate), channels, options.envelope_wah));
    }
    if (options.clean_boost_enabled) {
        effects.push_back(std::make_unique<tabletfx::CleanBoost>(
            options.clean_boost_db, static_cast<float>(sample_rate)));
    }
    if (options.overdrive_enabled) {
        effects.push_back(std::make_unique<tabletfx::TransparentOverdrive>(
            static_cast<float>(sample_rate), options.overdrive));
    }
    if (options.mid_overdrive_enabled) {
        effects.push_back(std::make_unique<tabletfx::MidFocusedOverdrive>(
            static_cast<float>(sample_rate), options.mid_overdrive));
    }
    if (options.high_gain_distortion_enabled) {
        effects.push_back(std::make_unique<tabletfx::HighGainDistortion>(
            static_cast<float>(sample_rate), options.high_gain_distortion));
    }
    if (options.round_fuzz_enabled) {
        effects.push_back(std::make_unique<tabletfx::RoundFuzz>(
            static_cast<float>(sample_rate), options.round_fuzz));
    }
    if (options.drive > 1.0001F) {
        effects.push_back(std::make_unique<tabletfx::SoftDrive>(options.drive));
    }
    if (options.tremolo_enabled) {
        effects.push_back(std::make_unique<tabletfx::Tremolo>(
            static_cast<float>(sample_rate), options.tremolo));
    }
    if (options.chorus_enabled) {
        effects.push_back(std::make_unique<tabletfx::Chorus>(
            static_cast<float>(sample_rate), channels, options.chorus));
    }
    if (options.flanger_enabled) {
        effects.push_back(std::make_unique<tabletfx::Flanger>(
            static_cast<float>(sample_rate), channels, options.flanger));
    }
    if (options.phaser_enabled) {
        effects.push_back(std::make_unique<tabletfx::Phaser>(
            static_cast<float>(sample_rate), channels, options.phaser));
    }
    if (options.vibrato_enabled) {
        effects.push_back(std::make_unique<tabletfx::Vibrato>(
            static_cast<float>(sample_rate), channels, options.vibrato));
    }
    if (options.eq_enabled) {
        auto eq = std::make_unique<tabletfx::ThreeBandParametricEQ>(
            static_cast<float>(sample_rate), channels);
        eq->configure(options.eq_band1_frequency_hz, options.eq_band1_gain_db, options.eq_band1_q,
                      options.eq_band2_frequency_hz, options.eq_band2_gain_db, options.eq_band2_q,
                      options.eq_band3_frequency_hz, options.eq_band3_gain_db, options.eq_band3_q);
        effects.push_back(std::move(eq));
    }
    if (options.delay_multitap_enabled || options.delay_dual_enabled ||
        options.delay_bbd_style ||
        ((options.delay_ms > 0.0F || options.delay_tempo_enabled) && options.delay_mix > 0.0F)) {
        auto delay = std::make_unique<tabletfx::Delay>(
            static_cast<float>(sample_rate), channels, 5000.0F,
            options.delay_ms > 0.0F ? options.delay_ms : 350.0F,
            options.delay_feedback, options.delay_mix, options.delay_dual_enabled);
        if (options.delay_tempo_enabled) {
            delay->configure_tempo(options.delay_tempo_bpm, options.delay_division,
                                   options.delay_feedback, options.delay_mix);
        }
        delay->configure_bbd_style(options.delay_bbd_style);
        if (options.delay_multitap_enabled) {
            for (std::size_t tap = 0; tap < tabletfx::maximum_delay_taps; ++tap) {
                delay->configure_multitap_tap(tap, options.delay_multitap_ms[tap],
                                              options.delay_multitap_level[tap],
                                              options.delay_multitap_pan[tap]);
            }
            delay->configure_multitap(true, options.delay_feedback, options.delay_mix);
            delay->reset();
        } else if (options.delay_dual_enabled) {
            delay->configure_dual(options.delay_dual_a_ms, options.delay_dual_a_feedback,
                                  options.delay_dual_a_mix, options.delay_dual_b_ms,
                                  options.delay_dual_b_feedback, options.delay_dual_b_mix);
            delay->reset();
        } else if (options.delay_ping_pong) {
            delay->configure_ping_pong(true);
            delay->reset();
        }
        effects.push_back(std::move(delay));
    }
    if (options.pitch_enabled) {
        auto pitch = std::make_unique<tabletfx::PolyPitchShifter>(
            static_cast<float>(sample_rate), channels);
        pitch->set_pitch_semitones(options.pitch_semitones);
        effects.push_back(std::move(pitch));
    }
    if (std::abs(options.output_gain_db) > 0.0001F) {
        effects.push_back(std::make_unique<tabletfx::Gain>(
            options.output_gain_db, static_cast<float>(sample_rate)));
    }
    if (options.limiter_enabled) {
        effects.push_back(std::make_unique<tabletfx::PeakLimiter>(
            static_cast<float>(sample_rate), options.limiter_ceiling_db,
            options.limiter_release_ms));
    }
    return effects;
}

void process_in_blocks(tabletfx::AudioBuffer& audio,
                       std::vector<std::unique_ptr<tabletfx::Effect>>& effects,
                       std::size_t block_frames) {
    const std::size_t channels = audio.channels;
    for (std::size_t offset = 0; offset < audio.frames(); offset += block_frames) {
        const std::size_t frames = std::min(block_frames, audio.frames() - offset);
        std::span<float> block(audio.interleaved.data() + offset * channels, frames * channels);
        for (const auto& effect : effects) {
            effect->process(block, audio.channels);
        }
    }
}

struct Metrics {
    double peak = 0.0;
    double rms = 0.0;
};

Metrics metrics(std::span<const float> samples) {
    Metrics result;
    double sum_squares = 0.0;
    for (float sample : samples) {
        const double value = static_cast<double>(sample);
        result.peak = std::max(result.peak, std::abs(value));
        sum_squares += value * value;
    }
    if (!samples.empty()) {
        result.rms = std::sqrt(sum_squares / static_cast<double>(samples.size()));
    }
    return result;
}

double percentile(std::vector<double>& values, double fraction) {
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(values.size() - 1)));
    return values[index];
}

template <typename Factory>
void benchmark_one(std::string_view name, Factory make_effect, std::uint32_t sample_rate,
                   std::uint16_t channels, std::size_t block_frames,
                   std::size_t warmup_blocks, std::size_t measured_blocks) {
    auto effect = make_effect();
    std::vector<float> buffer(block_frames * channels);
    std::uint32_t noise = 0x1234567U;
    for (float& sample : buffer) {
        noise = noise * 1664525U + 1013904223U;
        sample = static_cast<float>((noise >> 8U) & 0xffffU) / 32768.0F - 1.0F;
    }
    const std::vector<float> source_buffer = buffer;
    for (std::size_t i = 0; i < warmup_blocks; ++i) {
        std::copy(source_buffer.begin(), source_buffer.end(), buffer.begin());
        effect->process(buffer, channels);
    }
    effect->reset();
    std::vector<double> percentages;
    percentages.reserve(measured_blocks);
    const double deadline_us = static_cast<double>(block_frames) * 1'000'000.0 / sample_rate;
    for (std::size_t i = 0; i < measured_blocks; ++i) {
        std::copy(source_buffer.begin(), source_buffer.end(), buffer.begin());
        const auto start = Clock::now();
        effect->process(buffer, channels);
        const auto end = Clock::now();
        const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        percentages.push_back(100.0 * elapsed_us / deadline_us);
    }
    const Metrics signal = metrics(buffer);
    std::cout << name << "," << std::fixed << std::setprecision(4)
              << percentile(percentages, 0.50) << ","
              << percentile(percentages, 0.95) << ","
              << percentile(percentages, 0.99) << ","
              << percentile(percentages, 0.999) << ","
              << *std::max_element(percentages.begin(), percentages.end()) << ","
              << effect->memory_bytes() << ","
              << signal.rms << '\n';
}

void benchmark_signal_controls(std::uint32_t sample_rate, std::uint16_t channels,
                               std::size_t block_frames, std::size_t warmup_blocks,
                               std::size_t measured_blocks) {
    tabletfx::SignalControls controls(static_cast<float>(sample_rate));
    std::vector<float> input(block_frames * channels);
    std::vector<float> dry(block_frames * channels);
    std::vector<float> wet(block_frames * channels);
    std::vector<float> output(block_frames * channels);
    std::uint32_t noise = 0x1234567U;
    for (float& sample : input) {
        noise = noise * 1664525U + 1013904223U;
        sample = static_cast<float>((noise >> 8U) & 0xffffU) / 32768.0F - 1.0F;
    }
    std::copy(input.begin(), input.end(), wet.begin());
    const auto process = [&] {
        controls.process_input(input, dry, channels);
        controls.process_output(dry, wet, output, channels);
    };
    for (std::size_t i = 0; i < warmup_blocks; ++i) process();
    controls.reset();
    std::vector<double> percentages;
    percentages.reserve(measured_blocks);
    const double deadline_us = static_cast<double>(block_frames) * 1'000'000.0 / sample_rate;
    for (std::size_t i = 0; i < measured_blocks; ++i) {
        const auto start = Clock::now();
        process();
        const auto end = Clock::now();
        const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        percentages.push_back(100.0 * elapsed_us / deadline_us);
    }
    const Metrics signal = metrics(output);
    std::cout << "signal-controls," << std::fixed << std::setprecision(4)
              << percentile(percentages, 0.50) << ","
              << percentile(percentages, 0.95) << ","
              << percentile(percentages, 0.99) << ","
              << percentile(percentages, 0.999) << ","
              << *std::max_element(percentages.begin(), percentages.end()) << ","
              << controls.memory_bytes() << "," << signal.rms << '\n';
}

void benchmark_level_meters(std::uint32_t sample_rate, std::uint16_t channels,
                            std::size_t block_frames, std::size_t warmup_blocks,
                            std::size_t measured_blocks) {
    tabletfx::LevelMeter input_meter;
    tabletfx::LevelMeter output_meter;
    std::vector<float> samples(block_frames * channels);
    std::uint32_t noise = 0x7654321U;
    float meter_readback = 0.0F;
    for (float& sample : samples) {
        noise = noise * 1664525U + 1013904223U;
        sample = static_cast<float>((noise >> 8U) & 0xffffU) / 65536.0F - 0.5F;
    }
    const auto process = [&] {
        input_meter.process(samples, channels);
        output_meter.process(samples, channels);
        meter_readback = input_meter.peak() + input_meter.rms() +
                         static_cast<float>(input_meter.clip_flags()) +
                         static_cast<float>(input_meter.invalid_samples()) +
                         output_meter.peak() + output_meter.rms() +
                         static_cast<float>(output_meter.clip_flags()) +
                         static_cast<float>(output_meter.invalid_samples());
    };
    for (std::size_t i = 0; i < warmup_blocks; ++i) process();
    input_meter.reset();
    output_meter.reset();
    std::vector<double> percentages;
    percentages.reserve(measured_blocks);
    const double deadline_us = static_cast<double>(block_frames) * 1'000'000.0 / sample_rate;
    for (std::size_t i = 0; i < measured_blocks; ++i) {
        const auto start = Clock::now();
        process();
        const auto end = Clock::now();
        const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        percentages.push_back(100.0 * elapsed_us / deadline_us);
    }
    if (meter_readback < 0.0F) std::terminate();
    const Metrics signal = metrics(samples);
    const std::size_t memory_bytes = input_meter.memory_bytes() + output_meter.memory_bytes();
    std::cout << "level-meters-input-output," << std::fixed << std::setprecision(4)
              << percentile(percentages, 0.50) << ","
              << percentile(percentages, 0.95) << ","
              << percentile(percentages, 0.99) << ","
              << percentile(percentages, 0.999) << ","
              << *std::max_element(percentages.begin(), percentages.end()) << ","
              << memory_bytes << "," << signal.rms << '\n';
}

void benchmark_channel_conversion(std::uint32_t sample_rate,
                                  std::size_t block_frames,
                                  std::size_t warmup_blocks,
                                  std::size_t measured_blocks) {
    std::vector<float> stereo(block_frames * 2, 0.25F);
    std::vector<float> mono(block_frames, 0.0F);
    const double deadline_us = static_cast<double>(block_frames) * 1'000'000.0 / sample_rate;

    const auto measure = [&](std::string_view name, const auto& convert,
                             std::span<const float> input, std::span<float> output) {
        for (std::size_t i = 0; i < warmup_blocks; ++i) convert(input, output);
        std::vector<double> percentages;
        percentages.reserve(measured_blocks);
        for (std::size_t i = 0; i < measured_blocks; ++i) {
            const auto start = Clock::now();
            convert(input, output);
            const auto end = Clock::now();
            const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
            percentages.push_back(100.0 * elapsed_us / deadline_us);
        }
        const Metrics signal = metrics(output);
        std::cout << name << "," << std::fixed << std::setprecision(4)
                  << percentile(percentages, 0.50) << ","
                  << percentile(percentages, 0.95) << ","
                  << percentile(percentages, 0.99) << ","
                  << percentile(percentages, 0.999) << ","
                  << *std::max_element(percentages.begin(), percentages.end()) << ",0,"
                  << signal.rms << '\n';
    };

    measure("stereo-to-mono-average", [&](auto input, auto output) {
        if (!tabletfx::convert_channels(input, 2, output, 1).succeeded()) {
            throw std::runtime_error("stereo-to-mono benchmark conversion failed");
        }
    }, stereo, mono);
    measure("stereo-to-mono-equal-power", [&](auto input, auto output) {
        if (!tabletfx::convert_channels(input, 2, output, 1,
                                        tabletfx::MonoSumPolicy::EqualPower).succeeded()) {
            throw std::runtime_error("equal-power benchmark conversion failed");
        }
    }, stereo, mono);
    measure("mono-to-stereo", [&](auto input, auto output) {
        if (!tabletfx::convert_channels(input, 1, output, 2).succeeded()) {
            throw std::runtime_error("mono-to-stereo benchmark conversion failed");
        }
    }, mono, stereo);
}

void benchmark_parallel_routing(std::uint32_t sample_rate,
                                std::uint16_t channels,
                                std::size_t block_frames,
                                std::size_t warmup_blocks,
                                std::size_t measured_blocks) {
    const std::size_t samples = block_frames * channels;
    const double deadline_us = static_cast<double>(block_frames) * 1'000'000.0 / sample_rate;
    std::vector<float> input(samples, 0.25F);
    std::vector<float> branch_a(samples);
    std::vector<float> branch_b(samples);
    std::vector<float> dry(samples, 0.125F);
    std::vector<float> output(samples);

    const auto report = [&](std::string_view name, const auto& operation,
                            std::size_t memory_bytes, std::span<const float> result_buffer) {
        for (std::size_t i = 0; i < warmup_blocks; ++i) operation();
        std::vector<double> percentages;
        percentages.reserve(measured_blocks);
        for (std::size_t i = 0; i < measured_blocks; ++i) {
            const auto start = Clock::now();
            operation();
            const auto end = Clock::now();
            const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
            percentages.push_back(100.0 * elapsed_us / deadline_us);
        }
        const Metrics signal = metrics(result_buffer);
        std::cout << name << "," << std::fixed << std::setprecision(4)
                  << percentile(percentages, 0.50) << ","
                  << percentile(percentages, 0.95) << ","
                  << percentile(percentages, 0.99) << ","
                  << percentile(percentages, 0.999) << ","
                  << *std::max_element(percentages.begin(), percentages.end()) << ","
                  << memory_bytes << "," << signal.rms << '\n';
    };

    const auto split = [&] {
        const auto result = tabletfx::split_to_parallel(input, branch_a, branch_b, channels);
        if (!result.succeeded() || result.frames != block_frames) {
            throw std::runtime_error("parallel split benchmark failed");
        }
    };
    report("parallel-split", split, 0, branch_b);

    tabletfx::ParallelMixer mixer(static_cast<float>(sample_rate));
    tabletfx::ParallelMixParameters parameters;
    parameters.dry_gain = 0.7F;
    parameters.branch_a_gain = 0.35F;
    parameters.branch_a_balance = 0.2F;
    parameters.branch_b_gain = 0.25F;
    parameters.branch_b_balance = -0.1F;
    parameters.branch_b_inverted = true;
    mixer.set_parameters_immediate(parameters);
    const auto mix = [&] {
        const auto result = mixer.process(dry, branch_a, branch_b, output, channels);
        if (!result.succeeded() || result.frames != block_frames) {
            throw std::runtime_error("parallel mix benchmark failed");
        }
    };
    report("parallel-mix", mix, mixer.memory_bytes(), output);
}

void benchmark_chromatic_tuner(std::uint32_t sample_rate,
                               std::uint16_t channels,
                               std::size_t block_frames,
                               std::size_t warmup_blocks,
                               std::size_t measured_blocks) {
    const std::size_t samples = block_frames * channels;
    const double deadline_us = static_cast<double>(block_frames) * 1'000'000.0 / sample_rate;
    std::vector<float> source(samples);
    std::vector<float> buffer(samples);
    constexpr double frequency = 93.75;
    for (std::size_t frame = 0; frame < block_frames; ++frame) {
        const float value = 0.35F * static_cast<float>(
            std::sin(2.0 * 3.14159265358979323846 * frequency *
                     static_cast<double>(frame) / static_cast<double>(sample_rate)));
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            source[frame * channels + channel] = value;
        }
    }
    tabletfx::ChromaticTuner tuner(static_cast<float>(sample_rate));
    std::vector<double> percentages;
    percentages.reserve(measured_blocks);
    const auto operation = [&] {
        std::copy(source.begin(), source.end(), buffer.begin());
        const auto start = Clock::now();
        tuner.process(buffer, channels);
        const auto end = Clock::now();
        const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        return 100.0 * elapsed_us / deadline_us;
    };
    for (std::size_t index = 0; index < warmup_blocks; ++index) {
        (void)operation();
    }
    tuner.reset();
    for (std::size_t index = 0; index < measured_blocks; ++index) {
        percentages.push_back(operation());
    }
    const auto signal = metrics(buffer);
    std::cout << "chromatic-tuner-monitor," << std::fixed << std::setprecision(4)
              << percentile(percentages, 0.50) << ","
              << percentile(percentages, 0.95) << ","
              << percentile(percentages, 0.99) << ","
              << percentile(percentages, 0.999) << ","
              << *std::max_element(percentages.begin(), percentages.end()) << ","
              << tuner.memory_bytes() << "," << signal.rms << '\n';
}

void benchmark_full_chain(std::uint32_t sample_rate, std::uint16_t channels,
                          std::size_t block_frames, std::size_t warmup_blocks,
                          std::size_t measured_blocks) {
    tabletfx::CompressorParameters compressor_parameters;
    compressor_parameters.threshold_db = -24.0F;
    compressor_parameters.ratio = 3.0F;
    auto eq = std::make_unique<tabletfx::ThreeBandParametricEQ>(sample_rate, channels);
    eq->configure(120.0F, 2.0F, 0.707F, 800.0F, -1.0F, 0.707F,
                  5000.0F, 1.5F, 0.707F);

    std::vector<std::unique_ptr<tabletfx::Effect>> effects;
    effects.push_back(std::make_unique<tabletfx::Gain>(2.0F, static_cast<float>(sample_rate)));
    tabletfx::VolumePedalParameters volume_parameters;
    volume_parameters.position = 0.82F;
    effects.push_back(std::make_unique<tabletfx::VolumePedal>(
        static_cast<float>(sample_rate), volume_parameters));
    effects.push_back(std::make_unique<tabletfx::HighPassFilter>(sample_rate, channels, 80.0F));
    effects.push_back(std::make_unique<tabletfx::LowPassFilter>(sample_rate, channels, 12000.0F));
    effects.push_back(std::make_unique<tabletfx::NoiseGate>(sample_rate, -72.0F));
    effects.push_back(std::make_unique<tabletfx::PedalCompressor>(sample_rate));
    effects.push_back(std::make_unique<tabletfx::Compressor>(sample_rate, compressor_parameters));
    tabletfx::WahParameters wah_parameters;
    wah_parameters.position = 0.62F;
    effects.push_back(std::make_unique<tabletfx::Wah>(sample_rate, channels, wah_parameters));
    tabletfx::EnvelopeWahParameters envelope_wah_parameters;
    envelope_wah_parameters.sensitivity = 0.8F;
    effects.push_back(std::make_unique<tabletfx::EnvelopeWah>(
        static_cast<float>(sample_rate), channels, envelope_wah_parameters));
    effects.push_back(std::make_unique<tabletfx::CleanBoost>(3.0F, static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::TransparentOverdrive>(
        static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::MidFocusedOverdrive>(
        static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::HighGainDistortion>(
        static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::RoundFuzz>(
        static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::SoftDrive>(3.0F));
    tabletfx::TremoloParameters tremolo_parameters;
    tremolo_parameters.rate_hz = 4.0F;
    tremolo_parameters.depth = 0.2F;
    effects.push_back(std::make_unique<tabletfx::Tremolo>(sample_rate, tremolo_parameters));
    tabletfx::ChorusParameters chorus_parameters;
    chorus_parameters.rate_hz = 0.8F;
    chorus_parameters.base_delay_ms = 15.0F;
    chorus_parameters.depth_ms = 7.0F;
    chorus_parameters.mix = 0.2F;
    effects.push_back(std::make_unique<tabletfx::Chorus>(
        static_cast<float>(sample_rate), channels, chorus_parameters));
    tabletfx::FlangerParameters flanger_parameters;
    flanger_parameters.rate_hz = 0.25F;
    flanger_parameters.base_delay_ms = 2.0F;
    flanger_parameters.depth_ms = 1.0F;
    flanger_parameters.feedback = 0.35F;
    flanger_parameters.mix = 0.25F;
    effects.push_back(std::make_unique<tabletfx::Flanger>(
        static_cast<float>(sample_rate), channels, flanger_parameters));
    tabletfx::PhaserParameters phaser_parameters;
    phaser_parameters.feedback = 0.15F;
    phaser_parameters.mix = 0.2F;
    effects.push_back(std::make_unique<tabletfx::Phaser>(
        static_cast<float>(sample_rate), channels, phaser_parameters));
    tabletfx::VibratoParameters vibrato_parameters;
    vibrato_parameters.mix = 0.35F;
    effects.push_back(std::make_unique<tabletfx::Vibrato>(
        static_cast<float>(sample_rate), channels, vibrato_parameters));
    effects.push_back(std::move(eq));
    effects.push_back(std::make_unique<tabletfx::Delay>(sample_rate, channels, 2000.0F,
                                                        125.0F, 0.2F, 0.1F));
    auto pitch = std::make_unique<tabletfx::PolyPitchShifter>(
        static_cast<float>(sample_rate), channels);
    pitch->set_pitch_semitones(5.0F);
    effects.push_back(std::move(pitch));
    effects.push_back(std::make_unique<tabletfx::Gain>(-1.0F, static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::PeakLimiter>(sample_rate, -1.0F, 80.0F));
    tabletfx::SignalControls signal_controls(static_cast<float>(sample_rate));
    tabletfx::LevelMeter input_meter;
    tabletfx::LevelMeter output_meter;

    std::vector<float> buffer(block_frames * channels);
    std::vector<float> dry_buffer(block_frames * channels);
    std::vector<float> wet_buffer(block_frames * channels);
    std::vector<float> output_buffer(block_frames * channels);
    std::uint32_t noise = 0x1234567U;
    for (float& sample : buffer) {
        noise = noise * 1664525U + 1013904223U;
        sample = static_cast<float>((noise >> 8U) & 0xffffU) / 32768.0F - 1.0F;
    }
    const std::vector<float> source_buffer = buffer;
    const auto process_chain = [&effects, &signal_controls, &input_meter, &output_meter,
                                &dry_buffer, &wet_buffer, &output_buffer,
                                channels](std::span<const float> input) noexcept {
        input_meter.process(input, channels);
        signal_controls.process_input(input, dry_buffer, channels);
        std::copy(dry_buffer.begin(), dry_buffer.end(), wet_buffer.begin());
        for (const auto& effect : effects) {
            effect->process(wet_buffer, channels);
        }
        signal_controls.process_output(dry_buffer, wet_buffer, output_buffer, channels);
        output_meter.process(output_buffer, channels);
        const float telemetry = input_meter.peak() + input_meter.rms() +
                                static_cast<float>(input_meter.clip_flags()) +
                                static_cast<float>(input_meter.invalid_samples()) +
                                output_meter.peak() + output_meter.rms() +
                                static_cast<float>(output_meter.clip_flags()) +
                                static_cast<float>(output_meter.invalid_samples());
        if (telemetry < 0.0F) std::terminate();
    };
    for (std::size_t i = 0; i < warmup_blocks; ++i) {
        std::copy(source_buffer.begin(), source_buffer.end(), buffer.begin());
        process_chain(buffer);
    }
    for (const auto& effect : effects) {
        effect->reset();
    }
    std::vector<double> percentages;
    percentages.reserve(measured_blocks);
    const double deadline_us = static_cast<double>(block_frames) * 1'000'000.0 / sample_rate;
    for (std::size_t i = 0; i < measured_blocks; ++i) {
        std::copy(source_buffer.begin(), source_buffer.end(), buffer.begin());
        const auto start = Clock::now();
        process_chain(buffer);
        const auto end = Clock::now();
        const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        percentages.push_back(100.0 * elapsed_us / deadline_us);
    }
    std::size_t memory_bytes = 0;
    for (const auto& effect : effects) {
        memory_bytes += effect->memory_bytes();
    }
    memory_bytes += signal_controls.memory_bytes() + input_meter.memory_bytes() +
                    output_meter.memory_bytes();
    const Metrics signal = metrics(output_buffer);
    std::cout << "full-chain," << std::fixed << std::setprecision(4)
              << percentile(percentages, 0.50) << ","
              << percentile(percentages, 0.95) << ","
              << percentile(percentages, 0.99) << ","
              << percentile(percentages, 0.999) << ","
              << *std::max_element(percentages.begin(), percentages.end()) << ","
              << memory_bytes << "," << signal.rms << '\n';
}

void run_bench() {
    constexpr std::uint32_t sample_rate = 48000;
    constexpr std::uint16_t channels = 2;
    constexpr std::size_t block_frames = 128;
    constexpr std::size_t warmup_blocks = 2000;
    constexpr std::size_t measured_blocks = 5000;
    std::cout << "# build=" << TABLETFX_BUILD_TYPE << " sample_rate_hz=" << sample_rate
              << " block_frames=" << block_frames << " channels=" << channels
              << " warmup_blocks=" << warmup_blocks << " measured_blocks=" << measured_blocks
              << " metric=elapsed_block_us/callback_deadline_us*100\n";
    std::cout << "processor,median_pct,p95_pct,p99_pct,p99_9_pct,max_pct,owned_memory_bytes,output_rms\n";
    benchmark_one("input-gain", [=] {
        return std::make_unique<tabletfx::Gain>(6.0F, static_cast<float>(sample_rate));
    },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("output-gain", [=] {
        return std::make_unique<tabletfx::Gain>(-6.0F, static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("volume-pedal", [=] {
        tabletfx::VolumePedalParameters parameters;
        parameters.position = 0.72F;
        return std::make_unique<tabletfx::VolumePedal>(static_cast<float>(sample_rate), parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("high-pass", [=] {
        return std::make_unique<tabletfx::HighPassFilter>(sample_rate, channels, 80.0F);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("low-pass", [=] {
        return std::make_unique<tabletfx::LowPassFilter>(sample_rate, channels, 12000.0F);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("noise-gate", [=] { return std::make_unique<tabletfx::NoiseGate>(sample_rate, -48.0F); },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("soft-drive", [] { return std::make_unique<tabletfx::SoftDrive>(4.0F); },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay", [=] { return std::make_unique<tabletfx::Delay>(sample_rate, channels, 2000.0F); },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay-bbd-mono", [=] {
        auto delay = std::make_unique<tabletfx::Delay>(sample_rate, 1, 2000.0F);
        delay->configure_bbd_style(true);
        return delay;
    }, sample_rate, 1, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay-bbd-stereo", [=] {
        auto delay = std::make_unique<tabletfx::Delay>(sample_rate, 2, 2000.0F);
        delay->configure_bbd_style(true);
        return delay;
    }, sample_rate, 2, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay-ping-pong", [=] {
        auto delay = std::make_unique<tabletfx::Delay>(sample_rate, channels, 2000.0F);
        delay->configure_ping_pong(true);
        delay->reset();
        return delay;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay-dual-mono", [=] {
        auto delay = std::make_unique<tabletfx::Delay>(
            sample_rate, 1, 2000.0F, 350.0F, 0.25F, 0.25F, true);
        delay->configure_dual(350.0F, 0.25F, 0.25F,
                              525.0F, 0.25F, 0.25F);
        delay->reset();
        return delay;
    }, sample_rate, 1, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay-dual-stereo", [=] {
        auto delay = std::make_unique<tabletfx::Delay>(
            sample_rate, 2, 2000.0F, 350.0F, 0.25F, 0.25F, true);
        delay->configure_dual(350.0F, 0.25F, 0.25F,
                              525.0F, 0.25F, 0.25F);
        delay->reset();
        return delay;
    }, sample_rate, 2, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay-multitap-mono", [=] {
        auto delay = std::make_unique<tabletfx::Delay>(
            sample_rate, 1, 2000.0F, 350.0F, 0.25F, 0.25F);
        delay->configure_multitap_tap(0, 250.0F, 0.65F, -0.55F);
        delay->configure_multitap_tap(1, 500.0F, 0.45F, 0.55F);
        delay->configure_multitap_tap(2, 750.0F, 0.30F, -0.25F);
        delay->configure_multitap_tap(3, 1000.0F, 0.20F, 0.25F);
        delay->configure_multitap(true, 0.25F, 0.25F);
        delay->reset();
        return delay;
    }, sample_rate, 1, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay-multitap-stereo", [=] {
        auto delay = std::make_unique<tabletfx::Delay>(
            sample_rate, 2, 2000.0F, 350.0F, 0.25F, 0.25F);
        delay->configure_multitap_tap(0, 250.0F, 0.65F, -0.55F);
        delay->configure_multitap_tap(1, 500.0F, 0.45F, 0.55F);
        delay->configure_multitap_tap(2, 750.0F, 0.30F, -0.25F);
        delay->configure_multitap_tap(3, 1000.0F, 0.20F, 0.25F);
        delay->configure_multitap(true, 0.25F, 0.25F);
        delay->reset();
        return delay;
    }, sample_rate, 2, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("poly-pitch-shifter", [=] {
        auto effect = std::make_unique<tabletfx::PolyPitchShifter>(
            static_cast<float>(sample_rate), channels);
        effect->set_pitch_semitones(5.0F);
        return effect;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("poly-pitch-shifter-mono", [=] {
        auto effect = std::make_unique<tabletfx::PolyPitchShifter>(
            static_cast<float>(sample_rate), 1);
        effect->set_pitch_semitones(5.0F);
        return effect;
    }, sample_rate, 1, block_frames, warmup_blocks, measured_blocks);
    tabletfx::CompressorParameters compressor_parameters;
    compressor_parameters.threshold_db = -24.0F;
    compressor_parameters.ratio = 4.0F;
    benchmark_one("compressor", [=] {
        return std::make_unique<tabletfx::Compressor>(sample_rate, compressor_parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("pedal-compressor", [=] {
        return std::make_unique<tabletfx::PedalCompressor>(sample_rate);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("three-band-parametric-eq", [=] {
        auto effect = std::make_unique<tabletfx::ThreeBandParametricEQ>(sample_rate, channels);
        effect->configure(120.0F, 3.0F, 0.707F, 800.0F, -2.0F, 0.8F,
                          5000.0F, 4.0F, 0.707F);
        return effect;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("peak-limiter", [=] {
        return std::make_unique<tabletfx::PeakLimiter>(static_cast<float>(sample_rate), -1.0F, 80.0F);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("wah", [=] {
        tabletfx::WahParameters parameters;
        parameters.position = 0.7F;
        return std::make_unique<tabletfx::Wah>(static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("envelope-wah", [=] {
        tabletfx::EnvelopeWahParameters parameters;
        parameters.sensitivity = 0.8F;
        return std::make_unique<tabletfx::EnvelopeWah>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("clean-boost", [=] {
        return std::make_unique<tabletfx::CleanBoost>(6.0F, static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("transparent-overdrive", [=] {
        return std::make_unique<tabletfx::TransparentOverdrive>(static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("mid-focused-overdrive", [=] {
        return std::make_unique<tabletfx::MidFocusedOverdrive>(static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("high-gain-distortion", [=] {
        return std::make_unique<tabletfx::HighGainDistortion>(static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("round-fuzz", [=] {
        return std::make_unique<tabletfx::RoundFuzz>(static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_signal_controls(sample_rate, channels, block_frames,
                              warmup_blocks, measured_blocks);
    benchmark_level_meters(sample_rate, channels, block_frames,
                           warmup_blocks, measured_blocks);
    benchmark_channel_conversion(sample_rate, block_frames,
                                 warmup_blocks, measured_blocks);
    benchmark_parallel_routing(sample_rate, channels, block_frames,
                               warmup_blocks, measured_blocks);
    benchmark_chromatic_tuner(sample_rate, channels, block_frames,
                              warmup_blocks, measured_blocks);
    benchmark_one("tremolo", [=] {
        tabletfx::TremoloParameters parameters;
        parameters.rate_hz = 5.0F;
        parameters.depth = 0.5F;
        return std::make_unique<tabletfx::Tremolo>(static_cast<float>(sample_rate), parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("chorus", [=] {
        tabletfx::ChorusParameters parameters;
        parameters.rate_hz = 0.8F;
        parameters.base_delay_ms = 15.0F;
        parameters.depth_ms = 7.0F;
        parameters.mix = 0.3F;
        return std::make_unique<tabletfx::Chorus>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("flanger", [=] {
        tabletfx::FlangerParameters parameters;
        parameters.rate_hz = 0.25F;
        parameters.base_delay_ms = 2.0F;
        parameters.depth_ms = 1.0F;
        parameters.feedback = 0.35F;
        parameters.mix = 0.35F;
        return std::make_unique<tabletfx::Flanger>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("phaser", [=] {
        tabletfx::PhaserParameters parameters;
        parameters.feedback = 0.15F;
        return std::make_unique<tabletfx::Phaser>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("vibrato", [=] {
        tabletfx::VibratoParameters parameters;
        return std::make_unique<tabletfx::Vibrato>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_full_chain(sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
}

void print_usage() {
    std::cerr << "Usage:\n"
              << "  tabletfx_offline render INPUT.wav OUTPUT.wav [--gain-db dB] [--output-gain-db dB]"
                 " [--output-channels preserve|mono|stereo] [--mono-sum average|equal-power]"
                 " [--volume-position 0..1] [--volume-min-db dB] [--volume-max-db dB]"
                 " [--volume-curve linear|audio] [--limiter-ceiling-db dB] [--limiter-release-ms ms]"
                 " [--wah-position 0..1] [--wah-min-hz Hz] [--wah-max-hz Hz] [--wah-q Q] [--wah-level-db dB]"
                  " [--touch-wah-sensitivity 0..1] [--touch-wah-direction up|down]"
                  " [--touch-wah-min-hz Hz] [--touch-wah-max-hz Hz] [--touch-wah-q Q]"
                  " [--touch-wah-level-db dB] [--touch-wah-attack-ms ms]"
                  " [--touch-wah-release-ms ms] [--touch-wah-smoothing-ms ms]"
                 " [--tremolo-rate-hz Hz] [--tremolo-depth 0..1]"
                 " [--tremolo-waveform sine|triangle|square] [--tremolo-sync on|off]"
                 " [--tremolo-tempo-bpm BPM] [--tremolo-beats-per-cycle beats]"
                  " [--chorus-rate-hz Hz] [--chorus-delay-ms ms]"
                  " [--chorus-depth-ms ms] [--chorus-mix 0..1]"
                  " [--flanger-rate-hz Hz] [--flanger-delay-ms ms]"
                  " [--flanger-depth-ms ms] [--flanger-feedback 0..0.9]"
                  " [--flanger-feedback-polarity positive|negative] [--flanger-mix 0..1]"
                   " [--phaser-rate-hz Hz] [--phaser-min-hz Hz] [--phaser-max-hz Hz]"
                   " [--phaser-feedback -0.7..0.7] [--phaser-stages 4|6|8] [--phaser-mix 0..1]"
                   " [--vibrato-rate-hz 0.1..10] [--vibrato-depth-ms 0..1.5]"
                   " [--vibrato-mix 0..1]"
                    " [--overdrive-drive 0..0.5] [--overdrive-level-db -24..12]"
                    " [--overdrive-mix 0..1]"
                 " [--mid-overdrive-drive 0..0.5] [--mid-overdrive-boost-db 0..12]"
                    " [--mid-overdrive-level-db -24..12] [--mid-overdrive-mix 0..1]"
                  " [--high-gain-drive 0..1] [--high-gain-pre-low-cut-hz 20..1000]"
                  " [--high-gain-pre-high-cut-hz 1000..18000]"
                  " [--high-gain-post-tone-hz 500..18000]"
                  " [--high-gain-level-db -24..12] [--high-gain-mix 0..1]"
                  " [--round-fuzz 0..1] [--round-fuzz-level-db -24..12]"
                  " [--round-fuzz-mix 0..1]"
                 " [--boost-db 0..24]"
                 " [--high-pass-hz Hz]"
                 " [--low-pass-hz Hz] [--gate-threshold-db dB] [--gate-attenuation-db dB]"
                 " [--gate-hysteresis-db dB] [--gate-attack-ms ms] [--gate-hold-ms ms]"
                 " [--gate-release-ms ms]"
                   " [--drive 1..20] [--delay-preset digital|slapback|bbd] [--delay-bbd] [--delay-ms ms] [--delay-sync-bpm 20..300]"
                   " [--delay-ping-pong]"
                    " [--delay-dual] [--delay-dual-a-ms ms] [--delay-dual-a-feedback 0..0.95]"
                    " [--delay-dual-a-mix 0..1] [--delay-dual-b-ms ms]"
                    " [--delay-dual-b-feedback 0..0.95] [--delay-dual-b-mix 0..1]"
                   " [--delay-division whole|dotted-half|half|dotted-quarter|quarter|quarter-triplet|dotted-eighth|eighth|eighth-triplet|dotted-sixteenth|sixteenth|sixteenth-triplet]"
                  " [--delay-feedback 0..0.95] [--delay-mix 0..1]"
                 " [--pitch-semitones -12..12]\n"
              << "                 [--pedal-compressor-sensitivity 0..1] [--pedal-compressor-sustain 0..1]"
                 " [--pedal-compressor-attack 0..1] [--pedal-compressor-level-db -12..12]\n"
              << "                 [--compressor-threshold-db dB] [--compressor-ratio 1..20]"
                 " [--compressor-knee-db dB] [--compressor-attack-ms ms]"
                 " [--compressor-release-ms ms] [--compressor-makeup-db dB]\n"
              << "                 [--eq-bandN-frequency-hz Hz] [--eq-bandN-gain-db dB] [--eq-bandN-q Q] (N=1..3)\n"
              << "  tabletfx_offline --bench\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--bench") {
            run_bench();
            return 0;
        }
        if (argc < 4 || std::string_view(argv[1]) != "render") {
            print_usage();
            return 2;
        }
        const Options options = parse_options(argc, argv, 4);
        tabletfx::AudioBuffer input = tabletfx::read_wav(argv[2]);
        const std::uint16_t input_channels = input.channels;
        const Metrics input_metrics = metrics(input.interleaved);
        if (options.output_channels != 0 && options.output_channels != input.channels) {
            const std::size_t frames = input.frames();
            std::vector<float> converted(frames * options.output_channels);
            const auto result = tabletfx::convert_channels(
                input.interleaved, input.channels, converted, options.output_channels,
                options.mono_sum_policy);
            if (!result.succeeded() || result.frames != frames) {
                throw std::runtime_error("Could not convert WAV channel layout");
            }
            input.interleaved = std::move(converted);
            input.channels = options.output_channels;
        }
        auto effects = make_effects(options, input.sample_rate, input.channels);
        process_in_blocks(input, effects, 256);
        const Metrics output_metrics = metrics(input.interleaved);
        tabletfx::write_wav_float32(argv[3], input);
        std::cout << "rendered frames=" << input.frames() << " sample_rate_hz=" << input.sample_rate
                  << " input_channels=" << input_channels << " channels=" << input.channels
                  << " processors=" << effects.size()
                  << " input_peak=" << input_metrics.peak << " input_rms=" << input_metrics.rms
                  << " output_peak=" << output_metrics.peak << " output_rms=" << output_metrics.rms
                  << " output=" << argv[3] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "tabletfx_offline: " << error.what() << '\n';
        return 1;
    }
}
