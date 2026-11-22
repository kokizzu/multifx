#include "tabletfx/effects/shuffling_looper.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace tabletfx::effects {

ShufflingLooper::ShufflingLooper(const float sample_rate,
                                 const std::uint16_t channels,
                                 const std::uint32_t seed)
    : sample_rate_(sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate &&
                           std::isfinite(sample_rate)
                       ? sample_rate
                       : kDefaultSampleRate),
      channels_(validate_channels(channels)),
      samples_(std::make_unique<float[]>(kMaximumFrames * channels_)),
      seed_(seed == 0U ? 1U : seed),
      rng_state_(seed_) {}

std::string_view ShufflingLooper::id() const noexcept {
    return "shuffling-looper";
}

std::size_t ShufflingLooper::memory_bytes() const noexcept {
    return sizeof(*this) + kMaximumFrames *
                               static_cast<std::size_t>(channels_) * sizeof(float);
}

std::uint8_t ShufflingLooper::sequence_slice(const std::size_t step) const noexcept {
    if (step >= sequence_step_count_) return std::numeric_limits<std::uint8_t>::max();
    return sequence_[step].slice;
}

bool ShufflingLooper::record() noexcept {
    if (state_ == ShufflingLooperState::Recording) return false;
    // Destructive replacement: old samples become unreachable immediately.
    loop_frames_ = 0U;
    sequence_step_count_ = 0U;
    sequence_step_ = 0U;
    step_frame_ = 0U;
    state_ = ShufflingLooperState::Recording;
    lowpass_state_.fill(0.0F);
    highpass_input_.fill(0.0F);
    highpass_output_.fill(0.0F);
    return true;
}

bool ShufflingLooper::stop_recording() noexcept {
    if (state_ != ShufflingLooperState::Recording) return false;
    finish_recording();
    return state_ == ShufflingLooperState::Playing;
}

bool ShufflingLooper::play() noexcept {
    if (loop_frames_ == 0U || sequence_step_count_ == 0U ||
        state_ == ShufflingLooperState::Recording) {
        return false;
    }
    sequence_step_ = 0U;
    step_frame_ = 0U;
    state_ = ShufflingLooperState::Playing;
    lowpass_state_.fill(0.0F);
    highpass_input_.fill(0.0F);
    highpass_output_.fill(0.0F);
    return true;
}

void ShufflingLooper::stop() noexcept {
    if (state_ != ShufflingLooperState::Recording) {
        state_ = ShufflingLooperState::Stopped;
    }
}

bool ShufflingLooper::randomize_sequence() noexcept {
    if (loop_frames_ == 0U) return false;
    rebuild_sequence(true);
    return sequence_step_count_ != 0U;
}

bool ShufflingLooper::clear() noexcept {
    const bool had_loop = loop_frames_ != 0U;
    state_ = ShufflingLooperState::Stopped;
    loop_frames_ = 0U;
    sequence_step_count_ = 0U;
    sequence_step_ = 0U;
    step_frame_ = 0U;
    lowpass_state_.fill(0.0F);
    highpass_input_.fill(0.0F);
    highpass_output_.fill(0.0F);
    return had_loop;
}

void ShufflingLooper::configure(
    const ShufflingLooperParameters& parameters) noexcept {
    ShufflingLooperParameters sanitized = parameters;
    sanitized.slices = std::clamp<std::uint8_t>(
        sanitized.slices, 2U, static_cast<std::uint8_t>(kMaximumSlices));
    sanitized.sequence_length = std::clamp<std::uint8_t>(
        sanitized.sequence_length, 1U,
        static_cast<std::uint8_t>(kMaximumSequenceSteps));
    sanitized.shuffle = bounded(sanitized.shuffle, parameters_.shuffle, 0.0F, 1.0F);
    sanitized.pitch_probability = bounded(
        sanitized.pitch_probability, parameters_.pitch_probability, 0.0F, 1.0F);
    sanitized.reverse_probability = bounded(
        sanitized.reverse_probability, parameters_.reverse_probability, 0.0F, 1.0F);
    sanitized.repeat_probability = bounded(
        sanitized.repeat_probability, parameters_.repeat_probability, 0.0F, 1.0F);
    sanitized.smoothing = bounded(sanitized.smoothing, parameters_.smoothing,
                                  0.0F, 1.0F);
    sanitized.sequence_drift = bounded(
        sanitized.sequence_drift, parameters_.sequence_drift, 0.0F, 1.0F);
    sanitized.playback = bounded(sanitized.playback, parameters_.playback,
                                 0.0F, 1.0F);
    sanitized.low_cut_hz = bounded(sanitized.low_cut_hz,
                                   parameters_.low_cut_hz, 0.0F,
                                   kMaximumCutHz);
    sanitized.high_cut_hz = bounded(sanitized.high_cut_hz,
                                    parameters_.high_cut_hz, 0.0F,
                                    kMaximumCutHz);
    if (sanitized.low_cut_hz > 0.0F) {
        sanitized.low_cut_hz = std::max(sanitized.low_cut_hz, kMinimumCutHz);
    }
    if (sanitized.high_cut_hz > 0.0F) {
        sanitized.high_cut_hz = std::max(sanitized.high_cut_hz, kMinimumCutHz);
    }
    sanitized.interval1_semitones = std::clamp<std::int8_t>(
        sanitized.interval1_semitones, -12, 12);
    sanitized.interval2_semitones = std::clamp<std::int8_t>(
        sanitized.interval2_semitones, -12, 12);

    parameters_ = sanitized;
    if (loop_frames_ != 0U) rebuild_sequence(false);
}

void ShufflingLooper::set_seed(const std::uint32_t seed) noexcept {
    seed_ = seed == 0U ? 1U : seed;
    rng_state_ = seed_;
    if (loop_frames_ != 0U) rebuild_sequence(false);
}

void ShufflingLooper::reset() noexcept {
    state_ = ShufflingLooperState::Stopped;
    loop_frames_ = 0U;
    sequence_step_count_ = 0U;
    sequence_step_ = 0U;
    step_frame_ = 0U;
    rng_state_ = seed_;
    lowpass_state_.fill(0.0F);
    highpass_input_.fill(0.0F);
    highpass_output_.fill(0.0F);
}

void ShufflingLooper::process(std::span<float> interleaved,
                              const std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U ||
        interleaved.size() % channels != 0U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const std::size_t offset = frame * channels_;
        if (state_ == ShufflingLooperState::Recording) {
            if (loop_frames_ >= kMaximumFrames) {
                finish_recording();
            } else {
                const std::size_t destination = loop_frames_ * channels_;
                for (std::size_t channel = 0U; channel < channels_; ++channel) {
                    const std::size_t index = offset + channel;
                    const float input = std::isfinite(interleaved[index])
                                            ? interleaved[index]
                                            : 0.0F;
                    if (!std::isfinite(interleaved[index])) interleaved[index] = 0.0F;
                    samples_[destination + channel] =
                        std::clamp(input, -kSampleCeiling, kSampleCeiling);
                }
                ++loop_frames_;
                if (loop_frames_ == kMaximumFrames) finish_recording();
                continue;
            }
        }

        if (state_ != ShufflingLooperState::Playing ||
            sequence_step_count_ == 0U) {
            continue;
        }

        const SequenceStep& step = sequence_[sequence_step_];
        const float envelope = smoothing_gain(step, step_frame_);
        for (std::size_t channel = 0U; channel < channels_; ++channel) {
            const std::size_t index = offset + channel;
            const float dry = std::isfinite(interleaved[index])
                                  ? interleaved[index]
                                  : 0.0F;
            const float loop_sample = read_slice_sample(step, step_frame_, channel);
            const float filtered = filter_playback(loop_sample, channel);
            interleaved[index] = dry + filtered * parameters_.playback * envelope;
        }
        ++step_frame_;
        if (step_frame_ >= step.output_frames) advance_step();
    }
}

std::uint16_t ShufflingLooper::validate_channels(const std::uint16_t channels) {
    if (channels < kMinimumChannels || channels > kMaximumChannels) {
        throw std::invalid_argument("ShufflingLooper supports mono or stereo only");
    }
    return channels;
}

float ShufflingLooper::bounded(const float value, const float fallback,
                               const float minimum,
                               const float maximum) noexcept {
    if (!std::isfinite(value)) return fallback;
    return std::clamp(value, minimum, maximum);
}

float ShufflingLooper::random_unit() noexcept {
    // Xorshift32 is small, deterministic, and has no dynamic state.
    rng_state_ ^= rng_state_ << 13U;
    rng_state_ ^= rng_state_ >> 17U;
    rng_state_ ^= rng_state_ << 5U;
    if (rng_state_ == 0U) rng_state_ = 1U;
    return static_cast<float>(rng_state_ >> 8U) * (1.0F / 16777216.0F);
}

std::uint32_t ShufflingLooper::random_bounded(const std::uint32_t limit) noexcept {
    if (limit == 0U) return 0U;
    return static_cast<std::uint32_t>(random_unit() * static_cast<float>(limit));
}

bool ShufflingLooper::chance(const float probability) noexcept {
    if (probability <= 0.0F) return false;
    if (probability >= 1.0F) return true;
    return random_unit() < probability;
}

void ShufflingLooper::rebuild_sequence(const bool force_shuffle) noexcept {
    if (loop_frames_ < 2U) {
        sequence_step_count_ = 0U;
        sequence_step_ = 0U;
        step_frame_ = 0U;
        return;
    }

    const std::size_t slice_count = std::min(effective_slice_count(), kMaximumSlices);
    sequence_step_count_ = std::min<std::size_t>(
        parameters_.sequence_length, kMaximumSequenceSteps);
    std::array<std::uint8_t, kMaximumSlices> order{};
    const std::size_t order_count = slice_count;
    for (std::size_t slice = 0U; slice < order_count; ++slice) {
        order[slice] = static_cast<std::uint8_t>(slice);
    }
    for (std::size_t remaining = order_count; remaining > 1U; --remaining) {
        const std::size_t selected = random_bounded(
            static_cast<std::uint32_t>(remaining));
        std::swap(order[remaining - 1U], order[selected]);
    }

    for (std::size_t index = 0U; index < sequence_step_count_; ++index) {
        const bool repeat = index > 0U && chance(parameters_.repeat_probability);
        std::uint8_t slice = 0U;
        if (repeat) {
            slice = sequence_[index - 1U].slice;
        } else if (force_shuffle || chance(parameters_.shuffle)) {
            slice = order[index % slice_count];
        } else {
            slice = static_cast<std::uint8_t>(index % slice_count);
        }
        set_sequence_step(index, slice);
    }
    sequence_step_ = 0U;
    step_frame_ = 0U;
}

void ShufflingLooper::set_sequence_step(const std::size_t index,
                                        const std::uint8_t slice) noexcept {
    SequenceStep step{};
    step.slice = slice;
    if (chance(parameters_.pitch_probability)) {
        step.semitones = random_unit() < 0.5F
                             ? parameters_.interval1_semitones
                             : parameters_.interval2_semitones;
    }
    step.reverse = chance(parameters_.reverse_probability);

    const std::size_t slice_count = effective_slice_count();
    const std::size_t bounded_slice = std::min<std::size_t>(slice, slice_count - 1U);
    const std::size_t start = bounded_slice * loop_frames_ / slice_count;
    const std::size_t end = (bounded_slice + 1U) * loop_frames_ / slice_count;
    step.source_start = start;
    step.source_frames = std::max<std::size_t>(1U, end - start);
    step.source_step = std::exp2(static_cast<double>(step.semitones) / 12.0);
    const double output_frames =
        static_cast<double>(step.source_frames) / step.source_step;
    step.output_frames = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(output_frames)));
    sequence_[index] = step;
}

void ShufflingLooper::finish_recording() noexcept {
    if (loop_frames_ < 2U) {
        state_ = ShufflingLooperState::Stopped;
        loop_frames_ = 0U;
        sequence_step_count_ = 0U;
        sequence_step_ = 0U;
        step_frame_ = 0U;
        return;
    }
    rebuild_sequence(false);
    state_ = sequence_step_count_ == 0U ? ShufflingLooperState::Stopped
                                        : ShufflingLooperState::Playing;
}

void ShufflingLooper::advance_step() noexcept {
    step_frame_ = 0U;
    ++sequence_step_;
    if (sequence_step_ >= sequence_step_count_) {
        sequence_step_ = 0U;
        if (chance(parameters_.sequence_drift)) {
            rebuild_sequence(true);
            return;
        }
    }

    // Shuffle is applied at each sequence transition. Seq Drift is a separate
    // whole-sequence regeneration at the sequence boundary.
    if (chance(parameters_.shuffle)) {
        const std::size_t slice_count = effective_slice_count();
        std::uint8_t slice = static_cast<std::uint8_t>(
            random_bounded(static_cast<std::uint32_t>(slice_count)));
        if (sequence_step_ > 0U && chance(parameters_.repeat_probability)) {
            slice = sequence_[sequence_step_ - 1U].slice;
        } else if (sequence_step_ == 0U && chance(parameters_.repeat_probability)) {
            slice = sequence_[sequence_step_count_ - 1U].slice;
        }
        set_sequence_step(sequence_step_, slice);
    }
}

float ShufflingLooper::read_slice_sample(const SequenceStep& step,
                                         const std::size_t output_frame,
                                         const std::size_t channel) const noexcept {
    if (step.source_frames == 0U || loop_frames_ == 0U) return 0.0F;
    const double position = std::min(
        static_cast<double>(output_frame) * step.source_step,
        static_cast<double>(step.source_frames - 1U));
    const std::size_t base = static_cast<std::size_t>(position);
    const double fraction = position - static_cast<double>(base);
    std::size_t first_frame = 0U;
    std::size_t second_frame = 0U;
    if (!step.reverse) {
        first_frame = step.source_start + base;
        second_frame = step.source_start +
                       (base + 1U < step.source_frames ? base + 1U : base);
    } else {
        first_frame = step.source_start + step.source_frames - 1U - base;
        second_frame = base + 1U < step.source_frames
                           ? first_frame - 1U
                           : first_frame;
    }
    const float first = samples_[first_frame * channels_ + channel];
    const float second = samples_[second_frame * channels_ + channel];
    return static_cast<float>(static_cast<double>(first) * (1.0 - fraction) +
                              static_cast<double>(second) * fraction);
}

float ShufflingLooper::filter_playback(const float sample,
                                       const std::size_t channel) noexcept {
    float filtered = sample;
    const float nyquist_limit = sample_rate_ * 0.49F;
    if (parameters_.low_cut_hz > 0.0F) {
        const float cutoff = std::clamp(parameters_.low_cut_hz, kMinimumCutHz,
                                        nyquist_limit);
        const float alpha = std::exp(-2.0F * std::numbers::pi_v<float> * cutoff /
                                     sample_rate_);
        const float highpass = alpha *
            (highpass_output_[channel] + filtered - highpass_input_[channel]);
        highpass_input_[channel] = filtered;
        highpass_output_[channel] = highpass;
        filtered = highpass;
    }
    if (parameters_.high_cut_hz > 0.0F) {
        const float cutoff = std::clamp(parameters_.high_cut_hz, kMinimumCutHz,
                                        nyquist_limit);
        const float alpha = 1.0F - std::exp(
            -2.0F * std::numbers::pi_v<float> * cutoff / sample_rate_);
        lowpass_state_[channel] += alpha * (filtered - lowpass_state_[channel]);
        filtered = lowpass_state_[channel];
    }
    return filtered;
}

float ShufflingLooper::smoothing_gain(const SequenceStep& step,
                                     const std::size_t output_frame) const noexcept {
    if (parameters_.smoothing <= 0.0F || step.output_frames < 2U) return 1.0F;
    const std::size_t fade_frames = static_cast<std::size_t>(
        static_cast<double>(step.output_frames) * parameters_.smoothing * 0.25);
    if (fade_frames == 0U) return 1.0F;
    const float fade_in = static_cast<float>(output_frame + 1U) /
                          static_cast<float>(fade_frames + 1U);
    const float remaining = static_cast<float>(step.output_frames - output_frame);
    const float fade_out = static_cast<float>(remaining) /
                           static_cast<float>(fade_frames + 1U);
    return std::clamp(std::min(fade_in, fade_out), 0.0F, 1.0F);
}

std::size_t ShufflingLooper::effective_slice_count() const noexcept {
    return std::clamp<std::size_t>(parameters_.slices, 1U,
                                   std::min(kMaximumSlices, loop_frames_));
}

}  // namespace tabletfx::effects
