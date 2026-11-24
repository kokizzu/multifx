#pragma once

#include "tabletfx/effect.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace tabletfx {

enum class SevenBandEQShape : std::uint8_t {
    Peaking = 0,
    LowCut = 1,
    HighCut = 2,
};

struct SevenBandEQBand {
    bool enabled = true;
    SevenBandEQShape shape = SevenBandEQShape::Peaking;
    float frequency_hz = 1000.0F;
    float gain_db = 0.0F;
    float q = 0.707F;
    float slope_db_per_octave = 12.0F;
};

// Project-authored seven-band parametric EQ. Coefficients are prepared by
// configure(); process() uses fixed per-channel state, allocates nothing, and
// adds no fixed delay. Call configure/reset while not processing audio.
class StadiumSevenBandParametricEQ final : public Effect {
public:
    explicit StadiumSevenBandParametricEQ(float sample_rate = 48000.0F,
                                          std::uint16_t channels = 2)
        : sample_rate_(sample_rate), channels_(channels) {
        if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
            sample_rate_ > 384000.0F || channels_ == 0 || channels_ > 2) {
            throw std::invalid_argument(
                "StadiumSevenBandParametricEQ requires 8–384 kHz and mono or stereo");
        }
        configure(default_parameters());
    }

    [[nodiscard]] std::string_view id() const noexcept override {
        return "stadium-seven-band-peq";
    }

    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    [[nodiscard]] std::array<SevenBandEQBand, 7> parameters() const noexcept {
        return bands_;
    }

    void configure(std::array<SevenBandEQBand, 7> bands) noexcept {
        constexpr std::array<float, 7> kDefaultFrequencyHz{
            63.0F, 160.0F, 400.0F, 1000.0F, 2500.0F, 6300.0F, 12500.0F};
        const float max_frequency_hz = std::min(20000.0F, sample_rate_ * 0.45F);
        bool is_neutral = true;
        for (std::size_t index = 0; index < bands.size(); ++index) {
            const auto& source = bands[index];
            auto& destination = bands_[index];
            destination.enabled = source.enabled;
            destination.shape = known_shape(source.shape)
                                    ? source.shape
                                    : SevenBandEQShape::Peaking;
            destination.frequency_hz = bounded(
                source.frequency_hz, kDefaultFrequencyHz[index], 20.0F,
                max_frequency_hz);
            destination.gain_db = bounded(source.gain_db, 0.0F, -18.0F, 18.0F);
            destination.q = bounded(source.q, 0.707F, 0.25F, 10.0F);
            const float slope = bounded(
                source.slope_db_per_octave, 12.0F, 6.0F, 36.0F);
            destination.slope_db_per_octave =
                std::round(slope / 6.0F) * 6.0F;
            coefficients_[index] = make_band(destination, sample_rate_);
            if (destination.enabled &&
                (destination.shape != SevenBandEQShape::Peaking ||
                 destination.gain_db != 0.0F)) {
                is_neutral = false;
            }
        }
        neutral_ = is_neutral;
        reset();
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override {
        if (channels != channels_ || neutral_) return;
        const std::size_t frames = interleaved.size() / channels_;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                const std::size_t sample_index = frame * channels_ + channel;
                float sample = interleaved[sample_index];
                if (!std::isfinite(sample)) {
                    states_[channel] = {};
                    interleaved[sample_index] = 0.0F;
                    continue;
                }
                for (std::size_t band = 0; band < coefficients_.size(); ++band) {
                    if (!bands_[band].enabled) continue;
                    const auto& sections = coefficients_[band];
                    for (std::size_t section = 0; section < sections.count;
                         ++section) {
                        sample = process_section(
                            sample, sections.sections[section],
                            states_[channel][band][section]);
                    }
                }
                interleaved[sample_index] = sample;
            }
        }
    }

    void reset() noexcept override {
        for (auto& channel : states_) channel = {};
    }

private:
    struct Coefficients {
        float b0 = 1.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    struct SectionState {
        float z1 = 0.0F;
        float z2 = 0.0F;
    };

    struct BandCoefficients {
        std::array<Coefficients, 3> sections{};
        std::uint8_t count = 0;
    };

    [[nodiscard]] static constexpr bool known_shape(
        SevenBandEQShape shape) noexcept {
        return shape == SevenBandEQShape::Peaking ||
               shape == SevenBandEQShape::LowCut ||
               shape == SevenBandEQShape::HighCut;
    }

    [[nodiscard]] static constexpr std::array<SevenBandEQBand, 7>
    default_parameters() noexcept {
        return {{{true, SevenBandEQShape::Peaking, 63.0F, 0.0F, 0.707F, 12.0F},
                 {true, SevenBandEQShape::Peaking, 160.0F, 0.0F, 0.707F, 12.0F},
                 {true, SevenBandEQShape::Peaking, 400.0F, 0.0F, 0.707F, 12.0F},
                 {true, SevenBandEQShape::Peaking, 1000.0F, 0.0F, 0.707F, 12.0F},
                 {true, SevenBandEQShape::Peaking, 2500.0F, 0.0F, 0.707F, 12.0F},
                 {true, SevenBandEQShape::Peaking, 6300.0F, 0.0F, 0.707F, 12.0F},
                 {true, SevenBandEQShape::Peaking, 12500.0F, 0.0F, 0.707F, 12.0F}}};
    }

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float lower, float upper) noexcept {
        if (!std::isfinite(value)) value = fallback;
        return std::clamp(value, lower, upper);
    }

    [[nodiscard]] static Coefficients normalize(double b0, double b1,
                                                 double b2, double a0,
                                                 double a1, double a2) noexcept {
        if (!std::isfinite(a0) ||
            std::abs(a0) < std::numeric_limits<double>::min()) {
            return {};
        }
        const double inverse_a0 = 1.0 / a0;
        const Coefficients result{
            static_cast<float>(b0 * inverse_a0),
            static_cast<float>(b1 * inverse_a0),
            static_cast<float>(b2 * inverse_a0),
            static_cast<float>(a1 * inverse_a0),
            static_cast<float>(a2 * inverse_a0),
        };
        if (!std::isfinite(result.b0) || !std::isfinite(result.b1) ||
            !std::isfinite(result.b2) || !std::isfinite(result.a1) ||
            !std::isfinite(result.a2)) {
            return {};
        }
        return result;
    }

    [[nodiscard]] static Coefficients make_peak(float frequency_hz,
                                                 float gain_db, float q,
                                                 float sample_rate) noexcept {
        if (gain_db == 0.0F) return {};
        constexpr double kPi = 3.141592653589793238462643383279502884;
        const double omega = 2.0 * kPi * static_cast<double>(frequency_hz) /
                             static_cast<double>(sample_rate);
        const double cosine = std::cos(omega);
        const double amplitude = std::pow(10.0, static_cast<double>(gain_db) / 40.0);
        const double alpha = std::sin(omega) / (2.0 * static_cast<double>(q));
        return normalize(1.0 + alpha * amplitude, -2.0 * cosine,
                         1.0 - alpha * amplitude, 1.0 + alpha / amplitude,
                         -2.0 * cosine, 1.0 - alpha / amplitude);
    }

    [[nodiscard]] static Coefficients make_first_order(float frequency_hz,
                                                        float sample_rate,
                                                        bool high_pass) noexcept {
        constexpr double kPi = 3.141592653589793238462643383279502884;
        const double tangent = std::tan(
            kPi * static_cast<double>(frequency_hz) /
            static_cast<double>(sample_rate));
        const double inverse = 1.0 / (1.0 + tangent);
        const double a1 = (tangent - 1.0) * inverse;
        if (high_pass) {
            return normalize(inverse, -inverse, 0.0, 1.0, a1, 0.0);
        }
        return normalize(tangent * inverse, tangent * inverse, 0.0,
                         1.0, a1, 0.0);
    }

    [[nodiscard]] static Coefficients make_second_order(float frequency_hz,
                                                         float q,
                                                         float sample_rate,
                                                         bool high_pass) noexcept {
        constexpr double kPi = 3.141592653589793238462643383279502884;
        const double omega = 2.0 * kPi * static_cast<double>(frequency_hz) /
                             static_cast<double>(sample_rate);
        const double cosine = std::cos(omega);
        const double alpha = std::sin(omega) / (2.0 * static_cast<double>(q));
        const double b0 = high_pass ? (1.0 + cosine) / 2.0
                                    : (1.0 - cosine) / 2.0;
        const double b1 = high_pass ? -(1.0 + cosine) : 1.0 - cosine;
        return normalize(b0, b1, b0, 1.0 + alpha, -2.0 * cosine,
                         1.0 - alpha);
    }

    [[nodiscard]] static BandCoefficients make_butterworth(
        float frequency_hz, float slope_db_per_octave, float sample_rate,
        bool high_pass) noexcept {
        constexpr double kPi = 3.141592653589793238462643383279502884;
        const unsigned order = static_cast<unsigned>(
            std::lround(slope_db_per_octave / 6.0F));
        BandCoefficients result;
        std::size_t section = 0;
        if ((order & 1U) != 0U) {
            result.sections[section++] = make_first_order(
                frequency_hz, sample_rate, high_pass);
        }
        for (unsigned pair = 0; pair < order / 2U; ++pair) {
            const double angle = (2.0 * static_cast<double>(pair) + 1.0) *
                                 kPi / (2.0 * static_cast<double>(order));
            const float q = static_cast<float>(1.0 / (2.0 * std::sin(angle)));
            result.sections[section++] = make_second_order(
                frequency_hz, q, sample_rate, high_pass);
        }
        result.count = static_cast<std::uint8_t>(section);
        return result;
    }

    [[nodiscard]] static BandCoefficients make_band(
        const SevenBandEQBand& band, float sample_rate) noexcept {
        BandCoefficients result;
        if (!band.enabled) return result;
        switch (band.shape) {
        case SevenBandEQShape::Peaking:
            if (band.gain_db != 0.0F) {
                result.sections[0] = make_peak(
                    band.frequency_hz, band.gain_db, band.q, sample_rate);
                result.count = 1;
            }
            break;
        case SevenBandEQShape::LowCut:
            return make_butterworth(band.frequency_hz,
                                    band.slope_db_per_octave, sample_rate, true);
        case SevenBandEQShape::HighCut:
            return make_butterworth(band.frequency_hz,
                                    band.slope_db_per_octave, sample_rate, false);
        }
        return result;
    }

    [[nodiscard]] static float process_section(float input,
                                                const Coefficients& coefficients,
                                                SectionState& state) noexcept {
        const float output = coefficients.b0 * input + state.z1;
        state.z1 = coefficients.b1 * input - coefficients.a1 * output + state.z2;
        state.z2 = coefficients.b2 * input - coefficients.a2 * output;
        return std::isfinite(output) ? output : 0.0F;
    }

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    std::array<SevenBandEQBand, 7> bands_ = default_parameters();
    std::array<BandCoefficients, 7> coefficients_{};
    std::array<std::array<std::array<SectionState, 3>, 7>, 2> states_{};
    bool neutral_ = true;
};

} // namespace tabletfx
