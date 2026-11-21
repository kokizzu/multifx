#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tabletfx {

class Gain final : public Effect {
public:
    explicit Gain(float gain_db = 0.0F, float sample_rate = 48000.0F,
                  float smoothing_ms = 10.0F) noexcept;
    [[nodiscard]] std::string_view id() const noexcept override { return "gain"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }
    void set_gain_db(float gain_db) noexcept;
    void set_gain_db_immediate(float gain_db) noexcept;
    [[nodiscard]] float gain_db() const noexcept { return gain_db_; }
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;
    void configure_smoothing(float sample_rate, float smoothing_ms) noexcept;

private:
    float gain_db_;
    float gain_;
    float target_gain_;
    float ramp_step_;
    float sample_rate_;
    float smoothing_ms_;
    std::size_t ramp_remaining_ = 0;
};

struct NoiseGateParameters {
    float threshold_db = -60.0F;
    float attenuation_db = -72.0F;
    float hysteresis_db = 3.0F;
    float attack_ms = 2.0F;
    float hold_ms = 20.0F;
    float release_ms = 80.0F;
};

class NoiseGate final : public Effect {
public:
    NoiseGate(float sample_rate, float threshold_db = -60.0F,
              float attenuation_db = -72.0F, float attack_ms = 2.0F,
              float release_ms = 80.0F) noexcept;
    NoiseGate(float sample_rate, NoiseGateParameters parameters) noexcept;
    [[nodiscard]] std::string_view id() const noexcept override { return "noise-gate"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }
    void configure(float sample_rate, float threshold_db, float attenuation_db,
                   float attack_ms, float release_ms) noexcept;
    void configure(float sample_rate, NoiseGateParameters parameters) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    float sample_rate_;
    float threshold_;
    float close_threshold_;
    float closed_gain_;
    float attack_coefficient_;
    float release_coefficient_;
    float envelope_ = 0.0F;
    float gain_ = 0.0F;
    std::size_t hold_duration_samples_ = 0;
    std::size_t hold_remaining_samples_ = 0;
    bool gate_open_ = false;
};

class SoftDrive final : public Effect {
public:
    explicit SoftDrive(float drive = 1.0F) noexcept;
    [[nodiscard]] std::string_view id() const noexcept override { return "soft-drive"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }
    void set_drive(float drive) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;

private:
    float drive_;
    float normalization_;
};

enum class DelayTimeMode : std::uint8_t {
    milliseconds,
    tempo_sync,
};

enum class DelayDivision : std::uint8_t {
    whole,
    dotted_half,
    half,
    dotted_quarter,
    quarter,
    quarter_triplet,
    dotted_eighth,
    eighth,
    eighth_triplet,
    dotted_sixteenth,
    sixteenth,
    sixteenth_triplet,
};

class Delay final : public Effect {
public:
    Delay(float sample_rate, std::uint16_t channels, float max_delay_ms = 2000.0F,
          float delay_ms = 350.0F, float feedback = 0.25F, float mix = 0.25F,
          bool reserve_dual_line = false);
    [[nodiscard]] std::string_view id() const noexcept override { return "delay"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }
    [[nodiscard]] std::size_t delay_samples() const noexcept { return delay_samples_; }
    [[nodiscard]] float delay_time_ms() const noexcept {
        return static_cast<float>(static_cast<double>(delay_samples_) * 1000.0 /
                                  static_cast<double>(sample_rate_));
    }
    [[nodiscard]] float feedback() const noexcept { return target_feedback_; }
    [[nodiscard]] float mix() const noexcept { return target_mix_; }
    [[nodiscard]] bool ping_pong_enabled() const noexcept { return target_ping_pong_; }
    [[nodiscard]] bool dual_enabled() const noexcept { return target_dual_; }
    [[nodiscard]] float tempo_bpm() const noexcept { return tempo_bpm_; }
    [[nodiscard]] DelayDivision division() const noexcept { return division_; }
    [[nodiscard]] DelayTimeMode time_mode() const noexcept { return time_mode_; }
    void configure(float delay_ms, float feedback, float mix) noexcept;
    // Stereo wet routing crossfades over 10 ms. A mono instance keeps normal
    // same-channel delay behavior regardless of this setting.
    void configure_ping_pong(bool enabled) noexcept;
    // Independent stereo L/R delay lines. A mono instance needs the optional
    // second line reserved at construction and mixes both wet taps to mono.
    void configure_dual(float delay_a_ms, float feedback_a, float mix_a,
                        float delay_b_ms, float feedback_b, float mix_b) noexcept;
    void configure_tempo(float bpm, DelayDivision division, float feedback,
                         float mix) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    [[nodiscard]] std::size_t samples_for_ms(float delay_ms) const noexcept;
    [[nodiscard]] static float beats_for_division(DelayDivision division) noexcept;
    [[nodiscard]] float read_delay(std::size_t delay_samples,
                                   std::uint16_t channel) const noexcept;
    [[nodiscard]] float read_fractional_delay(float delay_samples,
                                              std::uint16_t channel) const noexcept;
    void set_delay_samples(std::size_t samples) noexcept;
    void set_dual_delay_samples(std::uint16_t line, std::size_t samples) noexcept;
    void retarget_parameters() noexcept;
    void advance_ramps() noexcept;

    float sample_rate_;
    std::uint16_t channels_;
    std::size_t storage_channels_;
    bool dual_capable_;
    std::size_t delay_samples_;
    float transition_from_delay_samples_ = 1.0F;
    std::size_t delay_transition_remaining_ = 0;
    std::size_t delay_transition_total_ = 1;
    float feedback_;
    float target_feedback_;
    float feedback_step_ = 0.0F;
    float mix_;
    float target_mix_;
    float mix_step_ = 0.0F;
    std::size_t parameter_ramp_remaining_ = 0;
    std::size_t parameter_ramp_total_ = 1;
    float tempo_bpm_ = 120.0F;
    DelayDivision division_ = DelayDivision::quarter;
    DelayTimeMode time_mode_ = DelayTimeMode::milliseconds;
    std::size_t write_frame_ = 0;
    std::size_t capacity_frames_;
    std::vector<float> storage_;
    std::array<std::size_t, 2> dual_delay_samples_{1, 1};
    std::array<float, 2> dual_transition_from_delay_samples_{1.0F, 1.0F};
    std::array<std::size_t, 2> dual_delay_transition_remaining_{};
    std::array<float, 2> dual_feedback_{};
    std::array<float, 2> target_dual_feedback_{};
    std::array<float, 2> dual_feedback_step_{};
    std::array<float, 2> dual_mix_{};
    std::array<float, 2> target_dual_mix_{};
    std::array<float, 2> dual_mix_step_{};
    std::size_t dual_parameter_ramp_remaining_ = 0;
    bool target_ping_pong_ = false;
    float ping_pong_mix_ = 0.0F;
    float ping_pong_step_ = 0.0F;
    std::size_t ping_pong_ramp_remaining_ = 0;
    bool target_dual_ = false;
    float dual_routing_mix_ = 0.0F;
    float dual_routing_step_ = 0.0F;
    std::size_t dual_routing_ramp_remaining_ = 0;
};

} // namespace tabletfx
