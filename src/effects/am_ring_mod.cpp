#include "tabletfx/effects/am_ring_mod.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx::effects {
namespace {

constexpr float kMaximumStableInput =
    std::numeric_limits<float>::max() / 16.0F;

}  // namespace

AmRingMod::AmRingMod(float sample_rate, std::uint16_t channels,
                     AmRingModParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0U || channels_ > 2U) {
        throw std::invalid_argument(
            "AM ring mod requires 8–384 kHz and mono or stereo channels");
    }
    set_parameters(parameters);
    reset();
}

void AmRingMod::set_parameters(AmRingModParameters parameters) noexcept {
    parameters.rate_hz = bounded(parameters.rate_hz, parameters_.rate_hz,
                                 kMinimumRateHz, kMaximumRateHz);
    parameters.depth = bounded(parameters.depth, parameters_.depth, 0.0F, 1.0F);
    parameters.mix = bounded(parameters.mix, parameters_.mix, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, parameters_.level_db,
                                   kMinimumLevelDb, kMaximumLevelDb);
    parameters_ = parameters;
    output_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    prepare_oscillator();
}

void AmRingMod::process(std::span<float> interleaved,
                        std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U || channels > 2U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const float envelope = 1.0F + parameters_.depth * phase_cosine_;
        const std::size_t offset = frame * channels_;
        for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
            const std::size_t index = offset + channel;
            const float input = interleaved[index];
            if (!std::isfinite(input) || std::abs(input) > kMaximumStableInput) {
                interleaved[index] = 0.0F;
                continue;
            }

            const float wet = input * envelope;
            const float mixed = input + (wet - input) * parameters_.mix;
            const float output = mixed * output_gain_;
            interleaved[index] = std::isfinite(output) ? output : 0.0F;
        }
        advance_oscillator();
    }
}

void AmRingMod::reset() noexcept {
    phase_sine_ = 0.0F;
    phase_cosine_ = 1.0F;
    normalization_counter_ = 0U;
}

float AmRingMod::bounded(float value, float fallback,
                         float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

void AmRingMod::prepare_oscillator() noexcept {
    // Keep the carrier below Nyquist. The input's modulation sidebands can
    // still alias at extreme signal/carrier combinations; no anti-alias claim.
    effective_rate_hz_ = std::min(parameters_.rate_hz, sample_rate_ * 0.45F);
    const float phase_step = kTwoPi * effective_rate_hz_ / sample_rate_;
    increment_sine_ = std::sin(phase_step);
    increment_cosine_ = std::cos(phase_step);
}

void AmRingMod::advance_oscillator() noexcept {
    const float previous_sine = phase_sine_;
    phase_sine_ = previous_sine * increment_cosine_ +
                  phase_cosine_ * increment_sine_;
    phase_cosine_ = phase_cosine_ * increment_cosine_ -
                    previous_sine * increment_sine_;

    if (++normalization_counter_ == 4096U) {
        const float magnitude_squared = phase_sine_ * phase_sine_ +
                                        phase_cosine_ * phase_cosine_;
        if (std::isfinite(magnitude_squared) && magnitude_squared > 0.0F) {
            const float inverse_magnitude = 1.0F / std::sqrt(magnitude_squared);
            phase_sine_ *= inverse_magnitude;
            phase_cosine_ *= inverse_magnitude;
        } else {
            phase_sine_ = 0.0F;
            phase_cosine_ = 1.0F;
        }
        normalization_counter_ = 0U;
    }
}

}  // namespace tabletfx::effects
