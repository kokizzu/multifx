#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class HarmonicTremoloWaveform : std::uint8_t {
    Sine,
    Triangle,
    Square,
};

struct HarmonicTremoloParameters {
    float rate_hz = 3.0F;
    float depth = 0.65F;
    float crossover_hz = 700.0F;
    HarmonicTremoloWaveform waveform = HarmonicTremoloWaveform::Sine;
    bool tempo_sync = false;
    float tempo_bpm = 120.0F;
    // Quarter-note beats per modulation cycle: 1 = quarter, 0.5 = eighth.
    float beats_per_cycle = 1.0F;
    // With stereo input, the right channel swaps the low/high modulation
    // assignment. Mono remains mono and uses the ordinary complementary sweep.
    bool stereo_alternation = true;
    float smoothing_ms = 10.0F;
};

// Project-authored harmonic tremolo using a complementary one-pole frequency
// split and opposing low/high amplitude modulation. Stereo alternation is a
// project design choice; this processor does not claim Fender algorithm,
// parameter-range, or sound parity. All state is fixed-size and process()
// performs no allocation or per-sample trigonometric calls.
class HarmonicTremolo final : public Effect {
public:
    explicit HarmonicTremolo(float sample_rate = 48000.0F,
                             HarmonicTremoloParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "harmonic-tremolo";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void configure(float sample_rate,
                   HarmonicTremoloParameters parameters) noexcept;
    void set_rate_hz(float rate_hz) noexcept;
    void set_depth(float depth) noexcept;
    void set_crossover_hz(float frequency_hz) noexcept;
    void set_waveform(HarmonicTremoloWaveform waveform) noexcept;
    void set_tempo_sync(bool enabled, float bpm,
                        float beats_per_cycle) noexcept;
    void set_stereo_alternation(bool enabled) noexcept;
    void set_smoothing_ms(float smoothing_ms) noexcept;

    [[nodiscard]] HarmonicTremoloParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_rate_hz() const noexcept {
        return effective_rate_hz_;
    }
    [[nodiscard]] float current_depth() const noexcept {
        return current_depth_;
    }
    [[nodiscard]] float current_crossover_hz() const noexcept;

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
    static constexpr float kMinimumCrossoverHz = 80.0F;
    static constexpr float kMaximumCrossoverHz = 8000.0F;
    static constexpr float kMaximumSmoothingMs = 1000.0F;
    static constexpr float kMaximumInternalSample = 1000000.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static HarmonicTremoloWaveform sanitize_waveform(
        HarmonicTremoloWaveform waveform) noexcept;
    [[nodiscard]] float maximum_crossover_hz() const noexcept;
    [[nodiscard]] float alpha_for_crossover(float frequency_hz) const noexcept;
    [[nodiscard]] float modulation_value() const noexcept;
    void prepare_oscillator() noexcept;
    void synchronize_sine_phase() noexcept;
    void retarget_depth(float depth) noexcept;
    void retarget_crossover(float frequency_hz) noexcept;
    void advance_state() noexcept;

    float sample_rate_ = 48000.0F;
    HarmonicTremoloParameters parameters_{};
    float effective_rate_hz_ = 3.0F;

    float current_depth_ = 0.65F;
    float target_depth_ = 0.65F;
    float depth_step_ = 0.0F;
    std::size_t depth_frames_remaining_ = 0;

    float current_alpha_ = 0.0F;
    float target_alpha_ = 0.0F;
    float alpha_step_ = 0.0F;
    std::size_t alpha_frames_remaining_ = 0;
    std::array<float, 2> low_state_{};

    float phase_increment_ = 0.0F;
    float sine_increment_ = 0.0F;
    float cosine_increment_ = 1.0F;
    float phase_ = 0.0F;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    std::uint32_t normalization_counter_ = 0;
};

} // namespace tabletfx
