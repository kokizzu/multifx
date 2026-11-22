#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

struct VibeParameters {
    float rate_hz = 1.5F;
    float depth = 0.7F;
    float trem = 0.2F;
    std::uint8_t stages = 2;
    float spread = 0.5F;
    float level = 1.0F;
};

// Project-authored Vibe modulation. It uses one or two swept first-order
// all-pass stages, blends the phase-shifted path with dry according to Depth,
// and applies amplitude modulation according to Trem. Spread sets the
// right-channel tremolo phase offset; Level is linear gain. Continuous control
// changes ramp over a project-authored 10 ms. The controls follow the names documented
// for Line 6 Helix Stadium Essex Vibe, but these ranges, defaults, topology,
// and DSP are project choices and do not claim Line 6 algorithm or sound parity.
// The Line 6 1.4 release notes do not specify numeric bounds/defaults or the
// internal topology. Fixed-size state is used and process() does not allocate.
class Vibe final : public Effect {
public:
    explicit Vibe(float sample_rate = 48000.0F,
                  std::uint16_t channels = 2,
                  VibeParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override { return "vibe"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void set_parameters(VibeParameters parameters) noexcept;
    [[nodiscard]] VibeParameters parameters() const noexcept { return parameters_; }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct StageState {
        float previous_input = 0.0F;
        float previous_output = 0.0F;
    };

    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumRateHz = 0.1F;
    static constexpr float kMaximumRateHz = 10.0F;
    static constexpr float kMaximumInternalSample = 1000000.0F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;
    static constexpr float kControlRampMs = 10.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void retarget(bool immediate) noexcept;
    void advance_lfo() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    VibeParameters parameters_{};
    std::array<std::array<StageState, 2>, 2> stages_{};

    float current_depth_ = 0.7F;
    float current_trem_ = 0.2F;
    float current_spread_cosine_ = 0.0F;
    float current_spread_sine_ = 1.0F;
    float current_level_ = 1.0F;
    float current_sine_increment_ = 0.0F;
    float current_cosine_increment_ = 1.0F;

    float target_depth_ = 0.7F;
    float target_trem_ = 0.2F;
    float target_spread_cosine_ = 0.0F;
    float target_spread_sine_ = 1.0F;
    float target_level_ = 1.0F;
    float target_sine_increment_ = 0.0F;
    float target_cosine_increment_ = 1.0F;

    float depth_step_ = 0.0F;
    float trem_step_ = 0.0F;
    float spread_cosine_step_ = 0.0F;
    float spread_sine_step_ = 0.0F;
    float level_step_ = 0.0F;
    float sine_increment_step_ = 0.0F;
    float cosine_increment_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;

    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    std::uint32_t normalization_counter_ = 0;
};

} // namespace tabletfx
