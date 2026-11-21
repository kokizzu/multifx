#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class ShelfTiltMode : std::uint8_t {
    LowShelf,
    HighShelf,
    Tilt,
};

// A project-authored low-shelf, high-shelf, and complementary tilt EQ.
// Parameter changes rebuild coefficients and reset fixed filter state, so
// configure the effect while it is not being processed.
class ShelfTiltEQ final : public Effect {
public:
    static constexpr float kMinimumFrequencyHz = 20.0F;
    static constexpr float kMinimumGainDb = -12.0F;
    static constexpr float kMaximumGainDb = 12.0F;

    explicit ShelfTiltEQ(float sample_rate = 48000.0F,
                         std::uint16_t channels = 2,
                         ShelfTiltMode mode = ShelfTiltMode::LowShelf,
                         float frequency_hz = 200.0F,
                         float gain_db = 0.0F) noexcept;

    [[nodiscard]] std::string_view id() const noexcept override;
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;

    [[nodiscard]] float sample_rate() const noexcept;
    [[nodiscard]] std::uint16_t channels() const noexcept;
    [[nodiscard]] ShelfTiltMode mode() const noexcept;
    [[nodiscard]] float frequency_hz() const noexcept;
    [[nodiscard]] float gain_db() const noexcept;

    // Frequencies are bounded to [20 Hz, 0.45 * sample rate], gain to
    // +/-12 dB. Non-finite frequency and gain values use safe defaults.
    // In Tilt mode, the low shelf uses gain_db and the high shelf uses its
    // inverse, creating a neutral crossing at frequency_hz.
    void configure(ShelfTiltMode mode, float frequency_hz,
                   float gain_db) noexcept;
    void set_mode(ShelfTiltMode mode) noexcept;
    void set_frequency_hz(float frequency_hz) noexcept;
    void set_gain_db(float gain_db) noexcept;

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
        SectionState low{};
        SectionState high{};
    };

    [[nodiscard]] static ShelfTiltMode valid_mode(ShelfTiltMode mode) noexcept;
    [[nodiscard]] static Coefficients make_shelf(float frequency_hz,
                                                 float gain_db,
                                                 float sample_rate,
                                                 bool high_shelf) noexcept;
    [[nodiscard]] static bool process_section(float& sample,
                                              const Coefficients& coefficients,
                                              SectionState& state) noexcept;

    void rebuild_coefficients() noexcept;
    void reset_channel(std::uint16_t channel) noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    ShelfTiltMode mode_ = ShelfTiltMode::LowShelf;
    float frequency_hz_ = 200.0F;
    float gain_db_ = 0.0F;
    Coefficients low_coefficients_{};
    Coefficients high_coefficients_{};
    std::array<ChannelState, 2> states_{};
};

} // namespace tabletfx
