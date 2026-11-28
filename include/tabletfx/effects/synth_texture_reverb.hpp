#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tabletfx::effects {

// Controls and bounds for this project's synthetic-texture reverb. Its fixed
// oscillator palette is driven by an amplitude envelope; it does not detect or
// follow the pitch of the input. These values are not copied from a commercial
// product and do not imply parity with any named pedal algorithm.
struct SynthTextureReverbParameters {
    float mix = 0.25F;              // Dry/wet blend, [0, 1].
    float texture = 0.45F;          // Fixed-spectrum synthetic excitation, [0, 1].
    float decay_seconds = 4.5F;     // Feedback decay target, [0.2, 12] s.
    float damping_hz = 7200.0F;     // Wet-network low-pass, [500, Nyquist-aware ceiling].
    float level_db = 0.0F;          // Wet output level, [-24, 12] dB.
    float envelope_release_ms = 240.0F; // Texture excitation release, [20, 2000] ms.
    bool spillover = false;
};

// Project-authored amplitude-excited synthetic texture followed by a compact
// stereo feedback-delay reverb. Its oscillator frequencies are fixed and
// independent of note pitch; it makes no input-note tracking or polyphony
// claim. The dry path has zero algorithmic latency. Delay memory is allocated
// at construction, and process() performs no allocation or locking.
class SynthTextureReverb final : public tabletfx::Effect {
public:
    explicit SynthTextureReverb(float sample_rate = 48000.0F,
                                std::uint16_t channels = 2U,
                                SynthTextureReverbParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "synth-texture-reverb";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] std::size_t fixed_latency_samples() const noexcept { return 0U; }
    [[nodiscard]] SynthTextureReverbParameters parameters() const noexcept {
        return parameters_;
    }

    void configure(SynthTextureReverbParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kLinesPerChannel = 4U;
    static constexpr std::size_t kMaximumChannels = 2U;
    static constexpr std::size_t kMaximumLines =
        kLinesPerChannel * kMaximumChannels;

    struct DelayLine {
        std::size_t offset = 0U;
        std::size_t length = 1U;
        std::size_t index = 0U;
        float damping_state = 0.0F;
        float feedback = 0.0F;
        float feedback_target = 0.0F;
        float feedback_step = 0.0F;
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] float time_coefficient(float milliseconds) const noexcept;
    void advance_parameters() noexcept;
    void settle_parameters() noexcept;
    void process_internal(std::span<float> interleaved,
                          bool bypassed) noexcept;
    [[nodiscard]] float texture_sample() noexcept;
    [[nodiscard]] float process_channel(std::size_t channel,
                                        float excitation) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    SynthTextureReverbParameters parameters_{};
    std::vector<float> storage_;
    std::array<DelayLine, kMaximumLines> lines_{};
    std::array<double, 4U> oscillator_phase_{};
    std::array<double, 4U> oscillator_increment_{};
    float input_envelope_ = 0.0F;
    float attack_coefficient_ = 0.0F;
    float release_coefficient_ = 0.0F;
    float texture_ = 0.45F;
    float texture_step_ = 0.0F;
    float mix_ = 0.25F;
    float mix_step_ = 0.0F;
    float damping_alpha_ = 0.0F;
    float target_damping_alpha_ = 0.0F;
    float damping_alpha_step_ = 0.0F;
    float level_ = 1.0F;
    float target_level_ = 1.0F;
    float level_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0U;
    std::size_t ramp_total_ = 1U;
    bool bypass_cleared_ = true;
};

}  // namespace tabletfx::effects
