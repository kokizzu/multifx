#include "tabletfx/effects/step_filter_delay.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kDefaultTempoBpm = 120.0F;
constexpr float kDefaultRateHz = 8.0F;
constexpr float kPi = 3.14159265358979323846F;

} // namespace

StepFilterDelay::StepFilterDelay(float sample_rate, std::uint16_t channels,
                                 float max_delay_ms,
                                 StepFilterDelayParameters parameters)
    : sample_rate_(sample_rate), channels_(channels),
      max_delay_ms_(max_delay_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2 ||
        !std::isfinite(max_delay_ms_) || max_delay_ms_ < kMinimumDelayMs ||
        max_delay_ms_ > kMaximumDelayMs) {
        throw std::invalid_argument(
            "Step Filter Delay requires 8-384 kHz, mono/stereo, and 1-2000 ms capacity");
    }

    const double maximum_frames = std::ceil(
        static_cast<double>(sample_rate_) *
        static_cast<double>(max_delay_ms_) / 1000.0);
    const std::size_t maximum_size = delay_buffer_.max_size() / channels_;
    if (!std::isfinite(maximum_frames) || maximum_frames < 1.0 ||
        maximum_frames >= static_cast<double>(maximum_size)) {
        throw std::length_error("Step Filter Delay buffer is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(maximum_frames) + 1U;
    delay_buffer_.assign(capacity_frames_ * channels_, 0.0F);
    configure(parameters);
    reset();
}

void StepFilterDelay::configure(StepFilterDelayParameters parameters) noexcept {
    const float maximum_time = std::min(kMaximumDelayMs, max_delay_ms_);
    parameters.delay_time_ms = bounded(parameters.delay_time_ms, 350.0F,
                                       kMinimumDelayMs, maximum_time);
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, kDefaultTempoBpm,
                                   kMinimumBpm, kMaximumBpm);
    parameters.delay_division = sanitize_division(parameters.delay_division);
    parameters.step_division = sanitize_division(parameters.step_division);
    parameters.step_rate_hz = bounded(parameters.step_rate_hz, kDefaultRateHz,
                                      kMinimumRateHz, kMaximumRateHz);
    parameters.feedback = bounded(parameters.feedback, 0.35F, 0.0F, 0.95F);
    parameters.mix = bounded(parameters.mix, 0.25F, 0.0F, 1.0F);
    parameters.filter_mode = sanitize_mode(parameters.filter_mode);

    const float cutoff_limit = std::min(kMaximumCutoffHz, sample_rate_ * 0.40F);
    parameters.minimum_cutoff_hz = bounded(parameters.minimum_cutoff_hz, 120.0F,
                                            kMinimumCutoffHz, cutoff_limit);
    parameters.maximum_cutoff_hz = bounded(parameters.maximum_cutoff_hz, 6000.0F,
                                            parameters.minimum_cutoff_hz,
                                            cutoff_limit);
    if (parameters.maximum_cutoff_hz < parameters.minimum_cutoff_hz) {
        parameters.maximum_cutoff_hz = parameters.minimum_cutoff_hz;
    }
    parameters.resonance_q = bounded(parameters.resonance_q, 0.707F,
                                     kMinimumQ, kMaximumQ);
    parameters.depth = bounded(parameters.depth, 1.0F, 0.0F, 1.0F);
    parameters.shape = bounded(parameters.shape, 0.15F, 0.0F, 1.0F);
    parameters.smoothing_ms = bounded(parameters.smoothing_ms, 4.0F,
                                      0.0F, kMaximumSmoothingMs);
    parameters.step_phase = bounded(parameters.step_phase, 0.0F, 0.0F,
                                    0.99999994F);
    for (float& step : parameters.steps) {
        step = bounded(step, 0.5F, 0.0F, 1.0F);
    }

    parameters_ = parameters;
    const float requested_delay_ms = parameters_.delay_tempo_sync
        ? (60000.0F / parameters_.tempo_bpm) *
              beats_per_division(parameters_.delay_division)
        : parameters_.delay_time_ms;
    effective_delay_time_ms_ = std::clamp(requested_delay_ms,
                                          kMinimumDelayMs, maximum_time);
    const double requested_frames = std::round(
        static_cast<double>(sample_rate_) *
        static_cast<double>(effective_delay_time_ms_) / 1000.0);
    delay_frames_ = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::max(1.0, requested_frames)), 1U,
        capacity_frames_ - 1U);

    prepare_step_clock();
    prepare_filter();
}

void StepFilterDelay::process(std::span<float> interleaved,
                              std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float pattern = pattern_value(phase_);
        const float target_position =
            0.5F + parameters_.depth * (pattern - 0.5F);
        smoothed_position_ +=
            (target_position - smoothed_position_) * smoothing_alpha_;
        smoothed_position_ = std::clamp(smoothed_position_, 0.0F, 1.0F);
        const float g = g_from_position(smoothed_position_);

        const std::size_t read_frame = write_frame_ >= delay_frames_
            ? write_frame_ - delay_frames_
            : write_frame_ + capacity_frames_ - delay_frames_;
        const std::size_t read_offset = read_frame * channels_;
        const std::size_t write_offset = write_frame_ * channels_;
        const std::size_t input_offset = frame * channels_;

        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = input_offset + channel;
            const float input = interleaved[index];
            const float dry = std::isfinite(input) ? input : 0.0F;
            if (!std::isfinite(input)) {
                filter_state_[channel] = {};
            }

            const float delayed = delay_buffer_[read_offset + channel];
            const float wet = filter_sample(delayed, channel, g);
            const double recorded = static_cast<double>(dry) +
                static_cast<double>(parameters_.feedback) * wet;
            const float bounded_recording = static_cast<float>(std::clamp(
                recorded, -static_cast<double>(kMaximumBufferSample),
                static_cast<double>(kMaximumBufferSample)));
            delay_buffer_[write_offset + channel] = bounded_recording;

            const double output = static_cast<double>(dry) *
                                      (1.0 - parameters_.mix) +
                                  static_cast<double>(wet) * parameters_.mix;
            interleaved[index] = std::isfinite(output)
                ? static_cast<float>(output)
                : 0.0F;
        }

        ++write_frame_;
        if (write_frame_ == capacity_frames_) {
            write_frame_ = 0;
        }
        phase_ += phase_increment_;
        if (phase_ >= 1.0F) {
            phase_ -= std::floor(phase_);
        }
    }
}

void StepFilterDelay::reset() noexcept {
    std::fill(delay_buffer_.begin(), delay_buffer_.end(), 0.0F);
    filter_state_ = {};
    write_frame_ = 0;
    phase_ = parameters_.step_phase;
    smoothed_position_ = 0.5F + parameters_.depth *
                                  (pattern_value(phase_) - 0.5F);
}

float StepFilterDelay::current_cutoff_hz() const noexcept {
    return cutoff_from_position(smoothed_position_);
}

float StepFilterDelay::bounded(float value, float fallback,
                               float minimum, float maximum) noexcept {
    return std::isfinite(value)
        ? std::clamp(value, minimum, maximum)
        : std::clamp(fallback, minimum, maximum);
}

DelayDivision StepFilterDelay::sanitize_division(
    DelayDivision division) noexcept {
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
        return division;
    }
    return DelayDivision::sixteenth;
}

StepFilterDelayMode StepFilterDelay::sanitize_mode(
    StepFilterDelayMode mode) noexcept {
    switch (mode) {
    case StepFilterDelayMode::LowPass:
    case StepFilterDelayMode::BandPass:
    case StepFilterDelayMode::HighPass:
    case StepFilterDelayMode::Notch:
        return mode;
    }
    return StepFilterDelayMode::LowPass;
}

float StepFilterDelay::beats_per_division(DelayDivision division) noexcept {
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
    return 0.25F;
}

float StepFilterDelay::pattern_value(float cycle_phase) const noexcept {
    float position = std::clamp(cycle_phase, 0.0F, 0.99999994F) * 16.0F;
    const float nearby_step = std::round(position);
    if (std::abs(position - nearby_step) < 1.0e-5F) {
        position = nearby_step;
    }
    const auto index = static_cast<std::size_t>(position);
    const std::size_t current_index =
        std::min(index, parameters_.steps.size() - 1U);
    const float fraction = position - static_cast<float>(current_index);
    const float current = parameters_.steps[current_index];
    if (parameters_.shape <= 0.0F ||
        fraction <= 1.0F - parameters_.shape) {
        return current;
    }

    const std::size_t next_index =
        (current_index + 1U) % parameters_.steps.size();
    const float transition = std::clamp(
        (fraction - (1.0F - parameters_.shape)) / parameters_.shape,
        0.0F, 1.0F);
    const float eased = transition * transition * (3.0F - 2.0F * transition);
    return current + (parameters_.steps[next_index] - current) * eased;
}

float StepFilterDelay::cutoff_from_position(float position) const noexcept {
    const float log_min = std::log(parameters_.minimum_cutoff_hz);
    const float log_max = std::log(parameters_.maximum_cutoff_hz);
    return std::exp(log_min + std::clamp(position, 0.0F, 1.0F) *
                                 (log_max - log_min));
}

float StepFilterDelay::g_from_position(float position) const noexcept {
    const float table_position = std::clamp(position, 0.0F, 1.0F) *
                                 static_cast<float>(kCoefficientSteps);
    const auto index = static_cast<std::size_t>(table_position);
    if (index >= kCoefficientSteps) {
        return g_table_[kCoefficientSteps];
    }
    const float fraction = table_position - static_cast<float>(index);
    const float lower = g_table_[index];
    const float upper = g_table_[index + 1U];
    return lower + (upper - lower) * fraction;
}

float StepFilterDelay::filter_sample(float input, std::uint16_t channel,
                                     float g) noexcept {
    auto& state = filter_state_[channel];
    const float denominator = 1.0F + g * (g + resonance_k_);
    const float inverse = 1.0F / denominator;
    const float v3 = input - state.integrator_2;
    const float band = (state.integrator_1 + g * v3) * inverse;
    const float low = state.integrator_2 + g * band;
    state.integrator_1 = 2.0F * band - state.integrator_1;
    state.integrator_2 = 2.0F * low - state.integrator_2;

    if (!std::isfinite(state.integrator_1) ||
        !std::isfinite(state.integrator_2) ||
        std::abs(state.integrator_1) > kMaximumFilterState ||
        std::abs(state.integrator_2) > kMaximumFilterState) {
        state = {};
        return 0.0F;
    }

    const float high = input - resonance_k_ * band - low;
    switch (parameters_.filter_mode) {
    case StepFilterDelayMode::LowPass: return low;
    case StepFilterDelayMode::BandPass: return band;
    case StepFilterDelayMode::HighPass: return high;
    case StepFilterDelayMode::Notch: return low + high;
    }
    return low;
}

void StepFilterDelay::prepare_step_clock() noexcept {
    const float rate = parameters_.step_tempo_sync
        ? parameters_.tempo_bpm /
              (60.0F * beats_per_division(parameters_.step_division))
        : parameters_.step_rate_hz;
    effective_step_rate_hz_ =
        std::clamp(rate, kMinimumRateHz, kMaximumRateHz);
    phase_increment_ = effective_step_rate_hz_ / (sample_rate_ * 16.0F);
}

void StepFilterDelay::prepare_filter() noexcept {
    resonance_k_ = 1.0F / parameters_.resonance_q;
    const float smoothing_seconds = parameters_.smoothing_ms * 0.001F;
    smoothing_alpha_ = smoothing_seconds <= 0.0F
        ? 1.0F
        : static_cast<float>(1.0 - std::exp(
              -1.0 / (static_cast<double>(smoothing_seconds) * sample_rate_)));

    const float log_min = std::log(parameters_.minimum_cutoff_hz);
    const float log_max = std::log(parameters_.maximum_cutoff_hz);
    for (std::size_t index = 0; index <= kCoefficientSteps; ++index) {
        const float position = static_cast<float>(index) /
                               static_cast<float>(kCoefficientSteps);
        const float cutoff = std::exp(log_min + position * (log_max - log_min));
        g_table_[index] = std::tan(kPi * (cutoff / sample_rate_));
    }
}

} // namespace tabletfx
