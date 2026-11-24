#include "tabletfx/effects/octave_divider.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {
namespace {

float checked_sample_rate(float sample_rate) {
    constexpr float kMinimumSampleRate = 8000.0F;
    constexpr float kMaximumSampleRate = 384000.0F;
    if (!std::isfinite(sample_rate) ||
        sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        throw std::invalid_argument("Octave Divider sample rate is unsupported");
    }
    return sample_rate;
}

std::uint16_t checked_channels(std::uint16_t channels) {
    if (channels == 0U || channels > 2U) {
        throw std::invalid_argument("Octave Divider supports mono or stereo");
    }
    return channels;
}

}  // namespace

OctaveDivider::OctaveDivider(float sample_rate, std::uint16_t channels,
                             OctaveDividerParameters parameters)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)) {
    silence_timeout_frames_ = std::max<std::uint32_t>(
        1U, static_cast<std::uint32_t>(std::lround(
                sample_rate_ * kSilenceTimeoutMs / 1000.0F)));
    gate_ramp_frames_ = std::max<std::uint32_t>(
        1U, static_cast<std::uint32_t>(std::lround(
                sample_rate_ * kGateRampMs / 1000.0F)));
    configure(parameters);
}

void OctaveDivider::Ramp::initialize(float value) noexcept {
    current = value;
    target = value;
    step = 0.0F;
    remaining = 0U;
}

void OctaveDivider::Ramp::set(float value, std::uint32_t frames) noexcept {
    target = value;
    remaining = frames;
    if (remaining == 0U) {
        current = target;
        step = 0.0F;
        return;
    }
    step = (target - current) / static_cast<float>(remaining);
}

float OctaveDivider::Ramp::advance() noexcept {
    if (remaining != 0U) {
        current += step;
        --remaining;
        if (remaining == 0U) {
            current = target;
            step = 0.0F;
        }
    }
    return current;
}

void OctaveDivider::Ramp::snap_to_target() noexcept {
    current = target;
    step = 0.0F;
    remaining = 0U;
}

float OctaveDivider::bounded(float value, float fallback,
                             float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return std::clamp(value, minimum, maximum);
}

float OctaveDivider::finite_input(float sample) noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return std::clamp(sample, -kMaximumInputMagnitude,
                      kMaximumInputMagnitude);
}

float OctaveDivider::pan_left(float pan) noexcept {
    constexpr float kHalf = 0.5F;
    return std::sqrt(std::max(0.0F, kHalf * (1.0F - pan)));
}

float OctaveDivider::pan_right(float pan) noexcept {
    constexpr float kHalf = 0.5F;
    return std::sqrt(std::max(0.0F, kHalf * (1.0F + pan)));
}

void OctaveDivider::configure(OctaveDividerParameters requested) noexcept {
    parameters_.input_gain = bounded(requested.input_gain, 1.0F, 0.0F, 4.0F);
    parameters_.voice1_level = bounded(requested.voice1_level, 0.45F, 0.0F, 1.0F);
    parameters_.voice1_pan = bounded(requested.voice1_pan, -0.25F, -1.0F, 1.0F);
    parameters_.voice2_level = bounded(requested.voice2_level, 0.20F, 0.0F, 1.0F);
    parameters_.voice2_pan = bounded(requested.voice2_pan, 0.25F, -1.0F, 1.0F);
    parameters_.mix = bounded(requested.mix, 0.65F, 0.0F, 1.0F);
    parameters_.output_level = bounded(requested.output_level, 1.0F, 0.0F, 2.0F);

    const auto ramp_frames = std::max<std::uint32_t>(
        1U, static_cast<std::uint32_t>(std::lround(
                sample_rate_ * kParameterRampMs / 1000.0F)));
    if (!parameters_initialized_) {
        input_gain_.initialize(parameters_.input_gain);
        voice1_level_.initialize(parameters_.voice1_level);
        voice1_pan_.initialize(parameters_.voice1_pan);
        voice2_level_.initialize(parameters_.voice2_level);
        voice2_pan_.initialize(parameters_.voice2_pan);
        mix_.initialize(parameters_.mix);
        output_level_.initialize(parameters_.output_level);
        parameters_initialized_ = true;
        return;
    }

    input_gain_.set(parameters_.input_gain, ramp_frames);
    voice1_level_.set(parameters_.voice1_level, ramp_frames);
    voice1_pan_.set(parameters_.voice1_pan, ramp_frames);
    voice2_level_.set(parameters_.voice2_level, ramp_frames);
    voice2_pan_.set(parameters_.voice2_pan, ramp_frames);
    mix_.set(parameters_.mix, ramp_frames);
    output_level_.set(parameters_.output_level, ramp_frames);
}

void OctaveDivider::set_gate_target(float target) noexcept {
    if (divider_.gate_target == target) {
        return;
    }
    divider_.gate_target = target;
    divider_.gate_ramp_remaining = gate_ramp_frames_;
    divider_.gate_step =
        (divider_.gate_target - divider_.gate_gain) /
        static_cast<float>(divider_.gate_ramp_remaining);
}

void OctaveDivider::advance_gate() noexcept {
    if (divider_.gate_ramp_remaining == 0U) {
        return;
    }
    divider_.gate_gain += divider_.gate_step;
    --divider_.gate_ramp_remaining;
    if (divider_.gate_ramp_remaining == 0U) {
        divider_.gate_gain = divider_.gate_target;
        divider_.gate_step = 0.0F;
    }
}

void OctaveDivider::process(std::span<float> interleaved,
                            std::uint16_t channels) noexcept {
    if (channels == 0U || channels != channels_ ||
        interleaved.size() % channels != 0U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const float input_gain = input_gain_.advance();
        const float input_left =
            finite_input(interleaved[frame * channels]) * input_gain;
        const float input_right = channels_ == 2U
            ? finite_input(interleaved[frame * channels + 1U]) * input_gain
            : input_left;
        const float divider_input = channels_ == 2U
            ? 0.5F * (input_left + input_right)
            : input_left;

        if (std::abs(divider_input) <= kSilenceThreshold) {
            if (divider_.quiet_frames < silence_timeout_frames_) {
                ++divider_.quiet_frames;
            }
        } else {
            divider_.quiet_frames = 0U;
        }

        if (divider_.quiet_frames >= silence_timeout_frames_) {
            if (divider_.active) {
                divider_.active = false;
                divider_.voice1_high = false;
                divider_.voice2_high = false;
                divider_.edge_armed = true;
                set_gate_target(0.0F);
            }
        } else {
            if (divider_input <= -kEdgeThreshold) {
                divider_.edge_armed = true;
            } else if (divider_.edge_armed &&
                       divider_input >= kEdgeThreshold) {
                divider_.edge_armed = false;
                divider_.active = true;
                set_gate_target(1.0F);

                const bool was_high = divider_.voice1_high;
                divider_.voice1_high = !divider_.voice1_high;
                if (!was_high && divider_.voice1_high) {
                    divider_.voice2_high = !divider_.voice2_high;
                }
            }
        }
        advance_gate();

        const float input_mono = channels_ == 2U
            ? 0.5F * (input_left + input_right)
            : input_left;
        const float dry_left = channels_ == 2U
            ? input_left
            : input_mono;
        const float dry_right = channels_ == 2U
            ? input_right
            : input_mono;
        const float voice1 =
            (divider_.voice1_high ? 1.0F : -1.0F) * divider_.gate_gain;
        const float voice2 =
            (divider_.voice2_high ? 1.0F : -1.0F) * divider_.gate_gain;
        const float level1 = voice1_level_.advance();
        const float pan1 = voice1_pan_.advance();
        const float level2 = voice2_level_.advance();
        const float pan2 = voice2_pan_.advance();
        const float mix = mix_.advance();
        const float output_level = output_level_.advance();

        float wet_left = voice1 * level1 + voice2 * level2;
        float wet_right = wet_left;
        if (channels_ == 2U) {
            wet_left = voice1 * level1 * pan_left(pan1) +
                       voice2 * level2 * pan_left(pan2);
            wet_right = voice1 * level1 * pan_right(pan1) +
                        voice2 * level2 * pan_right(pan2);
        }

        const float left = ((1.0F - mix) * dry_left + mix * wet_left) *
                           output_level;
        const float right = ((1.0F - mix) * dry_right + mix * wet_right) *
                            output_level;
        const std::size_t sample_index = frame * channels_;
        interleaved[sample_index] =
            std::clamp(std::isfinite(left) ? left : 0.0F, -8.0F, 8.0F);
        if (channels_ == 2U) {
            interleaved[sample_index + 1U] =
                std::clamp(std::isfinite(right) ? right : 0.0F, -8.0F, 8.0F);
        }
    }
}

void OctaveDivider::reset() noexcept {
    divider_ = {};
    divider_.edge_armed = true;
    input_gain_.snap_to_target();
    voice1_level_.snap_to_target();
    voice1_pan_.snap_to_target();
    voice2_level_.snap_to_target();
    voice2_pan_.snap_to_target();
    mix_.snap_to_target();
    output_level_.snap_to_target();
}

}  // namespace tabletfx::effects
