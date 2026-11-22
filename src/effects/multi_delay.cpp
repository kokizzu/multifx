#include "tabletfx/effects/multi_delay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr double kPiOverTwo = 1.57079632679489661923;
constexpr float kMaximumSeriesFeedback = 0.9F;

} // namespace

MultiDelay::MultiDelay(float sample_rate, float max_delay_ms,
                       MultiDelayParameters parameters)
    : sample_rate_(sample_rate), requested_max_delay_ms_(max_delay_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate ||
        !std::isfinite(requested_max_delay_ms_) ||
        requested_max_delay_ms_ < kMinimumDelayMs ||
        requested_max_delay_ms_ > kMaximumDelayMs) {
        throw std::invalid_argument(
            "Multi-Delay requires 8–384 kHz and a 1–2000 ms storage request");
    }

    constexpr std::size_t maximum_storage_samples =
        multi_delay_storage_limit_bytes / sizeof(float);
    constexpr std::size_t maximum_frames_by_memory =
        maximum_storage_samples / multi_delay_line_count;
    const double requested_frames = std::ceil(
        static_cast<double>(sample_rate_) *
        static_cast<double>(requested_max_delay_ms_) / 1000.0);
    if (!std::isfinite(requested_frames) || requested_frames < 1.0 ||
        requested_frames >=
            static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw std::length_error("Multi-Delay storage size is invalid");
    }

    capacity_frames_ = std::min(
        static_cast<std::size_t>(requested_frames) + 1U,
        maximum_frames_by_memory);
    if (capacity_frames_ < 2U ||
        capacity_frames_ > storage_.max_size() / multi_delay_line_count) {
        throw std::length_error("Multi-Delay exceeds its fixed memory cap");
    }
    storage_.assign(capacity_frames_ * multi_delay_line_count, 0.0F);
    if (storage_.capacity() > maximum_storage_samples) {
        throw std::length_error(
            "Multi-Delay allocator exceeded its 512-KiB storage cap");
    }

    configure(parameters);
}

float MultiDelay::bounded(float value, float fallback,
                          float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

bool MultiDelay::valid_time_mode(DelayTimeMode mode) noexcept {
    switch (mode) {
    case DelayTimeMode::milliseconds:
    case DelayTimeMode::tempo_sync:
        return true;
    }
    return false;
}

bool MultiDelay::valid_division(DelayDivision division) noexcept {
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
        return true;
    }
    return false;
}

float MultiDelay::beats_for_division(DelayDivision division) noexcept {
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
    }
    return 1.0F;
}

float MultiDelay::maximum_delay_ms() const noexcept {
    if (capacity_frames_ < 2U) return 0.0F;
    const double available = static_cast<double>(capacity_frames_ - 1U) *
                             1000.0 / static_cast<double>(sample_rate_);
    return static_cast<float>(std::min(
        static_cast<double>(requested_max_delay_ms_), available));
}

std::size_t MultiDelay::delay_samples(std::size_t line) const noexcept {
    return line < multi_delay_line_count ? delay_frames_[line] : 0U;
}

void MultiDelay::configure(MultiDelayParameters parameters) noexcept {
    const float maximum_time = std::max(
        kMinimumDelayMs, std::min(kMaximumDelayMs, maximum_delay_ms()));
    parameters.tempo_bpm = bounded(parameters.tempo_bpm,
        parameters_.tempo_bpm, kMinimumTempoBpm, kMaximumTempoBpm);
    switch (parameters.topology) {
    case MultiDelayTopology::quad_parallel:
    case MultiDelayTopology::quad_series:
    case MultiDelayTopology::quad_diffusor:
    case MultiDelayTopology::quad_tap:
    case MultiDelayTopology::quad_tap_band:
        break;
    default:
        parameters.topology = parameters_.topology;
        break;
    }
    parameters.series_feedback = bounded(parameters.series_feedback,
        parameters_.series_feedback, -kMaximumSeriesFeedback,
        kMaximumSeriesFeedback);
    parameters.series_feedback_send = std::min(
        parameters.series_feedback_send,
        static_cast<std::uint32_t>(multi_delay_line_count - 1U));
    parameters.series_feedback_return = std::min(
        parameters.series_feedback_return,
        static_cast<std::uint32_t>(multi_delay_line_count - 1U));
    parameters.master_time = bounded(parameters.master_time,
        parameters_.master_time, 0.1F, 2.0F);
    const float maximum_master_feedback =
        parameters.topology == MultiDelayTopology::quad_tap ||
                parameters.topology == MultiDelayTopology::quad_tap_band
            ? 2.0F
            : 0.9F;
    parameters.master_feedback = bounded(parameters.master_feedback,
        parameters_.master_feedback, 0.0F, maximum_master_feedback);

    for (std::size_t line = 0; line < multi_delay_line_count; ++line) {
        parameters.time_ms[line] = bounded(parameters.time_ms[line],
            parameters_.time_ms[line], kMinimumDelayMs, maximum_time);
        if (!valid_time_mode(parameters.time_mode[line])) {
            parameters.time_mode[line] = parameters_.time_mode[line];
        }
        if (!valid_division(parameters.division[line])) {
            parameters.division[line] = parameters_.division[line];
        }
        const float maximum_feedback =
            parameters.topology == MultiDelayTopology::quad_diffusor
                ? 0.9F
                : kMaximumLinkFeedback;
        parameters.feedback[line] = bounded(parameters.feedback[line],
            parameters_.feedback[line], -maximum_feedback, maximum_feedback);
        parameters.cross_feedback[line] = bounded(
            parameters.cross_feedback[line],
            parameters_.cross_feedback[line], -kMaximumLinkFeedback,
            kMaximumLinkFeedback);
        parameters.level[line] = bounded(parameters.level[line],
            parameters_.level[line], -kMaximumLevel, kMaximumLevel);
        parameters.pan[line] = bounded(parameters.pan[line],
            parameters_.pan[line], 0.0F, 1.0F);
    }

    const float maximum_filter_hz = std::max(
        20.0F, std::min(20000.0F, sample_rate_ * 0.45F));
    parameters.low_cut_hz = bounded(parameters.low_cut_hz,
        parameters_.low_cut_hz, 0.0F, maximum_filter_hz);
    parameters.high_cut_hz = bounded(parameters.high_cut_hz,
        parameters_.high_cut_hz, 20.0F, maximum_filter_hz);
    if (parameters.low_cut_hz > 0.0F &&
        parameters.high_cut_hz < parameters.low_cut_hz) {
        parameters.high_cut_hz = parameters.low_cut_hz;
    }
    parameters.input_gain_db = bounded(parameters.input_gain_db,
        parameters_.input_gain_db, -24.0F, 24.0F);
    parameters.mix = bounded(parameters.mix, parameters_.mix, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db,
        parameters_.level_db, -24.0F, 12.0F);
    parameters_ = parameters;

    input_gain_linear_ = static_cast<float>(std::pow(
        10.0, static_cast<double>(parameters_.input_gain_db) / 20.0));
    output_level_linear_ = static_cast<float>(std::pow(
        10.0, static_cast<double>(parameters_.level_db) / 20.0));
    low_cut_enabled_ = parameters_.low_cut_hz > 0.0F;
    low_cut_alpha_ = low_cut_enabled_
        ? std::exp(-2.0F * static_cast<float>(kPiOverTwo) * 2.0F *
                   parameters_.low_cut_hz / sample_rate_)
        : 0.0F;
    high_cut_coefficient_ = 1.0F - std::exp(
        -2.0F * static_cast<float>(kPiOverTwo) * 2.0F *
        parameters_.high_cut_hz / sample_rate_);

    for (std::size_t line = 0; line < multi_delay_line_count; ++line) {
        line_gains_[line] = parameters_.level[line];
        const float pan = parameters_.pan[line];
        const double angle = static_cast<double>(pan) * kPiOverTwo;
        left_gains_[line] = static_cast<float>(std::cos(angle));
        right_gains_[line] = static_cast<float>(std::sin(angle));
    }
    update_line_times();
}

void MultiDelay::update_line_times() noexcept {
    const float maximum_time = std::max(
        kMinimumDelayMs, std::min(kMaximumDelayMs, maximum_delay_ms()));
    for (std::size_t line = 0; line < multi_delay_line_count; ++line) {
        float time_ms = parameters_.time_ms[line];
        if (parameters_.time_mode[line] == DelayTimeMode::tempo_sync) {
            time_ms = 60000.0F / parameters_.tempo_bpm *
                      beats_for_division(parameters_.division[line]);
        }
        if (parameters_.topology == MultiDelayTopology::quad_diffusor ||
            parameters_.topology == MultiDelayTopology::quad_tap ||
            parameters_.topology == MultiDelayTopology::quad_tap_band) {
            time_ms *= parameters_.master_time;
        }
        time_ms = std::clamp(time_ms, kMinimumDelayMs, maximum_time);
        const double requested_frames = std::round(
            static_cast<double>(sample_rate_) *
            static_cast<double>(time_ms) / 1000.0);
        delay_frames_[line] = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::max(1.0, requested_frames)),
            1U, capacity_frames_ - 1U);
    }
}

std::size_t MultiDelay::read_frame(std::size_t delay,
                                   std::size_t line) const noexcept {
    const std::size_t offset = std::min(delay, capacity_frames_ - 1U);
    const std::size_t frame = write_frame_ >= offset
        ? write_frame_ - offset
        : write_frame_ + capacity_frames_ - offset;
    return line * capacity_frames_ + frame;
}

float MultiDelay::filter_line(float input, std::size_t line) noexcept {
    float value = input;
    if (low_cut_enabled_) {
        const float high_pass = low_cut_alpha_ *
            (low_cut_output_[line] + value - low_cut_input_[line]);
        low_cut_input_[line] = value;
        low_cut_output_[line] = high_pass;
        value = high_pass;
    }
    high_cut_output_[line] += high_cut_coefficient_ *
        (value - high_cut_output_[line]);
    return std::isfinite(high_cut_output_[line])
        ? high_cut_output_[line]
        : 0.0F;
}

float MultiDelay::safe_signal(float value) noexcept {
    if (!std::isfinite(value)) return 0.0F;
    return std::clamp(value, -kMaximumSignal, kMaximumSignal);
}

void MultiDelay::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    if (channels == 0U || channels > 2U) return;
    const std::size_t frames = interleaved.size() /
                               static_cast<std::size_t>(channels);
    const float mix = parameters_.mix;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * static_cast<std::size_t>(channels);
        const float dry_left = safe_signal(interleaved[offset]);
        const float dry_right = channels == 2U
            ? safe_signal(interleaved[offset + 1U]) : dry_left;
        const float summed_input = channels == 2U
            ? 0.5F * (dry_left + dry_right)
            : dry_left;
        const float effect_input = safe_signal(
            summed_input * input_gain_linear_);

        float wet_left = 0.0F;
        float wet_right = 0.0F;
        float wet_mono = 0.0F;
        if (parameters_.topology == MultiDelayTopology::quad_diffusor) {
            float stage_input = effect_input;
            for (std::size_t line = 0; line < multi_delay_line_count; ++line) {
                const float delayed_state =
                    storage_[read_frame(delay_frames_[line], line)];
                const float feedback = std::clamp(
                    parameters_.master_feedback,
                    -0.9F, 0.9F);
                const float stage_output = safe_signal(
                    delayed_state - feedback * stage_input);
                const std::size_t write_index =
                    line * capacity_frames_ + write_frame_;
                storage_[write_index] = safe_signal(
                    stage_input + feedback * stage_output);
                stage_input = stage_output;
            }
            const float wet = filter_line(stage_input, 0U);
            if (channels == 1U) {
                wet_mono = wet;
            } else {
                wet_left = wet;
                wet_right = wet;
            }
        } else {
            std::array<float, multi_delay_line_count> tap_outputs{};
            std::array<float, multi_delay_line_count> feedback_taps{};
            for (std::size_t line = 0; line < multi_delay_line_count; ++line) {
                const float delayed =
                    storage_[read_frame(delay_frames_[line], line)];
                tap_outputs[line] = filter_line(delayed, line);
                feedback_taps[line] =
                    parameters_.topology == MultiDelayTopology::quad_tap_band
                        ? delayed
                        : tap_outputs[line];
                const float levelled = tap_outputs[line] * line_gains_[line];
                if (channels == 1U) {
                    wet_mono += levelled;
                } else {
                    wet_left += levelled * left_gains_[line];
                    wet_right += levelled * right_gains_[line];
                }
            }

            if (parameters_.topology == MultiDelayTopology::quad_series) {
                std::array<float, multi_delay_line_count> line_inputs{};
                line_inputs[0] = effect_input;
                for (std::size_t line = 1;
                     line < multi_delay_line_count; ++line) {
                    line_inputs[line] = tap_outputs[line - 1U];
                }
                line_inputs[parameters_.series_feedback_return] = safe_signal(
                    line_inputs[parameters_.series_feedback_return] +
                    parameters_.series_feedback *
                        tap_outputs[parameters_.series_feedback_send]);
                for (std::size_t line = 0;
                     line < multi_delay_line_count; ++line) {
                    const std::size_t write_index =
                        line * capacity_frames_ + write_frame_;
                    storage_[write_index] = safe_signal(line_inputs[line]);
                }
            } else {
                for (std::size_t line = 0; line < multi_delay_line_count; ++line) {
                    const std::size_t previous =
                        (line + multi_delay_line_count - 1U) % multi_delay_line_count;
                    const float cross_feedback =
                        parameters_.topology == MultiDelayTopology::quad_parallel
                            ? parameters_.cross_feedback[previous] *
                                  feedback_taps[previous]
                            : 0.0F;
                    const float self_feedback =
                        parameters_.feedback[line] * feedback_taps[line] *
                        (parameters_.topology == MultiDelayTopology::quad_tap ||
                         parameters_.topology == MultiDelayTopology::quad_tap_band
                            ? parameters_.master_feedback
                            : 1.0F);
                    const float feedback_input =
                        self_feedback + cross_feedback;
                    const std::size_t write_index =
                        line * capacity_frames_ + write_frame_;
                    storage_[write_index] = safe_signal(effect_input + feedback_input);
                }
            }
        }
        write_frame_ = (write_frame_ + 1U) % capacity_frames_;

        if (channels == 1U) {
            interleaved[offset] = safe_signal(
                dry_left * (1.0F - mix) +
                wet_mono * mix * output_level_linear_);
        } else {
            interleaved[offset] = safe_signal(
                dry_left * (1.0F - mix) +
                wet_left * mix * output_level_linear_);
            interleaved[offset + 1U] = safe_signal(
                dry_right * (1.0F - mix) +
                wet_right * mix * output_level_linear_);
        }
    }
}

void MultiDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    low_cut_input_.fill(0.0F);
    low_cut_output_.fill(0.0F);
    high_cut_output_.fill(0.0F);
    write_frame_ = 0U;
}

} // namespace tabletfx
