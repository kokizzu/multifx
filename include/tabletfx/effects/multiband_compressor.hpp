#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class MultibandCrossoverSlope : std::uint8_t {
    Db12,
    Db24,
};

enum class MultibandDetector : std::uint8_t {
    Peak,
    Rms,
};

struct MultibandBandParameters {
    float threshold_db = -24.0F;
    float ratio = 2.0F;
    float knee_db = 6.0F;
    float attack_ms = 10.0F;
    float release_ms = 100.0F;
    float level_db = 0.0F;
    MultibandDetector detector = MultibandDetector::Rms;
    bool mute = false;
};

struct MultibandCompressorParameters {
    float low_mid_crossover_hz = 250.0F;
    float mid_high_crossover_hz = 2500.0F;
    MultibandCrossoverSlope crossover_slope = MultibandCrossoverSlope::Db24;
    std::array<MultibandBandParameters, 3> bands{};
};

/*
 * Project-authored three-band compressor. The serial crossover splits low,
 * mid, and high bands; each band has a stereo-linked Peak or RMS detector and
 * independent threshold, ratio, knee, attack, release, level, and mute.
 *
 * Fractal Audio's Blocks Guide describes a three-band compressor with these
 * general controls, Peak/RMS detection, a 12/24 dB-per-octave crossover, and
 * crossover ranges of 50–5,000 Hz (Low/Mid) and 100–10,000 Hz (Mid/High).
 * Those are the only copied documented ranges. Project-authored defaults are
 * 250/2,500 Hz crossovers, 24 dB/oct, and per-band -24 dB threshold, 2:1 ratio,
 * 6 dB knee, 10 ms attack, 100 ms release, 0 dB level, RMS detector, and
 * unmuted. Project-authored bounds are -60..0 dB threshold, 1..20 ratio,
 * 0..18 dB knee, 0.1..1,000 ms attack, 5..2,000 ms release, and -24..+12 dB
 * level. The crossovers are additionally limited to 0.4 x sample rate, at
 * least 10 Hz apart, and a minimum 1.5:1 frequency ratio. Input is clamped to
 * +/-16 and output to +/-64. This
 * implementation's Linkwitz-Riley topology, coefficient design, and signal
 * behavior are project-authored; it is not a Fractal algorithm or sound
 * emulation. Auto Attack/Release is not included.
 *
 * The processor supports mono/stereo interleaved input, linked per-band gain
 * reduction to preserve the stereo image, fixed in-object state, and no
 * allocation in process(). Parameter updates are intended during graph setup.
 */
class MultibandCompressor final : public Effect {
public:
    explicit MultibandCompressor(
        float sample_rate = 48000.0F,
        MultibandCompressorParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "multiband-compressor";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   MultibandCompressorParameters parameters) noexcept;
    void set_parameters(MultibandCompressorParameters parameters) noexcept;
    [[nodiscard]] MultibandCompressorParameters parameters() const noexcept {
        return parameters_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct BiquadCoefficients {
        float b0 = 1.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    struct BiquadState {
        float z1 = 0.0F;
        float z2 = 0.0F;
    };

    struct BandState {
        float rms_power = 0.0F;
        float rms_coefficient = 0.0F;
        float compression_gain = 1.0F;
        float attack_coefficient = 0.0F;
        float release_coefficient = 0.0F;
        float threshold_linear = 0.0F;
        float knee_low_linear = 0.0F;
        float knee_high_linear = 0.0F;
        float compression_slope = 0.0F;
        float level_gain = 1.0F;
    };

    static constexpr std::size_t kBandCount = 3;
    static constexpr std::size_t kChannelCount = 2;
    static constexpr std::size_t kSectionsPerCrossover = 2;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 192000.0F;
    static constexpr float kMaximumInputMagnitude = 16.0F;
    static constexpr float kMaximumStateMagnitude = 1000000.0F;
    static constexpr float kDenormalGuard = 1.0e-20F;
    static constexpr float kRmsWindowMilliseconds = 10.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static MultibandCrossoverSlope sanitize_slope(
        MultibandCrossoverSlope slope) noexcept;
    [[nodiscard]] static MultibandDetector sanitize_detector(
        MultibandDetector detector) noexcept;
    [[nodiscard]] static float decibels_to_linear(float db) noexcept;
    [[nodiscard]] static float time_coefficient(float milliseconds,
                                                float sample_rate) noexcept;
    [[nodiscard]] static float process_biquad(
        BiquadState& state, const BiquadCoefficients& coefficients,
        float input) noexcept;
    [[nodiscard]] static float apply_compression_knee(
        float level_db, float threshold_db, float ratio, float knee_db) noexcept;
    [[nodiscard]] float target_band_gain(std::size_t band,
                                        float detector_level) const noexcept;
    void prepare_parameters(MultibandCompressorParameters parameters) noexcept;
    void prepare_crossovers() noexcept;
    void prepare_band(std::size_t band) noexcept;
    void reset_channel(std::uint16_t channel) noexcept;
    void process_split(
        float input,
        std::array<BiquadState, kSectionsPerCrossover>& low_states,
        std::array<BiquadState, kSectionsPerCrossover>& high_states,
        const std::array<BiquadCoefficients, kSectionsPerCrossover>& low_coefficients,
        const std::array<BiquadCoefficients, kSectionsPerCrossover>& high_coefficients,
        float& low, float& high) noexcept;

    float sample_rate_ = 48000.0F;
    MultibandCompressorParameters parameters_{};
    std::array<BiquadCoefficients, kSectionsPerCrossover> low_mid_lowpass_{};
    std::array<BiquadCoefficients, kSectionsPerCrossover> low_mid_highpass_{};
    std::array<BiquadCoefficients, kSectionsPerCrossover> mid_high_lowpass_{};
    std::array<BiquadCoefficients, kSectionsPerCrossover> mid_high_highpass_{};
    std::array<std::array<BiquadState, kSectionsPerCrossover>, kChannelCount>
        low_mid_lowpass_state_{};
    std::array<std::array<BiquadState, kSectionsPerCrossover>, kChannelCount>
        low_mid_highpass_state_{};
    std::array<std::array<BiquadState, kSectionsPerCrossover>, kChannelCount>
        mid_high_lowpass_state_{};
    std::array<std::array<BiquadState, kSectionsPerCrossover>, kChannelCount>
        mid_high_highpass_state_{};
    std::array<std::array<BiquadState, kSectionsPerCrossover>, kChannelCount>
        low_branch_mid_high_lowpass_state_{};
    std::array<std::array<BiquadState, kSectionsPerCrossover>, kChannelCount>
        low_branch_mid_high_highpass_state_{};
    std::array<BandState, kBandCount> band_state_{};
};

} // namespace tabletfx
