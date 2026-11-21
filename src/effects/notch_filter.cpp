#include "tabletfx/effects/notch_filter.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kDefaultFrequencyHz = 1000.0F;
constexpr float kDefaultQ = 4.0F;
constexpr float kDefaultMix = 1.0F;
constexpr float kPi = 3.14159265358979323846F;

} // namespace

NotchFilter::NotchFilter(float sample_rate,
                         NotchFilterParameters parameters) noexcept {
    const bool valid_rate = std::isfinite(sample_rate) &&
                            sample_rate >= kMinimumSampleRate &&
                            sample_rate <= kMaximumSampleRate;
    sample_rate_ = valid_rate ? sample_rate : kDefaultSampleRate;
    set_parameters(parameters);
    g_ = target_g_;
    k_ = target_k_;
    mix_ = target_mix_;
    g_step_ = 0.0F;
    k_step_ = 0.0F;
    mix_step_ = 0.0F;
    ramp_remaining_ = 0;
    reset();
}

void NotchFilter::set_parameters(NotchFilterParameters parameters) noexcept {
    const float maximum_frequency =
        std::min(kMaximumFrequencyHz, sample_rate_ * 0.40F);
    parameters.frequency_hz = bounded(parameters.frequency_hz,
                                      kDefaultFrequencyHz,
                                      kMinimumFrequencyHz,
                                      maximum_frequency);
    parameters.q = bounded(parameters.q, kDefaultQ, kMinimumQ, kMaximumQ);
    parameters.mix = bounded(parameters.mix, kDefaultMix, 0.0F, 1.0F);
    parameters_ = parameters;

    target_g_ = std::tan(kPi * parameters_.frequency_hz / sample_rate_);
    target_k_ = 1.0F / parameters_.q;
    target_mix_ = parameters_.mix;
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(
                static_cast<double>(sample_rate_) *
                static_cast<double>(kParameterSmoothingMs) / 1000.0)));
    ramp_remaining_ = ramp_total_;
    g_step_ = (target_g_ - g_) / static_cast<float>(ramp_total_);
    k_step_ = (target_k_ - k_) / static_cast<float>(ramp_total_);
    mix_step_ = (target_mix_ - mix_) / static_cast<float>(ramp_total_);
}

void NotchFilter::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels == 0 || channels > 2) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float g = g_;
        const float k = k_;
        const float mix = mix_;
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            if (!std::isfinite(sample)) {
                integrator_1_[channel] = 0.0F;
                integrator_2_[channel] = 0.0F;
                sample = 0.0F;
                continue;
            }

            const float original = sample;
            const float input =
                std::clamp(original, -kMaximumInputMagnitude,
                           kMaximumInputMagnitude);
            const float wet = filter_sample(input, channel, g, k);
            if (!std::isfinite(wet)) {
                integrator_1_[channel] = 0.0F;
                integrator_2_[channel] = 0.0F;
                sample = 0.0F;
            } else if (mix <= 0.0F) {
                sample = original;
            } else {
                sample = input + (wet - input) * mix;
            }
        }
        advance_parameters();
    }
}

void NotchFilter::reset() noexcept {
    integrator_1_.fill(0.0F);
    integrator_2_.fill(0.0F);
    g_ = target_g_;
    k_ = target_k_;
    mix_ = target_mix_;
    g_step_ = 0.0F;
    k_step_ = 0.0F;
    mix_step_ = 0.0F;
    ramp_remaining_ = 0;
}

float NotchFilter::bounded(float value, float fallback,
                           float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

void NotchFilter::advance_parameters() noexcept {
    if (ramp_remaining_ == 0) {
        return;
    }
    --ramp_remaining_;
    if (ramp_remaining_ == 0) {
        g_ = target_g_;
        k_ = target_k_;
        mix_ = target_mix_;
        return;
    }
    g_ += g_step_;
    k_ += k_step_;
    mix_ += mix_step_;
}

float NotchFilter::filter_sample(float input, std::uint16_t channel,
                                 float g, float k) noexcept {
    const float denominator = 1.0F + g * (g + k);
    const float a1 = 1.0F / denominator;
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = input - integrator_2_[channel];
    const float band = a1 * integrator_1_[channel] + a2 * v3;
    const float low = integrator_2_[channel] + a2 * integrator_1_[channel] +
                      a3 * v3;
    integrator_1_[channel] = 2.0F * band - integrator_1_[channel];
    integrator_2_[channel] = 2.0F * low - integrator_2_[channel];

    if (!std::isfinite(integrator_1_[channel]) ||
        !std::isfinite(integrator_2_[channel]) ||
        std::abs(integrator_1_[channel]) > kMaximumStateMagnitude ||
        std::abs(integrator_2_[channel]) > kMaximumStateMagnitude) {
        integrator_1_[channel] = 0.0F;
        integrator_2_[channel] = 0.0F;
        return 0.0F;
    }

    return input - k * band;
}

} // namespace tabletfx
