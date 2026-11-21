#include "tabletfx/effects/auto_swell.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kMinimumTimeMs = 1.0F;
constexpr float kMaximumTimeMs = 5000.0F;
constexpr float kMinimumThresholdDb = -90.0F;
constexpr float kMaximumThresholdDb = 0.0F;
constexpr float kMaximumHysteresisDb = 36.0F;
constexpr float kMaximumVolume = 2.0F;
constexpr float kDetectorAttackMs = 1.0F;
constexpr float kDetectorReleaseMs = 45.0F;

float decibels_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float one_pole_coefficient(float sample_rate, float time_ms) noexcept {
    const double time_seconds = static_cast<double>(time_ms) / 1000.0;
    const double denominator = static_cast<double>(sample_rate) * time_seconds;
    return static_cast<float>(-std::expm1(-1.0 / denominator));
}

} // namespace

AutoSwell::AutoSwell(float sample_rate, AutoSwellParameters parameters) noexcept {
    configure(sample_rate, parameters);
    reset();
}

void AutoSwell::configure(float sample_rate,
                          AutoSwellParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;

    parameters_.threshold_db = bounded(parameters.threshold_db, -36.0F,
                                        kMinimumThresholdDb,
                                        kMaximumThresholdDb);
    parameters_.attack_ms = bounded(parameters.attack_ms, 250.0F,
                                    kMinimumTimeMs, kMaximumTimeMs);
    parameters_.release_ms = bounded(parameters.release_ms, 250.0F,
                                     kMinimumTimeMs, kMaximumTimeMs);
    parameters_.hysteresis_db = bounded(parameters.hysteresis_db, 6.0F,
                                        0.0F, kMaximumHysteresisDb);
    parameters_.taper = valid_taper(parameters.taper);
    parameters_.volume = bounded(parameters.volume, 1.0F, 0.0F,
                                 kMaximumVolume);

    threshold_linear_ = decibels_to_linear(parameters_.threshold_db);
    reset_threshold_linear_ = decibels_to_linear(
        parameters_.threshold_db - parameters_.hysteresis_db);
    detector_attack_coefficient_ = one_pole_coefficient(
        sample_rate_, kDetectorAttackMs);
    detector_release_coefficient_ = one_pole_coefficient(
        sample_rate_, kDetectorReleaseMs);
    attack_step_ = 1000.0F / (sample_rate_ * parameters_.attack_ms);
    release_step_ = 1000.0F / (sample_rate_ * parameters_.release_ms);
    build_taper_table();
}

void AutoSwell::process(std::span<float> interleaved,
                        std::uint16_t channels) noexcept {
    if (channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        float peak = 0.0F;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const float sample = interleaved[offset + channel];
            if (std::isfinite(sample)) {
                peak = std::max(peak, std::abs(sample));
            }
        }

        const float detector_coefficient =
            peak > envelope_ ? detector_attack_coefficient_
                             : detector_release_coefficient_;
        envelope_ += (peak - envelope_) * detector_coefficient;

        if (!triggered_ && envelope_ >= threshold_linear_) {
            triggered_ = true;
            // Each newly detected note starts a fresh rise, even if the prior
            // release is still in progress after a short quiet gap.
            phase_ = 0.0F;
        } else if (triggered_ && envelope_ <= reset_threshold_linear_) {
            triggered_ = false;
        }

        if (triggered_) {
            phase_ = std::min(1.0F, phase_ + attack_step_);
        } else {
            phase_ = std::max(0.0F, phase_ - release_step_);
        }
        gain_ = parameters_.volume * lookup_taper(phase_);
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            float& sample = interleaved[offset + channel];
            sample = std::isfinite(sample) ? sample * gain_ : 0.0F;
        }
    }
}

void AutoSwell::reset() noexcept {
    envelope_ = 0.0F;
    phase_ = 0.0F;
    gain_ = 0.0F;
    triggered_ = false;
}

float AutoSwell::bounded(float value, float fallback,
                         float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

AutoSwellTaper AutoSwell::valid_taper(AutoSwellTaper taper) noexcept {
    switch (taper) {
    case AutoSwellTaper::Linear:
    case AutoSwellTaper::Log30A:
    case AutoSwellTaper::Log20A:
    case AutoSwellTaper::Log15A:
    case AutoSwellTaper::Log10A:
    case AutoSwellTaper::Log5A:
    case AutoSwellTaper::SwellS:
        return taper;
    }
    return AutoSwellTaper::SwellS;
}

float AutoSwell::taper_value(AutoSwellTaper taper, float position) noexcept {
    switch (taper) {
    case AutoSwellTaper::Linear:
        return position;
    case AutoSwellTaper::Log30A:
    case AutoSwellTaper::Log20A:
    case AutoSwellTaper::Log15A:
    case AutoSwellTaper::Log10A:
    case AutoSwellTaper::Log5A: {
        // The guide names pot-taper families without publishing their curves.
        // These power curves are project-authored approximations whose value at
        // halfway matches the nominal suffix; they do not claim Fractal parity.
        float midpoint = 0.30F;
        if (taper == AutoSwellTaper::Log20A) midpoint = 0.20F;
        if (taper == AutoSwellTaper::Log15A) midpoint = 0.15F;
        if (taper == AutoSwellTaper::Log10A) midpoint = 0.10F;
        if (taper == AutoSwellTaper::Log5A) midpoint = 0.05F;
        const float exponent = std::log(midpoint) / std::log(0.5F);
        return std::pow(position, exponent);
    }
    case AutoSwellTaper::SwellS:
        return position * position * (3.0F - 2.0F * position);
    }
    return position;
}

float AutoSwell::lookup_taper(float position) const noexcept {
    const float table_position =
        std::clamp(position, 0.0F, 1.0F) * static_cast<float>(kTaperSteps);
    const std::size_t index = static_cast<std::size_t>(table_position);
    if (index >= kTaperSteps) {
        return taper_table_[kTaperSteps];
    }
    const float fraction = table_position - static_cast<float>(index);
    const float lower = taper_table_[index];
    const float upper = taper_table_[index + 1];
    return lower + (upper - lower) * fraction;
}

void AutoSwell::build_taper_table() noexcept {
    for (std::size_t index = 0; index <= kTaperSteps; ++index) {
        const float position = static_cast<float>(index) /
                               static_cast<float>(kTaperSteps);
        taper_table_[index] = taper_value(parameters_.taper, position);
    }
}

} // namespace tabletfx
