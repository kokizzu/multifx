#include "tabletfx/effects/shuffling_looper.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <numbers>

namespace tabletfx::effects {

ShufflingLooper::ShufflingLooper(const float sample_rate,
                                 const std::uint16_t channels,
                                 const std::uint32_t seed,
                                 const std::size_t instance_memory_limit_bytes)
    : sample_rate_(sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate &&
                           std::isfinite(sample_rate)
                       ? sample_rate
                       : kDefaultSampleRate),
      channels_(validate_channels(channels)),
      samples_(nullptr),
      instance_memory_limit_bytes_(std::min(
          instance_memory_limit_bytes, kMaximumInstanceMemoryBytes)),
      seed_(seed == 0U ? 1U : seed),
      rng_state_(seed_) {
    beat_phase_limit_units_ = static_cast<std::uint64_t>(
        std::llround(static_cast<double>(sample_rate_) * 60000.0));
    const std::size_t minimum_bytes = sizeof(*this) +
        static_cast<std::size_t>(channels_) * sizeof(float);
    if (instance_memory_limit_bytes_ < minimum_bytes) {
        throw std::invalid_argument(
            "ShufflingLooper memory limit cannot hold one frame");
    }
    const auto status = configure_capacity_seconds(kDefaultCapacitySeconds);
    if (status == ShufflingLooperCapacityStatus::AllocationFailed) {
        throw std::bad_alloc();
    }
}

std::string_view ShufflingLooper::id() const noexcept {
    return "shuffling-looper";
}

std::size_t ShufflingLooper::memory_bytes() const noexcept {
    return sizeof(*this) + capacity_storage_bytes();
}

std::uint8_t ShufflingLooper::sequence_slice(const std::size_t step) const noexcept {
    if (step >= sequence_step_count_) return std::numeric_limits<std::uint8_t>::max();
    return sequence_[step].slice;
}

bool ShufflingLooper::record() noexcept {
    if (state_ == ShufflingLooperState::Recording) return false;
    const bool quantized = parameters_.quantize_recording;
    const std::uint32_t requested_tempo_milli = static_cast<std::uint32_t>(
        std::lround(static_cast<double>(parameters_.tempo_bpm) * 1000.0));
    const std::size_t quantized_limit = quantized
        ? quantized_capacity_frames(requested_tempo_milli)
        : 0U;
    if (quantized && quantized_limit < 2U) return false;

    // Destructive replacement: old samples become unreachable immediately.
    loop_frames_ = 0U;
    sequence_step_count_ = 0U;
    sequence_step_ = 0U;
    step_frame_ = 0U;
    quantized_capture_ = quantized;
    capture_tempo_milli_bpm_ = requested_tempo_milli;
    quantized_capture_limit_frames_ = quantized_limit;
    state_ = quantized ? ShufflingLooperState::WaitingToRecord
                       : ShufflingLooperState::Recording;
    lowpass_state_.fill(0.0F);
    highpass_input_.fill(0.0F);
    highpass_output_.fill(0.0F);
    return true;
}

bool ShufflingLooper::stop_recording() noexcept {
    if (state_ == ShufflingLooperState::WaitingToRecord) {
        state_ = ShufflingLooperState::Stopped;
        loop_frames_ = 0U;
        sequence_step_count_ = 0U;
        sequence_step_ = 0U;
        quantized_capture_ = false;
        quantized_capture_limit_frames_ = 0U;
        return false;
    }
    if (state_ == ShufflingLooperState::WaitingToStop) return false;
    if (state_ != ShufflingLooperState::Recording) return false;
    if (quantized_capture_) {
        state_ = ShufflingLooperState::WaitingToStop;
        return true;
    }
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
    if (state_ == ShufflingLooperState::WaitingToRecord) {
        state_ = ShufflingLooperState::Stopped;
        loop_frames_ = 0U;
        sequence_step_count_ = 0U;
        sequence_step_ = 0U;
        step_frame_ = 0U;
        quantized_capture_ = false;
        quantized_capture_limit_frames_ = 0U;
    } else if (state_ != ShufflingLooperState::Recording &&
               state_ != ShufflingLooperState::WaitingToStop) {
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
    quantized_capture_ = false;
    quantized_capture_limit_frames_ = 0U;
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
    sanitized.tempo_bpm = bounded(sanitized.tempo_bpm, parameters_.tempo_bpm,
                                  kMinimumTempoBpm, kMaximumTempoBpm);

    const bool quantization_was_enabled = parameters_.quantize_recording;
    parameters_ = sanitized;
    tempo_milli_bpm_ = static_cast<std::uint32_t>(
        std::lround(static_cast<double>(parameters_.tempo_bpm) * 1000.0));
    if (parameters_.quantize_recording && !quantization_was_enabled &&
        !quantized_capture_) {
        beat_phase_units_ = 0U;
        next_frame_is_beat_ = true;
    }
    if (loop_frames_ != 0U) rebuild_sequence(false);
}

ShufflingLooperCapacityStatus ShufflingLooper::configure_capacity_seconds(
    const float requested_seconds) noexcept {
    if (!std::isfinite(requested_seconds) || !(requested_seconds > 0.0F)) {
        capacity_status_ =
            ShufflingLooperCapacityStatus::RejectedInvalidRequest;
        return capacity_status_;
    }

    const double maximum_seconds =
        static_cast<double>(maximum_capacity_seconds());
    const bool model_clamped =
        static_cast<double>(requested_seconds) > maximum_seconds;
    const double bounded_seconds = std::min(
        static_cast<double>(requested_seconds), maximum_seconds);
    const double requested_frames = bounded_seconds *
                                    static_cast<double>(sample_rate_);
    const std::size_t model_maximum_frames = static_cast<std::size_t>(
        std::llround(maximum_seconds * static_cast<double>(sample_rate_)));
    std::size_t candidate_frames = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::llround(requested_frames)),
        1U, model_maximum_frames);

    const std::size_t storage_budget =
        instance_memory_limit_bytes_ > sizeof(*this)
            ? instance_memory_limit_bytes_ - sizeof(*this)
            : 0U;
    const std::size_t transition_storage_budget = std::min(
        storage_budget, kMaximumTransitionSampleStorageBytes);
    const std::size_t bytes_per_frame =
        static_cast<std::size_t>(channels_) * sizeof(float);
    const std::size_t previous_storage_bytes = capacity_storage_bytes();
    bool memory_clamped = false;
    bool transient_memory_rejected = false;
    if (candidate_frames != capacity_frames_) {
        if (candidate_frames > capacity_frames_) {
            const std::size_t remaining_storage_budget =
                transition_storage_budget > previous_storage_bytes
                    ? transition_storage_budget - previous_storage_bytes
                    : 0U;
            const std::size_t memory_maximum_frames =
                remaining_storage_budget / bytes_per_frame;
            if (candidate_frames > memory_maximum_frames) {
                memory_clamped = true;
                candidate_frames = std::max(
                    capacity_frames_, memory_maximum_frames);
            }
        } else {
            const std::size_t replacement_storage_bytes =
                candidate_frames * bytes_per_frame;
            transient_memory_rejected =
                previous_storage_bytes > transition_storage_budget ||
                replacement_storage_bytes >
                    transition_storage_budget - previous_storage_bytes;
        }
    }

    if (candidate_frames == 0U) {
        capacity_status_ = ShufflingLooperCapacityStatus::AllocationFailed;
        return capacity_status_;
    }
    if (candidate_frames < loop_frames_) {
        capacity_status_ =
            ShufflingLooperCapacityStatus::RejectedLoopWouldBeTruncated;
        return capacity_status_;
    }
    if (transient_memory_rejected) {
        capacity_status_ =
            ShufflingLooperCapacityStatus::RejectedTransitionMemoryLimit;
        return capacity_status_;
    }

    ShufflingLooperCapacityStatus requested_status =
        ShufflingLooperCapacityStatus::Applied;
    if (model_clamped && memory_clamped) {
        requested_status =
            ShufflingLooperCapacityStatus::ClampedByModelAndMemoryLimit;
    } else if (model_clamped) {
        requested_status = ShufflingLooperCapacityStatus::ClampedByModelLimit;
    } else if (memory_clamped) {
        requested_status = ShufflingLooperCapacityStatus::ClampedByMemoryLimit;
    }

    if (candidate_frames == capacity_frames_) {
        capacity_status_ = requested_status;
        return capacity_status_;
    }

    const std::size_t sample_count =
        candidate_frames * static_cast<std::size_t>(channels_);
    std::unique_ptr<float[]> replacement(
        new (std::nothrow) float[sample_count]);
    if (!replacement) {
        capacity_status_ = ShufflingLooperCapacityStatus::AllocationFailed;
        return capacity_status_;
    }
    if (loop_frames_ != 0U) {
        std::copy_n(samples_.get(),
                    loop_frames_ * static_cast<std::size_t>(channels_),
                    replacement.get());
    }
    samples_.swap(replacement);
    capacity_frames_ = candidate_frames;
    capacity_status_ = requested_status;
    return capacity_status_;
}

void ShufflingLooper::set_seed(const std::uint32_t seed) noexcept {
    seed_ = seed == 0U ? 1U : seed;
    rng_state_ = seed_;
    if (loop_frames_ != 0U) rebuild_sequence(false);
}

void ShufflingLooper::reset() noexcept {
    state_ = ShufflingLooperState::Stopped;
    beat_phase_units_ = 0U;
    next_frame_is_beat_ = true;
    quantized_capture_ = false;
    quantized_capture_limit_frames_ = 0U;
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
        if ((parameters_.quantize_recording || quantized_capture_) &&
            next_frame_is_beat_) {
            if (state_ == ShufflingLooperState::WaitingToRecord) {
                state_ = ShufflingLooperState::Recording;
            } else if (state_ == ShufflingLooperState::WaitingToStop) {
                finish_recording();
            }
        }

        bool recorded_this_frame = false;
        if (state_ == ShufflingLooperState::Recording ||
            state_ == ShufflingLooperState::WaitingToStop) {
            if (loop_frames_ >= capacity_frames_) {
                if (quantized_capture_ &&
                    quantized_capture_limit_frames_ < loop_frames_) {
                    loop_frames_ = quantized_capture_limit_frames_;
                }
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
                recorded_this_frame = true;
                if (quantized_capture_ &&
                    loop_frames_ >= quantized_capture_limit_frames_) {
                    finish_recording();
                } else if (loop_frames_ == capacity_frames_) {
                    finish_recording();
                }
            }
        }

        if (!recorded_this_frame && state_ == ShufflingLooperState::Playing &&
            sequence_step_count_ != 0U) {
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

        advance_beat_clock();
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
    quantized_capture_ = false;
    quantized_capture_limit_frames_ = 0U;
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

std::size_t ShufflingLooper::quantized_capacity_frames(
    const std::uint32_t tempo_milli_bpm) const noexcept {
    if (beat_phase_limit_units_ == 0U || tempo_milli_bpm == 0U ||
        capacity_frames_ < 2U) {
        return 0U;
    }

    std::uint64_t start_phase = beat_phase_units_;
    if (!next_frame_is_beat_) {
        const std::uint64_t remaining = beat_phase_limit_units_ - start_phase;
        const std::uint64_t frames_to_edge =
            (remaining + tempo_milli_bpm - 1U) / tempo_milli_bpm;
        start_phase = start_phase + frames_to_edge * tempo_milli_bpm -
                      beat_phase_limit_units_;
    }

    std::size_t accepted_frames = 0U;
    // With a maximum 60-second loop and the project tempo bounds, no more than
    // 300 beat intervals fit. This fixed loop has no allocation or unbounded work.
    for (std::uint64_t beats = 1U; beats <= 300U; ++beats) {
        const std::uint64_t numerator =
            beats * beat_phase_limit_units_ - start_phase;
        const std::uint64_t frames =
            (numerator + tempo_milli_bpm - 1U) / tempo_milli_bpm;
        if (frames > capacity_frames_) break;
        accepted_frames = static_cast<std::size_t>(frames);
    }
    return accepted_frames;
}

void ShufflingLooper::advance_beat_clock() noexcept {
    if (!parameters_.quantize_recording && !quantized_capture_) return;
    const std::uint32_t tempo = quantized_capture_
        ? capture_tempo_milli_bpm_ : tempo_milli_bpm_;
    const std::uint64_t advanced = beat_phase_units_ + tempo;
    if (advanced >= beat_phase_limit_units_) {
        beat_phase_units_ = advanced - beat_phase_limit_units_;
        next_frame_is_beat_ = true;
    } else {
        beat_phase_units_ = advanced;
        next_frame_is_beat_ = false;
    }
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
