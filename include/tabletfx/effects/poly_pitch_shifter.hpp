#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace tabletfx {

// Polyphonic pitch transposition using the pinned stftPitchShift engine.
// FFT/window state is prepared in the constructor; process() uses fixed-size
// blocks and performs no allocation after construction.
class PolyPitchShifter final : public Effect {
public:
    explicit PolyPitchShifter(float sample_rate = 48000.0F,
                              std::uint16_t channels = 2,
                              float max_shift_semitones = 12.0F);
    ~PolyPitchShifter() override;

    PolyPitchShifter(const PolyPitchShifter&) = delete;
    PolyPitchShifter& operator=(const PolyPitchShifter&) = delete;
    PolyPitchShifter(PolyPitchShifter&&) = delete;
    PolyPitchShifter& operator=(PolyPitchShifter&&) = delete;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "poly-pitch-shifter";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] float pitch_semitones() const noexcept { return pitch_semitones_; }
    [[nodiscard]] float max_shift_semitones() const noexcept {
        return max_shift_semitones_;
    }
    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] float algorithmic_latency_ms() const noexcept;
    [[nodiscard]] std::uint32_t analysis_block_samples() const noexcept;
    [[nodiscard]] std::uint32_t analysis_interval_samples() const noexcept;

    // Range is limited to the per-instance maximum (±12 by default, or the
    // explicit ±24/±60 profiles for wider-range processors). Parameter updates
    // are intended to be applied by the audio thread or while processing is
    // stopped.
    void set_pitch_semitones(float semitones) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    float sample_rate_;
    std::uint16_t channels_;
    float max_shift_semitones_;
    float pitch_semitones_ = 0.0F;
};

} // namespace tabletfx
