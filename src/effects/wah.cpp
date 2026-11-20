#include "tabletfx/effects/wah.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kMinimumFrequencyHz = 20.0F;
constexpr float kMaximumResonanceQ = 10.0F;
constexpr float kMinimumResonanceQ = 0.7F;
constexpr float kMinimumLevelDb = -24.0F;
constexpr float kMaximumLevelDb = 12.0F;
constexpr float kMaximumSmoothingMs = 1000.0F;
constexpr float kDenormalThreshold = 1.0e-20F;
constexpr double kPi = 3.141592653589793238462643383279502884;

float decibels_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

} // namespace

Wah::Wah(float sample_rate, std::uint16_t channels, WahParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        throw std::invalid_argument("Wah sample rate must be in [8000, 384000] Hz");
    }
    if (channels == 0 || channels > 2) {
        throw std::invalid_argument("Wah supports mono or stereo");
    }
    configure(sample_rate, channels, parameters);
}

void Wah::configure(float sample_rate, std::uint16_t channels,
                    WahParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    channels_ = channels >= 1 && channels <= 2 ? channels : std::uint16_t{2};
    parameters_.position = bounded(parameters.position, 0.5F, 0.0F, 1.0F);
    parameters_.minimum_hz = bounded(parameters.minimum_hz, 350.0F,
                                     kMinimumFrequencyHz, sample_rate_ * 0.45F);
    parameters_.maximum_hz = bounded(parameters.maximum_hz, 2200.0F,
                                     kMinimumFrequencyHz, sample_rate_ * 0.45F);
    if (parameters_.minimum_hz > parameters_.maximum_hz) {
        std::swap(parameters_.minimum_hz, parameters_.maximum_hz);
    }
    parameters_.resonance_q = bounded(parameters.resonance_q, 4.0F,
                                      kMinimumResonanceQ, kMaximumResonanceQ);
    parameters_.level_db = bounded(parameters.level_db, 0.0F,
                                   kMinimumLevelDb, kMaximumLevelDb);
    parameters_.smoothing_ms = bounded(parameters.smoothing_ms, 10.0F,
                                       0.0F, kMaximumSmoothingMs);
    retarget(true);
    reset();
}

void Wah::set_position(float position) noexcept {
    if (!std::isfinite(position)) {
        return;
    }
    parameters_.position = std::clamp(position, 0.0F, 1.0F);
    retarget(false);
}

void Wah::set_sweep_range_hz(float minimum_hz, float maximum_hz) noexcept {
    const float max_frequency = sample_rate_ * 0.45F;
    parameters_.minimum_hz = bounded(minimum_hz, parameters_.minimum_hz,
                                     kMinimumFrequencyHz, max_frequency);
    parameters_.maximum_hz = bounded(maximum_hz, parameters_.maximum_hz,
                                     kMinimumFrequencyHz, max_frequency);
    if (parameters_.minimum_hz > parameters_.maximum_hz) {
        std::swap(parameters_.minimum_hz, parameters_.maximum_hz);
    }
    retarget(false);
}

void Wah::set_resonance_q(float resonance_q) noexcept {
    parameters_.resonance_q = bounded(resonance_q, parameters_.resonance_q,
                                      kMinimumResonanceQ, kMaximumResonanceQ);
    retarget(false);
}

void Wah::set_level_db(float level_db) noexcept {
    parameters_.level_db = bounded(level_db, parameters_.level_db,
                                   kMinimumLevelDb, kMaximumLevelDb);
    retarget(false);
}

void Wah::set_smoothing_ms(float smoothing_ms) noexcept {
    parameters_.smoothing_ms = bounded(smoothing_ms, 10.0F,
                                       0.0F, kMaximumSmoothingMs);
}

float Wah::current_frequency_hz() const noexcept {
    return static_cast<float>(std::atan(static_cast<double>(current_g_)) *
                              static_cast<double>(sample_rate_) / kPi);
}

void Wah::process(std::span<float> interleaved,
                  std::uint16_t channels) noexcept {
    if (channels == 0 || channels != channels_) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (ramp_remaining_ > 0) {
            current_g_ += g_step_;
            current_k_ += k_step_;
            current_level_ += level_step_;
            --ramp_remaining_;
            if (ramp_remaining_ == 0) {
                current_g_ = target_g_;
                current_k_ = target_k_;
                current_level_ = target_level_;
                g_step_ = 0.0F;
                k_step_ = 0.0F;
                level_step_ = 0.0F;
            }
        }

        const float denominator = 1.0F + current_g_ * (current_g_ + current_k_);
        const float inverse_denominator = 1.0F / denominator;
        const std::size_t offset = frame * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = offset + channel;
            const float input = interleaved[index];
            State& state = states_[channel];
            if (!std::isfinite(input)) {
                state = {};
                interleaved[index] = 0.0F;
                continue;
            }

            const float v1 = (state.integrator1 + current_g_ *
                              (input - state.integrator2)) * inverse_denominator;
            const float v2 = state.integrator2 + current_g_ * v1;
            const float next1 = 2.0F * v1 - state.integrator1;
            const float next2 = 2.0F * v2 - state.integrator2;
            const float output = v1 * current_level_;
            if (!std::isfinite(v1) || !std::isfinite(v2) ||
                !std::isfinite(next1) || !std::isfinite(next2) ||
                !std::isfinite(output)) {
                state = {};
                interleaved[index] = 0.0F;
                continue;
            }

            state.integrator1 = std::abs(next1) < kDenormalThreshold ? 0.0F : next1;
            state.integrator2 = std::abs(next2) < kDenormalThreshold ? 0.0F : next2;
            interleaved[index] = output;
        }
    }
}

void Wah::reset() noexcept {
    current_g_ = target_g_;
    current_k_ = target_k_;
    current_level_ = target_level_;
    g_step_ = 0.0F;
    k_step_ = 0.0F;
    level_step_ = 0.0F;
    ramp_remaining_ = 0;
    states_ = {};
}

float Wah::bounded(float value, float fallback,
                   float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float Wah::frequency_for_position(float position) const noexcept {
    const float ratio = parameters_.maximum_hz / parameters_.minimum_hz;
    return parameters_.minimum_hz * std::pow(ratio, position);
}

void Wah::retarget(bool immediate) noexcept {
    target_frequency_hz_ = frequency_for_position(parameters_.position);
    const double omega = kPi * static_cast<double>(target_frequency_hz_) /
                         static_cast<double>(sample_rate_);
    target_g_ = static_cast<float>(std::tan(omega));
    target_k_ = 1.0F / parameters_.resonance_q;
    target_level_ = decibels_to_linear(parameters_.level_db);

    if (immediate) {
        current_g_ = target_g_;
        current_k_ = target_k_;
        current_level_ = target_level_;
        g_step_ = 0.0F;
        k_step_ = 0.0F;
        level_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }

    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(parameters_.smoothing_ms) / 1000.0;
    const std::size_t ramp_frames = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_frames == 0) {
        current_g_ = target_g_;
        current_k_ = target_k_;
        current_level_ = target_level_;
        g_step_ = 0.0F;
        k_step_ = 0.0F;
        level_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }

    const float denominator = static_cast<float>(ramp_frames);
    g_step_ = (target_g_ - current_g_) / denominator;
    k_step_ = (target_k_ - current_k_) / denominator;
    level_step_ = (target_level_ - current_level_) / denominator;
    ramp_remaining_ = ramp_frames;
}

} // namespace tabletfx
