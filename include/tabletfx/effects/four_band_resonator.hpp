#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

enum class FourBandResonatorMode : std::uint8_t {
    Manual,
    Chord,
};

enum class FourBandResonatorChord : std::uint8_t {
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

enum class FourBandResonatorFilterLocation : std::uint8_t {
    Pre,
    Post,
};
enum class FourBandResonatorInputMode : std::uint8_t {
    Mono,
    Stereo,
};

struct FourBandResonatorParameters {
    FourBandResonatorMode mode = FourBandResonatorMode::Manual;
    FourBandResonatorChord chord = FourBandResonatorChord::Major;
    // Manual mode uses four individual frequencies scaled by this
    // project-authored factor. Chord mode uses chord_root_frequency_hz instead.
    float master_frequency = 1.0F;
    std::array<float, 4> manual_frequency_hz{110.0F, 164.81F, 220.0F,
                                              329.63F};
    float chord_root_frequency_hz = 110.0F;
    std::array<float, 4> feedback{0.75F, 0.75F, 0.75F, 0.75F};
    std::array<float, 4> q{2.0F, 2.0F, 2.0F, 2.0F};
    std::array<float, 4> level{0.5F, 0.5F, 0.5F, 0.5F};
    std::array<float, 4> pan{-0.75F, -0.25F, 0.25F, 0.75F};
    std::array<FourBandResonatorFilterLocation, 4> filter_location{
        FourBandResonatorFilterLocation::Post,
        FourBandResonatorFilterLocation::Post,
        FourBandResonatorFilterLocation::Post,
        FourBandResonatorFilterLocation::Post};
    FourBandResonatorInputMode input_mode =
        FourBandResonatorInputMode::Mono;
    float master_level = 1.0F;
    // Scales the four band pan positions. A negative value reverses their
    // stereo image; 1.0 leaves the per-band pans unchanged.
    float master_pan = 1.0F;
    float master_feedback = 1.0F;
    float master_q = 1.0F;
    float mix = 0.5F;
    // Project-authored effect-input trim. This scales the resonator feed only;
    // the dry path and the existing feedback return are unchanged.
    float input_gain_db = 0.0F;
};

/*
 * Project-authored four-band feedback-delay resonator. Each band has an
 * independent fractional delay and band-pass filter. The public frequency
 * controls preserve the documented MANUAL/CHORD distinction and Manual
 * master scaling. Its input gain scales the wet effect feed only, leaving dry
 * audio and the existing feedback return unchanged. Each band's BPF can be
 * set before or after that band’s
 * resonator. The pre/post signal routing, master-frequency scale range,
 * feedback, Q, gain, panning, defaults, other bounds, and DSP details are
 * project-authored. Master Level/Pan/Feedback/Q and Mono/Stereo input routing
 * are also exposed with project-authored bounds and behavior. This does not
 * claim Fractal DSP or setting parity. The Fractal family guide does not
 * publish numeric Feedback or Q ranges.
 *
 * The delay buffers are allocated at construction. process() is allocation-
 * free and accepts irregular mono/stereo blocks. Reconfigure parameters from
 * the control thread, outside process().
 */
class FourBandResonator final : public Effect {
public:
    explicit FourBandResonator(float sample_rate = 48000.0F,
                               FourBandResonatorParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "four-band-resonator";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + delay_storage_.capacity() * sizeof(float);
    }

    void set_parameters(FourBandResonatorParameters parameters) noexcept;
    [[nodiscard]] FourBandResonatorParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] std::array<float, 4> effective_frequencies_hz() const noexcept {
        return effective_frequencies_hz_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kBands = 4;
    static constexpr std::size_t kMaximumChannels = 2;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kManualMinimumFrequencyHz = 100.0F;
    static constexpr float kManualMaximumFrequencyHz = 10000.0F;
    static constexpr float kChordRootMinimumFrequencyHz = 50.0F;
    static constexpr float kChordRootMaximumFrequencyHz = 5000.0F;
    static constexpr float kMinimumMasterFrequency = 0.1F;
    static constexpr float kMaximumMasterFrequency = 10.0F;
    static constexpr float kMinimumMasterLevel = 0.0F;
    static constexpr float kMaximumMasterLevel = 2.0F;
    static constexpr float kMinimumMasterFeedback = 0.0F;
    static constexpr float kMaximumMasterFeedback = 1.0F;
    static constexpr float kMinimumMasterQ = 0.1F;
    static constexpr float kMaximumMasterQ = 4.0F;
    static constexpr float kMinimumInputGainDb = -24.0F;
    static constexpr float kMaximumInputGainDb = 12.0F;
    static constexpr float kMinimumFeedback = 0.0F;
    static constexpr float kMaximumFeedback = 0.98F;
    static constexpr float kMinimumQ = 0.5F;
    static constexpr float kMaximumQ = 12.0F;
    static constexpr float kMaximumLevel = 2.0F;
    static constexpr float kMaximumInputMagnitude = 16.0F;
    static constexpr float kMaximumStateMagnitude = 1000000.0F;
    static constexpr float kPi = 3.14159265358979323846F;

    struct Coefficients {
        float b0 = 0.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    struct FilterState {
        float z1 = 0.0F;
        float z2 = 0.0F;
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static std::array<int, kBands> chord_intervals(
        FourBandResonatorChord chord) noexcept;
    void prepare_resonators() noexcept;
    [[nodiscard]] float filter(float input, const Coefficients& coefficients,
                               FilterState& state) const noexcept;
    [[nodiscard]] float delayed_sample(std::size_t band, std::uint16_t channel,
                                       float delay_samples) const noexcept;
    [[nodiscard]] float& buffer_sample(std::size_t band, std::uint16_t channel,
                                       std::size_t frame) noexcept;
    [[nodiscard]] const float& buffer_sample(
        std::size_t band, std::uint16_t channel,
        std::size_t frame) const noexcept;

    float sample_rate_ = 48000.0F;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> delay_storage_;
    FourBandResonatorParameters parameters_{};
    float input_gain_linear_ = 1.0F;
    std::array<float, kBands> effective_frequencies_hz_{};
    std::array<float, kBands> delay_samples_{};
    std::array<Coefficients, kBands> coefficients_{};
    std::array<std::array<FilterState, kBands>, kMaximumChannels> filter_states_{};
};

} // namespace tabletfx
