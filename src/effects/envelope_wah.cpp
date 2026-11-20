#include "tabletfx/effects/envelope_wah.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kMinimumFrequencyHz = 20.0F;
constexpr float kMinimumQ = 0.7F;
constexpr float kMaximumQ = 10.0F;
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr float kDenormalThreshold = 1.0e-20F;

float bounded(float value, float fallback, float low, float high) noexcept {
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}

float coefficient(float time_ms, float sample_rate) noexcept {
    if (time_ms <= 0.0F) return 0.0F;
    const double tau_samples = static_cast<double>(sample_rate) *
                              static_cast<double>(time_ms) / 1000.0;
    return static_cast<float>(std::exp(-1.0 / std::max(tau_samples, 1.0e-9)));
}

float decibels_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

} // namespace

EnvelopeWah::EnvelopeWah(float sample_rate, std::uint16_t channels,
                         EnvelopeWahParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        throw std::invalid_argument("EnvelopeWah sample rate must be in [8000, 384000] Hz");
    }
    if (channels == 0 || channels > 2) {
        throw std::invalid_argument("EnvelopeWah supports mono or stereo");
    }
    configure(sample_rate, channels, parameters);
}

void EnvelopeWah::configure(float sample_rate, std::uint16_t channels,
                            EnvelopeWahParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) && sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    channels_ = channels >= 1 && channels <= 2 ? channels : std::uint16_t{2};
    parameters_.sensitivity = bounded(parameters.sensitivity, 0.5F, 0.0F, 1.0F);
    parameters_.minimum_hz = bounded(parameters.minimum_hz, 350.0F,
                                     kMinimumFrequencyHz, sample_rate_ * 0.45F);
    parameters_.maximum_hz = bounded(parameters.maximum_hz, 2200.0F,
                                     kMinimumFrequencyHz, sample_rate_ * 0.45F);
    if (parameters_.minimum_hz > parameters_.maximum_hz) {
        std::swap(parameters_.minimum_hz, parameters_.maximum_hz);
    }
    parameters_.resonance_q = bounded(parameters.resonance_q, 4.0F, kMinimumQ, kMaximumQ);
    parameters_.level_db = bounded(parameters.level_db, 0.0F, -24.0F, 12.0F);
    parameters_.attack_ms = bounded(parameters.attack_ms, 5.0F, 0.0F, 2000.0F);
    parameters_.release_ms = bounded(parameters.release_ms, 80.0F, 0.0F, 5000.0F);
    parameters_.smoothing_ms = bounded(parameters.smoothing_ms, 5.0F, 0.0F, 1000.0F);
    parameters_.direction = parameters.direction == EnvelopeWahDirection::Down
                                ? EnvelopeWahDirection::Down
                                : EnvelopeWahDirection::Up;

    // Sensitivity maps to the rising envelope threshold from -18 dBFS
    // (low sensitivity) to -60 dBFS (high sensitivity).
    const float threshold_db = -18.0F - 42.0F * parameters_.sensitivity;
    threshold_ = decibels_to_linear(threshold_db);
    attack_coefficient_ = coefficient(parameters_.attack_ms, sample_rate_);
    release_coefficient_ = coefficient(parameters_.release_ms, sample_rate_);
    smoothing_coefficient_ = coefficient(parameters_.smoothing_ms, sample_rate_);
    k_ = 1.0F / parameters_.resonance_q;
    level_ = decibels_to_linear(parameters_.level_db);

    const float ratio = parameters_.maximum_hz / parameters_.minimum_hz;
    for (std::size_t i = 0; i < g_table_.size(); ++i) {
        const float position = static_cast<float>(i) /
                               static_cast<float>(g_table_.size() - 1);
        const float frequency = parameters_.minimum_hz *
                                std::pow(ratio, position);
        const double omega = kPi * static_cast<double>(frequency) /
                             static_cast<double>(sample_rate_);
        g_table_[i] = static_cast<float>(std::tan(omega));
    }
    reset();
}

void EnvelopeWah::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels == 0 || channels != channels_) return;
    const std::size_t frames = interleaved.size() / channels_;
    const float envelope_span = std::max(0.5F - threshold_, 1.0e-6F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels_;
        float peak = 0.0F;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const float sample = interleaved[offset + channel];
            if (std::isfinite(sample)) peak = std::max(peak, std::abs(sample));
        }
        const float env_coefficient = peak > envelope_ ? attack_coefficient_
                                                       : release_coefficient_;
        envelope_ = env_coefficient * envelope_ + (1.0F - env_coefficient) * peak;

        float position = std::clamp((envelope_ - threshold_) / envelope_span, 0.0F, 1.0F);
        if (parameters_.direction == EnvelopeWahDirection::Down) position = 1.0F - position;
        const float table_position = position * static_cast<float>(g_table_.size() - 1);
        const auto lower = static_cast<std::size_t>(table_position);
        const std::size_t index = std::min(lower, g_table_.size() - 2);
        const float fraction = table_position - static_cast<float>(index);
        const float target_g = g_table_[index] +
                               fraction * (g_table_[index + 1] - g_table_[index]);
        current_g_ = smoothing_coefficient_ * current_g_ +
                     (1.0F - smoothing_coefficient_) * target_g;
        const float denominator = 1.0F + current_g_ * (current_g_ + k_);
        const float inverse_denominator = 1.0F / denominator;

        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t sample_index = offset + channel;
            const float input = interleaved[sample_index];
            if (!std::isfinite(input)) {
                states_[channel] = {};
                interleaved[sample_index] = 0.0F;
                continue;
            }
            State& state = states_[channel];
            const float v1 = (state.integrator1 + current_g_ *
                              (input - state.integrator2)) * inverse_denominator;
            const float v2 = state.integrator2 + current_g_ * v1;
            const float next1 = 2.0F * v1 - state.integrator1;
            const float next2 = 2.0F * v2 - state.integrator2;
            const float output = v1 * level_;
            if (!std::isfinite(v1) || !std::isfinite(v2) ||
                !std::isfinite(next1) || !std::isfinite(next2) ||
                !std::isfinite(output)) {
                state = {};
                interleaved[sample_index] = 0.0F;
                continue;
            }
            state.integrator1 = std::abs(next1) < kDenormalThreshold ? 0.0F : next1;
            state.integrator2 = std::abs(next2) < kDenormalThreshold ? 0.0F : next2;
            interleaved[sample_index] = output;
        }
    }
}

void EnvelopeWah::reset() noexcept {
    envelope_ = 0.0F;
    states_ = {};
    const float position = parameters_.direction == EnvelopeWahDirection::Down ? 1.0F : 0.0F;
    const std::size_t index = static_cast<std::size_t>(position *
                                  static_cast<float>(g_table_.size() - 1));
    current_g_ = g_table_[std::min(index, g_table_.size() - 1)];
}

float EnvelopeWah::current_frequency_hz() const noexcept {
    return static_cast<float>(std::atan(static_cast<double>(current_g_)) *
                              static_cast<double>(sample_rate_) / kPi);
}

float EnvelopeWah::target_frequency_hz() const noexcept {
    float position = std::clamp((envelope_ - threshold_) /
                                    std::max(0.5F - threshold_, 1.0e-6F),
                                0.0F, 1.0F);
    if (parameters_.direction == EnvelopeWahDirection::Down) position = 1.0F - position;
    return parameters_.minimum_hz *
           std::pow(parameters_.maximum_hz / parameters_.minimum_hz, position);
}

} // namespace tabletfx
