#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tabletfx {

struct CloudReverbParameters {
    // Project-authored controls (not Fender ranges): Mix 0..1 (0.25), normalized
    // Tone 0..1 (0.5; wet low-pass from 500 Hz to the sample-rate-aware ceiling),
    // Decay 0.2..20 s (7 s), Diffusion 0..1 (0.8), High Damp 1000..min(20000,
    // 0.45*sample_rate) Hz (9000 Hz), Low Damp 20..1000 Hz (80 Hz), modulation
    // Rate 0.05..8 Hz (0.3 Hz), Depth 0..1 (0.15; maps to a maximum ±4 ms), and
    // Spillover false. These bounds/defaults do not imply Fender algorithm
    // parity or reproduce a commercial reverb circuit.
    float mix = 0.25F;
    float tone = 0.5F;
    float decay_seconds = 7.0F;
    float diffusion = 0.8F;
    float high_damp_hz = 9000.0F;
    float low_damp_hz = 80.0F;
    float modulation_rate_hz = 0.3F;
    float modulation_depth = 0.15F;
    bool spillover = false;
};

// Project-authored stereo modulated feedback-delay reverb. Tone shapes the wet
// output; the damping controls act inside the feedback network. Storage is
// allocated during construction, and process() performs no allocations.
// Spillover preserves this block's wet tail during bypass; it does not implement
// preset or scene transitions and is not Fender Cloud Reverb DSP parity.
class CloudReverb final : public Effect {
public:
    explicit CloudReverb(float sample_rate = 48000.0F,
                         CloudReverbParameters parameters = {});
    CloudReverb(float sample_rate, std::uint16_t channels,
                CloudReverbParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "cloud-reverb";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] CloudReverbParameters parameters() const noexcept {
        return parameters_;
    }
    void configure(CloudReverbParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kLinesPerChannel = 8;
    static constexpr std::size_t kChannelCount = 2;
    static constexpr std::size_t kLineCount = kLinesPerChannel * kChannelCount;
    static constexpr std::size_t kDiffuserCount = 2;
    static constexpr float kMaximumModulationDepthMs = 4.0F;
    static constexpr float kControlSmoothingMs = 20.0F;

    struct DelayLine {
        std::size_t offset = 0;
        std::size_t length = 1;
        std::size_t index = 0;
        float nominal_delay_samples = 1.0F;
        float high_damp_state = 0.0F;
        float low_damp_previous_input = 0.0F;
        float low_damp_previous_output = 0.0F;
        float feedback = 0.0F;
        float feedback_target = 0.0F;
        float feedback_step = 0.0F;
    };

    struct DiffuserLine {
        std::size_t offset = 0;
        std::size_t length = 1;
        std::size_t index = 0;
        float coefficient = 0.7F;
    };

    void update_targets() noexcept;
    void advance_parameters() noexcept;
    void settle_parameters() noexcept;
    void clear_tail_state() noexcept;
    [[nodiscard]] float process_diffuser(DiffuserLine& line,
                                         float input) noexcept;
    [[nodiscard]] float read_delay(const DelayLine& line,
                                   float delay_samples) const noexcept;
    [[nodiscard]] std::array<float, 2> process_frame(float left,
                                                    float right) noexcept;

    float sample_rate_;
    std::uint16_t channels_ = 2;
    CloudReverbParameters parameters_{};
    std::vector<float> storage_;
    std::array<DelayLine, kLineCount> lines_{};
    std::array<DiffuserLine, kDiffuserCount * kChannelCount> diffusers_{};
    std::array<float, kChannelCount> tone_state_{};
    float mix_ = 0.25F;
    float mix_step_ = 0.0F;
    float tone_alpha_ = 0.0F;
    float target_tone_alpha_ = 0.0F;
    float tone_alpha_step_ = 0.0F;
    float diffusion_ = 0.8F;
    float diffusion_step_ = 0.0F;
    float high_damp_alpha_ = 0.0F;
    float target_high_damp_alpha_ = 0.0F;
    float high_damp_alpha_step_ = 0.0F;
    float low_damp_alpha_ = 0.0F;
    float target_low_damp_alpha_ = 0.0F;
    float low_damp_alpha_step_ = 0.0F;
    float modulation_depth_samples_ = 0.0F;
    float modulation_depth_step_ = 0.0F;
    float modulation_phase_increment_ = 0.0F;
    float modulation_phase_increment_step_ = 0.0F;
    float modulation_phase_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
    std::size_t ramp_total_ = 1;
    bool bypass_cleared_ = false;
};

} // namespace tabletfx
