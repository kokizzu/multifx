#include "tabletfx/effects/multi_delay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr double kPiOverTwo = 1.57079632679489661923;
constexpr float kMaximumSeriesFeedback = 0.9F;

float next_random_lfo_value(std::uint32_t& state) noexcept {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    const float unit = static_cast<float>(state >> 8U) / 16777215.0F;
    return unit * 2.0F - 1.0F;
}

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
    case DelayDivision::thirty_second:
    case DelayDivision::sixty_fourth:
        return true;
    }
    return false;
}

bool MultiDelay::valid_lfo_waveform(MultiDelayLfoWaveform waveform) noexcept {
    switch (waveform) {
    case MultiDelayLfoWaveform::sine:
    case MultiDelayLfoWaveform::triangle:
    case MultiDelayLfoWaveform::square:
    case MultiDelayLfoWaveform::saw_up:
    case MultiDelayLfoWaveform::saw_down:
    case MultiDelayLfoWaveform::random:
    case MultiDelayLfoWaveform::log:
    case MultiDelayLfoWaveform::exponential:
    case MultiDelayLfoWaveform::trapezoid:
    case MultiDelayLfoWaveform::astable:
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
    case DelayDivision::thirty_second: return 0.125F;
    case DelayDivision::sixty_fourth: return 0.0625F;
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
    const bool was_quad_tape =
        parameters_.topology == MultiDelayTopology::quad_tape;
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
    case MultiDelayTopology::quad_tape:
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
    parameters.motor_speed = bounded(parameters.motor_speed,
        parameters_.motor_speed, kMinimumMotorSpeed, kMaximumMotorSpeed);
    const float motor_speed_change =
        std::abs(parameters.motor_speed - parameters_.motor_speed);
    if (motor_speed_change > 0.0F) {
        read_head_slew_per_sample_ = std::min(
            motor_speed_change, kMaximumReadHeadSlewPerSample);
    }
    const float maximum_master_feedback =
        parameters.topology == MultiDelayTopology::quad_tap ||
                parameters.topology == MultiDelayTopology::quad_tap_band ||
                parameters.topology == MultiDelayTopology::quad_tape
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

    for (std::size_t lfo = 0; lfo < multi_delay_time_lfo_count; ++lfo) {
        auto& lfo_parameters = parameters.time_lfo[lfo];
        const auto& previous = parameters_.time_lfo[lfo];
        if (!valid_lfo_waveform(lfo_parameters.waveform)) {
            lfo_parameters.waveform = previous.waveform;
        }
        lfo_parameters.phase_degrees = bounded(
            lfo_parameters.phase_degrees, previous.phase_degrees,
            kMinimumTimeLfoPhaseDegrees, kMaximumTimeLfoPhaseDegrees);
        lfo_parameters.rate_hz = bounded(
            lfo_parameters.rate_hz, previous.rate_hz,
            kMinimumTimeLfoRateHz, kMaximumTimeLfoRateHz);
        if (!valid_division(lfo_parameters.division)) {
            lfo_parameters.division = previous.division;
        }
        lfo_parameters.depth_ms = bounded(
            lfo_parameters.depth_ms, previous.depth_ms, 0.0F,
            kMaximumTimeLfoDepthMs);
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
    effective_mix_ = parameters_.global_mix_enabled
        ? std::clamp(parameters_.mix + parameters_.global_mix_offset,
                     0.0F, 1.0F)
        : parameters_.mix;
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
    update_line_times(!was_quad_tape &&
                      parameters_.topology == MultiDelayTopology::quad_tape);
    prepare_time_lfos();
}

void MultiDelay::update_line_times(bool initialize_tape_heads) noexcept {
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
            parameters_.topology == MultiDelayTopology::quad_tap_band ||
            parameters_.topology == MultiDelayTopology::quad_tape) {
            time_ms *= parameters_.master_time;
        }
        if (parameters_.topology == MultiDelayTopology::quad_tape) {
            time_ms /= parameters_.motor_speed;
        }
        time_ms = std::clamp(time_ms, kMinimumDelayMs, maximum_time);
        const double requested_frames = std::round(
            static_cast<double>(sample_rate_) *
            static_cast<double>(time_ms) / 1000.0);
        delay_frames_[line] = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::max(1.0, requested_frames)),
            1U, capacity_frames_ - 1U);
        if (parameters_.topology != MultiDelayTopology::quad_tape ||
            initialize_tape_heads) {
            read_delay_frames_[line] = static_cast<float>(delay_frames_[line]);
        }
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

float MultiDelay::read_interpolated(float delay,
                                    std::size_t line) const noexcept {
    const float bounded_delay = std::clamp(
        delay, 1.0F, static_cast<float>(capacity_frames_ - 1U));
    const std::size_t whole_delay = static_cast<std::size_t>(bounded_delay);
    const float fraction = bounded_delay - static_cast<float>(whole_delay);
    const float nearer = storage_[read_frame(whole_delay, line)];
    const std::size_t farther_delay = std::min(whole_delay + 1U,
                                                capacity_frames_ - 1U);
    const float farther = storage_[read_frame(farther_delay, line)];
    return nearer + fraction * (farther - nearer);
}

void MultiDelay::prepare_time_lfos() noexcept {
    constexpr float two_pi = 4.0F * static_cast<float>(kPiOverTwo);
    time_lfo_active_ = false;
    for (std::size_t lfo = 0; lfo < multi_delay_time_lfo_count; ++lfo) {
        const auto& parameters = parameters_.time_lfo[lfo];
        float rate_hz = parameters.tempo_sync
            ? parameters_.tempo_bpm /
                  (60.0F * beats_for_division(parameters.division))
            : parameters.rate_hz;
        const float maximum_rate_hz = parameters.tempo_sync
            ? kMaximumTimeLfoSyncRateHz
            : kMaximumTimeLfoRateHz;
        rate_hz = std::clamp(rate_hz, kMinimumTimeLfoRateHz,
                             maximum_rate_hz);
        const float phase_increment = rate_hz / sample_rate_;
        time_lfo_phase_increment_[lfo] = phase_increment;
        const float step_radians = two_pi * phase_increment;
        time_lfo_sine_increment_[lfo] = std::sin(step_radians);
        time_lfo_cosine_increment_[lfo] = std::cos(step_radians);
        time_lfo_depth_frames_[lfo] =
            parameters.depth_ms * sample_rate_ / 1000.0F;
        time_lfo_active_ = time_lfo_active_ || parameters.depth_ms > 0.0F;
        time_lfo_normalization_counter_[lfo] = 0U;

        for (std::size_t line = 0; line < multi_delay_line_count; ++line) {
            if (time_lfo_random_state_[lfo][line] == 0U) {
                std::uint32_t seed = 0xA341316CU ^
                    (static_cast<std::uint32_t>(lfo + 1U) * 0x9E3779B9U) ^
                    (static_cast<std::uint32_t>(line + 1U) * 0x85EBCA6BU);
                if (seed == 0U) seed = 1U;
                time_lfo_random_state_[lfo][line] = seed;
                time_lfo_random_value_[lfo][line] =
                    next_random_lfo_value(time_lfo_random_state_[lfo][line]);
            }
            float phase_offset_degrees = 0.0F;
            if (line == 1U) {
                phase_offset_degrees = parameters.phase_degrees;
            } else if (line == 2U) {
                phase_offset_degrees = 180.0F;
            } else if (line == 3U) {
                phase_offset_degrees = parameters.phase_degrees + 180.0F;
            }
            float phase = time_lfo_base_phase_[lfo] +
                          phase_offset_degrees / 360.0F;
            phase -= std::floor(phase);
            time_lfo_tap_phase_[lfo][line] = phase;
            const float radians = two_pi * phase;
            time_lfo_tap_sine_[lfo][line] = std::sin(radians);
            time_lfo_tap_cosine_[lfo][line] = std::cos(radians);
        }
    }
}

void MultiDelay::reset_time_lfos() noexcept {
    time_lfo_base_phase_.fill(0.0F);
    time_lfo_normalization_counter_.fill(0U);
    for (std::size_t lfo = 0; lfo < multi_delay_time_lfo_count; ++lfo) {
        time_lfo_random_state_[lfo].fill(0U);
        time_lfo_random_value_[lfo].fill(0.0F);
    }
    prepare_time_lfos();
}

float MultiDelay::time_lfo_waveform_value(std::size_t lfo,
                                          std::size_t line) const noexcept {
    if (lfo >= multi_delay_time_lfo_count ||
        line >= multi_delay_line_count) {
        return 0.0F;
    }
    const float phase = time_lfo_tap_phase_[lfo][line];
    switch (parameters_.time_lfo[lfo].waveform) {
    case MultiDelayLfoWaveform::sine:
        return time_lfo_tap_sine_[lfo][line];
    case MultiDelayLfoWaveform::triangle:
        return phase < 0.5F ? -1.0F + 4.0F * phase
                            : 3.0F - 4.0F * phase;
    case MultiDelayLfoWaveform::square:
        return phase < 0.5F ? 1.0F : -1.0F;
    case MultiDelayLfoWaveform::saw_up:
        return 2.0F * phase - 1.0F;
    case MultiDelayLfoWaveform::saw_down:
        return 1.0F - 2.0F * phase;
    case MultiDelayLfoWaveform::random:
        return time_lfo_random_value_[lfo][line];
    case MultiDelayLfoWaveform::log:
        return 2.0F * std::log1p(9.0F * phase) / 2.302585093F - 1.0F;
    case MultiDelayLfoWaveform::exponential:
        return 2.0F * (std::exp(5.0F * phase) - 1.0F) /
                   147.413162F - 1.0F;
    case MultiDelayLfoWaveform::trapezoid:
        if (phase < 0.25F) return -1.0F + 8.0F * phase;
        if (phase < 0.75F) return 1.0F;
        return 7.0F - 8.0F * phase;
    case MultiDelayLfoWaveform::astable:
        return phase < 0.25F ? 1.0F : -1.0F;
    }
    return 0.0F;
}

float MultiDelay::time_lfo_offset_frames(std::size_t line) const noexcept {
    float offset_frames = 0.0F;
    for (std::size_t lfo = 0; lfo < multi_delay_time_lfo_count; ++lfo) {
        offset_frames += time_lfo_depth_frames_[lfo] *
                         time_lfo_waveform_value(lfo, line);
    }
    return offset_frames;
}

void MultiDelay::advance_time_lfos() noexcept {
    for (std::size_t lfo = 0; lfo < multi_delay_time_lfo_count; ++lfo) {
        const float increment = time_lfo_phase_increment_[lfo];
        time_lfo_base_phase_[lfo] += increment;
        if (time_lfo_base_phase_[lfo] >= 1.0F) {
            time_lfo_base_phase_[lfo] -= 1.0F;
        }

        for (std::size_t line = 0; line < multi_delay_line_count; ++line) {
            float& phase = time_lfo_tap_phase_[lfo][line];
            phase += increment;
            if (phase >= 1.0F) {
                phase -= 1.0F;
                if (parameters_.time_lfo[lfo].waveform ==
                    MultiDelayLfoWaveform::random) {
                    time_lfo_random_value_[lfo][line] =
                        next_random_lfo_value(time_lfo_random_state_[lfo][line]);
                }
            }
            const float old_sine = time_lfo_tap_sine_[lfo][line];
            time_lfo_tap_sine_[lfo][line] =
                old_sine * time_lfo_cosine_increment_[lfo] +
                time_lfo_tap_cosine_[lfo][line] *
                    time_lfo_sine_increment_[lfo];
            time_lfo_tap_cosine_[lfo][line] =
                time_lfo_tap_cosine_[lfo][line] *
                    time_lfo_cosine_increment_[lfo] -
                old_sine * time_lfo_sine_increment_[lfo];
        }

        if (++time_lfo_normalization_counter_[lfo] == 4096U) {
            for (std::size_t line = 0; line < multi_delay_line_count; ++line) {
                const float sine = time_lfo_tap_sine_[lfo][line];
                const float cosine = time_lfo_tap_cosine_[lfo][line];
                const float magnitude_squared = sine * sine + cosine * cosine;
                if (std::isfinite(magnitude_squared) &&
                    magnitude_squared > 0.0F) {
                    const float inverse_magnitude =
                        1.0F / std::sqrt(magnitude_squared);
                    time_lfo_tap_sine_[lfo][line] *= inverse_magnitude;
                    time_lfo_tap_cosine_[lfo][line] *= inverse_magnitude;
                } else {
                    time_lfo_tap_sine_[lfo][line] = 0.0F;
                    time_lfo_tap_cosine_[lfo][line] = 1.0F;
                }
            }
            time_lfo_normalization_counter_[lfo] = 0U;
        }
    }
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
                const float delayed_state = time_lfo_active_
                    ? read_interpolated(
                          static_cast<float>(delay_frames_[line]) +
                              time_lfo_offset_frames(line), line)
                    : storage_[read_frame(delay_frames_[line], line)];
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
                float delayed = 0.0F;
                if (parameters_.topology == MultiDelayTopology::quad_tape) {
                    const float target = static_cast<float>(delay_frames_[line]);
                    const float difference = target - read_delay_frames_[line];
                    read_delay_frames_[line] += std::clamp(
                        difference, -read_head_slew_per_sample_,
                        read_head_slew_per_sample_);
                    const float read_delay = read_delay_frames_[line] +
                        (time_lfo_active_ ? time_lfo_offset_frames(line) : 0.0F);
                    delayed = read_interpolated(read_delay, line);
                } else {
                    delayed = time_lfo_active_
                        ? read_interpolated(
                              static_cast<float>(delay_frames_[line]) +
                                  time_lfo_offset_frames(line), line)
                        : storage_[read_frame(delay_frames_[line], line)];
                }
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
                         parameters_.topology == MultiDelayTopology::quad_tap_band ||
                         parameters_.topology == MultiDelayTopology::quad_tape
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
        if (time_lfo_active_) {
            advance_time_lfos();
        }
    }
}

void MultiDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    low_cut_input_.fill(0.0F);
    low_cut_output_.fill(0.0F);
    high_cut_output_.fill(0.0F);
    for (std::size_t line = 0; line < multi_delay_line_count; ++line) {
        read_delay_frames_[line] = static_cast<float>(delay_frames_[line]);
    }
    write_frame_ = 0U;
    reset_time_lfos();
}

} // namespace tabletfx
