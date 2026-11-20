#include "tabletfx/effects/volume_pedal.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kMinimumDb = -96.0F;
constexpr float kMaximumDb = 12.0F;

float decibels_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

} // namespace

VolumePedal::VolumePedal(float sample_rate,
                         VolumePedalParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void VolumePedal::configure(float sample_rate,
                            VolumePedalParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    parameters_.position = bounded(parameters.position, 1.0F, 0.0F, 1.0F);
    parameters_.minimum_db = bounded(parameters.minimum_db, -96.0F,
                                     kMinimumDb, kMaximumDb);
    parameters_.maximum_db = bounded(parameters.maximum_db, 0.0F,
                                     kMinimumDb, kMaximumDb);
    if (parameters_.minimum_db > parameters_.maximum_db) {
        std::swap(parameters_.minimum_db, parameters_.maximum_db);
    }
    parameters_.taper = parameters.taper == VolumePedalTaper::LinearAmplitude
                            ? VolumePedalTaper::LinearAmplitude
                            : VolumePedalTaper::AudioTaperDb;
    parameters_.smoothing_ms = bounded(parameters.smoothing_ms, 10.0F, 0.0F, 1000.0F);
    retarget(true);
}

void VolumePedal::set_position(float position) noexcept {
    if (!std::isfinite(position)) {
        return;
    }
    parameters_.position = std::clamp(position, 0.0F, 1.0F);
    retarget(false);
}

void VolumePedal::set_endpoints_db(float minimum_db, float maximum_db) noexcept {
    parameters_.minimum_db = bounded(minimum_db, parameters_.minimum_db,
                                     kMinimumDb, kMaximumDb);
    parameters_.maximum_db = bounded(maximum_db, parameters_.maximum_db,
                                     kMinimumDb, kMaximumDb);
    if (parameters_.minimum_db > parameters_.maximum_db) {
        std::swap(parameters_.minimum_db, parameters_.maximum_db);
    }
    retarget(false);
}

void VolumePedal::set_taper(VolumePedalTaper taper) noexcept {
    parameters_.taper = taper == VolumePedalTaper::LinearAmplitude
                            ? VolumePedalTaper::LinearAmplitude
                            : VolumePedalTaper::AudioTaperDb;
    retarget(false);
}

void VolumePedal::set_smoothing_ms(float smoothing_ms) noexcept {
    parameters_.smoothing_ms = bounded(smoothing_ms, 10.0F, 0.0F, 1000.0F);
}

void VolumePedal::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (ramp_remaining_ > 0) {
            gain_ += ramp_step_;
            --ramp_remaining_;
            if (ramp_remaining_ == 0) {
                gain_ = target_gain_;
                ramp_step_ = 0.0F;
            }
        }
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            interleaved[offset + channel] *= gain_;
        }
    }
}

void VolumePedal::reset() noexcept {
    gain_ = target_gain_;
    ramp_step_ = 0.0F;
    ramp_remaining_ = 0;
}

float VolumePedal::bounded(float value, float fallback,
                           float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float VolumePedal::target_for_position(float position) const noexcept {
    if (parameters_.taper == VolumePedalTaper::LinearAmplitude) {
        const float minimum_gain = decibels_to_linear(parameters_.minimum_db);
        const float maximum_gain = decibels_to_linear(parameters_.maximum_db);
        return minimum_gain + (maximum_gain - minimum_gain) * position;
    }

    const float gain_db = parameters_.minimum_db +
                          (parameters_.maximum_db - parameters_.minimum_db) * position;
    return decibels_to_linear(gain_db);
}

void VolumePedal::retarget(bool immediate) noexcept {
    target_gain_ = target_for_position(parameters_.position);
    if (immediate) {
        gain_ = target_gain_;
        ramp_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }

    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(parameters_.smoothing_ms) / 1000.0;
    const std::size_t ramp_frames = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_frames == 0) {
        gain_ = target_gain_;
        ramp_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }
    ramp_step_ = (target_gain_ - gain_) / static_cast<float>(ramp_frames);
    ramp_remaining_ = ramp_frames;
}

} // namespace tabletfx
