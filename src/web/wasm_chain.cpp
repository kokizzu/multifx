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
#include "tabletfx/effects/cabinet_ir.hpp"
#include "tabletfx/effects/double_take.hpp"
#include "tabletfx/effects/eq5_graphic.hpp"
#include "tabletfx/effects/optical_tremolo.hpp"
#include "tabletfx/effects/fractal_bias_trem.hpp"
#include "tabletfx/effects/vibratone.hpp"
#include "tabletfx/effects/auto_swell.hpp"
#include "tabletfx/effects/auto_swell_delay.hpp"
#include "tabletfx/effects/auto_pan.hpp"
#include "tabletfx/effects/megatap_delay.hpp"
#include "tabletfx/effects/de_esser.hpp"
#include "tabletfx/effects/poly_detune.hpp"
#include "tabletfx/effects/shuffling_looper.hpp"
#include "tabletfx/effects/ten_tap_delay.hpp"
#include "tabletfx/effects/rotary_speaker.hpp"
#include "tabletfx/effects/ring_modulator.hpp"
#include "tabletfx/effects/rhythmic_slicer.hpp"
#include "tabletfx/effects/step_filter.hpp"
#include "tabletfx/effects/deluxe_looper.hpp"
#include "tabletfx/effects/asheville_pattrn.hpp"
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
#include "tabletfx/effects/manual_thru_zero_flanger.hpp"
#include "tabletfx/effects/octobot.hpp"
#include "tabletfx/effects/am_ring_mod.hpp"
#include "tabletfx/effects/tape_echo.hpp"
#include "tabletfx/effects/octave_fuzz.hpp"
#include "tabletfx/effects/rock_box_dist.hpp"
#include "tabletfx/effects/rock_box_boost.hpp"
#include "tabletfx/effects/stereo_panner.hpp"
#include "tabletfx/effects/stereo_crossover.hpp"
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
          tuner_(static_cast<float>(sample_rate)),
          cabinet_ir_(static_cast<float>(sample_rate),
                      static_cast<std::uint16_t>(channels)),
          octave_divider_(static_cast<float>(sample_rate),
                          static_cast<std::uint16_t>(channels)),
          double_take_(static_cast<float>(sample_rate),
                       static_cast<std::uint16_t>(channels)),
          eq5_graphic_(static_cast<float>(sample_rate),
                       static_cast<std::uint16_t>(channels)),
          optical_tremolo_(std::make_unique<tabletfx::OpticalTremolo>(
              static_cast<float>(sample_rate))),
          fractal_bias_trem_(static_cast<float>(sample_rate)),
          vibratone_(static_cast<float>(sample_rate),
                     static_cast<std::uint16_t>(channels)),
          uni_verse_vibe_(static_cast<float>(sample_rate),
                          static_cast<std::uint16_t>(channels), {},
                          tabletfx::VibeModel::UniVerse),
          four_osc_generator_(static_cast<float>(sample_rate)),
          poly_detune_(static_cast<float>(sample_rate),
                       static_cast<std::uint16_t>(channels)) {
        effects_.reserve(96);
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
                                static_cast<float>(sample_rate), channels, 24.0F),
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
        effects_.push_back({std::make_unique<tabletfx::CelestialReverb>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::FractalThreeVoiceSynth>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::CloudReverb>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::MercatorDistortion>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::ManualThruZeroFlanger>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::Octobot>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::AmRingMod>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::TapeEcho>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::OctaveFuzz>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::StereoCrossover>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::ProjectAshevillePattrn>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::RockBoxDistortion>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::RockBoxBoost>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::StereoPanner>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::effects::PolySustain>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::StepFilterDelay>(
                                static_cast<float>(sample_rate), channels, 2000.0F),
                            false});
        effects_.push_back({std::make_unique<tabletfx::VolumePan>(), false});
        stereo_filter_ = std::make_unique<tabletfx::StereoFilter>(
            static_cast<float>(sample_rate));
        step_tremolo_ = std::make_unique<tabletfx::StepTremolo>(
            static_cast<float>(sample_rate));
        deluxe_looper_ = std::make_unique<tabletfx::DeluxeLooper>(
            channels, static_cast<float>(sample_rate));
        chromatic_pitch_delay_ =
            std::make_unique<tabletfx::ChromaticPitchDelay>(
                static_cast<float>(sample_rate), channels);
    }

    [[nodiscard]] std::size_t effect_count() const noexcept { return effects_.size() + 14; }

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
        if (index == 96U) {
            stereo_filter_enabled_ = enabled;
            return true;
        }
        if (index == 97U) {
            step_tremolo_enabled_ = enabled;
            return true;
        }
        if (index == 98U) {
            deluxe_looper_enabled_ = enabled;
            return true;
        }
        if (index == 99U) {
            chromatic_pitch_delay_enabled_ = enabled;
            if (!enabled) chromatic_pitch_delay_->reset();
            return true;
        }
        if (index == 100U) {
            cabinet_ir_enabled_ = enabled;
            if (!enabled) cabinet_ir_.reset();
            return true;
        }
        if (index == 101U) {
            octave_divider_enabled_ = enabled;
            if (!enabled) octave_divider_.reset();
            return true;
        }
        if (index == 102U) {
            double_take_enabled_ = enabled;
            if (!enabled) double_take_.reset();
            return true;
        }
        if (index == 103U) {
            eq5_graphic_enabled_ = enabled;
            if (!enabled) eq5_graphic_.reset();
            return true;
        }
        if (index == 104U) {
            optical_tremolo_enabled_ = enabled;
            if (!enabled) optical_tremolo_->reset();
            return true;
        }
        if (index == 105U) {
            fractal_bias_trem_enabled_ = enabled;
            if (!enabled) fractal_bias_trem_.reset();
            return true;
        }
        if (index == 106U) {
            vibratone_enabled_ = enabled;
            if (!enabled) vibratone_.reset();
            return true;
        }
        if (index == 107U) {
            uni_verse_vibe_enabled_ = enabled;
            if (!enabled) uni_verse_vibe_.reset();
            return true;
        }
        if (index == 108U) {
            four_osc_generator_enabled_ = enabled;
            if (!enabled) four_osc_generator_.reset();
            return true;
        }
        if (index == 109U) {
            poly_detune_enabled_ = enabled;
            if (!enabled) poly_detune_.reset();
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
        if (index == 86U && effects_[index].enabled && !enabled) {
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
        if (frames_ == 0) {
            return;
        }
        const std::size_t samples = static_cast<std::size_t>(frames_) * channels_;
        if (index == 96U) {
            if (stereo_filter_enabled_) {
                stereo_filter_->process(
                    std::span<float>(output_.data(), samples), channels_);
            }
            return;
        }
        if (index == 97U) {
            if (step_tremolo_enabled_) {
                step_tremolo_->process(
                    std::span<float>(output_.data(), samples), channels_);
            }
            return;
        }
        if (index == 98U) {
            if (deluxe_looper_enabled_) {
                deluxe_looper_->process(
                    std::span<float>(output_.data(), samples), channels_);
            }
            return;
        }
        if (index == 99U) {
            const auto output = std::span<float>(output_.data(), samples);
            if (chromatic_pitch_delay_enabled_) {
                chromatic_pitch_delay_->process(output, channels_);
            } else {
                chromatic_pitch_delay_->process_bypassed(output, channels_);
            }
            return;
        }
        if (index == 100U) {
            const auto output = std::span<float>(output_.data(), samples);
            if (cabinet_ir_enabled_) {
                cabinet_ir_.process(output, channels_);
            } else {
                cabinet_ir_.process_bypassed(output, channels_);
            }
            return;
        }
        if (index == 101U) {
            if (octave_divider_enabled_) {
                octave_divider_.process(
                    std::span<float>(output_.data(), samples), channels_);
            }
            return;
        }
        if (index == 102U) {
            if (double_take_enabled_) {
                double_take_.process(
                    std::span<float>(output_.data(), samples), channels_);
            }
            return;
        }
        if (index == 103U) {
            if (eq5_graphic_enabled_) {
                eq5_graphic_.process(
                    std::span<float>(output_.data(), samples), channels_);
            }
            return;
        }
        if (index == 104U) {
            if (optical_tremolo_enabled_) {
                optical_tremolo_->process(
                    std::span<float>(output_.data(), samples), channels_);
            } else if (optical_tremolo_mute_bypass_) {
                std::fill_n(output_.data(), samples, 0.0F);
            }
            return;
        }
        if (index == 105U) {
            if (fractal_bias_trem_enabled_) {
                fractal_bias_trem_.process(
                    std::span<float>(output_.data(), samples), channels_);
            }
            return;
        }
        if (index == 106U) {
            if (vibratone_enabled_) {
                vibratone_.process(
                    std::span<float>(output_.data(), samples), channels_);
            }
            return;
        }
        if (index == 107U) {
            if (uni_verse_vibe_enabled_) {
                uni_verse_vibe_.process(
                    std::span<float>(output_.data(), samples), channels_);
            }
            return;
        }
        if (index == 108U) {
            if (four_osc_generator_enabled_) {
                auto generated = std::span<float>(work_.data(), samples);
                four_osc_generator_.process(generated, channels_);
                std::copy_n(work_.begin(), samples, dry_.begin());
            }
            return;
        }
        if (index == 109U) {
            if (poly_detune_enabled_) {
                poly_detune_.process(
                    std::span<float>(output_.data(), samples), channels_);
            }
            return;
        }
        if (index >= effects_.size()) {
            return;
        }
        if (!effects_[index].enabled && index != 38U && index != 64U) {
            effects_[index].effect->process_bypassed(
                std::span<float>(work_.data(), samples), channels_);
            return;
        }
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
        stereo_filter_->reset();
        step_tremolo_->reset();
        deluxe_looper_->reset();
        chromatic_pitch_delay_->reset();
        cabinet_ir_.reset();
        octave_divider_.reset();
        double_take_.reset();
        optical_tremolo_->reset();
        fractal_bias_trem_.reset();
        vibratone_.reset();
        uni_verse_vibe_.reset();
        four_osc_generator_.reset();
        poly_detune_.reset();
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
                                std::uint32_t shape1, float slope1,
                                float f2, float g2, float q2,
                                std::uint32_t shape2, float slope2,
                                float f3, float g3, float q3,
                                std::uint32_t shape3, float slope3,
                                float f4, float g4, float q4,
                                std::uint32_t shape4, float slope4,
                                float f5, float g5, float q5,
                                std::uint32_t shape5, float slope5,
                                std::uint32_t solo_band) noexcept {
        const auto shape = [](std::uint32_t value) noexcept {
            return static_cast<tabletfx::FiveBandEQShape>(
                value <= 7U ? value : 255U);
        };
        const std::array<tabletfx::FiveBandEQBand, 5> bands{{
            {f1, g1, q1, shape(shape1), slope1},
            {f2, g2, q2, shape(shape2), slope2},
            {f3, g3, q3, shape(shape3), slope3},
            {f4, g4, q4, shape(shape4), slope4},
            {f5, g5, q5, shape(shape5), slope5},
        }};
        auto* eq = static_cast<tabletfx::FiveBandParametricEQ*>(
            effects_[59].effect.get());
        eq->configure(bands);
        eq->set_solo_band(solo_band);
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

    void configure_looper(bool reverse, bool half_speed, float overdub_level,
                          bool auto_record_enabled,
                          float auto_record_threshold_db) noexcept {
        auto* looper = static_cast<tabletfx::Looper*>(effects_[60].effect.get());
        looper->set_reverse(reverse);
        looper->set_half_speed(half_speed);
        looper->set_overdub_level(overdub_level);
        looper->set_auto_record_enabled(auto_record_enabled);
        looper->set_auto_record_threshold_db(auto_record_threshold_db);
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

    [[nodiscard]] bool deluxe_looper_command(std::uint32_t command) noexcept {
        switch (command) {
        case 0U: return deluxe_looper_->record();
        case 1U: return deluxe_looper_->play();
        case 2U: return deluxe_looper_->overdub();
        case 3U:
            deluxe_looper_->stop();
            return true;
        case 4U: return deluxe_looper_->undo();
        case 5U: return deluxe_looper_->redo();
        case 6U:
            deluxe_looper_->clear();
            return true;
        case 7U: return deluxe_looper_->play_once();
        default: return false;
        }
    }

    void configure_deluxe_looper(bool reverse, bool half_speed,
                                 float playback_level,
                                 float overdub_decay_db,
                                 float low_cut_hz,
                                 float high_cut_hz) noexcept {
        deluxe_looper_->set_reverse(reverse);
        deluxe_looper_->set_half_speed(half_speed);
        deluxe_looper_->set_playback_level(playback_level);
        deluxe_looper_->set_overdub_decay_db(overdub_decay_db);
        deluxe_looper_->set_low_cut_hz(low_cut_hz);
        deluxe_looper_->set_high_cut_hz(high_cut_hz);
    }

    [[nodiscard]] std::uint32_t deluxe_looper_state() const noexcept {
        return static_cast<std::uint32_t>(deluxe_looper_->state());
    }

    [[nodiscard]] std::uint32_t deluxe_looper_frames() const noexcept {
        const std::size_t frames = deluxe_looper_->loop_frames();
        return frames > std::numeric_limits<std::uint32_t>::max()
                   ? std::numeric_limits<std::uint32_t>::max()
                   : static_cast<std::uint32_t>(frames);
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
        constexpr std::uint32_t kParameterCount = 34U;
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
        float mix, float level_db, float balance,
        std::uint32_t input_select, float input_diffusion_mix,
        float input_diffusion_time_ms,
        std::uint32_t kill_dry) noexcept {
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
        parameters.input_select = static_cast<tabletfx::PlexDelayInputSelect>(
            std::min(input_select, 2U));
        parameters.input_diffusion_mix = input_diffusion_mix;
        parameters.input_diffusion_time_ms = input_diffusion_time_ms;
        parameters.input_gain_db = input_gain_db;
        parameters.mix = mix;
        parameters.kill_dry = kill_dry != 0U;
        parameters.level_db = level_db;
        parameters.balance = balance;
        delay->configure(parameters);
    }

    void configure_plex_delay_advanced(
        std::uint32_t filter_type, float filter_frequency_hz, float filter_q,
        float filter_gain_db, std::uint32_t filter_lfo_enabled,
        std::uint32_t filter_lfo_type, float filter_lfo_rate_hz,
        float filter_mod_frequency_hz, std::uint32_t envelope_enabled,
        float envelope_threshold_db, float envelope_attack_ms,
        float envelope_release_ms, float ducker_attenuation_db,
        float ducker_threshold_db, float ducker_release_ms,
        float stereo_spread) noexcept {
        auto* delay = static_cast<tabletfx::PlexDelay*>(
            effects_[76].effect.get());
        auto parameters = delay->parameters();
        parameters.wet_filter_type = static_cast<tabletfx::PlexWetFilterType>(
            std::min(filter_type, 8U));
        parameters.wet_filter_frequency_hz = filter_frequency_hz;
        parameters.wet_filter_q = filter_q;
        parameters.wet_filter_gain_db = filter_gain_db;
        parameters.filter_lfo_enabled = filter_lfo_enabled != 0U;
        parameters.filter_lfo_type = static_cast<tabletfx::PlexFilterLfoType>(
            std::min(filter_lfo_type, 3U));
        parameters.filter_lfo_rate_hz = filter_lfo_rate_hz;
        parameters.filter_mod_frequency_hz = filter_mod_frequency_hz;
        parameters.envelope_enabled = envelope_enabled != 0U;
        parameters.envelope_threshold_db = envelope_threshold_db;
        parameters.envelope_attack_ms = envelope_attack_ms;
        parameters.envelope_release_ms = envelope_release_ms;
        parameters.ducker_attenuation_db = ducker_attenuation_db;
        parameters.ducker_threshold_db = ducker_threshold_db;
        parameters.ducker_release_ms = ducker_release_ms;
        parameters.stereo_spread = stereo_spread;
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
                        float level, std::uint32_t rate_mode,
                        std::uint32_t note_division,
                        float tempo_bpm) noexcept {
        auto* vibe = static_cast<tabletfx::Vibe*>(effects_[71].effect.get());
        tabletfx::VibeParameters parameters;
        parameters.rate_hz = rate_hz;
        parameters.depth = depth;
        parameters.trem = trem;
        parameters.stages = static_cast<std::uint8_t>(stages);
        parameters.spread = spread;
        parameters.level = level;
        parameters.rate_mode = rate_mode == 1U
            ? tabletfx::VibeRateMode::TempoNote
            : tabletfx::VibeRateMode::Free;
        parameters.note_division = note_division <= static_cast<std::uint32_t>(
                tabletfx::VibeNoteDivision::SixteenthTriplet)
            ? static_cast<tabletfx::VibeNoteDivision>(note_division)
            : tabletfx::VibeNoteDivision::Quarter;
        parameters.tempo_bpm = tempo_bpm;
        vibe->set_parameters(parameters);
    }

    void configure_uni_verse_vibe(float speed_hz, float intensity,
                                  float volume, std::uint32_t rate_mode,
                                  std::uint32_t note_division,
                                  float tempo_bpm,
                                  std::uint32_t tone_mode,
                                  std::uint32_t modulation_mode) noexcept {
        tabletfx::VibeParameters parameters;
        parameters.rate_hz = speed_hz;
        parameters.depth = intensity;
        parameters.trem = 0.0F;
        parameters.stages = 2;
        parameters.spread = 0.0F;
        parameters.level = volume;
        parameters.rate_mode = rate_mode == 1U
            ? tabletfx::VibeRateMode::TempoNote
            : tabletfx::VibeRateMode::Free;
        parameters.note_division = note_division <= static_cast<std::uint32_t>(
                tabletfx::VibeNoteDivision::SixteenthTriplet)
            ? static_cast<tabletfx::VibeNoteDivision>(note_division)
            : tabletfx::VibeNoteDivision::Quarter;
        parameters.tempo_bpm = tempo_bpm;
        parameters.tone_mode = tone_mode == 1U
            ? tabletfx::VibeToneMode::Vintage
            : tabletfx::VibeToneMode::Modern;
        parameters.modulation_mode = modulation_mode == 1U
            ? tabletfx::VibeModulationMode::Vibrato
            : tabletfx::VibeModulationMode::Chorus;
        uni_verse_vibe_.set_parameters(parameters);
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

    void configure_multi_delay_topology(
        std::uint32_t topology, float series_feedback,
        std::uint32_t feedback_send, std::uint32_t feedback_return,
        float master_time, float master_feedback, float motor_speed) noexcept {
        auto* delay = static_cast<tabletfx::MultiDelay*>(effects_[72].effect.get());
        auto parameters = delay->parameters();
        parameters.topology = topology <= 5U
            ? static_cast<tabletfx::MultiDelayTopology>(topology)
            : static_cast<tabletfx::MultiDelayTopology>(255U);
        parameters.series_feedback = series_feedback;
        parameters.series_feedback_send = feedback_send;
        parameters.series_feedback_return = feedback_return;
        parameters.master_time = master_time;
        parameters.master_feedback = master_feedback;
        parameters.motor_speed = motor_speed;
        delay->configure(parameters);
    }

    void configure_multi_delay_time_lfo(
        std::uint32_t lfo_index, std::uint32_t waveform,
        float phase_degrees, float rate_hz, std::uint32_t tempo_sync,
        std::uint32_t division, float depth_ms) noexcept {
        if (lfo_index >= tabletfx::multi_delay_time_lfo_count) return;
        auto* delay = static_cast<tabletfx::MultiDelay*>(effects_[72].effect.get());
        auto parameters = delay->parameters();
        auto& lfo = parameters.time_lfo[lfo_index];
        lfo.waveform = waveform <= 9U
            ? static_cast<tabletfx::MultiDelayLfoWaveform>(waveform)
            : static_cast<tabletfx::MultiDelayLfoWaveform>(255U);
        lfo.phase_degrees = phase_degrees;
        lfo.rate_hz = rate_hz;
        lfo.tempo_sync = tempo_sync != 0U;
        lfo.division = division <= 13U
            ? static_cast<tabletfx::DelayDivision>(division)
            : static_cast<tabletfx::DelayDivision>(255U);
        lfo.depth_ms = depth_ms;
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

    void configure_poly_sustain(float interval_semitones, float attack_ms,
                                float decay_ms, float fx_level, float level_db,
                                std::uint32_t operation) noexcept {
        tabletfx::effects::PolySustainParameters parameters;
        parameters.interval_semitones = interval_semitones;
        parameters.attack_ms = attack_ms;
        parameters.decay_ms = decay_ms;
        parameters.fx_level = fx_level;
        parameters.level_db = level_db;
        parameters.operation = static_cast<
            tabletfx::effects::PolySustainOperation>(
                std::min(operation, 2U));
        static_cast<tabletfx::effects::PolySustain*>(effects_[91].effect.get())
            ->configure(parameters);
    }

    void configure_step_filter_delay(const float* values,
                                     std::uint32_t value_count) noexcept {
        if (values == nullptr || value_count != 33U) return;
        const auto enum_index = [](float value, std::uint32_t maximum) noexcept {
            if (!std::isfinite(value)) return 0U;
            return static_cast<std::uint32_t>(std::clamp(
                std::round(value), 0.0F, static_cast<float>(maximum)));
        };
        tabletfx::StepFilterDelayParameters parameters;
        parameters.delay_time_ms = values[0];
        parameters.delay_tempo_sync = std::isfinite(values[1]) && values[1] >= 0.5F;
        parameters.delay_division = static_cast<tabletfx::DelayDivision>(
            enum_index(values[2], static_cast<std::uint32_t>(
                tabletfx::DelayDivision::sixty_fourth)));
        parameters.step_tempo_sync = std::isfinite(values[3]) && values[3] >= 0.5F;
        parameters.tempo_bpm = values[4];
        parameters.step_division = static_cast<tabletfx::DelayDivision>(
            enum_index(values[5], static_cast<std::uint32_t>(
                tabletfx::DelayDivision::sixty_fourth)));
        parameters.step_rate_hz = values[6];
        parameters.feedback = values[7];
        parameters.mix = values[8];
        parameters.filter_mode = static_cast<tabletfx::StepFilterDelayMode>(
            enum_index(values[9], static_cast<std::uint32_t>(
                tabletfx::StepFilterDelayMode::Notch)));
        parameters.minimum_cutoff_hz = values[10];
        parameters.maximum_cutoff_hz = values[11];
        parameters.resonance_q = values[12];
        parameters.depth = values[13];
        parameters.shape = values[14];
        parameters.smoothing_ms = values[15];
        parameters.step_phase = values[16];
        for (std::size_t index = 0; index < parameters.steps.size(); ++index) {
            parameters.steps[index] = values[17U + index];
        }
        static_cast<tabletfx::StepFilterDelay*>(effects_[92].effect.get())
            ->configure(parameters);
    }

    void configure_volume_pan(const float* values,
                              std::uint32_t value_count) noexcept {
        if (values == nullptr || value_count != 11U) return;
        const auto enum_index = [](float value, std::uint32_t maximum) noexcept {
            if (!std::isfinite(value)) return 0U;
            return static_cast<std::uint32_t>(std::clamp(
                std::round(value), 0.0F, static_cast<float>(maximum)));
        };
        tabletfx::VolumePanParameters parameters;
        parameters.volume = values[0];
        parameters.minimum_db = values[1];
        parameters.maximum_db = values[2];
        parameters.taper = static_cast<tabletfx::VolumePanTaper>(
            enum_index(values[3], 6U));
        parameters.balance_percent = values[4];
        parameters.input_select = static_cast<tabletfx::VolumePanInputSelect>(
            enum_index(values[5], 2U));
        parameters.pan_left_percent = values[6];
        parameters.pan_right_percent = values[7];
        parameters.output_level_db = values[8];
        parameters.bypassed = std::isfinite(values[9]) && values[9] >= 0.5F;
        parameters.bypass_mode = static_cast<tabletfx::VolumePanBypassMode>(
            enum_index(values[10], 1U));
        static_cast<tabletfx::VolumePan*>(effects_[93].effect.get())
            ->set_parameters(parameters);
    }

    void configure_stereo_filter(const float* values,
                                 std::uint32_t value_count) noexcept {
        if (values == nullptr || value_count != 34U) return;
        const auto enum_index = [](float value, std::uint32_t maximum) noexcept {
            if (!std::isfinite(value)) return 0U;
            return static_cast<std::uint32_t>(std::clamp(
                std::round(value), 0.0F, static_cast<float>(maximum)));
        };
        tabletfx::StereoFilterParameters parameters;
        parameters.type = static_cast<tabletfx::StereoFilterType>(
            enum_index(values[0], 17U));
        parameters.frequency_hz = values[1];
        parameters.order = static_cast<std::uint8_t>(enum_index(values[2], 4U));
        parameters.q = values[3];
        parameters.gain_db = values[4];
        parameters.low_cut_hz = values[5];
        parameters.high_cut_hz = values[6];
        parameters.pan_left_percent = values[7];
        parameters.pan_right_percent = values[8];
        parameters.phase_invert = static_cast<tabletfx::StereoFilterPhaseInvert>(
            enum_index(values[9], 3U));
        parameters.delay_time_ms = values[10];
        parameters.depth = values[11];
        parameters.feedback = values[12];
        parameters.level_db = values[13];
        parameters.balance_percent = values[14];
        parameters.lfo_enabled = std::isfinite(values[15]) && values[15] >= 0.5F;
        parameters.lfo_shape = static_cast<tabletfx::StereoFilterLfoShape>(
            enum_index(values[16], 5U));
        parameters.lfo_rate_hz = values[17];
        parameters.lfo_duty_percent = values[18];
        parameters.mod_frequency_hz = values[19];
        parameters.lfo_quantize_steps = static_cast<std::uint8_t>(
            enum_index(values[20], 16U));
        parameters.mode = static_cast<tabletfx::StereoFilterMode>(
            enum_index(values[21], 2U));
        parameters.sweep_shape = values[22];
        parameters.lfo_tempo_sync =
            std::isfinite(values[23]) && values[23] >= 0.5F;
        parameters.lfo_tempo_bpm = values[24];
        parameters.lfo_beats_per_cycle = values[25];
        parameters.sensitivity_db = values[26];
        parameters.attack_time_ms = values[27];
        parameters.release_time_ms = values[28];
        parameters.emphasis = values[29];
        parameters.ladder_resonance = values[30];
        parameters.ladder_drive = values[31];
        parameters.ladder_bass_comp = values[32];
        parameters.ladder_order = static_cast<std::uint8_t>(
            enum_index(values[33], 4U));
        stereo_filter_->set_parameters(parameters);
    }

    // Config vector order: rate Hz, depth, step count, 16 normalized levels,
    // transition milliseconds. These are project-authored controls.
    void configure_step_tremolo(const float* values,
                                std::uint32_t value_count) noexcept {
        if (values == nullptr || value_count != 20U) return;
        const float requested_count = std::isfinite(values[2])
                                          ? std::round(values[2])
                                          : 8.0F;
        const auto step_count = static_cast<std::uint8_t>(std::clamp(
            requested_count, 1.0F,
            static_cast<float>(tabletfx::kStepTremoloMaximumSteps)));

        step_tremolo_->set_rate_hz(values[0]);
        step_tremolo_->set_depth(values[1]);
        step_tremolo_->set_step_count(step_count);
        for (std::size_t step = 0;
             step < tabletfx::kStepTremoloMaximumSteps; ++step) {
            step_tremolo_->set_step_level(
                static_cast<std::uint8_t>(step), values[3U + step]);
        }
        step_tremolo_->set_transition_ms(values[19]);
    }

    void configure_delay(float delay_ms, float feedback, float mix) noexcept {
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure(delay_ms, feedback, mix);
    }

    void configure_delay_bbd_style(bool enabled) noexcept {
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure_bbd_style(enabled);
    }

    void configure_delay_bbd_mode(bool mod) noexcept {
        static_cast<tabletfx::Delay*>(effects_[21].effect.get())
            ->configure_bbd_mode(mod);
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

    void configure_pitch_expression(float target_semitones,
                                    float position) noexcept {
        static_cast<tabletfx::PolyPitchShifter*>(effects_[22].effect.get())
            ->set_expression_position(target_semitones, position);
    }

    void configure_chromatic_pitch_delay(
        tabletfx::ChromaticPitchDelayParameters parameters) noexcept {
        chromatic_pitch_delay_->configure(parameters);
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
        auto* harmonizer = static_cast<tabletfx::effects::DiatonicHarmonizer*>(
            effects_[50].effect.get());
        auto config = harmonizer->config();
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
        harmonizer->configure(config);
    }

    void configure_diatonic_custom_scale(
        std::uint32_t note_count, std::uint32_t note_0, std::uint32_t note_1,
        std::uint32_t note_2, std::uint32_t note_3, std::uint32_t note_4,
        std::uint32_t note_5, std::uint32_t note_6,
        std::uint32_t note_7) noexcept {
        auto* harmonizer = static_cast<tabletfx::effects::DiatonicHarmonizer*>(
            effects_[50].effect.get());
        auto config = harmonizer->config();
        config.custom_scale_note_count = static_cast<std::uint8_t>(
            std::clamp(note_count, 0U, 255U));
        const std::array<std::uint32_t, 8> requested{
            note_0, note_1, note_2, note_3, note_4, note_5, note_6, note_7};
        for (std::size_t index = 0; index < requested.size(); ++index) {
            config.custom_scale_notes[index] = static_cast<std::uint8_t>(
                std::min(requested[index], 11U));
        }
        harmonizer->configure(config);
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
                ? static_cast<int>(std::lround(std::clamp(requested, -24.0F, 24.0F)))
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
        for (std::size_t voice = 0; voice < 3U; ++voice) {
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

    void configure_four_osc_generator(const float* values,
                                     std::uint32_t value_count) noexcept {
        constexpr std::uint32_t kParameterCount = 17U;
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
        parameters.voice_count = 4U;
        for (std::size_t voice = 0; voice < parameters.voices.size(); ++voice) {
            const std::size_t offset = voice * 4U;
            parameters.voices[voice].waveform =
                static_cast<tabletfx::effects::NoteGeneratorWaveform>(
                    selector(values[offset], 3U));
            parameters.voices[voice].note_number =
                integer(values[offset + 1U], 0, 127, 60);
            parameters.voices[voice].octave =
                integer(values[offset + 2U], -2, 2, 0);
            parameters.voices[voice].level = values[offset + 3U];
        }
        parameters.output_level = values[16U];
        four_osc_generator_.configure(parameters);
    }

    void configure_poly_detune(std::uint32_t voice_count, float rate_hz,
                               float depth_ms, float width, float mix,
                               float level_db) noexcept {
        tabletfx::PolyDetuneParameters parameters;
        parameters.voice_count = static_cast<std::uint8_t>(
            std::clamp(voice_count, 1U, 2U));
        parameters.rate_hz = rate_hz;
        parameters.depth_ms = depth_ms;
        parameters.width = width;
        parameters.mix = mix;
        parameters.level_db = level_db;
        poly_detune_.set_parameters(parameters);
    }

    void configure_fractal_three_voice_synth(
        const float* values, std::uint32_t value_count) noexcept {
        constexpr std::uint32_t kParameterCount = 33U;
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
        tabletfx::effects::FractalThreeVoiceSynthConfig config;
        config.tracking_mode =
            static_cast<tabletfx::effects::FractalSynthTrackingMode>(
                selector(values[33U], 1U));
        for (std::size_t voice = 0; voice < config.voices.size(); ++voice) {
            const std::size_t offset = voice * 11U;
            auto& parameters = config.voices[voice];
            parameters.waveform =
                static_cast<tabletfx::effects::FractalSynthWaveform>(
                    selector(values[offset], 7U));
            parameters.tracking =
                static_cast<tabletfx::effects::FractalSynthTracking>(
                    selector(values[offset + 1U], 3U));
            parameters.frequency_hz = values[offset + 2U];
            parameters.shift_semitones = integer(
                values[offset + 3U], -24, 24, 0);
            parameters.tune_cents = values[offset + 4U];
            parameters.duty_cycle = values[offset + 5U];
            parameters.pan = values[offset + 6U];
            parameters.filter_frequency_hz = values[offset + 7U];
            parameters.filter_q = values[offset + 8U];
            parameters.attack_ms = values[offset + 9U];
            parameters.level = values[offset + 10U];
        }
        static_cast<tabletfx::effects::FractalThreeVoiceSynth*>(
            effects_[78].effect.get())->configure(config);
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

    void configure_celestial_reverb(const float* values) noexcept {
        if (values == nullptr) return;
        tabletfx::CelestialReverbParameters parameters;
        parameters.mix = values[0];
        parameters.decay_seconds = values[1];
        parameters.pitch_1_semitones = values[2];
        parameters.pitch_2_semitones = values[3];
        parameters.pitch_decay_seconds = values[4];
        parameters.high_cut_hz = values[5];
        parameters.blend = values[6];
        parameters.spillover = values[7] != 0.0F;
        static_cast<tabletfx::CelestialReverb*>(effects_[77].effect.get())
            ->configure(parameters);
    }

    void configure_cloud_reverb(const float* values) noexcept {
        if (values == nullptr) return;
        tabletfx::CloudReverbParameters parameters;
        parameters.mix = values[0];
        parameters.tone = values[1];
        parameters.decay_seconds = values[2];
        parameters.diffusion = values[3];
        parameters.high_damp_hz = values[4];
        parameters.low_damp_hz = values[5];
        parameters.modulation_rate_hz = values[6];
        parameters.modulation_depth = values[7];
        parameters.spillover = values[8] != 0.0F;
        static_cast<tabletfx::CloudReverb*>(effects_[79].effect.get())
            ->configure(parameters);
    }

    void configure_mercator_distortion(const float* values) noexcept {
        if (values == nullptr) return;
        tabletfx::MercatorDistParameters parameters;
        parameters.drive = values[0];
        parameters.drive_2 = values[1];
        parameters.bass_db = values[2];
        parameters.mid_db = values[3];
        parameters.treble_db = values[4];
        parameters.bright_1 = values[5] != 0.0F;
        parameters.bright_2 = values[6] != 0.0F;
        parameters.level_db = values[7];
        static_cast<tabletfx::MercatorDistortion*>(effects_[80].effect.get())
            ->set_parameters(parameters);
    }

    void configure_manual_thru_zero_flanger(const float* values) noexcept {
        if (values == nullptr) return;
        tabletfx::ManualThruZeroFlangerParameters parameters;
        parameters.minimum_time_ms = values[0];
        parameters.maximum_time_ms = values[1];
        parameters.manual = values[2];
        parameters.dry_delay_position = values[3];
        parameters.feedback_percent = values[4];
        const auto phase = static_cast<int>(values[5]);
        parameters.phase_reverse = phase <= 0
            ? tabletfx::FlangerPhaseReverse::None
            : phase == 1 ? tabletfx::FlangerPhaseReverse::Left
            : phase == 2 ? tabletfx::FlangerPhaseReverse::Right
                         : tabletfx::FlangerPhaseReverse::Both;
        parameters.mix = values[6];
        parameters.smoothing_ms = values[7];
        static_cast<tabletfx::ManualThruZeroFlanger*>(effects_[81].effect.get())
            ->set_parameters(parameters);
    }

    void configure_octobot(const float* values) noexcept {
        if (values == nullptr) return;
        tabletfx::effects::OctobotParameters parameters;
        parameters.direct = values[0];
        parameters.down = values[1];
        parameters.sizzle = values[2];
        static_cast<tabletfx::effects::Octobot*>(effects_[82].effect.get())
            ->configure(parameters);
    }

    void configure_am_ring_mod(float rate_hz, float depth, float mix,
                               float level_db) noexcept {
        tabletfx::effects::AmRingModParameters parameters;
        parameters.rate_hz = rate_hz;
        parameters.depth = depth;
        parameters.mix = mix;
        parameters.level_db = level_db;
        static_cast<tabletfx::effects::AmRingMod*>(effects_[83].effect.get())
            ->set_parameters(parameters);
    }

    void configure_tape_echo(std::uint32_t time_mode, float time_ms,
                             float tempo_bpm, std::uint32_t tap_division,
                             float feedback, float mix, float record_level,
                             float tape_bias, float tape_noise,
                             float wow_flutter, std::uint32_t spillover,
                             std::uint32_t independent_stereo_times,
                             float left_time_ms, float right_time_ms,
                             std::uint32_t left_tap_division,
                             std::uint32_t right_tap_division) noexcept {
        tabletfx::TapeEchoParameters parameters;
        parameters.time_mode = time_mode == 1U
            ? tabletfx::DelayTimeMode::tempo_sync
            : tabletfx::DelayTimeMode::milliseconds;
        parameters.time_ms = time_ms;
        parameters.tempo_bpm = tempo_bpm;
        parameters.tap_division = static_cast<tabletfx::DelayDivision>(tap_division);
        parameters.independent_stereo_times = independent_stereo_times != 0U;
        parameters.left_time_ms = left_time_ms;
        parameters.right_time_ms = right_time_ms;
        parameters.left_tap_division =
            static_cast<tabletfx::DelayDivision>(left_tap_division);
        parameters.right_tap_division =
            static_cast<tabletfx::DelayDivision>(right_tap_division);
        parameters.feedback = feedback;
        parameters.mix = mix;
        parameters.record_level = record_level;
        parameters.tape_bias = tape_bias;
        parameters.tape_noise = tape_noise;
        parameters.wow_flutter = wow_flutter;
        parameters.spillover = spillover != 0U;
        static_cast<tabletfx::TapeEcho*>(effects_[84].effect.get())
            ->configure(parameters);
    }

    void configure_octave_fuzz(float boost, float volume,
                               std::uint32_t octave_switch) noexcept {
        tabletfx::effects::OctaveFuzzParameters parameters;
        parameters.boost = boost;
        parameters.volume = volume;
        parameters.octave_switch = octave_switch != 0U;
        static_cast<tabletfx::effects::OctaveFuzz*>(effects_[85].effect.get())
            ->configure(parameters);
    }

    void configure_rock_box_dist(float drive, float tone, float level_db,
                                 float smoothing_ms) noexcept {
        tabletfx::RockBoxDistortionParameters parameters;
        parameters.drive = drive;
        parameters.tone = tone;
        parameters.level_db = level_db;
        parameters.smoothing_ms = smoothing_ms;
        static_cast<tabletfx::RockBoxDistortion*>(effects_[88U].effect.get())
            ->set_parameters(parameters);
    }

    void configure_rock_box_boost(float boost_db, float level_db,
                                  float smoothing_ms) noexcept {
        tabletfx::RockBoxBoostParameters parameters;
        parameters.boost_db = boost_db;
        parameters.level_db = level_db;
        parameters.smoothing_ms = smoothing_ms;
        static_cast<tabletfx::RockBoxBoost*>(effects_[89U].effect.get())
            ->set_parameters(parameters);
    }

    void configure_stereo_panner(
        float rate_hz, float width_percent, std::uint32_t waveform,
        std::uint32_t tempo_sync, float tempo_bpm, std::uint32_t division,
        float duty_cycle_percent, float shape_percent, float phase_degrees,
        float pan_center_percent, float ducking_percent, float level_db,
        float balance_percent, float smoothing_ms) noexcept {
        tabletfx::StereoPannerParameters parameters;
        parameters.rate_hz = rate_hz;
        parameters.width_percent = width_percent;
        parameters.waveform = static_cast<tabletfx::StereoPannerWaveform>(waveform);
        parameters.tempo_sync = tempo_sync != 0U;
        parameters.tempo_bpm = tempo_bpm;
        parameters.division = static_cast<tabletfx::AutoPanDivision>(division);
        parameters.duty_cycle_percent = duty_cycle_percent;
        parameters.shape_percent = shape_percent;
        parameters.phase_degrees = phase_degrees;
        parameters.pan_center_percent = pan_center_percent;
        parameters.ducking_percent = ducking_percent;
        parameters.level_db = level_db;
        parameters.balance_percent = balance_percent;
        parameters.smoothing_ms = smoothing_ms;
        static_cast<tabletfx::StereoPanner*>(effects_[90U].effect.get())
            ->set_parameters(parameters);
    }

    void configure_stereo_crossover(const float* values) noexcept {
        if (values == nullptr) return;
        tabletfx::StereoCrossoverParameters parameters;
        parameters.frequency_hz = values[0];
        parameters.frequency_multiplier = values[1] >= 0.5F
            ? tabletfx::CrossoverFrequencyMultiplier::x10
            : tabletfx::CrossoverFrequencyMultiplier::x1;
        parameters.low_level_left_db = values[2];
        parameters.low_level_right_db = values[3];
        parameters.high_level_left_db = values[4];
        parameters.high_level_right_db = values[5];
        parameters.low_pan_left = values[6];
        parameters.high_pan_left = values[7];
        parameters.low_pan_right = values[8];
        parameters.high_pan_right = values[9];
        parameters.level_db = values[10];
        parameters.balance = values[11];
        static_cast<tabletfx::StereoCrossover*>(effects_[86].effect.get())
            ->configure(parameters);
    }

    void configure_convolution_reverb(float mix, float level_db,
                                      float high_pass_hz, float low_pass_hz,
                                      std::uint32_t tails) noexcept {
        tabletfx::ConvolutionReverbParameters parameters;
        parameters.mix = mix;
        parameters.level_db = level_db;
        parameters.high_pass_hz = high_pass_hz;
        parameters.low_pass_hz = low_pass_hz;
        parameters.tails = tails != 0;
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

    [[nodiscard]] bool load_cabinet_ir(
        const float* samples, std::uint32_t frames,
        std::uint32_t ir_channels, std::uint32_t ir_sample_rate,
        std::uint32_t bits_per_sample) noexcept {
        if (samples == nullptr || frames != tabletfx::CabinetIr::kRequiredIrFrames ||
            ir_channels != 1U ||
            ir_sample_rate != tabletfx::CabinetIr::kRequiredIrSampleRate ||
            bits_per_sample != tabletfx::CabinetIr::kRequiredIrBitsPerSample) {
            return false;
        }
        return cabinet_ir_.load_impulse_response(
            std::span<const float>(samples, frames),
            static_cast<std::uint16_t>(ir_channels), ir_sample_rate,
            static_cast<std::uint16_t>(bits_per_sample));
    }

    [[nodiscard]] static constexpr std::uint32_t cabinet_ir_latency_frames() noexcept {
        return static_cast<std::uint32_t>(tabletfx::CabinetIr::kLatencyFrames);
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

    void configure_ring_modulator(std::uint32_t type, float frequency_hz,
                                  float frequency_multiplier, float high_cut_hz,
                                  float mix, std::uint32_t pitch_tracking,
                                  std::uint32_t tracking_mode) noexcept {
        auto* ring_modulator =
            static_cast<tabletfx::RingModulator*>(effects_[34].effect.get());
        tabletfx::RingModulatorParameters parameters;
        if (type <= static_cast<std::uint32_t>(tabletfx::RingModulatorType::ssb_lower)) {
            parameters.type = static_cast<tabletfx::RingModulatorType>(type);
        }
        parameters.frequency_hz = frequency_hz;
        parameters.frequency_multiplier = frequency_multiplier;
        parameters.pitch_tracking = pitch_tracking != 0U;
        if (tracking_mode <= static_cast<std::uint32_t>(
                                 tabletfx::RingModulatorTrackingMode::bass)) {
            parameters.tracking_mode =
                static_cast<tabletfx::RingModulatorTrackingMode>(tracking_mode);
        }
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

    void configure_asheville_pattrn(
        std::uint32_t bank, std::uint32_t pattern,
        float band_level_1, float band_level_2, float band_level_3,
        float band_level_4, float band_level_5, float band_level_6,
        float band_level_7, float band_level_8, float drive_db,
        float output_db, float mix, float pattern_rate_hz,
        float resonance_q, float envelope_shape, float smoothing_ms) noexcept {
        auto* pattrn = static_cast<tabletfx::ProjectAshevillePattrn*>(
            effects_[87U].effect.get());
        auto parameters = pattrn->parameters();
        parameters.bank = bank == 1U
                              ? tabletfx::AshevilleFrequencyBank::Mids
                              : tabletfx::AshevilleFrequencyBank::Bass;
        parameters.pattern = static_cast<std::uint8_t>(
            std::min(pattern, 255U));
        parameters.band_level = {band_level_1, band_level_2, band_level_3,
                                 band_level_4, band_level_5, band_level_6,
                                 band_level_7, band_level_8};
        parameters.drive_db = drive_db;
        parameters.output_db = output_db;
        parameters.mix = mix;
        parameters.pattern_rate_hz = pattern_rate_hz;
        parameters.resonance_q = resonance_q;
        parameters.envelope_shape = envelope_shape;
        parameters.smoothing_ms = smoothing_ms;
        pattrn->set_parameters(parameters);
    }

    void configure_notch_filter(float frequency_hz, float q, float mix) noexcept {
        auto* notch_filter = static_cast<tabletfx::NotchFilter*>(
            effects_[37].effect.get());
        notch_filter->set_parameters({frequency_hz, q, mix});
    }

    void configure_formant_filter(float vowel_position, float resonance_q,
                                  float mix, float level_db, float morph_mode,
                                  float start_vowel, float middle_vowel,
                                  float end_vowel, float control_percent,
                                  float balance_percent) noexcept {
        tabletfx::FormantFilterParameters parameters;
        parameters.vowel_position = vowel_position;
        parameters.resonance_q = resonance_q;
        parameters.mix = mix;
        parameters.level_db = level_db;
        parameters.three_vowel_morph = morph_mode >= 0.5F;
        const auto vowel = [](float value) noexcept {
            if (!std::isfinite(value)) {
                return tabletfx::FormantVowel::aaa;
            }
            const auto index = std::clamp(
                static_cast<int>(std::lround(value)), 0, 9);
            return static_cast<tabletfx::FormantVowel>(index);
        };
        parameters.start_vowel = vowel(start_vowel);
        parameters.middle_vowel = vowel(middle_vowel);
        parameters.end_vowel = vowel(end_vowel);
        parameters.control_percent = control_percent;
        parameters.balance_percent = balance_percent;
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
                                 float release_ms,
                                 std::uint32_t spillover,
                                 std::uint32_t time_mode, float tempo_bpm,
                                 std::uint32_t tap_division,
                                 float low_cut_hz, float high_cut_hz,
                                 float mod_rate_hz, float mod_depth_ms,
                                 std::uint32_t modulation_mode) noexcept {
        tabletfx::DuckingDelayParameters parameters;
        parameters.time_ms = time_ms;
        parameters.feedback = feedback;
        parameters.mix = mix;
        parameters.threshold_db = threshold_db;
        parameters.attenuation_db = attenuation_db;
        parameters.release_ms = release_ms;
        parameters.spillover = spillover != 0U;
        parameters.time_mode =
            static_cast<tabletfx::DuckingDelayTimeMode>(time_mode);
        parameters.tempo_bpm = tempo_bpm;
        parameters.tap_division =
            static_cast<tabletfx::DuckingDelayTapDivision>(tap_division);
        parameters.low_cut_hz = low_cut_hz;
        parameters.high_cut_hz = high_cut_hz;
        parameters.mod_rate_hz = mod_rate_hz;
        parameters.mod_depth_ms = mod_depth_ms;
        parameters.modulation_mode =
            static_cast<tabletfx::DuckingDelayModulationMode>(modulation_mode);
        static_cast<tabletfx::DuckingDelay*>(effects_[39].effect.get())->configure(
            parameters);
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
                                  float low_cut_hz, float high_cut_hz,
                                  std::uint32_t time_mode, float tempo_bpm,
                                  std::uint32_t division) noexcept {
        auto* effect = static_cast<tabletfx::FilteredDelay*>(
            effects_[41].effect.get());
        auto parameters = effect->parameters();
        parameters.time_ms = time_ms;
        parameters.feedback = feedback;
        parameters.mix = mix;
        parameters.low_cut_hz = low_cut_hz;
        parameters.high_cut_hz = high_cut_hz;
        parameters.time_mode = time_mode == 1
            ? tabletfx::DelayTimeMode::tempo_sync
            : tabletfx::DelayTimeMode::milliseconds;
        parameters.tempo_bpm = tempo_bpm;
        parameters.division = division <= static_cast<std::uint32_t>(
            tabletfx::DelayDivision::sixty_fourth)
            ? static_cast<tabletfx::DelayDivision>(division)
            : tabletfx::DelayDivision::quarter;
        effect->configure(parameters);
    }

    void configure_sweep_delay(float time_ms, float feedback, float mix,
                               float start_hz, float stop_hz,
                               float resonance_q, float rate_hz,
                               std::uint32_t waveform,
                               float phase_degrees,
                               std::uint32_t filter_type,
                               std::uint32_t rate_mode,
                               float tempo_bpm,
                               std::uint32_t division) noexcept {
        tabletfx::SweepDelayParameters parameters{
            time_ms, feedback, mix, start_hz, stop_hz, resonance_q, rate_hz,
            static_cast<tabletfx::SweepDelayWaveform>(waveform), phase_degrees};
        parameters.filter_type =
            static_cast<tabletfx::SweepDelayFilterType>(filter_type);
        parameters.sweep_rate_mode =
            static_cast<tabletfx::DelayTimeMode>(rate_mode);
        parameters.sweep_tempo_bpm = tempo_bpm;
        parameters.sweep_tempo_division =
            static_cast<tabletfx::DelayDivision>(division);
        static_cast<tabletfx::SweepDelay*>(effects_[42].effect.get())->configure(
            parameters);
    }

    void configure_limiter(float ceiling_db, float release_ms) noexcept {
        output_limiter_.set_ceiling_db(ceiling_db);
        output_limiter_.set_release_ms(release_ms);
    }

    void configure_octave_divider(float input_gain, float voice1_level,
                                  float voice1_pan, float voice2_level,
                                  float voice2_pan, float mix,
                                  float output_level) noexcept {
        tabletfx::effects::OctaveDividerParameters parameters;
        parameters.input_gain = input_gain;
        parameters.voice1_level = voice1_level;
        parameters.voice1_pan = voice1_pan;
        parameters.voice2_level = voice2_level;
        parameters.voice2_pan = voice2_pan;
        parameters.mix = mix;
        parameters.output_level = output_level;
        octave_divider_.configure(parameters);
    }

    void configure_double_take(std::uint32_t doubles, float slop,
                               float sensitivity, float dry_level,
                               float wet_level, std::uint32_t source) noexcept {
        tabletfx::DoubleTakeParameters parameters;
        parameters.doubles = static_cast<std::uint8_t>(doubles);
        parameters.slop = slop;
        parameters.sensitivity = sensitivity;
        parameters.dry_level = dry_level;
        parameters.wet_level = wet_level;
        parameters.source = static_cast<tabletfx::DoubleTakeSource>(source);
        double_take_.set_parameters(parameters);
    }

    void configure_eq5_graphic(float band1_db, float band2_db,
                               float band3_db, float band4_db,
                               float band5_db, float overall_gain_db) noexcept {
        eq5_graphic_.configure(
            {band1_db, band2_db, band3_db, band4_db, band5_db},
            overall_gain_db);
    }

    void configure_optical_tremolo(
        std::uint32_t variant, float rate_hz, float depth,
        std::uint32_t lfo, std::uint32_t tempo_sync, float tempo_bpm,
        float beats_per_cycle, float duty_cycle_percent, float shape_percent,
        float phase_degrees, float start_phase_degrees,
        std::uint32_t trigger_enabled, float trigger_threshold_dbfs,
        float ducking_percent, float mix, float level_db,
        float balance_percent, std::uint32_t bypass_mode) noexcept {
        tabletfx::OpticalTremoloParameters parameters;
        parameters.variant = variant <= static_cast<std::uint32_t>(
            tabletfx::OpticalTremoloVariant::Neon)
            ? static_cast<tabletfx::OpticalTremoloVariant>(variant)
            : static_cast<tabletfx::OpticalTremoloVariant>(255U);
        parameters.rate_hz = rate_hz;
        parameters.depth = depth;
        parameters.lfo = lfo <= static_cast<std::uint32_t>(
            tabletfx::OpticalTremoloWaveform::Astable)
            ? static_cast<tabletfx::OpticalTremoloWaveform>(lfo)
            : static_cast<tabletfx::OpticalTremoloWaveform>(255U);
        parameters.tempo_sync = tempo_sync != 0U;
        parameters.tempo_bpm = tempo_bpm;
        parameters.beats_per_cycle = beats_per_cycle;
        parameters.duty_cycle_percent = duty_cycle_percent;
        parameters.shape_percent = shape_percent;
        parameters.phase_degrees = phase_degrees;
        parameters.start_phase_degrees = start_phase_degrees;
        parameters.trigger_enabled = trigger_enabled != 0U;
        parameters.trigger_threshold_dbfs = trigger_threshold_dbfs;
        parameters.ducking_percent = ducking_percent;
        parameters.mix = mix;
        parameters.level_db = level_db;
        parameters.balance_percent = balance_percent;
        optical_tremolo_->set_parameters(parameters);
        optical_tremolo_mute_bypass_ = bypass_mode != 0U;
    }

    void configure_fractal_bias_trem(
        float rate_hz, float depth, std::uint32_t waveform,
        std::uint32_t tempo_sync, float tempo_bpm, float beats_per_cycle,
        std::uint32_t trigger_sync, float trigger_threshold_db,
        float ducking, float bias_drive, float mix, float level_db,
        float balance, std::uint32_t bypass) noexcept {
        tabletfx::BiasTremParameters parameters;
        parameters.rate_hz = rate_hz;
        parameters.depth = depth;
        parameters.waveform = waveform <= static_cast<std::uint32_t>(
            tabletfx::BiasTremWaveform::SampleHold)
            ? static_cast<tabletfx::BiasTremWaveform>(waveform)
            : static_cast<tabletfx::BiasTremWaveform>(255U);
        parameters.tempo_sync = tempo_sync != 0U;
        parameters.tempo_bpm = tempo_bpm;
        parameters.beats_per_cycle = beats_per_cycle;
        parameters.trigger_sync = trigger_sync != 0U;
        parameters.trigger_threshold_db = trigger_threshold_db;
        parameters.ducking = ducking;
        parameters.bias_drive = bias_drive;
        parameters.mix = mix;
        parameters.level_db = level_db;
        parameters.balance = balance;
        parameters.bypass = bypass != 0U;
        fractal_bias_trem_.set_parameters(parameters);
    }

    void configure_vibratone(float drive, float tone, float level_db,
                             float fast_speed_hz, float slow_speed_hz,
                             std::uint32_t speed_fast, float ramp_ms,
                             float sensitivity,
                             std::uint32_t dynamics_switch) noexcept {
        tabletfx::VibratoneParameters parameters;
        parameters.drive = drive;
        parameters.tone = tone;
        parameters.level_db = level_db;
        parameters.fast_speed_hz = fast_speed_hz;
        parameters.slow_speed_hz = slow_speed_hz;
        parameters.speed_fast = speed_fast != 0U;
        parameters.ramp_ms = ramp_ms;
        parameters.sensitivity = sensitivity;
        parameters.dynamics_switch = dynamics_switch != 0U;
        vibratone_.set_parameters(parameters);
    }

    [[nodiscard]] std::uint32_t memory_bytes() const noexcept {
        std::size_t bytes = sizeof(*this) + effects_.capacity() * sizeof(EffectEntry);
        bytes += input_.capacity() * sizeof(float) + dry_.capacity() * sizeof(float);
        bytes += work_.capacity() * sizeof(float) + output_.capacity() * sizeof(float);
        bytes += output_gain_.memory_bytes();
        bytes += output_limiter_.memory_bytes();
        bytes += stereo_filter_->memory_bytes();
        bytes += step_tremolo_->memory_bytes();
        bytes += deluxe_looper_->memory_bytes();
        bytes += chromatic_pitch_delay_->memory_bytes();
        bytes += cabinet_ir_.memory_bytes();
        bytes += octave_divider_.memory_bytes();
        bytes += double_take_.memory_bytes();
        bytes += eq5_graphic_.memory_bytes();
        bytes += optical_tremolo_->memory_bytes();
        bytes += fractal_bias_trem_.memory_bytes();
        bytes += vibratone_.memory_bytes();
        bytes += uni_verse_vibe_.memory_bytes();
        bytes += four_osc_generator_.memory_bytes();
        bytes += poly_detune_.memory_bytes();
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
        if (index == 96U) {
            const std::size_t bytes = stereo_filter_->memory_bytes();
            return bytes > std::numeric_limits<std::uint32_t>::max()
                       ? std::numeric_limits<std::uint32_t>::max()
                       : static_cast<std::uint32_t>(bytes);
        }
        if (index == 97U) {
            const std::size_t bytes = step_tremolo_->memory_bytes();
            return bytes > std::numeric_limits<std::uint32_t>::max()
                       ? std::numeric_limits<std::uint32_t>::max()
                       : static_cast<std::uint32_t>(bytes);
        }
        if (index == 98U) {
            const std::size_t bytes = deluxe_looper_->memory_bytes();
            return bytes > std::numeric_limits<std::uint32_t>::max()
                       ? std::numeric_limits<std::uint32_t>::max()
                       : static_cast<std::uint32_t>(bytes);
        }
        if (index == 99U) {
            const std::size_t bytes = chromatic_pitch_delay_->memory_bytes();
            return bytes > std::numeric_limits<std::uint32_t>::max()
                       ? std::numeric_limits<std::uint32_t>::max()
                       : static_cast<std::uint32_t>(bytes);
        }
        if (index == 100U) {
            const std::size_t bytes = cabinet_ir_.memory_bytes();
            return bytes > std::numeric_limits<std::uint32_t>::max()
                       ? std::numeric_limits<std::uint32_t>::max()
                       : static_cast<std::uint32_t>(bytes);
        }
        if (index == 101U) {
            return static_cast<std::uint32_t>(octave_divider_.memory_bytes());
        }
        if (index == 102U) {
            const std::size_t bytes = double_take_.memory_bytes();
            return bytes > std::numeric_limits<std::uint32_t>::max()
                       ? std::numeric_limits<std::uint32_t>::max()
                       : static_cast<std::uint32_t>(bytes);
        }
        if (index == 103U) {
            return static_cast<std::uint32_t>(eq5_graphic_.memory_bytes());
        }
        if (index == 104U) {
            return static_cast<std::uint32_t>(optical_tremolo_->memory_bytes());
        }
        if (index == 105U) {
            return static_cast<std::uint32_t>(fractal_bias_trem_.memory_bytes());
        }
        if (index == 106U) {
            const std::size_t bytes = vibratone_.memory_bytes();
            return bytes > std::numeric_limits<std::uint32_t>::max()
                       ? std::numeric_limits<std::uint32_t>::max()
                       : static_cast<std::uint32_t>(bytes);
        }
        if (index == 107U) {
            const std::size_t bytes = uni_verse_vibe_.memory_bytes();
            return bytes > std::numeric_limits<std::uint32_t>::max()
                       ? std::numeric_limits<std::uint32_t>::max()
                       : static_cast<std::uint32_t>(bytes);
        }
        if (index == 108U) {
            const std::size_t bytes = four_osc_generator_.memory_bytes();
            return bytes > std::numeric_limits<std::uint32_t>::max()
                       ? std::numeric_limits<std::uint32_t>::max()
                       : static_cast<std::uint32_t>(bytes);
        }
        if (index == 109U) {
            const std::size_t bytes = poly_detune_.memory_bytes();
            return bytes > std::numeric_limits<std::uint32_t>::max()
                       ? std::numeric_limits<std::uint32_t>::max()
                       : static_cast<std::uint32_t>(bytes);
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
    std::unique_ptr<tabletfx::StereoFilter> stereo_filter_;
    std::unique_ptr<tabletfx::StepTremolo> step_tremolo_;
    std::unique_ptr<tabletfx::DeluxeLooper> deluxe_looper_;
    std::unique_ptr<tabletfx::ChromaticPitchDelay> chromatic_pitch_delay_;
    tabletfx::Gain output_gain_;
    tabletfx::PeakLimiter output_limiter_;
    tabletfx::SignalControls signal_controls_;
    tabletfx::ChromaticTuner tuner_;
    tabletfx::LevelMeter input_meter_;
    tabletfx::LevelMeter output_meter_;
    tabletfx::CabinetIr cabinet_ir_;
    tabletfx::effects::OctaveDivider octave_divider_;
    tabletfx::DoubleTake double_take_;
    tabletfx::EQ5Graphic eq5_graphic_;
    std::unique_ptr<tabletfx::OpticalTremolo> optical_tremolo_;
    tabletfx::BiasTrem fractal_bias_trem_;
    tabletfx::Vibratone vibratone_;
    tabletfx::Vibe uni_verse_vibe_;
    tabletfx::effects::FourOscGenerator four_osc_generator_;
    tabletfx::PolyDetune poly_detune_;
    bool output_gain_enabled_ = true;
    bool output_limiter_enabled_ = true;
    bool stereo_filter_enabled_ = false;
    bool step_tremolo_enabled_ = false;
    bool deluxe_looper_enabled_ = false;
    bool chromatic_pitch_delay_enabled_ = false;
    bool cabinet_ir_enabled_ = false;
    bool octave_divider_enabled_ = false;
    bool double_take_enabled_ = false;
    bool eq5_graphic_enabled_ = false;
    bool optical_tremolo_enabled_ = false;
    bool optical_tremolo_mute_bypass_ = false;
    bool fractal_bias_trem_enabled_ = false;
    bool vibratone_enabled_ = false;
    bool uni_verse_vibe_enabled_ = false;
    bool four_osc_generator_enabled_ = false;
    bool poly_detune_enabled_ = false;
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
    void* opaque, std::uint32_t type, float frequency_hz,
    float frequency_multiplier, float high_cut_hz, float mix,
    std::uint32_t pitch_tracking, std::uint32_t tracking_mode) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_ring_modulator(
            type, frequency_hz, frequency_multiplier, high_cut_hz, mix,
            pitch_tracking, tracking_mode);
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

TABLETFX_WASM_EXPORT void tf_chain_set_asheville_pattrn(
    void* opaque, std::uint32_t bank, std::uint32_t pattern,
    float band_level_1, float band_level_2, float band_level_3,
    float band_level_4, float band_level_5, float band_level_6,
    float band_level_7, float band_level_8, float drive_db,
    float output_db, float mix, float pattern_rate_hz,
    float resonance_q, float envelope_shape, float smoothing_ms) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_asheville_pattrn(
            bank, pattern, band_level_1, band_level_2, band_level_3,
            band_level_4, band_level_5, band_level_6, band_level_7,
            band_level_8, drive_db, output_db, mix, pattern_rate_hz,
            resonance_q, envelope_shape, smoothing_ms);
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
    float level_db, float morph_mode, float start_vowel, float middle_vowel,
    float end_vowel, float control_percent, float balance_percent) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_formant_filter(
            vowel_position, resonance_q, mix, level_db, morph_mode, start_vowel,
            middle_vowel, end_vowel, control_percent, balance_percent);
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
    float threshold_db, float attenuation_db, float release_ms,
    std::uint32_t spillover, std::uint32_t time_mode, float tempo_bpm,
    std::uint32_t tap_division, float low_cut_hz, float high_cut_hz,
    float mod_rate_hz, float mod_depth_ms,
    std::uint32_t modulation_mode) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_ducking_delay(
            time_ms, feedback, mix, threshold_db, attenuation_db, release_ms,
            spillover, time_mode, tempo_bpm, tap_division, low_cut_hz,
            high_cut_hz, mod_rate_hz, mod_depth_ms, modulation_mode);
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
    float low_cut_hz, float high_cut_hz, std::uint32_t time_mode,
    float tempo_bpm, std::uint32_t division) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_filtered_delay(
            time_ms, feedback, mix, low_cut_hz, high_cut_hz, time_mode,
            tempo_bpm, division);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_sweep_delay(
    void* opaque, float time_ms, float feedback, float mix,
    float start_hz, float stop_hz, float resonance_q, float rate_hz,
    std::uint32_t waveform, float phase_degrees,
    std::uint32_t filter_type, std::uint32_t rate_mode,
    float tempo_bpm, std::uint32_t division) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_sweep_delay(
            time_ms, feedback, mix, start_hz, stop_hz, resonance_q,
            rate_hz, waveform, phase_degrees, filter_type, rate_mode,
            tempo_bpm, division);
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

TABLETFX_WASM_EXPORT void tf_chain_set_diatonic_custom_scale(
    void* opaque, std::uint32_t note_count, std::uint32_t note_0,
    std::uint32_t note_1, std::uint32_t note_2, std::uint32_t note_3,
    std::uint32_t note_4, std::uint32_t note_5, std::uint32_t note_6,
    std::uint32_t note_7) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_diatonic_custom_scale(
            note_count, note_0, note_1, note_2, note_3, note_4, note_5, note_6,
            note_7);
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

TABLETFX_WASM_EXPORT void tf_chain_set_four_osc_generator(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_four_osc_generator(values, value_count);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_poly_detune(
    void* opaque, std::uint32_t voice_count, float rate_hz, float depth_ms,
    float width, float mix, float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_poly_detune(
            voice_count, rate_hz, depth_ms, width, mix, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_fractal_three_voice_synth(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_fractal_three_voice_synth(values, value_count);
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

TABLETFX_WASM_EXPORT void tf_chain_set_celestial_reverb(
    void* opaque, const float* values) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_celestial_reverb(values);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_cloud_reverb(
    void* opaque, const float* values) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_cloud_reverb(values);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_mercator_dist(
    void* opaque, const float* values) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_mercator_distortion(values);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_manual_thru_zero_flanger(
    void* opaque, const float* values) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_manual_thru_zero_flanger(values);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_octobot(
    void* opaque, const float* values) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_octobot(values);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_am_ring_mod(
    void* opaque, float rate_hz, float depth, float mix,
    float level_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_am_ring_mod(rate_hz, depth, mix, level_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_tape_echo(
    void* opaque, std::uint32_t time_mode, float time_ms,
    float tempo_bpm, std::uint32_t tap_division, float feedback,
    float mix, float record_level, float tape_bias, float tape_noise,
    float wow_flutter, std::uint32_t spillover,
    std::uint32_t independent_stereo_times,
    float left_time_ms, float right_time_ms,
    std::uint32_t left_tap_division,
    std::uint32_t right_tap_division) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_tape_echo(
            time_mode, time_ms, tempo_bpm, tap_division, feedback, mix,
            record_level, tape_bias, tape_noise, wow_flutter, spillover,
            independent_stereo_times, left_time_ms, right_time_ms,
            left_tap_division, right_tap_division);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_octave_fuzz(
    void* opaque, float boost, float volume,
    std::uint32_t octave_switch) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_octave_fuzz(boost, volume, octave_switch);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_rock_box_dist(
    void* opaque, float drive, float tone, float level_db,
    float smoothing_ms) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_rock_box_dist(
            drive, tone, level_db, smoothing_ms);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_rock_box_boost(
    void* opaque, float boost_db, float level_db,
    float smoothing_ms) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_rock_box_boost(
            boost_db, level_db, smoothing_ms);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_stereo_panner(
    void* opaque, float rate_hz, float width_percent, std::uint32_t waveform,
    std::uint32_t tempo_sync, float tempo_bpm, std::uint32_t division,
    float duty_cycle_percent, float shape_percent, float phase_degrees,
    float pan_center_percent, float ducking_percent, float level_db,
    float balance_percent, float smoothing_ms) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_stereo_panner(
            rate_hz, width_percent, waveform, tempo_sync, tempo_bpm, division,
            duty_cycle_percent, shape_percent, phase_degrees,
            pan_center_percent, ducking_percent, level_db, balance_percent,
            smoothing_ms);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_stereo_crossover(
    void* opaque, const float* values) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_stereo_crossover(values);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_convolution_reverb(
    void* opaque, float mix, float level_db, float high_pass_hz,
    float low_pass_hz, std::uint32_t tails) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_convolution_reverb(
            mix, level_db, high_pass_hz, low_pass_hz, tails);
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

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_load_cabinet_ir(
    void* opaque, const float* samples, std::uint32_t frames,
    std::uint32_t ir_channels, std::uint32_t ir_sample_rate,
    std::uint32_t bits_per_sample) noexcept {
    if (opaque == nullptr) return 0;
    return as_chain(opaque)->load_cabinet_ir(
               samples, frames, ir_channels, ir_sample_rate, bits_per_sample)
        ? 1U
        : 0U;
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_cabinet_ir_latency_frames() noexcept {
    return WasmChain::cabinet_ir_latency_frames();
}

TABLETFX_WASM_EXPORT void tf_chain_set_octave_divider(
    void* opaque, float input_gain, float voice1_level, float voice1_pan,
    float voice2_level, float voice2_pan, float mix, float output_level) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_octave_divider(
            input_gain, voice1_level, voice1_pan, voice2_level, voice2_pan,
            mix, output_level);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_double_take(
    void* opaque, std::uint32_t doubles, float slop, float sensitivity,
    float dry_level, float wet_level, std::uint32_t source) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_double_take(
            doubles, slop, sensitivity, dry_level, wet_level, source);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_eq5_graphic(
    void* opaque, float band1_db, float band2_db, float band3_db,
    float band4_db, float band5_db, float overall_gain_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_eq5_graphic(
            band1_db, band2_db, band3_db, band4_db, band5_db,
            overall_gain_db);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_optical_tremolo(
    void* opaque, std::uint32_t variant, float rate_hz, float depth,
    std::uint32_t lfo, std::uint32_t tempo_sync, float tempo_bpm,
    float beats_per_cycle, float duty_cycle_percent, float shape_percent,
    float phase_degrees, float start_phase_degrees,
    std::uint32_t trigger_enabled, float trigger_threshold_dbfs,
    float ducking_percent, float mix, float level_db,
    float balance_percent, std::uint32_t bypass_mode) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_optical_tremolo(
            variant, rate_hz, depth, lfo, tempo_sync, tempo_bpm,
            beats_per_cycle, duty_cycle_percent, shape_percent,
            phase_degrees, start_phase_degrees, trigger_enabled,
            trigger_threshold_dbfs, ducking_percent, mix, level_db,
            balance_percent, bypass_mode);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_fractal_bias_trem(
    void* opaque, float rate_hz, float depth, std::uint32_t waveform,
    std::uint32_t tempo_sync, float tempo_bpm, float beats_per_cycle,
    std::uint32_t trigger_sync, float trigger_threshold_db,
    float ducking, float bias_drive, float mix, float level_db,
    float balance, std::uint32_t bypass) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_fractal_bias_trem(
            rate_hz, depth, waveform, tempo_sync, tempo_bpm, beats_per_cycle,
            trigger_sync, trigger_threshold_db, ducking, bias_drive, mix,
            level_db, balance, bypass);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_vibratone(
    void* opaque, float drive, float tone, float level_db,
    float fast_speed_hz, float slow_speed_hz, std::uint32_t speed_fast,
    float ramp_ms, float sensitivity, std::uint32_t dynamics_switch) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_vibratone(
            drive, tone, level_db, fast_speed_hz, slow_speed_hz, speed_fast,
            ramp_ms, sensitivity, dynamics_switch);
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

TABLETFX_WASM_EXPORT void tf_chain_set_five_band_eq(
    void* opaque,
    float f1, float g1, float q1, std::uint32_t shape1, float slope1,
    float f2, float g2, float q2, std::uint32_t shape2, float slope2,
    float f3, float g3, float q3, std::uint32_t shape3, float slope3,
    float f4, float g4, float q4, std::uint32_t shape4, float slope4,
    float f5, float g5, float q5, std::uint32_t shape5, float slope5,
    std::uint32_t solo_band) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_five_band_eq(
            f1, g1, q1, shape1, slope1, f2, g2, q2, shape2, slope2,
            f3, g3, q3, shape3, slope3, f4, g4, q4, shape4, slope4,
            f5, g5, q5, shape5, slope5, solo_band);
    }
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_looper_command(
    void* opaque, std::uint32_t command) noexcept {
    return opaque != nullptr && as_chain(opaque)->looper_command(command) ? 1U : 0U;
}

TABLETFX_WASM_EXPORT void tf_chain_set_looper_options(
    void* opaque, std::uint32_t reverse, std::uint32_t half_speed,
    float overdub_level, std::uint32_t auto_record_enabled,
    float auto_record_threshold_db) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_looper(reverse != 0U, half_speed != 0U,
                                           overdub_level,
                                           auto_record_enabled != 0U,
                                           auto_record_threshold_db);
    }
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_looper_state(void* opaque) noexcept {
    return opaque == nullptr ? 0U : as_chain(opaque)->looper_state();
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_looper_frames(void* opaque) noexcept {
    return opaque == nullptr ? 0U : as_chain(opaque)->looper_frames();
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_deluxe_looper_command(
    void* opaque, std::uint32_t command) noexcept {
    return opaque != nullptr &&
                   as_chain(opaque)->deluxe_looper_command(command)
               ? 1U
               : 0U;
}

TABLETFX_WASM_EXPORT void tf_chain_set_deluxe_looper_options(
    void* opaque, std::uint32_t reverse, std::uint32_t half_speed,
    float playback_level, float overdub_decay_db,
    float low_cut_hz, float high_cut_hz) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_deluxe_looper(
            reverse != 0U, half_speed != 0U, playback_level,
            overdub_decay_db, low_cut_hz, high_cut_hz);
    }
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_deluxe_looper_state(
    void* opaque) noexcept {
    return opaque == nullptr ? 0U : as_chain(opaque)->deluxe_looper_state();
}

TABLETFX_WASM_EXPORT std::uint32_t tf_chain_deluxe_looper_frames(
    void* opaque) noexcept {
    return opaque == nullptr ? 0U : as_chain(opaque)->deluxe_looper_frames();
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
    float input_gain_db, float mix, float level_db, float balance,
    std::uint32_t input_select, float input_diffusion_mix,
    float input_diffusion_time_ms, std::uint32_t kill_dry) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_plex_delay_global(
            active_lines, tempo_bpm, master_time, master_level, master_pan,
            decay_seconds, diffusion, low_cut_hz, high_cut_hz,
            pre_delay_ms, stack_mode, input_gain_db, mix, level_db, balance,
            input_select, input_diffusion_mix, input_diffusion_time_ms,
            kill_dry);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_plex_delay_advanced(
    void* opaque, std::uint32_t filter_type, float filter_frequency_hz,
    float filter_q, float filter_gain_db, std::uint32_t filter_lfo_enabled,
    std::uint32_t filter_lfo_type, float filter_lfo_rate_hz,
    float filter_mod_frequency_hz, std::uint32_t envelope_enabled,
    float envelope_threshold_db, float envelope_attack_ms,
    float envelope_release_ms, float ducker_attenuation_db,
    float ducker_threshold_db, float ducker_release_ms,
    float stereo_spread) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_plex_delay_advanced(
            filter_type, filter_frequency_hz, filter_q, filter_gain_db,
            filter_lfo_enabled, filter_lfo_type, filter_lfo_rate_hz,
            filter_mod_frequency_hz, envelope_enabled,
            envelope_threshold_db, envelope_attack_ms, envelope_release_ms,
            ducker_attenuation_db, ducker_threshold_db, ducker_release_ms,
            stereo_spread);
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
    std::uint32_t stages, float spread, float level,
    std::uint32_t rate_mode, std::uint32_t note_division,
    float tempo_bpm) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_vibe(rate_hz, depth, trem,
                                         stages, spread, level,
                                         rate_mode, note_division, tempo_bpm);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_uni_verse_vibe(
    void* opaque, float speed_hz, float intensity, float volume,
    std::uint32_t rate_mode, std::uint32_t note_division,
    float tempo_bpm, std::uint32_t tone_mode,
    std::uint32_t modulation_mode) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_uni_verse_vibe(
            speed_hz, intensity, volume, rate_mode, note_division,
            tempo_bpm, tone_mode, modulation_mode);
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

TABLETFX_WASM_EXPORT void tf_chain_set_multi_delay_topology(
    void* opaque, std::uint32_t topology, float series_feedback,
    std::uint32_t feedback_send, std::uint32_t feedback_return,
    float master_time, float master_feedback, float motor_speed) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_multi_delay_topology(
            topology, series_feedback, feedback_send, feedback_return,
            master_time, master_feedback, motor_speed);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_multi_delay_time_lfo(
    void* opaque, std::uint32_t lfo_index, std::uint32_t waveform,
    float phase_degrees, float rate_hz, std::uint32_t tempo_sync,
    std::uint32_t division, float depth_ms) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_multi_delay_time_lfo(
            lfo_index, waveform, phase_degrees, rate_hz, tempo_sync,
            division, depth_ms);
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

TABLETFX_WASM_EXPORT void tf_chain_set_poly_sustain(
    void* opaque, float interval_semitones, float attack_ms, float decay_ms,
    float fx_level, float level_db, std::uint32_t operation) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_poly_sustain(
            interval_semitones, attack_ms, decay_ms, fx_level, level_db,
            operation);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_step_filter_delay(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_step_filter_delay(values, value_count);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_volume_pan(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_volume_pan(values, value_count);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_stereo_filter(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_stereo_filter(values, value_count);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_step_tremolo(
    void* opaque, const float* values, std::uint32_t value_count) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_step_tremolo(values, value_count);
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

TABLETFX_WASM_EXPORT void tf_chain_set_delay_bbd_mode(
    void* opaque, std::uint32_t mod) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_delay_bbd_mode(mod != 0U);
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

TABLETFX_WASM_EXPORT void tf_chain_set_pitch_expression(
    void* opaque, float target_semitones, float position) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_pitch_expression(target_semitones, position);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_chromatic_pitch_delay(
    void* opaque, float pitch_semitones, float time_ms, float feedback,
    float tone, float mix, std::uint32_t time_mode, float tempo_bpm,
    std::uint32_t tap_division, std::uint32_t spillover) noexcept {
    if (opaque == nullptr) return;
    tabletfx::ChromaticPitchDelayParameters parameters;
    parameters.pitch_semitones = pitch_semitones;
    parameters.time_ms = time_ms;
    parameters.feedback = feedback;
    parameters.tone = tone;
    parameters.mix = mix;
    parameters.time_mode =
        static_cast<tabletfx::ChromaticPitchDelayTimeMode>(time_mode);
    parameters.tempo_bpm = tempo_bpm;
    parameters.tap_division =
        static_cast<tabletfx::ChromaticPitchDelayTapDivision>(tap_division);
    parameters.spillover = spillover != 0U;
    as_chain(opaque)->configure_chromatic_pitch_delay(parameters);
}

} // extern "C"
