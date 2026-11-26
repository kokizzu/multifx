#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/cloud_reverb.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tabletfx {

struct ToneMasterSmallHallParameters {
    // Project-authored values because Fender's Model Guide does not publish
    // numeric bounds or defaults for this model.
    float level = 0.25F;
    float decay_seconds = 3.2F;
    float dwell_db = 0.0F;
    float diffuse = 0.6F;
    float rate_hz = 0.3F;
    float depth = 0.15F;
    float high_cut_gain_db = 0.0F;
    float low_cut_gain_db = 0.0F;
    bool spillover = false;
};

// Project-authored, low-cost modulated hall reverb. It uses CloudReverb's
// fixed feedback-delay network as an engine, then applies project-chosen
// Dwell and post-reverb shelf controls. It is not Fender DSP or sound parity.
// The direct path is immediate; all storage is reserved at construction and
// process() performs no allocations.
class ToneMasterSmallHallReverb final : public Effect {
public:
    explicit ToneMasterSmallHallReverb(
        float sample_rate = 48000.0F, std::uint16_t channels = 2,
        ToneMasterSmallHallParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "tone-master-small-hall";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] ToneMasterSmallHallParameters parameters() const noexcept {
        return parameters_;
    }

    void configure(ToneMasterSmallHallParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kMaximumChunkFrames = 128U;
    static constexpr float kControlSmoothingMs = 20.0F;
    static constexpr float kLowShelfFrequencyHz = 250.0F;
    static constexpr float kHighShelfFrequencyHz = 6000.0F;

    void advance_input_controls() noexcept;
    void advance_output_controls() noexcept;
    [[nodiscard]] float filter_wet(float sample,
                                   std::uint16_t channel) noexcept;
    [[nodiscard]] static float safe_sample(float sample) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    ToneMasterSmallHallParameters parameters_{};
    CloudReverb reverb_;
    std::array<float, kMaximumChunkFrames * 2U> dry_buffer_{};
    std::array<float, 2U> low_shelf_state_{};
    std::array<float, 2U> high_shelf_lowpass_state_{};
    float low_shelf_alpha_ = 0.0F;
    float high_shelf_alpha_ = 0.0F;

    float dwell_gain_ = 1.0F;
    float dwell_gain_target_ = 1.0F;
    float dwell_gain_step_ = 0.0F;
    std::size_t input_ramp_remaining_ = 0U;
    std::size_t input_ramp_total_ = 1U;

    float level_ = 0.25F;
    float level_target_ = 0.25F;
    float level_step_ = 0.0F;
    float low_shelf_gain_ = 1.0F;
    float low_shelf_gain_target_ = 1.0F;
    float low_shelf_gain_step_ = 0.0F;
    float high_shelf_gain_ = 1.0F;
    float high_shelf_gain_target_ = 1.0F;
    float high_shelf_gain_step_ = 0.0F;
    std::size_t output_ramp_remaining_ = 0U;
    std::size_t output_ramp_total_ = 1U;
};

} // namespace tabletfx
