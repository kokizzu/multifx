#include "tabletfx/effects/phase_invert.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_identity_and_stereo_polarity() {
    tabletfx::PhaseInvert phase_invert;
    require(phase_invert.id() == "phase-invert", "phase invert ID changed");
    require(phase_invert.memory_bytes() == sizeof(phase_invert),
            "phase invert memory report is inaccurate");

    std::array<float, 6> samples{0.25F, -0.5F, 0.75F, -1.0F, 0.0F, 0.125F};
    phase_invert.process(samples, 2);
    const std::array<float, 6> expected{-0.25F, 0.5F, -0.75F,
                                        1.0F, -0.0F, -0.125F};
    for (std::size_t index = 0; index < samples.size(); ++index) {
        require(samples[index] == expected[index],
                "phase invert did not negate every interleaved channel sample");
    }

    phase_invert.process(samples, 2);
    require(samples[0] == 0.25F && samples[1] == -0.5F &&
                samples[2] == 0.75F && samples[3] == -1.0F &&
                samples[5] == 0.125F,
            "two phase inversions did not restore the input");
}

void test_invalid_channel_count_is_safe() {
    tabletfx::PhaseInvert phase_invert;
    std::array<float, 3> samples{0.25F, -0.5F, 0.75F};
    phase_invert.process(samples, 0);
    require(samples[0] == 0.25F && samples[1] == -0.5F &&
                samples[2] == 0.75F,
            "zero-channel input must remain unchanged");
    phase_invert.process(std::span<float>{}, 2);
}

} // namespace

int main() {
    try {
        test_identity_and_stereo_polarity();
        test_invalid_channel_count_is_safe();
        std::cout << "All phase invert tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Phase invert test failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
