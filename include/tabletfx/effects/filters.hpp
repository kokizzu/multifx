#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tabletfx {
namespace detail {

class ButterworthBiquad {
public:
    enum class Response : std::uint8_t { HighPass, LowPass };

    ButterworthBiquad(float sample_rate, std::uint16_t channels,
                      float default_cutoff_hz, Response response);

    void configure(float cutoff_hz) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept;
    void reset() noexcept;

    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] float cutoff_hz() const noexcept { return cutoff_hz_; }
private:
    struct Coefficients {
        float b0 = 1.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    struct State {
        float z1 = 0.0F;
        float z2 = 0.0F;
    };

    [[nodiscard]] static float process_sample(float input,
                                              const Coefficients& coefficients,
                                              State& state) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    float default_cutoff_hz_;
    float cutoff_hz_;
    Response response_;
    Coefficients coefficients_;
    std::array<State, 2> states_{};
};

} // namespace detail

// First-order-free, second-order Butterworth high-pass; cutoff is the -3 dB
// frequency. Processing uses fixed per-channel state and adds no block delay.
class HighPassFilter final : public Effect {
public:
    explicit HighPassFilter(float sample_rate = 48000.0F,
                            std::uint16_t channels = 2,
                            float cutoff_hz = 80.0F);

    [[nodiscard]] std::string_view id() const noexcept override { return "high-pass"; }
    void configure(float cutoff_hz) noexcept { filter_.configure(cutoff_hz); }
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override {
        filter_.process(interleaved, channels);
    }
    void reset() noexcept override { filter_.reset(); }

    [[nodiscard]] float sample_rate() const noexcept { return filter_.sample_rate(); }
    [[nodiscard]] std::uint16_t channels() const noexcept { return filter_.channels(); }
    [[nodiscard]] float cutoff_hz() const noexcept { return filter_.cutoff_hz(); }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }

private:
    detail::ButterworthBiquad filter_;
};

// Second-order Butterworth low-pass; cutoff is the -3 dB frequency.
class LowPassFilter final : public Effect {
public:
    explicit LowPassFilter(float sample_rate = 48000.0F,
                           std::uint16_t channels = 2,
                           float cutoff_hz = 12000.0F);

    [[nodiscard]] std::string_view id() const noexcept override { return "low-pass"; }
    void configure(float cutoff_hz) noexcept { filter_.configure(cutoff_hz); }
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override {
        filter_.process(interleaved, channels);
    }
    void reset() noexcept override { filter_.reset(); }

    [[nodiscard]] float sample_rate() const noexcept { return filter_.sample_rate(); }
    [[nodiscard]] std::uint16_t channels() const noexcept { return filter_.channels(); }
    [[nodiscard]] float cutoff_hz() const noexcept { return filter_.cutoff_hz(); }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }

private:
    detail::ButterworthBiquad filter_;
};

} // namespace tabletfx
