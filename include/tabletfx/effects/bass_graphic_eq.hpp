#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class BassGraphicEQLayout : std::uint8_t {
    FourBand,
    SevenBand,
};

// A project-authored bass graphic EQ with fixed-frequency peaking bands.
// Select the layout and set gains while the effect is not being processed;
// coefficient generation and state reset happen outside the audio callback.
// Sample rates outside [16 kHz, 384 kHz] fall back to 48 kHz; channel counts
// other than one select stereo.
class BassGraphicEQ final : public Effect {
public:
    static constexpr std::size_t kMaximumBands = 7;
    static constexpr float kMinimumGainDb = -12.0F;
    static constexpr float kMaximumGainDb = 12.0F;

    explicit BassGraphicEQ(float sample_rate = 48000.0F,
                           std::uint16_t channels = 2,
                           BassGraphicEQLayout layout = BassGraphicEQLayout::SevenBand) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override;
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;

    [[nodiscard]] float sample_rate() const noexcept;
    [[nodiscard]] std::uint16_t channels() const noexcept;
    [[nodiscard]] BassGraphicEQLayout layout() const noexcept;
    [[nodiscard]] std::size_t band_count() const noexcept;
    [[nodiscard]] float band_frequency_hz(std::size_t band) const noexcept;
    [[nodiscard]] float band_gain_db(std::size_t band) const noexcept;

    // Returns false for an index outside the active layout. Non-finite gains
    // are treated as 0 dB; finite gains are bounded to +/-12 dB. This updates
    // coefficients and clears filter state, so call outside process().
    [[nodiscard]] bool set_band_gain_db(std::size_t band, float gain_db) noexcept;

    // Switching layouts clears all band gains and filter state.
    void set_layout(BassGraphicEQLayout layout) noexcept;

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct Coefficients {
        float b0 = 1.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    struct SectionState {
        float z1 = 0.0F;
        float z2 = 0.0F;
    };

    struct ChannelState {
        std::array<SectionState, kMaximumBands> bands{};
    };

    [[nodiscard]] static float center_frequency_hz(BassGraphicEQLayout layout,
                                                    std::size_t band) noexcept;
    [[nodiscard]] static std::size_t band_count_for_layout(
        BassGraphicEQLayout layout) noexcept;
    [[nodiscard]] static BassGraphicEQLayout valid_layout(
        BassGraphicEQLayout layout) noexcept;
    [[nodiscard]] static Coefficients make_peak(float frequency_hz,
                                                float gain_db,
                                                float q,
                                                float sample_rate) noexcept;
    [[nodiscard]] static bool process_section(float& sample,
                                             const Coefficients& coefficients,
                                             SectionState& state) noexcept;

    void rebuild_coefficients() noexcept;
    void reset_channel(std::uint16_t channel) noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    BassGraphicEQLayout layout_ = BassGraphicEQLayout::SevenBand;
    std::size_t band_count_ = 7;
    std::array<float, kMaximumBands> gains_db_{};
    std::array<Coefficients, kMaximumBands> coefficients_{};
    std::array<ChannelState, 2> states_{};
};

} // namespace tabletfx
