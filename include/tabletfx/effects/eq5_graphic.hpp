#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

/*
 * Project-authored five-band fixed-center graphic EQ. The band centers match
 * the Tone Master Pro Model Guide v1.1.100 EQ5 Graphic table, but the guide
 * publishes no numeric gain bounds, defaults, filter Q, or DSP topology.
 * Project gain bounds/defaults and the fixed Q below are independent choices;
 * this does not claim Fender algorithm or sound parity.
 *
 * Gain changes rebuild fixed peaking-filter coefficients while preserving
 * filter history, so updating a slider does not clear the processor state.
 * process() uses fixed state, performs no buffering, and adds no fixed latency.
 */
class EQ5Graphic final : public Effect {
public:
    static constexpr std::size_t kBandCount = 5;
    static constexpr float kMinimumGainDb = -12.0F;
    static constexpr float kMaximumGainDb = 12.0F;
    static constexpr std::array<float, kBandCount> kCenterFrequenciesHz{
        80.0F, 240.0F, 750.0F, 2200.0F, 6600.0F,
    };

    explicit EQ5Graphic(float sample_rate = 48000.0F,
                        std::uint16_t channels = 2) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "eq5-graphic";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] float band_frequency_hz(std::size_t band) const noexcept;
    [[nodiscard]] float band_gain_db(std::size_t band) const noexcept;
    [[nodiscard]] float overall_gain_db() const noexcept {
        return overall_gain_db_;
    }

    // Finite values are clamped to ±12 dB; non-finite values select 0 dB.
    // Returns false for an index outside the five fixed bands.
    [[nodiscard]] bool set_band_gain_db(std::size_t band,
                                        float gain_db) noexcept;
    void set_overall_gain_db(float gain_db) noexcept;
    void configure(const std::array<float, kBandCount>& band_gains_db,
                   float overall_gain_db) noexcept;

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

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

    using ChannelState = std::array<SectionState, kBandCount>;

    [[nodiscard]] static float valid_sample_rate(float sample_rate) noexcept;
    [[nodiscard]] static std::uint16_t valid_channels(
        std::uint16_t channels) noexcept;
    [[nodiscard]] static float bounded_gain(float gain_db) noexcept;
    [[nodiscard]] static Coefficients make_peak(float frequency_hz,
                                                float gain_db,
                                                float sample_rate) noexcept;
    [[nodiscard]] static bool process_section(float& sample,
                                              const Coefficients& coefficients,
                                              SectionState& state) noexcept;
    void reset_channel(std::uint16_t channel) noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    std::array<float, kBandCount> gains_db_{};
    std::array<Coefficients, kBandCount> coefficients_{};
    std::array<ChannelState, 2> states_{};
    float overall_gain_db_ = 0.0F;
    float overall_gain_ = 1.0F;
};

} // namespace tabletfx
