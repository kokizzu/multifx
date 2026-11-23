#include "tabletfx/effects/stereo_crossover.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTwoPi = 2.0F * kPi;
constexpr float kButterworthQ = 0.7071067811865475244F;
constexpr float kMaximumInternalSample = 1000000.0F;

} // namespace

StereoCrossover::StereoCrossover(
    float sample_rate, StereoCrossoverParameters parameters) noexcept {
    configure(sample_rate, parameters);
    reset();
}

void StereoCrossover::configure(
    float sample_rate, StereoCrossoverParameters parameters) noexcept {
    const float sanitized_sample_rate =
        std::isfinite(sample_rate) && sample_rate >= kMinimumSampleRate &&
                sample_rate <= kMaximumSampleRate
            ? sample_rate
            : 48000.0F;
    const bool sample_rate_changed = sanitized_sample_rate != sample_rate_;
    sample_rate_ = sanitized_sample_rate;
    sanitize_parameters(parameters);
    parameters_ = parameters;
    prepare_filters();
    prepare_gains();
    if (sample_rate_changed) {
        reset();
    }
}

void StereoCrossover::configure(
    StereoCrossoverParameters parameters) noexcept {
    configure(sample_rate_, parameters);
}

float StereoCrossover::BiquadState::process(
    float input, const BiquadCoefficients& coefficients) noexcept {
    const float output = coefficients.b0 * input + z1;
    z1 = coefficients.b1 * input - coefficients.a1 * output + z2;
    z2 = coefficients.b2 * input - coefficients.a2 * output;
    if (!std::isfinite(output) || !std::isfinite(z1) || !std::isfinite(z2)) {
        reset();
        return 0.0F;
    }
    z1 = std::clamp(z1, -kMaximumInternalSample, kMaximumInternalSample);
    z2 = std::clamp(z2, -kMaximumInternalSample, kMaximumInternalSample);
    return std::clamp(output, -kMaximumInternalSample,
                      kMaximumInternalSample);
}

void StereoCrossover::ChannelState::reset() noexcept {
    low_first.reset();
    low_second.reset();
    high_first.reset();
    high_second.reset();
}

float StereoCrossover::bounded(float value, float fallback,
                               float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum)
                                : fallback;
}

float StereoCrossover::decibels_to_gain(float decibels) noexcept {
    return std::pow(10.0F, decibels / 20.0F);
}

StereoCrossover::PanGains StereoCrossover::pan_gains(float pan) noexcept {
    const float angle = (pan + 1.0F) * kPi * 0.25F;
    return {std::cos(angle), std::sin(angle)};
}

float StereoCrossover::multiplier_value(
    CrossoverFrequencyMultiplier multiplier) noexcept {
    return multiplier == CrossoverFrequencyMultiplier::x10 ? 10.0F : 1.0F;
}

float StereoCrossover::maximum_effective_frequency_hz() const noexcept {
    return std::min(20000.0F, sample_rate_ * 0.45F);
}

void StereoCrossover::sanitize_parameters(
    StereoCrossoverParameters& parameters) const noexcept {
    if (parameters.frequency_multiplier != CrossoverFrequencyMultiplier::x1 &&
        parameters.frequency_multiplier != CrossoverFrequencyMultiplier::x10) {
        parameters.frequency_multiplier = CrossoverFrequencyMultiplier::x1;
    }
    const float multiplier = multiplier_value(parameters.frequency_multiplier);
    const float maximum_base_frequency = std::max(
        kMinimumFrequencyHz,
        std::min(kMaximumBaseFrequencyHz,
                 maximum_effective_frequency_hz() / multiplier));
    parameters.frequency_hz = bounded(parameters.frequency_hz, 500.0F,
                                       kMinimumFrequencyHz,
                                       maximum_base_frequency);
    const float maximum_level_db = kMaximumBandLevelDb;
    parameters.low_level_left_db = bounded(parameters.low_level_left_db, 0.0F,
                                            kMinimumBandLevelDb,
                                            maximum_level_db);
    parameters.low_level_right_db = bounded(parameters.low_level_right_db, 0.0F,
                                             kMinimumBandLevelDb,
                                             maximum_level_db);
    parameters.high_level_left_db = bounded(parameters.high_level_left_db, 0.0F,
                                             kMinimumBandLevelDb,
                                             maximum_level_db);
    parameters.high_level_right_db = bounded(parameters.high_level_right_db, 0.0F,
                                              kMinimumBandLevelDb,
                                              maximum_level_db);
    parameters.low_pan_left = bounded(parameters.low_pan_left, -1.0F, -1.0F, 1.0F);
    parameters.high_pan_left = bounded(parameters.high_pan_left, -1.0F, -1.0F, 1.0F);
    parameters.low_pan_right = bounded(parameters.low_pan_right, 1.0F, -1.0F, 1.0F);
    parameters.high_pan_right = bounded(parameters.high_pan_right, 1.0F, -1.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, 0.0F,
                                  kMinimumBandLevelDb, maximum_level_db);
    parameters.balance = bounded(parameters.balance, 0.0F, -1.0F, 1.0F);
}

void StereoCrossover::prepare_filters() noexcept {
    const float multiplier = multiplier_value(parameters_.frequency_multiplier);
    effective_frequency_hz_ = std::clamp(
        parameters_.frequency_hz * multiplier, kMinimumFrequencyHz,
        maximum_effective_frequency_hz());

    const float omega = kTwoPi * effective_frequency_hz_ / sample_rate_;
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    const float alpha = sine / (2.0F * kButterworthQ);
    const float inverse_a0 = 1.0F / (1.0F + alpha);

    lowpass_.b0 = (1.0F - cosine) * 0.5F * inverse_a0;
    lowpass_.b1 = (1.0F - cosine) * inverse_a0;
    lowpass_.b2 = lowpass_.b0;
    highpass_.b0 = (1.0F + cosine) * 0.5F * inverse_a0;
    highpass_.b1 = -(1.0F + cosine) * inverse_a0;
    highpass_.b2 = highpass_.b0;
    lowpass_.a1 = -2.0F * cosine * inverse_a0;
    lowpass_.a2 = (1.0F - alpha) * inverse_a0;
    highpass_.a1 = lowpass_.a1;
    highpass_.a2 = lowpass_.a2;
}

void StereoCrossover::prepare_gains() noexcept {
    low_level_gain_[0] = decibels_to_gain(parameters_.low_level_left_db);
    low_level_gain_[1] = decibels_to_gain(parameters_.low_level_right_db);
    high_level_gain_[0] = decibels_to_gain(parameters_.high_level_left_db);
    high_level_gain_[1] = decibels_to_gain(parameters_.high_level_right_db);
    low_pan_gain_[0] = pan_gains(parameters_.low_pan_left);
    low_pan_gain_[1] = pan_gains(parameters_.low_pan_right);
    high_pan_gain_[0] = pan_gains(parameters_.high_pan_left);
    high_pan_gain_[1] = pan_gains(parameters_.high_pan_right);
    output_level_gain_ = decibels_to_gain(parameters_.level_db);
    balance_left_gain_ = parameters_.balance > 0.0F
                             ? 1.0F - parameters_.balance
                             : 1.0F;
    balance_right_gain_ = parameters_.balance < 0.0F
                              ? 1.0F + parameters_.balance
                              : 1.0F;
}

void StereoCrossover::process(std::span<float> interleaved,
                              std::uint16_t channels) noexcept {
    if (channels == 0 || channels > 2) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        const float input_left = interleaved[offset];
        const float input_right = channels == 2 ? interleaved[offset + 1] : 0.0F;
        const std::array<float, 2> input{input_left, input_right};
        std::array<float, 2> low{};
        std::array<float, 2> high{};

        for (std::size_t channel = 0; channel < channels; ++channel) {
            if (!std::isfinite(input[channel])) {
                state_[channel].reset();
                continue;
            }
            const float bounded_input = std::clamp(
                input[channel], -kMaximumSample, kMaximumSample);
            ChannelState& state = state_[channel];
            low[channel] = state.low_second.process(
                state.low_first.process(bounded_input, lowpass_), lowpass_);
            high[channel] = state.high_second.process(
                state.high_first.process(bounded_input, highpass_), highpass_);
        }

        if (channels == 1) {
            const float mono = output_level_gain_ *
                (low[0] * low_level_gain_[0] +
                 high[0] * high_level_gain_[0]);
            interleaved[offset] = std::isfinite(mono)
                ? std::clamp(mono, -kMaximumSample, kMaximumSample)
                : 0.0F;
            continue;
        }

        float output_left =
            low[0] * low_level_gain_[0] * low_pan_gain_[0].left +
            high[0] * high_level_gain_[0] * high_pan_gain_[0].left +
            low[1] * low_level_gain_[1] * low_pan_gain_[1].left +
            high[1] * high_level_gain_[1] * high_pan_gain_[1].left;
        float output_right =
            low[0] * low_level_gain_[0] * low_pan_gain_[0].right +
            high[0] * high_level_gain_[0] * high_pan_gain_[0].right +
            low[1] * low_level_gain_[1] * low_pan_gain_[1].right +
            high[1] * high_level_gain_[1] * high_pan_gain_[1].right;
        output_left *= output_level_gain_ * balance_left_gain_;
        output_right *= output_level_gain_ * balance_right_gain_;
        interleaved[offset] = std::isfinite(output_left)
            ? std::clamp(output_left, -kMaximumSample, kMaximumSample)
            : 0.0F;
        interleaved[offset + 1] = std::isfinite(output_right)
            ? std::clamp(output_right, -kMaximumSample, kMaximumSample)
            : 0.0F;
    }
}

void StereoCrossover::reset() noexcept {
    for (ChannelState& channel : state_) {
        channel.reset();
    }
}

} // namespace tabletfx
