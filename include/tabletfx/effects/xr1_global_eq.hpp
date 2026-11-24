#pragma once

#include "tabletfx/effects/five_band_eq.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

enum class XR1GlobalEQAssign : std::uint8_t {
    Stereo = 0,
    Left = 1,
    Right = 2,
};

enum class XR1GlobalEQType : std::uint8_t {
    Peak = 0,
    HighPass = 1,
    LowPass = 2,
    HighShelf = 3,
    LowShelf = 4,
};

struct XR1GlobalEQBand {
    XR1GlobalEQAssign assign = XR1GlobalEQAssign::Stereo;
    XR1GlobalEQType type = XR1GlobalEQType::Peak;
    float gain_db = 0.0F;
    float frequency_hz = 1000.0F;
    float q = 0.707F;
    bool bypass = false;
};

struct XR1GlobalEQParameters {
    std::array<XR1GlobalEQBand, 5> bands{{
        {XR1GlobalEQAssign::Stereo, XR1GlobalEQType::Peak, 0.0F, 80.0F, 0.707F, false},
        {XR1GlobalEQAssign::Stereo, XR1GlobalEQType::Peak, 0.0F, 250.0F, 0.707F, false},
        {XR1GlobalEQAssign::Stereo, XR1GlobalEQType::Peak, 0.0F, 750.0F, 0.707F, false},
        {XR1GlobalEQAssign::Stereo, XR1GlobalEQType::Peak, 0.0F, 2200.0F, 0.707F, false},
        {XR1GlobalEQAssign::Stereo, XR1GlobalEQType::Peak, 0.0F, 6600.0F, 0.707F, false},
    }};
    bool bypass = false;
};

/*
 * Project-authored five-band output EQ inspired by the XR1 manual's Global
 * EQ controls. The manual lists Assign, Type, Gain, Freq, Q and Bypass per
 * band, plus master Bypass/Reset, but does not publish numeric ranges,
 * defaults, filter topology, or bus routing details. Project controls use
 * ±12 dB, 20–20,000 Hz (with a 0.45 × sample-rate ceiling), Q 0.25–10, and
 * Stereo/Left/Right assignment. These choices do not claim XR1 DSP or output
 * bus parity. Dry DI routing is outside this processor's scope.
 *
 * Coefficients and fixed IIR state reuse FiveBandParametricEQ. Processing
 * allocates nothing, uses no block buffering, and adds zero fixed latency.
 */
class XR1GlobalEQ final : public Effect {
public:
    static constexpr std::size_t kBandCount = 5;
    static constexpr float kMinimumGainDb = -12.0F;
    static constexpr float kMaximumGainDb = 12.0F;
    static constexpr float kMinimumFrequencyHz = 20.0F;
    static constexpr float kMaximumFrequencyHz = 20000.0F;
    static constexpr float kMinimumQ = 0.25F;
    static constexpr float kMaximumQ = 10.0F;

    explicit XR1GlobalEQ(float sample_rate = 48000.0F,
                         std::uint16_t channels = 2);

    [[nodiscard]] std::string_view id() const noexcept override {
        return "xr1-global-eq-project-authored";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] XR1GlobalEQParameters parameters() const noexcept {
        return parameters_;
    }

    void configure(const XR1GlobalEQParameters& parameters) noexcept;
    void reset_parameters() noexcept;

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    [[nodiscard]] static XR1GlobalEQAssign valid_assign(
        XR1GlobalEQAssign assign) noexcept;
    [[nodiscard]] static XR1GlobalEQType valid_type(
        XR1GlobalEQType type) noexcept;
    [[nodiscard]] FiveBandEQShape filter_shape(XR1GlobalEQType type) const noexcept;
    void configure_filters() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    XR1GlobalEQParameters parameters_{};
    FiveBandParametricEQ left_filter_;
    FiveBandParametricEQ right_filter_;
    bool neutral_ = true;
};

} // namespace tabletfx
