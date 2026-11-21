#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

enum class AutoSwellTaper : std::uint8_t {
    Linear,
    Log30A,
    Log20A,
    Log15A,
    Log10A,
    Log5A,
    SwellS,
};

struct AutoSwellParameters {
    float threshold_db = -36.0F;
    float attack_ms = 250.0F;
    float release_ms = 250.0F;
    float hysteresis_db = 6.0F;
    AutoSwellTaper taper = AutoSwellTaper::SwellS;
    float volume = 1.0F;
};

// Project-authored slow-attack volume effect. It detects an onset, ramps a
// shared gain envelope, and rearms after the input falls below the hysteresis
// threshold. Controls and taper labels follow the categories documented for
// Fractal's Volume/Pan Auto-Swell; this is an independent implementation, not
// a model of Fractal's DSP or undocumented parameter ranges.
//
// Configure on the control thread. It precomputes the taper and detector
// coefficients, while process() uses fixed storage and performs no allocation.
class AutoSwell final : public Effect {
public:
    explicit AutoSwell(float sample_rate = 48000.0F,
                       AutoSwellParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "auto-swell";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    [[nodiscard]] AutoSwellParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float current_envelope() const noexcept { return envelope_; }
    [[nodiscard]] float current_gain_linear() const noexcept { return gain_; }
    [[nodiscard]] bool is_swell_active() const noexcept { return triggered_; }

    // Parameter bounds are project-authored operating limits; the manufacturer
    // guide documents the controls but does not publish numeric ranges.
    void configure(float sample_rate, AutoSwellParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kTaperSteps = 256;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static AutoSwellTaper valid_taper(AutoSwellTaper taper) noexcept;
    [[nodiscard]] static float taper_value(AutoSwellTaper taper,
                                           float position) noexcept;
    [[nodiscard]] float lookup_taper(float position) const noexcept;
    void build_taper_table() noexcept;

    float sample_rate_ = 48000.0F;
    AutoSwellParameters parameters_{};
    float threshold_linear_ = 0.0158489F;
    float reset_threshold_linear_ = 0.00794328F;
    float detector_attack_coefficient_ = 0.0F;
    float detector_release_coefficient_ = 0.0F;
    float attack_step_ = 0.0F;
    float release_step_ = 0.0F;
    float envelope_ = 0.0F;
    float phase_ = 0.0F;
    float gain_ = 0.0F;
    bool triggered_ = false;
    std::array<float, kTaperSteps + 1> taper_table_{};
};

} // namespace tabletfx
