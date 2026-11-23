#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/pitch_sequence.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx::effects {

enum class PolySustainOperation : std::uint8_t {
    Normal,
    DryKill,
    MuteAll,
};

struct PolySustainParameters {
    // Project controls. The manufacturer documents the control roles but not
    // public numeric bounds/defaults for Helix Stadium's Poly Sustain.
    float interval_semitones = 0.0F;
    float attack_ms = 222.0F;
    float decay_ms = 2000.0F;
    float fx_level = 1.0F;
    float level_db = 0.0F;
    PolySustainOperation operation = PolySustainOperation::Normal;
};

// Project-authored sample-loop sustain with polyphonic pitch transposition.
// It uses the existing bounded PitchSequence freeze loop and pitch shifter;
// this is not Line 6's spectral/polyphonic algorithm or parameter parity.
class PolySustain final : public tabletfx::Effect {
public:
    explicit PolySustain(float sample_rate = 48000.0F,
                         std::uint16_t channels = 1,
                         PolySustainParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "poly-sustain";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] const PolySustainParameters& parameters() const noexcept {
        return parameters_;
    }
    void configure(const PolySustainParameters& parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kMaximumFrames = 8192U;
    static constexpr float kMinimumOutputMagnitude = 16.0F;

    [[nodiscard]] static float finite_sample(float sample) noexcept;
    void configure_capture_engine(bool running) noexcept;
    void process_in_chunks(std::span<float> interleaved, bool wet_only) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    PitchSequence capture_engine_;
    PolySustainParameters parameters_{};
    std::array<float, kMaximumFrames * 2U> dry_buffer_{};
    float attack_coefficient_ = 1.0F;
    float decay_coefficient_ = 0.0F;
    float output_gain_ = 1.0F;
    float attack_gain_ = 0.0F;
    float tail_gain_ = 0.0F;
    bool active_ = false;
    bool holding_ = false;
};

}  // namespace tabletfx::effects
