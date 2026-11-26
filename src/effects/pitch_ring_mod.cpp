#include "tabletfx/effects/pitch_ring_mod.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {

PitchRingMod::PitchRingMod(float sample_rate, std::uint16_t channels,
                           PitchRingModParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate) {
        throw std::invalid_argument("Pitch Ring Mod sample rate is unsupported");
    }
    if (channels_ == 0U || channels_ > 2U) {
        throw std::invalid_argument("Pitch Ring Mod supports mono or stereo");
    }

    // Keep pitch detection at or below 12 kHz. The input is averaged in short
    // groups before entering the tracker; the carrier still runs at the audio
    // sample rate.
    decimation_factor_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::ceil(sample_rate_ / 12000.0F)));
    tracker_sample_rate_ = sample_rate_ /
                           static_cast<float>(decimation_factor_);
    configure(parameters);
    reset();
}

void PitchRingMod::configure(PitchRingModParameters parameters) noexcept {
    parameters.carrier_ratio = bounded(parameters.carrier_ratio, 1.0F,
                                       0.25F, 4.0F);
    parameters.fallback_pitch_hz = bounded(parameters.fallback_pitch_hz,
                                           110.0F, kMinimumPitchHz,
                                           880.0F);
    parameters.tracking_confidence = bounded(parameters.tracking_confidence,
                                             0.55F, 0.2F, 0.98F);
    parameters.mix = bounded(parameters.mix, 0.5F, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, 0.0F, -24.0F, 12.0F);
    parameters_ = parameters;
    output_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    if (!has_tracked_pitch_) {
        tracked_pitch_hz_ = parameters_.fallback_pitch_hz;
        update_carrier();
    }
}

void PitchRingMod::reset() noexcept {
    tracker_history_.fill(0.0F);
    tracker_write_index_ = 0U;
    tracker_samples_seen_ = 0U;
    tracker_update_counter_ = 0U;
    decimation_counter_ = 0U;
    decimation_sum_ = 0.0F;
    tracked_pitch_hz_ = parameters_.fallback_pitch_hz;
    pitch_confidence_ = 0.0F;
    has_tracked_pitch_ = false;
    phase_sine_ = 0.0F;
    phase_cosine_ = 1.0F;
    normalization_counter_ = 0U;
    update_carrier();
}

void PitchRingMod::process(std::span<float> interleaved,
                           std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U || channels > 2U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const std::size_t offset = frame * channels_;
        std::array<float, 2U> input{};
        float linked_input = 0.0F;
        for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
            input[channel] = finite_input(interleaved[offset + channel]);
            linked_input += input[channel];
        }
        linked_input /= static_cast<float>(channels_);

        decimation_sum_ += linked_input;
        ++decimation_counter_;
        if (decimation_counter_ >= decimation_factor_) {
            add_tracker_sample(decimation_sum_ /
                               static_cast<float>(decimation_factor_));
            decimation_sum_ = 0.0F;
            decimation_counter_ = 0U;
        }

        const float carrier = phase_sine_;
        for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
            const float dry = input[channel];
            const float wet = dry * carrier;
            const float mixed = dry + (wet - dry) * parameters_.mix;
            const float output = mixed * output_gain_;
            interleaved[offset + channel] = std::isfinite(output) ? output : 0.0F;
        }
        advance_carrier();
    }
}

float PitchRingMod::bounded(float value, float fallback,
                            float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum)
                                : fallback;
}

float PitchRingMod::finite_input(float value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0F;
    }
    return std::clamp(value, -kMaximumStableInput, kMaximumStableInput);
}

std::size_t PitchRingMod::analysis_window_frames() const noexcept {
    const float reference_pitch = has_tracked_pitch_
                                     ? tracked_pitch_hz_
                                     : parameters_.fallback_pitch_hz;
    // Keep roughly three cycles at half the last valid pitch. This gives the
    // tracker room to follow ordinary downward note changes while shrinking
    // the analysis window for higher notes. Very low notes retain the full
    // fixed-size history.
    const double requested = 6.0 * static_cast<double>(tracker_sample_rate_) /
                             static_cast<double>(reference_pitch);
    const auto frames = static_cast<std::size_t>(std::ceil(requested));
    return std::clamp(frames, kMinimumAnalysisFrames,
                      kTrackerWindowFrames);
}

void PitchRingMod::add_tracker_sample(float sample) noexcept {
    tracker_history_[tracker_write_index_] = sample;
    tracker_write_index_ = (tracker_write_index_ + 1U) %
                           kTrackerWindowFrames;
    tracker_samples_seen_ = std::min(tracker_samples_seen_ + 1U,
                                     kTrackerWindowFrames);

    const std::size_t required_frames = analysis_window_frames();
    if (tracker_samples_seen_ < required_frames) {
        return;
    }
    if (tracker_samples_seen_ == required_frames) {
        tracker_update_counter_ = 0U;
        estimate_pitch();
        return;
    }
    if (++tracker_update_counter_ >= kTrackerUpdateSamples) {
        tracker_update_counter_ = 0U;
        estimate_pitch();
    }
}

void PitchRingMod::estimate_pitch() noexcept {
    constexpr std::size_t kMaximumLagCount =
        kTrackerWindowFrames / 2U + 1U;
    std::array<float, kTrackerWindowFrames> centered{};
    std::array<double, kTrackerWindowFrames + 1U> cumulative_energy{};
    std::array<float, kMaximumLagCount> correlations{};

    const std::size_t window_frames = analysis_window_frames();
    double mean = 0.0;
    const std::size_t oldest_index =
        (tracker_write_index_ + kTrackerWindowFrames - window_frames) %
        kTrackerWindowFrames;
    for (std::size_t index = 0U; index < window_frames; ++index) {
        const std::size_t history_index =
            (oldest_index + index) % kTrackerWindowFrames;
        mean += static_cast<double>(tracker_history_[history_index]);
    }
    mean /= static_cast<double>(window_frames);

    for (std::size_t index = 0U; index < window_frames; ++index) {
        const std::size_t history_index =
            (oldest_index + index) % kTrackerWindowFrames;
        centered[index] = static_cast<float>(
            static_cast<double>(tracker_history_[history_index]) - mean);
        const double value = static_cast<double>(centered[index]);
        cumulative_energy[index + 1U] = cumulative_energy[index] + value * value;
    }

    const double energy = cumulative_energy[window_frames];
    const double mean_square = energy / static_cast<double>(window_frames);
    if (!std::isfinite(mean_square) ||
        mean_square < static_cast<double>(kMinimumRmsSquared)) {
        use_fallback_pitch();
        return;
    }

    const std::size_t minimum_lag = std::max<std::size_t>(
        2U, static_cast<std::size_t>(std::ceil(
                static_cast<double>(tracker_sample_rate_) /
                static_cast<double>(kMaximumPitchHz))));
    const std::size_t maximum_lag = std::min<std::size_t>(
        window_frames / 2U,
        static_cast<std::size_t>(tracker_sample_rate_ / kMinimumPitchHz));
    if (minimum_lag + 2U > maximum_lag) {
        use_fallback_pitch();
        return;
    }

    float strongest_peak = 0.0F;
    std::size_t selected_lag = 0U;
    float selected_left = 0.0F;
    float selected_center = 0.0F;
    float selected_right = 0.0F;
    const auto correlation_at = [&](std::size_t lag) noexcept {
        double cross = 0.0;
        const std::size_t overlap = window_frames - lag;
        for (std::size_t index = 0U; index < overlap; ++index) {
            cross += static_cast<double>(centered[index]) *
                     static_cast<double>(centered[index + lag]);
        }
        const double first_energy = cumulative_energy[overlap];
        const double second_energy =
            energy - cumulative_energy[lag];
        const double denominator = std::sqrt(first_energy * second_energy);
        if (denominator <= 0.0 || !std::isfinite(denominator)) {
            return 0.0F;
        }
        const double normalized = std::clamp(cross / denominator, -1.0, 1.0);
        return static_cast<float>(normalized);
    };

    for (std::size_t lag = minimum_lag - 1U;
         lag <= maximum_lag + 1U; ++lag) {
        correlations[lag] = correlation_at(lag);
    }
    for (std::size_t lag = minimum_lag; lag <= maximum_lag; ++lag) {
        const float center = correlations[lag];
        const float left = correlations[lag - 1U];
        const float right = correlations[lag + 1U];
        if (center < left || center < right) {
            continue;
        }
        strongest_peak = std::max(strongest_peak, center);
        if (center >= parameters_.tracking_confidence) {
            // Selecting the first strong autocorrelation peak reduces
            // octave/subharmonic errors on clean monophonic notes.
            selected_lag = lag;
            selected_left = left;
            selected_center = center;
            selected_right = right;
            break;
        }
    }

    pitch_confidence_ = strongest_peak;
    if (selected_lag == 0U) {
        use_fallback_pitch();
        pitch_confidence_ = strongest_peak;
        return;
    }

    double refined_lag = static_cast<double>(selected_lag);
    if (selected_lag > minimum_lag && selected_lag < maximum_lag) {
        const double curvature = static_cast<double>(selected_left) -
                                 2.0 * static_cast<double>(selected_center) +
                                 static_cast<double>(selected_right);
        if (std::abs(curvature) > 1.0e-9) {
            const double adjustment = std::clamp(
                0.5 * (static_cast<double>(selected_left) -
                       static_cast<double>(selected_right)) / curvature,
                -0.5, 0.5);
            refined_lag += adjustment;
        }
    }

    if (!std::isfinite(refined_lag) || refined_lag <= 0.0) {
        use_fallback_pitch();
        return;
    }
    const float measured_pitch = static_cast<float>(
        static_cast<double>(tracker_sample_rate_) / refined_lag);
    if (!std::isfinite(measured_pitch) || measured_pitch < kMinimumPitchHz ||
        measured_pitch > kMaximumPitchHz) {
        use_fallback_pitch();
        return;
    }

    if (!has_tracked_pitch_) {
        tracked_pitch_hz_ = measured_pitch;
        has_tracked_pitch_ = true;
    } else {
        // A half-step response filter damps jitter while keeping note changes
        // reasonably quick for a live instrument input.
        tracked_pitch_hz_ += 0.5F * (measured_pitch - tracked_pitch_hz_);
    }
    pitch_confidence_ = selected_center;
    update_carrier();
}

void PitchRingMod::use_fallback_pitch() noexcept {
    tracked_pitch_hz_ = parameters_.fallback_pitch_hz;
    pitch_confidence_ = 0.0F;
    has_tracked_pitch_ = false;
    update_carrier();
}

void PitchRingMod::update_carrier() noexcept {
    const float requested = tracked_pitch_hz_ * parameters_.carrier_ratio;
    carrier_frequency_hz_ = std::clamp(requested, 1.0F,
                                       sample_rate_ * 0.45F);
    const float phase_step = kTwoPi * carrier_frequency_hz_ / sample_rate_;
    increment_sine_ = std::sin(phase_step);
    increment_cosine_ = std::cos(phase_step);
}

void PitchRingMod::advance_carrier() noexcept {
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
        normalization_counter_ = 0U;
    }
}

}  // namespace tabletfx::effects
