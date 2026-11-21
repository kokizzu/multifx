#pragma once

#include "tabletfx/effects.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tabletfx {

struct SpringReverbParameters {
    float decay_seconds = 2.4F;
    float pre_delay_ms = 0.0F;
    float damping_hz = 8000.0F;
    float dwell = 0.35F;
    float mix = 0.2F;
    float level_db = 0.0F;
};

// Project-authored spring-style reverb using dispersive feedback delay lines.
// Predelay applies only to the wet path, and process() uses storage allocated
// at construction. Dwell controls tank excitation and nonlinear drive; it is
// not a model of any particular amplifier or spring unit.
class SpringReverb final : public Effect {
public:
    SpringReverb(float sample_rate, std::uint16_t channels,
                 SpringReverbParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "spring-reverb";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] SpringReverbParameters parameters() const noexcept {
        return parameters_;
    }
    void configure(SpringReverbParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct DelayLine {
        std::size_t offset = 0;
        std::size_t length = 1;
        std::size_t index = 0;
    };

    struct SpringLine : DelayLine {
        float damping_state = 0.0F;
        float feedback = 0.0F;
        float feedback_target = 0.0F;
        float feedback_step = 0.0F;
        std::array<float, 2> dispersion_state{};
    };

    void update_targets() noexcept;
    void advance_parameters() noexcept;
    [[nodiscard]] float read_predelay(std::uint16_t channel, float input) noexcept;
    [[nodiscard]] float process_diffuser(DelayLine& line, float input) noexcept;
    [[nodiscard]] float process_spring_line(SpringLine& line, float input,
                                            float dispersion_a,
                                            float dispersion_b) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    SpringReverbParameters parameters_;
    std::vector<float> storage_;
    std::array<SpringLine, 6> springs_{};
    std::array<DelayLine, 4> diffusers_{};
    std::array<std::size_t, 2> predelay_offset_{};
    std::array<std::size_t, 2> predelay_index_{};
    std::size_t predelay_capacity_ = 1;
    float damping_alpha_ = 0.0F;
    float target_damping_alpha_ = 0.0F;
    float damping_step_ = 0.0F;
    float pre_delay_samples_ = 0.0F;
    float target_pre_delay_samples_ = 0.0F;
    float predelay_step_ = 0.0F;
    float dwell_drive_ = 1.0F;
    float target_dwell_drive_ = 1.0F;
    float dwell_step_ = 0.0F;
    float wet_mix_ = 0.2F;
    float target_wet_mix_ = 0.2F;
    float mix_step_ = 0.0F;
    float wet_level_ = 1.0F;
    float target_wet_level_ = 1.0F;
    float level_step_ = 0.0F;
    std::size_t parameter_ramp_total_ = 1;
    std::size_t parameter_ramp_remaining_ = 0;
};

} // namespace tabletfx
