#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class VolumePedalTaper : std::uint8_t {
    // Interpolate directly between the endpoint amplitudes.
    LinearAmplitude,
    // Interpolate endpoint decibels, producing the familiar exponential
    // amplitude response of an audio/logarithmic control.
    AudioTaperDb,
};

struct VolumePedalParameters {
    float position = 1.0F;      // 0 = heel, 1 = toe
    float minimum_db = -96.0F;  // curve floor above the exact heel mute
    float maximum_db = 0.0F;    // toe endpoint; positive gain is bounded to +12 dB
    VolumePedalTaper taper = VolumePedalTaper::AudioTaperDb;
    float smoothing_ms = 10.0F;
};

// Control-rate volume pedal. Parameter setters are intended for the control
// thread; process() applies a precomputed linear-amplitude ramp without heap
// allocation or transcendental functions. This is a generic expression taper,
// not a model of a particular commercial pedal.
class VolumePedal final : public Effect {
public:
    explicit VolumePedal(float sample_rate = 48000.0F,
                         VolumePedalParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "volume-pedal";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    // Prepare all controls off the audio callback; the configured position is
    // applied immediately so a newly loaded preset does not ramp from unity.
    void configure(float sample_rate, VolumePedalParameters parameters) noexcept;
    void set_position(float position) noexcept;
    void set_endpoints_db(float minimum_db, float maximum_db) noexcept;
    void set_taper(VolumePedalTaper taper) noexcept;
    void set_smoothing_ms(float smoothing_ms) noexcept;

    [[nodiscard]] VolumePedalParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float current_gain_linear() const noexcept { return gain_; }

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] float target_for_position(float position) const noexcept;
    void retarget(bool immediate) noexcept;

    float sample_rate_ = 48000.0F;
    VolumePedalParameters parameters_{};
    float gain_ = 1.0F;
    float target_gain_ = 1.0F;
    float ramp_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
};

} // namespace tabletfx
