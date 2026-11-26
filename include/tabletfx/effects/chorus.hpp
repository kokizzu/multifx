#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

struct ChorusParameters {
    float rate_hz = 0.8F;
    bool tempo_sync = false;
    float tempo_bpm = 120.0F;
    // Project-authored 1–8 voice count; one preserves the legacy signal path.
    std::uint8_t number_of_voices = 1U;
    // Deprecated center/depth inputs retained for older callers.
    float base_delay_ms = 15.0F;
    float depth_ms = 7.0F;
    float mix = 0.3F;
    float smoothing_ms = 10.0F;
    // Project-authored controls using Fractal Audio's documented names/ranges.
    float minimum_delay_ms = 8.0F;
    float delay_range_ms = 14.0F;
    // Project-authored Tone Master Pro inspired controls. The Fender guide
    // names Mono Low Cut/High Cut and Stereo Width but does not publish ranges
    // or defaults. Zero disables either cut; width 1.0 preserves the input.
    float low_cut_hz = 0.0F;
    float high_cut_hz = 0.0F;
    float stereo_width = 1.0F;
    // Project-authored opt-in to adding a shared mix adjustment in percentage
    // points. The base mix remains unchanged; the effective mix is clamped to
    // [0, 1]. The offset is bounded to [-0.5, +0.5] and disabled by default.
    bool global_mix_enabled = false;
    float global_mix_offset = 0.0F;
};

// Low-cost sine-LFO modulated delay chorus. The wet path is interpolated
// between adjacent samples; stereo uses opposite LFO phase for width. Delay
// storage is allocated at construction, while process() is allocation-free.
class Chorus final : public Effect {
public:
    explicit Chorus(float sample_rate = 48000.0F, std::uint16_t channels = 2,
                    ChorusParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override { return "chorus"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    void set_parameters(ChorusParameters parameters) noexcept;
    [[nodiscard]] ChorusParameters parameters() const noexcept { return parameters_; }
    [[nodiscard]] float effective_rate_hz() const noexcept {
        return parameters_.tempo_sync ? parameters_.tempo_bpm / 60.0F
                                      : parameters_.rate_hz;
    }
    [[nodiscard]] float current_mix() const noexcept { return current_mix_; }

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumDelayMs = 0.1F;
    static constexpr float kMaximumMinimumDelayMs = 50.0F;
    static constexpr float kMinimumDelayRangeMs = 1.0F;
    static constexpr float kMaximumDelayRangeMs = 40.0F;
    static constexpr float kMaximumDelayMs = 90.0F;
    static constexpr float kStorageDelayMs = kMaximumDelayMs;
    static constexpr float kMaximumLowCutHz = 1200.0F;
    static constexpr float kMaximumHighCutHz = 20000.0F;
    static constexpr float kMaximumStereoWidth = 2.0F;
    static constexpr float kMinimumGlobalMixOffset = -0.5F;
    static constexpr float kMaximumGlobalMixOffset = 0.5F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void prepare_rate() noexcept;
    void retarget(bool immediate) noexcept;
    void advance_modulator() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;
    ChorusParameters parameters_{};
    float current_base_delay_ms_ = 15.0F;
    float current_depth_ms_ = 7.0F;
    float current_mix_ = 0.3F;
    float target_base_delay_ms_ = 15.0F;
    float target_depth_ms_ = 7.0F;
    float target_mix_ = 0.3F;
    float base_delay_step_ = 0.0F;
    float depth_step_ = 0.0F;
    float mix_step_ = 0.0F;
    float low_cut_alpha_ = 0.0F;
    float target_low_cut_alpha_ = 0.0F;
    float low_cut_alpha_step_ = 0.0F;
    float low_cut_mix_ = 0.0F;
    float target_low_cut_mix_ = 0.0F;
    float low_cut_mix_step_ = 0.0F;
    float high_cut_alpha_ = 0.0F;
    float target_high_cut_alpha_ = 0.0F;
    float high_cut_alpha_step_ = 0.0F;
    float high_cut_mix_ = 0.0F;
    float target_high_cut_mix_ = 0.0F;
    float high_cut_mix_step_ = 0.0F;
    float stereo_width_ = 1.0F;
    float target_stereo_width_ = 1.0F;
    float stereo_width_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
    float sine_increment_ = 0.0F;
    float cosine_increment_ = 1.0F;
    float voice_step_sine_ = 0.0F;
    float voice_step_cosine_ = 1.0F;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    std::uint32_t normalization_counter_ = 0;
    std::array<float, 2U> low_cut_previous_input_{};
    std::array<float, 2U> low_cut_previous_output_{};
    std::array<float, 2U> high_cut_previous_output_{};
};

} // namespace tabletfx
