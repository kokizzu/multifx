#include "tabletfx/effect.hpp"
#include "tabletfx/channel_conversion.hpp"
#include "tabletfx/effects.hpp"
#include "tabletfx/effects/clean_boost.hpp"
#include "tabletfx/effects/chorus.hpp"
#include "tabletfx/effects/compressor.hpp"
#include "tabletfx/effects/envelope_wah.hpp"
#include "tabletfx/effects/flanger.hpp"
#include "tabletfx/effects/filters.hpp"
#include "tabletfx/effects/optical_compressor.hpp"
#include "tabletfx/effects/fet_compressor.hpp"
#include "tabletfx/effects/guitar_graphic_eq.hpp"
#include "tabletfx/effects/bass_graphic_eq.hpp"
#include "tabletfx/effects/shelf_tilt_eq.hpp"
#include "tabletfx/effects/high_gain_distortion.hpp"
#include "tabletfx/effects/mid_overdrive.hpp"
#include "tabletfx/effects/overdrive.hpp"
#include "tabletfx/effects/peak_limiter.hpp"
#include "tabletfx/effects/round_fuzz.hpp"
#include "tabletfx/effects/pedal_compressor.hpp"
#include "tabletfx/effects/phaser.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"
#include "tabletfx/effects/three_band_eq.hpp"
#include "tabletfx/effects/tremolo.hpp"
#include "tabletfx/effects/vibrato.hpp"
#include "tabletfx/effects/volume_pedal.hpp"
#include "tabletfx/effects/wah.hpp"
#include "tabletfx/level_meter.hpp"
#include "tabletfx/parallel_mixer.hpp"
#include "tabletfx/signal_controls.hpp"
#include "tabletfx/tuner.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define TABLETFX_WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define TABLETFX_WASM_EXPORT
#endif

namespace {

struct EffectEntry {
    std::unique_ptr<tabletfx::Effect> effect;
    bool enabled = false;
};

class WasmChain {
public:
    WasmChain(std::uint32_t sample_rate, std::uint16_t channels,
              std::uint32_t max_frames)
        : sample_rate_(sample_rate), channels_(channels), max_frames_(max_frames),
          input_(static_cast<std::size_t>(channels) * max_frames, 0.0F),
          dry_(static_cast<std::size_t>(channels) * max_frames, 0.0F),
          work_(static_cast<std::size_t>(channels) * max_frames, 0.0F),
          output_(static_cast<std::size_t>(channels) * max_frames, 0.0F),
          output_gain_(0.0F, static_cast<float>(sample_rate), 10.0F),
          output_limiter_(static_cast<float>(sample_rate), -1.0F, 80.0F),
          signal_controls_(static_cast<float>(sample_rate)),
          tuner_(static_cast<float>(sample_rate)) {
        effects_.reserve(29);
        effects_.push_back({std::make_unique<tabletfx::Gain>(
                                0.0F, static_cast<float>(sample_rate), 10.0F),
                            false});
        effects_.push_back({std::make_unique<tabletfx::VolumePedal>(
                                static_cast<float>(sample_rate)),
                            true});
        effects_.push_back({std::make_unique<tabletfx::HighPassFilter>(
                                static_cast<float>(sample_rate), channels, 80.0F),
                            false});
        effects_.push_back({std::make_unique<tabletfx::LowPassFilter>(
                                static_cast<float>(sample_rate), channels, 12000.0F),
                            false});
        effects_.push_back({std::make_unique<tabletfx::NoiseGate>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::PedalCompressor>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::Compressor>(
                                static_cast<float>(sample_rate)),
                            true});
        effects_.push_back({std::make_unique<tabletfx::Wah>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::EnvelopeWah>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::CleanBoost>(
                                0.0F, static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::TransparentOverdrive>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::MidFocusedOverdrive>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::HighGainDistortion>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::RoundFuzz>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::SoftDrive>(3.0F), false});
        effects_.push_back({std::make_unique<tabletfx::Tremolo>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::Chorus>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::Flanger>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::Phaser>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::Vibrato>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::ThreeBandParametricEQ>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::Delay>(
                                static_cast<float>(sample_rate), channels, 2000.0F,
                                350.0F, 0.25F, 0.25F, true),
                            false});
        effects_.push_back({std::make_unique<tabletfx::PolyPitchShifter>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::OpticalCompressor>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::FetCompressor>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::GuitarGraphicEQ>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::BassGraphicEQ>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::ShelfTiltEQ>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::RoomReverb>(
                                static_cast<float>(sample_rate), channels),
                            false});
    }

    [[nodiscard]] std::size_t effect_count() const noexcept { return effects_.size() + 2; }

    [[nodiscard]] float* input_data() noexcept { return input_.data(); }
    [[nodiscard]] float* output_data() noexcept { return output_.data(); }

    void process_tuner() noexcept {
        if (frames_ == 0) return;
        const std::size_t samples = static_cast<std::size_t>(frames_) * channels_;
        tuner_.analyze(std::span<const float>(input_.data(), samples), channels_);
    }

    [[nodiscard]] bool set_enabled(std::uint32_t index, bool enabled) noexcept {
        if (index == effects_.size()) {
            output_gain_enabled_ = enabled;
            return true;
        }
        if (index == effects_.size() + 1) {
            output_limiter_enabled_ = enabled;
            return true;
        }
        if (index >= effects_.size()) {
            return false;
        }
        if (index == 28U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        effects_[index].enabled = enabled;
        return true;
    }

    [[nodiscard]] std::uint32_t begin(std::uint32_t frames) noexcept {
        if (frames == 0 || frames > max_frames_) {
            frames_ = 0;
            return 0;
        }
        frames_ = frames;
        const std::size_t samples = static_cast<std::size_t>(frames) * channels_;
        input_meter_.process(std::span<const float>(input_.data(), samples), channels_);
        signal_controls_.process_input(
            std::span<const float>(input_.data(), samples),
            std::span<float>(work_.data(), samples), channels_);
        std::copy_n(work_.begin(), samples, dry_.begin());
        return frames_;
    }

    void process_effect(std::uint32_t index) noexcept {
        if (frames_ == 0 || index >= effects_.size() || !effects_[index].enabled) {
            return;
        }
        const std::size_t samples = static_cast<std::size_t>(frames_) * channels_;
        effects_[index].effect->process(std::span<float>(work_.data(), samples), channels_);
    }

    void set_mix(float mix) noexcept {
        signal_controls_.set_mix(mix);
    }

    void set_global_bypass(bool bypassed) noexcept {
        signal_controls_.set_global_bypassed(bypassed);
    }

    void set_input_muted(bool muted) noexcept {
        signal_controls_.set_input_muted(muted);
    }

    void set_panic_muted(bool muted) noexcept {
        signal_controls_.set_panic_muted(muted);
    }

    void finish(std::uint32_t frames) noexcept {
        if (frames_ == 0 || frames != frames_) {
            return;
        }
        const std::size_t samples = static_cast<std::size_t>(frames_) * channels_;
        signal_controls_.process_output(
            std::span<const float>(dry_.data(), samples),
            std::span<const float>(work_.data(), samples),
            std::span<float>(output_.data(), samples), channels_);
    }

    void process_output_gain() noexcept {
        if (frames_ == 0 || !output_gain_enabled_) {
            return;
        }
        const std::size_t samples = static_cast<std::size_t>(frames_) * channels_;
        output_gain_.process(std::span<float>(output_.data(), samples), channels_);
    }

    void process_output_limiter() noexcept {
        if (frames_ == 0) {
            return;
        }
        const std::size_t samples = static_cast<std::size_t>(frames_) * channels_;
        if (output_limiter_enabled_) {
            output_limiter_.process(std::span<float>(output_.data(), samples), channels_);
        }
        tuner_.apply_output_mute(std::span<float>(output_.data(), samples), channels_);
        output_meter_.process(std::span<const float>(output_.data(), samples), channels_);
    }

    void reset() noexcept {
        for (EffectEntry& entry : effects_) {
            entry.effect->reset();
        }
        output_gain_.reset();
        output_limiter_.reset();
        signal_controls_.reset();
        tuner_.reset();
        input_meter_.reset();
        output_meter_.reset();
        frames_ = 0;
    }

    [[nodiscard]] float meter_peak(std::uint32_t index) const noexcept {
        if (index == 0) {
            return input_meter_.peak();
        }
        if (index == 1) {
            return output_meter_.peak();
        }
        return 0.0F;
    }

    [[nodiscard]] float meter_rms(std::uint32_t index) const noexcept {
        if (index == 0) {
            return input_meter_.rms();
        }
        if (index == 1) {
            return output_meter_.rms();
        }
        return 0.0F;
    }

    [[nodiscard]] std::uint32_t meter_flags(std::uint32_t index) const noexcept {
        if (index == 0) {
            return input_meter_.clip_flags();
        }
        if (index == 1) {
            return output_meter_.clip_flags();
        }
        return 0;
    }

    [[nodiscard]] std::uint32_t meter_invalid_samples(std::uint32_t index) const noexcept {
        if (index == 0) {
            return input_meter_.invalid_samples();
        }
        if (index == 1) {
            return output_meter_.invalid_samples();
        }
        return 0;
    }

    void clear_meter_clip(std::uint32_t index) noexcept {
        if (index == 0) {
            input_meter_.clear_clip_latch();
        } else if (index == 1) {
            output_meter_.clear_clip_latch();
        }
    }

    void set_tuner_reference(float frequency_hz) noexcept {
        tuner_.set_reference_a4_hz(frequency_hz);
    }

    void set_tuner_muted(bool muted) noexcept {
        tuner_.set_muted_output(muted);
    }

    [[nodiscard]] tabletfx::TunerReading tuner_reading() const noexcept {
        return tuner_.reading();
    }

    [[nodiscard]] std::uint32_t tuner_memory_bytes() const noexcept {
        return static_cast<std::uint32_t>(tuner_.memory_bytes());
    }

    void configure_compressor(float threshold_db, float ratio, float knee_db,
                              float attack_ms, float release_ms,
                              float makeup_db) noexcept {
        auto* compressor = static_cast<tabletfx::Compressor*>(effects_[6].effect.get());
        tabletfx::CompressorParameters parameters;
        parameters.threshold_db = threshold_db;
        parameters.ratio = ratio;
        parameters.knee_db = knee_db;
        parameters.attack_ms = attack_ms;
        parameters.release_ms = release_ms;
        parameters.makeup_db = makeup_db;
        compressor->set_parameters(parameters);
    }

    void configure_drive(float drive) noexcept {
        static_cast<tabletfx::SoftDrive*>(effects_[14].effect.get())->set_drive(drive);
    }

    void configure_clean_boost(float gain_db) noexcept {
        static_cast<tabletfx::CleanBoost*>(effects_[9].effect.get())->set_gain_db(gain_db);
    }

    void configure_overdrive(float drive, float level_db, float mix) noexcept {
        auto* overdrive = static_cast<tabletfx::TransparentOverdrive*>(effects_[10].effect.get());
        tabletfx::TransparentOverdriveParameters parameters;
        parameters.drive = drive;
        parameters.level_db = level_db;
        parameters.mix = mix;
        overdrive->configure(parameters);
    }

    void configure_mid_overdrive(float drive, float boost_db, float level_db,
                                 float mix) noexcept {
        auto* overdrive = static_cast<tabletfx::MidFocusedOverdrive*>(
            effects_[11].effect.get());
        tabletfx::MidFocusedOverdriveParameters parameters;
        parameters.drive = drive;
        parameters.mid_boost_db = boost_db;
        parameters.level_db = level_db;
        parameters.mix = mix;
        overdrive->configure(parameters);
    }

    void configure_high_gain_distortion(float drive, float pre_low_cut_hz,
                                        float pre_high_cut_hz, float post_tone_hz,
                                        float level_db, float mix) noexcept {
        auto* distortion = static_cast<tabletfx::HighGainDistortion*>(effects_[12].effect.get());
        tabletfx::HighGainDistortionParameters parameters;
        parameters.drive = drive;
        parameters.pre_low_cut_hz = pre_low_cut_hz;
        parameters.pre_high_cut_hz = pre_high_cut_hz;
        parameters.post_tone_hz = post_tone_hz;
        parameters.level_db = level_db;
        parameters.mix = mix;
        distortion->configure(parameters);
    }

    void configure_round_fuzz(float fuzz, float level_db, float mix) noexcept {
        auto* effect = static_cast<tabletfx::RoundFuzz*>(effects_[13].effect.get());
        tabletfx::RoundFuzzParameters parameters;
        parameters.fuzz = fuzz;
        parameters.level_db = level_db;
        parameters.mix = mix;
        effect->configure(parameters);
    }

    void configure_eq(float f1, float g1, float q1, float f2, float g2, float q2,
                      float f3, float g3, float q3) noexcept {
        static_cast<tabletfx::ThreeBandParametricEQ*>(effects_[20].effect.get())
            ->configure(f1, g1, q1, f2, g2, q2, f3, g3, q3);
    }

    void configure_delay(float delay_ms, float feedback, float mix) noexcept {
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure(delay_ms, feedback, mix);
    }

    void configure_delay_bbd_style(bool enabled) noexcept {
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure_bbd_style(enabled);
    }

    void configure_delay_tape_echo(bool enabled, float saturation,
                                   float wow_depth_ms, float wow_rate_hz,
                                   float flutter_depth_ms,
                                   float flutter_rate_hz) noexcept {
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure_tape_echo(enabled, saturation, wow_depth_ms, wow_rate_hz,
                                  flutter_depth_ms, flutter_rate_hz);
    }

    void configure_delay_tempo(float bpm, std::uint32_t division,
                               float feedback, float mix) noexcept {
        const auto safe_division = division <= 11U
            ? static_cast<tabletfx::DelayDivision>(division)
            : static_cast<tabletfx::DelayDivision>(255U);
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure_tempo(bpm, safe_division, feedback, mix);
    }

    void configure_delay_ping_pong(bool enabled) noexcept {
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure_ping_pong(enabled);
    }

    void configure_delay_dual(float delay_a_ms, float feedback_a, float mix_a,
                               float delay_b_ms, float feedback_b, float mix_b) noexcept {
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure_dual(delay_a_ms, feedback_a, mix_a,
                             delay_b_ms, feedback_b, mix_b);
    }

    void configure_delay_multitap(bool enabled, float feedback, float mix) noexcept {
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure_multitap(enabled, feedback, mix);
    }

    void configure_delay_multitap_tap(std::uint32_t tap, float delay_ms,
                                      float level, float pan) noexcept {
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure_multitap_tap(tap, delay_ms, level, pan);
    }

    void configure_delay_tape_heads(bool enabled, float spacing_ms,
                                    std::uint32_t selected_heads) noexcept {
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure_tape_heads(enabled, spacing_ms,
                                   static_cast<std::uint8_t>(selected_heads));
    }

    void configure_pitch(float semitones) noexcept {
        static_cast<tabletfx::PolyPitchShifter*>(effects_[22].effect.get())
            ->set_pitch_semitones(semitones);
    }

    void configure_filters(float high_pass_hz, float low_pass_hz) noexcept {
        static_cast<tabletfx::HighPassFilter*>(effects_[2].effect.get())->configure(high_pass_hz);
        static_cast<tabletfx::LowPassFilter*>(effects_[3].effect.get())->configure(low_pass_hz);
    }

    void configure_input_gain(float gain_db) noexcept {
        static_cast<tabletfx::Gain*>(effects_[0].effect.get())->set_gain_db_immediate(gain_db);
    }

    void configure_output_gain(float gain_db) noexcept {
        output_gain_.set_gain_db_immediate(gain_db);
    }

    void ramp_input_gain(float gain_db) noexcept {
        static_cast<tabletfx::Gain*>(effects_[0].effect.get())->set_gain_db(gain_db);
    }

    void ramp_output_gain(float gain_db) noexcept {
        output_gain_.set_gain_db(gain_db);
    }

    void configure_pedal_compressor(float sensitivity, float sustain, float attack,
                                    float level_db) noexcept {
        tabletfx::PedalCompressorParameters parameters;
        parameters.sensitivity = sensitivity;
        parameters.sustain = sustain;
        parameters.attack = attack;
        parameters.level_db = level_db;
        static_cast<tabletfx::PedalCompressor*>(effects_[5].effect.get())
            ->set_parameters(parameters);
    }

    void configure_noise_gate(float threshold_db, float attenuation_db,
                              float hysteresis_db, float attack_ms,
                              float hold_ms, float release_ms) noexcept {
        tabletfx::NoiseGateParameters parameters;
        parameters.threshold_db = threshold_db;
        parameters.attenuation_db = attenuation_db;
        parameters.hysteresis_db = hysteresis_db;
        parameters.attack_ms = attack_ms;
        parameters.hold_ms = hold_ms;
        parameters.release_ms = release_ms;
        static_cast<tabletfx::NoiseGate*>(effects_[4].effect.get())
            ->configure(static_cast<float>(sample_rate_), parameters);
    }

    void configure_volume_pedal(float position, float minimum_db,
                                float maximum_db, std::uint32_t taper) noexcept {
        auto* pedal = static_cast<tabletfx::VolumePedal*>(effects_[1].effect.get());
        pedal->set_endpoints_db(minimum_db, maximum_db);
        pedal->set_taper(taper == 0 ? tabletfx::VolumePedalTaper::LinearAmplitude
                                    : tabletfx::VolumePedalTaper::AudioTaperDb);
        pedal->set_position(position);
    }

    void configure_wah(float position, float minimum_hz, float maximum_hz,
                       float resonance_q, float level_db) noexcept {
        auto* wah = static_cast<tabletfx::Wah*>(effects_[7].effect.get());
        wah->set_sweep_range_hz(minimum_hz, maximum_hz);
        wah->set_resonance_q(resonance_q);
        wah->set_level_db(level_db);
        wah->set_position(position);
    }

    void configure_envelope_wah(float sensitivity, float minimum_hz,
                                float maximum_hz, float resonance_q,
                                float level_db, float attack_ms, float release_ms,
                                float smoothing_ms, std::uint32_t direction) noexcept {
        tabletfx::EnvelopeWahParameters parameters;
        parameters.sensitivity = sensitivity;
        parameters.minimum_hz = minimum_hz;
        parameters.maximum_hz = maximum_hz;
        parameters.resonance_q = resonance_q;
        parameters.level_db = level_db;
        parameters.attack_ms = attack_ms;
        parameters.release_ms = release_ms;
        parameters.smoothing_ms = smoothing_ms;
        parameters.direction = direction == 1 ? tabletfx::EnvelopeWahDirection::Down
                                              : tabletfx::EnvelopeWahDirection::Up;
        static_cast<tabletfx::EnvelopeWah*>(effects_[8].effect.get())
            ->configure(static_cast<float>(sample_rate_), channels_, parameters);
    }

    void configure_tremolo(float rate_hz, float depth, std::uint32_t waveform,
                           std::uint32_t tempo_sync, float tempo_bpm,
                           float beats_per_cycle) noexcept {
        auto* tremolo = static_cast<tabletfx::Tremolo*>(effects_[15].effect.get());
        tremolo->set_rate_hz(rate_hz);
        switch (waveform) {
        case 1:
            tremolo->set_waveform(tabletfx::TremoloWaveform::Triangle);
            break;
        case 2:
            tremolo->set_waveform(tabletfx::TremoloWaveform::Square);
            break;
        default:
            tremolo->set_waveform(tabletfx::TremoloWaveform::Sine);
            break;
        }
        tremolo->set_tempo_sync(tempo_sync != 0, tempo_bpm, beats_per_cycle);
        tremolo->set_depth(depth);
    }

    void configure_chorus(float rate_hz, float base_delay_ms, float depth_ms,
                          float mix) noexcept {
        tabletfx::ChorusParameters parameters;
        parameters.rate_hz = rate_hz;
        parameters.base_delay_ms = base_delay_ms;
        parameters.depth_ms = depth_ms;
        parameters.mix = mix;
        static_cast<tabletfx::Chorus*>(effects_[16].effect.get())
            ->set_parameters(parameters);
    }

    void configure_flanger(float rate_hz, float base_delay_ms, float depth_ms,
                           float feedback, std::uint32_t feedback_polarity,
                           float mix) noexcept {
        tabletfx::FlangerParameters parameters;
        parameters.rate_hz = rate_hz;
        parameters.base_delay_ms = base_delay_ms;
        parameters.depth_ms = depth_ms;
        parameters.feedback = feedback;
        parameters.feedback_polarity = feedback_polarity == 1
            ? tabletfx::FlangerFeedbackPolarity::Negative
            : tabletfx::FlangerFeedbackPolarity::Positive;
        parameters.mix = mix;
        static_cast<tabletfx::Flanger*>(effects_[17].effect.get())
            ->set_parameters(parameters);
    }

    void configure_phaser(float rate_hz, float minimum_hz, float maximum_hz,
                          float feedback, std::uint32_t stages,
                          float mix) noexcept {
        tabletfx::PhaserParameters parameters;
        parameters.rate_hz = rate_hz;
        parameters.minimum_hz = minimum_hz;
        parameters.maximum_hz = maximum_hz;
        parameters.feedback = feedback;
        parameters.stages = static_cast<std::uint8_t>(stages);
        parameters.mix = mix;
        static_cast<tabletfx::Phaser*>(effects_[18].effect.get())
            ->set_parameters(parameters);
    }

    void configure_vibrato(float rate_hz, float depth_ms, float mix) noexcept {
        tabletfx::VibratoParameters parameters;
        parameters.rate_hz = rate_hz;
        parameters.depth_ms = depth_ms;
        parameters.mix = mix;
        static_cast<tabletfx::Vibrato*>(effects_[19].effect.get())
            ->set_parameters(parameters);
    }

    void configure_optical_compressor(float threshold_db, float ratio, float knee_db,
                                      float attack_ms, float release_ms,
                                      float program_release_ms, float rms_window_ms,
                                      float makeup_db) noexcept {
        tabletfx::OpticalCompressorParameters parameters;
        parameters.threshold_db = threshold_db;
        parameters.ratio = ratio;
        parameters.knee_db = knee_db;
        parameters.attack_ms = attack_ms;
        parameters.release_ms = release_ms;
        parameters.program_release_ms = program_release_ms;
        parameters.rms_window_ms = rms_window_ms;
        parameters.makeup_db = makeup_db;
        static_cast<tabletfx::OpticalCompressor*>(effects_[23].effect.get())
            ->set_parameters(parameters);
    }

    void configure_fet_compressor(float threshold_db, float ratio, float knee_db,
                                  float attack_ms, float release_ms,
                                  float makeup_db) noexcept {
        tabletfx::FetCompressorParameters parameters;
        parameters.threshold_db = threshold_db;
        parameters.ratio = ratio;
        parameters.knee_db = knee_db;
        parameters.attack_ms = attack_ms;
        parameters.release_ms = release_ms;
        parameters.makeup_db = makeup_db;
        static_cast<tabletfx::FetCompressor*>(effects_[24].effect.get())
            ->set_parameters(parameters);
    }

    void configure_guitar_graphic_eq(std::uint32_t layout, float gain_1_db,
                                     float gain_2_db, float gain_3_db,
                                     float gain_4_db, float gain_5_db,
                                     float gain_6_db, float gain_7_db) noexcept {
        auto* eq = static_cast<tabletfx::GuitarGraphicEQ*>(effects_[25].effect.get());
        eq->set_layout(layout == 0U ? tabletfx::GuitarGraphicEQLayout::FiveBand
                                    : tabletfx::GuitarGraphicEQLayout::SevenBand);
        const std::array<float, 7> gains{
            gain_1_db, gain_2_db, gain_3_db, gain_4_db,
            gain_5_db, gain_6_db, gain_7_db};
        for (std::size_t band = 0; band < gains.size(); ++band) {
            static_cast<void>(eq->set_band_gain_db(band, gains[band]));
        }
    }

    void configure_bass_graphic_eq(std::uint32_t layout, float gain_1_db,
                                   float gain_2_db, float gain_3_db,
                                   float gain_4_db, float gain_5_db,
                                   float gain_6_db, float gain_7_db) noexcept {
        auto* eq = static_cast<tabletfx::BassGraphicEQ*>(effects_[26].effect.get());
        eq->set_layout(layout == 0U ? tabletfx::BassGraphicEQLayout::FourBand
                                    : tabletfx::BassGraphicEQLayout::SevenBand);
        const std::array<float, 7> gains{
            gain_1_db, gain_2_db, gain_3_db, gain_4_db,
            gain_5_db, gain_6_db, gain_7_db};
        for (std::size_t band = 0; band < gains.size(); ++band) {
            static_cast<void>(eq->set_band_gain_db(band, gains[band]));
        }
    }

    void configure_shelf_tilt_eq(std::uint32_t mode, float frequency_hz,
                                 float gain_db) noexcept {
        auto* eq = static_cast<tabletfx::ShelfTiltEQ*>(effects_[27].effect.get());
        eq->configure(static_cast<tabletfx::ShelfTiltMode>(mode), frequency_hz, gain_db);
    }

    void configure_room_reverb(std::uint32_t size, float decay_seconds,
                               float pre_delay_ms, float damping_hz, float mix,
                               float level_db) noexcept {
        auto* reverb = static_cast<tabletfx::RoomReverb*>(effects_[28].effect.get());
        tabletfx::RoomReverbParameters parameters;
        parameters.size = size == 0U ? tabletfx::RoomSize::small
                                     : tabletfx::RoomSize::large;
        parameters.decay_seconds = decay_seconds;
        parameters.pre_delay_ms = pre_delay_ms;
        parameters.damping_hz = damping_hz;
        parameters.mix = mix;
        parameters.level_db = level_db;
        reverb->configure(parameters);
    }

    void configure_limiter(float ceiling_db, float release_ms) noexcept {
        output_limiter_.set_ceiling_db(ceiling_db);
        output_limiter_.set_release_ms(release_ms);
    }

    [[nodiscard]] std::uint32_t memory_bytes() const noexcept {
        std::size_t bytes = sizeof(*this) + effects_.capacity() * sizeof(EffectEntry);
        bytes += input_.capacity() * sizeof(float) + dry_.capacity() * sizeof(float);
        bytes += work_.capacity() * sizeof(float) + output_.capacity() * sizeof(float);
        bytes += output_gain_.memory_bytes();
        bytes += output_limiter_.memory_bytes();
        for (const EffectEntry& entry : effects_) {
            bytes += entry.effect->memory_bytes();
        }
        return bytes > std::numeric_limits<std::uint32_t>::max()
                   ? std::numeric_limits<std::uint32_t>::max()
                   : static_cast<std::uint32_t>(bytes);
    }

    [[nodiscard]] std::uint32_t effect_memory_bytes(std::uint32_t index) const noexcept {
        if (index == effects_.size()) {
            return static_cast<std::uint32_t>(output_gain_.memory_bytes());
        }
        if (index == effects_.size() + 1) {
            return static_cast<std::uint32_t>(output_limiter_.memory_bytes());
        }
        if (index >= effects_.size()) {
            return 0;
        }
        const std::size_t bytes = effects_[index].effect->memory_bytes();
        return bytes > std::numeric_limits<std::uint32_t>::max()
                   ? std::numeric_limits<std::uint32_t>::max()
                   : static_cast<std::uint32_t>(bytes);
    }

private:
    std::uint32_t sample_rate_;
    std::uint16_t channels_;
    std::uint32_t max_frames_;
    std::uint32_t frames_ = 0;
    std::vector<EffectEntry> effects_;
    std::vector<float> input_;
    std::vector<float> dry_;
    std::vector<float> work_;
    std::vector<float> output_;
    tabletfx::Gain output_gain_;
    tabletfx::PeakLimiter output_limiter_;
    tabletfx::SignalControls signal_controls_;
    tabletfx::ChromaticTuner tuner_;
    tabletfx::LevelMeter input_meter_;
    tabletfx::LevelMeter output_meter_;
    bool output_gain_enabled_ = true;
    bool output_limiter_enabled_ = true;
};

WasmChain* as_chain(void* opaque) noexcept {
    return static_cast<WasmChain*>(opaque);
}

} // namespace

extern "C" {

TABLETFX_WASM_EXPORT std::uint32_t tf_convert_channels(
    const float* input, std::uint32_t input_samples, std::uint16_t input_channels,
    float* output, std::uint32_t output_capacity, std::uint16_t output_channels,
    std::uint32_t sum_policy) noexcept {
    if ((input_samples != 0 && input == nullptr) ||
        (output_capacity != 0 && output == nullptr)) {
        return static_cast<std::uint32_t>(tabletfx::ChannelConversionStatus::InvalidInputLength);
    }
    if (sum_policy > static_cast<std::uint32_t>(tabletfx::MonoSumPolicy::EqualPower)) {
        return static_cast<std::uint32_t>(tabletfx::ChannelConversionStatus::InvalidSummingPolicy);
    }
    const auto result = tabletfx::convert_channels(
        std::span<const float>(input, input_samples), input_channels,
        std::span<float>(output, output_capacity), output_channels,
        static_cast<tabletfx::MonoSumPolicy>(sum_policy));
    return static_cast<std::uint32_t>(result.status);
}

TABLETFX_WASM_EXPORT std::uint32_t tf_channel_conversion_self_test() noexcept {
    std::array<float, 2> mono{0.5F, -0.25F};
    std::array<float, 4> stereo{};
    std::array<float, 2> mixed{};
    const auto upmix = tf_convert_channels(mono.data(), 2, 1, stereo.data(), 4, 2, 0);
    if (upmix != static_cast<std::uint32_t>(tabletfx::ChannelConversionStatus::Success) ||
        stereo != std::array<float, 4>{0.5F, 0.5F, -0.25F, -0.25F}) {
        return 1;
    }
    const auto downmix = tf_convert_channels(stereo.data(), 4, 2, mixed.data(), 2, 1, 0);
    if (downmix != static_cast<std::uint32_t>(tabletfx::ChannelConversionStatus::Success) ||
        mixed != mono) {
        return 2;
    }
    return 0;
}

TABLETFX_WASM_EXPORT std::uint32_t tf_parallel_mixer_self_test() noexcept {
    const std::array<float, 1> dry{0.1F};
    const std::array<float, 1> branch_a{0.2F};
    const std::array<float, 1> branch_b{0.3F};
    const std::array<float, 1> input{0.25F};
    std::array<float, 1> split_a{};
    std::array<float, 1> split_b{};
    std::array<float, 1> output{};
    const auto split = tabletfx::split_to_parallel(input, split_a, split_b, 1);
    if (!split.succeeded() || split_a != input || split_b != input) return 1;

    tabletfx::ParallelMixer mixer(48000.0F, 0.0F);
    tabletfx::ParallelMixParameters parameters;
    parameters.dry_gain = 0.5F;
    parameters.branch_a_gain = 0.5F;
    parameters.branch_b_gain = 0.5F;
    parameters.branch_b_inverted = true;
    mixer.set_parameters_immediate(parameters);
    const auto mixed = mixer.process(dry, branch_a, branch_b, output, 1);
    if (!mixed.succeeded() || std::abs(output[0]) > 1.0e-7F) return 2;
    return 0;
}

TABLETFX_WASM_EXPORT void* tf_chain_create(std::uint32_t sample_rate,
                                         std::uint32_t channels,
                                         std::uint32_t max_frames) noexcept {
    if (sample_rate < 8000 || sample_rate > 192000 || channels == 0 || channels > 2 ||
        max_frames == 0 || max_frames > 8192) {
        return nullptr;
    }
    try {
        return new WasmChain(sample_rate, static_cast<std::uint16_t>(channels), max_frames);
    } catch (...) {
        return nullptr;
    }
}

TABLETFX_WASM_EXPORT void tf_chain_destroy(void* opaque) noexcept {
    delete as_chain(opaque);
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_effect_count(void* opaque) noexcept {
    return opaque == nullptr ? 0U : static_cast<std::uint32_t>(as_chain(opaque)->effect_count());
}

TABLETFX_WASM_EXPORT std::uintptr_t tf_chain_input_buffer(void* opaque) noexcept {
    return opaque == nullptr ? 0U : reinterpret_cast<std::uintptr_t>(as_chain(opaque)->input_data());
}

TABLETFX_WASM_EXPORT std::uintptr_t tf_chain_output_buffer(void* opaque) noexcept {
    return opaque == nullptr ? 0U : reinterpret_cast<std::uintptr_t>(as_chain(opaque)->output_data());
}

TABLETFX_WASM_EXPORT void tf_chain_process_tuner(void* opaque) noexcept {
    if (opaque != nullptr) as_chain(opaque)->process_tuner();
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_memory_bytes(void* opaque) noexcept {
    return opaque == nullptr ? 0U : as_chain(opaque)->memory_bytes();
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_effect_memory_bytes(void* opaque,
                                                               std::uint32_t index) noexcept {
    return opaque == nullptr ? 0U : as_chain(opaque)->effect_memory_bytes(index);
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_set_enabled(void* opaque, std::uint32_t index,
                                                       std::uint32_t enabled) noexcept {
    return opaque != nullptr && as_chain(opaque)->set_enabled(index, enabled != 0) ? 1U : 0U;
}

TABLETFX_WASM_EXPORT void tf_chain_set_mix(void* opaque, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->set_mix(mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_global_bypass(void* opaque,
                                                      std::uint32_t bypassed) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->set_global_bypass(bypassed != 0);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_input_mute(void* opaque,
                                                  std::uint32_t muted) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->set_input_muted(muted != 0);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_panic_mute(void* opaque,
                                                   std::uint32_t muted) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->set_panic_muted(muted != 0);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_input_gain(void* opaque, float gain_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_input_gain(gain_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_output_gain(void* opaque, float gain_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_output_gain(gain_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_ramp_input_gain(void* opaque, float gain_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->ramp_input_gain(gain_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_ramp_output_gain(void* opaque, float gain_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->ramp_output_gain(gain_db);
    }
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_begin(void* opaque,
                                                 std::uint32_t frames) noexcept {
    return opaque == nullptr ? 0U : as_chain(opaque)->begin(frames);
}

TABLETFX_WASM_EXPORT void tf_chain_process_effect(void* opaque,
                                                std::uint32_t index) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->process_effect(index);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_finish(void* opaque, std::uint32_t frames) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->finish(frames);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_process_output_gain(void* opaque) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->process_output_gain();
    }
}

TABLETFX_WASM_EXPORT void tf_chain_process_output_limiter(void* opaque) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->process_output_limiter();
    }
}

TABLETFX_WASM_EXPORT float tf_chain_meter_peak(void* opaque,
                                               std::uint32_t index) noexcept {
    return opaque == nullptr ? 0.0F : as_chain(opaque)->meter_peak(index);
}

TABLETFX_WASM_EXPORT float tf_chain_meter_rms(void* opaque,
                                              std::uint32_t index) noexcept {
    return opaque == nullptr ? 0.0F : as_chain(opaque)->meter_rms(index);
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_meter_flags(
    void* opaque, std::uint32_t index) noexcept {
    if (opaque == nullptr) {
        return 0;
    }
    return as_chain(opaque)->meter_flags(index);
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_meter_invalid_samples(
    void* opaque, std::uint32_t index) noexcept {
    return opaque == nullptr
               ? 0U
               : as_chain(opaque)->meter_invalid_samples(index);
}

TABLETFX_WASM_EXPORT void tf_chain_clear_meter_clip(void* opaque,
                                                    std::uint32_t index) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->clear_meter_clip(index);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_tuner_reference(void* opaque,
                                                       float frequency_hz) noexcept {
    if (opaque != nullptr) as_chain(opaque)->set_tuner_reference(frequency_hz);
}

TABLETFX_WASM_EXPORT void tf_chain_set_tuner_muted(void* opaque,
                                                   std::uint32_t muted) noexcept {
    if (opaque != nullptr) as_chain(opaque)->set_tuner_muted(muted != 0);
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_tuner_generation(void* opaque) noexcept {
    return opaque == nullptr ? 0U : as_chain(opaque)->tuner_reading().generation;
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_tuner_memory_bytes(void* opaque) noexcept {
    return opaque == nullptr ? 0U : as_chain(opaque)->tuner_memory_bytes();
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_tuner_valid(void* opaque) noexcept {
    return opaque != nullptr && as_chain(opaque)->tuner_reading().valid ? 1U : 0U;
}

TABLETFX_WASM_EXPORT std::int32_t tf_chain_tuner_note(void* opaque) noexcept {
    return opaque == nullptr ? -1 : as_chain(opaque)->tuner_reading().midi_note;
}

TABLETFX_WASM_EXPORT float tf_chain_tuner_frequency(void* opaque) noexcept {
    return opaque == nullptr ? 0.0F : as_chain(opaque)->tuner_reading().frequency_hz;
}

TABLETFX_WASM_EXPORT float tf_chain_tuner_cents(void* opaque) noexcept {
    return opaque == nullptr ? 0.0F : as_chain(opaque)->tuner_reading().cents;
}

TABLETFX_WASM_EXPORT float tf_chain_tuner_confidence(void* opaque) noexcept {
    return opaque == nullptr ? 0.0F : as_chain(opaque)->tuner_reading().confidence;
}

TABLETFX_WASM_EXPORT void tf_chain_reset(void* opaque) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->reset();
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_compressor(
    void* opaque, float threshold_db, float ratio, float knee_db, float attack_ms,
    float release_ms, float makeup_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_compressor(threshold_db, ratio, knee_db, attack_ms,
                                               release_ms, makeup_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_optical_compressor(
    void* opaque, float threshold_db, float ratio, float knee_db,
    float attack_ms, float release_ms, float program_release_ms,
    float rms_window_ms, float makeup_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_optical_compressor(
            threshold_db, ratio, knee_db, attack_ms, release_ms,
            program_release_ms, rms_window_ms, makeup_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_fet_compressor(
    void* opaque, float threshold_db, float ratio, float knee_db,
    float attack_ms, float release_ms, float makeup_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_fet_compressor(
            threshold_db, ratio, knee_db, attack_ms, release_ms, makeup_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_guitar_graphic_eq(
    void* opaque, std::uint32_t layout, float gain_1_db, float gain_2_db,
    float gain_3_db, float gain_4_db, float gain_5_db, float gain_6_db,
    float gain_7_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_guitar_graphic_eq(
            layout, gain_1_db, gain_2_db, gain_3_db, gain_4_db, gain_5_db,
            gain_6_db, gain_7_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_bass_graphic_eq(
    void* opaque, std::uint32_t layout, float gain_1_db, float gain_2_db,
    float gain_3_db, float gain_4_db, float gain_5_db, float gain_6_db,
    float gain_7_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_bass_graphic_eq(
            layout, gain_1_db, gain_2_db, gain_3_db, gain_4_db, gain_5_db,
            gain_6_db, gain_7_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_shelf_tilt_eq(
    void* opaque, std::uint32_t mode, float frequency_hz, float gain_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_shelf_tilt_eq(mode, frequency_hz, gain_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_room_reverb(
    void* opaque, std::uint32_t size, float decay_seconds, float pre_delay_ms,
    float damping_hz, float mix, float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_room_reverb(
            size, decay_seconds, pre_delay_ms, damping_hz, mix, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_filters(void* opaque, float high_pass_hz,
                                              float low_pass_hz) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_filters(high_pass_hz, low_pass_hz);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_pedal_compressor(
    void* opaque, float sensitivity, float sustain, float attack,
    float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_pedal_compressor(sensitivity, sustain, attack,
                                                     level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_noise_gate(
    void* opaque, float threshold_db, float attenuation_db,
    float hysteresis_db, float attack_ms, float hold_ms,
    float release_ms) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_noise_gate(threshold_db, attenuation_db,
                                              hysteresis_db, attack_ms,
                                              hold_ms, release_ms);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_volume_pedal(
    void* opaque, float position, float minimum_db, float maximum_db,
    std::uint32_t taper) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_volume_pedal(position, minimum_db,
                                                 maximum_db, taper);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_limiter(
    void* opaque, float ceiling_db, float release_ms) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_limiter(ceiling_db, release_ms);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_wah(
    void* opaque, float position, float minimum_hz, float maximum_hz,
    float resonance_q, float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_wah(position, minimum_hz, maximum_hz,
                                        resonance_q, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_envelope_wah(
    void* opaque, float sensitivity, float minimum_hz, float maximum_hz,
    float resonance_q, float level_db, float attack_ms, float release_ms,
    float smoothing_ms, std::uint32_t direction) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_envelope_wah(
            sensitivity, minimum_hz, maximum_hz, resonance_q, level_db,
            attack_ms, release_ms, smoothing_ms, direction);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_tremolo(
    void* opaque, float rate_hz, float depth, std::uint32_t waveform,
    std::uint32_t tempo_sync, float tempo_bpm, float beats_per_cycle) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_tremolo(rate_hz, depth, waveform,
                                            tempo_sync, tempo_bpm,
                                            beats_per_cycle);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_chorus(
    void* opaque, float rate_hz, float base_delay_ms,
    float depth_ms, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_chorus(rate_hz, base_delay_ms, depth_ms, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_flanger(
    void* opaque, float rate_hz, float base_delay_ms, float depth_ms,
    float feedback, std::uint32_t feedback_polarity, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_flanger(rate_hz, base_delay_ms, depth_ms,
                                            feedback, feedback_polarity, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_phaser(
    void* opaque, float rate_hz, float minimum_hz, float maximum_hz,
    float feedback, std::uint32_t stages, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_phaser(rate_hz, minimum_hz, maximum_hz,
                                           feedback, stages, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_vibrato(
    void* opaque, float rate_hz, float depth_ms, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_vibrato(rate_hz, depth_ms, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_overdrive(
    void* opaque, float drive, float level_db, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_overdrive(drive, level_db, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_mid_overdrive(
    void* opaque, float drive, float boost_db, float level_db, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_mid_overdrive(drive, boost_db, level_db, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_high_gain_distortion(
    void* opaque, float drive, float pre_low_cut_hz, float pre_high_cut_hz,
    float post_tone_hz, float level_db, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_high_gain_distortion(
            drive, pre_low_cut_hz, pre_high_cut_hz, post_tone_hz, level_db, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_round_fuzz(
    void* opaque, float fuzz, float level_db, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_round_fuzz(fuzz, level_db, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_drive(void* opaque, float drive) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_drive(drive);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_clean_boost(void* opaque,
                                                  float gain_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_clean_boost(gain_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_eq(
    void* opaque, float f1, float g1, float q1, float f2, float g2, float q2,
    float f3, float g3, float q3) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_eq(f1, g1, q1, f2, g2, q2, f3, g3, q3);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_delay(void* opaque, float delay_ms,
                                             float feedback, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_delay(delay_ms, feedback, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_delay_bbd_style(
    void* opaque, std::uint32_t enabled) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_delay_bbd_style(enabled != 0U);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_delay_tape_echo(
    void* opaque, std::uint32_t enabled, float saturation, float wow_depth_ms,
    float wow_rate_hz, float flutter_depth_ms, float flutter_rate_hz) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_delay_tape_echo(
            enabled != 0U, saturation, wow_depth_ms, wow_rate_hz,
            flutter_depth_ms, flutter_rate_hz);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_delay_tempo(void* opaque, float bpm,
                                                   std::uint32_t division,
                                                   float feedback, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_delay_tempo(bpm, division, feedback, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_delay_ping_pong(void* opaque,
                                                        std::uint32_t enabled) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_delay_ping_pong(enabled != 0U);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_delay_dual(
    void* opaque, float delay_a_ms, float feedback_a, float mix_a,
    float delay_b_ms, float feedback_b, float mix_b) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_delay_dual(delay_a_ms, feedback_a, mix_a,
                                                delay_b_ms, feedback_b, mix_b);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_delay_multitap(
    void* opaque, std::uint32_t enabled, float feedback, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_delay_multitap(enabled != 0U, feedback, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_delay_multitap_tap(
    void* opaque, std::uint32_t tap, float delay_ms, float level, float pan) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_delay_multitap_tap(tap, delay_ms, level, pan);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_delay_tape_heads(
    void* opaque, std::uint32_t enabled, float spacing_ms,
    std::uint32_t selected_heads) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_delay_tape_heads(enabled != 0U, spacing_ms,
                                                     selected_heads);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_pitch(void* opaque, float semitones) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_pitch(semitones);
    }
}

} // extern "C"
