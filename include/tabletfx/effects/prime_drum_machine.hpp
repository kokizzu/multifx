#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx::effects {

// Project-authored patterns; these names do not claim to match Prime's factory
// style names or its 134 documented styles.
enum class DrumMachineStyle : std::uint8_t {
    rock = 0,
    pop = 1,
    shuffle = 2,
    half_time = 3,
};

// Prime's v5.0 user guide and current support page identify these controls,
// but do not publish editable numeric ranges/defaults. All numeric values and
// defaults here are therefore TabletFX choices, not copied Prime settings.
// Evidence checked: Prime User Guide v5.0.0 §4.11, cumulative v5.0.0 release
// notes in Firmware Update Instructions v5.2.0 (Oct 2026), and the current
// Drum Machine support page:
// https://cdn.inmusicbrands.com/Software/SGEE/5/HeadRush%20Prime%20-%20User%20Guide%20-%20v5.0.0.pdf
// https://cdn.inmusicbrands.com/Software/SGEE/52/READ%20ME%20-%20HeadRush%20Prime%20Firmware%20Update%20Instructions%20v5.2.0.pdf
// https://support.headrushfx.com/en/support/solutions/articles/69000875010-headrush-fx-drum-machine-setup-and-troubleshooting
struct DrumMachineParameters {
    double tempo_bpm = 120.0;        // Project range: 40..300 BPM.
    // Project scale for the generated drum layer relative to the input.
    float mix = 0.25F;               // Project range: 0..1.
    // The Prime guide's main screen has a separate kit Volume control. 1.0 is
    // this project's unity-volume default, not a documented Prime default.
    float volume = 1.0F;              // Project range: 0..1.
    float intensity = 0.65F;         // Project range: 0..1.
    float groove = 0.0F;             // Project range: 0..1 swing amount.
    // These per-voice balances are project-only controls; Prime's reviewed
    // documentation does not list them as Drum Machine parameters.
    float kick_mix = 0.75F;          // Project range: 0..1.
    float snare_mix = 0.65F;         // Project range: 0..1.
    float cymbal_mix = 0.35F;        // Project range: 0..1.
    float tom_mix = 0.30F;           // Project range: 0..1.
    float eq_db = 0.0F;              // Project range: -12..+12 dB high shelf.
    float timbre = 0.5F;             // Project range: 0..1.
    float tuning_semitones = 0.0F;   // Project range: -12..+12 semitones.
    float reverb = 0.12F;            // Project range: 0..1.
    float drive = 0.0F;              // Project range: 0..1.
    // Project range: 1..4-bar phrase; a light tom turnaround marks its end.
    std::uint8_t length_bars = 1U;
    DrumMachineStyle style = DrumMachineStyle::rock;
};

// A small, deterministic synthesized rhythm generator for Loop & performance.
// It adds kick/snare/hat/tom/crash voices to the input without allocation.
// It is not a sampler and does not reproduce Prime's BFD assets, kits, styles,
// or pattern behavior.
class DrumMachine final : public tabletfx::Effect {
public:
    explicit DrumMachine(float sample_rate = 48000.0F,
                         std::uint16_t channels = 2U,
                         DrumMachineParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "drum-machine";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    [[nodiscard]] const DrumMachineParameters& parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] std::uint8_t current_step() const noexcept {
        return current_step_;
    }
    [[nodiscard]] std::uint8_t current_bar() const noexcept {
        return current_bar_;
    }
    [[nodiscard]] bool running() const noexcept { return running_; }
    [[nodiscard]] bool muted() const noexcept { return muted_; }
    [[nodiscard]] std::size_t reverb_delay_frames(std::size_t line) const noexcept {
        return line < reverb_delays_.size() ? reverb_delays_[line] : 0U;
    }

    void configure(DrumMachineParameters parameters) noexcept;
    void set_running(bool running) noexcept { running_ = running; }
    void set_muted(bool muted) noexcept { muted_ = muted; }
    void set_variation(bool variation) noexcept { variation_ = variation; }
    void trigger_accent() noexcept { accent_pending_ = true; }
    void trigger_fill() noexcept { fill_pending_ = true; }
    void reset() noexcept override;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    // Four fixed delay lines sized for the longest 29.9 ms project-authored
    // reverb tap at any supported rate, including 96 kHz. 3072 frames cover
    // the 2871-frame maximum with bounded fixed storage.
    static constexpr float kMaximumSampleRate = 96000.0F;
    static constexpr float kMaximumGeneratedSample = 8.0F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;
    static constexpr std::size_t kReverbLineLength = 3072U;
    static constexpr std::size_t kStepCount = 16U;

    struct Voice {
        float envelope = 0.0F;
        float amplitude = 0.0F;
        float phase = 0.0F;
        float phase_increment = 0.0F;
        float previous_noise = 0.0F;
        float highpass_state = 0.0F;
        std::uint32_t random_state = 1U;
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static double bounded(double value, double fallback,
                                        double minimum, double maximum) noexcept;
    [[nodiscard]] static bool valid_style(DrumMachineStyle style) noexcept;
    [[nodiscard]] float next_noise(Voice& voice) noexcept;
    [[nodiscard]] float process_kick() noexcept;
    [[nodiscard]] float process_snare() noexcept;
    [[nodiscard]] float process_hat() noexcept;
    [[nodiscard]] float process_tom() noexcept;
    [[nodiscard]] float process_crash() noexcept;
    void trigger_step() noexcept;
    void trigger_voice(Voice& voice, float amplitude) noexcept;
    void trigger_swing_hat(float amplitude) noexcept;
    void reverb_sample(float input, float& left, float& right) noexcept;
    [[nodiscard]] float saturate(float sample) const noexcept;
    [[nodiscard]] static float finite_input(float sample) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    DrumMachineParameters parameters_{};
    double step_phase_increment_ = 0.0;
    double step_phase_ = 0.0;
    float tuning_ratio_ = 1.0F;
    float high_shelf_gain_ = 1.0F;
    float tone_coefficient_ = 0.1F;
    float tone_state_[2]{};
    float decay_kick_ = 0.999F;
    float decay_snare_ = 0.999F;
    float decay_hat_ = 0.999F;
    float decay_tom_ = 0.999F;
    float decay_crash_ = 0.999F;
    Voice kick_{};
    Voice snare_{};
    Voice hat_{};
    Voice tom_{};
    Voice crash_{};
    std::array<std::array<float, kReverbLineLength>, 4U> reverb_lines_{};
    std::array<std::size_t, 4U> reverb_delays_{};
    std::array<float, 4U> reverb_feedback_{0.46F, 0.48F, 0.50F, 0.52F};
    std::size_t reverb_write_ = 0U;
    std::uint32_t swing_hat_delay_frames_ = 0U;
    float swing_hat_amplitude_ = 0.0F;
    std::uint8_t current_step_ = 0U;
    std::uint8_t current_bar_ = 0U;
    // Starting a new accompaniment is an explicit transport action; enabling
    // the effect must not unexpectedly add drums to playback.
    bool running_ = false;
    bool muted_ = false;
    bool variation_ = false;
    bool started_ = false;
    bool accent_pending_ = false;
    bool fill_pending_ = false;
};

}  // namespace tabletfx::effects
