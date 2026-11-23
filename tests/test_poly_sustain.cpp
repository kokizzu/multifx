#include "tabletfx/effects/poly_sustain.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

void fill_chord(std::span<float> samples, const std::uint16_t channels,
                const float sample_rate) {
    const std::size_t frames = samples.size() / channels;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const float time = static_cast<float>(frame) / sample_rate;
        const float value = 0.20F * std::sin(2.0F * 3.14159265358979323846F * 110.0F * time) +
                            0.13F * std::sin(2.0F * 3.14159265358979323846F * 164.81F * time) +
                            0.09F * std::sin(2.0F * 3.14159265358979323846F * 220.0F * time);
        for (std::uint16_t channel = 0U; channel < channels; ++channel) {
            samples[frame * channels + channel] = value;
        }
    }
}

double rms(std::span<const float> samples) {
    double energy = 0.0;
    for (const float sample : samples) {
        energy += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return samples.empty() ? 0.0 : std::sqrt(energy / static_cast<double>(samples.size()));
}

}  // namespace

int main() {
    constexpr float sample_rate = 48000.0F;
    tabletfx::effects::PolySustainParameters parameters;
    parameters.attack_ms = 0.0F;
    parameters.decay_ms = 60.0F;
    parameters.fx_level = 1.0F;
    parameters.operation = tabletfx::effects::PolySustainOperation::DryKill;
    tabletfx::effects::PolySustain sustain(sample_rate, 1U, parameters);

    std::vector<float> history(6000U, 0.0F);
    fill_chord(history, 1U, sample_rate);
    sustain.process_bypassed(history, 1U);
    for (const float sample : history) assert(std::isfinite(sample));
    assert(std::abs(rms(history)) > 0.05);

    std::vector<float> captured(4096U, 0.0F);
    sustain.process(captured, 1U);
    assert(std::abs(rms(captured)) > 0.01);
    for (const float sample : captured) assert(std::isfinite(sample));

    std::vector<float> tail(4096U, 0.0F);
    sustain.process_bypassed(tail, 1U);
    const double early_tail = rms(std::span<const float>(tail).first(512U));
    const double late_tail = rms(std::span<const float>(tail).last(512U));
    assert(early_tail > 0.0);
    assert(late_tail < early_tail);

    parameters.operation = tabletfx::effects::PolySustainOperation::MuteAll;
    parameters.attack_ms = 0.0F;
    sustain.configure(parameters);
    std::vector<float> muted(256U, 0.25F);
    sustain.process(muted, 1U);
    for (const float sample : muted) assert(sample == 0.0F);

    parameters.interval_semitones = 30.0F;
    parameters.attack_ms = -1.0F;
    parameters.decay_ms = 20000.0F;
    sustain.configure(parameters);
    assert(sustain.parameters().interval_semitones == 12.0F);
    assert(sustain.parameters().attack_ms == 0.0F);
    assert(sustain.parameters().decay_ms == 10000.0F);
    assert(sustain.memory_bytes() >= sizeof(sustain));

    sustain.reset();
    std::array<float, 4> invalid_input{0.2F, -0.2F, 0.1F, -0.1F};
    sustain.process(invalid_input, 2U);  // Mismatched layout is a safe no-op.
    assert(invalid_input[0] == 0.2F);
    assert(invalid_input[1] == -0.2F);

    std::cout << "Poly Sustain sample-loop hold, operation, tail, bounds, and channel guard passed\n";
    return 0;
}
