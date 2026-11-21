#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

struct TransparentOverdriveParameters {
    float drive = 0.15F;
    float level_db = -3.0F;
    float mix = 0.35F;
    float smoothing_ms = 10.0F;
};

// A low-gain, dry-blend overdrive using a symmetric rational soft clipper.
// It is a generic style target, not a circuit or product emulation.
class TransparentOverdrive final : public Effect {
public:
    explicit TransparentOverdrive(float sample_rate = 48000.0F,
                                  TransparentOverdriveParameters parameters = {}) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "transparent-overdrive";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }
    [[nodiscard]] float drive() const noexcept { return target_drive_; }
    [[nodiscard]] float level_db() const noexcept { return target_level_db_; }
    [[nodiscard]] float mix() const noexcept { return target_mix_; }

    void configure(TransparentOverdriveParameters parameters) noexcept;
    void set_drive(float drive) noexcept;
    void set_level_db(float level_db) noexcept;
    void set_mix(float mix) noexcept;
    void configure_smoothing(float sample_rate, float smoothing_ms) noexcept;
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    void retarget() noexcept;
    void advance_ramp() noexcept;
    [[nodiscard]] static float bounded_drive(float drive) noexcept;
    [[nodiscard]] static float bounded_level(float level_db) noexcept;
    [[nodiscard]] static float bounded_mix(float mix) noexcept;
    [[nodiscard]] static float db_to_gain(float level_db) noexcept;
    [[nodiscard]] static float soft_clip(float sample) noexcept;

    float sample_rate_ = 48000.0F;
    float smoothing_ms_ = 10.0F;
    float target_drive_ = 0.15F;
    float target_level_db_ = -3.0F;
    float target_mix_ = 0.35F;
    float drive_ = 0.15F;
    float drive_step_ = 0.0F;
    float level_gain_ = 0.70794576F;
    float target_level_gain_ = 0.70794576F;
    float level_step_ = 0.0F;
    float mix_ = 0.35F;
    float mix_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
};

} // namespace tabletfx
