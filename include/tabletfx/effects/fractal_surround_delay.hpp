#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

struct FractalSurroundDelayParameters {
    // The FM9-family Blocks Guide (November 2026, printed pp. 31-34) documents
    // Time/Tempo, Master Time, Master Feedback, stereo L/R Time Ratio, stereo
    // Spread/Width, independent Feedback L/R, and Rotation for Surround Delay.
    // Where the guide does not state bounds/defaults/transfer curves, the
    // values below are project-authored. In particular, Rotation's range and
    // angle mapping are ours; this processor does not claim Fractal DSP,
    // settings, surround-field, or sound parity.
    float time_ms = 375.0F;
    float master_time_percent = 100.0F;
    float right_time_ratio_percent = 100.0F;
    float feedback_left_percent = 35.0F;
    float feedback_right_percent = 35.0F;
    float master_feedback_percent = 100.0F;
    float rotation_percent = 50.0F;
    float width_percent = 100.0F;
    float mix_percent = 25.0F;
    float level_db = 0.0F;
};

/*
 * Project-authored stereo rotating delay, based on the Fractal FM9-family
 * Surround Delay control concept. The current official Blocks Guide is the
 * November 2026, 114-page edition linked from the FM9 Downloads page; its
 * Delay section describes Surround Delay on printed pp. 31-34. It identifies
 * Rotation as moving each echo around the stereo field, and says negative
 * values rotate the other way, but publishes no numeric Rotation bounds,
 * transfer curve, or default. The family guide applies to FM9; the official
 * FM9 Downloads page lists v12.0 for Original and Turbo but does not
 * separately name Mark II Turbo. Installed firmware is unknown.
 * Sources: https://www.fractalaudio.com/fm9-downloads/ and
 * https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
 *
 * This implementation chooses project-authored control ranges/defaults and
 * delay storage sized to the longest selected echo, growing only when a
 * control requires more history. The project caps echoes at 4 seconds: at
 * 48 kHz the default 375 ms stereo ring is about 144 KB, and the maximum is
 * about 1.54 MB. At 192 kHz those bounds are about 576 KB and 6.15 MB. Storage
 * changes only during control updates; process() is in-place and does not
 * allocate. This is not a Fractal settings/DSP/sound replica.
 */
class FractalSurroundDelay final : public Effect {
public:
    explicit FractalSurroundDelay(
        float sample_rate = 48000.0F,
        FractalSurroundDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "fractal-surround-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    void set_parameters(FractalSurroundDelayParameters parameters) noexcept;
    [[nodiscard]] FractalSurroundDelayParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::size_t maximum_delay_frames() const noexcept {
        return maximum_delay_frames_;
    }
    [[nodiscard]] std::size_t maximum_supported_delay_frames() const noexcept {
        return static_cast<std::size_t>(sample_rate_ *
                                        kMaximumDelayMs / 1000.0F);
    }
    [[nodiscard]] std::size_t delay_left_frames() const noexcept {
        return target_delay_left_frames_;
    }
    [[nodiscard]] std::size_t delay_right_frames() const noexcept {
        return target_delay_right_frames_;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 192000.0F;
    static constexpr float kMinimumTimeMs = 5.0F;
    static constexpr float kMaximumBaseTimeMs = 500.0F;
    static constexpr float kMaximumDelayMs = 4000.0F;
    static constexpr float kMinimumMasterTimePercent = 25.0F;
    static constexpr float kMaximumMasterTimePercent = 400.0F;
    static constexpr std::size_t kParameterRampMs = 10U;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static float finite_audio(float sample) noexcept;
    [[nodiscard]] std::size_t delay_frames(float time_ms) const noexcept;
    void grow_storage(std::size_t frames) noexcept;
    void update_targets(bool ramp_delay) noexcept;
    [[nodiscard]] float read_delay(std::uint16_t channel,
                                   float delay_frames) const noexcept;
    void advance_delay_ramp() noexcept;

    float sample_rate_ = 48000.0F;
    FractalSurroundDelayParameters parameters_{};
    std::vector<float> storage_;
    std::size_t maximum_delay_frames_ = 0U;
    std::size_t write_frame_ = 0U;
    std::size_t target_delay_left_frames_ = 0U;
    std::size_t target_delay_right_frames_ = 0U;
    float current_delay_left_frames_ = 0.0F;
    float current_delay_right_frames_ = 0.0F;
    float delay_left_step_ = 0.0F;
    float delay_right_step_ = 0.0F;
    std::size_t delay_ramp_remaining_ = 0U;
    float rotation_cos_ = 0.0F;
    float rotation_sin_ = 0.0F;
    float feedback_left_ = 0.0F;
    float feedback_right_ = 0.0F;
    float width_scale_ = 1.0F;
    float mix_ = 0.25F;
    float level_gain_ = 1.0F;
};

} // namespace tabletfx
