#include "tabletfx/effects/pitch_sequence.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {
namespace {

constexpr double kMinimumHistoryMs = 10.0;
constexpr double kMaximumHistoryMs = 120.0;
constexpr std::size_t kMaximumFreezeCrossfadeFrames = 64U;
constexpr float kMaximumInputMagnitude = 16.0F;

}  // namespace

float PitchSequence::checked_sample_rate(float sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        throw std::invalid_argument(
            "PitchSequence requires a sample rate from 8 to 96 kHz");
    }
    return sample_rate;
}

std::uint16_t PitchSequence::checked_channels(std::uint16_t channels) {
    if (channels == 0U || channels > 2U) {
        throw std::invalid_argument(
            "PitchSequence supports mono or stereo audio");
    }
    return channels;
}

PitchSequence::PitchSequence(float sample_rate, std::uint16_t channels,
                             PitchSequenceParameters parameters)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)),
      pitch_shifter_(sample_rate_, channels_) {
    history_frames_ = static_cast<std::size_t>(std::ceil(
        static_cast<double>(sample_rate_) * kMaximumHistoryMs / 1000.0));
    history_.assign(history_frames_ * channels_, 0.0F);

    dry_delay_frames_ = static_cast<std::size_t>(std::lround(
        static_cast<double>(pitch_shifter_.algorithmic_latency_ms()) *
        static_cast<double>(sample_rate_) / 1000.0));
    dry_delay_frames_ = std::max<std::size_t>(1U, dry_delay_frames_);
    dry_delay_.assign((dry_delay_frames_ + 1U) * channels_, 0.0F);

    configure(parameters);
    reset();
}

std::size_t PitchSequence::memory_bytes() const noexcept {
    return sizeof(*this) + history_.capacity() * sizeof(float) +
           dry_delay_.capacity() * sizeof(float) +
           pitch_shifter_.memory_bytes() - sizeof(pitch_shifter_);
}

float PitchSequence::bounded(float value, float fallback,
                             float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

bool PitchSequence::valid_time_mode(tabletfx::DelayTimeMode mode) noexcept {
    return mode == tabletfx::DelayTimeMode::milliseconds ||
           mode == tabletfx::DelayTimeMode::tempo_sync;
}

bool PitchSequence::valid_division(tabletfx::DelayDivision division) noexcept {
    return static_cast<unsigned>(division) <=
           static_cast<unsigned>(tabletfx::DelayDivision::sixteenth_triplet);
}

bool PitchSequence::valid_step_mode(PitchSequenceStepMode mode) noexcept {
    return static_cast<unsigned>(mode) <=
           static_cast<unsigned>(PitchSequenceStepMode::Freeze);
}

double PitchSequence::division_beats(
    tabletfx::DelayDivision division) noexcept {
    switch (division) {
        case tabletfx::DelayDivision::whole: return 4.0;
        case tabletfx::DelayDivision::dotted_half: return 3.0;
        case tabletfx::DelayDivision::half: return 2.0;
        case tabletfx::DelayDivision::dotted_quarter: return 1.5;
        case tabletfx::DelayDivision::quarter: return 1.0;
        case tabletfx::DelayDivision::quarter_triplet: return 2.0 / 3.0;
        case tabletfx::DelayDivision::dotted_eighth: return 0.75;
        case tabletfx::DelayDivision::eighth: return 0.5;
        case tabletfx::DelayDivision::eighth_triplet: return 1.0 / 3.0;
        case tabletfx::DelayDivision::dotted_sixteenth: return 0.375;
        case tabletfx::DelayDivision::sixteenth: return 0.25;
        case tabletfx::DelayDivision::sixteenth_triplet: return 1.0 / 6.0;
        case tabletfx::DelayDivision::thirty_second: return 0.125;
        case tabletfx::DelayDivision::sixty_fourth: return 0.0625;
    }
    return 1.0;
}

float PitchSequence::finite_output(double sample) noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return static_cast<float>(std::clamp(
        sample, -static_cast<double>(kMaximumOutputMagnitude),
        static_cast<double>(kMaximumOutputMagnitude)));
}

void PitchSequence::configure(
    const PitchSequenceParameters& requested) noexcept {
    const bool old_run = parameters_.run;
    const double old_step_duration = step_duration_samples_;
    parameters_ = requested;

    parameters_.steps = std::clamp<std::uint8_t>(
        parameters_.steps, std::uint8_t{1}, std::uint8_t{16});
    for (std::size_t step = 0; step < parameters_.semitones.size(); ++step) {
        parameters_.semitones[step] =
            std::clamp(parameters_.semitones[step], -12, 12);
        if (!valid_step_mode(parameters_.mode[step])) {
            parameters_.mode[step] = PitchSequenceStepMode::Pitch;
        }
    }
    if (!valid_time_mode(parameters_.time_mode)) {
        parameters_.time_mode = tabletfx::DelayTimeMode::milliseconds;
    }
    if (!valid_division(parameters_.division)) {
        parameters_.division = tabletfx::DelayDivision::quarter;
    }
    parameters_.free_step_ms = bounded(
        parameters_.free_step_ms, 250.0F, kMinimumStepMs, kMaximumStepMs);
    parameters_.tempo_bpm = bounded(
        parameters_.tempo_bpm, 120.0F, kMinimumTempoBpm, kMaximumTempoBpm);
    parameters_.glide_ms = bounded(parameters_.glide_ms, 15.0F, 0.0F, 250.0F);
    parameters_.glitch_depth_semitones = std::min<std::uint8_t>(
        parameters_.glitch_depth_semitones, std::uint8_t{12});
    parameters_.glitch_interval_ms = bounded(
        parameters_.glitch_interval_ms, 40.0F, 10.0F, 250.0F);
    parameters_.freeze_window_ms = bounded(
        parameters_.freeze_window_ms, 80.0F,
        static_cast<float>(kMinimumHistoryMs),
        static_cast<float>(kMaximumHistoryMs));
    parameters_.mix = bounded(parameters_.mix, 1.0F, 0.0F, 1.0F);
    parameters_.level = bounded(parameters_.level, 1.0F, 0.0F, 2.0F);

    const bool run_changed = old_run != parameters_.run;
    running_ = parameters_.run;
    if (run_changed) {
        current_step_ = 0U;
        step_elapsed_samples_ = 0.0;
        freeze_active_ = false;
        freeze_length_ = 0U;
        freeze_loop_period_ = 0U;
        freeze_read_ = 0U;
        glitch_countdown_ = 0U;
        glitch_offset_ = 0;
    } else if (current_step_ >= parameters_.steps) {
        current_step_ = 0U;
        step_elapsed_samples_ = 0.0;
    }

    update_step_duration(!run_changed, old_step_duration);
    glide_coefficient_ = parameters_.glide_ms <= 0.0F
        ? 1.0F
        : static_cast<float>(1.0 - std::exp(
              -1000.0 / (static_cast<double>(parameters_.glide_ms) *
                         static_cast<double>(sample_rate_))));
    glitch_interval_frames_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(
                static_cast<double>(parameters_.glitch_interval_ms) *
                static_cast<double>(sample_rate_) / 1000.0)));
    freeze_window_frames_ = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::lround(
            static_cast<double>(parameters_.freeze_window_ms) *
            static_cast<double>(sample_rate_) / 1000.0)),
        1U, history_frames_);
}

void PitchSequence::update_step_duration(bool preserve_fraction,
                                         double old_step_duration) noexcept {
    double duration_ms = static_cast<double>(parameters_.free_step_ms);
    if (parameters_.time_mode == tabletfx::DelayTimeMode::tempo_sync) {
        duration_ms = 60000.0 / static_cast<double>(parameters_.tempo_bpm) *
                      division_beats(parameters_.division);
    }
    step_duration_samples_ = std::max(
        1.0, duration_ms * static_cast<double>(sample_rate_) / 1000.0);
    if (preserve_fraction && old_step_duration > 0.0) {
        step_elapsed_samples_ = std::clamp(
            step_elapsed_samples_ / old_step_duration, 0.0, 1.0) *
            step_duration_samples_;
        if (step_elapsed_samples_ >= step_duration_samples_) {
            step_elapsed_samples_ = std::nextafter(step_duration_samples_, 0.0);
        }
    } else {
        step_elapsed_samples_ = 0.0;
    }
}

void PitchSequence::record_history(
    const std::array<float, 2>& input) noexcept {
    for (std::uint16_t channel = 0; channel < channels_; ++channel) {
        history_[history_write_ * channels_ + channel] = input[channel];
    }
    history_write_ = (history_write_ + 1U) % history_frames_;
    history_filled_ = std::min(history_filled_ + 1U, history_frames_);
}

void PitchSequence::synchronize_freeze_mode() noexcept {
    const bool should_freeze = running_ &&
        parameters_.mode[current_step_] == PitchSequenceStepMode::Freeze;
    if (should_freeze && !freeze_active_) {
        begin_freeze();
    } else if (!should_freeze && freeze_active_) {
        freeze_active_ = false;
        freeze_length_ = 0U;
        freeze_crossfade_frames_ = 0U;
        freeze_loop_period_ = 0U;
        freeze_read_ = 0U;
    }
}

void PitchSequence::begin_freeze() noexcept {
    freeze_length_ = std::min(history_filled_, freeze_window_frames_);
    if (freeze_length_ == 0U) {
        freeze_active_ = false;
        return;
    }
    freeze_start_ = (history_write_ + history_frames_ - freeze_length_) %
                    history_frames_;
    freeze_crossfade_frames_ = freeze_length_ >= 16U
        ? std::min(kMaximumFreezeCrossfadeFrames, freeze_length_ / 4U)
        : 0U;
    freeze_loop_period_ = freeze_crossfade_frames_ == 0U
        ? freeze_length_
        : freeze_length_ - 2U * freeze_crossfade_frames_;
    freeze_loop_period_ = std::max<std::size_t>(1U, freeze_loop_period_);
    freeze_read_ = 0U;
    freeze_active_ = true;
}

void PitchSequence::read_frozen_frame(
    std::array<float, 2>& frame) noexcept {
    frame.fill(0.0F);
    if (!freeze_active_ || freeze_length_ == 0U) {
        return;
    }

    const std::size_t fade_start = freeze_length_ -
                                   2U * freeze_crossfade_frames_;
    const bool crossfading = freeze_crossfade_frames_ > 0U &&
                             freeze_read_ >= fade_start;
    const std::size_t fade_offset = crossfading
        ? freeze_read_ - fade_start
        : 0U;
    const std::size_t fade_denominator =
        freeze_crossfade_frames_ > 1U ? freeze_crossfade_frames_ - 1U : 1U;
    const float fade = crossfading
        ? static_cast<float>(fade_offset) /
              static_cast<float>(fade_denominator)
        : 0.0F;

    for (std::uint16_t channel = 0; channel < channels_; ++channel) {
        const std::size_t tail_frame =
            (freeze_start_ + freeze_read_) % history_frames_;
        float value = history_[tail_frame * channels_ + channel];
        if (crossfading) {
            const std::size_t head_frame =
                (freeze_start_ + fade_offset) % history_frames_;
            const float head = history_[head_frame * channels_ + channel];
            value += fade * (head - value);
        }
        frame[channel] = value;
    }

    ++freeze_read_;
    const std::size_t end_of_crossfade =
        freeze_length_ - freeze_crossfade_frames_;
    if (freeze_read_ >= end_of_crossfade) {
        freeze_read_ = freeze_crossfade_frames_;
    }
}

int PitchSequence::next_glitch_offset() noexcept {
    if (parameters_.glitch_depth_semitones == 0U) {
        return 0;
    }
    std::uint32_t value = random_state_;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    random_state_ = value == 0U ? kGlitchSeed : value;
    const std::uint32_t width =
        2U * parameters_.glitch_depth_semitones + 1U;
    return static_cast<int>(random_state_ % width) -
           static_cast<int>(parameters_.glitch_depth_semitones);
}

int PitchSequence::step_pitch_target() noexcept {
    if (!running_) {
        glitch_offset_ = 0;
        glitch_countdown_ = 0U;
        return 0;
    }

    int target = parameters_.semitones[current_step_];
    if (parameters_.mode[current_step_] == PitchSequenceStepMode::Glitch) {
        if (glitch_countdown_ == 0U) {
            glitch_offset_ = next_glitch_offset();
            glitch_countdown_ = glitch_interval_frames_;
        }
        --glitch_countdown_;
        target += glitch_offset_;
    } else {
        glitch_countdown_ = 0U;
        glitch_offset_ = 0;
    }
    return std::clamp(target, -12, 12);
}

void PitchSequence::advance_sequence() noexcept {
    if (!running_) {
        return;
    }
    step_elapsed_samples_ += 1.0;
    if (step_elapsed_samples_ + 1.0e-9 < step_duration_samples_) {
        return;
    }

    step_elapsed_samples_ -= step_duration_samples_;
    if (step_elapsed_samples_ < 0.0) {
        step_elapsed_samples_ = 0.0;
    }
    current_step_ = static_cast<std::uint8_t>(
        (static_cast<unsigned>(current_step_) + 1U) % parameters_.steps);
    glitch_countdown_ = 0U;
    glitch_offset_ = 0;
    synchronize_freeze_mode();
}

void PitchSequence::trigger() noexcept {
    parameters_.run = true;
    running_ = true;
    current_step_ = 0U;
    step_elapsed_samples_ = 0.0;
    glitch_countdown_ = 0U;
    glitch_offset_ = 0;
    freeze_active_ = false;
    freeze_length_ = 0U;
    freeze_crossfade_frames_ = 0U;
    freeze_loop_period_ = 0U;
    freeze_read_ = 0U;
}

void PitchSequence::stop() noexcept {
    parameters_.run = false;
    running_ = false;
    current_step_ = 0U;
    step_elapsed_samples_ = 0.0;
    glitch_countdown_ = 0U;
    glitch_offset_ = 0;
    freeze_active_ = false;
    freeze_length_ = 0U;
    freeze_crossfade_frames_ = 0U;
    freeze_loop_period_ = 0U;
    freeze_read_ = 0U;
}

void PitchSequence::reset() noexcept {
    pitch_shifter_.reset();
    std::fill(history_.begin(), history_.end(), 0.0F);
    std::fill(dry_delay_.begin(), dry_delay_.end(), 0.0F);
    history_write_ = 0U;
    history_filled_ = 0U;
    dry_write_ = 0U;
    freeze_start_ = 0U;
    freeze_length_ = 0U;
    freeze_crossfade_frames_ = 0U;
    freeze_loop_period_ = 0U;
    freeze_read_ = 0U;
    glitch_countdown_ = 0U;
    random_state_ = kGlitchSeed;
    glitch_offset_ = 0;
    current_pitch_semitones_ = 0.0F;
    current_step_ = 0U;
    step_elapsed_samples_ = 0.0;
    running_ = parameters_.run;
    freeze_active_ = false;
}

void PitchSequence::record_input_history(
    const std::span<const float> interleaved,
    const std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U ||
        interleaved.size() % channels_ != 0U) {
        return;
    }

    std::array<float, 2> input{};
    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        input.fill(0.0F);
        for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
            const float sample = interleaved[frame * channels_ + channel];
            input[channel] = std::isfinite(sample)
                ? std::clamp(sample, -kMaximumInputMagnitude,
                             kMaximumInputMagnitude)
                : 0.0F;
        }
        if (!freeze_active_) record_history(input);
    }
}

void PitchSequence::process(std::span<float> interleaved,
                            std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    const std::size_t dry_buffer_frames = dry_delay_frames_ + 1U;
    std::array<float, 2> input{};
    std::array<float, 2> source{};
    std::array<float, 2> shifted{};
    std::array<float, 2> dry{};

    for (std::size_t frame = 0; frame < frames; ++frame) {
        input.fill(0.0F);
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const float sample = interleaved[frame * channels_ + channel];
            input[channel] = std::isfinite(sample)
                ? std::clamp(sample, -kMaximumInputMagnitude,
                             kMaximumInputMagnitude)
                : 0.0F;
        }

        if (!freeze_active_) {
            record_history(input);
        }
        synchronize_freeze_mode();
        if (freeze_active_) {
            read_frozen_frame(source);
        } else {
            source = input;
        }

        const int target = step_pitch_target();
        if (glide_coefficient_ >= 1.0F) {
            current_pitch_semitones_ = static_cast<float>(target);
        } else {
            current_pitch_semitones_ += glide_coefficient_ *
                (static_cast<float>(target) - current_pitch_semitones_);
        }
        current_pitch_semitones_ = std::clamp(
            current_pitch_semitones_, -12.0F, 12.0F);
        pitch_shifter_.set_pitch_semitones(current_pitch_semitones_);
        shifted = source;
        pitch_shifter_.process(
            std::span<float>(shifted.data(), channels_), channels_);

        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            dry_delay_[dry_write_ * channels_ + channel] = source[channel];
        }
        const std::size_t dry_read =
            (dry_write_ + dry_buffer_frames - dry_delay_frames_) %
            dry_buffer_frames;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            dry[channel] = dry_delay_[dry_read * channels_ + channel];
            const double mixed =
                static_cast<double>(dry[channel]) * (1.0 - parameters_.mix) +
                static_cast<double>(shifted[channel]) * parameters_.mix;
            interleaved[frame * channels_ + channel] = finite_output(
                mixed * static_cast<double>(parameters_.level));
        }
        dry_write_ = (dry_write_ + 1U) % dry_buffer_frames;
        advance_sequence();
    }
}

}  // namespace tabletfx::effects
