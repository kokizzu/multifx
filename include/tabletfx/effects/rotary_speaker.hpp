#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

enum class RotarySpeed : std::uint8_t {
    slow,
    fast,
};

enum class RotaryInputSelect : std::uint8_t {
    stereo,
    left,
    right,
    sum,
};

struct RotarySpeakerParameters {
    // All numeric ranges and defaults below are project-authored. The Fractal
    // guide names Rate/Tempo but gives no numeric rate range or tempo mapping.
    float rate_hz = 6.0F;
    bool tempo_sync = false;
    float tempo_bpm = 120.0F;
    float beats_per_rotation = 1.0F;
    RotarySpeed speed = RotarySpeed::slow;
    float slow_speed_multiplier = 0.12F;

    float drive = 0.0F;
    // High-frequency horn modulation depth. Zero leaves the high band
    // stationary and removes horn motion from the project-authored model.
    float hi_depth = 0.75F;
    float high_level = 1.0F;
    float low_depth = 0.55F;
    // Normalized horn-length control. Its mapping to delay modulation is
    // project-authored; the guide only describes increasing Doppler/chorus.
    float rotor_length = 0.5F;
    // Drum speed as a fraction of horn speed.
    float low_rate_multi = 0.72F;
    float low_time_constant_ms = 800.0F;
    float hi_time_constant_ms = 450.0F;
    // Normalized mic geometry controls; exact Fractal ranges are unspecified.
    float low_mic_spacing = 0.0F;
    float high_mic_spacing = 0.0F;
    float mic_distance = 0.25F;
    // Signed mid/side width control. Project-authored range follows the guide's
    // described -200% to +200% spread concept without claiming its mapping.
    float stereo_spread = 1.0F;
    // Mix, Level, and Balance are normalized/linear project controls.
    float mix = 1.0F;
    float level = 1.0F;
    float balance = 0.0F;
    RotaryInputSelect input_select = RotaryInputSelect::stereo;
    bool bypass = false;
};

// Project-authored rotary-speaker effect. A one-pole crossover feeds an
// independently accelerating drum and horn model. The horn path uses a small,
// construction-time delay ring for Doppler-like motion; Hi Depth controls both
// horn amplitude modulation and Doppler motion, so zero leaves only stationary
// high-band output. Linked mic phases create stereo movement. No
// signal-dependent tail or unbounded feedback state is used. process() performs
// no allocation.
//
// This is not a Fender, Line 6, HeadRush, or Fractal model. The Fractal Blocks
// Guide is a control/reference inventory only; ranges, defaults, crossover,
// mic mapping, rotor simulation, and acceleration math are project-authored.
class RotarySpeaker final : public Effect {
public:
    explicit RotarySpeaker(float sample_rate = 48000.0F,
                           std::uint16_t channels = 2,
                           RotarySpeakerParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "rotary-speaker";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;

    void set_parameters(RotarySpeakerParameters parameters) noexcept;
    [[nodiscard]] RotarySpeakerParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float target_hi_rate_hz() const noexcept {
        return target_hi_rate_hz_;
    }
    [[nodiscard]] float target_low_rate_hz() const noexcept {
        return target_low_rate_hz_;
    }
    [[nodiscard]] float current_hi_rate_hz() const noexcept {
        return current_hi_rate_hz_;
    }
    [[nodiscard]] float current_low_rate_hz() const noexcept {
        return current_low_rate_hz_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    // Restart horn and drum phase only. Rotor motor speeds and audio history
    // remain intact.
    void reset_modulation_phase() noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumRateHz = 0.1F;
    static constexpr float kMaximumRateHz = 10.0F;
    static constexpr float kMinimumTempoBpm = 20.0F;
    static constexpr float kMaximumTempoBpm = 300.0F;
    static constexpr float kMinimumBeatsPerRotation = 0.125F;
    static constexpr float kMaximumBeatsPerRotation = 8.0F;
    static constexpr float kMinimumTimeConstantMs = 10.0F;
    static constexpr float kMaximumTimeConstantMs = 5000.0F;
    static constexpr float kMaximumRotorDelayMs = 2.5F;
    static constexpr float kCrossoverHz = 800.0F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float sine_from_cycles(float cycles) noexcept;
    [[nodiscard]] static float cosine_from_cycles(float cycles) noexcept;
    [[nodiscard]] static RotarySpeed sanitize_speed(RotarySpeed speed) noexcept;
    [[nodiscard]] static RotaryInputSelect sanitize_input_select(
        RotaryInputSelect input_select) noexcept;
    [[nodiscard]] float rate_target_hz() const noexcept;
    void prepare_parameters() noexcept;
    void update_motor() noexcept;
    [[nodiscard]] float process_input(float input) const noexcept;
    [[nodiscard]] float read_rotor_delay(std::uint16_t channel,
                                         float delay_samples) const noexcept;
    [[nodiscard]] float phase_sine_for_channel(float phase,
                                                std::uint16_t channel,
                                                bool high_rotor) const noexcept;
    [[nodiscard]] float phase_cosine_for_channel(float phase,
                                                  std::uint16_t channel,
                                                  bool high_rotor) const noexcept;
    void advance_phase(float& phase, float rate_hz) const noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    RotarySpeakerParameters parameters_{};
    std::size_t delay_capacity_frames_ = 0;
    std::size_t delay_write_frame_ = 0;
    std::vector<float> rotor_delay_buffer_;
    std::array<float, 2> crossover_state_{};
    std::array<float, 2> low_mic_offset_sine_{};
    std::array<float, 2> low_mic_offset_cosine_{};
    std::array<float, 2> high_mic_offset_sine_{};
    std::array<float, 2> high_mic_offset_cosine_{};
    float crossover_alpha_ = 0.0F;
    float drive_gain_ = 1.0F;
    float drive_normalization_ = 0.0F;
    float low_acceleration_coefficient_ = 0.0F;
    float hi_acceleration_coefficient_ = 0.0F;
    float target_low_rate_hz_ = 0.0F;
    float target_hi_rate_hz_ = 0.0F;
    float current_low_rate_hz_ = 0.0F;
    float current_hi_rate_hz_ = 0.0F;
    float low_phase_cycles_ = 0.0F;
    float hi_phase_cycles_ = 0.0F;
};

} // namespace tabletfx
