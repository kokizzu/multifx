#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx::effects {

// Project-authored controls for a conventional amplitude-modulation effect.
// Stadium's current guide describes Rate, Depth, Mix, and Level generically,
// but does not provide an AM Ring Mod-specific parameter table or ranges.
struct AmRingModParameters {
    float rate_hz = 440.0F;  // Project range 0.1–2,000 Hz.
    float depth = 0.5F;      // Project range 0–1.
    float mix = 1.0F;        // Project range 0–1.
    float level_db = 0.0F;   // Project range −24…+12 dB.
};

/*
 * Project-authored amplitude modulation. A free-running cosine changes the
 * input amplitude by the envelope 1 + Depth*cos(phase), so Depth 0 is
 * unmodulated and Depth 1 reaches zero-to-double amplitude. Unlike the
 * project's existing bipolar ring modulator, this preserves a carrier
 * component and produces conventional AM sidebands.
 *
 * Line 6's current Stadium Model List includes mono/stereo AM Ring Mod, but
 * the Stadium manual does not publish this model's specific controls, ranges,
 * default values, oscillator waveform, or DSP. These controls and processing
 * are project choices; this is not a Line 6 algorithm/settings emulation.
 * Mono and stereo share one modulation phase. Processing uses fixed state and
 * performs no allocation or per-sample trigonometric calls.
 */
class AmRingMod final : public tabletfx::Effect {
public:
    explicit AmRingMod(float sample_rate = 48000.0F,
                       std::uint16_t channels = 2,
                       AmRingModParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "am-ring-mod";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    [[nodiscard]] const AmRingModParameters& parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_rate_hz() const noexcept {
        return effective_rate_hz_;
    }
    [[nodiscard]] std::size_t latency_samples() const noexcept { return 0U; }

    void set_parameters(AmRingModParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumRateHz = 0.1F;
    static constexpr float kMaximumRateHz = 2000.0F;
    static constexpr float kMinimumLevelDb = -24.0F;
    static constexpr float kMaximumLevelDb = 12.0F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void prepare_oscillator() noexcept;
    void advance_oscillator() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2U;
    AmRingModParameters parameters_{};
    float effective_rate_hz_ = 440.0F;
    float output_gain_ = 1.0F;
    float phase_sine_ = 0.0F;
    float phase_cosine_ = 1.0F;
    float increment_sine_ = 0.0F;
    float increment_cosine_ = 1.0F;
    std::uint32_t normalization_counter_ = 0U;
};

}  // namespace tabletfx::effects
