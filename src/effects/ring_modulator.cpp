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
    detector_decimation_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(
                std::lround(sample_rate_ / kDetectorTargetRate)));
    detector_rate_ = sample_rate_ / static_cast<float>(detector_decimation_);
    set_parameters(parameters);
    prepare_hilbert();
    reset();
}

void RingModulator::set_parameters(RingModulatorParameters parameters) noexcept {
    if (!valid_tracking_mode(parameters.tracking_mode)) {
        parameters.tracking_mode = parameters_.tracking_mode;
    }
    if (parameters.type != RingModulatorType::classic &&
        parameters.type != RingModulatorType::ssb_upper &&
        parameters.type != RingModulatorType::ssb_lower) {
        parameters.type = parameters_.type;
    }
    const bool reset_detector =
        parameters.pitch_tracking != parameters_.pitch_tracking ||
        parameters.tracking_mode != parameters_.tracking_mode;
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
    if (reset_detector) {
        reset_pitch_detector();
    }
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
        if (parameters_.pitch_tracking) {
            float detector_mono = 0.0F;
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                const float sample = interleaved[offset + channel];
                if (std::isfinite(sample) &&
                    std::abs(sample) <= kMaximumStableInput &&
                    std::abs(sample) > std::abs(detector_mono)) {
                    detector_mono = sample;
                }
            }
            process_pitch_detector(detector_mono);
        }
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
    reset_pitch_detector();
    prepare_oscillator();
    phase_sine_ = 0.0F;
    phase_cosine_ = 1.0F;
    normalization_counter_ = 0;
}

float RingModulator::bounded(float value, float fallback,
                             float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

bool RingModulator::valid_tracking_mode(
    const RingModulatorTrackingMode mode) noexcept {
    return mode == RingModulatorTrackingMode::guitar ||
           mode == RingModulatorTrackingMode::bass;
}

void RingModulator::process_pitch_detector(const float mono_sample) noexcept {
    detector_decimation_sum_ += std::isfinite(mono_sample) ? mono_sample : 0.0F;
    ++detector_decimation_count_;
    if (detector_decimation_count_ < detector_decimation_) {
        return;
    }

    const float downsampled = detector_decimation_sum_ /
                              static_cast<float>(detector_decimation_);
    detector_decimation_sum_ = 0.0F;
    detector_decimation_count_ = 0U;
    detector_frame_[detector_write_] = std::isfinite(downsampled)
        ? downsampled
        : 0.0F;
    detector_write_ = (detector_write_ + 1U) % kDetectorFrame;
    detector_count_ = std::min(detector_count_ + 1U, kDetectorFrame);
    if (detector_count_ < kDetectorFrame) {
        return;
    }

    ++detector_hop_count_;
    if (detector_hop_count_ < kDetectorHop) {
        return;
    }
    detector_hop_count_ = 0U;

    const float detected = detect_input_pitch();
    if (detected > 0.0F) {
        detected_input_frequency_hz_ = detected;
        detector_miss_count_ = 0U;
        if (!(tracked_frequency_hz_ > 0.0F)) {
            tracked_frequency_hz_ = detected;
        } else {
            constexpr float kSmoothingMs = 40.0F;
            const float hop_ms = 1000.0F * static_cast<float>(kDetectorHop) /
                                 detector_rate_;
            const float coefficient = 1.0F - std::exp(-hop_ms / kSmoothingMs);
            tracked_frequency_hz_ += coefficient *
                                     (detected - tracked_frequency_hz_);
        }
        prepare_oscillator();
        return;
    }

    detector_miss_count_ = std::min<std::size_t>(detector_miss_count_ + 1U, 5U);
    if (detector_miss_count_ >= 5U) {
        detected_input_frequency_hz_ = 0.0F;
        tracked_frequency_hz_ = 0.0F;
        prepare_oscillator();
    }
}

float RingModulator::detect_input_pitch() const noexcept {
    if (detector_count_ < kDetectorFrame || !(detector_rate_ > 0.0F) ||
        !std::isfinite(detector_rate_)) {
        return 0.0F;
    }

    std::array<float, kDetectorFrame> centered{};
    double mean = 0.0;
    for (std::size_t i = 0U; i < kDetectorFrame; ++i) {
        const std::size_t index = (detector_write_ + i) % kDetectorFrame;
        centered[i] = detector_frame_[index];
        mean += centered[i];
    }
    mean /= static_cast<double>(kDetectorFrame);

    double energy = 0.0;
    for (float& sample : centered) {
        sample -= static_cast<float>(mean);
        energy += static_cast<double>(sample) * sample;
    }
    const double rms = std::sqrt(energy / static_cast<double>(kDetectorFrame));
    if (!(rms > kDetectorMinimumRms) || !std::isfinite(rms)) {
        return 0.0F;
    }

    const float minimum_frequency =
        parameters_.tracking_mode == RingModulatorTrackingMode::bass
            ? kMinimumBassTrackedHz
            : kMinimumGuitarTrackedHz;
    const float maximum_frequency =
        parameters_.tracking_mode == RingModulatorTrackingMode::bass
            ? kMaximumBassTrackedHz
            : kMaximumGuitarTrackedHz;
    std::array<float, kDetectorFrame> difference{};
    const std::size_t minimum_lag = std::max<std::size_t>(
        2U, static_cast<std::size_t>(detector_rate_ / maximum_frequency));
    const std::size_t maximum_lag = std::min<std::size_t>(
        kDetectorFrame - 32U,
        static_cast<std::size_t>(std::ceil(detector_rate_ / minimum_frequency)));
    if (maximum_lag <= minimum_lag + 2U) {
        return 0.0F;
    }

    for (std::size_t lag = minimum_lag; lag <= maximum_lag; ++lag) {
        double sum = 0.0;
        const std::size_t count = kDetectorFrame - lag;
        for (std::size_t i = 0U; i < count; ++i) {
            const double delta = static_cast<double>(centered[i]) -
                                 centered[i + lag];
            sum += delta * delta;
        }
        difference[lag] = static_cast<float>(sum / static_cast<double>(count));
    }

    double cumulative = 0.0;
    float best_cmnd = 1.0F;
    std::size_t best_lag = 0U;
    std::size_t selected_lag = 0U;
    for (std::size_t lag = minimum_lag; lag <= maximum_lag; ++lag) {
        cumulative += difference[lag];
        if (cumulative <= 1.0e-18) {
            continue;
        }
        const float cmnd = static_cast<float>(
            static_cast<double>(difference[lag]) * static_cast<double>(lag) /
            cumulative);
        if (cmnd < best_cmnd) {
            best_cmnd = cmnd;
            best_lag = lag;
        }
        if (cmnd < kDetectorFirstMinimumCmnd && lag + 1U <= maximum_lag &&
            difference[lag] <= difference[lag + 1U]) {
            selected_lag = lag;
            break;
        }
    }
    if (selected_lag == 0U) {
        selected_lag = best_lag;
    }
    if (selected_lag == 0U || best_cmnd > kDetectorMaximumCmnd) {
        return 0.0F;
    }

    float refined_lag = static_cast<float>(selected_lag);
    if (selected_lag > minimum_lag && selected_lag < maximum_lag) {
        const float before = difference[selected_lag - 1U];
        const float center = difference[selected_lag];
        const float after = difference[selected_lag + 1U];
        const float denominator = before - 2.0F * center + after;
        if (std::abs(denominator) > 1.0e-9F) {
            refined_lag += std::clamp(
                0.5F * (before - after) / denominator, -0.5F, 0.5F);
        }
    }
    if (!(refined_lag > 0.0F) || !std::isfinite(refined_lag)) {
        return 0.0F;
    }
    const float frequency = detector_rate_ / refined_lag;
    if (frequency < minimum_frequency * 0.98F ||
        frequency > maximum_frequency) {
        return 0.0F;
    }
    return std::clamp(frequency, minimum_frequency, maximum_frequency);
}

void RingModulator::reset_pitch_detector() noexcept {
    detector_frame_ = {};
    detector_decimation_count_ = 0U;
    detector_write_ = 0U;
    detector_count_ = 0U;
    detector_hop_count_ = 0U;
    detector_miss_count_ = 0U;
    detector_decimation_sum_ = 0.0F;
    tracked_frequency_hz_ = 0.0F;
    detected_input_frequency_hz_ = 0.0F;
}

void RingModulator::prepare_oscillator() noexcept {
    // Keep the free-running carrier below Nyquist. Sidebands from the input can
    // still alias at extreme frequencies; no anti-aliasing claim is made.
    const float base_frequency = parameters_.pitch_tracking &&
                                         tracked_frequency_hz_ > 0.0F
                                     ? tracked_frequency_hz_
                                     : parameters_.frequency_hz;
    const float requested_frequency = base_frequency *
                                      parameters_.frequency_multiplier;
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
