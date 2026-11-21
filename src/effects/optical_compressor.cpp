#include "tabletfx/effects/optical_compressor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tabletfx {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 192000.0F;
constexpr float kMinimumThresholdDb = -60.0F;
constexpr float kMaximumThresholdDb = 0.0F;
constexpr float kMinimumRatio = 1.0F;
constexpr float kMaximumRatio = 20.0F;
constexpr float kMinimumKneeDb = 0.0F;
constexpr float kMaximumKneeDb = 18.0F;
constexpr float kMinimumAttackMs = 0.1F;
constexpr float kMaximumAttackMs = 200.0F;
constexpr float kMinimumReleaseMs = 5.0F;
constexpr float kMaximumReleaseMs = 2000.0F;
constexpr float kMaximumProgramReleaseMs = 5000.0F;
constexpr float kMinimumRmsWindowMs = 1.0F;
constexpr float kMaximumRmsWindowMs = 100.0F;
constexpr float kMinimumMakeupDb = -12.0F;
constexpr float kMaximumMakeupDb = 18.0F;
constexpr float kProgramMemoryAttackMs = 100.0F;
constexpr float kProgramMemoryReferenceReductionDb = 24.0F;
constexpr double kMaximumInputMagnitude = 1.0e20;
constexpr double kMaximumDetectorPower = 1.0e40;
constexpr double kDenormalGuard = 1.0e-30;

} // namespace

OpticalCompressor::OpticalCompressor(
    float sample_rate, OpticalCompressorParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void OpticalCompressor::configure(float sample_rate,
                                  OpticalCompressorParameters parameters) noexcept {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        sample_rate = 48000.0F;
    }
    sample_rate_ = sample_rate;

    parameters_.threshold_db = bounded(parameters.threshold_db, -24.0F,
                                       kMinimumThresholdDb, kMaximumThresholdDb);
    parameters_.ratio = bounded(parameters.ratio, 3.0F,
                                kMinimumRatio, kMaximumRatio);
    parameters_.knee_db = bounded(parameters.knee_db, 6.0F,
                                  kMinimumKneeDb, kMaximumKneeDb);
    parameters_.attack_ms = bounded(parameters.attack_ms, 10.0F,
                                    kMinimumAttackMs, kMaximumAttackMs);
    parameters_.release_ms = bounded(parameters.release_ms, 80.0F,
                                      kMinimumReleaseMs, kMaximumReleaseMs);
    parameters_.program_release_ms = bounded(parameters.program_release_ms, 600.0F,
                                              kMinimumReleaseMs,
                                              kMaximumProgramReleaseMs);
    parameters_.program_release_ms =
        std::max(parameters_.program_release_ms, parameters_.release_ms);
    parameters_.rms_window_ms = bounded(parameters.rms_window_ms, 10.0F,
                                        kMinimumRmsWindowMs, kMaximumRmsWindowMs);
    parameters_.makeup_db = bounded(parameters.makeup_db, 0.0F,
                                    kMinimumMakeupDb, kMaximumMakeupDb);

    threshold_db_ = parameters_.threshold_db;
    makeup_gain_ = decibels_to_linear(parameters_.makeup_db);
    compression_slope_ = 1.0F - 1.0F / parameters_.ratio;
    rms_coefficient_ = time_coefficient(parameters_.rms_window_ms, sample_rate_);
    attack_coefficient_ = time_coefficient(parameters_.attack_ms, sample_rate_);
    program_release_coefficient_ =
        time_coefficient(parameters_.program_release_ms, sample_rate_);
    program_attack_coefficient_ = time_coefficient(kProgramMemoryAttackMs, sample_rate_);
    for (std::size_t index = 0; index < kReleaseTableSteps; ++index) {
        const float fraction = static_cast<float>(index) /
                               static_cast<float>(kReleaseTableSteps - 1);
        const float release_ms = parameters_.release_ms +
            (parameters_.program_release_ms - parameters_.release_ms) * fraction;
        gain_release_coefficients_[index] = time_coefficient(release_ms, sample_rate_);
    }

    if (!state_initialized_) {
        gain_ = makeup_gain_;
        state_initialized_ = true;
    }
}

void OpticalCompressor::set_parameters(OpticalCompressorParameters parameters) noexcept {
    configure(sample_rate_, parameters);
}

void OpticalCompressor::process(std::span<float> interleaved,
                                std::uint16_t channels) noexcept {
    if (channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    const double channel_count = static_cast<double>(channels);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        double power_sum = 0.0;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            if (!std::isfinite(sample)) {
                sample = 0.0F;
            } else if (static_cast<double>(sample) > kMaximumInputMagnitude) {
                sample = static_cast<float>(kMaximumInputMagnitude);
            } else if (static_cast<double>(sample) < -kMaximumInputMagnitude) {
                sample = static_cast<float>(-kMaximumInputMagnitude);
            }
            const double input = static_cast<double>(sample);
            power_sum += input * input;
        }

        const double input_power = std::min(power_sum / channel_count,
                                            kMaximumDetectorPower);
        rms_power_ = rms_coefficient_ * rms_power_ +
                     (1.0 - rms_coefficient_) * input_power;
        if (rms_power_ < kDenormalGuard) {
            rms_power_ = 0.0;
        }
        const float rms_level = static_cast<float>(std::sqrt(rms_power_));
        const float target_reduction_db = gain_reduction_db(rms_level);
        const float program_target = std::clamp(
            target_reduction_db / kProgramMemoryReferenceReductionDb, 0.0F, 1.0F);
        const float program_coefficient = program_target > program_reduction_
            ? program_attack_coefficient_
            : program_release_coefficient_;
        program_reduction_ = program_coefficient * program_reduction_ +
                             (1.0F - program_coefficient) * program_target;
        program_reduction_ = std::clamp(program_reduction_, 0.0F, 1.0F);

        const float target_gain = makeup_gain_ *
            decibels_to_linear(-target_reduction_db);
        const float release_position = program_reduction_ *
            static_cast<float>(kReleaseTableSteps - 1);
        const std::size_t release_index = static_cast<std::size_t>(release_position);
        const std::size_t next_release_index =
            std::min(release_index + 1, kReleaseTableSteps - 1);
        const float release_fraction = release_position -
                                      static_cast<float>(release_index);
        const float release_coefficient = gain_release_coefficients_[release_index] +
            (gain_release_coefficients_[next_release_index] -
             gain_release_coefficients_[release_index]) * release_fraction;
        const float gain_coefficient = target_gain < gain_
            ? attack_coefficient_
            : release_coefficient;
        gain_ = gain_coefficient * gain_ + (1.0F - gain_coefficient) * target_gain;
        if (gain_ < static_cast<float>(kDenormalGuard)) {
            gain_ = 0.0F;
        }

        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const std::size_t index = offset + channel;
            const double output = static_cast<double>(interleaved[index]) *
                                  static_cast<double>(gain_);
            interleaved[index] = static_cast<float>(std::clamp(
                output,
                -static_cast<double>(std::numeric_limits<float>::max()),
                static_cast<double>(std::numeric_limits<float>::max())));
        }
    }
}

void OpticalCompressor::reset() noexcept {
    rms_power_ = 0.0;
    program_reduction_ = 0.0F;
    gain_ = makeup_gain_;
}

float OpticalCompressor::bounded(float value, float fallback,
                                 float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float OpticalCompressor::decibels_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float OpticalCompressor::time_coefficient(float milliseconds,
                                          float sample_rate) noexcept {
    return std::exp(-1.0F / (milliseconds * 0.001F * sample_rate));
}

float OpticalCompressor::gain_reduction_db(float detector_level) const noexcept {
    if (detector_level <= 0.0F || compression_slope_ <= 0.0F) {
        return 0.0F;
    }

    const float level_db = 20.0F * std::log10(detector_level);
    const float knee = parameters_.knee_db;
    const float lower_knee = threshold_db_ - 0.5F * knee;
    const float upper_knee = threshold_db_ + 0.5F * knee;
    if (level_db >= upper_knee) {
        return compression_slope_ * (level_db - threshold_db_);
    }
    if (knee > 0.0F && level_db > lower_knee) {
        const float knee_position = level_db - lower_knee;
        return compression_slope_ * knee_position * knee_position /
               (2.0F * knee);
    }
    return 0.0F;
}

} // namespace tabletfx
