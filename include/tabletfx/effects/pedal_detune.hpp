#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/poly_detune.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class PedalDetuneMode : std::uint8_t {
    Shallow,
    Deep,
};

struct PedalDetuneParameters {
    PedalDetuneMode mode = PedalDetuneMode::Shallow;
    float position = 0.5F;
};

/*
 * Project-authored interpretation of the Fender Tone Master Pro Pedal Detune
 * controls. Its Model Guide v1.1.100 / Rev. A (p. 68) documents a SHALLOW/DEEP
 * mode switch and POSITION blend, but no numeric ranges, defaults, or DSP
 * topology. This processor maps those controls to the existing PolyDetune
 * micro-delay core; it is not Fender DSP or sound parity.
 *
 * Position is a smoothed dry/wet blend. Shallow and Deep choose project
 * modulation depths of 0.35 ms and 1.4 ms respectively; rate is 0.25 Hz,
 * with two voices and 0.75 width. The dry path is immediate and has no added
 * fixed latency. The wet micro-delay is approximately 5 ms. The inner core
 * allocates one short ring buffer at construction, and processing allocates
 * nothing.
 */
class PedalDetune final : public Effect {
public:
    explicit PedalDetune(float sample_rate = 48000.0F,
                         std::uint16_t channels = 2,
                         PedalDetuneParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "pedal-detune";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;

    void set_parameters(PedalDetuneParameters parameters) noexcept;
    void set_mode(PedalDetuneMode mode) noexcept;
    void set_position(float position) noexcept;

    [[nodiscard]] PedalDetuneParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float current_position() const noexcept {
        return core_.current_mix();
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    [[nodiscard]] static PedalDetuneMode sanitize_mode(
        PedalDetuneMode mode) noexcept;
    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static PolyDetuneParameters core_parameters(
        PedalDetuneParameters parameters) noexcept;

    PedalDetuneParameters parameters_{};
    PolyDetune core_;
};

} // namespace tabletfx
