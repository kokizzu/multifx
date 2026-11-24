#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class VibeModel : std::uint8_t { Essex, UniVerse };
enum class VibeRateMode : std::uint8_t { Free = 0, TempoNote = 1 };
enum class VibeToneMode : std::uint8_t { Modern = 0, Vintage = 1 };
enum class VibeModulationMode : std::uint8_t { Chorus = 0, Vibrato = 1 };

// Project-authored note lengths; the Stadium documentation does not publish
// Essex Vibe's selectable division list.
enum class VibeNoteDivision : std::uint8_t {
    Whole = 0,
    DottedHalf,
    Half,
    DottedQuarter,
    Quarter,
    QuarterTriplet,
    DottedEighth,
    Eighth,
    EighthTriplet,
    DottedSixteenth,
    Sixteenth,
    SixteenthTriplet,
};

struct VibeParameters {
    float rate_hz = 1.5F;
    float depth = 0.7F;
    float trem = 0.2F;
    std::uint8_t stages = 2;
    float spread = 0.5F;
    float level = 1.0F;
    VibeRateMode rate_mode = VibeRateMode::Free;
    VibeNoteDivision note_division = VibeNoteDivision::Quarter;
    float tempo_bpm = 120.0F;
    VibeToneMode tone_mode = VibeToneMode::Modern;
    VibeModulationMode modulation_mode = VibeModulationMode::Chorus;
};

// Project-authored Vibe modulation. It uses one or two swept first-order
// all-pass stages, blends the phase-shifted path with dry according to Depth,
// and applies amplitude modulation according to Trem. Spread sets the
// right-channel tremolo phase offset; Level is linear gain. Continuous control
// changes ramp over a project-authored 10 ms. The controls follow the names documented
// for Line 6 Helix Stadium Essex Vibe. The same project processor also exposes
// Fender Tone Master Pro Uni-Verse Vibe's Vintage/Modern and Chorus/Vibrato
// control roles; those switches map to a project one-pole tone blend and a
// wet-only phase path, not Fender's circuit or DSP. Optional tempo-note sync
// follows project-supplied BPM; it does not receive shared tap tempo or MIDI
// clock. Ranges, defaults, topology, and DSP are project choices and do not
// claim Line 6 or Fender parity.
// Fixed-size state is used and process() does not allocate.
class Vibe final : public Effect {
public:
    explicit Vibe(float sample_rate = 48000.0F,
                  std::uint16_t channels = 2,
                  VibeParameters parameters = {},
                  VibeModel model = VibeModel::Essex);

    [[nodiscard]] std::string_view id() const noexcept override {
        return model_ == VibeModel::UniVerse ? "uni-verse-vibe" : "vibe";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void set_parameters(VibeParameters parameters) noexcept;
    [[nodiscard]] VibeParameters parameters() const noexcept { return parameters_; }
    [[nodiscard]] float effective_rate_hz() const noexcept {
        return effective_rate_hz_;
    }

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
    static constexpr float kMinimumTempoRateHz = 0.05F;
    static constexpr float kMaximumTempoRateHz = 30.0F;
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
    VibeModel model_ = VibeModel::Essex;
    std::array<std::array<StageState, 2>, 2> stages_{};
    std::array<float, 2> vintage_filter_state_{};
    float effective_rate_hz_ = 1.5F;
    float vintage_filter_coefficient_ = 0.0F;

    float current_depth_ = 0.7F;
    float current_vintage_mix_ = 0.0F;
    float current_vibrato_mix_ = 0.0F;
    float current_trem_ = 0.2F;
    float current_spread_cosine_ = 0.0F;
    float current_spread_sine_ = 1.0F;
    float current_level_ = 1.0F;
    float current_sine_increment_ = 0.0F;
    float current_cosine_increment_ = 1.0F;

    float target_depth_ = 0.7F;
    float target_vintage_mix_ = 0.0F;
    float target_vibrato_mix_ = 0.0F;
    float target_trem_ = 0.2F;
    float target_spread_cosine_ = 0.0F;
    float target_spread_sine_ = 1.0F;
    float target_level_ = 1.0F;
    float target_sine_increment_ = 0.0F;
    float target_cosine_increment_ = 1.0F;

    float depth_step_ = 0.0F;
    float vintage_mix_step_ = 0.0F;
    float vibrato_mix_step_ = 0.0F;
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
