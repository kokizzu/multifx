#include "tabletfx/effects/plex_delay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float kInvSqrtTwo = 0.7071067811865475244F;

} // namespace

PlexDelay::PlexDelay(float sample_rate, float max_delay_ms,
                     PlexDelayParameters parameters)
    : sample_rate_(sample_rate), requested_max_delay_ms_(max_delay_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate ||
        !std::isfinite(requested_max_delay_ms_) ||
        requested_max_delay_ms_ < kMinimumDelayMs ||
        requested_max_delay_ms_ > kMaximumDelayMs) {
        throw std::invalid_argument(
            "Plex Delay requires 8–384 kHz and a 1–2000 ms ring request");
    }

    constexpr std::size_t maximum_storage_samples =
        plex_delay_storage_limit_bytes / sizeof(float);
    constexpr std::size_t maximum_frames_by_memory =
        maximum_storage_samples / kRingCount;
    const double requested_frames = std::ceil(
        static_cast<double>(sample_rate_) *
        static_cast<double>(requested_max_delay_ms_) / 1000.0);
    if (!std::isfinite(requested_frames) || requested_frames < 1.0 ||
        requested_frames >=
            static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw std::length_error("Plex Delay storage size is invalid");
    }

    capacity_frames_ = std::min(
        static_cast<std::size_t>(requested_frames) + 1U,
        maximum_frames_by_memory);
    if (capacity_frames_ < 2U ||
        capacity_frames_ > storage_.max_size() / kRingCount) {
        throw std::length_error("Plex Delay exceeds its fixed memory cap");
    }
    storage_.assign(capacity_frames_ * kRingCount, 0.0F);
    if (storage_.capacity() > maximum_storage_samples) {
        throw std::length_error(
            "Plex Delay allocator exceeded its 1 MiB storage cap");
    }
    configure(parameters);
}

float PlexDelay::bounded(float value, float fallback,
                         float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

bool PlexDelay::valid_time_mode(DelayTimeMode mode) noexcept {
    switch (mode) {
    case DelayTimeMode::milliseconds:
    case DelayTimeMode::tempo_sync:
        return true;
    }
    return false;
}

bool PlexDelay::valid_division(DelayDivision division) noexcept {
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

bool PlexDelay::valid_stack_mode(PlexDelayStackMode mode) noexcept {
    switch (mode) {
    case PlexDelayStackMode::off:
    case PlexDelayStackMode::stack:
    case PlexDelayStackMode::hold:
        return true;
    }
    return false;
}

float PlexDelay::beats_for_division(DelayDivision division) noexcept {
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

float PlexDelay::maximum_delay_ms() const noexcept {
    if (capacity_frames_ < 2U) return 0.0F;
    return static_cast<float>(capacity_frames_ - 1U) * 1000.0F /
           sample_rate_;
}

std::size_t PlexDelay::delay_samples(std::size_t line) const noexcept {
    return line < plex_delay_line_count ? delay_frames_[line] : 0U;
}

void PlexDelay::configure(PlexDelayParameters parameters) noexcept {
    const float maximum_time = std::max(
        kMinimumDelayMs, std::min(requested_max_delay_ms_, maximum_delay_ms()));
    parameters.active_lines = std::clamp<std::uint8_t>(
        parameters.active_lines, 1U,
        static_cast<std::uint8_t>(plex_delay_line_count));
    parameters.tempo_bpm = bounded(parameters.tempo_bpm,
        parameters_.tempo_bpm, 20.0F, 300.0F);
    parameters.master_time = bounded(parameters.master_time,
        parameters_.master_time, 0.25F, 4.0F);
    parameters.master_level = bounded(parameters.master_level,
        parameters_.master_level, 0.0F, 2.0F);
    parameters.master_pan = bounded(parameters.master_pan,
        parameters_.master_pan, 0.0F, 2.0F);
    parameters.decay_seconds = bounded(parameters.decay_seconds,
        parameters_.decay_seconds, 0.1F, 30.0F);
    parameters.diffusion = bounded(parameters.diffusion,
        parameters_.diffusion, 0.0F, 1.0F);
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
    parameters.pre_delay_ms = bounded(parameters.pre_delay_ms,
        parameters_.pre_delay_ms, 0.0F, maximum_time);
    if (!valid_stack_mode(parameters.stack_mode)) {
        parameters.stack_mode = parameters_.stack_mode;
    }
    for (std::size_t line = 0; line < plex_delay_line_count; ++line) {
        parameters.time_ms[line] = bounded(parameters.time_ms[line],
            parameters_.time_ms[line], kMinimumDelayMs, maximum_time);
        if (!valid_time_mode(parameters.time_mode[line])) {
            parameters.time_mode[line] = parameters_.time_mode[line];
        }
        if (!valid_division(parameters.division[line])) {
            parameters.division[line] = parameters_.division[line];
        }
        parameters.line_level[line] = bounded(parameters.line_level[line],
            parameters_.line_level[line], 0.0F, 2.0F);
        parameters.line_pan[line] = bounded(parameters.line_pan[line],
            parameters_.line_pan[line], -1.0F, 1.0F);
    }
    parameters.input_gain_db = bounded(parameters.input_gain_db,
        parameters_.input_gain_db, -24.0F, 24.0F);
    parameters.mix = bounded(parameters.mix, parameters_.mix, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db,
        parameters_.level_db, -24.0F, 12.0F);
    parameters.balance = bounded(parameters.balance,
        parameters_.balance, -1.0F, 1.0F);
    parameters_ = parameters;

    input_gain_linear_ = static_cast<float>(std::pow(
        10.0, static_cast<double>(parameters_.input_gain_db) / 20.0));
    output_level_linear_ = static_cast<float>(std::pow(
        10.0, static_cast<double>(parameters_.level_db) / 20.0));
    low_cut_enabled_ = parameters_.low_cut_hz > 0.0F;
    low_cut_alpha_ = low_cut_enabled_
        ? std::exp(-2.0F * static_cast<float>(kPi) *
                   parameters_.low_cut_hz / sample_rate_)
        : 0.0F;
    high_cut_coefficient_ = 1.0F - std::exp(
        -2.0F * static_cast<float>(kPi) *
        parameters_.high_cut_hz / sample_rate_);

    update_times();
    for (std::size_t line = 0; line < plex_delay_line_count; ++line) {
        const float pan = std::clamp(
            parameters_.line_pan[line] * parameters_.master_pan, -1.0F, 1.0F);
        const double angle = (static_cast<double>(pan) + 1.0) * kPi / 4.0;
        output_left_gains_[line] = static_cast<float>(std::cos(angle));
        output_right_gains_[line] = static_cast<float>(std::sin(angle));
        const float delay_seconds = static_cast<float>(delay_frames_[line]) /
                                    sample_rate_;
        feedback_gains_[line] = std::exp(
            -3.0F * delay_seconds / parameters_.decay_seconds);
    }
}

void PlexDelay::update_times() noexcept {
    const float maximum_time = std::max(
        kMinimumDelayMs, std::min(requested_max_delay_ms_, maximum_delay_ms()));
    const std::size_t maximum_frames = capacity_frames_ - 1U;
    const double sample_rate = static_cast<double>(sample_rate_);
    for (std::size_t line = 0; line < plex_delay_line_count; ++line) {
        float time_ms = parameters_.time_ms[line];
        if (parameters_.time_mode[line] == DelayTimeMode::tempo_sync) {
            time_ms = 60000.0F / parameters_.tempo_bpm *
                      beats_for_division(parameters_.division[line]);
        }
        time_ms = bounded(time_ms * parameters_.master_time,
                          parameters_.time_ms[line], kMinimumDelayMs,
                          maximum_time);
        const double frames = std::round(
            static_cast<double>(time_ms) * sample_rate / 1000.0);
        delay_frames_[line] = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::max(1.0, frames)),
            1U, maximum_frames);
    }
    const double pre_delay_frames = std::round(
        static_cast<double>(parameters_.pre_delay_ms) * sample_rate / 1000.0);
    pre_delay_frames_ = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::max(0.0, pre_delay_frames)),
        0U, maximum_frames);
}

std::size_t PlexDelay::read_index(std::size_t delay,
                                  std::size_t ring) const noexcept {
    const std::size_t offset = std::min(delay, capacity_frames_ - 1U);
    const std::size_t frame = write_frame_ >= offset
        ? write_frame_ - offset
        : write_frame_ + capacity_frames_ - offset;
    return ring * capacity_frames_ + frame;
}

float PlexDelay::filter_feedback(float input, std::size_t line) noexcept {
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
    return safe_signal(high_cut_output_[line]);
}

float PlexDelay::safe_signal(float value) noexcept {
    if (!std::isfinite(value)) return 0.0F;
    return std::clamp(value, -kMaximumSignal, kMaximumSignal);
}

void PlexDelay::process(std::span<float> interleaved,
                        std::uint16_t channels) noexcept {
    if (channels == 0U || channels > 2U || capacity_frames_ < 2U) return;
    const std::size_t frames = interleaved.size() /
                               static_cast<std::size_t>(channels);
    const std::size_t active_lines = parameters_.active_lines;
    const float mix = parameters_.mix;
    const float balance_left = parameters_.balance > 0.0F
        ? 1.0F - parameters_.balance : 1.0F;
    const float balance_right = parameters_.balance < 0.0F
        ? 1.0F + parameters_.balance : 1.0F;

    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * static_cast<std::size_t>(channels);
        const float dry_left = safe_signal(interleaved[offset]);
        const float dry_right = channels == 2U
            ? safe_signal(interleaved[offset + 1U]) : dry_left;
        const float summed_input = channels == 2U
            ? 0.5F * (dry_left + dry_right) : dry_left;
        const float input = parameters_.stack_mode == PlexDelayStackMode::hold
            ? 0.0F : safe_signal(summed_input * input_gain_linear_);
        const float pre_delayed = pre_delay_frames_ == 0U
            ? input : storage_[read_index(pre_delay_frames_, plex_delay_line_count)];

        std::array<float, plex_delay_line_count> taps{};
        std::array<float, plex_delay_line_count> feedback_taps{};
        float tap_sum = 0.0F;
        for (std::size_t line = 0; line < active_lines; ++line) {
            const float delayed = storage_[read_index(delay_frames_[line], line)];
            taps[line] = filter_feedback(delayed, line);
            feedback_taps[line] = parameters_.stack_mode ==
                    PlexDelayStackMode::off
                ? taps[line] : delayed;
            tap_sum += feedback_taps[line];
        }
        const float tap_mean = tap_sum / static_cast<float>(active_lines);

        float wet_left = 0.0F;
        float wet_right = 0.0F;
        float wet_mono = 0.0F;
        for (std::size_t line = 0; line < active_lines; ++line) {
            const float levelled = taps[line] * parameters_.line_level[line] *
                                   parameters_.master_level;
            if (channels == 1U) {
                wet_mono += levelled *
                    (output_left_gains_[line] + output_right_gains_[line]) *
                    kInvSqrtTwo;
            } else {
                wet_left += levelled * output_left_gains_[line];
                wet_right += levelled * output_right_gains_[line];
            }
        }

        for (std::size_t line = 0; line < active_lines; ++line) {
            const float householder = 2.0F * tap_mean - feedback_taps[line];
            const float mixed_feedback = feedback_taps[line] *
                    (1.0F - parameters_.diffusion) +
                householder * parameters_.diffusion;
            const float feedback_gain =
                parameters_.stack_mode == PlexDelayStackMode::off
                    ? feedback_gains_[line] : 1.0F;
            storage_[line * capacity_frames_ + write_frame_] = safe_signal(
                pre_delayed + mixed_feedback * feedback_gain);
        }
        if (pre_delay_frames_ != 0U) {
            storage_[plex_delay_line_count * capacity_frames_ + write_frame_] =
                input;
        }
        write_frame_ = (write_frame_ + 1U) % capacity_frames_;

        if (channels == 1U) {
            interleaved[offset] = safe_signal(
                dry_left * (1.0F - mix) +
                wet_mono * mix * output_level_linear_);
        } else {
            interleaved[offset] = safe_signal(
                dry_left * (1.0F - mix) +
                wet_left * mix * output_level_linear_ * balance_left);
            interleaved[offset + 1U] = safe_signal(
                dry_right * (1.0F - mix) +
                wet_right * mix * output_level_linear_ * balance_right);
        }
    }
}

void PlexDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    low_cut_input_.fill(0.0F);
    low_cut_output_.fill(0.0F);
    high_cut_output_.fill(0.0F);
    write_frame_ = 0U;
}

} // namespace tabletfx
