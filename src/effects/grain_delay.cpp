#include "tabletfx/effects/grain_delay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr double kMaximumHistorySeconds =
    2.0 + 0.1 + 0.25 * (1.0 - 0.25);
constexpr std::size_t kRingGuardFrames = 8;

} // namespace

GrainDelay::GrainDelay(float sample_rate, std::uint16_t channels,
                       GrainDelayParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
        sample_rate_ > 384000.0F || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument(
            "Grain Delay requires an 8–384 kHz sample rate and mono/stereo audio");
    }

    const double maximum_frames = std::ceil(
        static_cast<double>(sample_rate_) * kMaximumHistorySeconds);
    const std::size_t sample_capacity_limit = storage_.max_size() / channels_;
    if (!std::isfinite(maximum_frames) || maximum_frames < 1.0 ||
        sample_capacity_limit <= kRingGuardFrames ||
        maximum_frames > static_cast<double>(sample_capacity_limit -
                                              kRingGuardFrames)) {
        throw std::length_error("Grain Delay buffer size is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(maximum_frames) +
                       kRingGuardFrames;
    storage_.assign(capacity_frames_ * channels_, 0.0F);
    window_table_.resize(kWindowTableSize);
    for (std::size_t index = 0; index < window_table_.size(); ++index) {
        const double phase = static_cast<double>(index) /
            static_cast<double>(window_table_.size() - 1U);
        window_table_[index] = static_cast<float>(
            0.5 - 0.5 * std::cos(2.0 * static_cast<double>(kPi) * phase));
    }

    configure(parameters);
    reset();
}

void GrainDelay::configure(GrainDelayParameters parameters) noexcept {
    parameters.time_ms = bounded(parameters.time_ms, 350.0F,
                                 kMinimumTimeMs, kMaximumTimeMs);
    parameters.feedback = bounded(parameters.feedback, 0.25F, 0.0F,
                                  kMaximumFeedback);
    parameters.mix = bounded(parameters.mix, 0.25F, 0.0F, 1.0F);
    parameters.grain_size_ms = bounded(
        parameters.grain_size_ms, 80.0F, kMinimumGrainSizeMs,
        kMaximumGrainSizeMs);
    parameters.overlap = std::clamp<std::uint8_t>(
        parameters.overlap, 1U, static_cast<std::uint8_t>(kMaximumGrains));
    parameters.pitch_semitones = bounded(
        parameters.pitch_semitones, 0.0F, -kMaximumPitchSemitones,
        kMaximumPitchSemitones);
    parameters.spray_ms = bounded(parameters.spray_ms, 0.0F, 0.0F,
                                  kMaximumSprayMs);

    parameters_ = parameters;
    delay_samples_ = static_cast<float>(samples_for_ms(parameters_.time_ms));
    spray_samples_ = static_cast<float>(samples_for_ms(parameters_.spray_ms));
    if (parameters_.spray_ms == 0.0F) spray_samples_ = 0.0F;
    configured_grain_length_ = std::max<std::size_t>(
        1U, samples_for_ms(parameters_.grain_size_ms));
    pitch_step_ = std::exp2(parameters_.pitch_semitones / 12.0F);
    effective_grain_length_ = grain_length_for(pitch_step_);
    overlap_normalization_ = std::max(
        1.0F, static_cast<float>(parameters_.overlap) * 0.5F);
    launch_interval_ = std::max<std::size_t>(
        1U, effective_grain_length_ / parameters_.overlap);
    launch_countdown_ = std::min(launch_countdown_, launch_interval_);
}

void GrainDelay::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (launch_countdown_ == 0U) {
            launch_grain();
            launch_countdown_ = launch_interval_;
        }

        std::array<double, 2> wet_sums{};
        for (auto& grain : grains_) {
            if (!grain.active) continue;

            const float window = window_value(grain.age, grain.length);
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                wet_sums[channel] +=
                    static_cast<double>(read_ring(grain.read_position, channel)) *
                    static_cast<double>(window);
            }

            ++grain.age;
            if (grain.age >= grain.length) {
                grain.active = false;
            } else {
                grain.read_position += grain.source_step;
                if (grain.read_position >= static_cast<float>(capacity_frames_)) {
                    grain.read_position -= static_cast<float>(capacity_frames_);
                }
            }
        }

        const std::size_t write_offset = write_frame_ * channels_;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t sample_index = frame * channels_ + channel;
            const float raw_input = interleaved[sample_index];
            const float input = std::isfinite(raw_input)
                ? std::clamp(raw_input, -kMaximumInputMagnitude,
                             kMaximumInputMagnitude)
                : 0.0F;
            const float wet = bounded_sample(
                wet_sums[channel] /
                static_cast<double>(overlap_normalization_));
            const double dry_value = std::isfinite(raw_input) ? raw_input : 0.0;
            const double output =
                dry_value * static_cast<double>(1.0F - parameters_.mix) +
                static_cast<double>(wet) * static_cast<double>(parameters_.mix);
            interleaved[sample_index] = bounded_sample(output);

            const double recorded = static_cast<double>(input) +
                static_cast<double>(wet) *
                    static_cast<double>(parameters_.feedback);
            storage_[write_offset + channel] = bounded_sample(std::clamp(
                recorded, -static_cast<double>(kMaximumStateMagnitude),
                static_cast<double>(kMaximumStateMagnitude)));
        }

        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
        if (launch_countdown_ > 0U) --launch_countdown_;
    }
}

void GrainDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    grains_.fill(Grain{});
    write_frame_ = 0;
    next_grain_ = 0;
    launch_countdown_ = 0;
    random_state_ = 0x6d2b79f5U;
}

float GrainDelay::bounded(float value, float fallback, float minimum,
                          float maximum) noexcept {
    if (!std::isfinite(value)) return fallback;
    return std::clamp(value, minimum, maximum);
}

float GrainDelay::random_unit(std::uint32_t& state) noexcept {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return static_cast<float>(state >> 8U) / 16777215.0F;
}

float GrainDelay::bounded_sample(double value) noexcept {
    if (!std::isfinite(value)) return 0.0F;
    return static_cast<float>(std::clamp(
        value, -static_cast<double>(kMaximumStateMagnitude),
        static_cast<double>(kMaximumStateMagnitude)));
}

std::size_t GrainDelay::samples_for_ms(float milliseconds) const noexcept {
    const double samples = std::round(
        static_cast<double>(sample_rate_) *
        static_cast<double>(milliseconds) / 1000.0);
    return static_cast<std::size_t>(std::max(0.0, samples));
}

std::size_t GrainDelay::grain_length_for(float source_step) const noexcept {
    if (source_step <= 1.0F) return configured_grain_length_;

    // The read head approaches the live write head at source_step - 1 samples
    // per output frame. Keep at least one sample of separation at the final
    // grain frame. The launch's random spray only increases this separation.
    const double available_age = std::max(
        1.0, static_cast<double>(delay_samples_) - 1.0);
    const double relative_rate = static_cast<double>(source_step) - 1.0;
    const double maximum_length =
        std::floor(available_age / relative_rate) + 1.0;
    const std::size_t safe_length = static_cast<std::size_t>(
        std::max(1.0, maximum_length));
    return std::min(configured_grain_length_, safe_length);
}

float GrainDelay::read_ring(float position,
                            std::uint16_t channel) const noexcept {
    if (!std::isfinite(position) || channel >= channels_) return 0.0F;
    const float capacity = static_cast<float>(capacity_frames_);
    float wrapped = position;
    if (wrapped < 0.0F) wrapped += capacity;
    else if (wrapped >= capacity) wrapped -= capacity;
    if (wrapped < 0.0F || wrapped >= capacity) return 0.0F;
    const float integral = std::floor(wrapped);
    const std::size_t first = static_cast<std::size_t>(integral);
    const std::size_t second = (first + 1U) % capacity_frames_;
    const float fraction = wrapped - integral;
    const float a = storage_[first * channels_ + channel];
    const float b = storage_[second * channels_ + channel];
    return a + (b - a) * fraction;
}

float GrainDelay::window_value(std::size_t age,
                               std::size_t length) const noexcept {
    if (length <= 1U) return 1.0F;
    const double phase = static_cast<double>(age) /
                         static_cast<double>(length - 1U);
    const double table_position = phase *
        static_cast<double>(window_table_.size() - 1U);
    const std::size_t first = static_cast<std::size_t>(table_position);
    if (first + 1U >= window_table_.size()) return window_table_.back();
    const float fraction = static_cast<float>(
        table_position - static_cast<double>(first));
    const float a = window_table_[first];
    const float b = window_table_[first + 1U];
    return a + (b - a) * fraction;
}

void GrainDelay::launch_grain() noexcept {
    const std::size_t index = next_grain_;
    next_grain_ = (next_grain_ + 1U) % kMaximumGrains;

    const float random_offset = spray_samples_ > 0.0F
        ? random_unit(random_state_) * spray_samples_
        : 0.0F;
    const std::size_t length = effective_grain_length_;
    float position = static_cast<float>(write_frame_) - delay_samples_ -
                     random_offset;
    if (position < 0.0F) position += static_cast<float>(capacity_frames_);

    grains_[index] = Grain{position, pitch_step_, 0U, length, true};
}

} // namespace tabletfx
