#include "tabletfx/effects/tape_echo.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kWowRateHz = 0.31F;
constexpr float kFlutterRateHz = 6.2F;
constexpr float kWowDepthMs = 8.0F;
constexpr float kFlutterDepthMs = 0.8F;
constexpr float kMaximumTapeNoise = 0.002F;
constexpr float kMaximumInputMagnitude = 1000000.0F;

float bounded_output(double value) noexcept {
    const double limit = static_cast<double>(std::numeric_limits<float>::max());
    return static_cast<float>(std::clamp(value, -limit, limit));
}

}  // namespace

TapeEcho::TapeEcho(float sample_rate, std::uint16_t channels,
                   float max_delay_ms, TapeEchoParameters parameters)
    : sample_rate_(sample_rate), channels_(channels), max_delay_ms_(max_delay_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0U || channels_ > 2U ||
        !std::isfinite(max_delay_ms_) || max_delay_ms_ < kMinimumTimeMs ||
        max_delay_ms_ > 6000.0F) {
        throw std::invalid_argument(
            "Tape Echo requires a supported sample rate, mono/stereo layout, and maximum time in [1, 6000] ms");
    }

    const double requested_frames = std::ceil(
        static_cast<double>(sample_rate_) * static_cast<double>(max_delay_ms_) /
        1000.0);
    const std::size_t max_frames = storage_.max_size() / channels_;
    if (!std::isfinite(requested_frames) || requested_frames < 1.0 ||
        requested_frames > static_cast<double>(max_frames - 3U)) {
        throw std::length_error("Tape Echo delay buffer is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(requested_frames) + 3U;
    storage_.assign(capacity_frames_ * channels_, 0.0F);
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::ceil(
                static_cast<double>(sample_rate_) * kControlRampMs / 1000.0)));

    configure(parameters);
    current_time_samples_ = target_time_samples_;
    current_feedback_ = parameters_.feedback;
    current_mix_ = parameters_.mix;
    current_record_level_ = parameters_.record_level;
    current_tape_bias_ = parameters_.tape_bias;
    current_tape_noise_ = parameters_.tape_noise;
    current_wow_flutter_ = parameters_.wow_flutter;
    time_step_.fill(0.0F);
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    record_level_step_ = 0.0F;
    tape_bias_step_ = 0.0F;
    tape_noise_step_ = 0.0F;
    wow_flutter_step_ = 0.0F;
    ramp_remaining_ = 0U;
}

float TapeEcho::bounded(float value, float fallback,
                        float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

float TapeEcho::finite_input(float sample) noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return std::clamp(sample, -kMaximumInputMagnitude,
                       kMaximumInputMagnitude);
}

bool TapeEcho::valid_division(DelayDivision division) noexcept {
    switch (division) {
    case DelayDivision::whole:
    case DelayDivision::dotted_half:
    case DelayDivision::half:
    case DelayDivision::dotted_quarter:
    case DelayDivision::quarter:
    case DelayDivision::quarter_triplet:
    case DelayDivision::dotted_eighth:
    case DelayDivision::eighth:
    case DelayDivision::eighth_triplet:
    case DelayDivision::dotted_sixteenth:
    case DelayDivision::sixteenth:
    case DelayDivision::sixteenth_triplet:
    case DelayDivision::thirty_second:
    case DelayDivision::sixty_fourth:
        return true;
    default:
        return false;
    }
}

float TapeEcho::beats_for_division(DelayDivision division) noexcept {
    switch (division) {
    case DelayDivision::whole: return 4.0F;
    case DelayDivision::dotted_half: return 3.0F;
    case DelayDivision::half: return 2.0F;
    case DelayDivision::dotted_quarter: return 1.5F;
    case DelayDivision::quarter: return 1.0F;
    case DelayDivision::quarter_triplet: return 2.0F / 3.0F;
    case DelayDivision::dotted_eighth: return 0.75F;
    case DelayDivision::eighth: return 0.5F;
    case DelayDivision::eighth_triplet: return 1.0F / 3.0F;
    case DelayDivision::dotted_sixteenth: return 0.375F;
    case DelayDivision::sixteenth: return 0.25F;
    case DelayDivision::sixteenth_triplet: return 1.0F / 6.0F;
    case DelayDivision::thirty_second: return 0.125F;
    case DelayDivision::sixty_fourth: return 0.0625F;
    default: return 1.0F;
    }
}

float TapeEcho::selected_time_ms() const noexcept {
    if (parameters_.time_mode == DelayTimeMode::tempo_sync) {
        const float beat_ms = 60000.0F / parameters_.tempo_bpm;
        return bounded(beat_ms * beats_for_division(parameters_.tap_division),
                       parameters_.time_ms, kMinimumTimeMs,
                       std::min(kMaximumTimeMs, max_delay_ms_));
    }
    return parameters_.time_ms;
}

float TapeEcho::selected_time_ms(std::uint16_t channel) const noexcept {
    if (!parameters_.independent_stereo_times || channel > 1U) {
        return selected_time_ms();
    }

    const float time_ms = channel == 0U ? parameters_.left_time_ms
                                        : parameters_.right_time_ms;
    if (parameters_.time_mode == DelayTimeMode::tempo_sync) {
        const DelayDivision division = channel == 0U
            ? parameters_.left_tap_division
            : parameters_.right_tap_division;
        const float beat_ms = 60000.0F / parameters_.tempo_bpm;
        return bounded(beat_ms * beats_for_division(division), time_ms,
                       kMinimumTimeMs, std::min(kMaximumTimeMs, max_delay_ms_));
    }
    return bounded(time_ms, 350.0F, kMinimumTimeMs,
                   std::min(kMaximumTimeMs, max_delay_ms_));
}

void TapeEcho::configure(TapeEchoParameters parameters) noexcept {
    const float maximum_time = std::min(kMaximumTimeMs, max_delay_ms_);
    parameters.time_ms = bounded(parameters.time_ms, 350.0F,
                                 kMinimumTimeMs, maximum_time);
    if (parameters.time_mode != DelayTimeMode::milliseconds &&
        parameters.time_mode != DelayTimeMode::tempo_sync) {
        parameters.time_mode = DelayTimeMode::milliseconds;
    }
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, 120.0F,
                                   kMinimumTempoBpm, kMaximumTempoBpm);
    if (!valid_division(parameters.tap_division)) {
        parameters.tap_division = DelayDivision::quarter;
    }
    parameters.left_time_ms = bounded(parameters.left_time_ms, 350.0F,
                                      kMinimumTimeMs, maximum_time);
    parameters.right_time_ms = bounded(parameters.right_time_ms, 350.0F,
                                       kMinimumTimeMs, maximum_time);
    if (!valid_division(parameters.left_tap_division)) {
        parameters.left_tap_division = DelayDivision::quarter;
    }
    if (!valid_division(parameters.right_tap_division)) {
        parameters.right_tap_division = DelayDivision::quarter;
    }
    parameters.feedback = bounded(parameters.feedback, 0.35F,
                                  0.0F, kMaximumFeedback);
    parameters.mix = bounded(parameters.mix, 0.25F, 0.0F, 1.0F);
    parameters.record_level = bounded(parameters.record_level, 0.25F,
                                      0.0F, 1.0F);
    parameters.tape_bias = bounded(parameters.tape_bias, 0.35F,
                                   0.0F, 1.0F);
    parameters.tape_noise = bounded(parameters.tape_noise, 0.0F,
                                    0.0F, 1.0F);
    parameters.wow_flutter = bounded(parameters.wow_flutter, 0.15F,
                                     0.0F, 1.0F);

    parameters_ = parameters;
    for (std::uint16_t channel = 0U; channel < 2U; ++channel) {
        const float requested_time = selected_time_ms(channel);
        const float target_time = static_cast<float>(
            static_cast<double>(sample_rate_) * requested_time / 1000.0);
        target_time_samples_[channel] = std::clamp(
            target_time, 1.0F, static_cast<float>(capacity_frames_ - 2U));
        time_step_[channel] =
            (target_time_samples_[channel] - current_time_samples_[channel]) /
            static_cast<float>(ramp_total_);
    }
    feedback_step_ = (parameters_.feedback - current_feedback_) /
                     static_cast<float>(ramp_total_);
    mix_step_ = (parameters_.mix - current_mix_) /
                static_cast<float>(ramp_total_);
    record_level_step_ = (parameters_.record_level - current_record_level_) /
                         static_cast<float>(ramp_total_);
    tape_bias_step_ = (parameters_.tape_bias - current_tape_bias_) /
                      static_cast<float>(ramp_total_);
    tape_noise_step_ = (parameters_.tape_noise - current_tape_noise_) /
                       static_cast<float>(ramp_total_);
    wow_flutter_step_ = (parameters_.wow_flutter - current_wow_flutter_) /
                        static_cast<float>(ramp_total_);
    ramp_remaining_ = ramp_total_;
}

float TapeEcho::read_delay(float delay_samples,
                           std::uint16_t channel) const noexcept {
    const float maximum_age = static_cast<float>(capacity_frames_ - 2U);
    const double age = static_cast<double>(
        std::clamp(delay_samples, 1.0F, maximum_age));
    double position = static_cast<double>(write_frame_) - age;
    const double capacity = static_cast<double>(capacity_frames_);
    if (position < 0.0) {
        position += capacity * std::ceil(-position / capacity);
    }
    if (position >= capacity) {
        position = std::fmod(position, capacity);
    }

    const std::size_t first_frame = static_cast<std::size_t>(position);
    const std::size_t second_frame = (first_frame + 1U) % capacity_frames_;
    const float fraction = static_cast<float>(
        position - static_cast<double>(first_frame));
    const float first = storage_[first_frame * channels_ + channel];
    const float second = storage_[second_frame * channels_ + channel];
    return first + (second - first) * fraction;
}

float TapeEcho::record_to_tape(float sample) const noexcept {
    const float drive = 1.0F + 4.0F * current_record_level_;
    const float headroom = 1.5F - current_tape_bias_;
    return headroom * std::tanh(finite_input(sample) * drive / headroom);
}

float TapeEcho::modulated_delay_samples(std::uint16_t channel) const noexcept {
    const float max_age = static_cast<float>(capacity_frames_ - 2U);
    const float wanted_depth = sample_rate_ *
        (kWowDepthMs + kFlutterDepthMs) / 1000.0F * current_wow_flutter_;
    const float depth = std::min(wanted_depth,
        std::max(0.0F, std::min(current_time_samples_[channel] - 1.0F,
                                max_age - current_time_samples_[channel])));
    const float wow = std::sin(kTau * wow_phase_);
    const float flutter = std::sin(kTau * flutter_phase_);
    const float weighting = kWowDepthMs / (kWowDepthMs + kFlutterDepthMs);
    const float modulation = depth *
        (weighting * wow + (1.0F - weighting) * flutter);
    return std::clamp(current_time_samples_[channel] + modulation, 1.0F, max_age);
}

float TapeEcho::signed_random(std::uint32_t& state) noexcept {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    const float normalized = static_cast<float>(state) /
                             static_cast<float>(
                                 std::numeric_limits<std::uint32_t>::max());
    return normalized * 2.0F - 1.0F;
}

void TapeEcho::advance_parameters() noexcept {
    if (ramp_remaining_ != 0U) {
        for (std::size_t channel = 0U; channel < current_time_samples_.size(); ++channel) {
            current_time_samples_[channel] += time_step_[channel];
        }
        current_feedback_ += feedback_step_;
        current_mix_ += mix_step_;
        current_record_level_ += record_level_step_;
        current_tape_bias_ += tape_bias_step_;
        current_tape_noise_ += tape_noise_step_;
        current_wow_flutter_ += wow_flutter_step_;
        --ramp_remaining_;
        if (ramp_remaining_ == 0U) {
            current_time_samples_ = target_time_samples_;
            current_feedback_ = parameters_.feedback;
            current_mix_ = parameters_.mix;
            current_record_level_ = parameters_.record_level;
            current_tape_bias_ = parameters_.tape_bias;
            current_tape_noise_ = parameters_.tape_noise;
            current_wow_flutter_ = parameters_.wow_flutter;
        }
    }

    wow_phase_ += kWowRateHz / sample_rate_;
    flutter_phase_ += kFlutterRateHz / sample_rate_;
    if (wow_phase_ >= 1.0F) {
        wow_phase_ -= 1.0F;
    }
    if (flutter_phase_ >= 1.0F) {
        flutter_phase_ -= 1.0F;
    }
}

void TapeEcho::process_audio(std::span<float> interleaved,
                             bool bypassed) noexcept {
    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        advance_parameters();
        const std::size_t write_offset = write_frame_ * channels_;
        for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
            const std::size_t index = frame * channels_ + channel;
            const float input = finite_input(interleaved[index]);
            const float delay_samples = modulated_delay_samples(channel);
            const float delayed = read_delay(delay_samples, channel);
            const float noise = signed_random(random_state_) *
                                current_tape_noise_ * kMaximumTapeNoise;
            const float wet = delayed + noise;
            const double output = bypassed
                ? static_cast<double>(input) +
                      static_cast<double>(wet) * current_mix_
                : static_cast<double>(input) * (1.0F - current_mix_) +
                      static_cast<double>(wet) * current_mix_;
            interleaved[index] = bounded_output(output);

            const float tape_input = bypassed
                ? delayed * current_feedback_
                : input + delayed * current_feedback_;
            storage_[write_offset + channel] = record_to_tape(tape_input);
        }
        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
    }
}

void TapeEcho::process(std::span<float> interleaved,
                       std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U || channels > 2U) {
        return;
    }
    storage_cleared_ = false;
    process_audio(interleaved, false);
}

void TapeEcho::process_bypassed(std::span<float> interleaved,
                                std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U || channels > 2U) {
        return;
    }
    if (!parameters_.spillover) {
        if (!storage_cleared_) {
            reset();
        }
        return;
    }
    storage_cleared_ = false;
    process_audio(interleaved, true);
}

void TapeEcho::reset() noexcept {
    if (!storage_cleared_) {
        std::fill(storage_.begin(), storage_.end(), 0.0F);
    }
    write_frame_ = 0U;
    wow_phase_ = 0.0F;
    flutter_phase_ = 0.0F;
    random_state_ = 0x9e3779b9U;
    storage_cleared_ = true;
}

}  // namespace tabletfx
