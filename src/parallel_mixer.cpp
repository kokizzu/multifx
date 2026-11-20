#include "tabletfx/parallel_mixer.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace tabletfx {
namespace {

bool ranges_overlap(const float* first, std::size_t first_count,
                    const float* second, std::size_t second_count) noexcept {
    if (first_count == 0 || second_count == 0) return false;
    const float* first_end = first + first_count;
    const float* second_end = second + second_count;
    const std::less<const float*> less;
    return less(first, second_end) && less(second, first_end);
}

bool partial_overlap(std::span<const float> input, std::span<float> output,
                     std::size_t samples) noexcept {
    return ranges_overlap(input.data(), samples, output.data(), samples) &&
           input.data() != output.data();
}

float bounded(float value, float low, float high, float fallback) noexcept {
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}

} // namespace

ChannelMixResult split_to_parallel(std::span<const float> input,
                                   std::span<float> branch_a,
                                   std::span<float> branch_b,
                                   std::uint16_t channels) noexcept {
    if (channels != 1 && channels != 2) {
        return {ChannelMixStatus::UnsupportedChannelCount, 0};
    }
    if (input.size() % channels != 0) {
        return {ChannelMixStatus::InvalidInputLength, 0};
    }
    const std::size_t samples = input.size();
    if (branch_a.size() < samples || branch_b.size() < samples) {
        return {ChannelMixStatus::BufferTooSmall, 0};
    }
    if (ranges_overlap(branch_a.data(), samples, branch_b.data(), samples) ||
        partial_overlap(input, branch_a, samples) ||
        partial_overlap(input, branch_b, samples)) {
        return {ChannelMixStatus::OverlappingBuffers, 0};
    }
    if (samples == 0) return {ChannelMixStatus::Success, 0};
    if (input.data() != branch_a.data()) {
        for (std::size_t i = 0; i < samples; ++i) branch_a[i] = input[i];
    }
    if (input.data() != branch_b.data()) {
        for (std::size_t i = 0; i < samples; ++i) branch_b[i] = input[i];
    }
    return {ChannelMixStatus::Success, samples / channels};
}

ParallelMixer::ParallelMixer(float sample_rate, float smoothing_ms) noexcept {
    configure_smoothing(sample_rate, smoothing_ms);
    set_parameters_immediate({});
}

void ParallelMixer::configure_smoothing(float sample_rate, float smoothing_ms) noexcept {
    sample_rate_ = std::isfinite(sample_rate) && sample_rate >= 8000.0F &&
                           sample_rate <= 384000.0F
                       ? sample_rate
                       : 48000.0F;
    smoothing_ms_ = std::isfinite(smoothing_ms)
                        ? std::clamp(smoothing_ms, 0.0F, 1000.0F)
                        : 10.0F;
    const double requested = static_cast<double>(sample_rate_) *
                             static_cast<double>(smoothing_ms_) / 1000.0;
    ramp_frames_ = static_cast<std::size_t>(std::llround(requested));
}

std::array<float, 7> ParallelMixer::coefficients(ParallelMixParameters parameters) noexcept {
    const float dry = bounded(parameters.dry_gain, 0.0F, 2.0F, 1.0F);
    const float gain_a = bounded(parameters.branch_a_gain, 0.0F, 2.0F, 0.0F);
    const float balance_a = bounded(parameters.branch_a_balance, -1.0F, 1.0F, 0.0F);
    const float phase_a = parameters.branch_a_inverted ? -1.0F : 1.0F;
    const float gain_b = bounded(parameters.branch_b_gain, 0.0F, 2.0F, 0.0F);
    const float balance_b = bounded(parameters.branch_b_balance, -1.0F, 1.0F, 0.0F);
    const float phase_b = parameters.branch_b_inverted ? -1.0F : 1.0F;
    const float a_left = balance_a > 0.0F ? 1.0F - balance_a : 1.0F;
    const float a_right = balance_a < 0.0F ? 1.0F + balance_a : 1.0F;
    const float b_left = balance_b > 0.0F ? 1.0F - balance_b : 1.0F;
    const float b_right = balance_b < 0.0F ? 1.0F + balance_b : 1.0F;
    return {dry, gain_a * phase_a, gain_a * a_left * phase_a,
            gain_a * a_right * phase_a, gain_b * phase_b,
            gain_b * b_left * phase_b, gain_b * b_right * phase_b};
}

void ParallelMixer::apply_immediately(const std::array<float, 7>& values) noexcept {
    current_ = values;
    target_ = values;
    step_.fill(0.0F);
    ramp_remaining_ = 0;
}

void ParallelMixer::set_parameters(ParallelMixParameters parameters) noexcept {
    target_ = coefficients(parameters);
    if (ramp_frames_ == 0) {
        apply_immediately(target_);
        return;
    }
    const float frames = static_cast<float>(ramp_frames_);
    for (std::size_t i = 0; i < current_.size(); ++i) {
        step_[i] = (target_[i] - current_[i]) / frames;
    }
    ramp_remaining_ = ramp_frames_;
}

void ParallelMixer::set_parameters_immediate(ParallelMixParameters parameters) noexcept {
    apply_immediately(coefficients(parameters));
}

ChannelMixResult ParallelMixer::process(std::span<const float> dry,
                                        std::span<const float> branch_a,
                                        std::span<const float> branch_b,
                                        std::span<float> output,
                                        std::uint16_t channels) noexcept {
    if (channels != 1 && channels != 2) {
        return {ChannelMixStatus::UnsupportedChannelCount, 0};
    }
    if (dry.size() % channels != 0) {
        return {ChannelMixStatus::InvalidInputLength, 0};
    }
    const std::size_t samples = dry.size();
    if (branch_a.size() < samples || branch_b.size() < samples || output.size() < samples) {
        return {ChannelMixStatus::BufferTooSmall, 0};
    }
    if (partial_overlap(dry, output, samples) ||
        partial_overlap(branch_a, output, samples) ||
        partial_overlap(branch_b, output, samples)) {
        return {ChannelMixStatus::OverlappingBuffers, 0};
    }

    const std::size_t frames = samples / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (ramp_remaining_ > 0) {
            for (std::size_t i = 0; i < current_.size(); ++i) current_[i] += step_[i];
            --ramp_remaining_;
            if (ramp_remaining_ == 0) {
                current_ = target_;
                step_.fill(0.0F);
            }
        }

        const std::size_t offset = frame * channels;
        const float dry_left = dry[offset];
        const float a_left = branch_a[offset];
        const float b_left = branch_b[offset];
        if (channels == 1) {
            output[offset] = dry_left * current_[0] +
                             a_left * current_[1] + b_left * current_[4];
        } else {
            const float dry_right = dry[offset + 1];
            const float a_right = branch_a[offset + 1];
            const float b_right = branch_b[offset + 1];
            output[offset] = dry_left * current_[0] +
                             a_left * current_[2] + b_left * current_[5];
            output[offset + 1] = dry_right * current_[0] +
                                 a_right * current_[3] + b_right * current_[6];
        }
    }
    return {ChannelMixStatus::Success, frames};
}

void ParallelMixer::reset() noexcept {
    apply_immediately(target_);
}

} // namespace tabletfx
