#include "tabletfx/effects/fractal_diatonic_harmonizer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {
namespace {

constexpr std::size_t kPitchUpdateHopAt48k = 128;
constexpr float kMinimumPitchHz = 55.0F;
constexpr float kMaximumPitchHz = 1100.0F;
constexpr float kFastPathMinimumHz = 125.0F;
constexpr float kLowMidPathMinimumHz = 80.0F;
constexpr float kMinimumRms = 0.00025F;
constexpr float kYinThreshold = 0.18F;
constexpr float kMinimumConfidence = 0.62F;
constexpr float kPitchConfirmationCents = 50.0F;
constexpr std::uint8_t kFastPathPitchObservations = 2U;
constexpr std::uint8_t kLowPathPitchObservations = 3U;

float checked_sample_rate(float sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate < 8000.0F ||
        sample_rate > 192000.0F) {
        throw std::invalid_argument(
            "FractalDiatonicHarmonizer sample rate must be 8-192 kHz");
    }
    return sample_rate;
}

std::uint16_t checked_channels(std::uint16_t channels) {
    if (channels != 1U && channels != 2U) {
        throw std::invalid_argument(
            "FractalDiatonicHarmonizer supports one or two channels");
    }
    return channels;
}

bool valid_scale(DiatonicScale scale) noexcept {
    return static_cast<unsigned>(scale) <=
           static_cast<unsigned>(DiatonicScale::Custom);
}

void sanitize_custom_scale(std::array<std::uint8_t, 8>& notes,
                           std::uint8_t& count) noexcept {
    const unsigned bounded_count = std::clamp<unsigned>(count, 4U, 8U);
    count = static_cast<std::uint8_t>(bounded_count);
    int previous = -1;
    for (unsigned index = 0; index < bounded_count; ++index) {
        const int minimum = previous + 1;
        const int maximum = 11 - static_cast<int>(bounded_count - 1U - index);
        const int requested = std::min<int>(notes[index], 11);
        const int sanitized = std::clamp(requested, minimum, maximum);
        notes[index] = static_cast<std::uint8_t>(sanitized);
        previous = sanitized;
    }
    for (unsigned index = bounded_count; index < notes.size(); ++index) {
        notes[index] = 0U;
    }
}

int pitch_class(int midi_note) noexcept {
    return ((midi_note % 12) + 12) % 12;
}

}  // namespace

FractalDiatonicHarmonizer::PitchTracker::PitchTracker(
    float sample_rate) noexcept
    : sample_rate_(sample_rate) {}

void FractalDiatonicHarmonizer::PitchTracker::reset() noexcept {
    ring_.fill(0.0F);
    frame_.fill(0.0F);
    difference_.fill(0.0F);
    write_ = 0;
    count_ = 0;
    hop_count_ = 0;
}

float FractalDiatonicHarmonizer::PitchTracker::estimate(
    std::size_t window_samples, float minimum_hz,
    float maximum_hz) noexcept {
    if (window_samples < 64U || window_samples > frame_.size() ||
        count_ < window_samples) {
        return 0.0F;
    }

    const std::size_t start =
        (write_ + ring_.size() - window_samples) % ring_.size();
    double mean = 0.0;
    for (std::size_t index = 0; index < window_samples; ++index) {
        const float value = ring_[(start + index) % ring_.size()];
        frame_[index] = value;
        mean += value;
    }
    mean /= static_cast<double>(window_samples);

    double energy = 0.0;
    for (std::size_t index = 0; index < window_samples; ++index) {
        frame_[index] -= static_cast<float>(mean);
        energy += static_cast<double>(frame_[index]) * frame_[index];
    }
    const double rms = std::sqrt(energy / static_cast<double>(window_samples));
    if (!(rms > static_cast<double>(kMinimumRms)) || !std::isfinite(rms)) {
        return 0.0F;
    }

    const std::size_t minimum_lag = std::max<std::size_t>(
        2U, static_cast<std::size_t>(sample_rate_ / maximum_hz));
    const std::size_t maximum_lag = std::min<std::size_t>(
        window_samples - 32U,
        static_cast<std::size_t>(sample_rate_ / minimum_hz));
    if (maximum_lag <= minimum_lag + 2U) {
        return 0.0F;
    }

    std::fill(difference_.begin(), difference_.begin() +
                  static_cast<std::ptrdiff_t>(maximum_lag + 1U), 0.0F);
    for (std::size_t lag = minimum_lag; lag <= maximum_lag; ++lag) {
        double sum = 0.0;
        const std::size_t compare_count = window_samples - lag;
        for (std::size_t index = 0; index < compare_count; ++index) {
            const double delta = static_cast<double>(frame_[index]) -
                                 static_cast<double>(frame_[index + lag]);
            sum += delta * delta;
        }
        difference_[lag] = static_cast<float>(
            sum / static_cast<double>(compare_count));
    }

    double cumulative = 0.0;
    float best_cmnd = 1.0F;
    std::size_t best_lag = 0U;
    std::size_t selected_lag = 0U;
    for (std::size_t lag = minimum_lag; lag <= maximum_lag; ++lag) {
        cumulative += difference_[lag];
        if (cumulative <= 1.0e-18) {
            continue;
        }
        const float cmnd = static_cast<float>(
            difference_[lag] * static_cast<double>(lag) / cumulative);
        if (cmnd < best_cmnd) {
            best_cmnd = cmnd;
            best_lag = lag;
        }
        if (cmnd < kYinThreshold && lag + 1U <= maximum_lag &&
            difference_[lag] <= difference_[lag + 1U]) {
            selected_lag = lag;
            break;
        }
    }
    if (selected_lag == 0U) {
        selected_lag = best_lag;
    }
    if (selected_lag == 0U || best_cmnd > 1.0F - kMinimumConfidence) {
        return 0.0F;
    }

    // A short, harmonically rich input can make the period detector settle on
    // a two-period lag. Prefer the shorter, still-supported period only when
    // its raw mean-square difference is materially lower; a genuine low note
    // retains a much stronger half-period mismatch and stays at its fundamental.
    if (selected_lag >= 2U * minimum_lag) {
        const std::size_t shorter_lag = selected_lag / 2U;
        if (shorter_lag >= minimum_lag && shorter_lag + 1U <= maximum_lag &&
            difference_[shorter_lag] <= difference_[shorter_lag + 1U] &&
            difference_[shorter_lag] < 0.7F * difference_[selected_lag]) {
            selected_lag = shorter_lag;
        }
    }

    // Reject an estimate pinned to the fast window's low-frequency boundary;
    // the longer low-note window can then resolve bass fundamentals cleanly.
    if (selected_lag + 12U >= maximum_lag) {
        return 0.0F;
    }

    float refined_lag = static_cast<float>(selected_lag);
    if (selected_lag > minimum_lag && selected_lag < maximum_lag) {
        const float before = difference_[selected_lag - 1U];
        const float center = difference_[selected_lag];
        const float after = difference_[selected_lag + 1U];
        const float denominator = before - 2.0F * center + after;
        if (std::abs(denominator) > 1.0e-9F) {
            const float correction = 0.5F * (before - after) / denominator;
            refined_lag += std::clamp(correction, -0.5F, 0.5F);
        }
    }
    if (!(refined_lag > 0.0F) || !std::isfinite(refined_lag)) {
        return 0.0F;
    }
    const float frequency = sample_rate_ / refined_lag;
    if (!std::isfinite(frequency) || frequency < minimum_hz ||
        frequency > maximum_hz) {
        return 0.0F;
    }
    return frequency;
}

bool FractalDiatonicHarmonizer::PitchTracker::process_sample(
    float left, float right, float& frequency_hz,
    std::uint32_t& window_samples) noexcept {
    const float mono = 0.5F * (left + right);
    ring_[write_] = std::isfinite(mono) ? mono : 0.0F;
    write_ = (write_ + 1U) % ring_.size();
    count_ = std::min(count_ + 1U, ring_.size());
    ++hop_count_;

    const std::size_t update_hop = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(
                sample_rate_ * static_cast<float>(kPitchUpdateHopAt48k) /
                48000.0F)));
    if (hop_count_ < update_hop) {
        return false;
    }
    hop_count_ = 0U;

    const std::size_t fast_window = static_cast<std::size_t>(std::lround(
        sample_rate_ * (512.0F / 48000.0F)));
    const std::size_t low_mid_window = static_cast<std::size_t>(std::lround(
        sample_rate_ * (768.0F / 48000.0F)));
    const std::size_t low_window = static_cast<std::size_t>(std::lround(
        sample_rate_ * (1024.0F / 48000.0F)));
    frequency_hz = 0.0F;
    window_samples = static_cast<std::uint32_t>(low_window);

    if (count_ < fast_window) {
        return true;
    }
    const float fast_frequency = estimate(fast_window,
                                          kFastPathMinimumHz,
                                          kMaximumPitchHz);
    if (fast_frequency > 0.0F) {
        frequency_hz = fast_frequency;
        window_samples = static_cast<std::uint32_t>(fast_window);
        return true;
    }

    if (count_ >= low_mid_window) {
        frequency_hz = estimate(low_mid_window, kLowMidPathMinimumHz,
                                kFastPathMinimumHz);
        if (frequency_hz > 0.0F) {
            window_samples = static_cast<std::uint32_t>(low_mid_window);
            return true;
        }
    }

    if (count_ >= low_window) {
        frequency_hz = estimate(low_window, kMinimumPitchHz,
                                kFastPathMinimumHz);
        window_samples = static_cast<std::uint32_t>(low_window);
    }
    return true;
}

FractalDiatonicHarmonizer::FractalDiatonicHarmonizer(
    float sample_rate, std::uint16_t channels)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)),
      shifter_(sample_rate_, channels_, 4U,
               FractalChromaticHarmonizer::InputMode::MonoSum),
      tracker_(sample_rate_) {
    config_ = sanitize(config_);
    for (std::uint8_t voice = 0U; voice < 4U; ++voice) {
        apply_voice(voice);
    }
    dry_gain_ = 1.0F - config_.mix;
    wet_gain_ = config_.mix;
    const std::size_t low_window = static_cast<std::size_t>(std::lround(
        sample_rate_ * (1024.0F / 48000.0F)));
    const std::size_t hop = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(
                sample_rate_ * static_cast<float>(kPitchUpdateHopAt48k) /
                48000.0F)));
    current_control_latency_samples_ = static_cast<std::uint32_t>(
        low_window + 3U * hop + latency_samples());
}

FractalDiatonicHarmonizerConfig FractalDiatonicHarmonizer::sanitize(
    const FractalDiatonicHarmonizerConfig& requested) noexcept {
    FractalDiatonicHarmonizerConfig result = requested;
    if (result.voice_count != 2U && result.voice_count != 4U) {
        result.voice_count = 2U;
    }
    result.key = std::clamp(result.key, 0, 11);
    if (!valid_scale(result.scale)) {
        result.scale = DiatonicScale::Major;
    }
    if (!std::isfinite(result.mix)) {
        result.mix = 0.5F;
    }
    result.mix = std::clamp(result.mix, 0.0F, 1.0F);
    if (!std::isfinite(result.glide_ms)) {
        result.glide_ms = 0.0F;
    }
    result.glide_ms = std::clamp(result.glide_ms, 0.0F, 500.0F);
    sanitize_custom_scale(result.custom_scale_notes,
                          result.custom_scale_note_count);

    for (auto& voice : result.voices) {
        voice.harmony_steps = std::clamp(voice.harmony_steps, -24, 24);
        if (!std::isfinite(voice.detune_cents)) voice.detune_cents = 0.0F;
        voice.detune_cents = std::clamp(voice.detune_cents, -50.0F, 50.0F);
        if (!std::isfinite(voice.level)) voice.level = 0.0F;
        voice.level = std::clamp(voice.level, 0.0F, 1.0F);
        if (!std::isfinite(voice.pan)) voice.pan = 0.0F;
        voice.pan = std::clamp(voice.pan, -1.0F, 1.0F);
        if (!std::isfinite(voice.delay_ms)) voice.delay_ms = 0.0F;
        voice.delay_ms = std::clamp(voice.delay_ms, 0.0F, 2000.0F);
        if (!std::isfinite(voice.feedback)) voice.feedback = 0.0F;
        voice.feedback = std::clamp(voice.feedback, 0.0F, 0.9F);
    }
    return result;
}

void FractalDiatonicHarmonizer::configure(
    const FractalDiatonicHarmonizerConfig& requested) noexcept {
    config_ = sanitize(requested);
    dry_gain_ = 1.0F - config_.mix;
    wet_gain_ = config_.mix;
    for (std::uint8_t voice = 0U; voice < 4U; ++voice) {
        apply_voice(voice);
    }
}

bool FractalDiatonicHarmonizer::set_voice_parameters(
    std::uint8_t voice,
    const FractalDiatonicVoiceParameters& parameters) noexcept {
    if (voice >= config_.voices.size() ||
        !std::isfinite(parameters.detune_cents) ||
        !std::isfinite(parameters.level) || !std::isfinite(parameters.pan) ||
        !std::isfinite(parameters.delay_ms) ||
        !std::isfinite(parameters.feedback)) {
        return false;
    }
    config_.voices[voice] = parameters;
    const auto sanitized = sanitize(config_);
    config_.voices[voice] = sanitized.voices[voice];
    apply_voice(voice);
    return true;
}

bool FractalDiatonicHarmonizer::set_custom_scale(
    std::uint8_t note_count,
    const std::array<std::uint8_t, 8>& notes) noexcept {
    config_.custom_scale_note_count = note_count;
    config_.custom_scale_notes = notes;
    config_ = sanitize(config_);
    return true;
}

void FractalDiatonicHarmonizer::apply_voice(std::uint8_t voice) noexcept {
    if (voice >= 4U) return;
    const auto& configured = config_.voices[voice];
    FractalChromaticHarmonizer::VoiceParameters parameters;
    parameters.shift_semitones = current_shift_semitones_[voice];
    parameters.detune_cents = configured.detune_cents;
    parameters.level = enabled_ && has_valid_pitch_ &&
                               voice < config_.voice_count
                           ? configured.level
                           : 0.0F;
    parameters.pan = configured.pan;
    parameters.delay_ms = voice < config_.voice_count
                              ? configured.delay_ms
                              : 0.0F;
    parameters.feedback = voice < config_.voice_count
                              ? configured.feedback
                              : 0.0F;
    (void)shifter_.set_voice_parameters(voice, parameters);
}

void FractalDiatonicHarmonizer::update_harmonies(
    float input_frequency_hz) noexcept {
    const bool valid_frequency = input_frequency_hz >= kMinimumPitchHz &&
                                 input_frequency_hz <= kMaximumPitchHz &&
                                 std::isfinite(input_frequency_hz);
    if (!valid_frequency) {
        has_valid_pitch_ = false;
        detected_input_frequency_hz_ = 0.0F;
        pending_input_frequency_hz_ = 0.0F;
        pending_pitch_observations_ = 0U;
        for (std::uint8_t voice = 0U; voice < 4U; ++voice) {
            target_shift_semitones_[voice] = 0.0F;
            apply_voice(voice);
        }
        return;
    }

    const float comparison_frequency = has_valid_pitch_
        ? detected_input_frequency_hz_
        : pending_input_frequency_hz_;
    const bool needs_confirmation = !has_valid_pitch_ ||
        comparison_frequency <= 0.0F ||
        std::abs(1200.0F * std::log2(input_frequency_hz /
                                     comparison_frequency)) >
            kPitchConfirmationCents;
    if (needs_confirmation) {
        const std::uint8_t required_observations =
            input_frequency_hz >= kFastPathMinimumHz
                ? kFastPathPitchObservations
                : kLowPathPitchObservations;
        const bool matches_pending = pending_input_frequency_hz_ > 0.0F &&
            std::abs(1200.0F * std::log2(input_frequency_hz /
                                         pending_input_frequency_hz_)) <=
                kPitchConfirmationCents;
        if (matches_pending) {
            pending_input_frequency_hz_ = input_frequency_hz;
            pending_pitch_observations_ = static_cast<std::uint8_t>(
                std::min<unsigned>(required_observations,
                    static_cast<unsigned>(pending_pitch_observations_) + 1U));
        } else {
            pending_input_frequency_hz_ = input_frequency_hz;
            pending_pitch_observations_ = 1U;
        }
        if (pending_pitch_observations_ < required_observations) {
            return;
        }
    }
    pending_input_frequency_hz_ = 0.0F;
    pending_pitch_observations_ = 0U;

    const float midi_value = 69.0F + 12.0F *
        std::log2(input_frequency_hz / 440.0F);
    if (!std::isfinite(midi_value)) {
        update_harmonies(0.0F);
        return;
    }
    const int input_midi = static_cast<int>(std::floor(midi_value + 0.5F));
    if (config_.learn) {
        config_.key = pitch_class(input_midi);
    }
    detected_input_frequency_hz_ = input_frequency_hz;
    has_valid_pitch_ = true;

    const float glide_alpha = config_.glide_ms <= 0.0F
        ? 1.0F
        : 1.0F - std::exp(-1000.0F *
              (static_cast<float>(kPitchUpdateHopAt48k) / 48000.0F) /
              (config_.glide_ms / 1000.0F));
    for (std::uint8_t voice = 0U; voice < 4U; ++voice) {
        int target_midi = input_midi;
        const int steps = config_.voices[voice].harmony_steps;
        if (config_.scale == DiatonicScale::Custom) {
            target_midi = DiatonicHarmonizer::harmony_midi_note_custom(
                input_midi, config_.key, steps, config_.custom_scale_notes,
                config_.custom_scale_note_count);
        } else {
            target_midi = DiatonicHarmonizer::harmony_midi_note(
                input_midi, config_.key, config_.scale, steps);
        }
        target_shift_semitones_[voice] =
            static_cast<float>(target_midi - input_midi);
        if (config_.glide_ms <= 0.0F) {
            current_shift_semitones_[voice] = target_shift_semitones_[voice];
        } else {
            current_shift_semitones_[voice] += glide_alpha *
                (target_shift_semitones_[voice] -
                 current_shift_semitones_[voice]);
        }
        apply_voice(voice);
    }
}

std::size_t FractalDiatonicHarmonizer::memory_bytes() const noexcept {
    return sizeof(*this) + shifter_.memory_bytes() - sizeof(shifter_);
}

std::uint32_t FractalDiatonicHarmonizer::latency_samples() const noexcept {
    return shifter_.latency_samples();
}

float FractalDiatonicHarmonizer::algorithmic_latency_ms() const noexcept {
    return shifter_.algorithmic_latency_ms();
}

std::uint32_t FractalDiatonicHarmonizer::pitch_control_latency_samples() const noexcept {
    return current_control_latency_samples_;
}

float FractalDiatonicHarmonizer::detected_input_frequency_hz() const noexcept {
    return detected_input_frequency_hz_;
}

float FractalDiatonicHarmonizer::candidate_input_frequency_hz() const noexcept {
    return candidate_input_frequency_hz_;
}

void FractalDiatonicHarmonizer::set_enabled(bool enabled) noexcept {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    reset();
}

void FractalDiatonicHarmonizer::reset() noexcept {
    tracker_.reset();
    shifter_.reset();
    for (auto& channel : dry_delay_) channel.fill(0.0F);
    source_block_.fill(0.0F);
    dry_block_.fill(0.0F);
    dry_write_ = 0U;
    detected_input_frequency_hz_ = 0.0F;
    candidate_input_frequency_hz_ = 0.0F;
    pending_input_frequency_hz_ = 0.0F;
    pending_pitch_observations_ = 0U;
    has_valid_pitch_ = false;
    current_shift_semitones_.fill(0.0F);
    target_shift_semitones_.fill(0.0F);
    const std::size_t low_window = static_cast<std::size_t>(std::lround(
        sample_rate_ * (1024.0F / 48000.0F)));
    const std::size_t hop = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::lround(
                sample_rate_ * static_cast<float>(kPitchUpdateHopAt48k) /
                48000.0F)));
    current_control_latency_samples_ = static_cast<std::uint32_t>(
        low_window + 3U * hop + latency_samples());
    for (std::uint8_t voice = 0U; voice < 4U; ++voice) {
        apply_voice(voice);
    }
}

void FractalDiatonicHarmonizer::process(
    std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (!enabled_ || channels != channels_ || channels == 0U) return;
    const std::size_t frames = interleaved.size() / channels_;
    std::size_t offset = 0U;
    while (offset < frames) {
        const std::size_t block_frames = std::min<std::size_t>(
            128U, frames - offset);
        for (std::size_t frame = 0U; frame < block_frames; ++frame) {
            float left = interleaved[(offset + frame) * channels_];
            float right = channels_ == 2U
                ? interleaved[(offset + frame) * channels_ + 1U]
                : left;
            if (!std::isfinite(left)) left = 0.0F;
            if (!std::isfinite(right)) right = 0.0F;
            const std::size_t block_index = frame * channels_;
            source_block_[block_index] = left;
            if (channels_ == 2U) source_block_[block_index + 1U] = right;

            const std::size_t dry_read =
                (dry_write_ + dry_delay_[0].size() - latency_samples()) %
                dry_delay_[0].size();
            dry_block_[block_index] = dry_delay_[0][dry_read];
            dry_delay_[0][dry_write_] = left;
            if (channels_ == 2U) {
                dry_block_[block_index + 1U] = dry_delay_[1][dry_read];
                dry_delay_[1][dry_write_] = right;
            }
            dry_write_ = (dry_write_ + 1U) % dry_delay_[0].size();

            float detected = 0.0F;
            std::uint32_t detector_window = 0U;
            if (tracker_.process_sample(left, right, detected,
                                        detector_window)) {
                candidate_input_frequency_hz_ = detected;
                update_harmonies(detected);
                if (detector_window != 0U) {
                    const std::size_t hop = std::max<std::size_t>(
                        1U, static_cast<std::size_t>(std::lround(
                                sample_rate_ *
                                static_cast<float>(kPitchUpdateHopAt48k) /
                                48000.0F)));
                    const std::uint32_t confirmation_hops =
                        detected >= kFastPathMinimumHz ? 2U : 3U;
                    current_control_latency_samples_ =
                        detector_window +
                        confirmation_hops * static_cast<std::uint32_t>(hop) +
                        latency_samples();
                }
            }
        }

        auto block = interleaved.subspan(
            offset * channels_, block_frames * channels_);
        std::copy_n(source_block_.begin(), block_frames * channels_,
                    block.begin());
        shifter_.process(block, channels_);
        for (std::size_t frame = 0U; frame < block_frames; ++frame) {
            const std::size_t block_index = frame * channels_;
            for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
                const float wet = block[block_index + channel];
                const float dry = dry_block_[block_index + channel];
                const float mixed = dry * dry_gain_ + wet * wet_gain_;
                block[block_index + channel] =
                    std::isfinite(mixed) ? mixed : 0.0F;
            }
        }
        offset += block_frames;
    }
}

}  // namespace tabletfx::effects
