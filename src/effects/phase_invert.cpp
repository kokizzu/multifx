#include "tabletfx/effects/phase_invert.hpp"

namespace tabletfx {

void PhaseInvert::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels == 0) return;
    for (float& sample : interleaved) {
        sample = -sample;
    }
}

} // namespace tabletfx
