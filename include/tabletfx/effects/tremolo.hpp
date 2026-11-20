#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class TremoloWaveform : std::uint8_t {
    Sine,
    Triangle,
    Square,
};

struct TremoloParameters {
    float rate_hz = 5.0F;
    float depth = 0.5F;
    TremoloWaveform waveform = TremoloWaveform::Sine;
    bool tempo_sync = false;
    float tempo_bpm = 120.0F;
    // Duration of one modulation cycle in quarter-note beats: 1 = quarter,
    // 0.5 = eighth, 2 = half note. Swing is not applied by this DSP block.
    float beats_per_cycle = 1.0F;
    float depth_smoothing_ms = 10.0F;
};

// Zero-delay amplitude modulation. All channels share one LFO phase so stereo
// movement does not collapse to unrelated channel gain changes. Depth changes
// use a bounded 10 ms default ramp. Setters and configure() prepare oscillator
// increments outside process(); processing uses fixed state, no allocation, and
// no per-sample trigonometric calls.
class Tremolo final : public Effect {
public:
    explicit Tremolo(float sample_rate = 48000.0F,
                     TremoloParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "tremolo";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    // bpm is quarter notes per minute; beats_per_cycle is the number of
    // quarter-note beats per tremolo period.
    void configure(float sample_rate, TremoloParameters parameters) noexcept;
    void set_rate_hz(float rate_hz) noexcept;
    void set_depth(float depth) noexcept;
    void set_depth_smoothing_ms(float smoothing_ms) noexcept;
    void set_waveform(TremoloWaveform waveform) noexcept;
    void set_tempo_sync(bool enabled, float bpm,
                        float beats_per_cycle) noexcept;

    [[nodiscard]] TremoloParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_rate_hz() const noexcept {
        return effective_rate_hz_;
    }
    [[nodiscard]] float current_depth() const noexcept { return current_depth_; }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumRateHz = 0.05F;
    static constexpr float kMaximumRateHz = 20.0F;
    static constexpr float kMinimumBpm = 30.0F;
    static constexpr float kMaximumBpm = 300.0F;
    static constexpr float kMinimumBeatsPerCycle = 0.0625F;
    static constexpr float kMaximumBeatsPerCycle = 16.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void prepare_oscillator() noexcept;

    float sample_rate_ = 48000.0F;
    TremoloParameters parameters_{};
    float effective_rate_hz_ = 5.0F;
    float current_depth_ = 0.5F;
    float target_depth_ = 0.5F;
    float depth_ramp_step_ = 0.0F;
    std::size_t depth_ramp_remaining_ = 0;
    float phase_increment_ = 0.0F;
    float sine_increment_ = 0.0F;
    float cosine_increment_ = 1.0F;
    float phase_ = 0.0F;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    std::uint32_t normalization_counter_ = 0;
};

} // namespace tabletfx
