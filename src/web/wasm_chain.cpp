#include "tabletfx/effect.hpp"
#include "tabletfx/effects.hpp"
#include "tabletfx/effects/compressor.hpp"
#include "tabletfx/effects/filters.hpp"
#include "tabletfx/effects/peak_limiter.hpp"
#include "tabletfx/effects/pedal_compressor.hpp"
#include "tabletfx/effects/three_band_eq.hpp"
#include "tabletfx/effects/tremolo.hpp"
#include "tabletfx/effects/volume_pedal.hpp"
#include "tabletfx/effects/wah.hpp"

#include <algorithm>
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
          output_limiter_(static_cast<float>(sample_rate), -1.0F, 80.0F) {
        effects_.reserve(12);
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
        effects_.push_back({std::make_unique<tabletfx::SoftDrive>(3.0F), false});
        effects_.push_back({std::make_unique<tabletfx::Tremolo>(
                                static_cast<float>(sample_rate)),
                            false});
        effects_.push_back({std::make_unique<tabletfx::ThreeBandParametricEQ>(
                                static_cast<float>(sample_rate), channels),
                            false});
        effects_.push_back({std::make_unique<tabletfx::Delay>(
                                static_cast<float>(sample_rate), channels, 2000.0F,
                                350.0F, 0.25F, 0.25F),
                            false});
    }

    [[nodiscard]] std::size_t effect_count() const noexcept { return effects_.size() + 2; }

    [[nodiscard]] float* input_data() noexcept { return input_.data(); }
    [[nodiscard]] float* output_data() noexcept { return output_.data(); }

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
        std::copy_n(input_.begin(), samples, dry_.begin());
        std::copy_n(input_.begin(), samples, work_.begin());
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
        if (std::isfinite(mix)) {
            mix_ = std::clamp(mix, 0.0F, 1.0F);
        }
    }

    void finish(std::uint32_t frames) noexcept {
        if (frames_ == 0 || frames != frames_) {
            return;
        }
        const std::size_t samples = static_cast<std::size_t>(frames_) * channels_;
        if (mix_ <= 0.0F) {
            std::copy_n(dry_.begin(), samples, output_.begin());
        } else if (mix_ >= 1.0F) {
            std::copy_n(work_.begin(), samples, output_.begin());
        } else {
            const float dry_gain = 1.0F - mix_;
            for (std::size_t i = 0; i < samples; ++i) {
                output_[i] = dry_[i] * dry_gain + work_[i] * mix_;
            }
        }
    }

    void process_output_gain() noexcept {
        if (frames_ == 0 || !output_gain_enabled_) {
            return;
        }
        const std::size_t samples = static_cast<std::size_t>(frames_) * channels_;
        output_gain_.process(std::span<float>(output_.data(), samples), channels_);
    }

    void process_output_limiter() noexcept {
        if (frames_ == 0 || !output_limiter_enabled_) {
            return;
        }
        const std::size_t samples = static_cast<std::size_t>(frames_) * channels_;
        output_limiter_.process(std::span<float>(output_.data(), samples), channels_);
    }

    void reset() noexcept {
        for (EffectEntry& entry : effects_) {
            entry.effect->reset();
        }
        output_gain_.reset();
        output_limiter_.reset();
        frames_ = 0;
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
        static_cast<tabletfx::SoftDrive*>(effects_[8].effect.get())->set_drive(drive);
    }

    void configure_eq(float f1, float g1, float q1, float f2, float g2, float q2,
                      float f3, float g3, float q3) noexcept {
        static_cast<tabletfx::ThreeBandParametricEQ*>(effects_[10].effect.get())
            ->configure(f1, g1, q1, f2, g2, q2, f3, g3, q3);
    }

    void configure_delay(float delay_ms, float feedback, float mix) noexcept {
        static_cast<tabletfx::Delay*>(effects_[11].effect.get())
            ->configure(delay_ms, feedback, mix);
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

    void configure_tremolo(float rate_hz, float depth, std::uint32_t waveform,
                           std::uint32_t tempo_sync, float tempo_bpm,
                           float beats_per_cycle) noexcept {
        auto* tremolo = static_cast<tabletfx::Tremolo*>(effects_[9].effect.get());
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
    float mix_ = 1.0F;
    std::vector<EffectEntry> effects_;
    std::vector<float> input_;
    std::vector<float> dry_;
    std::vector<float> work_;
    std::vector<float> output_;
    tabletfx::Gain output_gain_;
    tabletfx::PeakLimiter output_limiter_;
    bool output_gain_enabled_ = true;
    bool output_limiter_enabled_ = true;
};

WasmChain* as_chain(void* opaque) noexcept {
    return static_cast<WasmChain*>(opaque);
}

} // namespace

extern "C" {

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

TABLETFX_WASM_EXPORT void tf_chain_set_tremolo(
    void* opaque, float rate_hz, float depth, std::uint32_t waveform,
    std::uint32_t tempo_sync, float tempo_bpm, float beats_per_cycle) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_tremolo(rate_hz, depth, waveform,
                                            tempo_sync, tempo_bpm,
                                            beats_per_cycle);
    }
}

TABLETFX_WASM_EXPORT void tf_chain_set_drive(void* opaque, float drive) noexcept {
    if (opaque != nullptr) {
        as_chain(opaque)->configure_drive(drive);
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

} // extern "C"
