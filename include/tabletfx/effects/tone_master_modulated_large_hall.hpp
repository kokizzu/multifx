#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tabletfx {

struct ToneMasterModulatedLargeHallParameters {
    // Project-authored defaults and bounds. Fender documents control roles,
    // but publishes no numeric ranges, defaults, or DSP topology for this model.
    // Chosen project ranges/defaults: Level 0..1 (0.28 wet blend), Decay
    // 0.6..18 s (6.5 s), Dwell 0..2 linear input send (1.0), Diffuse 0..1
    // (0.74), Rate 0.05..4 Hz (0.23 Hz), Depth 0..1 (0.20, mapped to at most
    // +/-2.4 ms), High/Low Cut shelf gains -12..+6 dB (-1.5/-0.5 dB), and
    // Spillover off. None of these values claim Fender control parity.
    // High/Low Cut are implemented as shelving gains at project-chosen fixed
    // pivot frequencies; they are not cutoff-frequency controls.
    float level = 0.28F;
    float decay_seconds = 6.5F;
    float dwell = 1.0F;
    float diffuse = 0.74F;
    float rate_hz = 0.23F;
    float depth = 0.20F;
    float high_cut_gain_db = -1.5F;
    float low_cut_gain_db = -0.5F;
    bool spillover = false;
};

// Independent project-authored Modulated Large Hall effect. Its four-line
// orthogonal feedback-delay network and two input diffusers per channel use
// project-selected delays; the longer tank delays distinguish it from the
// project's Modulated Small Hall. This builds on the documented Large Hall
// concept without reproducing Fender DSP or claiming sound parity. The direct
// path has no algorithmic latency; storage is allocated during construction
// and process() never allocates.
class ToneMasterModulatedLargeHallReverb final : public Effect {
public:
    explicit ToneMasterModulatedLargeHallReverb(
        float sample_rate = 48000.0F, std::uint16_t channels = 2U,
        ToneMasterModulatedLargeHallParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "tone-master-modulated-large-hall";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] ToneMasterModulatedLargeHallParameters parameters() const noexcept {
        return parameters_;
    }

    // Configure outside the audio callback. Numeric values are project choices
    // and are smoothed over 20 ms; no Fender control bounds are implied.
    void configure(ToneMasterModulatedLargeHallParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;
    void reset_modulation_phase() noexcept override;

private:
    static constexpr std::size_t kFeedbackLinesPerChannel = 4U;
    static constexpr std::size_t kDiffusersPerChannel = 2U;
    static constexpr std::size_t kMaximumChannels = 2U;
    static constexpr std::size_t kFeedbackLineCount =
        kFeedbackLinesPerChannel * kMaximumChannels;
    static constexpr std::size_t kDiffuserCount =
        kDiffusersPerChannel * kMaximumChannels;
    static constexpr float kMaximumModulationMs = 2.4F;
    static constexpr float kControlSmoothingMs = 20.0F;
    static constexpr float kInternalLimit = 8.0F;

    struct DelayLine {
        std::size_t offset = 0U;
        std::size_t length = 1U;
        std::size_t index = 0U;
        float nominal_delay_samples = 1.0F;
        float damping_state = 0.0F;
        float feedback = 0.0F;
        float feedback_target = 0.0F;
        float feedback_step = 0.0F;
    };

    struct DiffuserLine {
        std::size_t offset = 0U;
        std::size_t length = 1U;
        std::size_t index = 0U;
    };

    void update_targets() noexcept;
    void advance_parameters() noexcept;
    void process_internal(std::span<float> interleaved,
                          bool bypassed) noexcept;
    [[nodiscard]] float process_channel(float input,
                                        std::uint16_t channel) noexcept;
    [[nodiscard]] float process_diffuser(DiffuserLine& line,
                                         float input) noexcept;
    [[nodiscard]] float read_delay(const DelayLine& line,
                                   float delay_samples) const noexcept;
    [[nodiscard]] float filter_wet(float sample,
                                   std::uint16_t channel) noexcept;
    void clear_tail_state() noexcept;
    void settle_parameters() noexcept;
    [[nodiscard]] static float safe_sample(float sample) noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    ToneMasterModulatedLargeHallParameters parameters_{};
    std::vector<float> storage_;
    std::array<DelayLine, kFeedbackLineCount> feedback_lines_{};
    std::array<DiffuserLine, kDiffuserCount> diffusers_{};
    std::array<float, kMaximumChannels> modulation_phase_{};
    std::array<float, kMaximumChannels> low_shelf_state_{};
    std::array<float, kMaximumChannels> high_shelf_lowpass_state_{};

    float low_shelf_alpha_ = 0.0F;
    float high_shelf_alpha_ = 0.0F;
    float damping_alpha_ = 0.0F;
    float level_ = 0.28F;
    float level_target_ = 0.28F;
    float level_step_ = 0.0F;
    float dwell_ = 1.0F;
    float dwell_target_ = 1.0F;
    float dwell_step_ = 0.0F;
    float diffuse_ = 0.74F;
    float diffuse_target_ = 0.74F;
    float diffuse_step_ = 0.0F;
    float modulation_depth_samples_ = 0.0F;
    float modulation_depth_target_samples_ = 0.0F;
    float modulation_depth_step_ = 0.0F;
    float modulation_phase_increment_ = 0.0F;
    float modulation_phase_increment_target_ = 0.0F;
    float modulation_phase_increment_step_ = 0.0F;
    float low_shelf_gain_ = 1.0F;
    float low_shelf_gain_target_ = 1.0F;
    float low_shelf_gain_step_ = 0.0F;
    float high_shelf_gain_ = 1.0F;
    float high_shelf_gain_target_ = 1.0F;
    float high_shelf_gain_step_ = 0.0F;
    std::size_t parameter_ramp_remaining_ = 0U;
    std::size_t parameter_ramp_total_ = 1U;
    bool bypass_cleared_ = false;
};

} // namespace tabletfx
