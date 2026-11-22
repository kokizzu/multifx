#include "tabletfx/effects/ring_modulator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

// Leave headroom for the one-pole update's intermediate difference. Values
// beyond this are far outside a useful audio range; silence them and clear
// that channel's filter state so a bad sample cannot poison later audio.
constexpr float kMaximumStableInput =
    std::numeric_limits<float>::max() * 0.25F;

} // namespace

RingModulator::RingModulator(float sample_rate, std::uint16_t channels,
                             RingModulatorParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument(
            "RingModulator requires 8–384 kHz and mono or stereo channels");
    }
    set_parameters(parameters);
    prepare_hilbert();
    reset();
}

void RingModulator::set_parameters(RingModulatorParameters parameters) noexcept {
    if (parameters.type != RingModulatorType::classic &&
        parameters.type != RingModulatorType::ssb_upper &&
        parameters.type != RingModulatorType::ssb_lower) {
        parameters.type = parameters_.type;
    }
    parameters.frequency_hz = bounded(parameters.frequency_hz,
                                      parameters_.frequency_hz,
                                      kMinimumFrequencyHz,
                                      kMaximumFrequencyHz);
    parameters.frequency_multiplier = bounded(
        parameters.frequency_multiplier, parameters_.frequency_multiplier,
        kMinimumMultiplier, kMaximumMultiplier);
    parameters.high_cut_hz = bounded(parameters.high_cut_hz,
                                     parameters_.high_cut_hz,
                                     kMinimumHighCutHz, kMaximumHighCutHz);
    parameters.mix = bounded(parameters.mix, parameters_.mix, 0.0F, 1.0F);
    parameters_ = parameters;
    prepare_oscillator();
    prepare_high_cut();
}

void RingModulator::process(std::span<float> interleaved,
                            std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = offset + channel;
            float input = interleaved[index];
            auto& history = input_history_[channel];
            if (!std::isfinite(input) || std::abs(input) > kMaximumStableInput) {
                high_cut_state_[channel] = 0.0F;
                history.fill(0.0F);
                interleaved[index] = 0.0F;
                continue;
            }

            history[history_write_index_] = input;
            float dry = input;
            float modulated = input * phase_sine_;
            if (parameters_.type != RingModulatorType::classic) {
                const std::size_t in_phase_index =
                    (history_write_index_ + kHilbertTaps - kHilbertDelay) %
                    kHilbertTaps;
                dry = history[in_phase_index];
                float quadrature = 0.0F;
                for (std::size_t tap = 0; tap < kHilbertTaps; ++tap) {
                    const std::size_t history_index =
                        (history_write_index_ + kHilbertTaps - tap) %
                        kHilbertTaps;
                    quadrature += hilbert_coefficients_[tap] *
                                  history[history_index];
                }
                modulated = dry * phase_cosine_;
                const float sideband = quadrature * phase_sine_;
                modulated += parameters_.type == RingModulatorType::ssb_upper
                                 ? -sideband
                                 : sideband;
            }

            high_cut_state_[channel] +=
                high_cut_alpha_ * (modulated - high_cut_state_[channel]);
            const float output = dry +
                (high_cut_state_[channel] - dry) * parameters_.mix;
            interleaved[index] = std::isfinite(output) ? output : 0.0F;
        }
        history_write_index_ = (history_write_index_ + 1U) % kHilbertTaps;
        advance_oscillator();
    }
}

void RingModulator::reset() noexcept {
    high_cut_state_ = {};
    input_history_ = {};
    history_write_index_ = 0;
    phase_sine_ = 0.0F;
    phase_cosine_ = 1.0F;
    normalization_counter_ = 0;
}

float RingModulator::bounded(float value, float fallback,
                             float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

void RingModulator::prepare_oscillator() noexcept {
    // Keep the free-running carrier below Nyquist. Sidebands from the input can
    // still alias at extreme frequencies; no anti-aliasing claim is made.
    const float requested_frequency =
        parameters_.frequency_hz * parameters_.frequency_multiplier;
    effective_frequency_hz_ = std::min(requested_frequency, sample_rate_ * 0.45F);
    const float phase_step = kTwoPi * effective_frequency_hz_ / sample_rate_;
    increment_sine_ = std::sin(phase_step);
    increment_cosine_ = std::cos(phase_step);
}

void RingModulator::prepare_high_cut() noexcept {
    // The cutoff range and one-pole response are project-authored; the FM9
    // guide documents the control but does not state its range or filter order.
    effective_high_cut_hz_ = std::min(parameters_.high_cut_hz,
                                      sample_rate_ * 0.45F);
    const float coefficient = kTwoPi * effective_high_cut_hz_ / sample_rate_;
    high_cut_alpha_ = 1.0F - std::exp(-coefficient);
}

void RingModulator::prepare_hilbert() noexcept {
    // Odd-offset samples form the ideal discrete Hilbert transformer. A
    // Hamming window limits truncation ripple; the 63-tap design is a bounded,
    // project-authored compromise rather than the proprietary Fractal DSP.
    for (std::size_t tap = 0; tap < kHilbertTaps; ++tap) {
        const int offset = static_cast<int>(tap) -
                           static_cast<int>(kHilbertDelay);
        if (offset == 0 || (std::abs(offset) % 2) == 0) {
            hilbert_coefficients_[tap] = 0.0F;
            continue;
        }
        const float position = static_cast<float>(tap) /
                               static_cast<float>(kHilbertTaps - 1U);
        const float window = 0.54F - 0.46F *
            std::cos(kTwoPi * position);
        hilbert_coefficients_[tap] =
            (2.0F / (kPi * static_cast<float>(offset))) * window;
    }
}

void RingModulator::advance_oscillator() noexcept {
    const float previous_sine = phase_sine_;
    phase_sine_ = previous_sine * increment_cosine_ +
                  phase_cosine_ * increment_sine_;
    phase_cosine_ = phase_cosine_ * increment_cosine_ -
                    previous_sine * increment_sine_;

    if (++normalization_counter_ == 4096U) {
        const float magnitude_squared = phase_sine_ * phase_sine_ +
                                        phase_cosine_ * phase_cosine_;
        if (std::isfinite(magnitude_squared) && magnitude_squared > 0.0F) {
            const float inverse_magnitude = 1.0F / std::sqrt(magnitude_squared);
            phase_sine_ *= inverse_magnitude;
            phase_cosine_ *= inverse_magnitude;
        } else {
            phase_sine_ = 0.0F;
            phase_cosine_ = 1.0F;
        }
        normalization_counter_ = 0;
    }
}

} // namespace tabletfx
