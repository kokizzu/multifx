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

// Three fully parametric peaking bands. Coefficients are prepared by
// configure(); process() is allocation-free and adds no algorithmic delay.
// Configure/reset the instance while it is not being processed. For live
// changes, prepare a replacement processor/graph off the audio thread.
class ThreeBandParametricEQ final : public Effect {
public:
    explicit ThreeBandParametricEQ(float sample_rate = 48000.0F,
                                   std::uint16_t channels = 2)
        : sample_rate_(sample_rate), channels_(channels) {
        if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
            sample_rate_ > 384000.0F) {
            throw std::invalid_argument("ThreeBandParametricEQ sample rate must be in [8000, 384000] Hz");
        }
        if (channels_ == 0 || channels_ > 2) {
            throw std::invalid_argument("ThreeBandParametricEQ supports mono or stereo");
        }
        configure(120.0F, 0.0F, 0.707F,
                  800.0F, 0.0F, 0.707F,
                  5000.0F, 0.0F, 0.707F);
    }

    [[nodiscard]] std::string_view id() const noexcept override {
        return "three-band-parametric-eq";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }

    // Call outside the audio callback and not concurrently with process().
    // Each frequency is bounded below Nyquist, gains to +/-18 dB, and Q to
    // [0.25, 10]. All three bands are peaking filters with frequency, gain,
    // and Q controls; none is a shelving filter.
    void configure(float band1_frequency_hz, float band1_gain_db, float band1_q,
                   float band2_frequency_hz, float band2_gain_db, float band2_q,
                   float band3_frequency_hz, float band3_gain_db, float band3_q) noexcept {
        constexpr float kMinFrequencyHz = 20.0F;
        constexpr float kMinGainDb = -18.0F;
        constexpr float kMaxGainDb = 18.0F;
        constexpr float kMinQ = 0.25F;
        constexpr float kMaxQ = 10.0F;

        const float max_frequency_hz = std::max(kMinFrequencyHz, sample_rate_ * 0.45F);
        const float default_frequencies[3]{120.0F, 800.0F, 5000.0F};
        const float frequencies[3]{band1_frequency_hz, band2_frequency_hz,
                                   band3_frequency_hz};
        const float gains[3]{band1_gain_db, band2_gain_db, band3_gain_db};
        const float qs[3]{band1_q, band2_q, band3_q};
        neutral_ = true;
        for (std::size_t band = 0; band < bands_.size(); ++band) {
            bands_[band].frequency_hz = bounded(frequencies[band], default_frequencies[band],
                                                kMinFrequencyHz, max_frequency_hz);
            bands_[band].gain_db = bounded(gains[band], 0.0F, kMinGainDb, kMaxGainDb);
            bands_[band].q = bounded(qs[band], 0.707F, kMinQ, kMaxQ);
            coefficients_[band] = make_peak(bands_[band].frequency_hz,
                                            bands_[band].gain_db,
                                            bands_[band].q, sample_rate_);
            neutral_ = neutral_ && bands_[band].gain_db == 0.0F;
        }
        reset();
    }

    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] float band1_frequency_hz() const noexcept { return bands_[0].frequency_hz; }
    [[nodiscard]] float band1_gain_db() const noexcept { return bands_[0].gain_db; }
    [[nodiscard]] float band1_q() const noexcept { return bands_[0].q; }
    [[nodiscard]] float band2_frequency_hz() const noexcept { return bands_[1].frequency_hz; }
    [[nodiscard]] float band2_gain_db() const noexcept { return bands_[1].gain_db; }
    [[nodiscard]] float band2_q() const noexcept { return bands_[1].q; }
    [[nodiscard]] float band3_frequency_hz() const noexcept { return bands_[2].frequency_hz; }
    [[nodiscard]] float band3_gain_db() const noexcept { return bands_[2].gain_db; }
    [[nodiscard]] float band3_q() const noexcept { return bands_[2].q; }

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override {
        if (channels == 0 || channels != channels_ || neutral_) {
            return;
        }

        const std::size_t frames = interleaved.size() / channels_;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                const std::size_t index = frame * channels_ + channel;
                float sample = interleaved[index];
                if (!std::isfinite(sample)) {
                    reset_channel(channel);
                    interleaved[index] = 0.0F;
                    continue;
                }

                ChannelState& state = states_[channel];
                for (std::size_t band = 0; band < coefficients_.size(); ++band) {
                    sample = process_section(sample, coefficients_[band], state.bands[band]);
                }
                interleaved[index] = sample;
            }
        }
    }

    void reset() noexcept override {
        for (ChannelState& state : states_) {
            state = {};
        }
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

    struct ChannelState {
        std::array<SectionState, 3> bands{};
    };

    struct BandSettings {
        float frequency_hz = 0.0F;
        float gain_db = 0.0F;
        float q = 0.707F;
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float lower, float upper) noexcept {
        if (!std::isfinite(value)) {
            value = fallback;
        }
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
        if (gain_db == 0.0F) {
            return {};
        }
        constexpr double kPi = 3.141592653589793238462643383279502884;
        const double omega = 2.0 * kPi * static_cast<double>(frequency_hz) /
                             static_cast<double>(sample_rate);
        const double cosine = std::cos(omega);
        const double amplitude = std::pow(10.0, static_cast<double>(gain_db) / 40.0);
        const double alpha = std::sin(omega) /
                             (2.0 * static_cast<double>(q));
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

    void reset_channel(std::uint16_t channel) noexcept {
        states_[channel] = {};
    }

    float sample_rate_;
    std::uint16_t channels_;
    bool neutral_ = true;
    std::array<BandSettings, 3> bands_{};
    std::array<Coefficients, 3> coefficients_{};
    std::array<ChannelState, 2> states_{};
};

} // namespace tabletfx
