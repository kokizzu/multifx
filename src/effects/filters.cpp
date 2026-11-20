#include "tabletfx/effects/filters.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::detail {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kMinimumCutoffHz = 20.0F;
constexpr float kNyquistSafetyRatio = 0.45F;
constexpr double kButterworthQ = 0.7071067811865475244;
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr float kDenormalThreshold = 1.0e-20F;

float sanitize_cutoff(float cutoff_hz, float fallback_hz,
                      float sample_rate) noexcept {
    if (!std::isfinite(cutoff_hz)) {
        cutoff_hz = fallback_hz;
    }
    const float maximum_hz = std::max(kMinimumCutoffHz,
                                      sample_rate * kNyquistSafetyRatio);
    return std::clamp(cutoff_hz, kMinimumCutoffHz, maximum_hz);
}

} // namespace

ButterworthBiquad::ButterworthBiquad(float sample_rate, std::uint16_t channels,
                                     float default_cutoff_hz, Response response)
    : sample_rate_(sample_rate), channels_(channels),
      default_cutoff_hz_(default_cutoff_hz), cutoff_hz_(default_cutoff_hz),
      response_(response) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate) {
        throw std::invalid_argument("Filter sample rate must be in [8000, 384000] Hz");
    }
    if (channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument("Filters support mono or stereo");
    }
    configure(default_cutoff_hz_);
}

void ButterworthBiquad::configure(float cutoff_hz) noexcept {
    cutoff_hz_ = sanitize_cutoff(cutoff_hz, default_cutoff_hz_, sample_rate_);

    const double omega = 2.0 * kPi * static_cast<double>(cutoff_hz_) /
                         static_cast<double>(sample_rate_);
    const double cosine = std::cos(omega);
    const double sine = std::sin(omega);
    const double alpha = sine / (2.0 * kButterworthQ);
    const double a0 = 1.0 + alpha;
    const double inverse_a0 = 1.0 / a0;
    double b0 = 0.0;
    double b1 = 0.0;
    double b2 = 0.0;

    if (response_ == Response::HighPass) {
        b0 = (1.0 + cosine) * 0.5;
        b1 = -(1.0 + cosine);
        b2 = (1.0 + cosine) * 0.5;
    } else {
        b0 = (1.0 - cosine) * 0.5;
        b1 = 1.0 - cosine;
        b2 = (1.0 - cosine) * 0.5;
    }

    coefficients_ = {
        static_cast<float>(b0 * inverse_a0),
        static_cast<float>(b1 * inverse_a0),
        static_cast<float>(b2 * inverse_a0),
        static_cast<float>((-2.0 * cosine) * inverse_a0),
        static_cast<float>((1.0 - alpha) * inverse_a0),
    };
    reset();
}

void ButterworthBiquad::process(std::span<float> interleaved,
                                std::uint16_t channels) noexcept {
    if (channels == 0 || channels != channels_) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = frame * channels_ + channel;
            const float input = interleaved[index];
            if (!std::isfinite(input)) {
                states_[channel] = {};
                interleaved[index] = 0.0F;
                continue;
            }
            interleaved[index] = process_sample(input, coefficients_, states_[channel]);
        }
    }
}

void ButterworthBiquad::reset() noexcept {
    states_ = {};
}

float ButterworthBiquad::process_sample(float input,
                                        const Coefficients& coefficients,
                                        State& state) noexcept {
    const float output = coefficients.b0 * input + state.z1;
    const float next_z1 = coefficients.b1 * input - coefficients.a1 * output + state.z2;
    const float next_z2 = coefficients.b2 * input - coefficients.a2 * output;
    if (!std::isfinite(output) || !std::isfinite(next_z1) || !std::isfinite(next_z2)) {
        state = {};
        return 0.0F;
    }
    state.z1 = std::abs(next_z1) < kDenormalThreshold ? 0.0F : next_z1;
    state.z2 = std::abs(next_z2) < kDenormalThreshold ? 0.0F : next_z2;
    return output;
}

} // namespace tabletfx::detail

namespace tabletfx {

HighPassFilter::HighPassFilter(float sample_rate, std::uint16_t channels,
                               float cutoff_hz)
    : filter_(sample_rate, channels, cutoff_hz,
              detail::ButterworthBiquad::Response::HighPass) {}

LowPassFilter::LowPassFilter(float sample_rate, std::uint16_t channels,
                             float cutoff_hz)
    : filter_(sample_rate, channels, cutoff_hz,
              detail::ButterworthBiquad::Response::LowPass) {}

} // namespace tabletfx
