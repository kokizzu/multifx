#include "tabletfx/effects/ten_tap_delay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr double kPiOverTwo = 1.57079632679489661923;
constexpr double kPi = 3.14159265358979323846;

float bounded_signal(double value) noexcept {
    if (!std::isfinite(value)) return 0.0F;
    constexpr double limit = 8.0;
    return static_cast<float>(std::clamp(value, -limit, limit));
}

} // namespace

TenTapDelay::TenTapDelay(float sample_rate, std::uint16_t channels,
                         float max_delay_ms,
                         TenTapDelayParameters parameters)
    : sample_rate_(sample_rate), channels_(channels),
      requested_max_delay_ms_(max_delay_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0U || channels_ > 2U ||
        !std::isfinite(requested_max_delay_ms_) ||
        requested_max_delay_ms_ < kMinimumDelayMs ||
        requested_max_delay_ms_ > kMaximumRequestedDelayMs) {
        throw std::invalid_argument(
            "Ten-Tap Delay requires 8–384 kHz, mono/stereo, and a 1–20000 ms buffer request");
    }

    const std::size_t maximum_storage_samples =
        ten_tap_delay_storage_limit_bytes / sizeof(float);
    const std::size_t maximum_frames_by_memory =
        maximum_storage_samples / static_cast<std::size_t>(channels_);
    const double requested_delay_frames = std::ceil(
        static_cast<double>(sample_rate_) *
        static_cast<double>(requested_max_delay_ms_) / 1000.0);
    if (!std::isfinite(requested_delay_frames) || requested_delay_frames < 1.0 ||
        requested_delay_frames >=
            static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw std::length_error("Ten-Tap Delay buffer size is invalid");
    }

    const std::size_t requested_capacity =
        static_cast<std::size_t>(requested_delay_frames) + 1U;
    capacity_frames_ = std::min(requested_capacity, maximum_frames_by_memory);
    if (capacity_frames_ < 2U ||
        capacity_frames_ > storage_.max_size() /
                               static_cast<std::size_t>(channels_)) {
        throw std::length_error("Ten-Tap Delay exceeds its fixed memory cap");
    }
    storage_.assign(capacity_frames_ * static_cast<std::size_t>(channels_), 0.0F);
    if (storage_.capacity() > maximum_storage_samples) {
        throw std::length_error(
            "Ten-Tap Delay allocator exceeded its one-MiB storage cap");
    }

    set_parameters(parameters);
}

void TenTapDelay::configure_rhythm_pattern(
    TenTapDivision quantize, float feedback_percent,
    const std::array<float, ten_tap_delay_max_taps>& tap_time_values) noexcept {
    if (!valid_division(quantize)) quantize = TenTapDivision::free;
    rhythm_mode_ = true;
    rhythm_quantize_ = quantize;
    rhythm_feedback_percent_ = bounded(feedback_percent, 0.0F, 0.0F, 95.0F);
    for (std::size_t tap = 0; tap < ten_tap_delay_max_taps; ++tap) {
        rhythm_tap_time_values_[tap] = bounded(
            tap_time_values[tap], 220.0F, 1.0F, kMaximumDelayMs);
    }
}

float TenTapDelay::bounded(float value, float fallback,
                           float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

float TenTapDelay::division_beats(TenTapDivision division) noexcept {
    switch (division) {
    case TenTapDivision::whole: return 4.0F;
    case TenTapDivision::half: return 2.0F;
    case TenTapDivision::dotted_half: return 3.0F;
    case TenTapDivision::half_triplet: return 4.0F / 3.0F;
    case TenTapDivision::quarter: return 1.0F;
    case TenTapDivision::dotted_quarter: return 1.5F;
    case TenTapDivision::quarter_triplet: return 2.0F / 3.0F;
    case TenTapDivision::eighth: return 0.5F;
    case TenTapDivision::dotted_eighth: return 0.75F;
    case TenTapDivision::eighth_triplet: return 1.0F / 3.0F;
    case TenTapDivision::sixteenth: return 0.25F;
    case TenTapDivision::dotted_sixteenth: return 0.375F;
    case TenTapDivision::sixteenth_triplet: return 1.0F / 6.0F;
    case TenTapDivision::thirty_second: return 0.125F;
    case TenTapDivision::thirty_second_triplet: return 1.0F / 12.0F;
    case TenTapDivision::free: return 0.0F;
    }
    return 0.0F;
}

bool TenTapDelay::valid_division(TenTapDivision division) noexcept {
    switch (division) {
    case TenTapDivision::free:
    case TenTapDivision::whole:
    case TenTapDivision::half:
    case TenTapDivision::dotted_half:
    case TenTapDivision::half_triplet:
    case TenTapDivision::quarter:
    case TenTapDivision::dotted_quarter:
    case TenTapDivision::quarter_triplet:
    case TenTapDivision::eighth:
    case TenTapDivision::dotted_eighth:
    case TenTapDivision::eighth_triplet:
    case TenTapDivision::sixteenth:
    case TenTapDivision::dotted_sixteenth:
    case TenTapDivision::sixteenth_triplet:
    case TenTapDivision::thirty_second:
    case TenTapDivision::thirty_second_triplet:
        return true;
    }
    return false;
}

bool TenTapDelay::valid_pan_shape(TenTapPanShape shape) noexcept {
    switch (shape) {
    case TenTapPanShape::constant:
    case TenTapPanShape::increasing:
    case TenTapPanShape::decreasing:
    case TenTapPanShape::sine:
        return true;
    }
    return false;
}

float TenTapDelay::maximum_delay_ms() const noexcept {
    const double memory_limited_ms =
        static_cast<double>(capacity_frames_ - 1U) * 1000.0 /
        static_cast<double>(sample_rate_);
    return static_cast<float>(std::min(
        static_cast<double>(requested_max_delay_ms_), memory_limited_ms));
}

float TenTapDelay::tap_interval_ms() const noexcept {
    if (parameters_.division == TenTapDivision::free) {
        return parameters_.delay_time_ms;
    }
    const float beats = division_beats(parameters_.division);
    return (60000.0F / parameters_.tempo_bpm) * beats;
}

std::size_t TenTapDelay::tap_delay_samples(std::size_t tap,
                                           std::uint16_t channel) const noexcept {
    if (tap >= ten_tap_delay_max_taps || channel > 1U) return 0U;
    return delay_frames_[tap][channel];
}

float TenTapDelay::effective_tap_pan(std::size_t tap) const noexcept {
    return tap < ten_tap_delay_max_taps ? effective_pans_[tap] : 0.5F;
}

float TenTapDelay::shaped_pan(std::size_t tap,
                              std::size_t tap_count) const noexcept {
    if (tap >= tap_count || tap_count == 0U ||
        parameters_.pan_shape == TenTapPanShape::constant) {
        return 0.5F;
    }

    const float position = tap_count <= 1U
        ? 0.5F
        : static_cast<float>(tap) / static_cast<float>(tap_count - 1U);
    float shape_position = position;
    switch (parameters_.pan_shape) {
    case TenTapPanShape::constant:
        shape_position = 0.5F;
        break;
    case TenTapPanShape::increasing:
        shape_position = position;
        break;
    case TenTapPanShape::decreasing:
        shape_position = 1.0F - position;
        break;
    case TenTapPanShape::sine:
        shape_position = static_cast<float>(
            0.5 + 0.5 * std::sin(kPi * static_cast<double>(position)));
        break;
    }
    const float amount = (parameters_.spread_percent / 100.0F) *
                         (parameters_.pan_alpha_percent / 100.0F);
    return std::clamp(0.5F + (shape_position - 0.5F) * amount, 0.0F, 1.0F);
}

void TenTapDelay::set_parameters(TenTapDelayParameters parameters) noexcept {
    parameters.tap_count = std::clamp(
        parameters.tap_count, std::uint8_t{1},
        static_cast<std::uint8_t>(ten_tap_delay_max_taps));
    parameters.delay_time_ms = bounded(
        parameters.delay_time_ms, parameters_.delay_time_ms,
        kMinimumDelayMs, kMaximumDelayMs);
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, parameters_.tempo_bpm,
                                    20.0F, 300.0F);
    if (!valid_division(parameters.division)) {
        parameters.division = parameters_.division;
    }
    parameters.decay_percent = bounded(parameters.decay_percent,
                                        parameters_.decay_percent, 0.0F, 100.0F);
    parameters.shuffle_percent = bounded(parameters.shuffle_percent,
                                          parameters_.shuffle_percent, 0.0F, 100.0F);
    parameters.spread_percent = bounded(parameters.spread_percent,
                                          parameters_.spread_percent, 0.0F, 100.0F);
    parameters.ratio_percent = bounded(parameters.ratio_percent,
                                         parameters_.ratio_percent, 50.0F, 200.0F);
    if (!valid_pan_shape(parameters.pan_shape)) {
        parameters.pan_shape = parameters_.pan_shape;
    }
    parameters.pan_alpha_percent = bounded(parameters.pan_alpha_percent,
        parameters_.pan_alpha_percent, 0.0F, 100.0F);
    const float maximum_cut = std::min(20000.0F, sample_rate_ * 0.45F);
    parameters.low_cut_hz = bounded(parameters.low_cut_hz,
        parameters_.low_cut_hz, 0.0F, std::min(10000.0F, maximum_cut));
    parameters.high_cut_hz = bounded(parameters.high_cut_hz,
        parameters_.high_cut_hz, std::min(1000.0F, maximum_cut), maximum_cut);
    if (parameters.low_cut_hz >= parameters.high_cut_hz) {
        parameters.low_cut_hz = std::max(0.0F, parameters.high_cut_hz - 1.0F);
    }
    parameters.mix = bounded(parameters.mix, parameters_.mix, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, parameters_.level_db,
                                   -24.0F, 12.0F);

    const float max_time = maximum_delay_ms();
    for (float& level : parameters.tap_level_db) {
        level = bounded(level, 0.0F, kMinimumLevelDb, kMaximumLevelDb);
    }
    parameters_ = parameters;

    const std::size_t max_frames = capacity_frames_ - 1U;
    const float ratio = parameters_.ratio_percent / 100.0F;
    const float shuffle = parameters_.shuffle_percent / 200.0F;
    const float decay_gain = 1.0F - parameters_.decay_percent / 100.0F;
    float cumulative_rhythm_ms = 0.0F;
    const float rhythm_division_ms = rhythm_quantize_ == TenTapDivision::free
        ? 0.0F
        : (60000.0F / parameters_.tempo_bpm) * division_beats(rhythm_quantize_);
    for (std::size_t tap = 0; tap < ten_tap_delay_max_taps; ++tap) {
        const float tap_number = static_cast<float>(tap + 1U);
        const float shuffle_offset = (tap % 2U == 1U) ? shuffle : 0.0F;
        float right_ms = 0.0F;
        if (rhythm_mode_) {
            const float interval_ms = rhythm_quantize_ == TenTapDivision::free
                ? rhythm_tap_time_values_[tap]
                : rhythm_tap_time_values_[tap] * rhythm_division_ms;
            cumulative_rhythm_ms += interval_ms;
            right_ms = cumulative_rhythm_ms;
        } else {
            const float interval_ms = std::min(tap_interval_ms(), max_time);
            right_ms = interval_ms * (tap_number + shuffle_offset);
        }
        const float left_ms = right_ms * ratio;
        const std::array<float, 2> times{left_ms, right_ms};
        for (std::size_t channel = 0; channel < 2U; ++channel) {
            const double requested_frames = std::round(
                static_cast<double>(sample_rate_) *
                static_cast<double>(std::min(times[channel], max_time)) / 1000.0);
            delay_frames_[tap][channel] = std::clamp<std::size_t>(
                static_cast<std::size_t>(std::max(1.0, requested_frames)),
                1U, max_frames);
        }
        const double envelope = std::pow(static_cast<double>(decay_gain),
                                          static_cast<double>(tap));
        const double tap_level = std::pow(10.0,
            static_cast<double>(parameters_.tap_level_db[tap]) / 20.0);
        level_gains_[tap] = static_cast<float>(envelope * tap_level);
        effective_pans_[tap] = shaped_pan(tap, parameters_.tap_count);
    }

    const double sample_rate = static_cast<double>(sample_rate_);
    low_cut_coefficient_ = parameters_.low_cut_hz <= 0.0F
        ? 0.0F
        : static_cast<float>(std::exp(-2.0 * kPi *
            static_cast<double>(parameters_.low_cut_hz) / sample_rate));
    high_cut_coefficient_ = static_cast<float>(1.0 - std::exp(
        -2.0 * kPi * static_cast<double>(parameters_.high_cut_hz) / sample_rate));
}

float TenTapDelay::read_delay(std::size_t delay_frames,
                              std::uint16_t channel) const noexcept {
    const std::size_t offset = delay_frames % capacity_frames_;
    const std::size_t read_frame =
        (write_frame_ + capacity_frames_ - offset) % capacity_frames_;
    return storage_[read_frame * static_cast<std::size_t>(channels_) + channel];
}

float TenTapDelay::filter_wet(float sample, std::uint16_t channel) noexcept {
    if (low_cut_coefficient_ > 0.0F) {
        const float high_pass = low_cut_coefficient_ *
            (low_cut_output_[channel] + sample - low_cut_input_[channel]);
        low_cut_input_[channel] = sample;
        low_cut_output_[channel] = high_pass;
        sample = high_pass;
    }
    high_cut_output_[channel] += high_cut_coefficient_ *
        (sample - high_cut_output_[channel]);
    return std::isfinite(high_cut_output_[channel])
        ? high_cut_output_[channel]
        : 0.0F;
}

void TenTapDelay::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels == 0U || channels != channels_) return;

    const std::size_t frames = interleaved.size() /
                               static_cast<std::size_t>(channels_);
    const double master_level = std::pow(10.0,
        static_cast<double>(parameters_.level_db) / 20.0);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * static_cast<std::size_t>(channels_);
        std::array<float, 2> dry{};
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const float input = interleaved[offset + channel];
            dry[channel] = std::isfinite(input)
                ? std::clamp(input, -kMaximumSignal, kMaximumSignal)
                : 0.0F;
        }

        double wet_left = 0.0;
        double wet_right = 0.0;
        double wet_mono = 0.0;
        std::array<float, 2> final_tap_signal{};
        for (std::size_t tap = 0; tap < parameters_.tap_count; ++tap) {
            float left = read_delay(delay_frames_[tap][0], 0U);
            float right = channels_ == 2U
                ? read_delay(delay_frames_[tap][1], 1U)
                : left;
            if (parameters_.mono && channels_ == 2U) {
                const float left_average = 0.5F *
                    (read_delay(delay_frames_[tap][0], 0U) +
                     read_delay(delay_frames_[tap][0], 1U));
                const float right_average = 0.5F *
                    (read_delay(delay_frames_[tap][1], 0U) +
                     read_delay(delay_frames_[tap][1], 1U));
                left = left_average;
                right = right_average;
            }
            const double gain = static_cast<double>(level_gains_[tap]);
            if (tap + 1U == parameters_.tap_count) {
                final_tap_signal[0] = bounded_signal(
                    static_cast<double>(left) * gain);
                final_tap_signal[1] = bounded_signal(
                    static_cast<double>(right) * gain);
            }
            if (channels_ == 1U) {
                wet_mono += 0.5 * (static_cast<double>(left) +
                                   static_cast<double>(right)) * gain;
            } else {
                const double angle = static_cast<double>(effective_pans_[tap]) *
                                     kPiOverTwo;
                const double left_gain = std::cos(angle);
                const double right_gain = std::sin(angle);
                wet_left += static_cast<double>(left) * gain * left_gain;
                wet_right += static_cast<double>(right) * gain * right_gain;
            }
        }

        const std::size_t write_offset =
            write_frame_ * static_cast<std::size_t>(channels_);
        const float feedback_gain = rhythm_mode_
            ? rhythm_feedback_percent_ / 100.0F
            : 0.0F;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const float input_mono = channels_ == 2U
                ? 0.5F * (dry[0] + dry[1])
                : dry[0];
            const float input = parameters_.mono ? input_mono : dry[channel];
            const float feedback = parameters_.mono
                ? 0.5F * (final_tap_signal[0] + final_tap_signal[1])
                : final_tap_signal[channel];
            storage_[write_offset + channel] = bounded_signal(
                static_cast<double>(input) +
                static_cast<double>(feedback) * feedback_gain);
        }

        if (channels_ == 1U) {
            const float wet = filter_wet(bounded_signal(wet_mono * master_level), 0U);
            interleaved[offset] = bounded_signal(
                static_cast<double>(dry[0]) * (1.0 - parameters_.mix) +
                static_cast<double>(wet) * parameters_.mix);
        } else {
            const float wet_l = filter_wet(
                bounded_signal(wet_left * master_level), 0U);
            const float wet_r = filter_wet(
                bounded_signal(wet_right * master_level), 1U);
            if (parameters_.mono) {
                const float wet = 0.5F * (wet_l + wet_r);
                interleaved[offset] = bounded_signal(
                    static_cast<double>(dry[0]) * (1.0 - parameters_.mix) +
                    static_cast<double>(wet) * parameters_.mix);
                interleaved[offset + 1U] = bounded_signal(
                    static_cast<double>(dry[1]) * (1.0 - parameters_.mix) +
                    static_cast<double>(wet) * parameters_.mix);
            } else {
                interleaved[offset] = bounded_signal(
                    static_cast<double>(dry[0]) * (1.0 - parameters_.mix) +
                    static_cast<double>(wet_l) * parameters_.mix);
                interleaved[offset + 1U] = bounded_signal(
                    static_cast<double>(dry[1]) * (1.0 - parameters_.mix) +
                    static_cast<double>(wet_r) * parameters_.mix);
            }
        }
        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
    }
}

void TenTapDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    low_cut_input_.fill(0.0F);
    low_cut_output_.fill(0.0F);
    high_cut_output_.fill(0.0F);
    write_frame_ = 0;
}

RhythmTapDelay::RhythmTapDelay(float sample_rate, std::uint16_t channels,
                               RhythmTapDelayParameters parameters)
    : TenTapDelay(sample_rate, channels, 20000.0F, parameters.common) {
    set_parameters(parameters);
}

void RhythmTapDelay::set_parameters(
    RhythmTapDelayParameters parameters) noexcept {
    if (!TenTapDelay::valid_division(parameters.quantize)) {
        parameters.quantize = parameters_.quantize;
    }
    parameters.feedback_percent = TenTapDelay::bounded(
        parameters.feedback_percent, parameters_.feedback_percent, 0.0F, 95.0F);
    for (std::size_t tap = 0; tap < ten_tap_delay_max_taps; ++tap) {
        parameters.tap_time_values[tap] = TenTapDelay::bounded(
            parameters.tap_time_values[tap], parameters_.tap_time_values[tap],
            1.0F, TenTapDelay::kMaximumDelayMs);
    }
    parameters.common.tap_count = std::clamp(
        parameters.common.tap_count, std::uint8_t{1},
        static_cast<std::uint8_t>(ten_tap_delay_max_taps));
    parameters.common.division = TenTapDivision::free;
    parameters.common.shuffle_percent = 0.0F;
    parameters.common.ratio_percent = 100.0F;
    parameters_ = parameters;
    configure_rhythm_pattern(parameters_.quantize,
                             parameters_.feedback_percent,
                             parameters_.tap_time_values);
    TenTapDelay::set_parameters(parameters_.common);
    parameters_.common = TenTapDelay::parameters();
}

} // namespace tabletfx
