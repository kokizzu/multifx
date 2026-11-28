#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

struct EuclideanDelayParameters {
    // These controls are project-authored. The Helix Stadium model catalog
    // names Euclidean Delay, but the accessible Stadium manual does not publish
    // model-specific controls, ranges, or defaults.
    float step_time_ms = 125.0F;
    std::uint8_t steps = 8U;
    std::uint8_t pulses = 3U;
    std::uint8_t rotation = 0U;
    float feedback = 0.25F;
    float mix = 0.25F;
    float level_db = 0.0F;
    float low_cut_hz = 20.0F;
    float high_cut_hz = 20000.0F;
    bool trails = false;
};

/*
 * Project-authored, stereo-capable Euclidean rhythmic delay.
 *
 * A single bounded feedback delay line feeds a Euclidean step gate. The
 * project distributes `pulses` hits over `steps` using a deterministic
 * integer phase rule, rotates that pattern, and repeats the gate every cycle.
 * It does not reproduce Line 6's named Stadium Euclidean Delay: the currently
 * accessible Stadium manual does not expose that model's parameter table,
 * ranges, defaults, or DSP. The project controls and signal path below are
 * independent choices.
 *
 * Project ranges/defaults: 10..2000 ms step time (125 ms), 1..16 steps (8),
 * 0..steps pulses (3), rotation modulo steps (0), feedback 0..0.85 (0.25),
 * linear dry/wet Mix 0..1 (0.25), and output Level -24..+12 dB (0 dB). The
 * delay ring is bounded to 2 MiB including both channels. Higher sample rates
 * can shorten the maximum available step time. One shared pattern clock gates
 * independent mono/stereo feedback lines. A short gate ramp reduces clicks at
 * pattern boundaries. The dry path has zero fixed latency.
 *
 * Allocate and configure before audio processing. `configure()` does not
 * allocate; it is intended to run on the owning audio/control thread. The
 * process callback has bounded work and no locks or allocations.
 */
class EuclideanDelay final : public Effect {
public:
    explicit EuclideanDelay(float sample_rate = 48000.0F,
                            EuclideanDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "euclidean-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }
    [[nodiscard]] std::size_t fixed_latency_frames() const noexcept {
        return 0U;
    }
    [[nodiscard]] std::size_t maximum_delay_frames() const noexcept {
        return capacity_frames_ > 2U ? capacity_frames_ - 2U : 0U;
    }
    [[nodiscard]] float maximum_step_time_ms() const noexcept;
    [[nodiscard]] float effective_step_time_ms() const noexcept {
        return current_delay_frames_ * 1000.0F / sample_rate_;
    }
    [[nodiscard]] EuclideanDelayParameters parameters() const noexcept {
        return parameters_;
    }

    void configure(EuclideanDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumStepTimeMs = 10.0F;
    static constexpr float kMaximumStepTimeMs = 2000.0F;
    static constexpr std::uint8_t kMinimumSteps = 1U;
    static constexpr std::uint8_t kMaximumSteps = 16U;
    static constexpr float kMaximumFeedback = 0.85F;
    static constexpr float kMinimumLevelDb = -24.0F;
    static constexpr float kMaximumLevelDb = 12.0F;
    static constexpr float kMaximumSampleMagnitude = 8.0F;
    static constexpr std::size_t kMaximumStorageBytes = 2U * 1024U * 1024U;
    static constexpr float kParameterRampMs = 5.0F;
    static constexpr float kGateRampMs = 1.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float safe_sample(float sample) noexcept;
    [[nodiscard]] static bool euclidean_hit(std::uint8_t step,
                                            std::uint8_t steps,
                                            std::uint8_t pulses,
                                            std::uint8_t rotation) noexcept;
    [[nodiscard]] std::size_t ring_index_behind(std::size_t index,
                                                std::size_t frames) const noexcept;
    [[nodiscard]] float read_delay(std::size_t channel,
                                   float delay_frames) const noexcept;
    [[nodiscard]] std::size_t control_ramp_frames() const noexcept;
    void restart_pattern() noexcept;
    void start_ramp(float target, float& current, float& step,
                    std::size_t& remaining) noexcept;
    void advance_ramps() noexcept;
    void advance_pattern() noexcept;

    float sample_rate_ = 48000.0F;
    std::size_t capacity_frames_ = 0U;
    std::size_t write_frame_ = 0U;
    std::vector<float> storage_;
    EuclideanDelayParameters parameters_{};

    float current_delay_frames_ = 6000.0F;
    float target_delay_frames_ = 6000.0F;
    float delay_frames_step_ = 0.0F;
    std::size_t delay_ramp_remaining_ = 0U;
    float current_feedback_ = 0.25F;
    float feedback_step_ = 0.0F;
    std::size_t feedback_ramp_remaining_ = 0U;
    float current_mix_ = 0.25F;
    float low_cut_alpha_ = 1.0F;
    float high_cut_alpha_ = 0.0F;
    bool low_cut_enabled_ = false;
    bool high_cut_enabled_ = false;
    std::array<float, 2U> low_cut_previous_input_{};
    std::array<float, 2U> low_cut_previous_output_{};
    std::array<float, 2U> high_cut_state_{};
    float mix_step_ = 0.0F;
    std::size_t mix_ramp_remaining_ = 0U;
    float current_level_gain_ = 1.0F;
    float level_gain_step_ = 0.0F;
    std::size_t level_ramp_remaining_ = 0U;

    float phase_ = 0.0F;
    float current_phase_increment_ = 1.0F / 6000.0F;
    float target_phase_increment_ = 1.0F / 6000.0F;
    float phase_increment_step_ = 0.0F;
    std::size_t phase_increment_ramp_remaining_ = 0U;
    std::uint8_t current_step_ = 0U;
    float gate_gain_ = 0.0F;
    float gate_target_ = 0.0F;
    float gate_step_ = 0.0F;
    std::size_t gate_ramp_remaining_ = 0U;
    bool initialized_ = false;
};

} // namespace tabletfx
