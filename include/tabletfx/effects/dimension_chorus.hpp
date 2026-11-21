#pragma once

#include "tabletfx/effect.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tabletfx {

enum class DimensionChorusVersion : std::uint8_t {
    C = 0,
    D = 1,
};

enum class DimensionChorusMode : std::uint8_t {
    Mode1 = 0,
    Mode2 = 1,
    Mode3 = 2,
    Mode4 = 3,
    Modes12 = 4,
    Modes13 = 5,
    Modes14 = 6,
    Modes23 = 7,
    Modes24 = 8,
    Modes34 = 9,
};

struct DimensionChorusParameters {
    DimensionChorusVersion version = DimensionChorusVersion::C;
    DimensionChorusMode mode = DimensionChorusMode::Mode1;
    // Mix/Level are project-authored controls; these ranges are not vendor specs.
    float mix = 0.5F;
    float level_db = 0.0F;
};

/*
 * Project-authored chorus with a short modulated delay tap. Line 6's Helix
 * Stadium 1.4 Release Notes name Dimension W, based on the BOSS DC-2W, and
 * document Version C/D and Mode 1–4 plus the pairs 1+2, 1+3, 1+4, 2+3, 2+4,
 * and 3+4. The notes say higher single-mode numbers have more effect and paired
 * modes provide deeper modulation. They describe Version C as emulating the
 * 1985 C pedal and Version D as the classic 1979 D rackmount version. Mix is
 * described as a dry/effect blend with 0% and 100% endpoints; Level is the
 * overall block level. No internal DSP, mix/level numerical ranges, or exact
 * response curves are published.
 *
 * This processor uses the documented Version/Mode labels only. Its phase
 * choices, ten rate/delay/depth profiles, Mix/Level bounds and DSP are
 * project-authored and do not claim Line 6/BOSS DSP or setting parity. Delay
 * storage is allocated at construction; process() does not allocate.
 *
 * Source: https://kb.line6.com/helix-stadium-1-4-release-notes
 */
class DimensionChorus final : public Effect {
public:
    explicit DimensionChorus(float sample_rate = 48000.0F,
                             std::uint16_t channels = 2,
                             DimensionChorusParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "dimension-chorus";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + storage_.capacity() * sizeof(float);
    }

    void set_parameters(DimensionChorusParameters parameters) noexcept;
    [[nodiscard]] DimensionChorusParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float current_mix() const noexcept { return current_mix_; }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct ModeProfile {
        float base_delay_ms;
        float depth_ms;
        float rate_hz;
    };

    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kStorageDelayMs = 30.0F;
    static constexpr float kParameterSmoothingMs = 10.0F;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] static DimensionChorusVersion sanitize_version(
        DimensionChorusVersion version) noexcept;
    [[nodiscard]] static DimensionChorusMode sanitize_mode(
        DimensionChorusMode mode) noexcept;
    [[nodiscard]] static ModeProfile profile_for(DimensionChorusMode mode) noexcept;
    void retarget(bool immediate) noexcept;
    void advance_modulator() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    std::size_t capacity_frames_ = 0;
    std::size_t write_frame_ = 0;
    std::vector<float> storage_;
    DimensionChorusParameters parameters_{};

    float current_base_delay_ms_ = 12.0F;
    float current_depth_ms_ = 0.5F;
    float current_mix_ = 0.5F;
    float current_level_gain_ = 1.0F;
    float current_version_blend_ = 0.0F;
    float current_sine_increment_ = 0.0F;
    float current_cosine_increment_ = 1.0F;

    float target_base_delay_ms_ = 12.0F;
    float target_depth_ms_ = 0.5F;
    float target_mix_ = 0.5F;
    float target_level_gain_ = 1.0F;
    float target_version_blend_ = 0.0F;
    float target_sine_increment_ = 0.0F;
    float target_cosine_increment_ = 1.0F;

    float base_delay_step_ = 0.0F;
    float depth_step_ = 0.0F;
    float mix_step_ = 0.0F;
    float level_gain_step_ = 0.0F;
    float version_blend_step_ = 0.0F;
    float sine_increment_step_ = 0.0F;
    float cosine_increment_step_ = 0.0F;
    std::size_t ramp_remaining_ = 0;
    float sine_phase_ = 0.0F;
    float cosine_phase_ = 1.0F;
    std::uint32_t normalization_counter_ = 0;
};

} // namespace tabletfx
