#include "tabletfx/effects/megatap_delay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
constexpr float kAllpassCoefficient = 0.65F;
constexpr std::array<float, 4U> kDiffusionDelayRatios{
    0.17F, 0.31F, 0.43F, 0.61F};

std::uint32_t next_random(std::uint32_t& state) noexcept {
    if (state == 0U) state = 0xA341316CU;
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return state;
}

float random_unit(std::uint32_t& state) noexcept {
    return static_cast<float>(next_random(state) >> 8U) / 16777215.0F;
}

} // namespace

MegatapDelay::MegatapDelay(float sample_rate, float max_storage_ms,
                           MegatapDelayParameters parameters)
    : sample_rate_(sample_rate), requested_max_storage_ms_(max_storage_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate ||
        !std::isfinite(requested_max_storage_ms_) ||
        requested_max_storage_ms_ < 1.0F || requested_max_storage_ms_ > 10000.0F) {
        throw std::invalid_argument(
            "Megatap Delay requires 8–384 kHz and a 1–10000 ms storage request");
    }

    const std::size_t maximum_storage_samples =
        megatap_delay_storage_limit_bytes / sizeof(float);
    const double requested_frames = std::ceil(
        static_cast<double>(sample_rate_) *
        static_cast<double>(requested_max_storage_ms_) / 1000.0);
    if (!std::isfinite(requested_frames) || requested_frames < 1.0 ||
        requested_frames >=
            static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw std::length_error("Megatap Delay storage size is invalid");
    }
    capacity_frames_ = std::min(static_cast<std::size_t>(requested_frames) + 1U,
                                maximum_storage_samples);
    if (capacity_frames_ < 2U || capacity_frames_ > storage_.max_size()) {
        throw std::length_error("Megatap Delay exceeds its fixed memory cap");
    }
    storage_.assign(capacity_frames_, 0.0F);
    if (storage_.capacity() > maximum_storage_samples) {
        throw std::length_error(
            "Megatap Delay allocator exceeded its one-MiB storage cap");
    }

    set_parameters(parameters);
}

float MegatapDelay::bounded(float value, float fallback,
                            float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

bool MegatapDelay::valid_amplitude_shape(
    MegatapAmplitudeShape shape) noexcept {
    switch (shape) {
    case MegatapAmplitudeShape::constant:
    case MegatapAmplitudeShape::increasing:
    case MegatapAmplitudeShape::decreasing:
    case MegatapAmplitudeShape::sine:
    case MegatapAmplitudeShape::alternating:
        return true;
    }
    return false;
}

bool MegatapDelay::valid_pan_shape(MegatapPanShape shape) noexcept {
    switch (shape) {
    case MegatapPanShape::constant:
    case MegatapPanShape::increasing:
    case MegatapPanShape::decreasing:
    case MegatapPanShape::sine:
        return true;
    }
    return false;
}

bool MegatapDelay::valid_input_select(
    MegatapInputSelect input_select) noexcept {
    switch (input_select) {
    case MegatapInputSelect::sum:
    case MegatapInputSelect::left:
    case MegatapInputSelect::right:
        return true;
    }
    return false;
}

float MegatapDelay::one_pole_coefficient(float sample_rate,
                                         float milliseconds) noexcept {
    const double duration_samples = static_cast<double>(sample_rate) *
                                   static_cast<double>(milliseconds) / 1000.0;
    if (!std::isfinite(duration_samples) || duration_samples <= 0.0) return 0.0F;
    return static_cast<float>(std::exp(-1.0 / duration_samples));
}

float MegatapDelay::maximum_delay_ms() const noexcept {
    if (capacity_frames_ < 2U) return 0.0F;
    const double available = static_cast<double>(capacity_frames_ - 1U) *
                             1000.0 / static_cast<double>(sample_rate_);
    return static_cast<float>(std::min(
        static_cast<double>(requested_max_storage_ms_), available));
}

std::size_t MegatapDelay::tap_delay_samples(std::size_t tap) const noexcept {
    return tap < megatap_delay_max_taps ? tap_delay_samples_[tap] : 0U;
}

float MegatapDelay::tap_gain(std::size_t tap) const noexcept {
    return tap < megatap_delay_max_taps ? tap_gains_[tap] : 0.0F;
}

float MegatapDelay::tap_pan(std::size_t tap) const noexcept {
    return tap < megatap_delay_max_taps ? tap_pans_[tap] : 0.5F;
}

void MegatapDelay::set_parameters(MegatapDelayParameters parameters) noexcept {
    const float available_ms = maximum_delay_ms();
    const float maximum_time_ms = std::max(
        1.0F, std::min(kMaximumDelayMs, available_ms));
    parameters.delay_time_ms = bounded(parameters.delay_time_ms,
        parameters_.delay_time_ms, 1.0F, maximum_time_ms);
    parameters.tap_count = std::clamp(parameters.tap_count, std::uint8_t{1},
        static_cast<std::uint8_t>(megatap_delay_max_taps));
    parameters.time_alpha = bounded(parameters.time_alpha,
        parameters_.time_alpha, kMinimumTimeAlpha, kMaximumTimeAlpha);
    parameters.time_randomize_percent = bounded(
        parameters.time_randomize_percent,
        parameters_.time_randomize_percent, 0.0F, 100.0F);
    if (!valid_amplitude_shape(parameters.amplitude_shape)) {
        parameters.amplitude_shape = parameters_.amplitude_shape;
    }
    parameters.amplitude_alpha = bounded(parameters.amplitude_alpha,
        parameters_.amplitude_alpha, kMinimumAmplitudeAlpha,
        kMaximumAmplitudeAlpha);
    parameters.amplitude_randomize_percent = bounded(
        parameters.amplitude_randomize_percent,
        parameters_.amplitude_randomize_percent, 0.0F, 100.0F);
    if (!valid_pan_shape(parameters.pan_shape)) {
        parameters.pan_shape = parameters_.pan_shape;
    }
    parameters.pan_alpha_percent = bounded(parameters.pan_alpha_percent,
        parameters_.pan_alpha_percent, 0.0F, 100.0F);
    parameters.stereo_spread_percent = bounded(
        parameters.stereo_spread_percent,
        parameters_.stereo_spread_percent, -100.0F, 100.0F);
    parameters.predelay_ms = bounded(parameters.predelay_ms,
        parameters_.predelay_ms, 0.0F, kMaximumPredelayMs);
    parameters.predelay_ms = std::min(parameters.predelay_ms,
        std::max(0.0F, available_ms - parameters.delay_time_ms));
    parameters.feedback = bounded(parameters.feedback,
        parameters_.feedback, 0.0F, 0.95F);
    parameters.feedback_tap = std::clamp(parameters.feedback_tap,
        std::uint8_t{1}, parameters.tap_count);
    const float maximum_filter_hz = std::max(
        20.0F, std::min(20000.0F, sample_rate_ * 0.45F));
    parameters.low_cut_hz = bounded(parameters.low_cut_hz,
        parameters_.low_cut_hz, 0.0F, maximum_filter_hz);
    parameters.high_cut_hz = bounded(parameters.high_cut_hz,
        parameters_.high_cut_hz, 20.0F, maximum_filter_hz);
    if (parameters.high_cut_hz < parameters.low_cut_hz &&
        parameters.low_cut_hz > 0.0F) {
        parameters.high_cut_hz = parameters.low_cut_hz;
    }
    parameters.diffusion_mix = bounded(parameters.diffusion_mix,
        parameters_.diffusion_mix, 0.0F, 1.0F);
    parameters.diffusion_time_ms = bounded(parameters.diffusion_time_ms,
        parameters_.diffusion_time_ms, 0.1F, kMaximumDiffusionMs);
    parameters.diffusion_lfo_rate_hz = bounded(
        parameters.diffusion_lfo_rate_hz,
        parameters_.diffusion_lfo_rate_hz, 0.05F, 5.0F);
    parameters.diffusion_lfo_depth_ms = bounded(
        parameters.diffusion_lfo_depth_ms,
        parameters_.diffusion_lfo_depth_ms, 0.0F,
        kMaximumDiffusionModDepthMs);
    parameters.envelope_threshold_db = bounded(
        parameters.envelope_threshold_db,
        parameters_.envelope_threshold_db, -90.0F, 0.0F);
    parameters.envelope_attack_ms = bounded(parameters.envelope_attack_ms,
        parameters_.envelope_attack_ms, 1.0F, 5000.0F);
    parameters.envelope_release_ms = bounded(parameters.envelope_release_ms,
        parameters_.envelope_release_ms, 1.0F, 5000.0F);
    parameters.input_gain_db = bounded(parameters.input_gain_db,
        parameters_.input_gain_db, -24.0F, 24.0F);
    parameters.mix = bounded(parameters.mix, parameters_.mix, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db,
        parameters_.level_db, -24.0F, 12.0F);
    if (!valid_input_select(parameters.input_select)) {
        parameters.input_select = parameters_.input_select;
    }
    if (parameters.random_seed == 0U) parameters.random_seed = 0x4D475450U;

    parameters_ = parameters;
    input_gain_linear_ = static_cast<float>(std::pow(
        10.0, static_cast<double>(parameters_.input_gain_db) / 20.0));
    output_level_linear_ = static_cast<float>(std::pow(
        10.0, static_cast<double>(parameters_.level_db) / 20.0));
    low_cut_alpha_ = parameters_.low_cut_hz <= 0.0F
        ? 0.0F
        : std::exp(-2.0F * static_cast<float>(kPi) *
                   parameters_.low_cut_hz / sample_rate_);
    low_cut_coefficient_ = parameters_.low_cut_hz <= 0.0F
        ? 0.0F
        : 1.0F - low_cut_alpha_;
    high_cut_coefficient_ = 1.0F - std::exp(
        -2.0F * static_cast<float>(kPi) *
        parameters_.high_cut_hz / sample_rate_);
    envelope_threshold_ = static_cast<float>(std::pow(
        10.0, static_cast<double>(parameters_.envelope_threshold_db) / 20.0));
    envelope_attack_coefficient_ = one_pole_coefficient(
        sample_rate_, parameters_.envelope_attack_ms);
    envelope_release_coefficient_ = one_pole_coefficient(
        sample_rate_, parameters_.envelope_release_ms);
    diffusion_lfo_phase_step_ = 2.0F * static_cast<float>(kPi) *
        parameters_.diffusion_lfo_rate_hz / sample_rate_;
    diffusion_mod_depth_samples_ = parameters_.diffusion_lfo_depth_ms *
                                    sample_rate_ / 1000.0F;

    rebuild_taps();
    rebuild_diffuser();
}

void MegatapDelay::rebuild_taps() noexcept {
    std::array<double, megatap_delay_max_taps> tap_times{};
    const std::size_t tap_count = parameters_.tap_count;
    const double time_frames = static_cast<double>(sample_rate_) *
        static_cast<double>(parameters_.delay_time_ms) / 1000.0;
    double weight_sum = 0.0;
    for (std::size_t tap = 0; tap < tap_count; ++tap) {
        weight_sum += std::pow(static_cast<double>(parameters_.time_alpha),
                               static_cast<double>(tap));
    }

    std::uint32_t random_state = parameters_.random_seed;
    double cumulative_weight = 0.0;
    const double average_spacing = time_frames / static_cast<double>(tap_count);
    for (std::size_t tap = 0; tap < tap_count; ++tap) {
        cumulative_weight += std::pow(
            static_cast<double>(parameters_.time_alpha),
            static_cast<double>(tap));
        double time = time_frames * cumulative_weight / weight_sum;
        if (parameters_.time_randomize_percent > 0.0F) {
            const double random_offset =
                (static_cast<double>(random_unit(random_state)) * 2.0 - 1.0) *
                average_spacing *
                static_cast<double>(parameters_.time_randomize_percent) / 100.0;
            time = std::clamp(time + random_offset, 1.0, time_frames);
        }
        tap_times[tap] = time;
    }
    std::sort(tap_times.begin(), tap_times.begin() +
              static_cast<std::ptrdiff_t>(tap_count));

    const double predelay_frames = static_cast<double>(sample_rate_) *
        static_cast<double>(parameters_.predelay_ms) / 1000.0;
    const std::size_t maximum_delay = capacity_frames_ - 1U;
    for (std::size_t tap = 0; tap < tap_count; ++tap) {
        const double delay = std::round(tap_times[tap] + predelay_frames);
        tap_delay_samples_[tap] = static_cast<std::size_t>(std::clamp(
            delay, 1.0, static_cast<double>(maximum_delay)));
    }
    for (std::size_t tap = tap_count; tap < megatap_delay_max_taps; ++tap) {
        tap_delay_samples_[tap] = 0U;
    }

    for (std::size_t tap = 0; tap < tap_count; ++tap) {
        const double position = static_cast<double>(tap + 1U) /
                                static_cast<double>(tap_count);
        double amplitude = 1.0;
        switch (parameters_.amplitude_shape) {
        case MegatapAmplitudeShape::constant:
            amplitude = 1.0;
            break;
        case MegatapAmplitudeShape::increasing:
            amplitude = std::pow(position,
                static_cast<double>(parameters_.amplitude_alpha));
            break;
        case MegatapAmplitudeShape::decreasing:
            amplitude = std::pow(1.0 - position,
                static_cast<double>(parameters_.amplitude_alpha));
            break;
        case MegatapAmplitudeShape::sine:
            amplitude = std::pow(std::sin(kPi * position),
                static_cast<double>(parameters_.amplitude_alpha));
            break;
        case MegatapAmplitudeShape::alternating:
            amplitude = (tap & 1U) == 0U ? 1.0 : 0.65;
            break;
        }
        if (tap_count == 1U &&
            parameters_.amplitude_shape == MegatapAmplitudeShape::decreasing) {
            amplitude = 1.0;
        }
        if (parameters_.amplitude_randomize_percent > 0.0F) {
            const double random_scale = 1.0 +
                (static_cast<double>(random_unit(random_state)) * 2.0 - 1.0) *
                static_cast<double>(parameters_.amplitude_randomize_percent) / 100.0;
            amplitude *= random_scale;
        }
        tap_gains_[tap] = static_cast<float>(std::clamp(amplitude, 0.0, 4.0));

        double pan = 0.5;
        if (parameters_.pan_shape == MegatapPanShape::constant) {
            pan = static_cast<double>(parameters_.pan_alpha_percent) / 100.0;
        } else {
            double shaped_position = position;
            switch (parameters_.pan_shape) {
            case MegatapPanShape::constant:
                break;
            case MegatapPanShape::increasing:
                shaped_position = position;
                break;
            case MegatapPanShape::decreasing:
                shaped_position = 1.0 - position;
                break;
            case MegatapPanShape::sine:
                shaped_position = 0.5 +
                    0.5 * std::sin(2.0 * kPi * position);
                break;
            }
            const double shape_depth =
                static_cast<double>(parameters_.pan_alpha_percent) / 100.0;
            pan = 0.5 + (shaped_position - 0.5) * 2.0 * shape_depth;
        }
        const double spread_scale = 1.0 +
            static_cast<double>(parameters_.stereo_spread_percent) / 100.0;
        pan = std::clamp(0.5 + (pan - 0.5) * spread_scale, 0.0, 1.0);
        tap_pans_[tap] = static_cast<float>(pan);
        const double angle = pan * kPi / 2.0;
        tap_left_gains_[tap] = static_cast<float>(std::cos(angle));
        tap_right_gains_[tap] = static_cast<float>(std::sin(angle));
    }
    for (std::size_t tap = tap_count; tap < megatap_delay_max_taps; ++tap) {
        tap_gains_[tap] = 0.0F;
        tap_pans_[tap] = 0.5F;
        tap_left_gains_[tap] = 0.70710678F;
        tap_right_gains_[tap] = 0.70710678F;
    }
}

void MegatapDelay::rebuild_diffuser() noexcept {
    for (std::size_t stage = 0; stage < kDiffusionStages; ++stage) {
        const float milliseconds = parameters_.diffusion_time_ms *
                                   kDiffusionDelayRatios[stage];
        const double frames = std::round(static_cast<double>(sample_rate_) *
                                         static_cast<double>(milliseconds) /
                                         1000.0);
        diffusion_delay_samples_[stage] = static_cast<std::size_t>(std::clamp(
            frames, 1.0, static_cast<double>(kDiffusionLineSamples - 4U)));
    }
}

float MegatapDelay::read_delay(std::size_t delay_samples) const noexcept {
    const std::size_t offset = std::min(delay_samples, capacity_frames_ - 1U);
    const std::size_t read_frame =
        (write_frame_ + capacity_frames_ - offset) % capacity_frames_;
    return storage_[read_frame];
}

float MegatapDelay::diffuse(float input) noexcept {
    if (parameters_.diffusion_mix <= 0.0F &&
        diffusion_mod_depth_samples_ <= 0.0F) {
        return input;
    }

    float value = input;
    for (std::size_t stage = 0; stage < kDiffusionStages; ++stage) {
        const float stage_phase = diffusion_lfo_phase_ +
            static_cast<float>(stage) * 0.5F * static_cast<float>(kPi);
        const float modulation = diffusion_mod_depth_samples_ > 0.0F
            ? std::sin(stage_phase) * diffusion_mod_depth_samples_
            : 0.0F;
        const float delay = std::clamp(
            static_cast<float>(diffusion_delay_samples_[stage]) + modulation,
            1.0F, static_cast<float>(kDiffusionLineSamples - 3U));
        const std::size_t write = diffusion_write_frames_[stage];
        float read_position = static_cast<float>(write) - delay;
        while (read_position < 0.0F) {
            read_position += static_cast<float>(kDiffusionLineSamples);
        }
        while (read_position >= static_cast<float>(kDiffusionLineSamples)) {
            read_position -= static_cast<float>(kDiffusionLineSamples);
        }
        const std::size_t first = static_cast<std::size_t>(read_position);
        const std::size_t second = (first + 1U) % kDiffusionLineSamples;
        const float fraction = read_position - static_cast<float>(first);
        const float delayed = diffusion_lines_[stage][first] * (1.0F - fraction) +
                              diffusion_lines_[stage][second] * fraction;
        const float output = delayed - kAllpassCoefficient * value;
        diffusion_lines_[stage][write] = safe_signal(
            value + kAllpassCoefficient * output);
        diffusion_write_frames_[stage] =
            (write + 1U) % kDiffusionLineSamples;
        value = safe_signal(output);
    }
    diffusion_lfo_phase_ += diffusion_lfo_phase_step_;
    if (diffusion_lfo_phase_ >= 2.0F * static_cast<float>(kPi)) {
        diffusion_lfo_phase_ -= 2.0F * static_cast<float>(kPi);
    }
    const float mix = parameters_.diffusion_mix;
    return safe_signal(input * (1.0F - mix) + value * mix);
}

float MegatapDelay::filter_tap(float input, std::size_t tap) noexcept {
    float value = input;
    if (low_cut_coefficient_ > 0.0F) {
        const float high_pass = low_cut_alpha_ *
            (low_cut_output_[tap] + value - low_cut_input_[tap]);
        low_cut_input_[tap] = value;
        low_cut_output_[tap] = high_pass;
        value = high_pass;
    }
    if (high_cut_coefficient_ > 0.0F) {
        high_cut_output_[tap] += high_cut_coefficient_ *
            (value - high_cut_output_[tap]);
        value = high_cut_output_[tap];
    }
    return std::isfinite(value) ? value : 0.0F;
}

float MegatapDelay::envelope_gain(float input) noexcept {
    const float magnitude = std::abs(input);
    const float coefficient = magnitude > envelope_
        ? envelope_attack_coefficient_ : envelope_release_coefficient_;
    envelope_ += (1.0F - coefficient) * (magnitude - envelope_);
    if (!std::isfinite(envelope_)) envelope_ = 0.0F;
    const float top = std::max(0.25F, envelope_threshold_ * 4.0F);
    return std::clamp((envelope_ - envelope_threshold_) /
                      std::max(1.0e-4F, top - envelope_threshold_),
                      0.0F, 1.0F);
}

float MegatapDelay::safe_signal(float value) noexcept {
    if (!std::isfinite(value)) return 0.0F;
    return std::clamp(value, -kMaximumSignal, kMaximumSignal);
}

void MegatapDelay::process(std::span<float> interleaved,
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
        float selected_input = dry_left;
        if (channels == 2U) {
            switch (parameters_.input_select) {
            case MegatapInputSelect::sum:
                selected_input = 0.5F * (dry_left + dry_right);
                break;
            case MegatapInputSelect::left:
                selected_input = dry_left;
                break;
            case MegatapInputSelect::right:
                selected_input = dry_right;
                break;
            }
        }
        float input = safe_signal(selected_input * input_gain_linear_);
        if (parameters_.envelope_enabled) {
            input *= envelope_gain(input);
        } else {
            (void)envelope_gain(input);
        }
        const float delay_input = diffuse(safe_signal(input));

        float wet_left = 0.0F;
        float wet_right = 0.0F;
        float wet_mono = 0.0F;
        float feedback_source = 0.0F;
        for (std::size_t tap = 0; tap < parameters_.tap_count; ++tap) {
            const float filtered = filter_tap(read_delay(tap_delay_samples_[tap]), tap);
            const float weighted = filtered * tap_gains_[tap];
            if (channels == 1U) {
                wet_mono += weighted;
            } else {
                wet_left += weighted * tap_left_gains_[tap];
                wet_right += weighted * tap_right_gains_[tap];
            }
            if (tap + 1U == parameters_.feedback_tap) {
                feedback_source = weighted;
            }
        }

        storage_[write_frame_] = safe_signal(
            delay_input + parameters_.feedback * feedback_source);
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

void MegatapDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    for (auto& line : diffusion_lines_) {
        line.fill(0.0F);
    }
    low_cut_input_.fill(0.0F);
    low_cut_output_.fill(0.0F);
    high_cut_output_.fill(0.0F);
    diffusion_write_frames_.fill(0U);
    write_frame_ = 0U;
    diffusion_lfo_phase_ = 0.0F;
    envelope_ = 0.0F;
}

} // namespace tabletfx
