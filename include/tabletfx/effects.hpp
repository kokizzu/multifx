#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tabletfx {

class Gain final : public Effect {
public:
    explicit Gain(float gain_db = 0.0F) noexcept;
    [[nodiscard]] std::string_view id() const noexcept override { return "gain"; }
    void set_gain_db(float gain_db) noexcept;
    [[nodiscard]] float gain_db() const noexcept { return gain_db_; }
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;

private:
    float gain_db_;
    float gain_;
};

class NoiseGate final : public Effect {
public:
    NoiseGate(float sample_rate, float threshold_db = -60.0F,
              float attenuation_db = -72.0F, float attack_ms = 2.0F,
              float release_ms = 80.0F) noexcept;
    [[nodiscard]] std::string_view id() const noexcept override { return "noise-gate"; }
    void configure(float sample_rate, float threshold_db, float attenuation_db,
                   float attack_ms, float release_ms) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    float sample_rate_;
    float threshold_;
    float closed_gain_;
    float attack_coefficient_;
    float release_coefficient_;
    float envelope_ = 0.0F;
    float gain_ = 0.0F;
};

class SoftDrive final : public Effect {
public:
    explicit SoftDrive(float drive = 1.0F) noexcept;
    [[nodiscard]] std::string_view id() const noexcept override { return "soft-drive"; }
    void set_drive(float drive) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;

private:
    float drive_;
    float normalization_;
};

class Delay final : public Effect {
public:
    Delay(float sample_rate, std::uint16_t channels, float max_delay_ms = 2000.0F,
          float delay_ms = 350.0F, float feedback = 0.25F, float mix = 0.25F);
    [[nodiscard]] std::string_view id() const noexcept override { return "delay"; }
    void configure(float delay_ms, float feedback, float mix) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    float sample_rate_;
    std::uint16_t channels_;
    std::size_t delay_samples_;
    float feedback_;
    float mix_;
    std::size_t write_frame_ = 0;
    std::size_t capacity_frames_;
    std::vector<float> storage_;
};

} // namespace tabletfx
