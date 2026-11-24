#pragma once

#include "tabletfx/effect.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace tabletfx {

enum class FiveBandEQShape : std::uint8_t {
    Peaking = 0,
    LowShelf = 1,
    HighShelf = 2,
    LowCut = 3,
    HighCut = 4,
    Peaking2 = 5,
    LowShelf2 = 6,
    HighShelf2 = 7,
};

struct FiveBandEQBand {
    float frequency_hz = 1000.0F;
    float gain_db = 0.0F;
    float q = 0.707F;
    FiveBandEQShape shape = FiveBandEQShape::Peaking;
    float slope_db_per_octave = 12.0F;
};

// Project-authored five-band parametric EQ. Parameters are prepared by
// configure(); process() uses fixed per-channel state and adds no delay.
// Call configure/reset while the instance is not being processed.
class FiveBandParametricEQ final : public Effect {
public:
    explicit FiveBandParametricEQ(float sample_rate = 48000.0F,
                                  std::uint16_t channels = 2)
        : sample_rate_(sample_rate), channels_(channels) {
        if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
            sample_rate_ > 384000.0F || channels_ == 0 || channels_ > 2) {
            throw std::invalid_argument(
                "FiveBandParametricEQ requires 8–384 kHz and mono or stereo");
        }
        configure(default_parameters());
    }

    [[nodiscard]] std::string_view id() const noexcept override {
        return "five-band-parametric-eq";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    [[nodiscard]] std::array<FiveBandEQBand, 5> parameters() const noexcept {
        return bands_;
    }

    // Project audition behavior: 0 disables band solo; 1–5 routes only the
    // selected frequency-region band-pass to the output.
    void set_solo_band(std::uint32_t band) noexcept {
        solo_band_ = band <= 5U ? static_cast<std::uint8_t>(band) : 0U;
        neutral_ = solo_band_ == 0U && is_neutral_configuration();
        reset();
    }

    [[nodiscard]] std::uint8_t solo_band() const noexcept {
        return solo_band_;
    }

    // The five-band layout is project-authored. Its frequency spans use the
    // Fractal family guide as a reference; bounds, shapes, shelf transition,
    // and filter realization remain independent of the manufacturer's DSP.
    void configure(std::array<FiveBandEQBand, 5> bands) noexcept {
        configure_impl(bands, false, false);
    }

    // Shared IIR/filter core for project Global EQ variants whose five bands
    // may each select any supported shape and frequency. The default
    // configure() keeps the original per-band restrictions unchanged.
    void configure_global_eq(std::array<FiveBandEQBand, 5> bands) noexcept {
        configure_impl(bands, true, true);
    }

    [[nodiscard]] float process_sample(float sample,
                                       std::uint16_t channel = 0) noexcept {
        if (channel >= channels_) return sample;
        if (!std::isfinite(sample)) {
            states_[channel] = {};
            return 0.0F;
        }
        if (neutral_) return sample;

        if (solo_band_ != 0U) {
            const std::size_t band = solo_band_ - 1U;
            return process_section(sample, solo_coefficients_[band],
                                   states_[channel][band][0]);
        }
        for (std::size_t band = 0; band < coefficients_.size(); ++band) {
            for (std::size_t section = 0;
                 section < coefficients_[band].count; ++section) {
                sample = process_section(
                    sample, coefficients_[band].sections[section],
                    states_[channel][band][section]);
            }
        }
        return sample;
    }

private:
    void configure_impl(std::array<FiveBandEQBand, 5> bands,
                        bool allow_any_shape,
                        bool use_uniform_frequency_range) noexcept {
        constexpr std::array<float, 5> kMinimumFrequencyHz{
            20.0F, 100.0F, 100.0F, 100.0F, 200.0F};
        constexpr std::array<float, 5> kMaximumFrequencyHz{
            2000.0F, 10000.0F, 10000.0F, 10000.0F, 20000.0F};
        constexpr std::array<float, 5> kDefaultFrequencyHz{
            100.0F, 350.0F, 1000.0F, 3500.0F, 8000.0F};
        constexpr float kMinimumGainDb = -18.0F;
        constexpr float kMaximumGainDb = 18.0F;
        constexpr float kMinimumQ = 0.25F;
        constexpr float kMaximumQ = 10.0F;
        const float nyquist_guard_hz = sample_rate_ * 0.45F;

        bool bands_are_neutral = true;
        for (std::size_t band = 0; band < bands.size(); ++band) {
            const float min_hz = use_uniform_frequency_range
                                     ? 20.0F
                                     : kMinimumFrequencyHz[band];
            const float max_hz = std::min(
                use_uniform_frequency_range ? 20000.0F
                                            : kMaximumFrequencyHz[band],
                nyquist_guard_hz);
            bands_[band].frequency_hz = bounded(
                bands[band].frequency_hz, kDefaultFrequencyHz[band],
                min_hz, max_hz);
            bands_[band].gain_db = bounded(
                bands[band].gain_db, 0.0F, kMinimumGainDb, kMaximumGainDb);
            bands_[band].q = bounded(
                bands[band].q, 0.707F, kMinimumQ, kMaximumQ);
            bands_[band].shape =
                    (allow_any_shape ? known_shape(bands[band].shape)
                                     : supported_shape(band, bands[band].shape))
                                     ? bands[band].shape
                                     : FiveBandEQShape::Peaking;
            const float slope = bounded(
                bands[band].slope_db_per_octave, 12.0F, 6.0F, 36.0F);
            bands_[band].slope_db_per_octave =
                std::round(slope / 6.0F) * 6.0F;
            coefficients_[band] = make_band(bands_[band], sample_rate_);
            solo_coefficients_[band] = make_band_pass(
                bands_[band].frequency_hz, bands_[band].q, sample_rate_);
            bands_are_neutral = bands_are_neutral &&
                                !is_blocking(bands_[band].shape) &&
                                bands_[band].gain_db == 0.0F;
        }
        neutral_ = solo_band_ == 0U && bands_are_neutral;
        reset();
    }

public:

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override {
        if (channels != channels_ || neutral_) return;
        const std::size_t frames = interleaved.size() / channels_;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                const std::size_t index = frame * channels_ + channel;
                interleaved[index] = process_sample(interleaved[index], channel);
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
        std::uint8_t count = 1;
    };

    [[nodiscard]] bool is_neutral_configuration() const noexcept {
        for (const auto& band : bands_) {
            if (is_blocking(band.shape) || band.gain_db != 0.0F) return false;
        }
        return true;
    }

    [[nodiscard]] static constexpr bool is_blocking(
        FiveBandEQShape shape) noexcept {
        return shape == FiveBandEQShape::LowCut ||
               shape == FiveBandEQShape::HighCut;
    }

    [[nodiscard]] static constexpr std::array<FiveBandEQBand, 5>
    default_parameters() noexcept {
        return {{{100.0F, 0.0F, 0.707F},
                 {350.0F, 0.0F, 0.707F},
                 {1000.0F, 0.0F, 0.707F},
                 {3500.0F, 0.0F, 0.707F},
                 {8000.0F, 0.0F, 0.707F}}};
    }

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float lower, float upper) noexcept {
        if (!std::isfinite(value)) value = fallback;
        return std::clamp(value, lower, upper);
    }

    [[nodiscard]] static bool supported_shape(
        std::size_t band, FiveBandEQShape shape) noexcept {
        switch (shape) {
        case FiveBandEQShape::Peaking:
        case FiveBandEQShape::Peaking2:
            return true;
        case FiveBandEQShape::LowShelf:
        case FiveBandEQShape::LowShelf2:
            return band <= 1;
        case FiveBandEQShape::HighShelf:
        case FiveBandEQShape::HighShelf2:
            return band >= 3;
        case FiveBandEQShape::LowCut:
            return band == 0;
        case FiveBandEQShape::HighCut:
            return band == 4;
        }
        return false;
    }

    [[nodiscard]] static bool known_shape(FiveBandEQShape shape) noexcept {
        switch (shape) {
        case FiveBandEQShape::Peaking:
        case FiveBandEQShape::LowShelf:
        case FiveBandEQShape::HighShelf:
        case FiveBandEQShape::LowCut:
        case FiveBandEQShape::HighCut:
        case FiveBandEQShape::Peaking2:
        case FiveBandEQShape::LowShelf2:
        case FiveBandEQShape::HighShelf2:
            return true;
        }
        return false;
    }

    [[nodiscard]] static Coefficients normalize(double b0, double b1, double b2,
                                                 double a0, double a1,
                                                 double a2) noexcept {
        if (!std::isfinite(a0) || std::abs(a0) < std::numeric_limits<double>::min()) {
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

    [[nodiscard]] static Coefficients make_peak(float frequency_hz, float gain_db,
                                                 float q, float sample_rate) noexcept {
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

    [[nodiscard]] static Coefficients make_peak2(float frequency_hz,
                                                  float gain_db, float q,
                                                  float sample_rate) noexcept {
        // The guide describes a different response at low Q, but does not
        // publish its coefficients. This project variant uses a narrower
        // low-Q mapping as an explicitly project-authored choice.
        const float mapped_q = q < 1.0F ? std::sqrt(q) : q;
        return make_peak(frequency_hz, gain_db, mapped_q, sample_rate);
    }

    [[nodiscard]] static Coefficients make_shelf(float frequency_hz,
                                                  float gain_db,
                                                  float sample_rate,
                                                  bool high_shelf) noexcept {
        if (gain_db == 0.0F) return {};
        constexpr double kPi = 3.141592653589793238462643383279502884;
        const double omega = 2.0 * kPi * static_cast<double>(frequency_hz) /
                             static_cast<double>(sample_rate);
        const double cosine = std::cos(omega);
        const double sine = std::sin(omega);
        const double amplitude = std::pow(10.0, static_cast<double>(gain_db) / 40.0);
        constexpr double kShelfSlope = 1.0;
        const double alpha = (sine / 2.0) * std::sqrt(
            (amplitude + 1.0 / amplitude) * (1.0 / kShelfSlope - 1.0) + 2.0);
        const double beta = 2.0 * std::sqrt(amplitude) * alpha;
        if (high_shelf) {
            return normalize(
                amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine + beta),
                -2.0 * amplitude * ((amplitude - 1.0) + (amplitude + 1.0) * cosine),
                amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine - beta),
                (amplitude + 1.0) - (amplitude - 1.0) * cosine + beta,
                2.0 * ((amplitude - 1.0) - (amplitude + 1.0) * cosine),
                (amplitude + 1.0) - (amplitude - 1.0) * cosine - beta);
        }
        return normalize(
            amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine + beta),
            2.0 * amplitude * ((amplitude - 1.0) - (amplitude + 1.0) * cosine),
            amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine - beta),
            (amplitude + 1.0) + (amplitude - 1.0) * cosine + beta,
            -2.0 * ((amplitude - 1.0) + (amplitude + 1.0) * cosine),
            (amplitude + 1.0) + (amplitude - 1.0) * cosine - beta);
    }

    [[nodiscard]] static BandCoefficients make_shelf2(
        float frequency_hz, float gain_db, float q, float sample_rate,
        bool high_shelf) noexcept {
        BandCoefficients result;
        result.sections[0] = make_shelf(
            frequency_hz, gain_db, sample_rate, high_shelf);
        if (gain_db != 0.0F) {
            // A second, gentle peak adds an audible project-authored
            // cutoff overshoot; this is not the manufacturer's filter DSP.
            result.sections[1] = make_peak(
                frequency_hz, gain_db * 0.30F,
                std::clamp(q * 0.90F, 0.5F, 2.0F), sample_rate);
            result.count = 2;
        }
        return result;
    }

    [[nodiscard]] static Coefficients make_band_pass(
        float frequency_hz, float q, float sample_rate) noexcept {
        constexpr double kPi = 3.141592653589793238462643383279502884;
        const double omega = 2.0 * kPi * static_cast<double>(frequency_hz) /
                             static_cast<double>(sample_rate);
        const double cosine = std::cos(omega);
        const double alpha = std::sin(omega) / (2.0 * static_cast<double>(q));
        return normalize(alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * cosine,
                         1.0 - alpha);
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
        const double b2 = b0;
        return normalize(b0, b1, b2, 1.0 + alpha, -2.0 * cosine,
                         1.0 - alpha);
    }

    [[nodiscard]] static BandCoefficients make_butterworth(
        float frequency_hz, float slope_db_per_octave, float sample_rate,
        bool high_pass) noexcept {
        constexpr double kPi = 3.141592653589793238462643383279502884;
        const auto order = static_cast<unsigned>(
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
        const FiveBandEQBand& band, float sample_rate) noexcept {
        BandCoefficients result;
        switch (band.shape) {
        case FiveBandEQShape::Peaking:
            result.sections[0] = make_peak(
                band.frequency_hz, band.gain_db, band.q, sample_rate);
            break;
        case FiveBandEQShape::Peaking2:
            result.sections[0] = make_peak2(
                band.frequency_hz, band.gain_db, band.q, sample_rate);
            break;
        case FiveBandEQShape::LowShelf:
            result.sections[0] = make_shelf(
                band.frequency_hz, band.gain_db, sample_rate, false);
            break;
        case FiveBandEQShape::LowShelf2:
            return make_shelf2(band.frequency_hz, band.gain_db, band.q,
                               sample_rate, false);
        case FiveBandEQShape::HighShelf2:
            return make_shelf2(band.frequency_hz, band.gain_db, band.q,
                               sample_rate, true);
        case FiveBandEQShape::HighShelf:
            result.sections[0] = make_shelf(
                band.frequency_hz, band.gain_db, sample_rate, true);
            break;
        case FiveBandEQShape::LowCut:
            return make_butterworth(band.frequency_hz,
                                    band.slope_db_per_octave,
                                    sample_rate, true);
        case FiveBandEQShape::HighCut:
            return make_butterworth(band.frequency_hz,
                                    band.slope_db_per_octave,
                                    sample_rate, false);
        }
        return result;
    }

    [[nodiscard]] static float process_section(float input,
                                               const Coefficients& coefficients,
                                               SectionState& state) noexcept {
        const float output = coefficients.b0 * input + state.z1;
        const float next_z1 = coefficients.b1 * input - coefficients.a1 * output + state.z2;
        const float next_z2 = coefficients.b2 * input - coefficients.a2 * output;
        if (!std::isfinite(output) || !std::isfinite(next_z1) || !std::isfinite(next_z2)) {
            state = {};
            return 0.0F;
        }
        constexpr float kDenormalThreshold = 1.0e-20F;
        state.z1 = std::abs(next_z1) < kDenormalThreshold ? 0.0F : next_z1;
        state.z2 = std::abs(next_z2) < kDenormalThreshold ? 0.0F : next_z2;
        return output;
    }

    float sample_rate_;
    std::uint16_t channels_;
    bool neutral_ = true;
    std::uint8_t solo_band_ = 0U;
    std::array<FiveBandEQBand, 5> bands_{};
    std::array<BandCoefficients, 5> coefficients_{};
    std::array<Coefficients, 5> solo_coefficients_{};
    std::array<std::array<std::array<SectionState, 3>, 5>, 2> states_{};
};

} // namespace tabletfx
