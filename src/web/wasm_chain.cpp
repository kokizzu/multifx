#include "tabletfx/effect.hpp"
#include "tabletfx/channel_conversion.hpp"
#include "tabletfx/effects.hpp"
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
#include "tabletfx/effects/shimmer_reverb.hpp"
#include "tabletfx/effects/convolution_reverb.hpp"
#include "tabletfx/effects/auto_swell.hpp"
#include "tabletfx/effects/auto_swell_delay.hpp"
#include "tabletfx/effects/auto_pan.hpp"
#include "tabletfx/effects/megatap_delay.hpp"
#include "tabletfx/effects/de_esser.hpp"
#include "tabletfx/effects/shuffling_looper.hpp"
#include "tabletfx/effects/ten_tap_delay.hpp"
#include "tabletfx/effects/rotary_speaker.hpp"
#include "tabletfx/effects/ring_modulator.hpp"
#include "tabletfx/effects/rhythmic_slicer.hpp"
#include "tabletfx/effects/step_filter.hpp"
#include "tabletfx/effects/notch_filter.hpp"
#include "tabletfx/effects/formant_filter.hpp"
#include "tabletfx/effects/four_band_resonator.hpp"
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
#include "tabletfx/effects/peak_limiter.hpp"
#include "tabletfx/effects/round_fuzz.hpp"
#include "tabletfx/effects/pedal_compressor.hpp"
#include "tabletfx/effects/phaser.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"
#include "tabletfx/effects/three_band_eq.hpp"
#include "tabletfx/effects/tremolo.hpp"
#include "tabletfx/effects/harmonic_tremolo.hpp"
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
        effects_.reserve(73);
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
        effects_.push_back({std::make_unique<tabletfx::HallReverb>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::PlateReverb>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::SpringReverb>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::AutoSwell>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::RotarySpeaker>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::RingModulator>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::RhythmicSlicer>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::StepFilter>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::NotchFilter>(
                                static_cast<float>(sample_rate)),
                            false});
        auto reverse_delay = std::make_unique<tabletfx::ReverseDelay>(
            static_cast<float>(sample_rate), channels);
        reverse_delay->set_enabled(false);
        effects_.push_back({std::move(reverse_delay), false});
        effects_.push_back({std::make_unique<tabletfx::DuckingDelay>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::ModulatedDelay>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::FilteredDelay>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::SweepDelay>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::FormantFilter>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::HarmonicResonator>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::MultibandCompressor>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::BitCrusher>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::DimensionChorus>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::TripleDoubleChorus>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::FourBandResonator>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::DiatonicHarmonizer>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::PitchArpeggiator>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::MonoGuitarSynth>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::NoteGenerator>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::GrainDelay>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::PitchFeedbackDelay>(
                                static_cast<float>(sample_rate), channels, 6000.0F),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::FeedbackGenerator>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::ShimmerReverb>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::ConvolutionReverb>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::FiveBandParametricEQ>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::Looper>(channels), false});
        effects_.push_back({std::make_unique<tabletfx::Enhancer>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::ResonantDelay>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::HarmonicTremolo>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::AutoSwellDelay>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::ShufflingLooper>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::TenTapDelay>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::AutoPan>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::MegatapDelay>(
                                static_cast<float>(sample_rate), 5000.0F),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::DeEsser>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::DualEngineDistortion>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::Vibe>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::MultiDelay>(
                                static_cast<float>(sample_rate), 2000.0F),
                            false});
        effects_.push_back({std::make_unique<tabletfx::PortaPreamp>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::PitchSequence>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::RhythmTapDelay>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::PlexDelay>(
                                static_cast<float>(sample_rate), 2000.0F),
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
        if ((index >= 28U && index <= 31U) && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 32U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 49U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 38U) {
            static_cast<tabletfx::ReverseDelay*>(effects_[index].effect.get())
                ->set_enabled(enabled);
        }
        if (index == 64U) {
            static_cast<tabletfx::AutoSwellDelay*>(effects_[index].effect.get())
                ->set_enabled(enabled);
        }
        if (index == 50U) {
            static_cast<tabletfx::effects::DiatonicHarmonizer*>(
                effects_[index].effect.get())->set_enabled(enabled);
        }
        if (index == 51U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 52U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 55U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 56U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 57U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 58U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 61U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 62U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 68U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 69U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 70U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 71U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 72U && effects_[index].enabled && !enabled) {
            effects_[index].effect->reset();
        }
        if (index == 75U && effects_[index].enabled && !enabled) {
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
        if (frames_ == 0 || index >= effects_.size() ||
            (!effects_[index].enabled && index != 38U && index != 64U)) {
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

    void configure_five_band_eq(float f1, float g1, float q1,
                                float f2, float g2, float q2,
                                float f3, float g3, float q3,
                                float f4, float g4, float q4,
                                float f5, float g5, float q5) noexcept {
        const std::array<tabletfx::FiveBandEQBand, 5> bands{{
            {f1, g1, q1}, {f2, g2, q2}, {f3, g3, q3},
            {f4, g4, q4}, {f5, g5, q5},
        }};
        static_cast<tabletfx::FiveBandParametricEQ*>(effects_[59].effect.get())
            ->configure(bands);
    }

    [[nodiscard]] bool looper_command(std::uint32_t command) noexcept {
        auto* looper = static_cast<tabletfx::Looper*>(effects_[60].effect.get());
        switch (command) {
        case 0U: return looper->record();
        case 1U: return looper->play();
        case 2U: return looper->overdub();
        case 3U:
            looper->stop();
            return true;
        case 4U: return looper->undo();
        case 5U: return looper->redo();
        case 6U: return looper->clear();
        default: return false;
        }
    }

    void configure_looper(bool reverse, bool half_speed,
                          float overdub_level) noexcept {
        auto* looper = static_cast<tabletfx::Looper*>(effects_[60].effect.get());
        looper->set_reverse(reverse);
        looper->set_half_speed(half_speed);
        looper->set_overdub_level(overdub_level);
    }

    [[nodiscard]] std::uint32_t looper_state() const noexcept {
        const auto* looper = static_cast<const tabletfx::Looper*>(
            effects_[60].effect.get());
        return static_cast<std::uint32_t>(looper->state());
    }

    [[nodiscard]] std::uint32_t looper_frames() const noexcept {
        const auto* looper = static_cast<const tabletfx::Looper*>(
            effects_[60].effect.get());
        const std::size_t frames = looper->loop_frames();
        return frames > std::numeric_limits<std::uint32_t>::max()
                   ? std::numeric_limits<std::uint32_t>::max()
                   : static_cast<std::uint32_t>(frames);
    }

    [[nodiscard]] bool shuffling_looper_command(std::uint32_t command) noexcept {
        auto* looper = static_cast<tabletfx::effects::ShufflingLooper*>(
            effects_[65].effect.get());
        switch (command) {
        case 0U: return looper->record();
        case 1U: return looper->stop_recording();
        case 2U: return looper->play();
        case 3U:
            looper->stop();
            return true;
        case 4U: return looper->randomize_sequence();
        case 5U: return looper->clear();
        default: return false;
        }
    }

    void configure_shuffling_looper(
        std::uint32_t slices, std::uint32_t sequence_length,
        float shuffle, float pitch_probability, float reverse_probability,
        float repeat_probability, float smoothing, float sequence_drift,
        float playback, float low_cut_hz, float high_cut_hz,
        std::int32_t interval1_semitones,
        std::int32_t interval2_semitones) noexcept {
        tabletfx::effects::ShufflingLooperParameters parameters;
        parameters.slices = static_cast<std::uint8_t>(
            std::clamp(slices, 2U, 16U));
        parameters.sequence_length = static_cast<std::uint8_t>(
            std::clamp(sequence_length, 1U, 32U));
        parameters.shuffle = shuffle;
        parameters.pitch_probability = pitch_probability;
        parameters.reverse_probability = reverse_probability;
        parameters.repeat_probability = repeat_probability;
        parameters.smoothing = smoothing;
        parameters.sequence_drift = sequence_drift;
        parameters.playback = playback;
        parameters.low_cut_hz = low_cut_hz;
        parameters.high_cut_hz = high_cut_hz;
        parameters.interval1_semitones = static_cast<std::int8_t>(
            std::clamp(interval1_semitones, -12, 12));
        parameters.interval2_semitones = static_cast<std::int8_t>(
            std::clamp(interval2_semitones, -12, 12));
        static_cast<tabletfx::effects::ShufflingLooper*>(
            effects_[65].effect.get())->configure(parameters);
    }

    [[nodiscard]] std::uint32_t shuffling_looper_state() const noexcept {
        const auto* looper = static_cast<const tabletfx::effects::ShufflingLooper*>(
            effects_[65].effect.get());
        return static_cast<std::uint32_t>(looper->state());
    }

    [[nodiscard]] std::uint32_t shuffling_looper_frames() const noexcept {
        const auto* looper = static_cast<const tabletfx::effects::ShufflingLooper*>(
            effects_[65].effect.get());
        const std::size_t frames = looper->loop_frames();
        return frames > std::numeric_limits<std::uint32_t>::max()
                   ? std::numeric_limits<std::uint32_t>::max()
                   : static_cast<std::uint32_t>(frames);
    }

    [[nodiscard]] std::uint32_t shuffling_looper_steps() const noexcept {
        return static_cast<std::uint32_t>(
            static_cast<const tabletfx::effects::ShufflingLooper*>(
                effects_[65].effect.get())->sequence_steps());
    }

    void configure_ten_tap_delay(
        std::uint32_t tap_count, float delay_time_ms, float tempo_bpm,
        std::uint32_t division, float decay_percent, float shuffle_percent,
        float spread_percent, float ratio_percent, std::uint32_t pan_shape,
        float pan_alpha_percent, float low_cut_hz, float high_cut_hz,
        float mix, float level_db, std::uint32_t mono) noexcept {
        auto* delay = static_cast<tabletfx::TenTapDelay*>(effects_[66].effect.get());
        auto parameters = delay->parameters();
        parameters.tap_count = static_cast<std::uint8_t>(
            std::clamp(tap_count, 1U, 10U));
        parameters.delay_time_ms = delay_time_ms;
        parameters.tempo_bpm = tempo_bpm;
        parameters.division = static_cast<tabletfx::TenTapDivision>(
            std::min(division, 15U));
        parameters.decay_percent = decay_percent;
        parameters.shuffle_percent = shuffle_percent;
        parameters.spread_percent = spread_percent;
        parameters.ratio_percent = ratio_percent;
        parameters.pan_shape = static_cast<tabletfx::TenTapPanShape>(
            std::min(pan_shape, 3U));
        parameters.pan_alpha_percent = pan_alpha_percent;
        parameters.low_cut_hz = low_cut_hz;
        parameters.high_cut_hz = high_cut_hz;
        parameters.mix = mix;
        parameters.level_db = level_db;
        parameters.mono = mono != 0U;
        delay->set_parameters(parameters);
    }

    void configure_ten_tap_delay_level(std::uint32_t tap, float level_db) noexcept {
        if (tap >= tabletfx::ten_tap_delay_max_taps) return;
        auto* delay = static_cast<tabletfx::TenTapDelay*>(effects_[66].effect.get());
        auto parameters = delay->parameters();
        parameters.tap_level_db[tap] = level_db;
        delay->set_parameters(parameters);
    }

    void configure_rhythm_tap_delay(const float* values,
                                    std::uint32_t value_count) noexcept {
        constexpr std::uint32_t kParameterCount = 33U;
        if (values == nullptr || value_count != kParameterCount) return;
        auto* delay = static_cast<tabletfx::RhythmTapDelay*>(
            effects_[75].effect.get());
        auto parameters = delay->parameters();
        const auto bounded_integer = [](float value,
                                        std::uint32_t maximum) noexcept {
            if (!std::isfinite(value)) return 0U;
            return static_cast<std::uint32_t>(std::clamp(
                value, 0.0F, static_cast<float>(maximum)));
        };
        parameters.common.tap_count = static_cast<std::uint8_t>(
            std::clamp(bounded_integer(values[0], 10U), 1U, 10U));
        parameters.common.tempo_bpm = values[1];
        parameters.quantize = static_cast<tabletfx::TenTapDivision>(
            std::min(bounded_integer(values[2], 15U), 15U));
        parameters.feedback_percent = values[3];
        parameters.common.decay_percent = values[4];
        parameters.common.spread_percent = values[5];
        parameters.common.pan_shape = static_cast<tabletfx::TenTapPanShape>(
            std::min(bounded_integer(values[6], 3U), 3U));
        parameters.common.pan_alpha_percent = values[7];
        parameters.common.low_cut_hz = values[8];
        parameters.common.high_cut_hz = values[9];
        parameters.common.mix = values[10];
        parameters.common.level_db = values[11];
        parameters.common.mono = bounded_integer(values[12], 1U) != 0U;
        for (std::size_t tap = 0; tap < tabletfx::ten_tap_delay_max_taps; ++tap) {
            parameters.tap_time_values[tap] = values[13U + tap];
            parameters.common.tap_level_db[tap] = values[23U + tap];
        }
        delay->set_parameters(parameters);
    }

    void configure_plex_delay_global(
        std::uint32_t active_lines, float tempo_bpm, float master_time,
        float master_level, float master_pan, float decay_seconds,
        float diffusion, float low_cut_hz, float high_cut_hz,
        float pre_delay_ms, std::uint32_t stack_mode, float input_gain_db,
        float mix, float level_db, float balance) noexcept {
        auto* delay = static_cast<tabletfx::PlexDelay*>(
            effects_[76].effect.get());
        auto parameters = delay->parameters();
        parameters.active_lines = static_cast<std::uint8_t>(
            std::clamp(active_lines, 1U, 8U));
        parameters.tempo_bpm = tempo_bpm;
        parameters.master_time = master_time;
        parameters.master_level = master_level;
        parameters.master_pan = master_pan;
        parameters.decay_seconds = decay_seconds;
        parameters.diffusion = diffusion;
        parameters.low_cut_hz = low_cut_hz;
        parameters.high_cut_hz = high_cut_hz;
        parameters.pre_delay_ms = pre_delay_ms;
        parameters.stack_mode = static_cast<tabletfx::PlexDelayStackMode>(
            std::min(stack_mode, 2U));
        parameters.input_gain_db = input_gain_db;
        parameters.mix = mix;
        parameters.level_db = level_db;
        parameters.balance = balance;
        delay->configure(parameters);
    }

    void configure_plex_delay_line(std::uint32_t line, float time_ms,
                                   std::uint32_t time_mode,
                                   std::uint32_t division, float level,
                                   float pan) noexcept {
        if (line >= tabletfx::plex_delay_line_count) return;
        auto* delay = static_cast<tabletfx::PlexDelay*>(
            effects_[76].effect.get());
        auto parameters = delay->parameters();
        parameters.time_ms[line] = time_ms;
        parameters.time_mode[line] = static_cast<tabletfx::DelayTimeMode>(
            std::min(time_mode, 1U));
        parameters.division[line] = static_cast<tabletfx::DelayDivision>(
            std::min(division, 11U));
        parameters.line_level[line] = level;
        parameters.line_pan[line] = pan;
        delay->configure(parameters);
    }

    void configure_auto_pan(float rate_hz, float depth, std::uint32_t waveform,
                            std::uint32_t tempo_sync, float tempo_bpm,
                            std::uint32_t division) noexcept {
        auto* panner = static_cast<tabletfx::AutoPan*>(effects_[67].effect.get());
        panner->set_rate_hz(rate_hz);
        panner->set_depth(depth);
        panner->set_waveform(static_cast<tabletfx::AutoPanWaveform>(waveform));
        panner->set_tempo_sync(tempo_sync != 0U, tempo_bpm,
                               static_cast<tabletfx::AutoPanDivision>(division));
    }

    void configure_megatap_delay(
        float delay_time_ms, std::uint32_t tap_count, float time_alpha,
        float time_randomize_percent, std::uint32_t amplitude_shape,
        float amplitude_alpha, float amplitude_randomize_percent,
        std::uint32_t pan_shape, float pan_alpha_percent,
        float stereo_spread_percent, float predelay_ms, float feedback,
        std::uint32_t feedback_tap, float low_cut_hz, float high_cut_hz,
        float diffusion_mix, float diffusion_time_ms,
        float diffusion_lfo_rate_hz, float diffusion_lfo_depth_ms,
        float envelope_threshold_db, float envelope_attack_ms,
        float envelope_release_ms, float input_gain_db, float mix,
        float level_db, std::uint32_t input_select,
        std::uint32_t envelope_enabled) noexcept {
        auto* delay = static_cast<tabletfx::MegatapDelay*>(effects_[68].effect.get());
        auto parameters = delay->parameters();
        parameters.delay_time_ms = delay_time_ms;
        parameters.tap_count = static_cast<std::uint8_t>(
            std::clamp(tap_count, 1U, 64U));
        parameters.time_alpha = time_alpha;
        parameters.time_randomize_percent = time_randomize_percent;
        parameters.amplitude_shape = static_cast<tabletfx::MegatapAmplitudeShape>(
            std::min(amplitude_shape, 4U));
        parameters.amplitude_alpha = amplitude_alpha;
        parameters.amplitude_randomize_percent = amplitude_randomize_percent;
        parameters.pan_shape = static_cast<tabletfx::MegatapPanShape>(
            std::min(pan_shape, 3U));
        parameters.pan_alpha_percent = pan_alpha_percent;
        parameters.stereo_spread_percent = stereo_spread_percent;
        parameters.predelay_ms = predelay_ms;
        parameters.feedback = feedback;
        parameters.feedback_tap = static_cast<std::uint8_t>(
            std::clamp(feedback_tap, 1U, 64U));
        parameters.low_cut_hz = low_cut_hz;
        parameters.high_cut_hz = high_cut_hz;
        parameters.diffusion_mix = diffusion_mix;
        parameters.diffusion_time_ms = diffusion_time_ms;
        parameters.diffusion_lfo_rate_hz = diffusion_lfo_rate_hz;
        parameters.diffusion_lfo_depth_ms = diffusion_lfo_depth_ms;
        parameters.envelope_threshold_db = envelope_threshold_db;
        parameters.envelope_attack_ms = envelope_attack_ms;
        parameters.envelope_release_ms = envelope_release_ms;
        parameters.input_gain_db = input_gain_db;
        parameters.mix = mix;
        parameters.level_db = level_db;
        parameters.input_select = static_cast<tabletfx::MegatapInputSelect>(
            std::min(input_select, 2U));
        parameters.envelope_enabled = envelope_enabled != 0U;
        delay->set_parameters(parameters);
    }

    void configure_de_esser(float detector_frequency_hz, float detector_q,
                            float threshold_dbfs, float max_reduction_db,
                            float attack_ms, float release_ms) noexcept {
        auto* de_esser = static_cast<tabletfx::effects::DeEsser*>(
            effects_[69].effect.get());
        auto parameters = de_esser->parameters();
        parameters.detector_frequency_hz = detector_frequency_hz;
        parameters.detector_q = detector_q;
        parameters.threshold_dbfs = threshold_dbfs;
        parameters.max_reduction_db = max_reduction_db;
        parameters.attack_ms = attack_ms;
        parameters.release_ms = release_ms;
        de_esser->configure(parameters);
    }

    void configure_dual_engine_distortion(
        float gain_a, float tone_a, float gain_b, float tone_b,
        float blend, std::uint32_t mode, std::uint32_t bass_boost,
        float level_db) noexcept {
        auto* distortion = static_cast<tabletfx::DualEngineDistortion*>(
            effects_[70].effect.get());
        auto parameters = distortion->parameters();
        parameters.gain_a = gain_a;
        parameters.tone_a = tone_a;
        parameters.gain_b = gain_b;
        parameters.tone_b = tone_b;
        parameters.blend = blend;
        parameters.mode = mode == 1U
            ? tabletfx::DualEngineDistortionMode::Series
            : tabletfx::DualEngineDistortionMode::Blend;
        parameters.bass_boost = bass_boost != 0U;
        parameters.level_db = level_db;
        distortion->set_parameters(parameters);
    }

    void configure_vibe(float rate_hz, float depth, float trem,
                        std::uint32_t stages, float spread,
                        float level) noexcept {
        auto* vibe = static_cast<tabletfx::Vibe*>(effects_[71].effect.get());
        vibe->set_parameters({rate_hz, depth, trem,
                              static_cast<std::uint8_t>(stages), spread, level});
    }

    void configure_multi_delay_line(
        std::uint32_t line, float time_ms, std::uint32_t time_mode,
        std::uint32_t division, float feedback, float cross_feedback,
        float level, float pan) noexcept {
        if (line >= tabletfx::multi_delay_line_count) return;
        auto* delay = static_cast<tabletfx::MultiDelay*>(effects_[72].effect.get());
        auto parameters = delay->parameters();
        parameters.time_ms[line] = time_ms;
        parameters.time_mode[line] = time_mode <= 1U
            ? static_cast<tabletfx::DelayTimeMode>(time_mode)
            : static_cast<tabletfx::DelayTimeMode>(255U);
        parameters.division[line] = division < 12U
            ? static_cast<tabletfx::DelayDivision>(division)
            : static_cast<tabletfx::DelayDivision>(255U);
        parameters.feedback[line] = feedback;
        parameters.cross_feedback[line] = cross_feedback;
        parameters.level[line] = level;
        parameters.pan[line] = pan;
        delay->configure(parameters);
    }

    void configure_multi_delay_global(
        float tempo_bpm, float low_cut_hz, float high_cut_hz,
        float input_gain_db, float mix, float level_db) noexcept {
        auto* delay = static_cast<tabletfx::MultiDelay*>(effects_[72].effect.get());
        auto parameters = delay->parameters();
        parameters.tempo_bpm = tempo_bpm;
        parameters.low_cut_hz = low_cut_hz;
        parameters.high_cut_hz = high_cut_hz;
        parameters.input_gain_db = input_gain_db;
        parameters.mix = mix;
        parameters.level_db = level_db;
        delay->configure(parameters);
    }

    void configure_porta_preamp(float gain_1_db, float gain_2_db,
                                float bass_db, float treble_db,
                                float level_db) noexcept {
        auto* preamp = static_cast<tabletfx::PortaPreamp*>(
            effects_[73].effect.get());
        auto parameters = preamp->parameters();
        parameters.gain_1_db = gain_1_db;
        parameters.gain_2_db = gain_2_db;
        parameters.bass_db = bass_db;
        parameters.treble_db = treble_db;
        parameters.level_db = level_db;
        preamp->set_parameters(parameters);
    }

    void configure_pitch_sequence(const float* values,
                                  std::uint32_t value_count) noexcept {
        constexpr std::uint32_t kParameterCount = 44U;
        if (values == nullptr || value_count != kParameterCount) return;
        const auto selector = [](float value, std::uint32_t maximum) noexcept {
            if (!std::isfinite(value)) return 0U;
            return static_cast<std::uint32_t>(std::clamp(
                std::lround(value), 0L, static_cast<long>(maximum)));
        };
        tabletfx::effects::PitchSequenceParameters parameters;
        for (std::size_t step = 0; step < parameters.semitones.size(); ++step) {
            const float requested = values[step];
            parameters.semitones[step] = std::isfinite(requested)
                ? static_cast<int>(std::lround(std::clamp(requested, -12.0F, 12.0F)))
                : 0;
            parameters.mode[step] = static_cast<
                tabletfx::effects::PitchSequenceStepMode>(
                    selector(values[16U + step], 2U));
        }
        parameters.steps = static_cast<std::uint8_t>(selector(values[32], 16U));
        parameters.run = std::isfinite(values[33]) && values[33] >= 0.5F;
        parameters.time_mode = selector(values[34], 1U) == 1U
            ? tabletfx::DelayTimeMode::tempo_sync
            : tabletfx::DelayTimeMode::milliseconds;
        parameters.division = static_cast<tabletfx::DelayDivision>(
            selector(values[35], static_cast<std::uint32_t>(
                tabletfx::DelayDivision::sixteenth_triplet)));
        parameters.free_step_ms = values[36];
        parameters.tempo_bpm = values[37];
        parameters.glide_ms = values[38];
        parameters.glitch_depth_semitones = static_cast<std::uint8_t>(
            selector(values[39], 12U));
        parameters.glitch_interval_ms = values[40];
        parameters.freeze_window_ms = values[41];
        parameters.mix = values[42];
        parameters.level = values[43];
        static_cast<tabletfx::effects::PitchSequence*>(effects_[74].effect.get())
            ->configure(parameters);
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

    void configure_harmonic_tremolo(float rate_hz, float depth,
                                    float crossover_hz,
                                    std::uint32_t waveform,
                                    std::uint32_t tempo_sync,
                                    float tempo_bpm, float beats_per_cycle,
                                    std::uint32_t stereo_alternation,
                                    float smoothing_ms) noexcept {
        auto* tremolo = static_cast<tabletfx::HarmonicTremolo*>(
            effects_[63].effect.get());
        tremolo->set_rate_hz(rate_hz);
        tremolo->set_depth(depth);
        tremolo->set_crossover_hz(crossover_hz);
        switch (waveform) {
        case 1:
            tremolo->set_waveform(tabletfx::HarmonicTremoloWaveform::Triangle);
            break;
        case 2:
            tremolo->set_waveform(tabletfx::HarmonicTremoloWaveform::Square);
            break;
        default:
            tremolo->set_waveform(tabletfx::HarmonicTremoloWaveform::Sine);
            break;
        }
        tremolo->set_tempo_sync(tempo_sync != 0, tempo_bpm, beats_per_cycle);
        tremolo->set_stereo_alternation(stereo_alternation != 0);
        tremolo->set_smoothing_ms(smoothing_ms);
    }

    void configure_auto_swell_delay(
        float time_ms, float feedback, float mix, std::uint32_t time_mode,
        std::uint32_t tap_division, float tempo_bpm, float swell_ms, float mod,
        float tone, float sense_db, std::uint32_t taper,
        std::uint32_t dry, std::uint32_t spillover) noexcept {
        tabletfx::AutoSwellDelayParameters parameters;
        parameters.time_ms = time_ms;
        parameters.feedback = feedback;
        parameters.mix = mix;
        parameters.time_mode = time_mode == 1U
            ? tabletfx::DelayTimeMode::tempo_sync
            : tabletfx::DelayTimeMode::milliseconds;
        parameters.tap_division = static_cast<tabletfx::DelayDivision>(
            std::min(tap_division, 11U));
        parameters.tempo_bpm = tempo_bpm;
        parameters.swell_ms = swell_ms;
        parameters.mod = mod;
        parameters.tone = tone;
        parameters.sense_db = sense_db;
        parameters.taper = static_cast<tabletfx::AutoSwellDelayTaper>(
            std::min(taper, 2U));
        parameters.dry = dry != 0U;
        parameters.spillover = spillover != 0U;
        static_cast<tabletfx::AutoSwellDelay*>(effects_[64].effect.get())
            ->configure(parameters);
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

    void configure_dimension_chorus(std::uint32_t version, std::uint32_t mode,
                                    float mix, float level_db) noexcept {
        tabletfx::DimensionChorusParameters parameters;
        parameters.version = static_cast<tabletfx::DimensionChorusVersion>(version);
        parameters.mode = static_cast<tabletfx::DimensionChorusMode>(mode);
        parameters.mix = mix;
        parameters.level_db = level_db;
        static_cast<tabletfx::DimensionChorus*>(effects_[47].effect.get())
            ->set_parameters(parameters);
    }

    void configure_triple_double_chorus(
        std::uint32_t preset_switch, std::uint32_t manual_switch,
        float rate_hz, float left_depth_ms, float center_depth_ms,
        float right_depth_ms, float feedback, float sparkle,
        std::uint32_t stereo_switch) noexcept {
        tabletfx::effects::TripleDoubleChorusConfig config;
        config.preset_switch = preset_switch != 0;
        config.manual_switch = manual_switch != 0;
        config.rate_hz = rate_hz;
        config.left_depth_ms = left_depth_ms;
        config.center_depth_ms = center_depth_ms;
        config.right_depth_ms = right_depth_ms;
        config.feedback = feedback;
        config.sparkle = sparkle;
        config.stereo_switch = stereo_switch != 0;
        static_cast<tabletfx::effects::TripleDoubleChorus*>(
            effects_[48].effect.get())->configure(config);
    }

    void configure_four_band_resonator(const float* values,
                                       std::uint32_t value_count) noexcept {
        constexpr std::uint32_t kParameterCount = 34U;
        if (values == nullptr || value_count != kParameterCount) {
            return;
        }
        const auto selector = [](float value, std::uint32_t maximum) noexcept {
            if (!std::isfinite(value)) {
                return 0U;
            }
            return static_cast<std::uint32_t>(
                std::clamp(value, 0.0F, static_cast<float>(maximum)) + 0.5F);
        };
        tabletfx::FourBandResonatorParameters parameters;
        parameters.mode = static_cast<tabletfx::FourBandResonatorMode>(
            selector(values[0], 1U));
        parameters.chord = static_cast<tabletfx::FourBandResonatorChord>(
            selector(values[1], 8U));
        parameters.master_frequency = values[2];
        for (std::size_t band = 0; band < 4U; ++band) {
            parameters.manual_frequency_hz[band] = values[3U + band];
        }
        parameters.chord_root_frequency_hz = values[7];
        for (std::size_t band = 0; band < 4U; ++band) {
            parameters.feedback[band] = values[8U + band];
            parameters.q[band] = values[12U + band];
            parameters.level[band] = values[16U + band];
            parameters.pan[band] = values[20U + band];
            parameters.filter_location[band] =
                static_cast<tabletfx::FourBandResonatorFilterLocation>(
                    selector(values[24U + band], 1U));
        }
        parameters.input_mode = static_cast<tabletfx::FourBandResonatorInputMode>(
            selector(values[28], 1U));
        parameters.master_level = values[29];
        parameters.master_pan = values[30];
        parameters.master_feedback = values[31];
        parameters.master_q = values[32];
        parameters.mix = values[33];
        static_cast<tabletfx::FourBandResonator*>(effects_[49].effect.get())
            ->set_parameters(parameters);
    }

    void configure_diatonic_harmonizer(float mix, float pitch_interval,
                                        std::uint32_t key, std::uint32_t scale,
                                        float tone_hz, float delay_ms,
                                        float feedback, std::uint32_t tap_division,
                                        float tempo_bpm) noexcept {
        tabletfx::effects::DiatonicHarmonizerConfig config;
        config.mix = mix;
        config.pitch_interval = static_cast<int>(std::lround(pitch_interval));
        config.key = static_cast<int>(key);
        config.scale = static_cast<tabletfx::effects::DiatonicScale>(scale);
        config.tone_hz = tone_hz;
        config.delay_ms = delay_ms;
        config.feedback = feedback;
        config.tap_division = static_cast<tabletfx::effects::TapDivision>(tap_division);
        config.tempo_bpm = tempo_bpm;
        config.spillover = false;
        static_cast<tabletfx::effects::DiatonicHarmonizer*>(effects_[50].effect.get())
            ->configure(config);
    }

    void configure_pitch_arpeggiator(const float* values,
                                      std::uint32_t value_count) noexcept {
        constexpr std::uint32_t kParameterCount = 31U;
        if (values == nullptr || value_count != kParameterCount) {
            return;
        }
        const auto selector = [](float value, std::uint32_t maximum) noexcept {
            if (!std::isfinite(value)) {
                return 0U;
            }
            return static_cast<std::uint32_t>(
                std::clamp(value, 0.0F, static_cast<float>(maximum)) + 0.5F);
        };
        tabletfx::effects::PitchArpeggiatorParameters parameters;
        parameters.run = std::isfinite(values[0]) && values[0] >= 0.5F;
        parameters.mix = values[1];
        parameters.output_level = values[2];
        parameters.key = static_cast<int>(selector(values[3], 11U));
        parameters.scale = static_cast<tabletfx::effects::DiatonicScale>(
            selector(values[4], static_cast<std::uint32_t>(
                                    tabletfx::effects::DiatonicScale::Chromatic)));
        parameters.steps = static_cast<std::uint8_t>(selector(values[5], 16U));
        parameters.repeats = static_cast<std::uint8_t>(selector(values[6], 64U));
        parameters.step_division =
            static_cast<tabletfx::effects::PitchArpeggiatorDivision>(
                selector(values[7], static_cast<std::uint32_t>(
                                        tabletfx::effects::PitchArpeggiatorDivision::ThirtySecond)));
        parameters.tempo_bpm = values[8];
        parameters.free_step_ms = values[9];
        parameters.amplitude_shape =
            static_cast<tabletfx::effects::PitchArpeggiatorAmplitudeShape>(
                selector(values[10], static_cast<std::uint32_t>(
                                         tabletfx::effects::PitchArpeggiatorAmplitudeShape::Triangle)));
        parameters.amplitude_alpha = values[11];
        parameters.pan_shape =
            static_cast<tabletfx::effects::PitchArpeggiatorPanShape>(
                selector(values[12], static_cast<std::uint32_t>(
                                         tabletfx::effects::PitchArpeggiatorPanShape::PingPong)));
        parameters.pan_alpha = values[13];
        parameters.trigger_threshold_dbfs = values[30];
        for (std::size_t step = 0; step < parameters.stage_shift_scale_degrees.size(); ++step) {
            const float requested = values[14U + step];
            parameters.stage_shift_scale_degrees[step] = std::isfinite(requested)
                ? static_cast<int>(std::lround(std::clamp(requested, -7.0F, 7.0F)))
                : 0;
        }
        static_cast<tabletfx::effects::PitchArpeggiator*>(effects_[51].effect.get())
            ->configure(parameters);
    }

    void configure_mono_guitar_synth(const float* values,
                                     std::uint32_t value_count) noexcept {
        constexpr std::uint32_t kParameterCount = 7U;
        if (values == nullptr || value_count != kParameterCount) {
            return;
        }
        const auto selector = [](float value, std::uint32_t maximum) noexcept {
            if (!std::isfinite(value)) {
                return 2U;
            }
            return static_cast<std::uint32_t>(
                std::clamp(value, 0.0F, static_cast<float>(maximum)) + 0.5F);
        };
        tabletfx::effects::MonoGuitarSynthConfig config;
        config.waveform = static_cast<tabletfx::effects::MonoGuitarSynthWaveform>(
            selector(values[0], 3U));
        config.mix = values[1];
        config.level = values[2];
        config.gate_threshold_db = values[3];
        config.attack_ms = values[4];
        config.release_ms = values[5];
        config.portamento_ms = values[6];
        static_cast<tabletfx::effects::MonoGuitarSynth*>(effects_[52].effect.get())
            ->configure(config);
    }

    void configure_note_generator(const float* values,
                                  std::uint32_t value_count) noexcept {
        constexpr std::uint32_t kParameterCount = 14U;
        if (values == nullptr || value_count != kParameterCount) return;
        const auto selector = [](float value, std::uint32_t maximum) noexcept {
            if (!std::isfinite(value)) return 0U;
            return static_cast<std::uint32_t>(
                std::clamp(value, 0.0F, static_cast<float>(maximum)) + 0.5F);
        };
        const auto integer = [](float value, int minimum, int maximum,
                                int fallback) noexcept {
            if (!std::isfinite(value)) return fallback;
            return static_cast<int>(std::lround(std::clamp(
                value, static_cast<float>(minimum), static_cast<float>(maximum))));
        };
        tabletfx::effects::NoteGeneratorParameters parameters;
        parameters.voice_count = static_cast<std::uint8_t>(selector(values[0], 3U));
        for (std::size_t voice = 0; voice < parameters.voices.size(); ++voice) {
            const std::size_t offset = 1U + voice * 4U;
            parameters.voices[voice].waveform =
                static_cast<tabletfx::effects::NoteGeneratorWaveform>(
                    selector(values[offset], 3U));
            parameters.voices[voice].note_number =
                integer(values[offset + 1U], 0, 127, 60);
            parameters.voices[voice].octave =
                integer(values[offset + 2U], -2, 2, 0);
            parameters.voices[voice].level = values[offset + 3U];
        }
        parameters.output_level = values[13U];
        static_cast<tabletfx::effects::NoteGenerator*>(effects_[53].effect.get())
            ->configure(parameters);
    }

    void configure_enhancer(const float* values,
                            std::uint32_t value_count) noexcept {
        constexpr std::uint32_t kParameterCount = 11U;
        if (values == nullptr || value_count != kParameterCount) return;
        const auto integer = [](float value, int minimum, int maximum,
                                int fallback) noexcept {
            if (!std::isfinite(value)) return fallback;
            return static_cast<int>(std::lround(std::clamp(
                value, static_cast<float>(minimum), static_cast<float>(maximum))));
        };
        tabletfx::EnhancerParameters parameters;
        parameters.mode = static_cast<tabletfx::EnhancerMode>(
            integer(values[0], 0, 2, 0));
        parameters.width = values[1];
        parameters.depth = values[2];
        parameters.low_cut_hz = values[3];
        parameters.high_cut_hz = values[4];
        parameters.classic_delay_ms = values[5];
        parameters.phase_invert = static_cast<tabletfx::EnhancerPhaseInvert>(
            integer(values[6], 0, 3, 0));
        parameters.pan_left = values[7];
        parameters.pan_right = values[8];
        parameters.balance = values[9];
        parameters.mix = values[10];
        static_cast<tabletfx::Enhancer*>(effects_[61].effect.get())
            ->set_parameters(parameters);
    }

    void configure_resonant_delay(const float* values,
                                  std::uint32_t value_count) noexcept {
        if (values == nullptr || value_count != 5U) return;
        tabletfx::ResonantDelayParameters parameters;
        parameters.time_ms = values[0];
        parameters.feedback = values[1];
        parameters.center_hz = values[2];
        parameters.resonance_q = values[3];
        parameters.mix = values[4];
        static_cast<tabletfx::ResonantDelay*>(effects_[62].effect.get())
            ->configure(parameters);
    }

    void configure_grain_delay(float time_ms, float feedback, float mix,
                               float grain_size_ms, std::uint32_t overlap,
                               float pitch_semitones, float spray_ms) noexcept {
        tabletfx::GrainDelayParameters parameters;
        parameters.time_ms = time_ms;
        parameters.feedback = feedback;
        parameters.mix = mix;
        parameters.grain_size_ms = grain_size_ms;
        parameters.overlap = static_cast<std::uint8_t>(
            std::clamp(overlap, 0U, 255U));
        parameters.pitch_semitones = pitch_semitones;
        parameters.spray_ms = spray_ms;
        static_cast<tabletfx::GrainDelay*>(effects_[54].effect.get())
            ->configure(parameters);
    }

    void configure_pitch_feedback_delay(float time_ms, float feedback,
                                        float mix, float pitch_semitones) noexcept {
        tabletfx::PitchFeedbackDelayParameters parameters;
        parameters.time_ms = time_ms;
        parameters.feedback = feedback;
        parameters.mix = mix;
        parameters.pitch_semitones = pitch_semitones;
        static_cast<tabletfx::PitchFeedbackDelay*>(effects_[55].effect.get())
            ->configure(parameters);
    }

    void configure_feedback_generator(
        std::uint32_t type, float gain, float attack_ms, float release_ms,
        std::uint32_t dry_kill, float dry_level,
        float silence_threshold_db) noexcept {
        tabletfx::effects::FeedbackGeneratorParameters parameters;
        parameters.type = static_cast<tabletfx::effects::FeedbackGeneratorType>(type);
        parameters.feedback_gain = gain;
        parameters.attack_ms = attack_ms;
        parameters.release_ms = release_ms;
        parameters.dry_kill = static_cast<tabletfx::effects::FeedbackGeneratorDryKill>(dry_kill);
        parameters.dry_level = dry_level;
        parameters.silence_threshold_db = silence_threshold_db;
        static_cast<tabletfx::effects::FeedbackGenerator*>(effects_[56].effect.get())
            ->configure(parameters);
    }

    void configure_shimmer_reverb(const float* values) noexcept {
        if (values == nullptr) return;
        tabletfx::ShimmerReverbParameters parameters;
        parameters.decay_seconds = values[0];
        parameters.damping_hz = values[1];
        parameters.diffusion = values[2];
        parameters.feedback = values[3];
        parameters.high_pass_hz = values[4];
        parameters.pitch_semitones = values[5];
        parameters.shimmer_intensity = values[6];
        parameters.mix = values[7];
        parameters.level_db = values[8];
        static_cast<tabletfx::ShimmerReverb*>(effects_[57].effect.get())
            ->configure(parameters);
    }

    void configure_convolution_reverb(float mix, float level_db) noexcept {
        tabletfx::ConvolutionReverbParameters parameters;
        parameters.mix = mix;
        parameters.level_db = level_db;
        static_cast<tabletfx::ConvolutionReverb*>(effects_[58].effect.get())
            ->configure(parameters);
    }

    [[nodiscard]] bool load_convolution_reverb_ir(
        const float* samples, std::uint32_t frames,
        std::uint32_t ir_channels, std::uint32_t ir_sample_rate) noexcept {
        if (samples == nullptr || frames == 0 ||
            (ir_channels != 1 && ir_channels != 2) ||
            frames > tabletfx::ConvolutionReverb::maximum_impulse_frames()) {
            return false;
        }
        const std::size_t sample_count = static_cast<std::size_t>(frames) *
                                         static_cast<std::size_t>(ir_channels);
        return static_cast<tabletfx::ConvolutionReverb*>(
                   effects_[58].effect.get())
            ->load_impulse_response(
                std::span<const float>(samples, sample_count),
                static_cast<std::uint16_t>(ir_channels), ir_sample_rate);
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

    void configure_hall_reverb(float decay_seconds, float pre_delay_ms,
                               float damping_hz, float low_cut_hz, float mix,
                               float level_db) noexcept {
        auto* reverb = static_cast<tabletfx::HallReverb*>(effects_[29].effect.get());
        tabletfx::HallReverbParameters parameters;
        parameters.decay_seconds = decay_seconds;
        parameters.pre_delay_ms = pre_delay_ms;
        parameters.damping_hz = damping_hz;
        parameters.low_cut_hz = low_cut_hz;
        parameters.mix = mix;
        parameters.level_db = level_db;
        reverb->configure(parameters);
    }

    void configure_plate_reverb(float decay_seconds, float pre_delay_ms,
                                float damping_hz, float diffusion, float balance,
                                float mix, float level_db) noexcept {
        auto* reverb = static_cast<tabletfx::PlateReverb*>(effects_[30].effect.get());
        tabletfx::PlateReverbParameters parameters;
        parameters.decay_seconds = decay_seconds;
        parameters.pre_delay_ms = pre_delay_ms;
        parameters.damping_hz = damping_hz;
        parameters.diffusion = diffusion;
        parameters.balance = balance;
        parameters.mix = mix;
        parameters.level_db = level_db;
        reverb->configure(parameters);
    }

    void configure_spring_reverb(float decay_seconds, float pre_delay_ms,
                                 float damping_hz, float dwell, float mix,
                                 float level_db) noexcept {
        auto* reverb = static_cast<tabletfx::SpringReverb*>(effects_[31].effect.get());
        tabletfx::SpringReverbParameters parameters;
        parameters.decay_seconds = decay_seconds;
        parameters.pre_delay_ms = pre_delay_ms;
        parameters.damping_hz = damping_hz;
        parameters.dwell = dwell;
        parameters.mix = mix;
        parameters.level_db = level_db;
        reverb->configure(parameters);
    }

    void configure_auto_swell(float threshold_db, float attack_ms,
                              float release_ms, float hysteresis_db,
                              std::uint32_t taper, float volume) noexcept {
        auto* swell = static_cast<tabletfx::AutoSwell*>(effects_[32].effect.get());
        tabletfx::AutoSwellParameters parameters;
        parameters.threshold_db = threshold_db;
        parameters.attack_ms = attack_ms;
        parameters.release_ms = release_ms;
        parameters.hysteresis_db = hysteresis_db;
        parameters.taper = static_cast<tabletfx::AutoSwellTaper>(taper);
        parameters.volume = volume;
        swell->configure(static_cast<float>(sample_rate_), parameters);
    }

    void configure_rotary_speaker(
        float rate_hz, std::uint32_t tempo_sync, float tempo_bpm,
        float beats_per_rotation, std::uint32_t speed,
        float slow_speed_multiplier, float drive, float hi_depth,
        float high_level, float low_depth, float rotor_length,
        float low_rate_multi, float low_time_constant_ms,
        float hi_time_constant_ms, float low_mic_spacing,
        float high_mic_spacing, float mic_distance,
        float stereo_spread, float mix, float level, float balance,
        std::uint32_t input_select) noexcept {
        auto* rotary = static_cast<tabletfx::RotarySpeaker*>(effects_[33].effect.get());
        tabletfx::RotarySpeakerParameters parameters;
        parameters.rate_hz = rate_hz;
        parameters.tempo_sync = tempo_sync != 0;
        parameters.tempo_bpm = tempo_bpm;
        parameters.beats_per_rotation = beats_per_rotation;
        parameters.speed = static_cast<tabletfx::RotarySpeed>(speed);
        parameters.slow_speed_multiplier = slow_speed_multiplier;
        parameters.drive = drive;
        parameters.hi_depth = hi_depth;
        parameters.high_level = high_level;
        parameters.low_depth = low_depth;
        parameters.rotor_length = rotor_length;
        parameters.low_rate_multi = low_rate_multi;
        parameters.low_time_constant_ms = low_time_constant_ms;
        parameters.hi_time_constant_ms = hi_time_constant_ms;
        parameters.low_mic_spacing = low_mic_spacing;
        parameters.high_mic_spacing = high_mic_spacing;
        parameters.mic_distance = mic_distance;
        parameters.stereo_spread = stereo_spread;
        parameters.mix = mix;
        parameters.level = level;
        parameters.balance = balance;
        parameters.input_select = static_cast<tabletfx::RotaryInputSelect>(input_select);
        rotary->set_parameters(parameters);
    }

    void configure_ring_modulator(float frequency_hz, float frequency_multiplier,
                                  float high_cut_hz, float mix) noexcept {
        auto* ring_modulator =
            static_cast<tabletfx::RingModulator*>(effects_[34].effect.get());
        tabletfx::RingModulatorParameters parameters;
        parameters.frequency_hz = frequency_hz;
        parameters.frequency_multiplier = frequency_multiplier;
        parameters.high_cut_hz = high_cut_hz;
        parameters.mix = mix;
        ring_modulator->set_parameters(parameters);
    }

    void configure_rhythmic_slicer(
        std::uint32_t tempo_sync, float tempo_bpm, std::uint32_t division,
        float rate_hz, float depth, float shape, float attack_ms,
        float release_ms, float phase, const std::array<float, 16>& steps) noexcept {
        auto* slicer = static_cast<tabletfx::RhythmicSlicer*>(
            effects_[35].effect.get());
        const float previous_phase = slicer->parameters().phase;
        slicer->set_tempo_sync(tempo_sync != 0U, tempo_bpm,
                               static_cast<tabletfx::RhythmicDivision>(division));
        slicer->set_rate_hz(rate_hz);
        slicer->set_depth(depth);
        slicer->set_shape(shape);
        slicer->set_edge_times_ms(attack_ms, release_ms);
        slicer->set_steps(steps);
        if (phase != previous_phase) {
            slicer->set_phase(phase);
        }
    }

    void configure_step_filter(
        std::uint32_t tempo_sync, float tempo_bpm, std::uint32_t division,
        float rate_hz, std::uint32_t mode, float minimum_cutoff_hz,
        float maximum_cutoff_hz, float resonance_q, float depth, float shape,
        float smoothing_ms, float mix, float phase,
        const std::array<float, 16>& steps) noexcept {
        auto* step_filter = static_cast<tabletfx::StepFilter*>(
            effects_[36].effect.get());
        const float previous_phase = step_filter->parameters().phase;
        auto parameters = step_filter->parameters();
        parameters.tempo_sync = tempo_sync != 0U;
        parameters.tempo_bpm = tempo_bpm;
        parameters.division =
            static_cast<tabletfx::RhythmicDivision>(division);
        parameters.rate_hz = rate_hz;
        parameters.mode = static_cast<tabletfx::StepFilterMode>(mode);
        parameters.minimum_cutoff_hz = minimum_cutoff_hz;
        parameters.maximum_cutoff_hz = maximum_cutoff_hz;
        parameters.resonance_q = resonance_q;
        parameters.depth = depth;
        parameters.shape = shape;
        parameters.smoothing_ms = smoothing_ms;
        parameters.mix = mix;
        parameters.phase = phase;
        parameters.steps = steps;
        step_filter->set_parameters(parameters);
        if (phase != previous_phase) {
            step_filter->reset();
        }
    }

    void configure_notch_filter(float frequency_hz, float q, float mix) noexcept {
        auto* notch_filter = static_cast<tabletfx::NotchFilter*>(
            effects_[37].effect.get());
        notch_filter->set_parameters({frequency_hz, q, mix});
    }

    void configure_formant_filter(float vowel_position, float resonance_q,
                                  float mix, float level_db) noexcept {
        tabletfx::FormantFilterParameters parameters;
        parameters.vowel_position = vowel_position;
        parameters.resonance_q = resonance_q;
        parameters.mix = mix;
        parameters.level_db = level_db;
        static_cast<tabletfx::FormantFilter*>(effects_[43].effect.get())
            ->set_parameters(parameters);
    }

    void configure_harmonic_resonator(float root_frequency_hz,
                                      float decay_seconds, float brightness,
                                      float mix) noexcept {
        tabletfx::HarmonicResonatorParameters parameters;
        parameters.root_frequency_hz = root_frequency_hz;
        parameters.decay_seconds = decay_seconds;
        parameters.brightness = brightness;
        parameters.mix = mix;
        static_cast<tabletfx::HarmonicResonator*>(effects_[44].effect.get())
            ->set_parameters(parameters);
    }

    void configure_multiband_crossover(float low_mid_hz, float mid_high_hz,
                                       std::uint32_t slope) noexcept {
        auto* compressor = static_cast<tabletfx::MultibandCompressor*>(
            effects_[45].effect.get());
        auto parameters = compressor->parameters();
        parameters.low_mid_crossover_hz = low_mid_hz;
        parameters.mid_high_crossover_hz = mid_high_hz;
        parameters.crossover_slope =
            static_cast<tabletfx::MultibandCrossoverSlope>(slope);
        compressor->set_parameters(parameters);
    }

    void configure_multiband_band(std::uint32_t band, float threshold_db,
                                  float ratio, float knee_db, float attack_ms,
                                  float release_ms, float level_db,
                                  std::uint32_t detector,
                                  std::uint32_t mute) noexcept {
        if (band >= 3U) {
            return;
        }
        auto* compressor = static_cast<tabletfx::MultibandCompressor*>(
            effects_[45].effect.get());
        auto parameters = compressor->parameters();
        auto& band_parameters = parameters.bands[band];
        band_parameters.threshold_db = threshold_db;
        band_parameters.ratio = ratio;
        band_parameters.knee_db = knee_db;
        band_parameters.attack_ms = attack_ms;
        band_parameters.release_ms = release_ms;
        band_parameters.level_db = level_db;
        band_parameters.detector =
            static_cast<tabletfx::MultibandDetector>(detector);
        band_parameters.mute = mute != 0U;
        compressor->set_parameters(parameters);
    }

    void configure_bit_crusher(std::uint32_t bit_depth,
                               std::uint32_t sample_rate_divisor,
                               float mix) noexcept {
        auto* crusher = static_cast<tabletfx::BitCrusher*>(
            effects_[46].effect.get());
        auto parameters = crusher->parameters();
        parameters.bit_depth = static_cast<std::uint8_t>(
            std::clamp(bit_depth, 2U, 24U));
        parameters.sample_rate_divisor = static_cast<std::uint16_t>(
            std::clamp(sample_rate_divisor, 1U, 128U));
        parameters.mix = mix;
        crusher->set_parameters(parameters);
    }

    void configure_reverse_delay(float time_ms, float feedback, float mix,
                                 float crossfade_ms, std::uint32_t time_mode,
                                 std::uint32_t division, float tempo_bpm,
                                 float low_cut_hz, float high_cut_hz,
                                 float forward_level, bool spillover) noexcept {
        auto* delay = static_cast<tabletfx::ReverseDelay*>(
            effects_[38].effect.get());
        delay->set_parameters({
            time_ms, feedback, mix, crossfade_ms,
            static_cast<tabletfx::DelayTimeMode>(time_mode),
            static_cast<tabletfx::DelayDivision>(division), tempo_bpm,
            low_cut_hz, high_cut_hz, forward_level, spillover});
    }

    void set_reverse_delay_running(bool running) noexcept {
        static_cast<tabletfx::ReverseDelay*>(effects_[38].effect.get())
            ->set_running(running);
    }

    void trigger_reverse_delay_restart() noexcept {
        static_cast<tabletfx::ReverseDelay*>(effects_[38].effect.get())
            ->trigger_restart();
    }

    void configure_ducking_delay(float time_ms, float feedback, float mix,
                                 float threshold_db, float attenuation_db,
                                 float release_ms) noexcept {
        static_cast<tabletfx::DuckingDelay*>(effects_[39].effect.get())->configure(
            {time_ms, feedback, mix, threshold_db, attenuation_db, release_ms});
    }

    void configure_modulated_delay(float time_ms, float feedback, float mix,
                                   float rate_hz, float depth_ms,
                                   std::uint32_t waveform, float spread_degrees,
                                   float trigger_threshold_db) noexcept {
        static_cast<tabletfx::ModulatedDelay*>(effects_[40].effect.get())->configure(
            {time_ms, feedback, mix, rate_hz, depth_ms,
             static_cast<tabletfx::ModulatedDelayWaveform>(waveform),
             spread_degrees, trigger_threshold_db});
    }

    void configure_filtered_delay(float time_ms, float feedback, float mix,
                                  float low_cut_hz, float high_cut_hz) noexcept {
        static_cast<tabletfx::FilteredDelay*>(effects_[41].effect.get())->configure(
            {time_ms, feedback, mix, low_cut_hz, high_cut_hz});
    }

    void configure_sweep_delay(float time_ms, float feedback, float mix,
                               float start_hz, float stop_hz,
                               float resonance_q, float rate_hz,
                               std::uint32_t waveform,
                               float phase_degrees) noexcept {
        static_cast<tabletfx::SweepDelay*>(effects_[42].effect.get())->configure(
            {time_ms, feedback, mix, start_hz, stop_hz, resonance_q, rate_hz,
             static_cast<tabletfx::SweepDelayWaveform>(waveform), phase_degrees});
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

TABLETFX_WASM_EXPORT void tf_chain_set_hall_reverb(
    void* opaque, float decay_seconds, float pre_delay_ms, float damping_hz,
    float low_cut_hz, float mix, float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_hall_reverb(
            decay_seconds, pre_delay_ms, damping_hz, low_cut_hz, mix, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_plate_reverb(
    void* opaque, float decay_seconds, float pre_delay_ms, float damping_hz,
    float diffusion, float balance, float mix, float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_plate_reverb(
            decay_seconds, pre_delay_ms, damping_hz, diffusion, balance, mix, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_spring_reverb(
    void* opaque, float decay_seconds, float pre_delay_ms, float damping_hz,
    float dwell, float mix, float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_spring_reverb(
            decay_seconds, pre_delay_ms, damping_hz, dwell, mix, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_auto_swell(
    void* opaque, float threshold_db, float attack_ms, float release_ms,
    float hysteresis_db, std::uint32_t taper, float volume) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_auto_swell(
            threshold_db, attack_ms, release_ms, hysteresis_db, taper, volume);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_rotary_speaker(
    void* opaque, float rate_hz, std::uint32_t tempo_sync, float tempo_bpm,
    float beats_per_rotation, std::uint32_t speed, float slow_speed_multiplier,
    float drive, float hi_depth, float high_level, float low_depth,
    float rotor_length, float low_rate_multi, float low_time_constant_ms,
    float hi_time_constant_ms, float low_mic_spacing,
    float high_mic_spacing, float mic_distance,
    float stereo_spread, float mix, float level, float balance,
    std::uint32_t input_select) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_rotary_speaker(
            rate_hz, tempo_sync, tempo_bpm, beats_per_rotation, speed,
            slow_speed_multiplier, drive, hi_depth, high_level, low_depth,
            rotor_length, low_rate_multi, low_time_constant_ms,
            hi_time_constant_ms, low_mic_spacing, high_mic_spacing,
            mic_distance, stereo_spread,
            mix, level, balance, input_select);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_ring_modulator(
    void* opaque, float frequency_hz, float frequency_multiplier,
    float high_cut_hz, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_ring_modulator(
            frequency_hz, frequency_multiplier, high_cut_hz, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_rhythmic_slicer(
    void* opaque, std::uint32_t tempo_sync, float tempo_bpm,
    std::uint32_t division, float rate_hz, float depth, float shape,
    float attack_ms, float release_ms, float phase,
    float step_1, float step_2, float step_3, float step_4,
    float step_5, float step_6, float step_7, float step_8,
    float step_9, float step_10, float step_11, float step_12,
    float step_13, float step_14, float step_15, float step_16) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_rhythmic_slicer(
            tempo_sync, tempo_bpm, division, rate_hz, depth, shape,
            attack_ms, release_ms, phase,
            {step_1, step_2, step_3, step_4, step_5, step_6, step_7, step_8,
             step_9, step_10, step_11, step_12, step_13, step_14, step_15,
             step_16});
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_step_filter(
    void* opaque, std::uint32_t tempo_sync, float tempo_bpm,
    std::uint32_t division, float rate_hz, std::uint32_t mode,
    float minimum_cutoff_hz, float maximum_cutoff_hz, float resonance_q,
    float depth, float shape, float smoothing_ms, float mix, float phase,
    float step_1, float step_2, float step_3, float step_4,
    float step_5, float step_6, float step_7, float step_8,
    float step_9, float step_10, float step_11, float step_12,
    float step_13, float step_14, float step_15, float step_16) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_step_filter(
            tempo_sync, tempo_bpm, division, rate_hz, mode,
            minimum_cutoff_hz, maximum_cutoff_hz, resonance_q, depth, shape,
            smoothing_ms, mix, phase,
            {step_1, step_2, step_3, step_4, step_5, step_6, step_7, step_8,
             step_9, step_10, step_11, step_12, step_13, step_14, step_15,
             step_16});
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

TABLETFX_WASM_EXPORT void tf_chain_set_notch_filter(
    void* opaque, float frequency_hz, float q, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_notch_filter(frequency_hz, q, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_formant_filter(
    void* opaque, float vowel_position, float resonance_q, float mix,
    float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_formant_filter(
            vowel_position, resonance_q, mix, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_harmonic_resonator(
    void* opaque, float root_frequency_hz, float decay_seconds,
    float brightness, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_harmonic_resonator(
            root_frequency_hz, decay_seconds, brightness, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_multiband_crossover(
    void* opaque, float low_mid_hz, float mid_high_hz,
    std::uint32_t slope) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_multiband_crossover(
            low_mid_hz, mid_high_hz, slope);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_multiband_band(
    void* opaque, std::uint32_t band, float threshold_db, float ratio,
    float knee_db, float attack_ms, float release_ms, float level_db,
    std::uint32_t detector, std::uint32_t mute) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_multiband_band(
            band, threshold_db, ratio, knee_db, attack_ms, release_ms,
            level_db, detector, mute);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_bit_crusher(
    void* opaque, std::uint32_t bit_depth,
    std::uint32_t sample_rate_divisor, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_bit_crusher(
            bit_depth, sample_rate_divisor, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_reverse_delay(
    void* opaque, float time_ms, float feedback, float mix,
    float crossfade_ms, std::uint32_t time_mode, std::uint32_t division,
    float tempo_bpm, float low_cut_hz, float high_cut_hz,
    float forward_level, std::uint32_t spillover) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_reverse_delay(time_ms, feedback, mix,
            crossfade_ms, time_mode, division, tempo_bpm, low_cut_hz,
            high_cut_hz, forward_level, spillover != 0U);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_reverse_delay_run(
    void* opaque, std::uint32_t running) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->set_reverse_delay_running(running != 0U);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_restart_reverse_delay(void* opaque) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->trigger_reverse_delay_restart();
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_ducking_delay(
    void* opaque, float time_ms, float feedback, float mix,
    float threshold_db, float attenuation_db, float release_ms) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_ducking_delay(
            time_ms, feedback, mix, threshold_db, attenuation_db, release_ms);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_modulated_delay(
    void* opaque, float time_ms, float feedback, float mix,
    float rate_hz, float depth_ms, std::uint32_t waveform,
    float spread_degrees, float trigger_threshold_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_modulated_delay(
            time_ms, feedback, mix, rate_hz, depth_ms, waveform,
            spread_degrees, trigger_threshold_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_filtered_delay(
    void* opaque, float time_ms, float feedback, float mix,
    float low_cut_hz, float high_cut_hz) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_filtered_delay(
            time_ms, feedback, mix, low_cut_hz, high_cut_hz);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_sweep_delay(
    void* opaque, float time_ms, float feedback, float mix,
    float start_hz, float stop_hz, float resonance_q, float rate_hz,
    std::uint32_t waveform, float phase_degrees) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_sweep_delay(
            time_ms, feedback, mix, start_hz, stop_hz, resonance_q,
            rate_hz, waveform, phase_degrees);
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

TABLETFX_WASM_EXPORT void tf_chain_set_harmonic_tremolo(
    void* opaque, float rate_hz, float depth, float crossover_hz,
    std::uint32_t waveform, std::uint32_t tempo_sync, float tempo_bpm,
    float beats_per_cycle, std::uint32_t stereo_alternation,
    float smoothing_ms) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_harmonic_tremolo(
            rate_hz, depth, crossover_hz, waveform, tempo_sync, tempo_bpm,
            beats_per_cycle, stereo_alternation, smoothing_ms);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_auto_swell_delay(
    void* opaque, float time_ms, float feedback, float mix,
    std::uint32_t time_mode, std::uint32_t tap_division, float tempo_bpm,
    float swell_ms, float mod, float tone, float sense_db,
    std::uint32_t taper, std::uint32_t dry, std::uint32_t spillover) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_auto_swell_delay(
            time_ms, feedback, mix, time_mode, tap_division, tempo_bpm,
            swell_ms, mod, tone, sense_db, taper, dry, spillover);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_chorus(
    void* opaque, float rate_hz, float base_delay_ms,
    float depth_ms, float mix) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_chorus(rate_hz, base_delay_ms, depth_ms, mix);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_dimension_chorus(
    void* opaque, std::uint32_t version, std::uint32_t mode,
    float mix, float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_dimension_chorus(version, mode, mix, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_triple_double_chorus(
    void* opaque, std::uint32_t preset_switch, std::uint32_t manual_switch,
    float rate_hz, float left_depth_ms, float center_depth_ms,
    float right_depth_ms, float feedback, float sparkle,
    std::uint32_t stereo_switch) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_triple_double_chorus(
            preset_switch, manual_switch, rate_hz, left_depth_ms,
            center_depth_ms, right_depth_ms, feedback, sparkle,
            stereo_switch);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_four_band_resonator(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_four_band_resonator(values, value_count);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_diatonic_harmonizer(
    void* opaque, float mix, float pitch_interval, std::uint32_t key,
    std::uint32_t scale, float tone_hz, float delay_ms, float feedback,
    std::uint32_t tap_division, float tempo_bpm) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_diatonic_harmonizer(
            mix, pitch_interval, key, scale, tone_hz, delay_ms, feedback,
            tap_division, tempo_bpm);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_pitch_arpeggiator(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_pitch_arpeggiator(values, value_count);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_mono_guitar_synth(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_mono_guitar_synth(values, value_count);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_note_generator(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_note_generator(values, value_count);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_enhancer(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_enhancer(values, value_count);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_resonant_delay(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_resonant_delay(values, value_count);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_grain_delay(
    void* opaque, float time_ms, float feedback, float mix,
    float grain_size_ms, std::uint32_t overlap, float pitch_semitones,
    float spray_ms) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_grain_delay(
            time_ms, feedback, mix, grain_size_ms, overlap,
            pitch_semitones, spray_ms);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_pitch_feedback_delay(
    void* opaque, float time_ms, float feedback, float mix,
    float pitch_semitones) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_pitch_feedback_delay(
            time_ms, feedback, mix, pitch_semitones);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_feedback_generator(
    void* opaque, std::uint32_t type, float gain, float attack_ms,
    float release_ms, std::uint32_t dry_kill, float dry_level,
    float silence_threshold_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_feedback_generator(
            type, gain, attack_ms, release_ms, dry_kill, dry_level,
            silence_threshold_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_shimmer_reverb(
    void* opaque, const float* values) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_shimmer_reverb(values);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_convolution_reverb(
    void* opaque, float mix, float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_convolution_reverb(mix, level_db);
    }
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_load_convolution_reverb_ir(
    void* opaque, const float* samples, std::uint32_t frames,
    std::uint32_t ir_channels, std::uint32_t ir_sample_rate) noexcept {
    if (opaque == nullptr) return 0;
    return as_chain(opaque)->load_convolution_reverb_ir(
               samples, frames, ir_channels, ir_sample_rate)
        ? 1U
        : 0U;
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

TABLETFX_WASM_EXPORT void tf_chain_set_five_band_eq(
    void* opaque,
    float f1, float g1, float q1, float f2, float g2, float q2,
    float f3, float g3, float q3, float f4, float g4, float q4,
    float f5, float g5, float q5) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_five_band_eq(
            f1, g1, q1, f2, g2, q2, f3, g3, q3, f4, g4, q4, f5, g5, q5);
    }
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_looper_command(
    void* opaque, std::uint32_t command) noexcept {
    return opaque != nullptr && as_chain(opaque)->looper_command(command) ? 1U : 0U;
}

TABLETFX_WASM_EXPORT void tf_chain_set_looper_options(
    void* opaque, std::uint32_t reverse, std::uint32_t half_speed,
    float overdub_level) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_looper(reverse != 0U, half_speed != 0U,
                                           overdub_level);
    }
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_looper_state(void* opaque) noexcept {
    return opaque == nullptr ? 0U : as_chain(opaque)->looper_state();
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_looper_frames(void* opaque) noexcept {
    return opaque == nullptr ? 0U : as_chain(opaque)->looper_frames();
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_shuffling_looper_command(
    void* opaque, std::uint32_t command) noexcept {
    return opaque != nullptr &&
                   as_chain(opaque)->shuffling_looper_command(command)
               ? 1U
               : 0U;
}

TABLETFX_WASM_EXPORT void tf_chain_set_shuffling_looper_options(
    void* opaque, std::uint32_t slices, std::uint32_t sequence_length,
    float shuffle, float pitch_probability, float reverse_probability,
    float repeat_probability, float smoothing, float sequence_drift,
    float playback, float low_cut_hz, float high_cut_hz,
    std::int32_t interval1_semitones,
    std::int32_t interval2_semitones) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_shuffling_looper(
            slices, sequence_length, shuffle, pitch_probability,
            reverse_probability, repeat_probability, smoothing,
            sequence_drift, playback, low_cut_hz, high_cut_hz,
            interval1_semitones, interval2_semitones);
    }
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_shuffling_looper_state(
    void* opaque) noexcept {
    return opaque == nullptr ? 0U
                             : as_chain(opaque)->shuffling_looper_state();
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_shuffling_looper_frames(
    void* opaque) noexcept {
    return opaque == nullptr ? 0U
                             : as_chain(opaque)->shuffling_looper_frames();
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_shuffling_looper_steps(
    void* opaque) noexcept {
    return opaque == nullptr ? 0U
                             : as_chain(opaque)->shuffling_looper_steps();
}

TABLETFX_WASM_EXPORT void tf_chain_set_ten_tap_delay_options(
    void* opaque, std::uint32_t tap_count, float delay_time_ms, float tempo_bpm,
    std::uint32_t division, float decay_percent, float shuffle_percent,
    float spread_percent, float ratio_percent, std::uint32_t pan_shape,
    float pan_alpha_percent, float low_cut_hz, float high_cut_hz, float mix,
    float level_db, std::uint32_t mono) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_ten_tap_delay(
            tap_count, delay_time_ms, tempo_bpm, division, decay_percent,
            shuffle_percent, spread_percent, ratio_percent, pan_shape,
            pan_alpha_percent, low_cut_hz, high_cut_hz, mix, level_db, mono);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_ten_tap_delay_level(
    void* opaque, std::uint32_t tap, float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_ten_tap_delay_level(tap, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_rhythm_tap_delay(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_rhythm_tap_delay(values, value_count);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_plex_delay_global(
    void* opaque, std::uint32_t active_lines, float tempo_bpm,
    float master_time, float master_level, float master_pan,
    float decay_seconds, float diffusion, float low_cut_hz,
    float high_cut_hz, float pre_delay_ms, std::uint32_t stack_mode,
    float input_gain_db, float mix, float level_db, float balance) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_plex_delay_global(
            active_lines, tempo_bpm, master_time, master_level, master_pan,
            decay_seconds, diffusion, low_cut_hz, high_cut_hz,
            pre_delay_ms, stack_mode, input_gain_db, mix, level_db, balance);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_plex_delay_line(
    void* opaque, std::uint32_t line, float time_ms,
    std::uint32_t time_mode, std::uint32_t division,
    float level, float pan) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_plex_delay_line(
            line, time_ms, time_mode, division, level, pan);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_auto_pan(
    void* opaque, float rate_hz, float depth, std::uint32_t waveform,
    std::uint32_t tempo_sync, float tempo_bpm,
    std::uint32_t division) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_auto_pan(rate_hz, depth, waveform,
                                             tempo_sync, tempo_bpm, division);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_megatap_delay(
    void* opaque, float delay_time_ms, std::uint32_t tap_count,
    float time_alpha, float time_randomize_percent,
    std::uint32_t amplitude_shape, float amplitude_alpha,
    float amplitude_randomize_percent, std::uint32_t pan_shape,
    float pan_alpha_percent, float stereo_spread_percent, float predelay_ms,
    float feedback, std::uint32_t feedback_tap, float low_cut_hz,
    float high_cut_hz, float diffusion_mix, float diffusion_time_ms,
    float diffusion_lfo_rate_hz, float diffusion_lfo_depth_ms,
    float envelope_threshold_db, float envelope_attack_ms,
    float envelope_release_ms, float input_gain_db, float mix, float level_db,
    std::uint32_t input_select, std::uint32_t envelope_enabled) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_megatap_delay(
            delay_time_ms, tap_count, time_alpha, time_randomize_percent,
            amplitude_shape, amplitude_alpha, amplitude_randomize_percent,
            pan_shape, pan_alpha_percent, stereo_spread_percent, predelay_ms,
            feedback, feedback_tap, low_cut_hz, high_cut_hz, diffusion_mix,
            diffusion_time_ms, diffusion_lfo_rate_hz, diffusion_lfo_depth_ms,
            envelope_threshold_db, envelope_attack_ms, envelope_release_ms,
            input_gain_db, mix, level_db, input_select, envelope_enabled);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_de_esser(
    void* opaque, float detector_frequency_hz, float detector_q,
    float threshold_dbfs, float max_reduction_db, float attack_ms,
    float release_ms) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_de_esser(
            detector_frequency_hz, detector_q, threshold_dbfs,
            max_reduction_db, attack_ms, release_ms);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_dual_engine_distortion(
    void* opaque, float gain_a, float tone_a, float gain_b, float tone_b,
    float blend, std::uint32_t mode, std::uint32_t bass_boost,
    float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_dual_engine_distortion(
            gain_a, tone_a, gain_b, tone_b, blend, mode, bass_boost, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_vibe(
    void* opaque, float rate_hz, float depth, float trem,
    std::uint32_t stages, float spread, float level) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_vibe(rate_hz, depth, trem,
                                         stages, spread, level);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_multi_delay_line(
    void* opaque, std::uint32_t line, float time_ms, std::uint32_t time_mode,
    std::uint32_t division, float feedback, float cross_feedback,
    float level, float pan) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_multi_delay_line(
            line, time_ms, time_mode, division, feedback, cross_feedback,
            level, pan);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_multi_delay_global(
    void* opaque, float tempo_bpm, float low_cut_hz, float high_cut_hz,
    float input_gain_db, float mix, float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_multi_delay_global(
            tempo_bpm, low_cut_hz, high_cut_hz, input_gain_db, mix, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_porta_preamp(
    void* opaque, float gain_1_db, float gain_2_db,
    float bass_db, float treble_db, float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_porta_preamp(
            gain_1_db, gain_2_db, bass_db, treble_db, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_pitch_sequence(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_pitch_sequence(values, value_count);
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
