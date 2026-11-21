#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

struct GrainDelayParameters {
    float time_ms = 350.0F;
    float feedback = 0.25F;
    float mix = 0.25F;
    float grain_size_ms = 80.0F;
    std::uint8_t overlap = 2;
    float pitch_semitones = 0.0F;
    float spray_ms = 0.0F;
};

/*
 * Project-authored granular delay. It captures the input and feedback in a
 * fixed ring, then launches overlapping, windowed grains that read delayed
 * audio at a pitch-controlled rate. Spray randomly offsets each grain's start
 * farther into the past. This is distinct from a fixed multi-tap delay: each
 * grain has its own duration, playback rate, window, and randomized start.
 *
 * Controls, ranges, defaults, windowing, feedback routing, and grain scheduling
 * are project-authored. They are not claimed to match HeadRush Prime controls
 * or DSP; the accessible official Prime documentation confirms the effect name
 * and category but does not expose those controls or ranges. Project controls
 * are Time 20–2000 ms (350), Feedback 0–0.9 (0.25), Mix 0–1 (0.25), Grain Size
 * 10–250 ms (80), Overlap 1–4 (2), Pitch −24 to +24 semitones (0), and Spray
 * 0–100 ms (0). Grain windows use a Hann taper; the steady-state overlap gain
 * normalization is project-authored. Positive pitch shifts may shorten a
 * grain to keep its read head behind the live write head; negative pitch shifts
 * use a larger fixed history buffer sized for the full configured range.
 *
 * Ring storage is allocated during construction. process() performs no dynamic
 * allocation, supports mono/stereo interleaved audio, and preserves state across
 * irregular block boundaries. Delay offset, pitch rate, and length changes
 * affect newly launched grains; active grains retain those values until they
 * finish. Mix and feedback changes apply immediately.
 */
class GrainDelay final : public Effect {
public:
    explicit GrainDelay(float sample_rate = 48000.0F,
                        std::uint16_t channels = 1,
                        GrainDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "grain-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float) +
               window_table_.capacity() * sizeof(float);
    }

    [[nodiscard]] GrainDelayParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] std::size_t capacity_frames() const noexcept {
        return capacity_frames_;
    }
    void configure(GrainDelayParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr std::size_t kMaximumGrains = 4;
    static constexpr std::size_t kWindowTableSize = 4097;
    static constexpr float kMinimumTimeMs = 20.0F;
    static constexpr float kMaximumTimeMs = 2000.0F;
    static constexpr float kMaximumSprayMs = 100.0F;
    static constexpr float kMinimumGrainSizeMs = 10.0F;
    static constexpr float kMaximumGrainSizeMs = 250.0F;
    static constexpr float kMaximumFeedback = 0.9F;
    static constexpr float kMaximumPitchSemitones = 24.0F;
    static constexpr float kMaximumInputMagnitude = 16.0F;
    static constexpr float kMaximumStateMagnitude = 1000000.0F;

    struct Grain {
        float read_position = 0.0F;
        float source_step = 1.0F;
        std::size_t age = 0;
        std::size_t length = 0;
        bool active = false;
    };

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float random_unit(std::uint32_t& state) noexcept;
    [[nodiscard]] static float bounded_sample(double value) noexcept;
    [[nodiscard]] std::size_t samples_for_ms(float milliseconds) const noexcept;
    [[nodiscard]] std::size_t grain_length_for(float source_step) const noexcept;
    [[nodiscard]] float read_ring(float position,
                                  std::uint16_t channel) const noexcept;
    [[nodiscard]] float window_value(std::size_t age,
                                     std::size_t length) const noexcept;
    void launch_grain() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 1;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;
    std::vector<float> window_table_;
    GrainDelayParameters parameters_{};
    std::array<Grain, kMaximumGrains> grains_{};
    std::size_t next_grain_ = 0;
    std::size_t launch_countdown_ = 0;
    std::size_t launch_interval_ = 1;
    std::size_t configured_grain_length_ = 1;
    std::size_t effective_grain_length_ = 1;
    float overlap_normalization_ = 1.0F;
    float delay_samples_ = 1.0F;
    float spray_samples_ = 0.0F;
    float pitch_step_ = 1.0F;
    std::uint32_t random_state_ = 0x6d2b79f5U;
};

} // namespace tabletfx
