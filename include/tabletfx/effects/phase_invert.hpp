#pragma once

#include "tabletfx/effect.hpp"

namespace tabletfx {

// Stateless polarity inversion used by the per-block Advanced Edit control.
class PhaseInvert final : public Effect {
public:
    [[nodiscard]] std::string_view id() const noexcept override {
        return "phase-invert";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
};

} // namespace tabletfx
