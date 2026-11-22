#include "tabletfx/effects/de_esser.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {
namespace {

constexpr float kDefaultDetectorFrequencyHz = 6500.0F;
constexpr float kDefaultDetectorQ = 1.5F;
constexpr float kDefaultThresholdDbfs = -30.0F;
constexpr float kDefaultReductionDb = 12.0F;
constexpr float kDefaultAttackMs = 2.0F;
constexpr float kDefaultReleaseMs = 80.0F;
constexpr float kDenormalThreshold = 1.0e-20F;
constexpr double kPi = 3.141592653589793238462643383279502884;

} // namespace

DeEsser::DeEsser(float sample_rate, std::uint16_t channels,
                 DeEsserParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        throw std::invalid_argument("de-esser sample rate is unsupported");
    }
    if (channels == 0U || channels > 2U) {
        throw std::invalid_argument("de-esser supports mono or stereo");
    }
    configure(parameters);
}

std::size_t DeEsser::memory_bytes() const noexcept {
    return sizeof(*this);
}

float DeEsser::bounded(float value, float fallback, float minimum,
                       float maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

float DeEsser::time_coefficient(float milliseconds,
                                float sample_rate) noexcept {
    if (milliseconds <= 0.0F) {
        return 0.0F;
    }
    const double seconds = static_cast<double>(milliseconds) / 1000.0;
    return static_cast<float>(std::exp(
        -1.0 / (static_cast<double>(sample_rate) * seconds)));
}

float DeEsser::finite_input(float sample) noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return std::clamp(sample, -kMaximumInputMagnitude,
                      kMaximumInputMagnitude);
}

void DeEsser::configure(const DeEsserParameters& requested) noexcept {
    parameters_ = requested;
    const float max_frequency = std::min(
        kMaximumDetectorFrequencyHz, sample_rate_ * 0.45F);
    parameters_.detector_frequency_hz = bounded(
        parameters_.detector_frequency_hz,
        std::clamp(kDefaultDetectorFrequencyHz,
                   kMinimumDetectorFrequencyHz, max_frequency),
        kMinimumDetectorFrequencyHz, max_frequency);
    parameters_.detector_q = bounded(
        parameters_.detector_q, kDefaultDetectorQ,
        kMinimumDetectorQ, kMaximumDetectorQ);
    parameters_.threshold_dbfs = bounded(
        parameters_.threshold_dbfs, kDefaultThresholdDbfs,
        kMinimumThresholdDbfs, kMaximumThresholdDbfs);
    parameters_.max_reduction_db = bounded(
        parameters_.max_reduction_db, kDefaultReductionDb,
        kMinimumReductionDb, kMaximumReductionDb);
    parameters_.attack_ms = bounded(
        parameters_.attack_ms, kDefaultAttackMs,
        kMinimumAttackMs, kMaximumAttackMs);
    parameters_.release_ms = bounded(
        parameters_.release_ms, kDefaultReleaseMs,
        kMinimumReleaseMs, kMaximumReleaseMs);

    threshold_amplitude_ = static_cast<float>(std::pow(
        10.0, static_cast<double>(parameters_.threshold_dbfs) / 20.0));
    maximum_gain_ = static_cast<float>(std::pow(
        10.0, -static_cast<double>(parameters_.max_reduction_db) / 20.0));
    attack_coefficient_ = time_coefficient(parameters_.attack_ms, sample_rate_);
    release_coefficient_ = time_coefficient(parameters_.release_ms, sample_rate_);
    rebuild_coefficients();
    reset();
}

void DeEsser::rebuild_coefficients() noexcept {
    const double omega = 2.0 * kPi *
        static_cast<double>(parameters_.detector_frequency_hz) /
        static_cast<double>(sample_rate_);
    const double sine = std::sin(omega);
    const double cosine = std::cos(omega);
    const double alpha = sine /
        (2.0 * static_cast<double>(parameters_.detector_q));
    const double inverse_a0 = 1.0 / (1.0 + alpha);

    // RBJ constant-0 dB-peak band-pass biquad. It serves as both detector
    // filter and the band removed when dynamic attenuation is applied.
    detector_coefficients_ = {
        static_cast<float>(alpha * inverse_a0),
        0.0F,
        static_cast<float>(-alpha * inverse_a0),
        static_cast<float>(-2.0 * cosine * inverse_a0),
        static_cast<float>((1.0 - alpha) * inverse_a0),
    };
}

void DeEsser::reset() noexcept {
    states_.fill({});
}

void DeEsser::reset_channel(std::uint16_t channel) noexcept {
    states_[channel] = {};
}

void DeEsser::process(std::span<float> interleaved,
                      std::uint16_t channels) noexcept {
    if (channels == 0U || channels != channels_ || channels > 2U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = frame * channels_ + channel;
            float& sample = interleaved[index];
            if (!std::isfinite(sample)) {
                sample = 0.0F;
                reset_channel(channel);
                continue;
            }

            const float input = finite_input(sample);
            ChannelState& state = states_[channel];
            const float band = detector_coefficients_.b0 * input + state.z1;
            const float next_z1 = detector_coefficients_.b1 * input -
                detector_coefficients_.a1 * band + state.z2;
            const float next_z2 = detector_coefficients_.b2 * input -
                detector_coefficients_.a2 * band;
            if (!std::isfinite(band) || !std::isfinite(next_z1) ||
                !std::isfinite(next_z2)) {
                sample = 0.0F;
                reset_channel(channel);
                continue;
            }
            state.z1 = std::abs(next_z1) < kDenormalThreshold ? 0.0F : next_z1;
            state.z2 = std::abs(next_z2) < kDenormalThreshold ? 0.0F : next_z2;

            const float detector = std::abs(band);
            const float smoothing = detector > state.detector_envelope
                ? attack_coefficient_
                : release_coefficient_;
            state.detector_envelope = smoothing * state.detector_envelope +
                (1.0F - smoothing) * detector;
            if (!std::isfinite(state.detector_envelope)) {
                sample = 0.0F;
                reset_channel(channel);
                continue;
            }

            float gain = 1.0F;
            if (state.detector_envelope > threshold_amplitude_ &&
                parameters_.max_reduction_db > 0.0F) {
                const float relative_level = state.detector_envelope /
                                             threshold_amplitude_;
                const float reduction_progress = std::clamp(
                    (relative_level - 1.0F) /
                        (kMaximumThresholdAmplitudeRatio - 1.0F),
                    0.0F, 1.0F);
                gain = 1.0F - reduction_progress * (1.0F - maximum_gain_);
            }

            const float output = input - band * (1.0F - gain);
            if (!std::isfinite(output)) {
                sample = 0.0F;
                reset_channel(channel);
                continue;
            }
            sample = std::clamp(output, -kMaximumInputMagnitude,
                                kMaximumInputMagnitude);
        }
    }
}

} // namespace tabletfx::effects
