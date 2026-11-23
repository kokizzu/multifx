#include "tabletfx/effects/volume_pan.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kMinimumDb = -96.0F;
constexpr float kMaximumDb = 12.0F;
constexpr float kMinimumOutputDb = -24.0F;
constexpr float kMaximumOutputDb = 12.0F;

float decibels_to_linear(float decibels) noexcept {
    return std::pow(10.0F, decibels / 20.0F);
}

float safe_audio(float sample) noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return std::clamp(sample, -8.0F, 8.0F);
}

} // namespace

VolumePan::VolumePan(VolumePanParameters parameters) noexcept {
    set_parameters(parameters);
}

void VolumePan::set_parameters(VolumePanParameters parameters) noexcept {
    parameters_ = parameters;
    parameters_.volume = bounded(parameters_.volume, 1.0F, 0.0F, 1.0F);
    parameters_.minimum_db = bounded(parameters_.minimum_db, -96.0F,
                                     kMinimumDb, kMaximumDb);
    parameters_.maximum_db = bounded(parameters_.maximum_db, 0.0F,
                                     kMinimumDb, kMaximumDb);
    if (parameters_.minimum_db > parameters_.maximum_db) {
        std::swap(parameters_.minimum_db, parameters_.maximum_db);
    }
    parameters_.taper = sanitize_taper(parameters_.taper);
    parameters_.balance_percent = bounded(parameters_.balance_percent, 0.0F,
                                           -100.0F, 100.0F);
    parameters_.input_select = sanitize_input_select(parameters_.input_select);
    parameters_.pan_left_percent = bounded(parameters_.pan_left_percent, -100.0F,
                                           -100.0F, 100.0F);
    parameters_.pan_right_percent = bounded(parameters_.pan_right_percent, 100.0F,
                                            -100.0F, 100.0F);
    parameters_.output_level_db = bounded(parameters_.output_level_db, 0.0F,
                                          kMinimumOutputDb,
                                          kMaximumOutputDb);
    parameters_.bypass_mode = sanitize_bypass_mode(parameters_.bypass_mode);
    prepare_gains();
}

void VolumePan::process(std::span<float> interleaved,
                        std::uint16_t channels) noexcept {
    if (channels == 0U || interleaved.empty()) {
        return;
    }
    if (parameters_.bypassed) {
        if (parameters_.bypass_mode == VolumePanBypassMode::Mute) {
            std::fill(interleaved.begin(), interleaved.end(), 0.0F);
        }
        return;
    }

    const float shared_gain = volume_gain_ * output_gain_;
    if (channels == 1U) {
        for (float& sample : interleaved) {
            sample = safe_audio(sample * shared_gain);
        }
        return;
    }

    const std::size_t frame_count = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
        const std::size_t offset = frame * channels;
        float left = safe_audio(interleaved[offset]);
        float right = safe_audio(interleaved[offset + 1U]);
        switch (parameters_.input_select) {
        case VolumePanInputSelect::Stereo:
            break;
        case VolumePanInputSelect::LeftOnly:
            right = 0.0F;
            break;
        case VolumePanInputSelect::RightOnly:
            left = 0.0F;
            break;
        }

        const float panned_left =
            left * left_input_left_gain_ + right * right_input_left_gain_;
        const float panned_right =
            left * left_input_right_gain_ + right * right_input_right_gain_;
        interleaved[offset] = safe_audio(
            panned_left * shared_gain * left_balance_gain_);
        interleaved[offset + 1U] = safe_audio(
            panned_right * shared_gain * right_balance_gain_);

        for (std::uint16_t channel = 2U; channel < channels; ++channel) {
            interleaved[offset + channel] =
                safe_audio(interleaved[offset + channel] * shared_gain);
        }
    }
}

void VolumePan::reset() noexcept {
    // This utility has no delay or oscillator state. Parameter-derived gains
    // are already prepared by set_parameters().
}

float VolumePan::bounded(float value, float fallback,
                         float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return std::clamp(value, minimum, maximum);
}

VolumePanTaper VolumePan::sanitize_taper(VolumePanTaper taper) noexcept {
    switch (taper) {
    case VolumePanTaper::Linear:
    case VolumePanTaper::Log30A:
    case VolumePanTaper::Log20A:
    case VolumePanTaper::Log15A:
    case VolumePanTaper::Log10A:
    case VolumePanTaper::Log5A:
    case VolumePanTaper::STaper:
        return taper;
    }
    return VolumePanTaper::Linear;
}

VolumePanInputSelect VolumePan::sanitize_input_select(
    VolumePanInputSelect input_select) noexcept {
    switch (input_select) {
    case VolumePanInputSelect::Stereo:
    case VolumePanInputSelect::LeftOnly:
    case VolumePanInputSelect::RightOnly:
        return input_select;
    }
    return VolumePanInputSelect::Stereo;
}

VolumePanBypassMode VolumePan::sanitize_bypass_mode(
    VolumePanBypassMode bypass_mode) noexcept {
    switch (bypass_mode) {
    case VolumePanBypassMode::Pass:
    case VolumePanBypassMode::Mute:
        return bypass_mode;
    }
    return VolumePanBypassMode::Pass;
}

float VolumePan::volume_curve(float position, VolumePanTaper taper) noexcept {
    position = std::clamp(position, 0.0F, 1.0F);
    if (taper == VolumePanTaper::Linear) {
        return position;
    }
    if (taper == VolumePanTaper::STaper) {
        return position * position * (3.0F - 2.0F * position);
    }

    float midpoint_db = -20.0F;
    switch (taper) {
    case VolumePanTaper::Log30A: midpoint_db = -30.0F; break;
    case VolumePanTaper::Log20A: midpoint_db = -20.0F; break;
    case VolumePanTaper::Log15A: midpoint_db = -15.0F; break;
    case VolumePanTaper::Log10A: midpoint_db = -10.0F; break;
    case VolumePanTaper::Log5A: midpoint_db = -5.0F; break;
    case VolumePanTaper::Linear:
    case VolumePanTaper::STaper:
        return position;
    }

    const float minimum_gain = decibels_to_linear(kMinimumDb);
    const float maximum_gain = 1.0F;
    const float midpoint_gain = decibels_to_linear(midpoint_db);
    const float midpoint_fraction = std::clamp(
        (midpoint_gain - minimum_gain) / (maximum_gain - minimum_gain),
        1.0e-6F, 1.0F - 1.0e-6F);
    const float exponent = std::log(midpoint_fraction) / std::log(0.5F);
    return std::pow(position, exponent);
}

void VolumePan::pan_gains(float position, float& left_gain,
                          float& right_gain) noexcept {
    // Project-authored equal-power law. -100 is hard left; +100 is hard right.
    const float normalized = std::clamp((position + 100.0F) / 200.0F,
                                        0.0F, 1.0F);
    if (normalized <= 0.0F) {
        left_gain = 1.0F;
        right_gain = 0.0F;
        return;
    }
    if (normalized >= 1.0F) {
        left_gain = 0.0F;
        right_gain = 1.0F;
        return;
    }
    const float angle = normalized * (kPi * 0.5F);
    left_gain = std::cos(angle);
    right_gain = std::sin(angle);
}

void VolumePan::prepare_gains() noexcept {
    const float minimum_gain = decibels_to_linear(parameters_.minimum_db);
    const float maximum_gain = decibels_to_linear(parameters_.maximum_db);
    const float curve = volume_curve(parameters_.volume, parameters_.taper);
    volume_gain_ = minimum_gain + (maximum_gain - minimum_gain) * curve;
    output_gain_ = decibels_to_linear(parameters_.output_level_db);

    pan_gains(parameters_.pan_left_percent,
              left_input_left_gain_, left_input_right_gain_);
    pan_gains(parameters_.pan_right_percent,
              right_input_left_gain_, right_input_right_gain_);

    if (parameters_.balance_percent < 0.0F) {
        left_balance_gain_ = 1.0F;
        right_balance_gain_ = 1.0F + parameters_.balance_percent / 100.0F;
    } else {
        left_balance_gain_ = 1.0F - parameters_.balance_percent / 100.0F;
        right_balance_gain_ = 1.0F;
    }
}

} // namespace tabletfx
