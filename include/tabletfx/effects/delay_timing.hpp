#pragma once

#include <cstdint>

namespace tabletfx {

enum class DelayTimeMode : std::uint8_t {
    milliseconds,
    tempo_sync,
};

enum class DelayDivision : std::uint8_t {
    whole,
    dotted_half,
    half,
    dotted_quarter,
    quarter,
    quarter_triplet,
    dotted_eighth,
    eighth,
    eighth_triplet,
    dotted_sixteenth,
    sixteenth,
    sixteenth_triplet,
};

} // namespace tabletfx
