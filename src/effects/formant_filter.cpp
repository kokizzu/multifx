#include "tabletfx/effects/formant_filter.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kDefaultQ = 4.0F;
constexpr float kDefaultLevelDb = 0.0F;
constexpr float kMaximumLevelDb = 12.0F;
constexpr float kMinimumLevelDb = -24.0F;
constexpr float kPi = 3.14159265358979323846F;

// The rows are generic, project-authored A/E/I/O/U formant profiles in Hz.
// The gain rows shape their relative emphasis in dB and are not manufacturer
// presets or speech synthesis data.
constexpr float kProfileFrequencyHz[5][4] = {
    {800.0F, 1150.0F, 2900.0F, 3900.0F}, // A
    {400.0F, 1700.0F, 2600.0F, 3600.0F}, // E
    {300.0F, 2200.0F, 3000.0F, 3800.0F}, // I
    {500.0F, 900.0F, 2600.0F, 3600.0F},  // O
    {350.0F, 850.0F, 2200.0F, 3300.0F},  // U
};

constexpr float kProfileGainDb[5][4] = {
    {8.0F, 5.0F, 7.0F, 3.0F},
    {6.0F, 8.0F, 6.0F, 3.0F},
    {4.0F, 9.0F, 7.0F, 3.0F},
    {6.0F, 5.0F, 8.0F, 4.0F},
    {4.0F, 5.0F, 6.0F, 4.0F},
};

} // namespace

FormantFilter::FormantFilter(float sample_rate,
                             FormantFilterParameters parameters) noexcept {
    const bool valid_rate = std::isfinite(sample_rate) &&
                            sample_rate >= kMinimumSampleRate &&
                            sample_rate <= kMaximumSampleRate;
    sample_rate_ = valid_rate ? sample_rate : kDefaultSampleRate;
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(
                static_cast<double>(sample_rate_) * kParameterSmoothingMs /
                1000.0)));
    set_parameters(parameters);
    coefficients_ = target_coefficients_;
    mix_ = target_mix_;
    level_gain_ = target_level_gain_;
    for (auto& bank : coefficient_steps_) {
        bank.fill(BiquadCoefficients{});
    }
    mix_step_ = 0.0F;
    level_gain_step_ = 0.0F;
    ramp_remaining_ = 0U;
}

void FormantFilter::set_parameters(FormantFilterParameters parameters) noexcept {
    parameters.vowel_position = bounded(parameters.vowel_position, 0.0F, 0.0F,
                                        static_cast<float>(kProfileCount - 1U));
    parameters.resonance_q = bounded(parameters.resonance_q, kDefaultQ,
                                      kMinimumQ, kMaximumQ);
    parameters.mix = bounded(parameters.mix, 1.0F, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, kDefaultLevelDb,
                                  kMinimumLevelDb, kMaximumLevelDb);
    parameters_ = parameters;

    const std::size_t lower = std::min(
        static_cast<std::size_t>(parameters_.vowel_position),
        kProfileCount - 1U);
    const std::size_t upper = std::min(lower + 1U, kProfileCount - 1U);
    const float fraction = parameters_.vowel_position -
                           static_cast<float>(lower);
    for (std::size_t formant = 0; formant < kFormantCount; ++formant) {
        // Interpolate center frequency on a log scale to make the continuous
        // vowel path perceptually more even than linear-Hz interpolation.
        const float lower_log = std::log(kProfileFrequencyHz[lower][formant]);
        const float upper_log = std::log(kProfileFrequencyHz[upper][formant]);
        const float frequency = std::exp(lower_log + (upper_log - lower_log) * fraction);
        const float gain = kProfileGainDb[lower][formant] +
                           (kProfileGainDb[upper][formant] -
                            kProfileGainDb[lower][formant]) * fraction;
        const float maximum_frequency = std::min(18000.0F, sample_rate_ * 0.40F);
        const float bounded_frequency = std::clamp(frequency, 20.0F,
                                                    maximum_frequency);
        const BiquadCoefficients target = make_coefficients(
            bounded_frequency, parameters_.resonance_q, gain);
        for (std::size_t channel = 0; channel < target_coefficients_.size();
             ++channel) {
            target_coefficients_[channel][formant] = target;
        }
    }

    target_mix_ = parameters_.mix;
    target_level_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    ramp_remaining_ = ramp_total_;
    mix_step_ = (target_mix_ - mix_) / static_cast<float>(ramp_total_);
    level_gain_step_ = (target_level_gain_ - level_gain_) /
                       static_cast<float>(ramp_total_);
    for (std::size_t channel = 0; channel < coefficient_steps_.size(); ++channel) {
        for (std::size_t formant = 0; formant < kFormantCount; ++formant) {
            const auto& current = coefficients_[channel][formant];
            const auto& target = target_coefficients_[channel][formant];
            auto& step = coefficient_steps_[channel][formant];
            step.b0 = (target.b0 - current.b0) / static_cast<float>(ramp_total_);
            step.b1 = (target.b1 - current.b1) / static_cast<float>(ramp_total_);
            step.b2 = (target.b2 - current.b2) / static_cast<float>(ramp_total_);
            step.a1 = (target.a1 - current.a1) / static_cast<float>(ramp_total_);
            step.a2 = (target.a2 - current.a2) / static_cast<float>(ramp_total_);
        }
    }
}

void FormantFilter::process(std::span<float> interleaved,
                            std::uint16_t channels) noexcept {
    if (channels == 0U || channels > 2U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float mix = mix_;
        const float level = level_gain_;
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            if (!std::isfinite(sample)) {
                for (auto& state : states_[channel]) {
                    state = {};
                }
                sample = 0.0F;
                continue;
            }

            const float original = sample;
            const float input = std::clamp(original, -kMaximumInputMagnitude,
                                            kMaximumInputMagnitude);
            const float wet = filter_sample(input, channel);
            if (!std::isfinite(wet)) {
                for (auto& state : states_[channel]) {
                    state = {};
                }
                sample = 0.0F;
            } else if (mix <= 0.0F && level == 1.0F) {
                sample = original;
            } else {
                sample = ((input * (1.0F - mix)) + (wet * mix)) * level;
            }
        }
        advance_parameters();
    }
}

void FormantFilter::reset() noexcept {
    for (auto& channel : states_) {
        channel.fill(BiquadState{});
    }
    coefficients_ = target_coefficients_;
    mix_ = target_mix_;
    level_gain_ = target_level_gain_;
    for (auto& bank : coefficient_steps_) {
        bank.fill(BiquadCoefficients{});
    }
    mix_step_ = 0.0F;
    level_gain_step_ = 0.0F;
    ramp_remaining_ = 0U;
}

float FormantFilter::bounded(float value, float fallback, float minimum,
                             float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

FormantFilter::BiquadCoefficients FormantFilter::make_coefficients(
    float frequency_hz, float q, float gain_db) const noexcept {
    const float a = std::pow(10.0F, gain_db / 40.0F);
    const float omega = 2.0F * kPi * frequency_hz / sample_rate_;
    const float cosine = std::cos(omega);
    const float alpha = std::sin(omega) / (2.0F * q);
    const float a0 = 1.0F + alpha / a;
    return {
        (1.0F + alpha * a) / a0,
        (-2.0F * cosine) / a0,
        (1.0F - alpha * a) / a0,
        (-2.0F * cosine) / a0,
        (1.0F - alpha / a) / a0,
    };
}

float FormantFilter::filter_sample(float input, std::uint16_t channel) noexcept {
    float output = input;
    for (std::size_t formant = 0; formant < kFormantCount; ++formant) {
        const auto& coefficients = coefficients_[channel][formant];
        auto& state = states_[channel][formant];
        const float filtered = coefficients.b0 * output + state.s1;
        state.s1 = coefficients.b1 * output - coefficients.a1 * filtered + state.s2;
        state.s2 = coefficients.b2 * output - coefficients.a2 * filtered;
        if (!std::isfinite(filtered) || !std::isfinite(state.s1) ||
            !std::isfinite(state.s2) ||
            std::abs(state.s1) > kMaximumStateMagnitude ||
            std::abs(state.s2) > kMaximumStateMagnitude) {
            state = {};
            return 0.0F;
        }
        output = filtered;
    }
    return output;
}

void FormantFilter::advance_parameters() noexcept {
    if (ramp_remaining_ == 0U) {
        return;
    }
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) {
        coefficients_ = target_coefficients_;
        mix_ = target_mix_;
        level_gain_ = target_level_gain_;
        for (auto& bank : coefficient_steps_) {
            bank.fill(BiquadCoefficients{});
        }
        mix_step_ = 0.0F;
        level_gain_step_ = 0.0F;
        return;
    }

    mix_ += mix_step_;
    level_gain_ += level_gain_step_;
    for (std::size_t channel = 0; channel < coefficients_.size(); ++channel) {
        for (std::size_t formant = 0; formant < kFormantCount; ++formant) {
            auto& current = coefficients_[channel][formant];
            const auto& step = coefficient_steps_[channel][formant];
            current.b0 += step.b0;
            current.b1 += step.b1;
            current.b2 += step.b2;
            current.a1 += step.a1;
            current.a2 += step.a2;
        }
    }
}

} // namespace tabletfx
