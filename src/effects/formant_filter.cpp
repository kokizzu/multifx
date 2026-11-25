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
constexpr float kMinimumControlLfoRateHz = 0.05F;
constexpr float kMaximumControlLfoRateHz = 8.0F;
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

// Separate, project-authored profiles for the ten vowel labels documented by
// Fractal. The guide names these choices but publishes no filter frequencies,
// gains, or DSP; these values are generic design material, not Fractal data.
constexpr float kMorphFrequencyHz[10][4] = {
    {800.0F, 1150.0F, 2900.0F, 3900.0F}, // AAA
    {550.0F, 1900.0F, 2700.0F, 3700.0F}, // EEE
    {300.0F, 2200.0F, 3000.0F, 3800.0F}, // III
    {550.0F, 900.0F, 2500.0F, 3500.0F},  // OHH
    {350.0F, 850.0F, 2200.0F, 3300.0F},  // OOO
    {530.0F, 1700.0F, 2500.0F, 3500.0F}, // EHH
    {900.0F, 1250.0F, 2700.0F, 3800.0F}, // AHH
    {600.0F, 850.0F, 2400.0F, 3500.0F},  // AWW
    {650.0F, 1300.0F, 2350.0F, 3300.0F}, // UHH
    {500.0F, 1350.0F, 1850.0F, 2800.0F}, // ERR
};

constexpr float kMorphGainDb[10][4] = {
    {8.0F, 5.0F, 7.0F, 3.0F},
    {6.0F, 8.0F, 6.0F, 3.0F},
    {4.0F, 9.0F, 7.0F, 3.0F},
    {6.0F, 6.0F, 8.0F, 4.0F},
    {4.0F, 5.0F, 6.0F, 4.0F},
    {6.0F, 8.0F, 6.0F, 3.0F},
    {8.0F, 5.0F, 7.0F, 3.0F},
    {6.0F, 5.0F, 8.0F, 4.0F},
    {4.0F, 5.0F, 6.0F, 4.0F},
    {6.0F, 7.0F, 6.0F, 3.0F},
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
    balance_left_gain_ = target_balance_left_gain_;
    balance_right_gain_ = target_balance_right_gain_;
    for (auto& bank : coefficient_steps_) {
        bank.fill(BiquadCoefficients{});
    }
    mix_step_ = 0.0F;
    level_gain_step_ = 0.0F;
    balance_left_gain_step_ = 0.0F;
    balance_right_gain_step_ = 0.0F;
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
    const auto clamp_vowel = [](FormantVowel vowel) noexcept {
        return static_cast<std::size_t>(vowel) < 10U ? vowel : FormantVowel::aaa;
    };
    parameters.start_vowel = clamp_vowel(parameters.start_vowel);
    parameters.middle_vowel = clamp_vowel(parameters.middle_vowel);
    parameters.end_vowel = clamp_vowel(parameters.end_vowel);
    parameters.control_percent = bounded(parameters.control_percent, 0.0F,
                                          0.0F, 100.0F);
    parameters.balance_percent = bounded(parameters.balance_percent, 0.0F,
                                          -100.0F, 100.0F);
    switch (parameters.control_lfo_waveform) {
    case FormantControlLfoWaveform::sine:
    case FormantControlLfoWaveform::triangle:
    case FormantControlLfoWaveform::square:
        break;
    default:
        parameters.control_lfo_waveform = FormantControlLfoWaveform::sine;
        break;
    }
    parameters.control_lfo_rate_hz = bounded(
        parameters.control_lfo_rate_hz, 0.5F, kMinimumControlLfoRateHz,
        kMaximumControlLfoRateHz);
    parameters.control_lfo_min_percent = bounded(
        parameters.control_lfo_min_percent, 0.0F, 0.0F, 100.0F);
    parameters.control_lfo_max_percent = bounded(
        parameters.control_lfo_max_percent, 100.0F, 0.0F, 100.0F);
    parameters_ = parameters;
    control_lfo_phase_step_ = parameters_.control_lfo_rate_hz / sample_rate_;

    const float control_percent = parameters_.three_vowel_morph &&
                                  parameters_.control_lfo_enabled
        ? control_lfo_value()
        : parameters_.control_percent;
    set_formant_coefficients(control_percent);

    target_mix_ = parameters_.mix;
    target_level_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    const float balance = parameters_.balance_percent / 100.0F;
    target_balance_left_gain_ = balance > 0.0F ? 1.0F - balance : 1.0F;
    target_balance_right_gain_ = balance < 0.0F ? 1.0F + balance : 1.0F;
    ramp_remaining_ = ramp_total_;
    mix_step_ = (target_mix_ - mix_) / static_cast<float>(ramp_total_);
    level_gain_step_ = (target_level_gain_ - level_gain_) /
                       static_cast<float>(ramp_total_);
    balance_left_gain_step_ = (target_balance_left_gain_ - balance_left_gain_) /
                              static_cast<float>(ramp_total_);
    balance_right_gain_step_ = (target_balance_right_gain_ - balance_right_gain_) /
                               static_cast<float>(ramp_total_);
}

void FormantFilter::set_formant_coefficients(float control_percent) noexcept {
    control_percent = bounded(control_percent, 0.0F, 0.0F, 100.0F);
    std::size_t lower = 0U;
    std::size_t upper = 0U;
    float fraction = 0.0F;
    if (parameters_.three_vowel_morph) {
        const float morph = control_percent / 100.0F;
        if (morph <= 0.5F) {
            lower = static_cast<std::size_t>(parameters_.start_vowel);
            upper = static_cast<std::size_t>(parameters_.middle_vowel);
            fraction = morph * 2.0F;
        } else {
            lower = static_cast<std::size_t>(parameters_.middle_vowel);
            upper = static_cast<std::size_t>(parameters_.end_vowel);
            fraction = (morph - 0.5F) * 2.0F;
        }
    } else {
        lower = std::min(static_cast<std::size_t>(parameters_.vowel_position),
                         kProfileCount - 1U);
        upper = std::min(lower + 1U, kProfileCount - 1U);
        fraction = parameters_.vowel_position - static_cast<float>(lower);
    }
    for (std::size_t formant = 0; formant < kFormantCount; ++formant) {
        // Interpolate center frequency on a log scale to make the continuous
        // vowel path perceptually more even than linear-Hz interpolation.
        const float lower_frequency = parameters_.three_vowel_morph
            ? kMorphFrequencyHz[lower][formant]
            : kProfileFrequencyHz[lower][formant];
        const float upper_frequency = parameters_.three_vowel_morph
            ? kMorphFrequencyHz[upper][formant]
            : kProfileFrequencyHz[upper][formant];
        const float lower_gain = parameters_.three_vowel_morph
            ? kMorphGainDb[lower][formant]
            : kProfileGainDb[lower][formant];
        const float upper_gain = parameters_.three_vowel_morph
            ? kMorphGainDb[upper][formant]
            : kProfileGainDb[upper][formant];
        const float lower_log = std::log(lower_frequency);
        const float upper_log = std::log(upper_frequency);
        const float frequency = std::exp(lower_log + (upper_log - lower_log) * fraction);
        const float gain = lower_gain + (upper_gain - lower_gain) * fraction;
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

    ramp_remaining_ = ramp_total_;
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

float FormantFilter::control_lfo_value() const noexcept {
    float bipolar = 0.0F;
    switch (parameters_.control_lfo_waveform) {
    case FormantControlLfoWaveform::sine:
        bipolar = std::sin(2.0F * kPi * control_lfo_phase_ - 0.5F * kPi);
        break;
    case FormantControlLfoWaveform::triangle:
        bipolar = control_lfo_phase_ < 0.5F
            ? -1.0F + 4.0F * control_lfo_phase_
            : 3.0F - 4.0F * control_lfo_phase_;
        break;
    case FormantControlLfoWaveform::square:
        bipolar = control_lfo_phase_ < 0.5F ? -1.0F : 1.0F;
        break;
    }
    const float normalized = 0.5F * (bipolar + 1.0F);
    return parameters_.control_lfo_min_percent +
           (parameters_.control_lfo_max_percent -
            parameters_.control_lfo_min_percent) * normalized;
}

void FormantFilter::process(std::span<float> interleaved,
                            std::uint16_t channels) noexcept {
    if (channels == 0U || channels > 2U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    const bool control_lfo_active = parameters_.three_vowel_morph &&
                                    parameters_.control_lfo_enabled;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (control_lfo_active && control_lfo_update_remaining_ == 0U) {
            set_formant_coefficients(control_lfo_value());
            control_lfo_update_remaining_ = kControlLfoUpdateFrames;
        }
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
            } else if (mix <= 0.0F && level == 1.0F &&
                       balance_left_gain_ == 1.0F &&
                       balance_right_gain_ == 1.0F) {
                sample = original;
            } else {
                const float balance = channels == 2U
                    ? (channel == 0U ? balance_left_gain_ : balance_right_gain_)
                    : 1.0F;
                sample = ((input * (1.0F - mix)) + (wet * mix)) * level * balance;
            }
        }
        advance_parameters();
        if (control_lfo_active) {
            control_lfo_phase_ += control_lfo_phase_step_;
            if (control_lfo_phase_ >= 1.0F) {
                control_lfo_phase_ -= 1.0F;
            }
            if (control_lfo_update_remaining_ > 0U) {
                --control_lfo_update_remaining_;
            }
        }
    }
}

void FormantFilter::reset() noexcept {
    for (auto& channel : states_) {
        channel.fill(BiquadState{});
    }
    control_lfo_phase_ = 0.0F;
    control_lfo_update_remaining_ = 0U;
    if (parameters_.three_vowel_morph && parameters_.control_lfo_enabled) {
        set_formant_coefficients(control_lfo_value());
    }
    coefficients_ = target_coefficients_;
    mix_ = target_mix_;
    level_gain_ = target_level_gain_;
    balance_left_gain_ = target_balance_left_gain_;
    balance_right_gain_ = target_balance_right_gain_;
    for (auto& bank : coefficient_steps_) {
        bank.fill(BiquadCoefficients{});
    }
    mix_step_ = 0.0F;
    level_gain_step_ = 0.0F;
    balance_left_gain_step_ = 0.0F;
    balance_right_gain_step_ = 0.0F;
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
        balance_left_gain_ = target_balance_left_gain_;
        balance_right_gain_ = target_balance_right_gain_;
        for (auto& bank : coefficient_steps_) {
            bank.fill(BiquadCoefficients{});
        }
        mix_step_ = 0.0F;
        level_gain_step_ = 0.0F;
        balance_left_gain_step_ = 0.0F;
        balance_right_gain_step_ = 0.0F;
        return;
    }

    mix_ += mix_step_;
    level_gain_ += level_gain_step_;
    balance_left_gain_ += balance_left_gain_step_;
    balance_right_gain_ += balance_right_gain_step_;
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
