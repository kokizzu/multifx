#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

struct ReverseDelayParameters {
    float time_ms = 350.0F;
    float feedback = 0.25F;
    float mix = 0.35F;
    float crossfade_ms = 10.0F;
};

/*
 * Project-authored rolling reverse-snippet delay. Controls are independently
 * bounded and are not intended to reproduce any manufacturer's sound or DSP.
 * The implementation records a snippet, plays it backward, and overlaps
 * adjacent snippets with an equal-power crossfade. Its wet path begins after
 * one selected Time interval; the dry path remains immediate.
 *
 * Manufacturer evidence checked 2026-11-21:
 * - The November 2026 Fractal Audio Blocks Guide lists Reverse Delay and names
 *   Time, Run, Trigger Restart, Crossfade Time, Mix, and a delay-only EQ page.
 *   It describes rolling reversed snippets; the public guide gives no numeric
 *   bounds for Reverse-specific Time/Crossfade controls. FM9 firmware v12.0 is
 *   listed for Original/Turbo; Mark II Turbo applicability is family-level,
 *   not a separate SKU line in the release notes:
 *   https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
 *   https://www.fractalaudio.com/fm9-downloads/
 * - Line 6's Helix Stadium catalog lists mono and stereo Reverse Delay as a
 *   Line 6 Original (initial appearance 1.0); the current v1.4.1 effect-block
 *   manual describes Delay generically rather than publishing a model-specific
 *   parameter table:
 *   https://line6.com/helix-stadium-models/
 *   https://manuals.line6.com/en/helix-stadium/live/effect-blocks
 * - Fender Tone Master Pro Model Guide revision v1.1.100 lists Time, Tap
 *   Division, Feedback, Mix, High Cut, Low Cut, Forward Level, and Spillover
 *   Switch for Reverse Delay. It describes Forward Level as an additional
 *   forward delay signal; the guide does not publish numeric ranges. Latest
 *   checked firmware is v1.8.85:
 *   https://www.fmicassets.com/Damroot/Original/10018/OM_2274900000_TONE-MASTER-PRO-Model-Guide_English.pdf
 *   https://support.fender.com/hc/en-us/articles/46768129660315-Fender-Tone-Master-Pro-Firmware-and-Pro-Control-App-Updates
 * - HeadRush's Prime product page lists Reverse Delay inspired by Boss DD-5,
 *   but does not publish its controls or ranges:
 *   https://www.headrushfx.com/products/prime/
 *
 * This project implements the shared Time, Feedback, and Mix behavior plus
 * Fractal's Run, Trigger Restart, and Crossfade controls. Tempo/tap sync, wet
 * High/Low Cut filters, Forward Level, and Spillover are tracked in the
 * comparison checklist and are not implemented by this module yet.
 * These are project-authored bounds: Time 1–2000 ms, Feedback 0–0.95, Mix 0–1,
 * and Crossfade 0–50 ms, with the effective crossfade additionally limited to
 * less than half of Time. The audio thread performs no allocations.
 */
class ReverseDelay final : public Effect {
public:
    explicit ReverseDelay(float sample_rate = 48000.0F,
                          std::uint16_t channels = 1,
                          float max_delay_ms = 2000.0F,
                          ReverseDelayParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "reverse-delay";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    void set_parameters(ReverseDelayParameters parameters) noexcept;
    [[nodiscard]] ReverseDelayParameters parameters() const noexcept {
        return parameters_;
    }
    void set_running(bool running) noexcept { running_ = running; }
    [[nodiscard]] bool running() const noexcept { return running_; }
    void trigger_restart() noexcept;

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 1.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kDefaultSampleRate = 48000.0F;
    static constexpr float kDefaultTimeMs = 350.0F;
    static constexpr float kDefaultFeedback = 0.25F;
    static constexpr float kDefaultMix = 0.35F;
    static constexpr float kDefaultCrossfadeMs = 10.0F;
    static constexpr float kMaximumCrossfadeMs = 50.0F;
    static constexpr float kParameterSmoothingMs = 10.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] std::size_t samples_for_ms(float milliseconds) const noexcept;
    [[nodiscard]] float read_age(std::size_t age,
                                 std::uint16_t channel) const noexcept;
    void advance_parameters() noexcept;

    float sample_rate_ = kDefaultSampleRate;
    std::uint16_t channels_ = 1;
    float max_delay_ms_ = 2000.0F;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::uint64_t write_count_ = 0;
    std::uint64_t capture_start_frame_ = 0;
    std::size_t delay_samples_ = 1;
    std::size_t crossfade_samples_ = 0;
    std::vector<float> storage_;
    ReverseDelayParameters parameters_{};
    float feedback_ = 0.0F;
    float feedback_step_ = 0.0F;
    float mix_ = 0.0F;
    float mix_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
    std::size_t ramp_total_ = 1;
    bool running_ = true;
};

} // namespace tabletfx
