#include "tabletfx/effects.hpp"
#include "tabletfx/channel_conversion.hpp"
#include "tabletfx/parallel_mixer.hpp"
#include "tabletfx/tuner.hpp"
#include "tabletfx/effects/clean_boost.hpp"
#include "tabletfx/effects/chorus.hpp"
#include "tabletfx/effects/dimension_chorus.hpp"
#include "tabletfx/effects/compressor.hpp"
#include "tabletfx/effects/envelope_wah.hpp"
#include "tabletfx/effects/flanger.hpp"
#include "tabletfx/effects/filters.hpp"
#include "tabletfx/effects/hall_reverb.hpp"
#include "tabletfx/effects/plate_reverb.hpp"
#include "tabletfx/effects/spring_reverb.hpp"
#include "tabletfx/effects/convolution_reverb.hpp"
#include "tabletfx/effects/auto_swell.hpp"
#include "tabletfx/effects/bit_crusher.hpp"
#include "tabletfx/effects/rotary_speaker.hpp"
#include "tabletfx/effects/ring_modulator.hpp"
#include "tabletfx/effects/rhythmic_slicer.hpp"
#include "tabletfx/effects/step_filter.hpp"
#include "tabletfx/effects/notch_filter.hpp"
#include "tabletfx/effects/formant_filter.hpp"
#include "tabletfx/effects/harmonic_resonator.hpp"
#include "tabletfx/effects/multiband_compressor.hpp"
#include "tabletfx/effects/reverse_delay.hpp"
#include "tabletfx/effects/ducking_delay.hpp"
#include "tabletfx/effects/modulated_delay.hpp"
#include "tabletfx/effects/optical_compressor.hpp"
#include "tabletfx/effects/fet_compressor.hpp"
#include "tabletfx/effects/five_band_eq.hpp"
#include "tabletfx/effects/guitar_graphic_eq.hpp"
#include "tabletfx/effects/bass_graphic_eq.hpp"
#include "tabletfx/effects/shelf_tilt_eq.hpp"
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
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <numbers>
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
    bool dimension_chorus_enabled = false;
    tabletfx::DimensionChorusParameters dimension_chorus{};
    bool triple_double_chorus_enabled = false;
    tabletfx::effects::TripleDoubleChorusConfig triple_double_chorus{};
    bool four_band_resonator_enabled = false;
    tabletfx::FourBandResonatorParameters four_band_resonator{};
    bool diatonic_harmonizer_enabled = false;
    tabletfx::effects::DiatonicHarmonizerConfig diatonic_harmonizer{};
    bool pitch_arpeggiator_enabled = false;
    bool pitch_arpeggiator_run_explicit = false;
    tabletfx::effects::PitchArpeggiatorParameters pitch_arpeggiator{};
    bool mono_guitar_synth_enabled = false;
    tabletfx::effects::MonoGuitarSynthConfig mono_guitar_synth{};
    bool note_generator_enabled = false;
    tabletfx::effects::NoteGeneratorParameters note_generator{};
    bool enhancer_enabled = false;
    tabletfx::EnhancerParameters enhancer{};
    bool pitch_feedback_delay_enabled = false;
    tabletfx::PitchFeedbackDelayParameters pitch_feedback_delay{};
    bool feedback_generator_enabled = false;
    tabletfx::effects::FeedbackGeneratorParameters feedback_generator{};
    bool shimmer_reverb_enabled = false;
    tabletfx::ShimmerReverbParameters shimmer_reverb{};
    bool convolution_reverb_enabled = false;
    std::string convolution_ir_path;
    tabletfx::ConvolutionReverbParameters convolution_reverb{};
    bool grain_delay_enabled = false;
    tabletfx::GrainDelayParameters grain_delay{};
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
    bool delay_tape_enabled = false;
    float delay_tape_saturation = 0.45F;
    float delay_tape_wow_depth_ms = 0.35F;
    float delay_tape_wow_rate_hz = 0.3F;
    float delay_tape_flutter_depth_ms = 0.08F;
    float delay_tape_flutter_rate_hz = 6.0F;
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
    bool delay_tape_heads_enabled = false;
    float delay_tape_head_spacing_ms = 110.0F;
    std::uint8_t delay_tape_head_mask = 0x0FU;
    bool pitch_enabled = false;
    float pitch_semitones = 0.0F;
    bool compressor_enabled = false;
    tabletfx::CompressorParameters compressor{};
    bool optical_compressor_enabled = false;
    tabletfx::OpticalCompressorParameters optical_compressor{};
    bool fet_compressor_enabled = false;
    tabletfx::FetCompressorParameters fet_compressor{};
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
    bool five_band_eq_enabled = false;
    std::array<tabletfx::FiveBandEQBand, 5> five_band_eq{{
        {100.0F, 0.0F, 0.707F}, {350.0F, 0.0F, 0.707F},
        {1000.0F, 0.0F, 0.707F}, {3500.0F, 0.0F, 0.707F},
        {8000.0F, 0.0F, 0.707F}}};
    bool guitar_graphic_eq_enabled = false;
    tabletfx::GuitarGraphicEQLayout guitar_graphic_eq_layout =
        tabletfx::GuitarGraphicEQLayout::SevenBand;
    std::array<float, tabletfx::GuitarGraphicEQ::kMaximumBands> guitar_graphic_eq_gains{};
    bool bass_graphic_eq_enabled = false;
    tabletfx::BassGraphicEQLayout bass_graphic_eq_layout =
        tabletfx::BassGraphicEQLayout::SevenBand;
    std::array<float, tabletfx::BassGraphicEQ::kMaximumBands> bass_graphic_eq_gains{};
    bool shelf_tilt_eq_enabled = false;
    tabletfx::ShelfTiltMode shelf_tilt_eq_mode = tabletfx::ShelfTiltMode::LowShelf;
    float shelf_tilt_eq_frequency_hz = 200.0F;
    float shelf_tilt_eq_gain_db = 0.0F;
    bool room_reverb_enabled = false;
    tabletfx::RoomReverbParameters room_reverb{};
    bool hall_reverb_enabled = false;
    tabletfx::HallReverbParameters hall_reverb{};
    bool plate_reverb_enabled = false;
    tabletfx::PlateReverbParameters plate_reverb{};
    bool spring_reverb_enabled = false;
    tabletfx::SpringReverbParameters spring_reverb{};
    bool auto_swell_enabled = false;
    tabletfx::AutoSwellParameters auto_swell{};
    bool rotary_speaker_enabled = false;
    tabletfx::RotarySpeakerParameters rotary_speaker{};
    bool ring_modulator_enabled = false;
    tabletfx::RingModulatorParameters ring_modulator{};
    bool bit_crusher_enabled = false;
    tabletfx::BitCrusherParameters bit_crusher{};
    bool rhythmic_slicer_enabled = false;
    tabletfx::RhythmicSlicerParameters rhythmic_slicer{};
    bool step_filter_enabled = false;
    tabletfx::StepFilterParameters step_filter{};
    bool notch_filter_enabled = false;
    tabletfx::NotchFilterParameters notch_filter{};
    bool formant_filter_enabled = false;
    tabletfx::FormantFilterParameters formant_filter{};
    bool harmonic_resonator_enabled = false;
    tabletfx::HarmonicResonatorParameters harmonic_resonator{};
    bool multiband_compressor_enabled = false;
    tabletfx::MultibandCompressorParameters multiband_compressor{};
    bool reverse_delay_enabled = false;
    tabletfx::ReverseDelayParameters reverse_delay{};
    bool reverse_delay_running = true;
    bool ducking_delay_enabled = false;
    tabletfx::DuckingDelayParameters ducking_delay{};
    bool modulated_delay_enabled = false;
    tabletfx::ModulatedDelayParameters modulated_delay{};
    bool filtered_delay_enabled = false;
    tabletfx::FilteredDelayParameters filtered_delay{};
    bool resonant_delay_enabled = false;
    tabletfx::ResonantDelayParameters resonant_delay{};
    bool sweep_delay_enabled = false;
    tabletfx::SweepDelayParameters sweep_delay{};
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

template <std::size_t Count>
std::array<float, Count> parse_float_array(std::string_view name,
                                          std::string_view text) {
    std::array<float, Count> values{};
    std::size_t start = 0;
    for (std::size_t index = 0; index < Count; ++index) {
        const std::size_t separator = text.find(',', start);
        if ((index + 1U < Count && separator == std::string_view::npos) ||
            (index + 1U == Count && separator != std::string_view::npos)) {
            throw std::invalid_argument(std::string(name) +
                                        " requires exactly " +
                                        std::to_string(Count) +
                                        " comma-separated values");
        }
        const std::string_view item = text.substr(
            start, separator == std::string_view::npos ? text.size() - start
                                                       : separator - start);
        const std::string item_string(item);
        values[index] = parse_float(name, item_string.c_str());
        if (separator != std::string_view::npos) {
            start = separator + 1U;
        }
    }
    return values;
}

bool set_five_band_eq_parameter(std::string_view key, Options& options,
                                float value) {
    constexpr std::array<std::string_view, 3> suffixes{
        "frequency-hz", "gain-db", "q"};
    for (std::size_t band = 0; band < options.five_band_eq.size(); ++band) {
        const std::string prefix = "--five-band-eq-band" +
                                   std::to_string(band + 1U) + "-";
        for (std::size_t parameter = 0; parameter < suffixes.size(); ++parameter) {
            if (key != prefix + std::string(suffixes[parameter])) continue;
            auto& selected = options.five_band_eq[band];
            if (parameter == 0) selected.frequency_hz = value;
            else if (parameter == 1) selected.gain_db = value;
            else selected.q = value;
            options.five_band_eq_enabled = true;
            return true;
        }
    }
    return false;
}

std::array<tabletfx::FourBandResonatorFilterLocation, 4>
parse_four_band_resonator_filter_locations(std::string_view text) {
    std::array<tabletfx::FourBandResonatorFilterLocation, 4> locations{};
    std::size_t start = 0;
    for (std::size_t index = 0; index < locations.size(); ++index) {
        const std::size_t separator = text.find(',', start);
        if ((index + 1U < locations.size() && separator == std::string_view::npos) ||
            (index + 1U == locations.size() && separator != std::string_view::npos)) {
            throw std::invalid_argument(
                "Four-band resonator filter locations require four comma-separated pre/post values");
        }
        const std::string_view item = text.substr(
            start, separator == std::string_view::npos ? text.size() - start
                                                       : separator - start);
        if (item == "pre") {
            locations[index] = tabletfx::FourBandResonatorFilterLocation::Pre;
        } else if (item == "post") {
            locations[index] = tabletfx::FourBandResonatorFilterLocation::Post;
        } else {
            throw std::invalid_argument(
                "Four-band resonator filter locations must be pre or post");
        }
        if (separator != std::string_view::npos) {
            start = separator + 1U;
        }
    }
    return locations;
}

tabletfx::FourBandResonatorChord parse_four_band_resonator_chord(
    std::string_view value) {
    using Chord = tabletfx::FourBandResonatorChord;
    if (value == "major") return Chord::Major;
    if (value == "major6") return Chord::Major6;
    if (value == "dominant7") return Chord::Dominant7;
    if (value == "major7") return Chord::Major7;
    if (value == "minor") return Chord::Minor;
    if (value == "minor6") return Chord::Minor6;
    if (value == "minor7") return Chord::Minor7;
    if (value == "diminished") return Chord::Diminished;
    if (value == "augmented") return Chord::Augmented;
    throw std::invalid_argument(
        "Four-band resonator chord must be major, major6, dominant7, major7, minor, minor6, minor7, diminished, or augmented");
}

std::uint32_t parse_unsigned_integer(std::string_view name, const char* raw) {
    const std::string_view text(raw);
    std::uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        throw std::invalid_argument("Invalid integer argument for " +
                                    std::string(name) + ": " + std::string(text));
    }
    return value;
}

tabletfx::RhythmicDivision parse_rhythmic_division(std::string_view value) {
    using tabletfx::RhythmicDivision;
    if (value == "whole") return RhythmicDivision::Whole;
    if (value == "half") return RhythmicDivision::Half;
    if (value == "quarter") return RhythmicDivision::Quarter;
    if (value == "dotted-quarter") return RhythmicDivision::DottedQuarter;
    if (value == "quarter-triplet") return RhythmicDivision::QuarterTriplet;
    if (value == "eighth") return RhythmicDivision::Eighth;
    if (value == "dotted-eighth") return RhythmicDivision::DottedEighth;
    if (value == "eighth-triplet") return RhythmicDivision::EighthTriplet;
    if (value == "sixteenth") return RhythmicDivision::Sixteenth;
    if (value == "dotted-sixteenth") return RhythmicDivision::DottedSixteenth;
    if (value == "sixteenth-triplet") return RhythmicDivision::SixteenthTriplet;
    if (value == "thirty-second") return RhythmicDivision::ThirtySecond;
    if (value == "thirty-second-triplet") return RhythmicDivision::ThirtySecondTriplet;
    throw std::invalid_argument("Unsupported rhythmic subdivision");
}

tabletfx::StepFilterMode parse_step_filter_mode(std::string_view value) {
    using tabletfx::StepFilterMode;
    if (value == "lowpass") return StepFilterMode::LowPass;
    if (value == "bandpass") return StepFilterMode::BandPass;
    if (value == "highpass") return StepFilterMode::HighPass;
    if (value == "notch") return StepFilterMode::Notch;
    throw std::invalid_argument("Step-filter mode must be lowpass, bandpass, highpass, or notch");
}

std::array<float, 16> parse_step_pattern(std::string_view value) {
    std::array<float, 16> steps{};
    std::size_t start = 0;
    for (std::size_t index = 0; index < steps.size(); ++index) {
        const std::size_t separator = value.find(',', start);
        if ((index + 1U < steps.size() && separator == std::string_view::npos) ||
            (index + 1U == steps.size() && separator != std::string_view::npos)) {
            throw std::invalid_argument("Step pattern requires exactly 16 comma-separated values");
        }
        const std::string_view item = value.substr(
            start, separator == std::string_view::npos ? value.size() - start
                                                       : separator - start);
        const std::string text(item);
        std::size_t parsed = 0;
        const float step = std::stof(text, &parsed);
        if (parsed != text.size() || !std::isfinite(step)) {
            throw std::invalid_argument("Step pattern values must be finite numbers");
        }
        steps[index] = step;
        if (separator != std::string_view::npos) {
            start = separator + 1U;
        }
    }
    return steps;
}

std::uint8_t parse_delay_head_mask(std::string_view value) {
    if (value == "all") return 0x0FU;
    if (value == "none") return 0U;
    std::uint8_t mask = 0U;
    std::size_t start = 0;
    while (start < value.size()) {
        const std::size_t separator = value.find(',', start);
        const std::string_view item = value.substr(
            start, separator == std::string_view::npos ? value.size() - start
                                                       : separator - start);
        if (item.size() != 1 || item[0] < '1' || item[0] > '4') {
            throw std::invalid_argument("Delay heads must be all, none, or a comma-separated list from 1 to 4");
        }
        const unsigned int head = static_cast<unsigned int>(item[0] - '1');
        mask = static_cast<std::uint8_t>(mask | static_cast<std::uint8_t>(1U << head));
        if (separator == std::string_view::npos) break;
        start = separator + 1;
    }
    if (value.empty() || value.back() == ',') {
        throw std::invalid_argument("Delay heads must be all, none, or a comma-separated list from 1 to 4");
    }
    return mask;
}

bool set_graphic_eq_band_gain(std::string_view key, std::string_view prefix,
                              std::array<float, 7>& gains, bool& enabled,
                              float value) {
    constexpr std::string_view suffix = "-gain-db";
    if (!key.starts_with(prefix) || !key.ends_with(suffix)) {
        return false;
    }
    const std::string_view band_text = key.substr(
        prefix.size(), key.size() - prefix.size() - suffix.size());
    if (band_text.size() != 1 || band_text.front() < '1' || band_text.front() > '7') {
        throw std::invalid_argument("Graphic EQ band gain options must use a band number from 1 to 7");
    }
    const std::size_t band = static_cast<std::size_t>(band_text.front() - '1');
    gains[band] = value;
    enabled = true;
    return true;
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

tabletfx::DimensionChorusMode parse_dimension_chorus_mode(std::string_view value) {
    if (value == "1") return tabletfx::DimensionChorusMode::Mode1;
    if (value == "2") return tabletfx::DimensionChorusMode::Mode2;
    if (value == "3") return tabletfx::DimensionChorusMode::Mode3;
    if (value == "4") return tabletfx::DimensionChorusMode::Mode4;
    if (value == "1+2") return tabletfx::DimensionChorusMode::Modes12;
    if (value == "1+3") return tabletfx::DimensionChorusMode::Modes13;
    if (value == "1+4") return tabletfx::DimensionChorusMode::Modes14;
    if (value == "2+3") return tabletfx::DimensionChorusMode::Modes23;
    if (value == "2+4") return tabletfx::DimensionChorusMode::Modes24;
    if (value == "3+4") return tabletfx::DimensionChorusMode::Modes34;
    throw std::invalid_argument(
        "Dimension Chorus Mode must be 1, 2, 3, 4, 1+2, 1+3, 1+4, 2+3, 2+4, or 3+4");
}

Options parse_options(int argc, char** argv, int first) {
    Options options;
    for (int i = first; i < argc; ++i) {
        const std::string_view key(argv[i]);
        if (key == "--delay-multitap") {
            options.delay_multitap_enabled = true;
            options.delay_tape_heads_enabled = false;
            continue;
        }
        if (key == "--delay-tape-heads") {
            options.delay_tape_heads_enabled = true;
            options.delay_multitap_enabled = false;
            continue;
        }
        if (key == "--delay-bbd") {
            options.delay_bbd_style = true;
            options.delay_tape_enabled = false;
            continue;
        }
        if (key == "--delay-tape") {
            options.delay_tape_enabled = true;
            options.delay_bbd_style = false;
            continue;
        }
        if (key == "--optical-compressor") {
            options.optical_compressor_enabled = true;
            continue;
        }
        if (key == "--fet-compressor") {
            options.fet_compressor_enabled = true;
            continue;
        }
        if (key == "--guitar-graphic-eq") {
            options.guitar_graphic_eq_enabled = true;
            continue;
        }
        if (key == "--bass-graphic-eq") {
            options.bass_graphic_eq_enabled = true;
            continue;
        }
        if (key == "--shelf-tilt-eq") {
            options.shelf_tilt_eq_enabled = true;
            continue;
        }
        if (key == "--room-reverb") {
            options.room_reverb_enabled = true;
            continue;
        }
        if (key == "--hall-reverb") {
            options.hall_reverb_enabled = true;
            continue;
        }
        if (key == "--plate-reverb") {
            options.plate_reverb_enabled = true;
            continue;
        }
        if (key == "--spring-reverb") {
            options.spring_reverb_enabled = true;
            continue;
        }
        if (key == "--auto-swell") {
            options.auto_swell_enabled = true;
            continue;
        }
        if (key == "--rotary-speaker") {
            options.rotary_speaker_enabled = true;
            continue;
        }
        if (key == "--ring-mod") {
            options.ring_modulator_enabled = true;
            continue;
        }
        if (key == "--bit-crusher") {
            options.bit_crusher_enabled = true;
            continue;
        }
        if (key == "--dimension-chorus") {
            options.dimension_chorus_enabled = true;
            continue;
        }
        if (key == "--triple-double-chorus") {
            options.triple_double_chorus_enabled = true;
            continue;
        }
        if (key == "--four-band-resonator") {
            options.four_band_resonator_enabled = true;
            continue;
        }
        if (key == "--diatonic-harmonizer") {
            options.diatonic_harmonizer_enabled = true;
            continue;
        }
        if (key == "--pitch-arpeggiator") {
            options.pitch_arpeggiator_enabled = true;
            continue;
        }
        if (key == "--mono-guitar-synth") {
            options.mono_guitar_synth_enabled = true;
            continue;
        }
        if (key == "--note-generator") {
            options.note_generator_enabled = true;
            continue;
        }
        if (key == "--enhancer") {
            options.enhancer_enabled = true;
            continue;
        }
        if (key == "--pitch-feedback-delay") {
            options.pitch_feedback_delay_enabled = true;
            continue;
        }
        if (key == "--feedback-generator") {
            options.feedback_generator_enabled = true;
            continue;
        }
        if (key == "--shimmer-reverb") {
            options.shimmer_reverb_enabled = true;
            continue;
        }
        if (key == "--convolution-reverb") {
            options.convolution_reverb_enabled = true;
            continue;
        }
        if (key == "--convolution-ir") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --convolution-ir");
            }
            options.convolution_ir_path = argv[++i];
            options.convolution_reverb_enabled = true;
            continue;
        }
        if (key.starts_with("--convolution-reverb-")) {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const float value = parse_float(key, argv[++i]);
            const std::string_view setting = key.substr(
                std::string_view("--convolution-reverb-").size());
            if (setting == "mix") options.convolution_reverb.mix = value;
            else if (setting == "level-db") options.convolution_reverb.level_db = value;
            else throw std::invalid_argument("Unknown option: " + std::string(key));
            options.convolution_reverb_enabled = true;
            continue;
        }
        if (key == "--grain-delay") {
            options.grain_delay_enabled = true;
            continue;
        }
        if (key.starts_with("--diatonic-harmonizer-")) {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const std::string_view value(argv[++i]);
            auto& parameters = options.diatonic_harmonizer;
            const std::string_view setting = key.substr(
                std::string_view("--diatonic-harmonizer-").size());
            if (setting == "mix") {
                parameters.mix = parse_float(key, argv[i]);
            } else if (setting == "interval") {
                parameters.pitch_interval = static_cast<int>(
                    std::lround(parse_float(key, argv[i])));
            } else if (setting == "key") {
                parameters.key = static_cast<int>(
                    std::lround(parse_float(key, argv[i])));
            } else if (setting == "scale") {
                if (value == "major") parameters.scale = tabletfx::effects::DiatonicScale::Major;
                else if (value == "natural-minor") parameters.scale = tabletfx::effects::DiatonicScale::NaturalMinor;
                else if (value == "dorian") parameters.scale = tabletfx::effects::DiatonicScale::Dorian;
                else if (value == "phrygian") parameters.scale = tabletfx::effects::DiatonicScale::Phrygian;
                else if (value == "lydian") parameters.scale = tabletfx::effects::DiatonicScale::Lydian;
                else if (value == "mixolydian") parameters.scale = tabletfx::effects::DiatonicScale::Mixolydian;
                else if (value == "locrian") parameters.scale = tabletfx::effects::DiatonicScale::Locrian;
                else if (value == "harmonic-minor") parameters.scale = tabletfx::effects::DiatonicScale::HarmonicMinor;
                else if (value == "melodic-minor") parameters.scale = tabletfx::effects::DiatonicScale::MelodicMinor;
                else if (value == "major-pentatonic") parameters.scale = tabletfx::effects::DiatonicScale::MajorPentatonic;
                else if (value == "minor-pentatonic") parameters.scale = tabletfx::effects::DiatonicScale::MinorPentatonic;
                else if (value == "blues") parameters.scale = tabletfx::effects::DiatonicScale::Blues;
                else if (value == "chromatic") parameters.scale = tabletfx::effects::DiatonicScale::Chromatic;
                else throw std::invalid_argument("Unknown diatonic harmonizer scale");
            } else if (setting == "tone-hz") {
                parameters.tone_hz = parse_float(key, argv[i]);
            } else if (setting == "delay-ms") {
                parameters.delay_ms = parse_float(key, argv[i]);
            } else if (setting == "feedback") {
                parameters.feedback = parse_float(key, argv[i]);
            } else if (setting == "tap-division") {
                if (value == "free") parameters.tap_division = tabletfx::effects::TapDivision::Free;
                else if (value == "half") parameters.tap_division = tabletfx::effects::TapDivision::Half;
                else if (value == "quarter") parameters.tap_division = tabletfx::effects::TapDivision::Quarter;
                else if (value == "dotted-quarter") parameters.tap_division = tabletfx::effects::TapDivision::DottedQuarter;
                else if (value == "quarter-triplet") parameters.tap_division = tabletfx::effects::TapDivision::QuarterTriplet;
                else if (value == "eighth") parameters.tap_division = tabletfx::effects::TapDivision::Eighth;
                else if (value == "dotted-eighth") parameters.tap_division = tabletfx::effects::TapDivision::DottedEighth;
                else if (value == "eighth-triplet") parameters.tap_division = tabletfx::effects::TapDivision::EighthTriplet;
                else if (value == "sixteenth") parameters.tap_division = tabletfx::effects::TapDivision::Sixteenth;
                else throw std::invalid_argument("Unknown diatonic harmonizer tap division");
            } else if (setting == "tempo-bpm") {
                parameters.tempo_bpm = parse_float(key, argv[i]);
            } else {
                throw std::invalid_argument("Unknown option: " + std::string(key));
            }
            options.diatonic_harmonizer_enabled = true;
            continue;
        }
        if (key.starts_with("--pitch-arpeggiator-")) {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const std::string_view value(argv[++i]);
            auto& parameters = options.pitch_arpeggiator;
            const std::string_view setting = key.substr(
                std::string_view("--pitch-arpeggiator-").size());
            if (setting == "mix") {
                parameters.mix = parse_float(key, argv[i]);
            } else if (setting == "output-level") {
                parameters.output_level = parse_float(key, argv[i]);
            } else if (setting == "key") {
                parameters.key = static_cast<int>(std::lround(std::clamp(
                    parse_float(key, argv[i]), 0.0F, 11.0F)));
            } else if (setting == "scale") {
                if (value == "major") parameters.scale = tabletfx::effects::DiatonicScale::Major;
                else if (value == "natural-minor") parameters.scale = tabletfx::effects::DiatonicScale::NaturalMinor;
                else if (value == "dorian") parameters.scale = tabletfx::effects::DiatonicScale::Dorian;
                else if (value == "phrygian") parameters.scale = tabletfx::effects::DiatonicScale::Phrygian;
                else if (value == "lydian") parameters.scale = tabletfx::effects::DiatonicScale::Lydian;
                else if (value == "mixolydian") parameters.scale = tabletfx::effects::DiatonicScale::Mixolydian;
                else if (value == "locrian") parameters.scale = tabletfx::effects::DiatonicScale::Locrian;
                else if (value == "harmonic-minor") parameters.scale = tabletfx::effects::DiatonicScale::HarmonicMinor;
                else if (value == "melodic-minor") parameters.scale = tabletfx::effects::DiatonicScale::MelodicMinor;
                else if (value == "major-pentatonic") parameters.scale = tabletfx::effects::DiatonicScale::MajorPentatonic;
                else if (value == "minor-pentatonic") parameters.scale = tabletfx::effects::DiatonicScale::MinorPentatonic;
                else if (value == "blues") parameters.scale = tabletfx::effects::DiatonicScale::Blues;
                else if (value == "chromatic") parameters.scale = tabletfx::effects::DiatonicScale::Chromatic;
                else throw std::invalid_argument("Unknown pitch arpeggiator scale");
            } else if (setting == "steps") {
                parameters.steps = static_cast<std::uint8_t>(std::lround(
                    std::clamp(parse_float(key, argv[i]), 1.0F, 16.0F)));
            } else if (setting == "repeats") {
                parameters.repeats = static_cast<std::uint8_t>(std::lround(
                    std::clamp(parse_float(key, argv[i]), 0.0F, 64.0F)));
            } else if (setting == "run") {
                if (value == "on") parameters.run = true;
                else if (value == "off") parameters.run = false;
                else throw std::invalid_argument("Pitch arpeggiator run must be on or off");
                options.pitch_arpeggiator_run_explicit = true;
            } else if (setting == "trigger-threshold-dbfs") {
                parameters.trigger_threshold_dbfs = parse_float(key, argv[i]);
            } else if (setting == "division") {
                using Division = tabletfx::effects::PitchArpeggiatorDivision;
                if (value == "free") parameters.step_division = Division::Free;
                else if (value == "whole") parameters.step_division = Division::Whole;
                else if (value == "half") parameters.step_division = Division::Half;
                else if (value == "dotted-half") parameters.step_division = Division::DottedHalf;
                else if (value == "quarter") parameters.step_division = Division::Quarter;
                else if (value == "dotted-quarter") parameters.step_division = Division::DottedQuarter;
                else if (value == "quarter-triplet") parameters.step_division = Division::QuarterTriplet;
                else if (value == "eighth") parameters.step_division = Division::Eighth;
                else if (value == "dotted-eighth") parameters.step_division = Division::DottedEighth;
                else if (value == "eighth-triplet") parameters.step_division = Division::EighthTriplet;
                else if (value == "sixteenth") parameters.step_division = Division::Sixteenth;
                else if (value == "dotted-sixteenth") parameters.step_division = Division::DottedSixteenth;
                else if (value == "sixteenth-triplet") parameters.step_division = Division::SixteenthTriplet;
                else if (value == "thirty-second") parameters.step_division = Division::ThirtySecond;
                else throw std::invalid_argument("Unknown pitch arpeggiator division");
            } else if (setting == "tempo-bpm") {
                parameters.tempo_bpm = parse_float(key, argv[i]);
            } else if (setting == "step-ms") {
                parameters.free_step_ms = parse_float(key, argv[i]);
            } else if (setting == "amplitude-shape") {
                using Shape = tabletfx::effects::PitchArpeggiatorAmplitudeShape;
                if (value == "constant") parameters.amplitude_shape = Shape::Constant;
                else if (value == "ramp-up") parameters.amplitude_shape = Shape::RampUp;
                else if (value == "ramp-down") parameters.amplitude_shape = Shape::RampDown;
                else if (value == "triangle") parameters.amplitude_shape = Shape::Triangle;
                else throw std::invalid_argument("Unknown pitch arpeggiator amplitude shape");
            } else if (setting == "amplitude-alpha") {
                parameters.amplitude_alpha = parse_float(key, argv[i]);
            } else if (setting == "pan-shape") {
                using Shape = tabletfx::effects::PitchArpeggiatorPanShape;
                if (value == "center") parameters.pan_shape = Shape::Center;
                else if (value == "left-to-right") parameters.pan_shape = Shape::LeftToRight;
                else if (value == "right-to-left") parameters.pan_shape = Shape::RightToLeft;
                else if (value == "ping-pong") parameters.pan_shape = Shape::PingPong;
                else throw std::invalid_argument("Unknown pitch arpeggiator pan shape");
            } else if (setting == "pan-alpha") {
                parameters.pan_alpha = parse_float(key, argv[i]);
            } else if (setting == "shifts") {
                const auto shifts = parse_step_pattern(value);
                for (std::size_t step = 0; step < shifts.size(); ++step) {
                    parameters.stage_shift_scale_degrees[step] = static_cast<int>(
                        std::lround(std::clamp(shifts[step], -24.0F, 24.0F)));
                }
            } else if (setting.starts_with("step-")) {
                unsigned step = 0;
                const char* first = setting.data() + 5;
                const char* last = setting.data() + setting.size();
                const auto parsed = std::from_chars(first, last, step);
                if (parsed.ec != std::errc{} || parsed.ptr != last ||
                    step == 0U || step > parameters.stage_shift_scale_degrees.size()) {
                    throw std::invalid_argument("Pitch arpeggiator step must be 1 through 16");
                }
                parameters.stage_shift_scale_degrees[step - 1U] = static_cast<int>(
                    std::lround(std::clamp(parse_float(key, argv[i]), -24.0F, 24.0F)));
            } else {
                throw std::invalid_argument("Unknown option: " + std::string(key));
            }
            options.pitch_arpeggiator_enabled = true;
            continue;
        }
        if (key.starts_with("--mono-guitar-synth-")) {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const std::string_view value(argv[++i]);
            auto& parameters = options.mono_guitar_synth;
            const std::string_view setting = key.substr(
                std::string_view("--mono-guitar-synth-").size());
            if (setting == "waveform") {
                using Waveform = tabletfx::effects::MonoGuitarSynthWaveform;
                if (value == "sine") parameters.waveform = Waveform::Sine;
                else if (value == "triangle") parameters.waveform = Waveform::Triangle;
                else if (value == "sawtooth") parameters.waveform = Waveform::Sawtooth;
                else if (value == "square") parameters.waveform = Waveform::Square;
                else throw std::invalid_argument(
                    "Mono guitar synth waveform must be sine, triangle, sawtooth, or square");
            } else {
                const float number = parse_float(key, argv[i]);
                if (setting == "mix") parameters.mix = number;
                else if (setting == "level") parameters.level = number;
                else if (setting == "gate-threshold-db") {
                    parameters.gate_threshold_db = number;
                } else if (setting == "attack-ms") parameters.attack_ms = number;
                else if (setting == "release-ms") parameters.release_ms = number;
                else if (setting == "portamento-ms") parameters.portamento_ms = number;
                else throw std::invalid_argument("Unknown option: " + std::string(key));
            }
            continue;
        }
        if (key.starts_with("--note-generator-")) {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const std::string_view value(argv[++i]);
            auto& parameters = options.note_generator;
            const std::string_view setting = key.substr(
                std::string_view("--note-generator-").size());
            if (setting == "voice-count") {
                const long count = std::lround(parse_float(key, argv[i]));
                parameters.voice_count = static_cast<std::uint8_t>(
                    std::clamp(count, 1L, 3L));
            } else if (setting == "output-level") {
                parameters.output_level = parse_float(key, argv[i]);
            } else if (setting.starts_with("voice-")) {
                const std::string_view voice_setting = setting.substr(6U);
                if (voice_setting.size() < 3U || voice_setting[1] != '-' ||
                    voice_setting[0] < '1' || voice_setting[0] > '3') {
                    throw std::invalid_argument(
                        "Note Generator voice options must use voice-1 through voice-3");
                }
                auto& voice = parameters.voices[
                    static_cast<std::size_t>(voice_setting[0] - '1')];
                const std::string_view field = voice_setting.substr(2U);
                if (field == "waveform") {
                    using Waveform = tabletfx::effects::NoteGeneratorWaveform;
                    if (value == "sine") voice.waveform = Waveform::Sine;
                    else if (value == "triangle") voice.waveform = Waveform::Triangle;
                    else if (value == "sawtooth") voice.waveform = Waveform::Sawtooth;
                    else if (value == "square") voice.waveform = Waveform::Square;
                    else throw std::invalid_argument(
                        "Note Generator waveform must be sine, triangle, sawtooth, or square");
                } else {
                    const float number = parse_float(key, argv[i]);
                    if (field == "note") {
                        voice.note_number = static_cast<int>(std::lround(std::clamp(
                            number, 0.0F, 127.0F)));
                    } else if (field == "octave") {
                        voice.octave = static_cast<int>(std::lround(std::clamp(
                            number, -2.0F, 2.0F)));
                    } else if (field == "level") {
                        voice.level = number;
                    } else {
                        throw std::invalid_argument("Unknown option: " + std::string(key));
                    }
                }
            } else {
                throw std::invalid_argument("Unknown option: " + std::string(key));
            }
            options.note_generator_enabled = true;
            continue;
        }
        if (key.starts_with("--pitch-feedback-delay-")) {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const float value = parse_float(key, argv[++i]);
            const std::string_view setting = key.substr(
                std::string_view("--pitch-feedback-delay-").size());
            auto& parameters = options.pitch_feedback_delay;
            if (setting == "time-ms") parameters.time_ms = value;
            else if (setting == "feedback") parameters.feedback = value;
            else if (setting == "mix") parameters.mix = value;
            else if (setting == "pitch-semitones") parameters.pitch_semitones = value;
            else throw std::invalid_argument("Unknown option: " + std::string(key));
            options.pitch_feedback_delay_enabled = true;
            continue;
        }
        if (key.starts_with("--feedback-generator-")) {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const std::string_view setting = key.substr(
                std::string_view("--feedback-generator-").size());
            auto& parameters = options.feedback_generator;
            if (setting == "type") {
                const std::string_view value(argv[++i]);
                using Type = tabletfx::effects::FeedbackGeneratorType;
                if (value == "octave-down") parameters.type = Type::OctaveDown;
                else if (value == "unison") parameters.type = Type::Unison;
                else if (value == "octave-up") parameters.type = Type::OctaveUp;
                else throw std::invalid_argument(
                    "Feedback Generator type must be octave-down, unison, or octave-up");
            } else if (setting == "dry-kill") {
                const std::string_view value(argv[++i]);
                using DryKill = tabletfx::effects::FeedbackGeneratorDryKill;
                if (value == "off") parameters.dry_kill = DryKill::Off;
                else if (value == "on") parameters.dry_kill = DryKill::On;
                else throw std::invalid_argument(
                    "Feedback Generator dry kill must be off or on");
            } else {
                const float value = parse_float(key, argv[++i]);
                if (setting == "gain") parameters.feedback_gain = value;
                else if (setting == "attack-ms") parameters.attack_ms = value;
                else if (setting == "release-ms") parameters.release_ms = value;
                else if (setting == "dry-level") parameters.dry_level = value;
                else if (setting == "silence-threshold-db") {
                    parameters.silence_threshold_db = value;
                } else throw std::invalid_argument("Unknown option: " + std::string(key));
            }
            options.feedback_generator_enabled = true;
            continue;
        }
        if (key.starts_with("--shimmer-reverb-")) {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const float value = parse_float(key, argv[++i]);
            const std::string_view setting = key.substr(
                std::string_view("--shimmer-reverb-").size());
            auto& parameters = options.shimmer_reverb;
            if (setting == "decay-seconds") parameters.decay_seconds = value;
            else if (setting == "damping-hz") parameters.damping_hz = value;
            else if (setting == "diffusion") parameters.diffusion = value;
            else if (setting == "feedback") parameters.feedback = value;
            else if (setting == "high-pass-hz") parameters.high_pass_hz = value;
            else if (setting == "pitch-semitones") parameters.pitch_semitones = value;
            else if (setting == "intensity") parameters.shimmer_intensity = value;
            else if (setting == "mix") parameters.mix = value;
            else if (setting == "level-db") parameters.level_db = value;
            else throw std::invalid_argument("Unknown option: " + std::string(key));
            options.shimmer_reverb_enabled = true;
            continue;
        }
        if (key.starts_with("--grain-delay-")) {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const std::string_view setting = key.substr(
                std::string_view("--grain-delay-").size());
            const float value = parse_float(key, argv[++i]);
            auto& parameters = options.grain_delay;
            if (setting == "time-ms") parameters.time_ms = value;
            else if (setting == "feedback") parameters.feedback = value;
            else if (setting == "mix") parameters.mix = value;
            else if (setting == "grain-size-ms") parameters.grain_size_ms = value;
            else if (setting == "overlap") {
                parameters.overlap = static_cast<std::uint8_t>(
                    std::clamp(std::lround(value), 0L, 255L));
            } else if (setting == "pitch-semitones") {
                parameters.pitch_semitones = value;
            } else if (setting == "spray-ms") {
                parameters.spray_ms = value;
            } else {
                throw std::invalid_argument("Unknown option: " + std::string(key));
            }
            options.grain_delay_enabled = true;
            continue;
        }
        if (key.starts_with("--four-band-resonator-")) {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const std::string_view value(argv[++i]);
            auto& parameters = options.four_band_resonator;
            const std::string_view setting = key.substr(
                std::string_view("--four-band-resonator-").size());
            if (setting == "mode") {
                if (value == "manual") {
                    parameters.mode = tabletfx::FourBandResonatorMode::Manual;
                } else if (value == "chord") {
                    parameters.mode = tabletfx::FourBandResonatorMode::Chord;
                } else {
                    throw std::invalid_argument(
                        "Four-band resonator mode must be manual or chord");
                }
            } else if (setting == "chord") {
                parameters.chord = parse_four_band_resonator_chord(value);
            } else if (setting == "master-frequency") {
                parameters.master_frequency = parse_float(key, argv[i]);
            } else if (setting == "frequencies-hz") {
                parameters.manual_frequency_hz = parse_float_array<4>(key, value);
            } else if (setting == "frequency-hz") {
                parameters.chord_root_frequency_hz = parse_float(key, argv[i]);
            } else if (setting == "feedback") {
                parameters.feedback = parse_float_array<4>(key, value);
            } else if (setting == "q") {
                parameters.q = parse_float_array<4>(key, value);
            } else if (setting == "levels") {
                parameters.level = parse_float_array<4>(key, value);
            } else if (setting == "pans") {
                parameters.pan = parse_float_array<4>(key, value);
            } else if (setting == "filter-locations") {
                parameters.filter_location =
                    parse_four_band_resonator_filter_locations(value);
            } else if (setting == "input-mode") {
                if (value == "mono") {
                    parameters.input_mode =
                        tabletfx::FourBandResonatorInputMode::Mono;
                } else if (value == "stereo") {
                    parameters.input_mode =
                        tabletfx::FourBandResonatorInputMode::Stereo;
                } else {
                    throw std::invalid_argument(
                        "Four-band resonator input mode must be mono or stereo");
                }
            } else if (setting == "master-level") {
                parameters.master_level = parse_float(key, argv[i]);
            } else if (setting == "master-pan") {
                parameters.master_pan = parse_float(key, argv[i]);
            } else if (setting == "master-feedback") {
                parameters.master_feedback = parse_float(key, argv[i]);
            } else if (setting == "master-q") {
                parameters.master_q = parse_float(key, argv[i]);
            } else if (setting == "mix") {
                parameters.mix = parse_float(key, argv[i]);
            } else {
                throw std::invalid_argument("Unknown option: " + std::string(key));
            }
            options.four_band_resonator_enabled = true;
            continue;
        }
        if (key == "--rhythmic-slicer") {
            options.rhythmic_slicer_enabled = true;
            continue;
        }
        if (key == "--step-filter") {
            options.step_filter_enabled = true;
            continue;
        }
        if (key == "--notch-filter") {
            options.notch_filter_enabled = true;
            continue;
        }
        if (key == "--formant-filter") {
            options.formant_filter_enabled = true;
            continue;
        }
        if (key == "--harmonic-resonator") {
            options.harmonic_resonator_enabled = true;
            continue;
        }
        if (key == "--multiband-compressor") {
            options.multiband_compressor_enabled = true;
            continue;
        }
        if (key == "--reverse-delay") {
            options.reverse_delay_enabled = true;
            continue;
        }
        if (key == "--ducking-delay") {
            options.ducking_delay_enabled = true;
            continue;
        }
        if (key == "--modulated-delay") {
            options.modulated_delay_enabled = true;
            continue;
        }
        if (key == "--filtered-delay") {
            options.filtered_delay_enabled = true;
            continue;
        }
        if (key == "--resonant-delay") {
            options.resonant_delay_enabled = true;
            continue;
        }
        if (key == "--sweep-delay") {
            options.sweep_delay_enabled = true;
            continue;
        }
        if (key == "--multiband-slope" || key == "--multiband-low-detector" ||
            key == "--multiband-mid-detector" ||
            key == "--multiband-high-detector" ||
            key == "--multiband-low-mute" || key == "--multiband-mid-mute" ||
            key == "--multiband-high-mute") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const std::string_view value(argv[++i]);
            auto& parameters = options.multiband_compressor;
            if (key == "--multiband-slope") {
                if (value == "db12") {
                    parameters.crossover_slope =
                        tabletfx::MultibandCrossoverSlope::Db12;
                } else if (value == "db24") {
                    parameters.crossover_slope =
                        tabletfx::MultibandCrossoverSlope::Db24;
                } else {
                    throw std::invalid_argument(
                        "Multiband slope must be db12 or db24");
                }
            } else {
                const std::size_t band =
                    key.find("-low-") != std::string_view::npos ? 0U :
                    key.find("-mid-") != std::string_view::npos ? 1U : 2U;
                auto& band_parameters = parameters.bands[band];
                if (key.ends_with("-detector")) {
                    if (value == "peak") {
                        band_parameters.detector =
                            tabletfx::MultibandDetector::Peak;
                    } else if (value == "rms") {
                        band_parameters.detector =
                            tabletfx::MultibandDetector::Rms;
                    } else {
                        throw std::invalid_argument(
                            "Multiband detector must be peak or rms");
                    }
                } else if (value == "on") {
                    band_parameters.mute = true;
                } else if (value == "off") {
                    band_parameters.mute = false;
                } else {
                    throw std::invalid_argument(
                        "Multiband mute must be on or off");
                }
            }
            options.multiband_compressor_enabled = true;
            continue;
        }
        if (key == "--modulated-delay-waveform") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --modulated-delay-waveform");
            }
            const std::string_view value(argv[++i]);
            if (value == "sine") {
                options.modulated_delay.waveform = tabletfx::ModulatedDelayWaveform::sine;
            } else if (value == "triangle") {
                options.modulated_delay.waveform = tabletfx::ModulatedDelayWaveform::triangle;
            } else if (value == "random") {
                options.modulated_delay.waveform = tabletfx::ModulatedDelayWaveform::random;
            } else if (value == "triggered-ramp") {
                options.modulated_delay.waveform =
                    tabletfx::ModulatedDelayWaveform::triggered_ramp;
            } else {
                throw std::invalid_argument(
                    "Modulated-delay waveform must be sine, triangle, random, or triggered-ramp");
            }
            options.modulated_delay_enabled = true;
            continue;
        }
        if (key == "--sweep-delay-waveform") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --sweep-delay-waveform");
            }
            const std::string_view value(argv[++i]);
            if (value == "sine") {
                options.sweep_delay.waveform = tabletfx::SweepDelayWaveform::sine;
            } else if (value == "triangle") {
                options.sweep_delay.waveform = tabletfx::SweepDelayWaveform::triangle;
            } else if (value == "square") {
                options.sweep_delay.waveform = tabletfx::SweepDelayWaveform::square;
            } else {
                throw std::invalid_argument(
                    "Sweep-delay waveform must be sine, triangle, or square");
            }
            options.sweep_delay_enabled = true;
            continue;
        }
        if (key == "--reverse-delay-run") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --reverse-delay-run");
            }
            const std::string_view value(argv[++i]);
            if (value == "on") options.reverse_delay_running = true;
            else if (value == "off") options.reverse_delay_running = false;
            else throw std::invalid_argument("Reverse-delay Run must be on or off");
            options.reverse_delay_enabled = true;
            continue;
        }
        if (key == "--reverse-delay-time-mode") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --reverse-delay-time-mode");
            }
            const std::string_view value(argv[++i]);
            if (value == "milliseconds" || value == "ms") {
                options.reverse_delay.time_mode = tabletfx::DelayTimeMode::milliseconds;
            } else if (value == "tempo-sync") {
                options.reverse_delay.time_mode = tabletfx::DelayTimeMode::tempo_sync;
            } else {
                throw std::invalid_argument(
                    "Reverse-delay time mode must be milliseconds or tempo-sync");
            }
            options.reverse_delay_enabled = true;
            continue;
        }
        if (key == "--reverse-delay-division") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --reverse-delay-division");
            }
            options.reverse_delay.division = parse_delay_division(argv[++i]);
            options.reverse_delay_enabled = true;
            continue;
        }
        if (key == "--reverse-delay-spillover") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --reverse-delay-spillover");
            }
            const std::string_view value(argv[++i]);
            if (value == "on") options.reverse_delay.spillover = true;
            else if (value == "off") options.reverse_delay.spillover = false;
            else throw std::invalid_argument("Reverse-delay spillover must be on or off");
            options.reverse_delay_enabled = true;
            continue;
        }
        if (key == "--dimension-chorus-version") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --dimension-chorus-version");
            }
            const std::string_view value(argv[++i]);
            if (value == "C") {
                options.dimension_chorus.version = tabletfx::DimensionChorusVersion::C;
            } else if (value == "D") {
                options.dimension_chorus.version = tabletfx::DimensionChorusVersion::D;
            } else {
                throw std::invalid_argument("Dimension Chorus Version must be C or D");
            }
            options.dimension_chorus_enabled = true;
            continue;
        }
        if (key == "--dimension-chorus-mode") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --dimension-chorus-mode");
            }
            options.dimension_chorus.mode =
                parse_dimension_chorus_mode(argv[++i]);
            options.dimension_chorus_enabled = true;
            continue;
        }
        if (key == "--rotary-tempo-sync") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --rotary-tempo-sync");
            }
            const std::string_view value(argv[++i]);
            if (value == "on") options.rotary_speaker.tempo_sync = true;
            else if (value == "off") options.rotary_speaker.tempo_sync = false;
            else throw std::invalid_argument("Rotary tempo sync must be on or off");
            options.rotary_speaker_enabled = true;
            continue;
        }
        if (key == "--rotary-speed") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --rotary-speed");
            }
            const std::string_view value(argv[++i]);
            if (value == "slow") options.rotary_speaker.speed = tabletfx::RotarySpeed::slow;
            else if (value == "fast") options.rotary_speaker.speed = tabletfx::RotarySpeed::fast;
            else throw std::invalid_argument("Rotary speed must be slow or fast");
            options.rotary_speaker_enabled = true;
            continue;
        }
        if (key == "--rhythmic-slicer-tempo-sync") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --rhythmic-slicer-tempo-sync");
            }
            const std::string_view value(argv[++i]);
            if (value == "on") options.rhythmic_slicer.tempo_sync = true;
            else if (value == "off") options.rhythmic_slicer.tempo_sync = false;
            else throw std::invalid_argument("Rhythmic-slicer tempo sync must be on or off");
            options.rhythmic_slicer_enabled = true;
            continue;
        }
        if (key == "--rhythmic-slicer-division") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --rhythmic-slicer-division");
            }
            options.rhythmic_slicer.division =
                parse_rhythmic_division(argv[++i]);
            options.rhythmic_slicer_enabled = true;
            continue;
        }
        if (key == "--rhythmic-slicer-steps") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --rhythmic-slicer-steps");
            }
            options.rhythmic_slicer.steps =
                parse_step_pattern(argv[++i]);
            options.rhythmic_slicer_enabled = true;
            continue;
        }
        if (key == "--step-filter-tempo-sync") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --step-filter-tempo-sync");
            }
            const std::string_view value(argv[++i]);
            if (value == "on") options.step_filter.tempo_sync = true;
            else if (value == "off") options.step_filter.tempo_sync = false;
            else throw std::invalid_argument("Step-filter tempo sync must be on or off");
            options.step_filter_enabled = true;
            continue;
        }
        if (key == "--step-filter-division") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --step-filter-division");
            }
            options.step_filter.division = parse_rhythmic_division(argv[++i]);
            options.step_filter_enabled = true;
            continue;
        }
        if (key == "--step-filter-mode") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --step-filter-mode");
            }
            options.step_filter.mode = parse_step_filter_mode(argv[++i]);
            options.step_filter_enabled = true;
            continue;
        }
        if (key == "--step-filter-steps") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --step-filter-steps");
            }
            options.step_filter.steps = parse_step_pattern(argv[++i]);
            options.step_filter_enabled = true;
            continue;
        }
        if (key == "--rotary-input-select") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --rotary-input-select");
            }
            const std::string_view value(argv[++i]);
            if (value == "stereo") options.rotary_speaker.input_select = tabletfx::RotaryInputSelect::stereo;
            else if (value == "left") options.rotary_speaker.input_select = tabletfx::RotaryInputSelect::left;
            else if (value == "right") options.rotary_speaker.input_select = tabletfx::RotaryInputSelect::right;
            else if (value == "sum") options.rotary_speaker.input_select = tabletfx::RotaryInputSelect::sum;
            else throw std::invalid_argument("Rotary input select must be stereo, left, right, or sum");
            options.rotary_speaker_enabled = true;
            continue;
        }
        if (key == "--auto-swell-taper") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for --auto-swell-taper");
            }
            const std::string_view value(argv[++i]);
            if (value == "linear") options.auto_swell.taper = tabletfx::AutoSwellTaper::Linear;
            else if (value == "log30a") options.auto_swell.taper = tabletfx::AutoSwellTaper::Log30A;
            else if (value == "log20a") options.auto_swell.taper = tabletfx::AutoSwellTaper::Log20A;
            else if (value == "log15a") options.auto_swell.taper = tabletfx::AutoSwellTaper::Log15A;
            else if (value == "log10a") options.auto_swell.taper = tabletfx::AutoSwellTaper::Log10A;
            else if (value == "log5a") options.auto_swell.taper = tabletfx::AutoSwellTaper::Log5A;
            else if (value == "s-taper") options.auto_swell.taper = tabletfx::AutoSwellTaper::SwellS;
            else throw std::invalid_argument("Auto Swell taper must be linear, log30a, log20a, log15a, log10a, log5a, or s-taper");
            options.auto_swell_enabled = true;
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
        if (key == "--bit-crusher-bit-depth" ||
            key == "--bit-crusher-sample-rate-divisor") {
            const std::uint32_t value = parse_unsigned_integer(key, argv[++i]);
            if (key == "--bit-crusher-bit-depth") {
                if (value < 2U || value > 24U) {
                    throw std::invalid_argument(
                        "Bit crusher bit depth must be an integer from 2 to 24");
                }
                options.bit_crusher.bit_depth =
                    static_cast<std::uint8_t>(value);
            } else {
                if (value < 1U || value > 128U) {
                    throw std::invalid_argument(
                        "Bit crusher sample-rate divisor must be an integer from 1 to 128");
                }
                options.bit_crusher.sample_rate_divisor =
                    static_cast<std::uint16_t>(value);
            }
            options.bit_crusher_enabled = true;
            continue;
        }
        if (key == "--guitar-eq-layout") {
            const std::string_view value(argv[++i]);
            if (value == "5" || value == "five") {
                options.guitar_graphic_eq_layout = tabletfx::GuitarGraphicEQLayout::FiveBand;
            } else if (value == "7" || value == "seven") {
                options.guitar_graphic_eq_layout = tabletfx::GuitarGraphicEQLayout::SevenBand;
            } else {
                throw std::invalid_argument("Guitar EQ layout must be 5/five or 7/seven");
            }
            options.guitar_graphic_eq_enabled = true;
            continue;
        }
        if (key == "--bass-eq-layout") {
            const std::string_view value(argv[++i]);
            if (value == "4" || value == "four") {
                options.bass_graphic_eq_layout = tabletfx::BassGraphicEQLayout::FourBand;
            } else if (value == "7" || value == "seven") {
                options.bass_graphic_eq_layout = tabletfx::BassGraphicEQLayout::SevenBand;
            } else {
                throw std::invalid_argument("Bass EQ layout must be 4/four or 7/seven");
            }
            options.bass_graphic_eq_enabled = true;
            continue;
        }
        if (key == "--shelf-tilt-mode") {
            const std::string_view value(argv[++i]);
            if (value == "low-shelf") {
                options.shelf_tilt_eq_mode = tabletfx::ShelfTiltMode::LowShelf;
            } else if (value == "high-shelf") {
                options.shelf_tilt_eq_mode = tabletfx::ShelfTiltMode::HighShelf;
            } else if (value == "tilt") {
                options.shelf_tilt_eq_mode = tabletfx::ShelfTiltMode::Tilt;
            } else {
                throw std::invalid_argument("Shelf/tilt EQ mode must be low-shelf, high-shelf, or tilt");
            }
            options.shelf_tilt_eq_enabled = true;
            continue;
        }
        if (key == "--room-size") {
            const std::string_view value(argv[++i]);
            if (value == "small") {
                options.room_reverb.size = tabletfx::RoomSize::small;
            } else if (value == "large") {
                options.room_reverb.size = tabletfx::RoomSize::large;
            } else {
                throw std::invalid_argument("Room size must be small or large");
            }
            options.room_reverb_enabled = true;
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
                options.delay_tape_enabled = false;
                options.delay_tape_heads_enabled = false;
            } else if (value == "slapback") {
                options.delay_ms = 110.0F;
                options.delay_feedback = 0.12F;
                options.delay_mix = 0.25F;
                options.delay_tempo_enabled = false;
                options.delay_bbd_style = false;
                options.delay_tape_enabled = false;
                options.delay_tape_heads_enabled = false;
            } else if (value == "bbd") {
                options.delay_ms = 280.0F;
                options.delay_feedback = 0.28F;
                options.delay_mix = 0.25F;
                options.delay_tempo_enabled = false;
                options.delay_bbd_style = true;
                options.delay_tape_enabled = false;
                options.delay_tape_heads_enabled = false;
            } else if (value == "tape") {
                options.delay_ms = 300.0F;
                options.delay_feedback = 0.35F;
                options.delay_mix = 0.25F;
                options.delay_tempo_enabled = false;
                options.delay_bbd_style = false;
                options.delay_tape_enabled = true;
                options.delay_tape_heads_enabled = false;
            } else if (value == "tape-heads") {
                options.delay_ms = 110.0F;
                options.delay_feedback = 0.28F;
                options.delay_mix = 0.25F;
                options.delay_tempo_enabled = false;
                options.delay_bbd_style = false;
                options.delay_tape_enabled = true;
                options.delay_tape_heads_enabled = true;
                options.delay_tape_head_spacing_ms = 110.0F;
                options.delay_tape_head_mask = 0x0FU;
            } else {
                throw std::invalid_argument("Delay preset must be digital, slapback, bbd, tape, or tape-heads");
            }
            continue;
        }
        if (key == "--delay-heads") {
            options.delay_tape_head_mask = parse_delay_head_mask(argv[++i]);
            options.delay_tape_heads_enabled = true;
            options.delay_multitap_enabled = false;
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
        if (key == "--triple-double-chorus-preset-switch" ||
            key == "--triple-double-chorus-manual-switch" ||
            key == "--triple-double-chorus-stereo-switch") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value for " + std::string(key));
            }
            const std::string_view value(argv[++i]);
            if (value != "on" && value != "off") {
                throw std::invalid_argument("Triple-Double Chorus switches must be on or off");
            }
            const bool enabled = value == "on";
            if (key == "--triple-double-chorus-preset-switch") {
                options.triple_double_chorus.preset_switch = enabled;
            } else if (key == "--triple-double-chorus-manual-switch") {
                options.triple_double_chorus.manual_switch = enabled;
            } else {
                options.triple_double_chorus.stereo_switch = enabled;
            }
            options.triple_double_chorus_enabled = true;
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
        else if (key == "--dimension-chorus-mix") {
            options.dimension_chorus.mix = value;
            options.dimension_chorus_enabled = true;
        }
        else if (key == "--dimension-chorus-level-db") {
            options.dimension_chorus.level_db = value;
            options.dimension_chorus_enabled = true;
        }
        else if (key == "--triple-double-chorus-rate-hz") {
            options.triple_double_chorus.rate_hz = value;
            options.triple_double_chorus_enabled = true;
        }
        else if (key == "--triple-double-chorus-left-depth-ms") {
            options.triple_double_chorus.left_depth_ms = value;
            options.triple_double_chorus_enabled = true;
        }
        else if (key == "--triple-double-chorus-center-depth-ms") {
            options.triple_double_chorus.center_depth_ms = value;
            options.triple_double_chorus_enabled = true;
        }
        else if (key == "--triple-double-chorus-right-depth-ms") {
            options.triple_double_chorus.right_depth_ms = value;
            options.triple_double_chorus_enabled = true;
        }
        else if (key == "--triple-double-chorus-feedback") {
            options.triple_double_chorus.feedback = value;
            options.triple_double_chorus_enabled = true;
        }
        else if (key == "--triple-double-chorus-sparkle") {
            options.triple_double_chorus.sparkle = value;
            options.triple_double_chorus_enabled = true;
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
        else if (key == "--delay-head-spacing-ms") {
            options.delay_tape_head_spacing_ms = value;
            options.delay_tape_heads_enabled = true;
            options.delay_multitap_enabled = false;
        }
        else if (key == "--delay-tape-saturation") options.delay_tape_saturation = value;
        else if (key == "--delay-tape-wow-depth-ms") options.delay_tape_wow_depth_ms = value;
        else if (key == "--delay-tape-wow-rate-hz") options.delay_tape_wow_rate_hz = value;
        else if (key == "--delay-tape-flutter-depth-ms") options.delay_tape_flutter_depth_ms = value;
        else if (key == "--delay-tape-flutter-rate-hz") options.delay_tape_flutter_rate_hz = value;
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
        else if (key == "--optical-compressor-threshold-db") {
            options.optical_compressor.threshold_db = value;
            options.optical_compressor_enabled = true;
        }
        else if (key == "--optical-compressor-ratio") {
            options.optical_compressor.ratio = value;
            options.optical_compressor_enabled = true;
        }
        else if (key == "--optical-compressor-knee-db") {
            options.optical_compressor.knee_db = value;
            options.optical_compressor_enabled = true;
        }
        else if (key == "--optical-compressor-attack-ms") {
            options.optical_compressor.attack_ms = value;
            options.optical_compressor_enabled = true;
        }
        else if (key == "--optical-compressor-release-ms") {
            options.optical_compressor.release_ms = value;
            options.optical_compressor_enabled = true;
        }
        else if (key == "--optical-compressor-program-release-ms") {
            options.optical_compressor.program_release_ms = value;
            options.optical_compressor_enabled = true;
        }
        else if (key == "--optical-compressor-rms-window-ms") {
            options.optical_compressor.rms_window_ms = value;
            options.optical_compressor_enabled = true;
        }
        else if (key == "--optical-compressor-makeup-db") {
            options.optical_compressor.makeup_db = value;
            options.optical_compressor_enabled = true;
        }
        else if (key == "--fet-compressor-threshold-db") {
            options.fet_compressor.threshold_db = value;
            options.fet_compressor_enabled = true;
        }
        else if (key == "--fet-compressor-ratio") {
            options.fet_compressor.ratio = value;
            options.fet_compressor_enabled = true;
        }
        else if (key == "--fet-compressor-knee-db") {
            options.fet_compressor.knee_db = value;
            options.fet_compressor_enabled = true;
        }
        else if (key == "--fet-compressor-attack-ms") {
            options.fet_compressor.attack_ms = value;
            options.fet_compressor_enabled = true;
        }
        else if (key == "--fet-compressor-release-ms") {
            options.fet_compressor.release_ms = value;
            options.fet_compressor_enabled = true;
        }
        else if (key == "--fet-compressor-makeup-db") {
            options.fet_compressor.makeup_db = value;
            options.fet_compressor_enabled = true;
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
        else if (set_five_band_eq_parameter(key, options, value)) {}
        else if (key == "--shelf-tilt-frequency-hz") {
            options.shelf_tilt_eq_frequency_hz = value;
            options.shelf_tilt_eq_enabled = true;
        }
        else if (key == "--shelf-tilt-gain-db") {
            options.shelf_tilt_eq_gain_db = value;
            options.shelf_tilt_eq_enabled = true;
        }
        else if (key == "--room-decay-s") {
            options.room_reverb.decay_seconds = value;
            options.room_reverb_enabled = true;
        }
        else if (key == "--room-pre-delay-ms") {
            options.room_reverb.pre_delay_ms = value;
            options.room_reverb_enabled = true;
        }
        else if (key == "--room-damping-hz") {
            options.room_reverb.damping_hz = value;
            options.room_reverb_enabled = true;
        }
        else if (key == "--room-mix") {
            options.room_reverb.mix = value;
            options.room_reverb_enabled = true;
        }
        else if (key == "--room-level-db") {
            options.room_reverb.level_db = value;
            options.room_reverb_enabled = true;
        }
        else if (key == "--hall-decay-s") {
            options.hall_reverb.decay_seconds = value;
            options.hall_reverb_enabled = true;
        }
        else if (key == "--hall-pre-delay-ms") {
            options.hall_reverb.pre_delay_ms = value;
            options.hall_reverb_enabled = true;
        }
        else if (key == "--hall-damping-hz") {
            options.hall_reverb.damping_hz = value;
            options.hall_reverb_enabled = true;
        }
        else if (key == "--hall-low-cut-hz") {
            options.hall_reverb.low_cut_hz = value;
            options.hall_reverb_enabled = true;
        }
        else if (key == "--hall-mix") {
            options.hall_reverb.mix = value;
            options.hall_reverb_enabled = true;
        }
        else if (key == "--hall-level-db") {
            options.hall_reverb.level_db = value;
            options.hall_reverb_enabled = true;
        }
        else if (key == "--plate-decay-s") {
            options.plate_reverb.decay_seconds = value;
            options.plate_reverb_enabled = true;
        }
        else if (key == "--plate-pre-delay-ms") {
            options.plate_reverb.pre_delay_ms = value;
            options.plate_reverb_enabled = true;
        }
        else if (key == "--plate-damping-hz") {
            options.plate_reverb.damping_hz = value;
            options.plate_reverb_enabled = true;
        }
        else if (key == "--plate-diffusion") {
            options.plate_reverb.diffusion = value;
            options.plate_reverb_enabled = true;
        }
        else if (key == "--plate-balance") {
            options.plate_reverb.balance = value;
            options.plate_reverb_enabled = true;
        }
        else if (key == "--plate-mix") {
            options.plate_reverb.mix = value;
            options.plate_reverb_enabled = true;
        }
        else if (key == "--plate-level-db") {
            options.plate_reverb.level_db = value;
            options.plate_reverb_enabled = true;
        }
        else if (key == "--spring-decay-s") {
            options.spring_reverb.decay_seconds = value;
            options.spring_reverb_enabled = true;
        }
        else if (key == "--spring-pre-delay-ms") {
            options.spring_reverb.pre_delay_ms = value;
            options.spring_reverb_enabled = true;
        }
        else if (key == "--spring-damping-hz") {
            options.spring_reverb.damping_hz = value;
            options.spring_reverb_enabled = true;
        }
        else if (key == "--spring-dwell") {
            options.spring_reverb.dwell = value;
            options.spring_reverb_enabled = true;
        }
        else if (key == "--spring-mix") {
            options.spring_reverb.mix = value;
            options.spring_reverb_enabled = true;
        }
        else if (key == "--spring-level-db") {
            options.spring_reverb.level_db = value;
            options.spring_reverb_enabled = true;
        }
        else if (key == "--auto-swell-threshold-db") {
            options.auto_swell.threshold_db = value;
            options.auto_swell_enabled = true;
        }
        else if (key == "--auto-swell-attack-ms") {
            options.auto_swell.attack_ms = value;
            options.auto_swell_enabled = true;
        }
        else if (key == "--auto-swell-release-ms") {
            options.auto_swell.release_ms = value;
            options.auto_swell_enabled = true;
        }
        else if (key == "--auto-swell-hysteresis-db") {
            options.auto_swell.hysteresis_db = value;
            options.auto_swell_enabled = true;
        }
        else if (key == "--auto-swell-volume") {
            options.auto_swell.volume = value;
            options.auto_swell_enabled = true;
        }
        else if (key == "--ring-mod-frequency-hz") {
            options.ring_modulator.frequency_hz = value;
            options.ring_modulator_enabled = true;
        }
        else if (key == "--ring-mod-multiplier") {
            options.ring_modulator.frequency_multiplier = value;
            options.ring_modulator_enabled = true;
        }
        else if (key == "--ring-mod-high-cut-hz") {
            options.ring_modulator.high_cut_hz = value;
            options.ring_modulator_enabled = true;
        }
        else if (key == "--ring-mod-mix") {
            options.ring_modulator.mix = value;
            options.ring_modulator_enabled = true;
        }
        else if (key == "--bit-crusher-mix") {
            options.bit_crusher.mix = value;
            options.bit_crusher_enabled = true;
        }
        else if (key == "--rhythmic-slicer-tempo-bpm") {
            options.rhythmic_slicer.tempo_bpm = value;
            options.rhythmic_slicer_enabled = true;
        }
        else if (key == "--rhythmic-slicer-rate-hz") {
            options.rhythmic_slicer.rate_hz = value;
            options.rhythmic_slicer_enabled = true;
        }
        else if (key == "--rhythmic-slicer-depth") {
            options.rhythmic_slicer.depth = value;
            options.rhythmic_slicer_enabled = true;
        }
        else if (key == "--rhythmic-slicer-shape") {
            options.rhythmic_slicer.shape = value;
            options.rhythmic_slicer_enabled = true;
        }
        else if (key == "--rhythmic-slicer-attack-ms") {
            options.rhythmic_slicer.attack_ms = value;
            options.rhythmic_slicer_enabled = true;
        }
        else if (key == "--rhythmic-slicer-release-ms") {
            options.rhythmic_slicer.release_ms = value;
            options.rhythmic_slicer_enabled = true;
        }
        else if (key == "--rhythmic-slicer-phase") {
            options.rhythmic_slicer.phase = value;
            options.rhythmic_slicer_enabled = true;
        }
        else if (key == "--step-filter-tempo-bpm") {
            options.step_filter.tempo_bpm = value;
            options.step_filter_enabled = true;
        }
        else if (key == "--step-filter-rate-hz") {
            options.step_filter.rate_hz = value;
            options.step_filter_enabled = true;
        }
        else if (key == "--step-filter-min-cutoff-hz") {
            options.step_filter.minimum_cutoff_hz = value;
            options.step_filter_enabled = true;
        }
        else if (key == "--step-filter-max-cutoff-hz") {
            options.step_filter.maximum_cutoff_hz = value;
            options.step_filter_enabled = true;
        }
        else if (key == "--step-filter-resonance-q") {
            options.step_filter.resonance_q = value;
            options.step_filter_enabled = true;
        }
        else if (key == "--step-filter-depth") {
            options.step_filter.depth = value;
            options.step_filter_enabled = true;
        }
        else if (key == "--step-filter-shape") {
            options.step_filter.shape = value;
            options.step_filter_enabled = true;
        }
        else if (key == "--step-filter-smoothing-ms") {
            options.step_filter.smoothing_ms = value;
            options.step_filter_enabled = true;
        }
        else if (key == "--step-filter-mix") {
            options.step_filter.mix = value;
            options.step_filter_enabled = true;
        }
        else if (key == "--step-filter-phase") {
            options.step_filter.phase = value;
            options.step_filter_enabled = true;
        }
        else if (key == "--notch-filter-frequency-hz") {
            options.notch_filter.frequency_hz = value;
            options.notch_filter_enabled = true;
        }
        else if (key == "--notch-filter-q") {
            options.notch_filter.q = value;
            options.notch_filter_enabled = true;
        }
        else if (key == "--notch-filter-mix") {
            options.notch_filter.mix = value;
            options.notch_filter_enabled = true;
        }
        else if (key == "--formant-filter-vowel-position") {
            options.formant_filter.vowel_position = value;
            options.formant_filter_enabled = true;
        }
        else if (key == "--formant-filter-q") {
            options.formant_filter.resonance_q = value;
            options.formant_filter_enabled = true;
        }
        else if (key == "--formant-filter-mix") {
            options.formant_filter.mix = value;
            options.formant_filter_enabled = true;
        }
        else if (key == "--formant-filter-level-db") {
            options.formant_filter.level_db = value;
            options.formant_filter_enabled = true;
        }
        else if (key == "--harmonic-resonator-root-frequency-hz") {
            options.harmonic_resonator.root_frequency_hz = value;
            options.harmonic_resonator_enabled = true;
        }
        else if (key == "--harmonic-resonator-decay-seconds") {
            options.harmonic_resonator.decay_seconds = value;
            options.harmonic_resonator_enabled = true;
        }
        else if (key == "--harmonic-resonator-brightness") {
            options.harmonic_resonator.brightness = value;
            options.harmonic_resonator_enabled = true;
        }
        else if (key == "--harmonic-resonator-mix") {
            options.harmonic_resonator.mix = value;
            options.harmonic_resonator_enabled = true;
        }
        else if (key == "--multiband-low-mid-hz") {
            options.multiband_compressor.low_mid_crossover_hz = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-mid-high-hz") {
            options.multiband_compressor.mid_high_crossover_hz = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-low-threshold-db") {
            options.multiband_compressor.bands[0].threshold_db = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-low-ratio") {
            options.multiband_compressor.bands[0].ratio = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-low-knee-db") {
            options.multiband_compressor.bands[0].knee_db = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-low-attack-ms") {
            options.multiband_compressor.bands[0].attack_ms = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-low-release-ms") {
            options.multiband_compressor.bands[0].release_ms = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-low-level-db") {
            options.multiband_compressor.bands[0].level_db = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-mid-threshold-db") {
            options.multiband_compressor.bands[1].threshold_db = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-mid-ratio") {
            options.multiband_compressor.bands[1].ratio = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-mid-knee-db") {
            options.multiband_compressor.bands[1].knee_db = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-mid-attack-ms") {
            options.multiband_compressor.bands[1].attack_ms = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-mid-release-ms") {
            options.multiband_compressor.bands[1].release_ms = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-mid-level-db") {
            options.multiband_compressor.bands[1].level_db = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-high-threshold-db") {
            options.multiband_compressor.bands[2].threshold_db = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-high-ratio") {
            options.multiband_compressor.bands[2].ratio = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-high-knee-db") {
            options.multiband_compressor.bands[2].knee_db = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-high-attack-ms") {
            options.multiband_compressor.bands[2].attack_ms = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-high-release-ms") {
            options.multiband_compressor.bands[2].release_ms = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--multiband-high-level-db") {
            options.multiband_compressor.bands[2].level_db = value;
            options.multiband_compressor_enabled = true;
        }
        else if (key == "--reverse-delay-time-ms") {
            options.reverse_delay.time_ms = value;
            options.reverse_delay_enabled = true;
        }
        else if (key == "--reverse-delay-feedback") {
            options.reverse_delay.feedback = value;
            options.reverse_delay_enabled = true;
        }
        else if (key == "--reverse-delay-mix") {
            options.reverse_delay.mix = value;
            options.reverse_delay_enabled = true;
        }
        else if (key == "--reverse-delay-crossfade-ms") {
            options.reverse_delay.crossfade_ms = value;
            options.reverse_delay_enabled = true;
        }
        else if (key == "--reverse-delay-tempo-bpm") {
            options.reverse_delay.tempo_bpm = value;
            options.reverse_delay_enabled = true;
        }
        else if (key == "--reverse-delay-low-cut-hz") {
            options.reverse_delay.low_cut_hz = value;
            options.reverse_delay_enabled = true;
        }
        else if (key == "--reverse-delay-high-cut-hz") {
            options.reverse_delay.high_cut_hz = value;
            options.reverse_delay_enabled = true;
        }
        else if (key == "--reverse-delay-forward-level") {
            options.reverse_delay.forward_level = value;
            options.reverse_delay_enabled = true;
        }
        else if (key == "--ducking-delay-time-ms") {
            options.ducking_delay.time_ms = value;
            options.ducking_delay_enabled = true;
        }
        else if (key == "--ducking-delay-feedback") {
            options.ducking_delay.feedback = value;
            options.ducking_delay_enabled = true;
        }
        else if (key == "--ducking-delay-mix") {
            options.ducking_delay.mix = value;
            options.ducking_delay_enabled = true;
        }
        else if (key == "--ducking-delay-threshold-db") {
            options.ducking_delay.threshold_db = value;
            options.ducking_delay_enabled = true;
        }
        else if (key == "--ducking-delay-attenuation-db") {
            options.ducking_delay.attenuation_db = value;
            options.ducking_delay_enabled = true;
        }
        else if (key == "--ducking-delay-release-ms") {
            options.ducking_delay.release_ms = value;
            options.ducking_delay_enabled = true;
        }
        else if (key == "--modulated-delay-time-ms") {
            options.modulated_delay.time_ms = value;
            options.modulated_delay_enabled = true;
        }
        else if (key == "--modulated-delay-feedback") {
            options.modulated_delay.feedback = value;
            options.modulated_delay_enabled = true;
        }
        else if (key == "--modulated-delay-mix") {
            options.modulated_delay.mix = value;
            options.modulated_delay_enabled = true;
        }
        else if (key == "--modulated-delay-rate-hz") {
            options.modulated_delay.rate_hz = value;
            options.modulated_delay_enabled = true;
        }
        else if (key == "--modulated-delay-depth-ms") {
            options.modulated_delay.depth_ms = value;
            options.modulated_delay_enabled = true;
        }
        else if (key == "--modulated-delay-spread-degrees") {
            options.modulated_delay.stereo_spread_degrees = value;
            options.modulated_delay_enabled = true;
        }
        else if (key == "--modulated-delay-trigger-threshold-db") {
            options.modulated_delay.trigger_threshold_db = value;
            options.modulated_delay_enabled = true;
        }
        else if (key == "--filtered-delay-time-ms") {
            options.filtered_delay.time_ms = value;
            options.filtered_delay_enabled = true;
        }
        else if (key == "--filtered-delay-feedback") {
            options.filtered_delay.feedback = value;
            options.filtered_delay_enabled = true;
        }
        else if (key == "--filtered-delay-mix") {
            options.filtered_delay.mix = value;
            options.filtered_delay_enabled = true;
        }
        else if (key == "--filtered-delay-low-cut-hz") {
            options.filtered_delay.low_cut_hz = value;
            options.filtered_delay_enabled = true;
        }
        else if (key == "--filtered-delay-high-cut-hz") {
            options.filtered_delay.high_cut_hz = value;
            options.filtered_delay_enabled = true;
        }
        else if (key == "--resonant-delay-time-ms") {
            options.resonant_delay.time_ms = value;
            options.resonant_delay_enabled = true;
        }
        else if (key == "--resonant-delay-feedback") {
            options.resonant_delay.feedback = value;
            options.resonant_delay_enabled = true;
        }
        else if (key == "--resonant-delay-center-hz") {
            options.resonant_delay.center_hz = value;
            options.resonant_delay_enabled = true;
        }
        else if (key == "--resonant-delay-q") {
            options.resonant_delay.resonance_q = value;
            options.resonant_delay_enabled = true;
        }
        else if (key == "--resonant-delay-mix") {
            options.resonant_delay.mix = value;
            options.resonant_delay_enabled = true;
        }
        else if (key == "--sweep-delay-time-ms") {
            options.sweep_delay.time_ms = value;
            options.sweep_delay_enabled = true;
        }
        else if (key == "--sweep-delay-feedback") {
            options.sweep_delay.feedback = value;
            options.sweep_delay_enabled = true;
        }
        else if (key == "--sweep-delay-mix") {
            options.sweep_delay.mix = value;
            options.sweep_delay_enabled = true;
        }
        else if (key == "--sweep-delay-start-hz") {
            options.sweep_delay.start_hz = value;
            options.sweep_delay_enabled = true;
        }
        else if (key == "--sweep-delay-stop-hz") {
            options.sweep_delay.stop_hz = value;
            options.sweep_delay_enabled = true;
        }
        else if (key == "--sweep-delay-resonance-q") {
            options.sweep_delay.resonance_q = value;
            options.sweep_delay_enabled = true;
        }
        else if (key == "--sweep-delay-rate-hz") {
            options.sweep_delay.rate_hz = value;
            options.sweep_delay_enabled = true;
        }
        else if (key == "--sweep-delay-phase-degrees") {
            options.sweep_delay.phase_degrees = value;
            options.sweep_delay_enabled = true;
        }
        else if (key == "--rotary-rate-hz") {
            options.rotary_speaker.rate_hz = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-tempo-bpm") {
            options.rotary_speaker.tempo_bpm = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-beats-per-rotation") {
            options.rotary_speaker.beats_per_rotation = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-slow-speed-multiplier") {
            options.rotary_speaker.slow_speed_multiplier = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-drive") {
            options.rotary_speaker.drive = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-hi-depth") {
            options.rotary_speaker.hi_depth = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-high-level") {
            options.rotary_speaker.high_level = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-low-depth") {
            options.rotary_speaker.low_depth = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-rotor-length") {
            options.rotary_speaker.rotor_length = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-low-rate-multi") {
            options.rotary_speaker.low_rate_multi = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-low-time-constant-ms") {
            options.rotary_speaker.low_time_constant_ms = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-hi-time-constant-ms") {
            options.rotary_speaker.hi_time_constant_ms = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-low-mic-spacing") {
            options.rotary_speaker.low_mic_spacing = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-high-mic-spacing") {
            options.rotary_speaker.high_mic_spacing = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-mic-distance") {
            options.rotary_speaker.mic_distance = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-stereo-spread") {
            options.rotary_speaker.stereo_spread = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-mix") {
            options.rotary_speaker.mix = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-level") {
            options.rotary_speaker.level = value;
            options.rotary_speaker_enabled = true;
        }
        else if (key == "--rotary-balance") {
            options.rotary_speaker.balance = value;
            options.rotary_speaker_enabled = true;
        }
        else if (set_graphic_eq_band_gain(key, "--guitar-eq-band",
                                          options.guitar_graphic_eq_gains,
                                          options.guitar_graphic_eq_enabled, value)) {}
        else if (set_graphic_eq_band_gain(key, "--bass-eq-band",
                                          options.bass_graphic_eq_gains,
                                          options.bass_graphic_eq_enabled, value)) {}
        else if (set_five_band_eq_parameter(key, options, value)) {}
        else throw std::invalid_argument("Unknown option: " + std::string(key));
    }
    if (options.pitch_arpeggiator_enabled &&
        !options.pitch_arpeggiator_run_explicit) {
        options.pitch_arpeggiator.run = true;
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
    if (options.delay_tape_heads_enabled || options.delay_multitap_enabled || options.delay_dual_enabled ||
        options.delay_bbd_style || options.delay_tape_enabled ||
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
        delay->configure_tape_echo(options.delay_tape_enabled,
                                   options.delay_tape_saturation,
                                   options.delay_tape_wow_depth_ms,
                                   options.delay_tape_wow_rate_hz,
                                   options.delay_tape_flutter_depth_ms,
                                   options.delay_tape_flutter_rate_hz);
        if (options.delay_tape_heads_enabled) {
            delay->configure_tape_heads(true, options.delay_tape_head_spacing_ms,
                                        options.delay_tape_head_mask);
            delay->reset();
        } else if (options.delay_multitap_enabled) {
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
    // Keep this suffix aligned with the stable effect-slot order in the WASM chain.
    if (options.optical_compressor_enabled) {
        effects.push_back(std::make_unique<tabletfx::OpticalCompressor>(
            static_cast<float>(sample_rate), options.optical_compressor));
    }
    if (options.fet_compressor_enabled) {
        effects.push_back(std::make_unique<tabletfx::FetCompressor>(
            static_cast<float>(sample_rate), options.fet_compressor));
    }
    if (options.guitar_graphic_eq_enabled) {
        auto eq = std::make_unique<tabletfx::GuitarGraphicEQ>(
            static_cast<float>(sample_rate), channels, options.guitar_graphic_eq_layout);
        for (std::size_t band = 0; band < options.guitar_graphic_eq_gains.size(); ++band) {
            const float gain_db = options.guitar_graphic_eq_gains[band];
            if (gain_db != 0.0F && !eq->set_band_gain_db(band, gain_db)) {
                throw std::invalid_argument("Selected guitar graphic EQ layout does not include band " +
                                            std::to_string(band + 1));
            }
        }
        effects.push_back(std::move(eq));
    }
    if (options.bass_graphic_eq_enabled) {
        auto eq = std::make_unique<tabletfx::BassGraphicEQ>(
            static_cast<float>(sample_rate), channels, options.bass_graphic_eq_layout);
        for (std::size_t band = 0; band < options.bass_graphic_eq_gains.size(); ++band) {
            const float gain_db = options.bass_graphic_eq_gains[band];
            if (gain_db != 0.0F && !eq->set_band_gain_db(band, gain_db)) {
                throw std::invalid_argument("Selected bass graphic EQ layout does not include band " +
                                            std::to_string(band + 1));
            }
        }
        effects.push_back(std::move(eq));
    }
    if (options.shelf_tilt_eq_enabled) {
        effects.push_back(std::make_unique<tabletfx::ShelfTiltEQ>(
            static_cast<float>(sample_rate), channels, options.shelf_tilt_eq_mode,
            options.shelf_tilt_eq_frequency_hz, options.shelf_tilt_eq_gain_db));
    }
    if (options.room_reverb_enabled) {
        effects.push_back(std::make_unique<tabletfx::RoomReverb>(
            static_cast<float>(sample_rate), channels, options.room_reverb));
    }
    if (options.hall_reverb_enabled) {
        effects.push_back(std::make_unique<tabletfx::HallReverb>(
            static_cast<float>(sample_rate), channels, options.hall_reverb));
    }
    if (options.plate_reverb_enabled) {
        effects.push_back(std::make_unique<tabletfx::PlateReverb>(
            static_cast<float>(sample_rate), channels, options.plate_reverb));
    }
    if (options.spring_reverb_enabled) {
        effects.push_back(std::make_unique<tabletfx::SpringReverb>(
            static_cast<float>(sample_rate), channels, options.spring_reverb));
    }
    if (options.auto_swell_enabled) {
        effects.push_back(std::make_unique<tabletfx::AutoSwell>(
            static_cast<float>(sample_rate), options.auto_swell));
    }
    if (options.rotary_speaker_enabled) {
        effects.push_back(std::make_unique<tabletfx::RotarySpeaker>(
            static_cast<float>(sample_rate), channels, options.rotary_speaker));
    }
    if (options.ring_modulator_enabled) {
        effects.push_back(std::make_unique<tabletfx::RingModulator>(
            static_cast<float>(sample_rate), channels, options.ring_modulator));
    }
    if (options.rhythmic_slicer_enabled) {
        effects.push_back(std::make_unique<tabletfx::RhythmicSlicer>(
            static_cast<float>(sample_rate), options.rhythmic_slicer));
    }
    if (options.step_filter_enabled) {
        effects.push_back(std::make_unique<tabletfx::StepFilter>(
            static_cast<float>(sample_rate), options.step_filter));
    }
    if (options.notch_filter_enabled) {
        effects.push_back(std::make_unique<tabletfx::NotchFilter>(
            static_cast<float>(sample_rate), options.notch_filter));
    }
    if (options.reverse_delay_enabled) {
        auto reverse_delay = std::make_unique<tabletfx::ReverseDelay>(
            static_cast<float>(sample_rate), channels, 2000.0F,
            options.reverse_delay);
        reverse_delay->set_running(options.reverse_delay_running);
        effects.push_back(std::move(reverse_delay));
    }
    if (options.ducking_delay_enabled) {
        effects.push_back(std::make_unique<tabletfx::DuckingDelay>(
            static_cast<float>(sample_rate), channels, 2000.0F,
            options.ducking_delay));
    }
    if (options.modulated_delay_enabled) {
        effects.push_back(std::make_unique<tabletfx::ModulatedDelay>(
            static_cast<float>(sample_rate), channels, 2000.0F,
            options.modulated_delay));
    }
    if (options.filtered_delay_enabled) {
        effects.push_back(std::make_unique<tabletfx::FilteredDelay>(
            static_cast<float>(sample_rate), channels, 2000.0F,
            options.filtered_delay));
    }
    if (options.resonant_delay_enabled) {
        effects.push_back(std::make_unique<tabletfx::ResonantDelay>(
            static_cast<float>(sample_rate), channels, 2000.0F,
            options.resonant_delay));
    }
    if (options.sweep_delay_enabled) {
        effects.push_back(std::make_unique<tabletfx::SweepDelay>(
            static_cast<float>(sample_rate), channels, 2000.0F,
            options.sweep_delay));
    }
    if (options.formant_filter_enabled) {
        effects.push_back(std::make_unique<tabletfx::FormantFilter>(
            static_cast<float>(sample_rate), options.formant_filter));
    }
    if (options.harmonic_resonator_enabled) {
        effects.push_back(std::make_unique<tabletfx::HarmonicResonator>(
            static_cast<float>(sample_rate), options.harmonic_resonator));
    }
    if (options.multiband_compressor_enabled) {
        effects.push_back(std::make_unique<tabletfx::MultibandCompressor>(
            static_cast<float>(sample_rate), options.multiband_compressor));
    }
    if (options.bit_crusher_enabled) {
        effects.push_back(std::make_unique<tabletfx::BitCrusher>(
            static_cast<float>(sample_rate), channels, options.bit_crusher));
    }
    if (options.dimension_chorus_enabled) {
        effects.push_back(std::make_unique<tabletfx::DimensionChorus>(
            static_cast<float>(sample_rate), channels, options.dimension_chorus));
    }
    if (options.triple_double_chorus_enabled) {
        auto chorus = std::make_unique<tabletfx::effects::TripleDoubleChorus>(
            static_cast<float>(sample_rate));
        chorus->configure(options.triple_double_chorus);
        effects.push_back(std::move(chorus));
    }
    if (options.four_band_resonator_enabled) {
        effects.push_back(std::make_unique<tabletfx::FourBandResonator>(
            static_cast<float>(sample_rate), options.four_band_resonator));
    }
    if (options.diatonic_harmonizer_enabled) {
        auto harmonizer = std::make_unique<tabletfx::effects::DiatonicHarmonizer>(
            static_cast<float>(sample_rate));
        harmonizer->configure(options.diatonic_harmonizer);
        effects.push_back(std::move(harmonizer));
    }
    if (options.pitch_arpeggiator_enabled) {
        auto arpeggiator = std::make_unique<tabletfx::effects::PitchArpeggiator>(
            static_cast<float>(sample_rate), options.pitch_arpeggiator);
        effects.push_back(std::move(arpeggiator));
    }
    if (options.mono_guitar_synth_enabled) {
        effects.push_back(std::make_unique<tabletfx::effects::MonoGuitarSynth>(
            static_cast<float>(sample_rate), options.mono_guitar_synth));
    }
    if (options.note_generator_enabled) {
        effects.push_back(std::make_unique<tabletfx::effects::NoteGenerator>(
            static_cast<float>(sample_rate), options.note_generator));
    }
    if (options.enhancer_enabled) {
        effects.push_back(std::make_unique<tabletfx::Enhancer>(
            static_cast<float>(sample_rate), channels, options.enhancer));
    }
    if (options.grain_delay_enabled) {
        effects.push_back(std::make_unique<tabletfx::GrainDelay>(
            static_cast<float>(sample_rate), channels, options.grain_delay));
    }
    if (options.pitch_feedback_delay_enabled) {
        effects.push_back(std::make_unique<tabletfx::PitchFeedbackDelay>(
            static_cast<float>(sample_rate), channels, 6000.0F,
            options.pitch_feedback_delay));
    }
    if (options.feedback_generator_enabled) {
        effects.push_back(std::make_unique<tabletfx::effects::FeedbackGenerator>(
            static_cast<float>(sample_rate), channels,
            options.feedback_generator));
    }
    if (options.shimmer_reverb_enabled) {
        effects.push_back(std::make_unique<tabletfx::ShimmerReverb>(
            static_cast<float>(sample_rate), channels, options.shimmer_reverb));
    }
    if (options.convolution_reverb_enabled) {
        if (options.convolution_ir_path.empty()) {
            throw std::invalid_argument(
                "Convolution Reverb requires --convolution-ir INPUT.wav");
        }
        auto convolution = std::make_unique<tabletfx::ConvolutionReverb>(
            static_cast<float>(sample_rate), channels, options.convolution_reverb);
        const tabletfx::AudioBuffer ir =
            tabletfx::read_wav(options.convolution_ir_path);
        if (!convolution->load_impulse_response(
                ir.interleaved, ir.channels, ir.sample_rate)) {
            throw std::invalid_argument(
                "Convolution IR must be finite mono/stereo WAV and fit the 24000-frame target-rate limit");
        }
        effects.push_back(std::move(convolution));
    }
    if (options.five_band_eq_enabled) {
        auto eq = std::make_unique<tabletfx::FiveBandParametricEQ>(
            static_cast<float>(sample_rate), channels);
        eq->configure(options.five_band_eq);
        effects.push_back(std::move(eq));
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

std::vector<float> make_convolution_benchmark_ir(std::uint32_t sample_rate,
                                                 std::uint16_t channels) {
    std::vector<float> impulse(
        tabletfx::ConvolutionReverb::maximum_impulse_frames() * channels, 0.0F);
    for (std::size_t frame = 0;
         frame < tabletfx::ConvolutionReverb::maximum_impulse_frames(); ++frame) {
        const float time = static_cast<float>(frame) /
                           static_cast<float>(sample_rate);
        const float decay = std::exp(-time / 0.08F);
        const float sample = frame == 0
            ? 0.8F
            : 0.12F * decay * std::sin(
                  2.0F * std::numbers::pi_v<float> * 1300.0F * time);
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            impulse[frame * channels + channel] = sample;
        }
    }
    return impulse;
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
    tabletfx::effects::TripleDoubleChorusConfig triple_double_parameters;
    auto triple_double = std::make_unique<tabletfx::effects::TripleDoubleChorus>(
        static_cast<float>(sample_rate));
    triple_double->configure(triple_double_parameters);
    effects.push_back(std::move(triple_double));
    effects.push_back(std::make_unique<tabletfx::FourBandResonator>(
        static_cast<float>(sample_rate)));
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
    tabletfx::effects::PitchArpeggiatorParameters arpeggiator_parameters;
    arpeggiator_parameters.run = true;
    arpeggiator_parameters.steps = 8;
    arpeggiator_parameters.mix = 0.5F;
    effects.push_back(std::make_unique<tabletfx::effects::PitchArpeggiator>(
        static_cast<float>(sample_rate), arpeggiator_parameters));
    tabletfx::effects::FeedbackGeneratorParameters feedback_generator_parameters;
    effects.push_back(std::make_unique<tabletfx::effects::FeedbackGenerator>(
        static_cast<float>(sample_rate), channels, feedback_generator_parameters));
    tabletfx::OpticalCompressorParameters optical_parameters;
    optical_parameters.threshold_db = -24.0F;
    optical_parameters.ratio = 3.0F;
    effects.push_back(std::make_unique<tabletfx::OpticalCompressor>(
        static_cast<float>(sample_rate), optical_parameters));
    tabletfx::FetCompressorParameters fet_parameters;
    fet_parameters.threshold_db = -30.0F;
    fet_parameters.ratio = 5.0F;
    effects.push_back(std::make_unique<tabletfx::FetCompressor>(
        static_cast<float>(sample_rate), fet_parameters));
    auto guitar_eq = std::make_unique<tabletfx::GuitarGraphicEQ>(
        static_cast<float>(sample_rate), channels,
        tabletfx::GuitarGraphicEQLayout::SevenBand);
    static_cast<void>(guitar_eq->set_band_gain_db(0, 1.5F));
    static_cast<void>(guitar_eq->set_band_gain_db(1, -1.0F));
    static_cast<void>(guitar_eq->set_band_gain_db(3, 1.0F));
    static_cast<void>(guitar_eq->set_band_gain_db(5, -0.5F));
    static_cast<void>(guitar_eq->set_band_gain_db(6, 1.0F));
    effects.push_back(std::move(guitar_eq));
    auto bass_eq = std::make_unique<tabletfx::BassGraphicEQ>(
        static_cast<float>(sample_rate), channels,
        tabletfx::BassGraphicEQLayout::SevenBand);
    static_cast<void>(bass_eq->set_band_gain_db(0, -1.0F));
    static_cast<void>(bass_eq->set_band_gain_db(1, 0.5F));
    static_cast<void>(bass_eq->set_band_gain_db(2, 2.0F));
    static_cast<void>(bass_eq->set_band_gain_db(3, -2.0F));
    static_cast<void>(bass_eq->set_band_gain_db(4, 0.5F));
    static_cast<void>(bass_eq->set_band_gain_db(5, 1.0F));
    static_cast<void>(bass_eq->set_band_gain_db(6, -0.5F));
    effects.push_back(std::move(bass_eq));
    effects.push_back(std::make_unique<tabletfx::ShelfTiltEQ>(
        static_cast<float>(sample_rate), channels, tabletfx::ShelfTiltMode::Tilt,
        1200.0F, 2.0F));
    tabletfx::RoomReverbParameters room_reverb_parameters;
    room_reverb_parameters.size = tabletfx::RoomSize::large;
    room_reverb_parameters.decay_seconds = 1.2F;
    room_reverb_parameters.pre_delay_ms = 18.0F;
    room_reverb_parameters.damping_hz = 5000.0F;
    room_reverb_parameters.mix = 0.3F;
    effects.push_back(std::make_unique<tabletfx::RoomReverb>(
        static_cast<float>(sample_rate), channels, room_reverb_parameters));
    tabletfx::HallReverbParameters hall_reverb_parameters;
    hall_reverb_parameters.decay_seconds = 3.2F;
    hall_reverb_parameters.pre_delay_ms = 32.0F;
    hall_reverb_parameters.damping_hz = 9000.0F;
    hall_reverb_parameters.low_cut_hz = 100.0F;
    hall_reverb_parameters.mix = 0.25F;
    effects.push_back(std::make_unique<tabletfx::HallReverb>(
        static_cast<float>(sample_rate), channels, hall_reverb_parameters));
    tabletfx::PlateReverbParameters plate_reverb_parameters;
    effects.push_back(std::make_unique<tabletfx::PlateReverb>(
        static_cast<float>(sample_rate), channels, plate_reverb_parameters));
    effects.push_back(std::make_unique<tabletfx::SpringReverb>(
        static_cast<float>(sample_rate), channels));
    effects.push_back(std::make_unique<tabletfx::ShimmerReverb>(
        static_cast<float>(sample_rate), channels));
    auto convolution_reverb = std::make_unique<tabletfx::ConvolutionReverb>(
        static_cast<float>(sample_rate), channels);
    const std::vector<float> convolution_ir =
        make_convolution_benchmark_ir(sample_rate, channels);
    if (!convolution_reverb->load_impulse_response(
            convolution_ir, channels, sample_rate)) {
        throw std::runtime_error("Could not prepare the full-chain convolution IR");
    }
    effects.push_back(std::move(convolution_reverb));
    effects.push_back(std::make_unique<tabletfx::AutoSwell>(
        static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::RotarySpeaker>(
        static_cast<float>(sample_rate), channels));
    tabletfx::RingModulatorParameters ring_parameters;
    ring_parameters.frequency_hz = 713.0F;
    ring_parameters.frequency_multiplier = 1.75F;
    ring_parameters.high_cut_hz = 9000.0F;
    ring_parameters.mix = 0.83F;
    effects.push_back(std::make_unique<tabletfx::RingModulator>(
        static_cast<float>(sample_rate), channels, ring_parameters));
    tabletfx::RhythmicSlicerParameters slicer_parameters;
    slicer_parameters.depth = 0.4F;
    slicer_parameters.shape = 0.15F;
    slicer_parameters.attack_ms = 0.5F;
    slicer_parameters.release_ms = 1.0F;
    effects.push_back(std::make_unique<tabletfx::RhythmicSlicer>(
        static_cast<float>(sample_rate), slicer_parameters));
    effects.push_back(std::make_unique<tabletfx::StepFilter>(
        static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::NotchFilter>(
        static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::ReverseDelay>(
        static_cast<float>(sample_rate), channels));
    tabletfx::DuckingDelayParameters ducking_delay_parameters;
    ducking_delay_parameters.threshold_db = -30.0F;
    ducking_delay_parameters.attenuation_db = 12.0F;
    ducking_delay_parameters.release_ms = 250.0F;
    effects.push_back(std::make_unique<tabletfx::DuckingDelay>(
        static_cast<float>(sample_rate), channels, 2000.0F,
        ducking_delay_parameters));
    effects.push_back(std::make_unique<tabletfx::ModulatedDelay>(
        static_cast<float>(sample_rate), channels, 2000.0F));
    effects.push_back(std::make_unique<tabletfx::FilteredDelay>(
        static_cast<float>(sample_rate), channels, 2000.0F));
    effects.push_back(std::make_unique<tabletfx::SweepDelay>(
        static_cast<float>(sample_rate), channels, 2000.0F));
    effects.push_back(std::make_unique<tabletfx::FormantFilter>(
        static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::HarmonicResonator>(
        static_cast<float>(sample_rate)));
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
    benchmark_one("ducking-delay", [=] {
        tabletfx::DuckingDelayParameters parameters;
        parameters.threshold_db = -30.0F;
        parameters.attenuation_db = 18.0F;
        parameters.release_ms = 250.0F;
        return std::make_unique<tabletfx::DuckingDelay>(
            static_cast<float>(sample_rate), channels, 2000.0F, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("modulated-delay", [=] {
        return std::make_unique<tabletfx::ModulatedDelay>(
            static_cast<float>(sample_rate), channels, 2000.0F);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("filtered-delay", [=] {
        return std::make_unique<tabletfx::FilteredDelay>(
            static_cast<float>(sample_rate), channels, 2000.0F);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("sweep-delay", [=] {
        return std::make_unique<tabletfx::SweepDelay>(
            static_cast<float>(sample_rate), channels, 2000.0F);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("formant-filter", [=] {
        return std::make_unique<tabletfx::FormantFilter>(
            static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("harmonic-resonator", [=] {
        return std::make_unique<tabletfx::HarmonicResonator>(
            static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("multiband-compressor", [=] {
        return std::make_unique<tabletfx::MultibandCompressor>(
            static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("room-reverb", [=] {
        tabletfx::RoomReverbParameters parameters;
        parameters.size = tabletfx::RoomSize::large;
        parameters.decay_seconds = 1.2F;
        parameters.pre_delay_ms = 18.0F;
        parameters.damping_hz = 5000.0F;
        parameters.mix = 0.3F;
        return std::make_unique<tabletfx::RoomReverb>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("hall-reverb", [=] {
        tabletfx::HallReverbParameters parameters;
        parameters.decay_seconds = 3.2F;
        parameters.pre_delay_ms = 32.0F;
        parameters.damping_hz = 9000.0F;
        parameters.low_cut_hz = 100.0F;
        parameters.mix = 0.25F;
        return std::make_unique<tabletfx::HallReverb>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("plate-reverb", [=] {
        tabletfx::PlateReverbParameters parameters;
        return std::make_unique<tabletfx::PlateReverb>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("spring-reverb", [=] {
        return std::make_unique<tabletfx::SpringReverb>(
            static_cast<float>(sample_rate), channels);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("auto-swell", [=] {
        return std::make_unique<tabletfx::AutoSwell>(
            static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("rotary-speaker", [=] {
        tabletfx::RotarySpeakerParameters parameters;
        parameters.drive = 0.15F;
        return std::make_unique<tabletfx::RotarySpeaker>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("ring-modulator", [=] {
        tabletfx::RingModulatorParameters parameters;
        parameters.frequency_hz = 713.0F;
        parameters.frequency_multiplier = 1.75F;
        parameters.high_cut_hz = 9000.0F;
        parameters.mix = 0.83F;
        return std::make_unique<tabletfx::RingModulator>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("bit-crusher", [=] {
        tabletfx::BitCrusherParameters parameters;
        parameters.bit_depth = 8;
        parameters.sample_rate_divisor = 4;
        return std::make_unique<tabletfx::BitCrusher>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("dimension-chorus", [=] {
        tabletfx::DimensionChorusParameters parameters;
        parameters.version = tabletfx::DimensionChorusVersion::C;
        parameters.mode = tabletfx::DimensionChorusMode::Mode1;
        parameters.mix = 0.5F;
        parameters.level_db = 0.0F;
        return std::make_unique<tabletfx::DimensionChorus>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("triple-double-chorus", [=] {
        auto effect = std::make_unique<tabletfx::effects::TripleDoubleChorus>(
            static_cast<float>(sample_rate));
        effect->configure({});
        return effect;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("four-band-resonator", [=] {
        return std::make_unique<tabletfx::FourBandResonator>(
            static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("diatonic-harmonizer", [=] {
        auto effect = std::make_unique<tabletfx::effects::DiatonicHarmonizer>(
            static_cast<float>(sample_rate));
        tabletfx::effects::DiatonicHarmonizerConfig parameters;
        parameters.mix = 1.0F;
        parameters.pitch_interval = 2;
        effect->configure(parameters);
        return effect;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("pitch-arpeggiator", [=] {
        tabletfx::effects::PitchArpeggiatorParameters parameters;
        parameters.run = true;
        parameters.steps = 8;
        parameters.mix = 0.5F;
        return std::make_unique<tabletfx::effects::PitchArpeggiator>(
            static_cast<float>(sample_rate), parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("mono-guitar-synth", [=] {
        tabletfx::effects::MonoGuitarSynthConfig parameters;
        parameters.waveform = tabletfx::effects::MonoGuitarSynthWaveform::Sawtooth;
        parameters.mix = 0.75F;
        parameters.level = 0.6F;
        return std::make_unique<tabletfx::effects::MonoGuitarSynth>(
            static_cast<float>(sample_rate), parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("grain-delay-worst-case", [=] {
        tabletfx::GrainDelayParameters parameters;
        parameters.time_ms = 2000.0F;
        parameters.feedback = 0.7F;
        parameters.mix = 0.7F;
        parameters.grain_size_ms = 250.0F;
        parameters.overlap = 4U;
        parameters.pitch_semitones = -24.0F;
        parameters.spray_ms = 100.0F;
        return std::make_unique<tabletfx::GrainDelay>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("pitch-feedback-delay", [=] {
        tabletfx::PitchFeedbackDelayParameters parameters;
        parameters.time_ms = 350.0F;
        parameters.feedback = 0.35F;
        parameters.mix = 0.25F;
        parameters.pitch_semitones = 7.0F;
        return std::make_unique<tabletfx::PitchFeedbackDelay>(
            static_cast<float>(sample_rate), channels, 6000.0F, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("feedback-generator-unison", [=] {
        tabletfx::effects::FeedbackGeneratorParameters parameters;
        parameters.type = tabletfx::effects::FeedbackGeneratorType::Unison;
        return std::make_unique<tabletfx::effects::FeedbackGenerator>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("shimmer-reverb", [=] {
        tabletfx::ShimmerReverbParameters parameters;
        parameters.decay_seconds = 3.4F;
        parameters.damping_hz = 7200.0F;
        parameters.diffusion = 0.72F;
        parameters.feedback = 0.35F;
        parameters.high_pass_hz = 120.0F;
        parameters.pitch_semitones = 7.0F;
        parameters.shimmer_intensity = 0.65F;
        parameters.mix = 0.3F;
        parameters.level_db = -3.0F;
        return std::make_unique<tabletfx::ShimmerReverb>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("convolution-reverb-max-0.5s", [=] {
        auto effect = std::make_unique<tabletfx::ConvolutionReverb>(
            static_cast<float>(sample_rate), channels);
        const std::vector<float> impulse =
            make_convolution_benchmark_ir(sample_rate, channels);
        if (!effect->load_impulse_response(impulse, channels, sample_rate)) {
            throw std::runtime_error("Could not prepare the convolution benchmark IR");
        }
        return effect;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("rhythmic-slicer", [=] {
        tabletfx::RhythmicSlicerParameters parameters;
        parameters.depth = 0.5F;
        return std::make_unique<tabletfx::RhythmicSlicer>(
            static_cast<float>(sample_rate), parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("step-filter", [=] {
        tabletfx::StepFilterParameters parameters;
        return std::make_unique<tabletfx::StepFilter>(
            static_cast<float>(sample_rate), parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("notch-filter", [=] {
        return std::make_unique<tabletfx::NotchFilter>(
            static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("reverse-delay", [=] {
        return std::make_unique<tabletfx::ReverseDelay>(
            static_cast<float>(sample_rate), channels);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
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
    benchmark_one("delay-tape-mono", [=] {
        auto delay = std::make_unique<tabletfx::Delay>(sample_rate, 1, 2000.0F);
        delay->configure_tape_echo(true, 0.45F, 0.35F, 0.3F, 0.08F, 6.0F);
        return delay;
    }, sample_rate, 1, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay-tape-stereo", [=] {
        auto delay = std::make_unique<tabletfx::Delay>(sample_rate, 2, 2000.0F);
        delay->configure_tape_echo(true, 0.45F, 0.35F, 0.3F, 0.08F, 6.0F);
        return delay;
    }, sample_rate, 2, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay-tape-heads", [=] {
        auto delay = std::make_unique<tabletfx::Delay>(sample_rate, channels, 2000.0F);
        delay->configure_tape_heads(true, 110.0F, 0x0FU);
        delay->reset();
        return delay;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
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
    benchmark_one("optical-compressor", [=] {
        tabletfx::OpticalCompressorParameters parameters;
        parameters.threshold_db = -24.0F;
        parameters.ratio = 3.0F;
        return std::make_unique<tabletfx::OpticalCompressor>(
            static_cast<float>(sample_rate), parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("fet-compressor", [=] {
        tabletfx::FetCompressorParameters parameters;
        parameters.threshold_db = -30.0F;
        parameters.ratio = 5.0F;
        return std::make_unique<tabletfx::FetCompressor>(
            static_cast<float>(sample_rate), parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("three-band-parametric-eq", [=] {
        auto effect = std::make_unique<tabletfx::ThreeBandParametricEQ>(sample_rate, channels);
        effect->configure(120.0F, 3.0F, 0.707F, 800.0F, -2.0F, 0.8F,
                          5000.0F, 4.0F, 0.707F);
        return effect;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("five-band-parametric-eq", [=] {
        auto effect = std::make_unique<tabletfx::FiveBandParametricEQ>(
            static_cast<float>(sample_rate), channels);
        auto bands = effect->parameters();
        bands[0].gain_db = 2.0F;
        bands[2].gain_db = -3.0F;
        bands[4].gain_db = 1.5F;
        effect->configure(bands);
        return effect;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("guitar-graphic-eq", [=] {
        auto effect = std::make_unique<tabletfx::GuitarGraphicEQ>(
            static_cast<float>(sample_rate), channels,
            tabletfx::GuitarGraphicEQLayout::SevenBand);
        static_cast<void>(effect->set_band_gain_db(0, 3.0F));
        static_cast<void>(effect->set_band_gain_db(3, -2.0F));
        static_cast<void>(effect->set_band_gain_db(6, 1.5F));
        return effect;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("bass-graphic-eq", [=] {
        auto effect = std::make_unique<tabletfx::BassGraphicEQ>(
            static_cast<float>(sample_rate), channels,
            tabletfx::BassGraphicEQLayout::SevenBand);
        static_cast<void>(effect->set_band_gain_db(0, -2.0F));
        static_cast<void>(effect->set_band_gain_db(3, 3.0F));
        static_cast<void>(effect->set_band_gain_db(6, 1.5F));
        return effect;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("shelf-tilt-eq", [=] {
        return std::make_unique<tabletfx::ShelfTiltEQ>(
            static_cast<float>(sample_rate), channels,
            tabletfx::ShelfTiltMode::Tilt, 1200.0F, 4.0F);
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
                  " [--convolution-ir IR.wav] [--convolution-reverb-mix 0..1] [--convolution-reverb-level-db dB]"
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
                  " [--dimension-chorus] [--dimension-chorus-version C|D]"
                  " [--dimension-chorus-mode 1|2|3|4|1+2|1+3|1+4|2+3|2+4|3+4]"
                  " [--dimension-chorus-mix 0..1] [--dimension-chorus-level-db dB]"
                   " [--triple-double-chorus] [--triple-double-chorus-preset-switch on|off]"
                   " [--triple-double-chorus-manual-switch on|off]"
                   " [--triple-double-chorus-rate-hz Hz]"
                   " [--triple-double-chorus-left-depth-ms ms]"
                   " [--triple-double-chorus-center-depth-ms ms]"
                   " [--triple-double-chorus-right-depth-ms ms]"
                   " [--triple-double-chorus-feedback -0.8..0.8]"
                   " [--triple-double-chorus-sparkle 0..1]"
                   " [--triple-double-chorus-stereo-switch on|off]"
                   " [--four-band-resonator] [--four-band-resonator-mode manual|chord]"
                   " [--four-band-resonator-chord major|major6|dominant7|major7|minor|minor6|minor7|diminished|augmented]"
                   " [--four-band-resonator-master-frequency 0.1..10]"
                   " [--four-band-resonator-frequencies-hz f1,f2,f3,f4]"
                   " [--four-band-resonator-frequency-hz 50..5000]"
                   " [--four-band-resonator-feedback f1,f2,f3,f4]"
                   " [--four-band-resonator-q q1,q2,q3,q4]"
                   " [--four-band-resonator-levels l1,l2,l3,l4]"
                   " [--four-band-resonator-pans p1,p2,p3,p4]"
                   " [--four-band-resonator-filter-locations pre,post,pre,post]"
                   " [--four-band-resonator-input-mode mono|stereo]"
                   " [--four-band-resonator-master-level value] [--four-band-resonator-master-pan -1..1]"
                   " [--four-band-resonator-master-feedback 0..1] [--four-band-resonator-master-q 0.1..4]"
                   " [--four-band-resonator-mix 0..1]"
                    " [--diatonic-harmonizer] [--diatonic-harmonizer-mix 0..1]"
                    " [--diatonic-harmonizer-interval -7..7] [--diatonic-harmonizer-key 0..11]"
                    " [--diatonic-harmonizer-scale major|natural-minor|dorian|phrygian|lydian|mixolydian|locrian|harmonic-minor|melodic-minor|major-pentatonic|minor-pentatonic|blues|chromatic]"
                    " [--diatonic-harmonizer-tone-hz Hz] [--diatonic-harmonizer-delay-ms ms]"
                    " [--diatonic-harmonizer-feedback 0..0.85]"
                    " [--diatonic-harmonizer-tap-division free|half|quarter|dotted-quarter|quarter-triplet|eighth|dotted-eighth|eighth-triplet|sixteenth]"
                    " [--diatonic-harmonizer-tempo-bpm BPM]"
                  " [--pitch-arpeggiator] [--pitch-arpeggiator-run on|off]"
                  " [--pitch-arpeggiator-mix 0..1] [--pitch-arpeggiator-output-level 0..2]"
                  " [--pitch-arpeggiator-trigger-threshold-dbfs -90..0]"
                  " [--pitch-arpeggiator-key 0..11] [--pitch-arpeggiator-scale major|natural-minor|dorian|phrygian|lydian|mixolydian|locrian|harmonic-minor|melodic-minor|major-pentatonic|minor-pentatonic|blues|chromatic]"
                  " [--pitch-arpeggiator-steps 1..16] [--pitch-arpeggiator-repeats 0..64]"
                  " [--pitch-arpeggiator-division free|whole|half|dotted-half|quarter|dotted-quarter|quarter-triplet|eighth|dotted-eighth|eighth-triplet|sixteenth|dotted-sixteenth|sixteenth-triplet|thirty-second]"
                  " [--pitch-arpeggiator-tempo-bpm BPM] [--pitch-arpeggiator-step-ms 10..2000]"
                  " [--pitch-arpeggiator-shifts 16 comma-separated shifts (-24..24 degrees)]"
                  " [--pitch-arpeggiator-amplitude-shape constant|ramp-up|ramp-down|triangle]"
                  " [--pitch-arpeggiator-amplitude-alpha 0..1]"
                  " [--pitch-arpeggiator-pan-shape center|left-to-right|right-to-left|ping-pong]"
                  " [--pitch-arpeggiator-pan-alpha 0..1]"
                  " [--mono-guitar-synth] [--mono-guitar-synth-waveform sine|triangle|sawtooth|square]"
                  " [--mono-guitar-synth-mix 0..1] [--mono-guitar-synth-level 0..2]"
                  " [--mono-guitar-synth-gate-threshold-db -90..0]"
                  " [--mono-guitar-synth-attack-ms 0..2000] [--mono-guitar-synth-release-ms 0..5000]"
                  " [--mono-guitar-synth-portamento-ms 0..250]"
                  " [--enhancer]"
                  " [--feedback-generator] [--feedback-generator-type octave-down|unison|octave-up]"
                  " [--feedback-generator-gain 0..1] [--feedback-generator-attack-ms 0..2000]"
                  " [--feedback-generator-release-ms 0..5000] [--feedback-generator-dry-kill off|on]"
                  " [--feedback-generator-dry-level 0..1] [--feedback-generator-silence-threshold-db -90..0]"
                   " [--shimmer-reverb] [--shimmer-reverb-decay-seconds 0.2..12]"
                   " [--shimmer-reverb-damping-hz 200..18000] [--shimmer-reverb-diffusion 0..1]"
                   " [--shimmer-reverb-feedback 0..0.65] [--shimmer-reverb-high-pass-hz 20..1500]"
                   " [--shimmer-reverb-pitch-semitones 1..12] [--shimmer-reverb-intensity 0..1]"
                   " [--shimmer-reverb-mix 0..1] [--shimmer-reverb-level-db -24..12]"
                  " [--grain-delay] [--grain-delay-time-ms 20..2000]"
                  " [--grain-delay-feedback 0..0.9] [--grain-delay-mix 0..1]"
                  " [--grain-delay-grain-size-ms 10..250] [--grain-delay-overlap 1..4]"
                  " [--grain-delay-pitch-semitones -24..24] [--grain-delay-spray-ms 0..100]"
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
                   " [--drive 1..20] [--delay-preset digital|slapback|bbd|tape|tape-heads] [--delay-bbd|--delay-tape|--delay-tape-heads] [--delay-head-spacing-ms ms] [--delay-heads all|none|1,2,3,4] [--delay-ms ms] [--delay-tape-saturation 0..1] [--delay-tape-wow-depth-ms 0..5] [--delay-tape-wow-rate-hz 0.05..2] [--delay-tape-flutter-depth-ms 0..1] [--delay-tape-flutter-rate-hz 1..20] [--delay-sync-bpm 20..300]"
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
              << "                 [--optical-compressor] [--optical-compressor-threshold-db dB]"
                 " [--optical-compressor-ratio 1..20] [--optical-compressor-knee-db dB]"
                 " [--optical-compressor-attack-ms ms] [--optical-compressor-release-ms ms]"
                 " [--optical-compressor-program-release-ms ms] [--optical-compressor-rms-window-ms ms]"
                 " [--optical-compressor-makeup-db dB]\n"
              << "                 [--fet-compressor] [--fet-compressor-threshold-db dB]"
                 " [--fet-compressor-ratio 1..20] [--fet-compressor-knee-db dB]"
                 " [--fet-compressor-attack-ms ms] [--fet-compressor-release-ms ms]"
                 " [--fet-compressor-makeup-db dB]\n"
              << "                 [--eq-bandN-frequency-hz Hz] [--eq-bandN-gain-db dB] [--eq-bandN-q Q] (N=1..3)\n"
               << "                 [--five-band-eq-bandN-frequency-hz Hz] [--five-band-eq-bandN-gain-db dB] [--five-band-eq-bandN-q Q] (N=1..5)\n"
              << "                 [--guitar-graphic-eq] [--guitar-eq-layout 5|7] [--guitar-eq-bandN-gain-db dB] (N=1..7)\n"
              << "                 [--bass-graphic-eq] [--bass-eq-layout 4|7] [--bass-eq-bandN-gain-db dB] (N=1..7)\n"
              << "                 [--shelf-tilt-eq] [--shelf-tilt-mode low-shelf|high-shelf|tilt]"
                 " [--shelf-tilt-frequency-hz Hz] [--shelf-tilt-gain-db dB]\n"
              << "                 [--room-reverb] [--room-size small|large] [--room-decay-s 0.15..8]"
                 " [--room-pre-delay-ms 0..80] [--room-damping-hz Hz] [--room-mix 0..1]"
                 " [--room-level-db -24..12]\n"
              << "                 [--hall-reverb] [--hall-decay-s 0.5..12]"
                 " [--hall-pre-delay-ms 0..120] [--hall-damping-hz Hz]"
                 " [--hall-low-cut-hz 20..1500] [--hall-mix 0..1]"
                 " [--hall-level-db -24..12]\n"
              << "                 [--plate-reverb] [--plate-decay-s 0.2..12]"
                 " [--plate-pre-delay-ms 0..120] [--plate-damping-hz Hz]"
                 " [--plate-diffusion 0..1] [--plate-balance 0..1]"
                 " [--plate-mix 0..1] [--plate-level-db -24..12]\n"
               << "                 [--spring-reverb] [--spring-decay-s 0.2..8]"
                  " [--spring-pre-delay-ms 0..120] [--spring-damping-hz Hz]"
                  " [--spring-dwell 0..1] [--spring-mix 0..1]"
                  " [--spring-level-db -24..12]\n"
               << "                 [--auto-swell] [--auto-swell-threshold-db -90..0]"
                  " [--auto-swell-attack-ms 1..5000] [--auto-swell-release-ms 1..5000]"
                  " [--auto-swell-hysteresis-db 0..36]"
                  " [--auto-swell-taper linear|log30a|log20a|log15a|log10a|log5a|s-taper]"
                  " [--auto-swell-volume 0..2]\n"
                << "                 [--rotary-speaker] [--rotary-rate-hz 0.1..10]"
                   " [--rotary-tempo-sync on|off] [--rotary-tempo-bpm 20..300]"
                   " [--rotary-beats-per-rotation 0.125..8] [--rotary-speed slow|fast]"
                   " [--rotary-slow-speed-multiplier 0..1] [--rotary-drive 0..1]"
                   " [--rotary-hi-depth 0..1] [--rotary-high-level 0..2]"
                   " [--rotary-low-depth 0..1] [--rotary-rotor-length 0..1]"
                   " [--rotary-low-rate-multi 0..1] [--rotary-low-time-constant-ms ms]"
                    " [--rotary-hi-time-constant-ms ms]"
                    " [--rotary-low-mic-spacing 0..1] [--rotary-high-mic-spacing 0..1]"
                   " [--rotary-mic-distance 0..1] [--rotary-stereo-spread -2..2]"
                   " [--rotary-mix 0..1] [--rotary-level 0..2] [--rotary-balance -1..1]"
                   " [--rotary-input-select stereo|left|right|sum]\n"
               << "                 [--ring-mod] [--ring-mod-frequency-hz 2..2000]"
                  " [--ring-mod-multiplier 0.25..4] [--ring-mod-high-cut-hz 20..20000]"
                  " [--ring-mod-mix 0..1]\n"
                 << "                 [--bit-crusher] [--bit-crusher-bit-depth 2..24]"
                    " [--bit-crusher-sample-rate-divisor 1..128]"
                    " [--bit-crusher-mix 0..1]\n"
                << "                 [--rhythmic-slicer] [--rhythmic-slicer-tempo-sync on|off]"
                   " [--rhythmic-slicer-tempo-bpm 30..300]"
                   " [--rhythmic-slicer-division whole|half|quarter|dotted-quarter|quarter-triplet|eighth|dotted-eighth|eighth-triplet|sixteenth|dotted-sixteenth|sixteenth-triplet|thirty-second|thirty-second-triplet]"
                   " [--rhythmic-slicer-rate-hz 0.05..60] [--rhythmic-slicer-depth 0..1]"
                   " [--rhythmic-slicer-shape 0..1] [--rhythmic-slicer-attack-ms 0..100]"
                   " [--rhythmic-slicer-release-ms 0..100] [--rhythmic-slicer-phase 0..1]"
                   " [--rhythmic-slicer-steps 16 comma-separated values in 0..1]\n"
                << "                 [--step-filter] [--step-filter-mode lowpass|bandpass|highpass|notch]"
                   " [--step-filter-tempo-sync on|off] [--step-filter-tempo-bpm 30..300]"
                   " [--step-filter-division whole|half|quarter|dotted-quarter|quarter-triplet|eighth|dotted-eighth|eighth-triplet|sixteenth|dotted-sixteenth|sixteenth-triplet|thirty-second|thirty-second-triplet]"
                   " [--step-filter-rate-hz 0.05..60] [--step-filter-min-cutoff-hz 20..18000]"
                   " [--step-filter-max-cutoff-hz 20..18000] [--step-filter-resonance-q 0.5..12]"
                   " [--step-filter-depth 0..1] [--step-filter-shape 0..1]"
                   " [--step-filter-smoothing-ms 0..100] [--step-filter-mix 0..1]"
                   " [--step-filter-phase 0..1] [--step-filter-steps 16 values in 0..1]\n"
                << "                 [--notch-filter] [--notch-filter-frequency-hz 20..18000]"
                   " [--notch-filter-q 0.5..12] [--notch-filter-mix 0..1]\n"
                  << "                 [--formant-filter] [--formant-filter-vowel-position 0..4]"
                     " [--formant-filter-q 0.5..12] [--formant-filter-mix 0..1]"
                     " [--formant-filter-level-db -24..12]\n"
                   << "                 [--harmonic-resonator]"
                      " [--harmonic-resonator-root-frequency-hz 20..1000]"
                      " [--harmonic-resonator-decay-seconds 0.05..8]"
                      " [--harmonic-resonator-brightness 0..1]"
                      " [--harmonic-resonator-mix 0..1]\n"
                   << "                 [--multiband-compressor] [--multiband-slope db12|db24]"
                      " [--multiband-low-mid-hz 50..5000] [--multiband-mid-high-hz 100..10000]"
                      " [--multiband-{low|mid|high}-threshold-db -60..0]"
                      " [--multiband-{low|mid|high}-ratio 1..20]"
                      " [--multiband-{low|mid|high}-knee-db 0..18]"
                      " [--multiband-{low|mid|high}-attack-ms 0.1..1000]"
                      " [--multiband-{low|mid|high}-release-ms 5..2000]"
                      " [--multiband-{low|mid|high}-level-db -24..12]"
                      " [--multiband-{low|mid|high}-detector peak|rms]"
                      " [--multiband-{low|mid|high}-mute on|off]\n"
                 << "                 [--reverse-delay] [--reverse-delay-time-ms 1..2000]"
                    " [--reverse-delay-feedback 0..0.95] [--reverse-delay-mix 0..1]"
                     " [--reverse-delay-time-mode milliseconds|tempo-sync]"
                     " [--reverse-delay-tempo-bpm 20..300]"
                     " [--reverse-delay-division whole|dotted-half|half|dotted-quarter|quarter|quarter-triplet|dotted-eighth|eighth|eighth-triplet|dotted-sixteenth|sixteenth|sixteenth-triplet]"
                     " [--reverse-delay-low-cut-hz 0..1000]"
                     " [--reverse-delay-high-cut-hz 0..20000]"
                     " [--reverse-delay-forward-level 0..1]"
                     " [--reverse-delay-crossfade-ms 0..50] [--reverse-delay-run on|off]"
                     " [--reverse-delay-spillover on|off]\n"
                 << "                 [--ducking-delay] [--ducking-delay-time-ms 1..2000]"
                    " [--ducking-delay-feedback 0..0.95] [--ducking-delay-mix 0..1]"
                    " [--ducking-delay-threshold-db -72..0]"
                    " [--ducking-delay-attenuation-db 0..60]"
                    " [--ducking-delay-release-ms 1..2000]\n"
                  << "                 [--modulated-delay] [--modulated-delay-time-ms 1..2000]"
                     " [--modulated-delay-feedback 0..0.95] [--modulated-delay-mix 0..1]"
                     " [--modulated-delay-rate-hz 0.05..10] [--modulated-delay-depth-ms 0..50]"
                     " [--modulated-delay-waveform sine|triangle|random|triggered-ramp]"
                     " [--modulated-delay-spread-degrees 0..180]"
                     " [--modulated-delay-trigger-threshold-db -72..0]\n"
                  << "                 [--filtered-delay] [--filtered-delay-time-ms 1..2000]"
                     " [--filtered-delay-feedback 0..0.95] [--filtered-delay-mix 0..1]"
                     " [--filtered-delay-low-cut-hz 20..1000]"
                     " [--filtered-delay-high-cut-hz 2000..20000]\n"
                  << "                 [--resonant-delay] [--resonant-delay-time-ms 1..2000]"
                     " [--resonant-delay-feedback 0..0.95] [--resonant-delay-mix 0..1]"
                     " [--resonant-delay-center-hz 50..8000]"
                     " [--resonant-delay-q 0.5..12]\n"
                  << "                 [--sweep-delay] [--sweep-delay-time-ms 1..2000]"
                     " [--sweep-delay-feedback 0..0.95] [--sweep-delay-mix 0..1]"
                     " [--sweep-delay-start-hz 20..18000] [--sweep-delay-stop-hz 20..18000]"
                     " [--sweep-delay-resonance-q 0.5..12] [--sweep-delay-rate-hz 0.05..10]"
                     " [--sweep-delay-waveform sine|triangle|square]"
                     " [--sweep-delay-phase-degrees 0..360]\n"
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
