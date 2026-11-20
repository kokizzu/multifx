#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tabletfx {

enum class EnvelopeWahDirection : std::uint8_t { Up, Down };

struct EnvelopeWahParameters {
    float sensitivity = 0.5F; // 0 = less sensitive; 1 = more sensitive
    float minimum_hz = 350.0F;
    float maximum_hz = 2200.0F;
    float resonance_q = 4.0F;
    float level_db = 0.0F;
    float attack_ms = 5.0F;
    float release_ms = 80.0F;
    float smoothing_ms = 5.0F;
    EnvelopeWahDirection direction = EnvelopeWahDirection::Up;
};

// Envelope-controlled resonant band-pass. Its log-frequency table is built
// during configure(), keeping trigonometry and allocation off the audio path.
class EnvelopeWah final : public Effect {
public:
    explicit EnvelopeWah(float sample_rate = 48000.0F,
                         std::uint16_t channels = 2,
                         EnvelopeWahParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override { return "envelope-wah"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }
    [[nodiscard]] EnvelopeWahParameters parameters() const noexcept { return parameters_; }
    [[nodiscard]] float current_frequency_hz() const noexcept;
    [[nodiscard]] float target_frequency_hz() const noexcept;

    void configure(float sample_rate, std::uint16_t channels,
                   EnvelopeWahParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    struct State { float integrator1 = 0.0F; float integrator2 = 0.0F; };
    static constexpr std::size_t kTableEntries = 129;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    EnvelopeWahParameters parameters_{};
    std::array<float, kTableEntries> g_table_{};
    std::array<State, 2> states_{};
    float threshold_ = 0.01F;
    float envelope_ = 0.0F;
    float attack_coefficient_ = 0.0F;
    float release_coefficient_ = 0.0F;
    float smoothing_coefficient_ = 0.0F;
    float current_g_ = 0.0F;
    float k_ = 0.25F;
    float level_ = 1.0F;
};

} // namespace tabletfx
