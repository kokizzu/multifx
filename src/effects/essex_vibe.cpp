#include "tabletfx/effects/essex_vibe.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTwoPi = 2.0F * kPi;
constexpr float kLogTwo = 0.69314718055994530942F;

} // namespace

EssexVibe::EssexVibe(float sample_rate,
                     EssexVibeParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

float EssexVibe::bounded(float value, float fallback, float minimum,
                         float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum)
                                : fallback;
}

void EssexVibe::configure(float sample_rate,
                          EssexVibeParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;

    parameters.rate_hz = bounded(parameters.rate_hz, 0.8F,
                                 kMinimumRateHz, kMaximumRateHz);
    parameters.depth = bounded(parameters.depth, 0.65F, 0.0F, 1.0F);
    parameters.trem = bounded(parameters.trem, 0.0F, 0.0F, 1.0F);
    parameters.stages = parameters.stages == 1U ? 1U : 2U;
    parameters.spread = bounded(parameters.spread, 0.0F, 0.0F, 1.0F);
    parameters.level = bounded(parameters.level, 1.0F, 0.0F,
                                kMaximumLevel);
    parameters_ = parameters;

    prepare_coefficients();
    prepare_oscillator();
    const float spread_radians = kPi * parameters_.spread;
    spread_sine_ = std::sin(spread_radians);
    spread_cosine_ = std::cos(spread_radians);
    reset();
}

void EssexVibe::prepare_coefficients() noexcept {
    constexpr float minimum_hz = kMinimumCenterHz / 4.0F;
    const float maximum_hz = std::min(kMaximumSweepHz, sample_rate_ * 0.45F);
    const float safe_maximum_hz = std::max(minimum_hz * 2.0F, maximum_hz);
    const float octave_span = std::log(safe_maximum_hz / minimum_hz) / kLogTwo;

    for (std::size_t index = 0U; index < kCoefficientCount; ++index) {
        const float position = static_cast<float>(index) /
                               static_cast<float>(kCoefficientCount - 1U);
        const float frequency = minimum_hz *
            std::exp2(position * octave_span);
        const float tangent = std::tan(kPi * frequency / sample_rate_);
        coefficient_table_[index] = (tangent - 1.0F) / (tangent + 1.0F);
    }
}

void EssexVibe::prepare_oscillator() noexcept {
    const float radians = kTwoPi * parameters_.rate_hz / sample_rate_;
    sine_increment_ = std::sin(radians);
    cosine_increment_ = std::cos(radians);
}

float EssexVibe::coefficient_for(float lfo_value) const noexcept {
    const float center = 0.5F *
                         static_cast<float>(kCoefficientCount - 1U);
    const float table_position = std::clamp(
        center + lfo_value * parameters_.depth * center, 0.0F, center * 2.0F);
    const auto lower_index = static_cast<std::size_t>(table_position);
    const auto upper_index = std::min(lower_index + 1U,
                                      kCoefficientCount - 1U);
    const float fraction = table_position - static_cast<float>(lower_index);
    return coefficient_table_[lower_index] +
           fraction * (coefficient_table_[upper_index] -
                       coefficient_table_[lower_index]);
}

void EssexVibe::process(std::span<float> interleaved,
                        std::uint16_t channels) noexcept {
    if (channels == 0U || channels > 2U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const float left_lfo = sine_phase_;
        const float right_lfo = channels == 2U
            ? left_lfo * spread_cosine_ + cosine_phase_ * spread_sine_
            : left_lfo;
        const std::size_t offset = frame * channels;

        for (std::uint16_t channel = 0U; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            const float dry = std::isfinite(sample) ? sample : 0.0F;
            const float channel_lfo = channel == 0U ? left_lfo : right_lfo;
            const float coefficient = coefficient_for(channel_lfo);

            float wet = dry;
            auto& channel_state = state_[channel];
            for (std::uint8_t stage = 0U; stage < parameters_.stages; ++stage) {
                auto& filter = channel_state[stage];
                const float output = coefficient * wet +
                    filter.previous_input - coefficient * filter.previous_output;
                filter.previous_input = wet;
                filter.previous_output = std::isfinite(output) ? output : 0.0F;
                wet = filter.previous_output;
            }

            // The fixed 50/50 dry/wet blend is a project choice because the
            // documented Essex Vibe controls do not include a Mix control.
            const float tremolo_gain = 1.0F - parameters_.trem *
                (1.0F - channel_lfo) * 0.5F;
            const float processed =
                (0.5F * dry + 0.5F * wet) * tremolo_gain * parameters_.level;
            sample = std::isfinite(processed) ? processed : 0.0F;
        }

        const float old_sine = sine_phase_;
        sine_phase_ = old_sine * cosine_increment_ +
                      cosine_phase_ * sine_increment_;
        cosine_phase_ = cosine_phase_ * cosine_increment_ -
                         old_sine * sine_increment_;
        if (++normalization_counter_ == 4096U) {
            const float magnitude_squared = sine_phase_ * sine_phase_ +
                                            cosine_phase_ * cosine_phase_;
            if (std::isfinite(magnitude_squared) && magnitude_squared > 0.0F) {
                const float inverse_magnitude = 1.0F / std::sqrt(magnitude_squared);
                sine_phase_ *= inverse_magnitude;
                cosine_phase_ *= inverse_magnitude;
            } else {
                sine_phase_ = 0.0F;
                cosine_phase_ = 1.0F;
            }
            normalization_counter_ = 0U;
        }
    }
}

void EssexVibe::reset_modulation_phase() noexcept {
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    normalization_counter_ = 0U;
}

void EssexVibe::reset() noexcept {
    state_ = {};
    reset_modulation_phase();
}

} // namespace tabletfx
