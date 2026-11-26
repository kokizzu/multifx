#include "tabletfx/effects/modulated_delay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr double kMaximumFloat =
    static_cast<double>(std::numeric_limits<float>::max());
constexpr float kTwoPi = 6.2831853071795864769F;

float bounded_sample(double value) noexcept {
    return static_cast<float>(std::clamp(value, -kMaximumFloat, kMaximumFloat));
}

} // namespace

ModulatedDelay::ModulatedDelay(float sample_rate, std::uint16_t channels,
                               float max_delay_ms,
                               ModulatedDelayParameters parameters)
    : sample_rate_(sample_rate), channels_(channels), max_delay_ms_(max_delay_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < 1.0F ||
        sample_rate_ > 384000.0F || channels_ == 0 || channels_ > 2 ||
        !std::isfinite(max_delay_ms_) || max_delay_ms_ < kMinimumTimeMs ||
        max_delay_ms_ > 6000.0F) {
        throw std::invalid_argument(
            "Modulated Delay requires a supported sample rate, mono/stereo layout, and maximum time in [1, 6000] ms");
    }

    const double maximum_delay_samples = std::ceil(
        static_cast<double>(sample_rate_) * static_cast<double>(max_delay_ms_) / 1000.0);
    const std::size_t sample_capacity_limit = storage_.max_size() / channels_;
    if (!std::isfinite(maximum_delay_samples) || maximum_delay_samples < 1.0 ||
        maximum_delay_samples > static_cast<double>(sample_capacity_limit - 3U)) {
        throw std::length_error("Modulated Delay buffer size is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(maximum_delay_samples) + 3U;
    storage_.assign(capacity_frames_ * channels_, 0.0F);
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::ceil(
                static_cast<double>(sample_rate_) * kControlRampMs / 1000.0)));

    configure(parameters);
    current_time_samples_ = target_time_samples_;
    current_feedback_ = parameters_.feedback;
    current_mix_ = parameters_.mix;
    current_rate_hz_ = parameters_.rate_hz;
    current_depth_samples_ = static_cast<float>(
        static_cast<double>(sample_rate_) * parameters_.depth_ms / 1000.0);
    current_spread_degrees_ = parameters_.stereo_spread_degrees;
    current_input_gain_ = std::pow(10.0F, parameters_.input_gain_db / 20.0F);
    current_grit_amplitude_ = parameters_.grit_percent / 100.0F *
                              kMaximumGritNoiseAmplitude;
    current_vibrato_blend_ = parameters_.mode == ModulatedDelayMode::vibrato
                                 ? 1.0F
                                 : 0.0F;
    time_step_ = 0.0F;
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    rate_step_ = 0.0F;
    depth_step_ = 0.0F;
    spread_step_ = 0.0F;
    input_gain_step_ = 0.0F;
    grit_amplitude_step_ = 0.0F;
    vibrato_blend_step_ = 0.0F;
    ramp_remaining_ = 0;
    reset();
}

void ModulatedDelay::configure(ModulatedDelayParameters parameters) noexcept {
    parameters.time_ms = bounded(parameters.time_ms, 350.0F,
                                 kMinimumTimeMs,
                                 std::min(kMaximumTimeMs, max_delay_ms_));
    parameters.feedback = bounded(parameters.feedback, 0.25F,
                                  0.0F, kMaximumFeedback);
    parameters.mix = bounded(parameters.mix, 0.25F, 0.0F, 1.0F);
    parameters.rate_hz = bounded(parameters.rate_hz, 0.8F, 0.05F,
                                 kMaximumRateHz);
    const float available_depth_ms = std::max(
        0.0F, std::min(parameters.time_ms - kMinimumTimeMs,
                       max_delay_ms_ - parameters.time_ms));
    parameters.depth_ms = bounded(parameters.depth_ms, 2.0F, 0.0F,
                                  std::min(kMaximumDepthMs, available_depth_ms));
    parameters.stereo_spread_degrees = bounded(
        parameters.stereo_spread_degrees, 180.0F, 0.0F, 180.0F);
    parameters.trigger_threshold_db = bounded(
        parameters.trigger_threshold_db, -24.0F, kMinimumTriggerDb, 0.0F);
    switch (parameters.waveform) {
    case ModulatedDelayWaveform::sine:
    case ModulatedDelayWaveform::triangle:
    case ModulatedDelayWaveform::random:
    case ModulatedDelayWaveform::triggered_ramp:
        break;
    default:
        parameters.waveform = ModulatedDelayWaveform::sine;
        break;
    }

    const float new_time_samples = static_cast<float>(samples_for_ms(parameters.time_ms));
    target_time_samples_ = new_time_samples;
    const float new_depth_samples = static_cast<float>(
        static_cast<double>(sample_rate_) * parameters.depth_ms / 1000.0);
    time_step_ = (new_time_samples - current_time_samples_) /
                 static_cast<float>(ramp_total_);
    feedback_step_ = (parameters.feedback - current_feedback_) /
                     static_cast<float>(ramp_total_);
    mix_step_ = (parameters.mix - current_mix_) /
                static_cast<float>(ramp_total_);
    rate_step_ = (parameters.rate_hz - current_rate_hz_) /
                 static_cast<float>(ramp_total_);
    depth_step_ = (new_depth_samples - current_depth_samples_) /
                  static_cast<float>(ramp_total_);
    spread_step_ = (parameters.stereo_spread_degrees - current_spread_degrees_) /
                   static_cast<float>(ramp_total_);
    const float new_input_gain = std::pow(10.0F, parameters.input_gain_db / 20.0F);
    input_gain_step_ = (new_input_gain - current_input_gain_) /
                       static_cast<float>(ramp_total_);
    const float new_grit_amplitude = parameters.grit_percent / 100.0F *
                                     kMaximumGritNoiseAmplitude;
    grit_amplitude_step_ = (new_grit_amplitude - current_grit_amplitude_) /
                           static_cast<float>(ramp_total_);
    const float new_vibrato_blend = parameters.mode == ModulatedDelayMode::vibrato
                                        ? 1.0F
                                        : 0.0F;
    vibrato_blend_step_ = (new_vibrato_blend - current_vibrato_blend_) /
                          static_cast<float>(ramp_total_);
    trigger_amplitude_ = std::pow(10.0F, parameters.trigger_threshold_db / 20.0F);
    parameters_ = parameters;
    ramp_remaining_ = ramp_total_;
}

void ModulatedDelay::process(std::span<float> interleaved,
                             std::uint16_t channels) noexcept {
    process_impl(interleaved, channels, true);
}

void ModulatedDelay::process_bypassed(std::span<float> interleaved,
                                      std::uint16_t channels) noexcept {
    if (!parameters_.spillover) return;
    process_impl(interleaved, channels, false);
}

void ModulatedDelay::process_impl(std::span<float> interleaved,
                                  std::uint16_t channels,
                                  bool capture_input) noexcept {
    if (channels != channels_ || channels == 0) return;
    const std::size_t frames = interleaved.size() / channels_;
    std::array<float, 2> original{};
    std::array<float, 2> delayed{};

    for (std::size_t frame = 0; frame < frames; ++frame) {
        float input_peak = 0.0F;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = frame * channels_ + channel;
            original[channel] = std::isfinite(interleaved[index])
                ? interleaved[index]
                : 0.0F;
            input_peak = std::max(input_peak, std::abs(original[channel]));
            delayed[channel] = read_fractional_delay(
                delay_samples_for_channel(channel), channel);
        }

        if (parameters_.waveform == ModulatedDelayWaveform::triggered_ramp) {
            const bool above_threshold = input_peak >= trigger_amplitude_;
            if (above_threshold && trigger_armed_) {
                phase_ = 0.0F;
                has_triggered_ = true;
            }
            trigger_armed_ = !above_threshold;
        }

        const std::size_t write_offset = write_frame_ * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            float wet = delayed[channel];
            if (current_grit_amplitude_ > 0.0F &&
                (capture_input || std::abs(delayed[channel]) > 1.0e-12F)) {
                wet += random_value(grit_random_state_) *
                       current_grit_amplitude_;
            }
            const double chorus_output =
                static_cast<double>(original[channel]) *
                    static_cast<double>(1.0F - current_mix_) +
                static_cast<double>(wet) * static_cast<double>(current_mix_);
            const double vibrato_output = static_cast<double>(wet) *
                                          static_cast<double>(current_mix_);
            const double output = capture_input
                ? chorus_output *
                      static_cast<double>(1.0F - current_vibrato_blend_) +
                      vibrato_output *
                      static_cast<double>(current_vibrato_blend_)
                : static_cast<double>(original[channel]) +
                      static_cast<double>(wet) * static_cast<double>(current_mix_);
            interleaved[frame * channels_ + channel] = bounded_sample(output);
            const double recorded = (capture_input
                ? static_cast<double>(original[channel]) *
                      static_cast<double>(current_input_gain_)
                : 0.0) +
                static_cast<double>(delayed[channel]) *
                    static_cast<double>(current_feedback_);
            storage_[write_offset + channel] = bounded_sample(recorded);
        }

        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
        phase_ += current_rate_hz_ / sample_rate_;
        if (phase_ >= 1.0F) {
            phase_ -= std::floor(phase_);
            if (parameters_.waveform == ModulatedDelayWaveform::random) {
                random_current_ = random_next_;
                random_next_ = random_value(random_state_);
            }
        }
        advance_ramps();
    }
}

void ModulatedDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
    phase_ = 0.0F;
    random_state_ = 0x6d2b79f5U;
    random_current_ = random_value(random_state_);
    random_next_ = random_value(random_state_);
    trigger_armed_ = true;
    has_triggered_ = false;
    current_time_samples_ = target_time_samples_;
    current_feedback_ = parameters_.feedback;
    current_mix_ = parameters_.mix;
    current_rate_hz_ = parameters_.rate_hz;
    current_depth_samples_ = static_cast<float>(
        static_cast<double>(sample_rate_) * parameters_.depth_ms / 1000.0);
    current_spread_degrees_ = parameters_.stereo_spread_degrees;
    current_input_gain_ = std::pow(10.0F, parameters_.input_gain_db / 20.0F);
    current_grit_amplitude_ = parameters_.grit_percent / 100.0F *
                              kMaximumGritNoiseAmplitude;
    current_vibrato_blend_ = parameters_.mode == ModulatedDelayMode::vibrato
                                 ? 1.0F
                                 : 0.0F;
    time_step_ = 0.0F;
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    rate_step_ = 0.0F;
    depth_step_ = 0.0F;
    spread_step_ = 0.0F;
    input_gain_step_ = 0.0F;
    grit_amplitude_step_ = 0.0F;
    vibrato_blend_step_ = 0.0F;
    ramp_remaining_ = 0;
}

float ModulatedDelay::bounded(float value, float fallback,
                              float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float ModulatedDelay::random_value(std::uint32_t& state) noexcept {
    state = state * 1664525U + 1013904223U;
    const float unit = static_cast<float>(state >> 8U) / 16777215.0F;
    return unit * 2.0F - 1.0F;
}

std::size_t ModulatedDelay::samples_for_ms(float time_ms) const noexcept {
    const double samples = std::round(
        static_cast<double>(sample_rate_) * static_cast<double>(time_ms) / 1000.0);
    const double maximum = static_cast<double>(capacity_frames_ - 3U);
    return static_cast<std::size_t>(std::clamp(samples, 1.0, maximum));
}

float ModulatedDelay::read_fractional_delay(float delay_samples,
                                            std::uint16_t channel) const noexcept {
    const float bounded_delay = std::clamp(
        delay_samples, 1.0F, static_cast<float>(capacity_frames_ - 2U));
    const auto whole = static_cast<std::size_t>(std::floor(bounded_delay));
    const float fraction = bounded_delay - static_cast<float>(whole);
    const float recent = read_age(whole, channel);
    const float previous = read_age(whole + 1U, channel);
    return recent + (previous - recent) * fraction;
}

float ModulatedDelay::read_age(std::size_t age,
                               std::uint16_t channel) const noexcept {
    if (age == 0U || age >= capacity_frames_ || channel >= channels_) return 0.0F;
    const std::size_t read_frame =
        (write_frame_ + capacity_frames_ - age) % capacity_frames_;
    return storage_[read_frame * channels_ + channel];
}

float ModulatedDelay::modulation_value(float phase) const noexcept {
    switch (parameters_.waveform) {
    case ModulatedDelayWaveform::sine:
        return std::sin(kTwoPi * phase);
    case ModulatedDelayWaveform::triangle:
        return 1.0F - 4.0F * std::abs(phase - 0.5F);
    case ModulatedDelayWaveform::random:
        return random_current_ + (random_next_ - random_current_) * phase;
    case ModulatedDelayWaveform::triggered_ramp:
        return has_triggered_ ? phase * 2.0F - 1.0F : 0.0F;
    }
    return 0.0F;
}

float ModulatedDelay::delay_samples_for_channel(std::uint16_t channel) const noexcept {
    float channel_phase = phase_;
    if (channels_ == 2U && channel == 1U) {
        channel_phase += current_spread_degrees_ / 360.0F;
        channel_phase -= std::floor(channel_phase);
    }
    const float modulation = modulation_value(channel_phase) * current_depth_samples_;
    const float maximum = static_cast<float>(capacity_frames_ - 2U);
    return std::clamp(current_time_samples_ + modulation, 1.0F, maximum);
}

void ModulatedDelay::advance_ramps() noexcept {
    if (ramp_remaining_ == 0U) return;
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) {
        current_time_samples_ = target_time_samples_;
        current_feedback_ = parameters_.feedback;
        current_mix_ = parameters_.mix;
        current_rate_hz_ = parameters_.rate_hz;
        current_depth_samples_ = static_cast<float>(
            static_cast<double>(sample_rate_) * parameters_.depth_ms / 1000.0);
        current_spread_degrees_ = parameters_.stereo_spread_degrees;
        current_input_gain_ = std::pow(10.0F, parameters_.input_gain_db / 20.0F);
        current_grit_amplitude_ = parameters_.grit_percent / 100.0F *
                                  kMaximumGritNoiseAmplitude;
        current_vibrato_blend_ = parameters_.mode == ModulatedDelayMode::vibrato
                                     ? 1.0F
                                     : 0.0F;
    } else {
        current_time_samples_ += time_step_;
        current_feedback_ += feedback_step_;
        current_mix_ += mix_step_;
        current_rate_hz_ += rate_step_;
        current_depth_samples_ += depth_step_;
        current_spread_degrees_ += spread_step_;
        current_input_gain_ += input_gain_step_;
        current_grit_amplitude_ += grit_amplitude_step_;
        current_vibrato_blend_ += vibrato_blend_step_;
    }
}

} // namespace tabletfx
