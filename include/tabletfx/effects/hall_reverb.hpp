#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <vector>

namespace tabletfx {

struct HallReverbParameters {
    float decay_seconds = 3.2F;
    float pre_delay_ms = 32.0F;
    float damping_hz = 9000.0F;
    float low_cut_hz = 100.0F;
    float mix = 0.25F;
    float level_db = 0.0F;
    bool spillover = false;
};

// Project-authored, algorithmic Hall reverb. The dry path has no added
// algorithmic latency; pre-delay affects only the wet path. Storage is fixed
// during construction, and process() never allocates. The tail continues to
// evolve when process() is called at zero mix; reset() clears it immediately.
class HallReverb final : public Effect {
public:
    HallReverb(float sample_rate, std::uint16_t channels,
               HallReverbParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override { return "hall-reverb"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] HallReverbParameters parameters() const noexcept { return parameters_; }
    void configure(HallReverbParameters parameters) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
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
    void process_internal(std::span<float> interleaved, bool bypassed) noexcept;
    [[nodiscard]] float read_predelay(std::uint16_t channel, float input) noexcept;
    [[nodiscard]] float process_comb(DelayLine& line, float input) noexcept;
    [[nodiscard]] float process_diffuser(DelayLine& line, float input) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    HallReverbParameters parameters_{};
    float target_decay_seconds_ = 3.2F;
    float highpass_alpha_ = 0.0F;
    float target_highpass_alpha_ = 0.0F;
    float highcut_alpha_ = 0.0F;
    float target_highcut_alpha_ = 0.0F;
    float predelay_samples_ = 0.0F;
    float target_predelay_samples_ = 0.0F;
    float mix_ = 0.25F;
    float target_mix_ = 0.25F;
    float level_ = 1.0F;
    float target_level_ = 1.0F;
    float highpass_step_ = 0.0F;
    float highcut_step_ = 0.0F;
    float predelay_step_ = 0.0F;
    float mix_step_ = 0.0F;
    float level_step_ = 0.0F;
    std::size_t parameter_ramp_remaining_ = 0;
    std::size_t parameter_ramp_total_ = 1;
    std::vector<float> storage_;
    std::size_t predelay_capacity_ = 1;
    std::array<std::size_t, 2> predelay_offset_{};
    std::array<std::size_t, 2> predelay_index_{};
    std::array<float, 2> input_previous_{};
    std::array<float, 2> highpass_previous_{};
    std::array<DelayLine, 12> combs_{};
    std::array<DelayLine, 4> diffusers_{};
    bool bypass_cleared_ = true;
};

} // namespace tabletfx
