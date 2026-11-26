#pragma once

#include "tabletfx/effect.hpp"
#include "tabletfx/effects/chorus.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class ToneMasterPro3DChorusMode : std::uint8_t {
    Mode1 = 0,
    Mode2 = 1,
    Mode3 = 2,
    Mode4 = 3,
};

// Project-authored four-mode stereo chorus. Fender's Tone Master Pro guide
// names a Mode control with four modulations but does not publish individual
// mode names, control curves, numeric settings, DSP, or latency. This class
// exposes only the documented mode choice and reuses the existing Chorus DSP.
class ToneMasterPro3DChorus final : public Effect {
public:
    explicit ToneMasterPro3DChorus(float sample_rate = 48000.0F,
                                    std::uint16_t channels = 2);

    [[nodiscard]] std::string_view id() const noexcept override {
        return "tone-master-pro-3d-chorus";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] ToneMasterPro3DChorusMode mode() const noexcept {
        return mode_;
    }
    void set_mode(ToneMasterPro3DChorusMode mode) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    [[nodiscard]] static ChorusParameters parameters_for_mode(
        ToneMasterPro3DChorusMode mode) noexcept;

    Chorus chorus_;
    ToneMasterPro3DChorusMode mode_ = ToneMasterPro3DChorusMode::Mode1;
};

} // namespace tabletfx
