#include "tabletfx/signal_controls.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {

SignalControls::SignalControls(float sample_rate, float ramp_ms) noexcept
    : sample_rate_(std::isfinite(sample_rate) && sample_rate >= 8000.0F &&
                           sample_rate <= 384000.0F
                       ? sample_rate
                       : 48000.0F),
      ramp_ms_(std::isfinite(ramp_ms) ? std::clamp(ramp_ms, 0.0F, 1000.0F)
                                     : 10.0F) {}

std::size_t SignalControls::ramp_frames() const noexcept {
    return static_cast<std::size_t>(std::llround(
        static_cast<double>(sample_rate_) * static_cast<double>(ramp_ms_) / 1000.0));
}

void SignalControls::advance(float& current, float target, float step,
                             std::size_t& remaining) noexcept {
    if (remaining == 0) return;
    current += step;
    --remaining;
    if (remaining == 0) current = target;
}

void SignalControls::set_input_muted(bool muted) noexcept {
    input_muted_ = muted;
    input_gain_target_ = muted ? 0.0F : 1.0F;
    input_gain_remaining_ = ramp_frames();
    if (input_gain_remaining_ == 0) {
        input_gain_ = input_gain_target_;
        input_gain_step_ = 0.0F;
    } else {
        input_gain_step_ = (input_gain_target_ - input_gain_) /
                           static_cast<float>(input_gain_remaining_);
    }
}

void SignalControls::set_global_bypassed(bool bypassed) noexcept {
    global_bypassed_ = bypassed;
    retarget_mix();
}

void SignalControls::set_panic_muted(bool muted) noexcept {
    if (muted == panic_muted_) return;
    panic_muted_ = muted;
    if (muted) {
        panic_gain_ = 0.0F;
        panic_gain_step_ = 0.0F;
        panic_gain_remaining_ = 0;
        return;
    }
    panic_gain_ = 0.0F;
    panic_gain_remaining_ = ramp_frames();
    if (panic_gain_remaining_ == 0) {
        panic_gain_ = 1.0F;
        panic_gain_step_ = 0.0F;
    } else {
        panic_gain_step_ = 1.0F / static_cast<float>(panic_gain_remaining_);
    }
}

void SignalControls::set_mix(float wet_mix) noexcept {
    if (!std::isfinite(wet_mix)) return;
    mix_target_ = std::clamp(wet_mix, 0.0F, 1.0F);
    retarget_mix();
}

void SignalControls::retarget_mix() noexcept {
    effective_mix_target_ = global_bypassed_ ? 0.0F : mix_target_;
    mix_remaining_ = ramp_frames();
    if (mix_remaining_ == 0) {
        mix_ = effective_mix_target_;
        mix_step_ = 0.0F;
    } else {
        mix_step_ = (effective_mix_target_ - mix_) / static_cast<float>(mix_remaining_);
    }
}

void SignalControls::process_input(std::span<const float> input,
                                   std::span<float> output,
                                   std::uint16_t channels) noexcept {
    if (channels == 0) return;
    const std::size_t frames = std::min(input.size(), output.size()) / channels;
    const std::size_t samples = frames * channels;
    if (input_gain_remaining_ == 0 && input_gain_ == 1.0F) {
        std::copy_n(input.begin(), samples, output.begin());
        return;
    }
    if (input_gain_remaining_ == 0 && input_gain_ == 0.0F) {
        std::fill_n(output.begin(), samples, 0.0F);
        return;
    }
    for (std::size_t frame = 0; frame < frames; ++frame) {
        advance(input_gain_, input_gain_target_, input_gain_step_, input_gain_remaining_);
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            output[offset + channel] = input[offset + channel] * input_gain_;
        }
    }
}

void SignalControls::process_output(std::span<const float> dry,
                                    std::span<const float> wet,
                                    std::span<float> output,
                                    std::uint16_t channels) noexcept {
    if (channels == 0) return;
    const std::size_t frames = std::min({dry.size(), wet.size(), output.size()}) / channels;
    const std::size_t samples = frames * channels;
    if (panic_muted_) {
        std::fill_n(output.begin(), samples, 0.0F);
        return;
    }
    if (panic_gain_remaining_ == 0 && mix_remaining_ == 0 && panic_gain_ == 1.0F) {
        if (mix_ <= 0.0F) {
            std::copy_n(dry.begin(), samples, output.begin());
            return;
        }
        if (mix_ >= 1.0F) {
            std::copy_n(wet.begin(), samples, output.begin());
            return;
        }
    }
    for (std::size_t frame = 0; frame < frames; ++frame) {
        advance(mix_, effective_mix_target_, mix_step_, mix_remaining_);
        if (!panic_muted_) {
            advance(panic_gain_, 1.0F, panic_gain_step_, panic_gain_remaining_);
        }
        const float dry_gain = 1.0F - mix_;
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            output[offset + channel] =
                (dry[offset + channel] * dry_gain + wet[offset + channel] * mix_) *
                panic_gain_;
        }
    }
}

void SignalControls::reset() noexcept {
    input_gain_ = input_gain_target_;
    input_gain_step_ = 0.0F;
    input_gain_remaining_ = 0;
    mix_ = effective_mix_target_;
    mix_step_ = 0.0F;
    mix_remaining_ = 0;
    panic_gain_ = panic_muted_ ? 0.0F : 1.0F;
    panic_gain_step_ = 0.0F;
    panic_gain_remaining_ = 0;
}

} // namespace tabletfx
