#include "tabletfx/effects/harmonic_resonator.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kDefaultRootFrequencyHz = 110.0F;
constexpr float kDefaultDecaySeconds = 1.2F;
constexpr float kDefaultBrightness = 0.55F;
constexpr float kDefaultMix = 0.5F;
constexpr float kPi = 3.14159265358979323846F;

} // namespace

HarmonicResonator::HarmonicResonator(
    float sample_rate, HarmonicResonatorParameters parameters) noexcept {
    const bool valid_rate = std::isfinite(sample_rate) &&
                            sample_rate >= kMinimumSampleRate &&
                            sample_rate <= kMaximumSampleRate;
    sample_rate_ = valid_rate ? sample_rate : kDefaultSampleRate;
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(
                static_cast<double>(sample_rate_) *
                static_cast<double>(kParameterSmoothingMs) / 1000.0)));
    set_parameters(parameters);
    coefficients_ = targets_;
    for (auto& mode_steps : steps_) {
        mode_steps = {};
    }
    ramp_remaining_ = 0;
    reset();
}

void HarmonicResonator::set_parameters(
    HarmonicResonatorParameters parameters) noexcept {
    const float root_max = std::min(
        kMaximumRootFrequencyHz,
        sample_rate_ * 0.45F / static_cast<float>(kModes));
    parameters.root_frequency_hz = bounded(
        parameters.root_frequency_hz, kDefaultRootFrequencyHz,
        kMinimumRootFrequencyHz, root_max);
    parameters.decay_seconds = bounded(
        parameters.decay_seconds, kDefaultDecaySeconds,
        kMinimumDecaySeconds, kMaximumDecaySeconds);
    parameters.brightness = bounded(parameters.brightness,
                                    kDefaultBrightness, 0.0F, 1.0F);
    parameters.mix = bounded(parameters.mix, kDefaultMix, 0.0F, 1.0F);
    parameters_ = parameters;
    prepare_coefficients();
}

void HarmonicResonator::process(std::span<float> interleaved,
                                std::uint16_t channels) noexcept {
    if (channels == 0 || channels > 2) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            if (!std::isfinite(sample)) {
                state_1_[channel].fill(0.0F);
                state_2_[channel].fill(0.0F);
                sample = 0.0F;
                continue;
            }

            const float original = sample;
            const float input = std::clamp(original, -kMaximumInputMagnitude,
                                           kMaximumInputMagnitude);
            double sum = 0.0;
            for (std::size_t mode = 0; mode < kModes; ++mode) {
                const Coefficients& c = coefficients_[mode];
                const float output = c.b0 * input + state_1_[channel][mode];
                const float next_1 = c.b1 * input - c.a1 * output +
                                     state_2_[channel][mode];
                const float next_2 = c.b2 * input - c.a2 * output;
                if (!std::isfinite(output) || !std::isfinite(next_1) ||
                    !std::isfinite(next_2) ||
                    std::abs(next_1) > kMaximumStateMagnitude ||
                    std::abs(next_2) > kMaximumStateMagnitude) {
                    state_1_[channel][mode] = 0.0F;
                    state_2_[channel][mode] = 0.0F;
                    continue;
                }
                state_1_[channel][mode] = next_1;
                state_2_[channel][mode] = next_2;
                sum += static_cast<double>(output) * weights_[mode];
            }

            const float wet = static_cast<float>(sum / weight_norm_);
            if (!std::isfinite(wet)) {
                state_1_[channel].fill(0.0F);
                state_2_[channel].fill(0.0F);
                sample = 0.0F;
            } else if (parameters_.mix == 0.0F) {
                sample = original;
            } else {
                sample = input + (wet - input) * parameters_.mix;
            }
        }
        advance_coefficients();
    }
}

void HarmonicResonator::reset() noexcept {
    for (auto& channel : state_1_) {
        channel.fill(0.0F);
    }
    for (auto& channel : state_2_) {
        channel.fill(0.0F);
    }
    coefficients_ = targets_;
    for (auto& mode_steps : steps_) {
        mode_steps = {};
    }
    ramp_remaining_ = 0;
}

float HarmonicResonator::bounded(float value, float fallback,
                                 float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum)
                                : fallback;
}

void HarmonicResonator::prepare_coefficients() noexcept {
    const float exponent = 2.0F - 1.5F * parameters_.brightness;
    double norm_squared = 0.0;
    for (std::size_t mode = 0; mode < kModes; ++mode) {
        const float harmonic = static_cast<float>(mode + 1U);
        weights_[mode] = 1.0F / std::pow(harmonic, exponent);
        norm_squared += static_cast<double>(weights_[mode]) * weights_[mode];

        const float frequency = parameters_.root_frequency_hz * harmonic;
        const float maximum_frequency = std::min(18000.0F, sample_rate_ * 0.45F);
        if (frequency > maximum_frequency) {
            targets_[mode] = {};
            continue;
        }

        const float q = std::clamp(
            0.455F * frequency * parameters_.decay_seconds,
            0.5F, kMaximumQ);
        const float omega = 2.0F * kPi * frequency / sample_rate_;
        const float sine = std::sin(omega);
        const float cosine = std::cos(omega);
        const float alpha = sine / (2.0F * q);
        const float a0 = 1.0F + alpha;
        targets_[mode] = {
            alpha / a0,
            0.0F,
            -alpha / a0,
            (-2.0F * cosine) / a0,
            (1.0F - alpha) / a0,
        };
    }
    weight_norm_ = static_cast<float>(std::sqrt(norm_squared));
    if (!std::isfinite(weight_norm_) || weight_norm_ <= 0.0F) {
        weight_norm_ = 1.0F;
    }

    ramp_remaining_ = ramp_total_;
    const float divisor = static_cast<float>(ramp_total_);
    for (std::size_t mode = 0; mode < kModes; ++mode) {
        const Coefficients& current = coefficients_[mode];
        const Coefficients& target = targets_[mode];
        steps_[mode] = {
            (target.b0 - current.b0) / divisor,
            (target.b1 - current.b1) / divisor,
            (target.b2 - current.b2) / divisor,
            (target.a1 - current.a1) / divisor,
            (target.a2 - current.a2) / divisor,
        };
    }
}

void HarmonicResonator::advance_coefficients() noexcept {
    if (ramp_remaining_ == 0) {
        return;
    }
    --ramp_remaining_;
    if (ramp_remaining_ == 0) {
        coefficients_ = targets_;
        return;
    }

    for (std::size_t mode = 0; mode < kModes; ++mode) {
        Coefficients& current = coefficients_[mode];
        const Coefficients& step = steps_[mode];
        current.b0 += step.b0;
        current.b1 += step.b1;
        current.b2 += step.b2;
        current.a1 += step.a1;
        current.a2 += step.a2;
    }
}

} // namespace tabletfx
