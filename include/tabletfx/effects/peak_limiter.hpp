#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

// Zero-lookahead linked peak limiter. The attack is immediate at each sample;
// gain recovery follows a one-pole release. This is a low-latency safety stage,
// not a model of a particular analog limiter.
class PeakLimiter final : public Effect {
public:
    explicit PeakLimiter(float sample_rate = 48000.0F,
                         float ceiling_db = -1.0F,
                         float release_ms = 80.0F) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "peak-limiter";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    // Configuration is intended for graph preparation, outside the audio callback.
    void configure(float sample_rate, float ceiling_db,
                   float release_ms) noexcept;
    void set_ceiling_db(float ceiling_db) noexcept;
    void set_release_ms(float release_ms) noexcept;
    [[nodiscard]] float ceiling_db() const noexcept { return ceiling_db_; }
    [[nodiscard]] float release_ms() const noexcept { return release_ms_; }
    [[nodiscard]] float ceiling_linear() const noexcept { return ceiling_linear_; }

    void process(std::span<float> interleaved,
                  std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kDefaultSampleRate = 48000.0F;
    static constexpr float kDefaultCeilingDb = -1.0F;
    static constexpr float kDefaultReleaseMs = 80.0F;

    float sample_rate_ = kDefaultSampleRate;
    float ceiling_db_ = kDefaultCeilingDb;
    float ceiling_linear_ = 0.8912509F;
    float release_ms_ = kDefaultReleaseMs;
    float release_coefficient_ = 0.0F;
    float gain_ = 1.0F;
};

} // namespace tabletfx
