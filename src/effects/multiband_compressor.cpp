#include "tabletfx/effects/multiband_compressor.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tabletfx {

namespace {

constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kMinimumCrossoverSpacingHz = 10.0F;
constexpr float kMinimumCrossoverRatio = 1.5F;
constexpr float kMaximumOutputMagnitude = 64.0F;
constexpr float kButterworthQ = 0.7071067811865475F;
constexpr float kLinkwitzRiley2Q = 0.5F;
constexpr float kPi = std::numbers::pi_v<float>;

} // namespace

MultibandCompressor::MultibandCompressor(
    float sample_rate, MultibandCompressorParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

float MultibandCompressor::bounded(float value, float fallback, float minimum,
                                   float maximum) noexcept {
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return std::clamp(value, minimum, maximum);
}

MultibandCrossoverSlope MultibandCompressor::sanitize_slope(
    MultibandCrossoverSlope slope) noexcept {
    switch (slope) {
    case MultibandCrossoverSlope::Db12:
    case MultibandCrossoverSlope::Db24:
        return slope;
    }
    return MultibandCrossoverSlope::Db24;
}

MultibandDetector MultibandCompressor::sanitize_detector(
    MultibandDetector detector) noexcept {
    switch (detector) {
    case MultibandDetector::Peak:
    case MultibandDetector::Rms:
        return detector;
    }
    return MultibandDetector::Rms;
}

float MultibandCompressor::decibels_to_linear(float db) noexcept {
    if (!std::isfinite(db)) {
        return 1.0F;
    }
    return std::pow(10.0F, db * 0.05F);
}

float MultibandCompressor::time_coefficient(float milliseconds,
                                             float sample_rate) noexcept {
    const float seconds = milliseconds * 0.001F;
    if (!std::isfinite(seconds) || seconds <= 0.0F ||
        !std::isfinite(sample_rate) || sample_rate <= 0.0F) {
        return 0.0F;
    }
    return std::exp(-1.0F / (seconds * sample_rate));
}

float MultibandCompressor::process_biquad(
    BiquadState& state, const BiquadCoefficients& coefficients,
    float input) noexcept {
    if (!std::isfinite(input)) {
        input = 0.0F;
        state = {};
    }

    const float output = coefficients.b0 * input + state.z1;
    if (!std::isfinite(output)) {
        state = {};
        return 0.0F;
    }

    const float next_z1 = coefficients.b1 * input -
                          coefficients.a1 * output + state.z2;
    const float next_z2 = coefficients.b2 * input -
                          coefficients.a2 * output;
    state.z1 = std::isfinite(next_z1)
                   ? std::clamp(next_z1, -kMaximumStateMagnitude,
                                kMaximumStateMagnitude)
                   : 0.0F;
    state.z2 = std::isfinite(next_z2)
                   ? std::clamp(next_z2, -kMaximumStateMagnitude,
                                kMaximumStateMagnitude)
                   : 0.0F;

    if (std::abs(state.z1) < kDenormalGuard) {
        state.z1 = 0.0F;
    }
    if (std::abs(state.z2) < kDenormalGuard) {
        state.z2 = 0.0F;
    }
    return std::clamp(output, -kMaximumStateMagnitude,
                      kMaximumStateMagnitude);
}

float MultibandCompressor::apply_compression_knee(
    float level_db, float threshold_db, float ratio, float knee_db) noexcept {
    if (!std::isfinite(level_db) || !std::isfinite(threshold_db) ||
        !std::isfinite(ratio) || !std::isfinite(knee_db)) {
        return 0.0F;
    }
    const float slope = 1.0F / std::clamp(ratio, 1.0F, 20.0F) - 1.0F;
    if (slope == 0.0F) {
        return 0.0F;
    }
    if (knee_db <= 0.0F) {
        return level_db > threshold_db
                   ? slope * (level_db - threshold_db)
                   : 0.0F;
    }

    const float knee_low = threshold_db - 0.5F * knee_db;
    const float knee_high = threshold_db + 0.5F * knee_db;
    if (level_db <= knee_low) {
        return 0.0F;
    }
    if (level_db >= knee_high) {
        return slope * (level_db - threshold_db);
    }
    const float knee_offset = level_db - knee_low;
    return slope * knee_offset * knee_offset / (2.0F * knee_db);
}

float MultibandCompressor::target_band_gain(
    std::size_t band, float detector_level) const noexcept {
    if (band >= kBandCount || !std::isfinite(detector_level) ||
        detector_level <= 0.0F) {
        return band < kBandCount ? band_state_[band].level_gain : 1.0F;
    }

    const auto& parameters = parameters_.bands[band];
    const auto& state = band_state_[band];
    const float level_db = 20.0F * std::log10(detector_level);
    float attenuation_db = 0.0F;
    const bool above_knee_start =
        parameters.knee_db == 0.0F
            ? detector_level > state.threshold_linear
            : detector_level > state.knee_low_linear;
    if (above_knee_start) {
        attenuation_db = apply_compression_knee(
            level_db, parameters.threshold_db, parameters.ratio,
            parameters.knee_db);
    }
    return state.level_gain * decibels_to_linear(attenuation_db);
}

void MultibandCompressor::configure(
    float sample_rate, MultibandCompressorParameters parameters) noexcept {
    sample_rate_ = bounded(sample_rate, kDefaultSampleRate,
                           kMinimumSampleRate, kMaximumSampleRate);
    prepare_parameters(parameters);
    reset();
}

void MultibandCompressor::set_parameters(
    MultibandCompressorParameters parameters) noexcept {
    const auto old_parameters = parameters_;
    prepare_parameters(parameters);
    if (old_parameters.low_mid_crossover_hz !=
            parameters_.low_mid_crossover_hz ||
        old_parameters.mid_high_crossover_hz !=
            parameters_.mid_high_crossover_hz ||
        old_parameters.crossover_slope != parameters_.crossover_slope) {
        reset();
    }
}

void MultibandCompressor::prepare_parameters(
    MultibandCompressorParameters parameters) noexcept {
    parameters.crossover_slope = sanitize_slope(parameters.crossover_slope);

    // The manufacturer-published crossover ranges are retained. The 0.4 x
    // sample-rate ceiling and 10 Hz separation are project-authored limits.
    const float sample_rate_ceiling = 0.4F * sample_rate_;
    const float low_max = std::min(5000.0F, sample_rate_ceiling);
    const float high_max = std::min(10000.0F, sample_rate_ceiling);
    parameters.low_mid_crossover_hz = bounded(
        parameters.low_mid_crossover_hz, 250.0F, 50.0F, low_max);
    parameters.mid_high_crossover_hz = bounded(
        parameters.mid_high_crossover_hz, 2500.0F, 100.0F, high_max);
    const float required_high = std::max(
        parameters.low_mid_crossover_hz * kMinimumCrossoverRatio,
        parameters.low_mid_crossover_hz + kMinimumCrossoverSpacingHz);
    if (parameters.mid_high_crossover_hz < required_high) {
        parameters.mid_high_crossover_hz = std::min(high_max, required_high);
        if (parameters.mid_high_crossover_hz < required_high) {
            parameters.low_mid_crossover_hz = std::max(
                50.0F,
                std::min(parameters.low_mid_crossover_hz,
                         std::min(parameters.mid_high_crossover_hz /
                                      kMinimumCrossoverRatio,
                                  parameters.mid_high_crossover_hz -
                                      kMinimumCrossoverSpacingHz)));
        }
    }

    for (auto& band : parameters.bands) {
        band.threshold_db = bounded(band.threshold_db, -24.0F, -60.0F, 0.0F);
        band.ratio = bounded(band.ratio, 2.0F, 1.0F, 20.0F);
        band.knee_db = bounded(band.knee_db, 6.0F, 0.0F, 18.0F);
        band.attack_ms = bounded(band.attack_ms, 10.0F, 0.1F, 1000.0F);
        band.release_ms = bounded(band.release_ms, 100.0F, 5.0F, 2000.0F);
        band.level_db = bounded(band.level_db, 0.0F, -24.0F, 12.0F);
        band.detector = sanitize_detector(band.detector);
    }

    parameters_ = parameters;
    prepare_crossovers();
    for (std::size_t band = 0; band < kBandCount; ++band) {
        prepare_band(band);
    }
}

void MultibandCompressor::prepare_crossovers() noexcept {
    low_mid_lowpass_ = {};
    low_mid_highpass_ = {};
    mid_high_lowpass_ = {};
    mid_high_highpass_ = {};

    const auto second_order = [this](float frequency, bool lowpass, float q) {
        BiquadCoefficients result{};
        const float omega = 2.0F * kPi * frequency / sample_rate_;
        const float cosine = std::cos(omega);
        const float sine = std::sin(omega);
        const float alpha = sine / (2.0F * q);
        const float a0 = 1.0F + alpha;
        if (lowpass) {
            result.b0 = (1.0F - cosine) / (2.0F * a0);
            result.b1 = (1.0F - cosine) / a0;
            result.b2 = result.b0;
        } else {
            result.b0 = (1.0F + cosine) / (2.0F * a0);
            result.b1 = -(1.0F + cosine) / a0;
            result.b2 = result.b0;
        }
        result.a1 = -2.0F * cosine / a0;
        result.a2 = (1.0F - alpha) / a0;
        return result;
    };

    const auto prepare_one = [&](float frequency,
                                 std::array<BiquadCoefficients,
                                            kSectionsPerCrossover>& lowpass,
                                 std::array<BiquadCoefficients,
                                            kSectionsPerCrossover>& highpass) {
        if (parameters_.crossover_slope == MultibandCrossoverSlope::Db12) {
            lowpass[0] = second_order(frequency, true, kLinkwitzRiley2Q);
            highpass[0] = second_order(frequency, false, kLinkwitzRiley2Q);
            // The second section's default coefficients are an identity.
        } else {
            for (std::size_t section = 0;
                 section < kSectionsPerCrossover; ++section) {
                lowpass[section] = second_order(frequency, true,
                                                kButterworthQ);
                highpass[section] = second_order(frequency, false,
                                                 kButterworthQ);
            }
        }
    };

    prepare_one(parameters_.low_mid_crossover_hz, low_mid_lowpass_,
                low_mid_highpass_);
    prepare_one(parameters_.mid_high_crossover_hz, mid_high_lowpass_,
                mid_high_highpass_);
}

void MultibandCompressor::prepare_band(std::size_t band) noexcept {
    const auto& parameters = parameters_.bands[band];
    auto& state = band_state_[band];
    state.attack_coefficient = time_coefficient(parameters.attack_ms,
                                                sample_rate_);
    state.release_coefficient = time_coefficient(parameters.release_ms,
                                                 sample_rate_);
    state.rms_coefficient = time_coefficient(kRmsWindowMilliseconds,
                                             sample_rate_);
    state.threshold_linear = decibels_to_linear(parameters.threshold_db);
    state.knee_low_linear =
        decibels_to_linear(parameters.threshold_db -
                           0.5F * parameters.knee_db);
    state.knee_high_linear =
        decibels_to_linear(parameters.threshold_db +
                           0.5F * parameters.knee_db);
    state.compression_slope = 1.0F / parameters.ratio - 1.0F;
    state.level_gain = decibels_to_linear(parameters.level_db);
}

void MultibandCompressor::reset_channel(std::uint16_t channel) noexcept {
    if (channel >= kChannelCount) {
        return;
    }
    low_mid_lowpass_state_[channel] = {};
    low_mid_highpass_state_[channel] = {};
    mid_high_lowpass_state_[channel] = {};
    mid_high_highpass_state_[channel] = {};
    low_branch_mid_high_lowpass_state_[channel] = {};
    low_branch_mid_high_highpass_state_[channel] = {};
}

void MultibandCompressor::process_split(
    float input,
    std::array<BiquadState, kSectionsPerCrossover>& low_states,
    std::array<BiquadState, kSectionsPerCrossover>& high_states,
    const std::array<BiquadCoefficients, kSectionsPerCrossover>& low_coefficients,
    const std::array<BiquadCoefficients, kSectionsPerCrossover>& high_coefficients,
    float& low, float& high) noexcept {
    low = input;
    high = input;
    for (std::size_t section = 0; section < kSectionsPerCrossover; ++section) {
        low = process_biquad(low_states[section], low_coefficients[section], low);
        high = process_biquad(high_states[section], high_coefficients[section],
                              high);
    }
    if (parameters_.crossover_slope == MultibandCrossoverSlope::Db12) {
        // A single critically damped second-order high-pass is 12 dB/oct.
        // The project-authored LR2 sum requires its high branch polarity flip.
        high = -high;
    }
}

void MultibandCompressor::process(std::span<float> interleaved,
                                  std::uint16_t channels) noexcept {
    if (channels == 0 || channels > kChannelCount) {
        return;
    }
    const std::size_t frame_count = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        std::array<float, kChannelCount> input{};
        std::array<std::array<float, kBandCount>, kChannelCount> bands{};
        std::array<float, kBandCount> peak_measurement{};
        std::array<float, kBandCount> power_measurement{};
        bool invalid_frame = false;

        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const float value = interleaved[frame * channels + channel];
            if (!std::isfinite(value)) {
                invalid_frame = true;
                break;
            }
            input[channel] = std::clamp(value, -kMaximumInputMagnitude,
                                        kMaximumInputMagnitude);
        }
        if (invalid_frame) {
            reset();
            for (std::uint16_t channel = 0; channel < channels; ++channel) {
                interleaved[frame * channels + channel] = 0.0F;
            }
            continue;
        }

        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float low = 0.0F;
            float high_after_low_mid = 0.0F;
            process_split(input[channel], low_mid_lowpass_state_[channel],
                          low_mid_highpass_state_[channel], low_mid_lowpass_,
                          low_mid_highpass_, low, high_after_low_mid);
            float low_allpass = 0.0F;
            float low_allpass_complement = 0.0F;
            process_split(low,
                          low_branch_mid_high_lowpass_state_[channel],
                          low_branch_mid_high_highpass_state_[channel],
                          mid_high_lowpass_, mid_high_highpass_, low_allpass,
                          low_allpass_complement);
            low = low_allpass + low_allpass_complement;
            float mid = 0.0F;
            float high = 0.0F;
            process_split(high_after_low_mid,
                          mid_high_lowpass_state_[channel],
                          mid_high_highpass_state_[channel], mid_high_lowpass_,
                          mid_high_highpass_, mid, high);
            bands[channel] = {low, mid, high};
            for (std::size_t band = 0; band < kBandCount; ++band) {
                const float magnitude = std::abs(bands[channel][band]);
                peak_measurement[band] =
                    std::max(peak_measurement[band], magnitude);
                power_measurement[band] += magnitude * magnitude;
            }
        }

        std::array<float, kBandCount> gain{};
        const float channel_count = static_cast<float>(channels);
        for (std::size_t band = 0; band < kBandCount; ++band) {
            auto& state = band_state_[band];
            const float mean_power = power_measurement[band] / channel_count;
            state.rms_power = state.rms_coefficient * state.rms_power +
                              (1.0F - state.rms_coefficient) * mean_power;
            state.rms_power = std::clamp(state.rms_power, 0.0F,
                                         kMaximumStateMagnitude);
            if (state.rms_power < kDenormalGuard) {
                state.rms_power = 0.0F;
            }

            const float detector_level =
                parameters_.bands[band].detector == MultibandDetector::Peak
                    ? peak_measurement[band]
                    : std::sqrt(state.rms_power);
            const float target_gain = target_band_gain(band, detector_level);
            const float gain_coefficient = target_gain < state.compression_gain
                                               ? state.attack_coefficient
                                               : state.release_coefficient;
            state.compression_gain =
                gain_coefficient * state.compression_gain +
                (1.0F - gain_coefficient) * target_gain;
            state.compression_gain = std::clamp(
                std::isfinite(state.compression_gain)
                    ? state.compression_gain
                    : state.level_gain,
                0.0F, kMaximumStateMagnitude);
            gain[band] = state.compression_gain;
            if (parameters_.bands[band].mute) {
                gain[band] = 0.0F;
            }
        }

        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float output = 0.0F;
            for (std::size_t band = 0; band < kBandCount; ++band) {
                output += bands[channel][band] * gain[band];
            }
            if (!std::isfinite(output)) {
                output = 0.0F;
                reset();
            }
            interleaved[frame * channels + channel] =
                std::clamp(output, -kMaximumOutputMagnitude,
                           kMaximumOutputMagnitude);
        }
    }
}

void MultibandCompressor::reset() noexcept {
    for (std::uint16_t channel = 0; channel < kChannelCount; ++channel) {
        reset_channel(channel);
    }
    for (auto& state : band_state_) {
        state.rms_power = 0.0F;
        state.compression_gain = state.level_gain;
    }
}

} // namespace tabletfx
