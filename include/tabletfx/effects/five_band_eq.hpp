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

struct FiveBandEQBand {
    float frequency_hz = 1000.0F;
    float gain_db = 0.0F;
    float q = 0.707F;
};

// Project-authored five-band peaking EQ. Parameters are prepared by
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

    // These project bounds use the Fractal family guide's useful per-band
    // frequency spans as a reference, while keeping this implementation
    // peaking-only and using the project's existing +/-18 dB and Q bounds.
    void configure(std::array<FiveBandEQBand, 5> bands) noexcept {
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

        neutral_ = true;
        for (std::size_t band = 0; band < bands.size(); ++band) {
            const float max_hz = std::min(kMaximumFrequencyHz[band], nyquist_guard_hz);
            bands_[band].frequency_hz = bounded(
                bands[band].frequency_hz, kDefaultFrequencyHz[band],
                kMinimumFrequencyHz[band], max_hz);
            bands_[band].gain_db = bounded(
                bands[band].gain_db, 0.0F, kMinimumGainDb, kMaximumGainDb);
            bands_[band].q = bounded(
                bands[band].q, 0.707F, kMinimumQ, kMaximumQ);
            coefficients_[band] = make_peak(
                bands_[band].frequency_hz, bands_[band].gain_db,
                bands_[band].q, sample_rate_);
            neutral_ = neutral_ && bands_[band].gain_db == 0.0F;
        }
        reset();
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override {
        if (channels != channels_ || neutral_) return;
        const std::size_t frames = interleaved.size() / channels_;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                const std::size_t index = frame * channels_ + channel;
                float sample = interleaved[index];
                if (!std::isfinite(sample)) {
                    states_[channel] = {};
                    interleaved[index] = 0.0F;
                    continue;
                }
                for (std::size_t band = 0; band < coefficients_.size(); ++band) {
                    sample = process_section(
                        sample, coefficients_[band], states_[channel][band]);
                }
                interleaved[index] = sample;
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
    std::array<FiveBandEQBand, 5> bands_{};
    std::array<Coefficients, 5> coefficients_{};
    std::array<std::array<SectionState, 5>, 2> states_{};
};

} // namespace tabletfx
