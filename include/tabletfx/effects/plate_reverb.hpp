#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tabletfx {

struct PlateReverbParameters {
    float decay_seconds = 1.8F;
    float pre_delay_ms = 0.0F;
    float damping_hz = 7500.0F;
    float diffusion = 0.82F;
    // 0 favors the early-reflection component; 1 favors the dense late tail.
    float balance = 0.5F;
    float mix = 0.2F;
    float level_db = 0.0F;
};

// Project-authored plate-style reverb using a short allpass diffuser followed
// by an eight-line Householder feedback-delay network. It is not an emulation
// of any named hardware or commercial effect. Predelay affects only the wet
// path. Storage is allocated at construction; process() performs no allocation.
// Call reset() to clear the full reverb tail while retaining current controls.
class PlateReverb final : public Effect {
public:
    PlateReverb(float sample_rate, std::uint16_t channels,
                PlateReverbParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override { return "plate-reverb"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] PlateReverbParameters parameters() const noexcept { return parameters_; }

    // Configure outside the audio callback; changes are smoothed over 20 ms.
    void configure(PlateReverbParameters parameters) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kFeedbackLineCount = 8;
    static constexpr std::size_t kDiffuserCount = 4;

    struct DelayLine {
        std::size_t offset = 0;
        std::size_t length = 1;
        std::size_t index = 0;
        float damping_state = 0.0F;
        float feedback = 0.0F;
        float feedback_target = 0.0F;
        float feedback_step = 0.0F;
    };

    void update_targets() noexcept;
    void advance_parameters() noexcept;
    [[nodiscard]] float read_predelay(std::uint16_t channel, float input) noexcept;
    [[nodiscard]] float process_diffuser(DelayLine& line, float input,
                                         float diffusion) noexcept;
    void settle_parameters() noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    PlateReverbParameters parameters_{};
    float damping_alpha_ = 0.0F;
    float target_damping_alpha_ = 0.0F;
    float pre_delay_samples_ = 0.0F;
    float target_pre_delay_samples_ = 0.0F;
    float diffusion_ = 0.82F;
    float target_diffusion_ = 0.82F;
    float balance_ = 0.5F;
    float target_balance_ = 0.5F;
    float mix_ = 0.2F;
    float target_mix_ = 0.2F;
    float level_ = 1.0F;
    float target_level_ = 1.0F;
    float damping_step_ = 0.0F;
    float pre_delay_step_ = 0.0F;
    float diffusion_step_ = 0.0F;
    float balance_step_ = 0.0F;
    float mix_step_ = 0.0F;
    float level_step_ = 0.0F;
    std::size_t parameter_ramp_remaining_ = 0;
    std::size_t parameter_ramp_total_ = 1;
    std::vector<float> storage_;
    std::array<std::size_t, 2> predelay_offset_{};
    std::size_t predelay_capacity_ = 1;
    std::array<std::size_t, 2> predelay_index_{};
    std::array<DelayLine, kFeedbackLineCount * 2> feedback_lines_{};
    std::array<DelayLine, kDiffuserCount * 2> diffusers_{};
};

} // namespace tabletfx
