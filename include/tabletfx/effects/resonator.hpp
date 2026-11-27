#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

enum class ResonatorMode : std::uint8_t {
    Manual,
    Chordal,
};

enum class ResonatorChord : std::uint8_t {
    Major,
    Major6,
    Dominant7,
    Major7,
    Minor,
    Minor6,
    Minor7,
    Diminished,
    Augmented,
};

enum class ResonatorFilterLocation : std::uint8_t {
    Pre,
    Post,
};

enum class ResonatorInputMode : std::uint8_t {
    Mono,
    Stereo,
};

struct ResonatorParameters {
    ResonatorMode mode = ResonatorMode::Manual;
    ResonatorChord chord = ResonatorChord::Major;
    // Manual mode multiplies each voice's frequency by master_frequency.
    // In Chordal mode, chord_root_frequency_hz is used instead.
    float master_frequency = 1.0F;
    std::array<float, 4> manual_frequency_hz{
        110.0F, 164.81F, 220.0F, 329.63F};
    float chord_root_frequency_hz = 110.0F;

    std::array<float, 4> feedback{0.55F, 0.55F, 0.55F, 0.55F};
    std::array<float, 4> filter_q{2.0F, 2.0F, 2.0F, 2.0F};
    std::array<float, 4> level{0.4F, 0.4F, 0.4F, 0.4F};
    std::array<float, 4> pan{-0.75F, -0.25F, 0.25F, 0.75F};
    std::array<ResonatorFilterLocation, 4> filter_location{
        ResonatorFilterLocation::Post,
        ResonatorFilterLocation::Post,
        ResonatorFilterLocation::Post,
        ResonatorFilterLocation::Post};
    ResonatorInputMode input_mode = ResonatorInputMode::Mono;

    float master_level = 0.8F;
    // A negative Master Pan value reverses the per-voice stereo image.
    float master_pan = 1.0F;
    float master_feedback = 1.0F;
    float master_q = 1.0F;

    float mix = 0.35F;
    float level_db = 0.0F;
    float balance = 0.0F;
    float input_gain_db = 0.0F;
    bool global_mix_enabled = false;
    float global_mix_offset_percentage_points = 0.0F;
};

/*
 * Project-authored resonator/filter based on the Fractal Audio Blocks Guide
 * (November 2026, printed p. 92). The guide describes four parallel,
 * feedback-fed delays and four matching band-pass filters. This effect does
 * not detect notes or track pitch and is not a tracked synthesizer.
 *
 * The guide's controls are Mode (MANUAL or CHORDAL); Master Frequency in
 * MANUAL mode or Frequency (the 50–5,000 Hz chord root) in CHORDAL; Chord
 * (Major, Major 6, Dominant 7, Major 7, Minor, Minor 6, Minor 7, Diminished,
 * Augmented); Master Level/Pan/Feedback/Q; Input Mode (MONO sums L/R to all
 * voices, STEREO sends L to voices 1–2 and R to 3–4); and, for each voice,
 * Frequency (100–10,000 Hz in MANUAL), Feedback, Filter Location (pre/post),
 * Filter Q, Level, and Pan. This project uses an arithmetic mean for MONO
 * stereo input to avoid a 6 dB increase. The specific four-note voicings
 * below are project choices because the guide does not specify their register
 * or exact voice assignment. It also gives no numeric bounds or defaults for
 * feedback, Q, levels, pan, master scales, mix-page controls, or its pre/post
 * filter switch; those bounds, defaults, equal-power panning, and DSP details
 * are project-authored as well.
 *
 * The Mix, Level, Balance, and Input Gain names correspond to the guide's
 * Mix page, with project-defined ranges and behavior. The guide lists a
 * Global Mix switch that opts this block into a shared ±50% Effects Mix
 * offset; it does not specify the combination equation. This project's
 * default-off Global Mix option adds a −50…+50 percentage-point offset to
 * the stored base Mix and clamps the effective value to [0, 1]. It is a
 * project interpretation, not a Fractal setting or parity claim. Bypass and
 * Bypass Mode remain host-level controls.
 * process_bypassed() emits the stored feedback tail over the dry signal, as
 * supported by the shared Effect interface.
 *
 * Ring storage is allocated at construction for a bounded 50 Hz minimum
 * effective frequency. process() and process_bypassed() allocate no memory.
 * Change parameters from the control thread, outside audio processing.
 * Source: https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
 */
class Resonator final : public Effect {
public:
    explicit Resonator(float sample_rate = 48000.0F,
                       ResonatorParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "resonator";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + delay_storage_.capacity() * sizeof(float);
    }

    void set_parameters(ResonatorParameters parameters) noexcept;
    [[nodiscard]] ResonatorParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] std::array<float, 4> effective_frequencies_hz() const noexcept {
        return effective_frequencies_hz_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kVoiceCount = 4U;
    static constexpr std::size_t kStorageChannels = 2U;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kManualMinimumFrequencyHz = 100.0F;
    static constexpr float kManualMaximumFrequencyHz = 10000.0F;
    static constexpr float kChordRootMinimumFrequencyHz = 50.0F;
    static constexpr float kChordRootMaximumFrequencyHz = 5000.0F;
    static constexpr float kMaximumInputMagnitude = 8.0F;
    static constexpr float kMaximumStateMagnitude = 64.0F;
    static constexpr float kMaximumOutputMagnitude = 64.0F;
    static constexpr float kMaximumFeedback = 0.90F;
    static constexpr float kPi = 3.14159265358979323846F;

    struct BiquadCoefficients {
        float b0 = 0.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    struct BiquadState {
        float z1 = 0.0F;
        float z2 = 0.0F;
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static std::array<int, kVoiceCount> chord_intervals(
        ResonatorChord chord) noexcept;
    void prepare_voices() noexcept;
    void process_internal(std::span<float> interleaved,
                          std::uint16_t channels, bool bypassed) noexcept;
    [[nodiscard]] float filter(float input,
                               const BiquadCoefficients& coefficients,
                               BiquadState& state) const noexcept;
    [[nodiscard]] float delayed_sample(std::size_t voice,
                                       std::uint16_t channel) const noexcept;
    [[nodiscard]] float& buffer_sample(std::size_t voice,
                                       std::uint16_t channel,
                                       std::size_t frame) noexcept;
    [[nodiscard]] const float& buffer_sample(std::size_t voice,
                                             std::uint16_t channel,
                                             std::size_t frame) const noexcept;

    float sample_rate_ = 48000.0F;
    std::size_t capacity_frames_ = 0U;
    std::size_t write_frame_ = 0U;
    std::vector<float> delay_storage_;
    ResonatorParameters parameters_{};
    float input_gain_linear_ = 1.0F;
    float output_level_linear_ = 1.0F;
    float effective_mix_ = 0.35F;
    std::array<float, kVoiceCount> effective_frequencies_hz_{};
    std::array<float, kVoiceCount> delay_samples_{};
    std::array<float, kVoiceCount> effective_feedback_{};
    std::array<float, kVoiceCount> pan_left_gain_{};
    std::array<float, kVoiceCount> pan_right_gain_{};
    std::array<BiquadCoefficients, kVoiceCount> coefficients_{};
    std::array<std::array<BiquadState, kVoiceCount>, kStorageChannels>
        filter_states_{};
};

} // namespace tabletfx
