#include "tabletfx/effects/diatonic_harmonizer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tabletfx::effects {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kMinSampleRate = 8000.0F;
constexpr float kMaxSampleRate = 96000.0F;
constexpr float kMinToneHz = 500.0F;
constexpr float kMaxToneHz = 12000.0F;
constexpr float kMaxDelayMs = 600.0F;
constexpr float kMinTempoBpm = 30.0F;
constexpr float kMaxTempoBpm = 300.0F;
constexpr float kMaxFeedback = 0.85F;
constexpr float kMinTrackedHz = 55.0F;
constexpr float kMaxTrackedHz = 1100.0F;
constexpr float kMinPitchConfidence = 0.62F;
constexpr float kMaxPitchDifference = 1.0F - kMinPitchConfidence;

float clamp_finite(float value, float fallback, float low, float high) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, low, high);
}

bool valid_scale(DiatonicScale scale) noexcept {
    return static_cast<unsigned>(scale) <=
           static_cast<unsigned>(DiatonicScale::Chromatic);
}

bool valid_tap_division(TapDivision division) noexcept {
    return static_cast<unsigned>(division) <=
           static_cast<unsigned>(TapDivision::Sixteenth);
}

std::uint16_t scale_mask(DiatonicScale scale) noexcept {
    switch (scale) {
        case DiatonicScale::Major: return 0x0AB5U;
        case DiatonicScale::NaturalMinor: return 0x05ADU;
        case DiatonicScale::Dorian: return 0x06ADU;
        case DiatonicScale::Phrygian: return 0x05ABU;
        case DiatonicScale::Lydian: return 0x0AD5U;
        case DiatonicScale::Mixolydian: return 0x06B5U;
        case DiatonicScale::Locrian: return 0x056BU;
        case DiatonicScale::HarmonicMinor: return 0x09ADU;
        case DiatonicScale::MelodicMinor: return 0x0AADU;
        case DiatonicScale::MajorPentatonic: return 0x0295U;
        case DiatonicScale::MinorPentatonic: return 0x04A9U;
        case DiatonicScale::Blues: return 0x04E9U;
        case DiatonicScale::Chromatic: return 0x0FFFU;
    }
    return 0x0AB5U;
}

bool note_is_in_scale(int midi_note, int key, DiatonicScale scale) noexcept {
    const int pc = ((midi_note - key) % 12 + 12) % 12;
    return (scale_mask(scale) & (1U << pc)) != 0U;
}

int nearest_scale_note(int midi_note, int key, DiatonicScale scale) noexcept {
    int best_note = midi_note;
    int best_distance = std::numeric_limits<int>::max();
    for (int candidate = midi_note - 6; candidate <= midi_note + 6; ++candidate) {
        if (!note_is_in_scale(candidate, key, scale)) {
            continue;
        }
        const int distance = std::abs(candidate - midi_note);
        if (distance < best_distance ||
            (distance == best_distance && candidate < best_note)) {
            best_note = candidate;
            best_distance = distance;
        }
    }
    return best_note;
}

int move_scale_degrees(int root_note,
                       int key,
                       DiatonicScale scale,
                       int interval_steps) noexcept {
    int note = root_note;
    const int direction = interval_steps < 0 ? -1 : 1;
    int remaining = std::abs(interval_steps);
    while (remaining > 0) {
        note += direction;
        if (note_is_in_scale(note, key, scale)) {
            --remaining;
        }
    }
    return note;
}

float tap_multiplier(TapDivision division) noexcept {
    switch (division) {
        case TapDivision::Free: return 0.0F;
        case TapDivision::Half: return 2.0F;
        case TapDivision::Quarter: return 1.0F;
        case TapDivision::DottedQuarter: return 1.5F;
        case TapDivision::QuarterTriplet: return 2.0F / 3.0F;
        case TapDivision::Eighth: return 0.5F;
        case TapDivision::DottedEighth: return 0.75F;
        case TapDivision::EighthTriplet: return 1.0F / 3.0F;
        case TapDivision::Sixteenth: return 0.25F;
    }
    return 0.0F;
}

}  // namespace

DiatonicHarmonizer::DiatonicHarmonizer(float sample_rate)
    : sample_rate_(clamp_finite(sample_rate, 48000.0F,
                                kMinSampleRate, kMaxSampleRate)),
      pitch_shifter_(sample_rate_, 2),
      detector_rate_(0.0F),
      decimation_(1),
      analysis_latency_samples_(0),
      delay_samples_(0) {
    decimation_ = static_cast<std::size_t>(
        std::ceil(sample_rate_ / 24000.0F));
    decimation_ = std::max<std::size_t>(1, decimation_);
    detector_rate_ = sample_rate_ / static_cast<float>(decimation_);
    analysis_latency_samples_ = kDetectorFrame * decimation_;
    update_derived_parameters();
}

void DiatonicHarmonizer::configure(
    const DiatonicHarmonizerConfig& requested) noexcept {
    const bool old_spillover = config_.spillover;
    config_ = requested;
    config_.mix = clamp_finite(config_.mix, 0.35F, 0.0F, 1.0F);
    config_.pitch_interval = std::clamp(config_.pitch_interval, -7, 7);
    config_.key = std::clamp(config_.key, 0, 11);
    if (!valid_scale(config_.scale)) {
        config_.scale = DiatonicScale::Major;
    }
    config_.tone_hz = clamp_finite(config_.tone_hz, 8000.0F,
                                   kMinToneHz, kMaxToneHz);
    config_.delay_ms = clamp_finite(config_.delay_ms, 0.0F,
                                    0.0F, kMaxDelayMs);
    config_.feedback = clamp_finite(config_.feedback, 0.2F,
                                    0.0F, kMaxFeedback);
    config_.tempo_bpm = clamp_finite(config_.tempo_bpm, 120.0F,
                                     kMinTempoBpm, kMaxTempoBpm);
    if (!valid_tap_division(config_.tap_division)) {
        config_.tap_division = TapDivision::Free;
    }
    update_derived_parameters();
    if (!config_.spillover && old_spillover && !enabled_) {
        reset();
    }
}

void DiatonicHarmonizer::set_enabled(bool enabled) noexcept {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    if (!enabled_ && !config_.spillover) {
        reset();
    }
}

void DiatonicHarmonizer::reset() noexcept {
    pitch_shifter_.reset();
    pitch_shifter_.set_pitch_semitones(0.0F);
    for (auto& channel : dry_delay_) {
        channel.fill(0.0F);
    }
    clear_delay();
    for (auto& channel : detector_frame_) {
        channel.fill(0.0F);
    }
    dry_write_ = 0;
    delay_write_ = 0;
    detector_write_ = 0;
    detector_count_ = 0;
    detector_hop_count_ = 0;
    decimation_count_ = 0;
    lost_pitch_samples_ = 0;
    decimation_sum_[0] = 0.0F;
    decimation_sum_[1] = 0.0F;
    target_ratio_ = 1.0F;
    smoothed_ratio_ = 1.0F;
    confidence_ = 0.0F;
    filter_state_[0] = 0.0F;
    filter_state_[1] = 0.0F;
    had_valid_pitch_ = false;
}

void DiatonicHarmonizer::process(const float* input_left,
                                const float* input_right,
                                float* output_left,
                                float* output_right,
                                std::size_t frames) noexcept {
    if (output_left == nullptr) {
        return;
    }

    const bool stereo = input_right != nullptr;
    const float pitch_update_smoothing = 1.0F - std::exp(
        -static_cast<float>(kDetectorHop * decimation_) /
         (0.015F * sample_rate_));
    std::array<float, 2> shifted_pair{};

    for (std::size_t frame = 0; frame < frames; ++frame) {
        float left = input_left == nullptr ? 0.0F : input_left[frame];
        float right = stereo ? input_right[frame] : 0.0F;
        if (!std::isfinite(left)) {
            left = 0.0F;
        }
        if (!std::isfinite(right)) {
            right = 0.0F;
        }

        bool pitch_measurement = false;
        decimation_sum_[0] += left;
        decimation_sum_[1] += right;
        ++decimation_count_;
        if (decimation_count_ >= decimation_) {
            const float divisor = static_cast<float>(decimation_);
            detector_frame_[0][detector_write_] = decimation_sum_[0] / divisor;
            detector_frame_[1][detector_write_] = decimation_sum_[1] / divisor;
            decimation_sum_[0] = 0.0F;
            decimation_sum_[1] = 0.0F;
            decimation_count_ = 0;
            detector_write_ = (detector_write_ + 1) % kDetectorFrame;
            detector_count_ = std::min(detector_count_ + 1, kDetectorFrame);
            ++detector_hop_count_;
            if (detector_count_ == kDetectorFrame &&
                detector_hop_count_ >= kDetectorHop) {
                detect_pitch();
                detector_hop_count_ = 0;
                pitch_measurement = true;
            }
        }

        if (pitch_measurement) {
            smoothed_ratio_ += pitch_update_smoothing *
                               (target_ratio_ - smoothed_ratio_);
            if (!std::isfinite(smoothed_ratio_)) {
                smoothed_ratio_ = 1.0F;
            }
            smoothed_ratio_ = std::clamp(smoothed_ratio_, 0.5F, 2.0F);
            const float semitones = 12.0F * std::log2(smoothed_ratio_);
            if (std::isfinite(semitones)) {
                pitch_shifter_.set_pitch_semitones(semitones);
            }
        }

        shifted_pair[0] = enabled_ ? left : 0.0F;
        shifted_pair[1] = enabled_ ? right : 0.0F;
        pitch_shifter_.process(shifted_pair, 2);
        const std::size_t dry_read =
            (dry_write_ + kDryDelaySize - kPitchLatencySamples) % kDryDelaySize;
        for (std::size_t channel = 0; channel < 2; ++channel) {
            const float source = channel == 0 ? left : right;
            const float dry = dry_delay_[channel][dry_read];
            dry_delay_[channel][dry_write_] = source;
            float wet = 0.0F;
            float tail = 0.0F;
            if (enabled_) {
                float shifted = shifted_pair[channel];
                if (!std::isfinite(shifted)) {
                    shifted = 0.0F;
                }
                filter_state_[channel] += tone_alpha_ *
                    (shifted - filter_state_[channel]);
                if (!std::isfinite(filter_state_[channel])) {
                    filter_state_[channel] = 0.0F;
                }

                const float previous = delay_samples_ > 0
                    ? delayed_sample(channel, delay_samples_)
                    : 0.0F;
                const float echo = delay_samples_ > 0 ? previous : 0.0F;
                const float written = filter_state_[channel] + previous * feedback_;
                delay_line_[channel][delay_write_] =
                    std::isfinite(written) ? written : 0.0F;
                wet = filter_state_[channel] + echo;
            } else if (config_.spillover && delay_samples_ > 0) {
                const float previous = delayed_sample(channel, delay_samples_);
                const float written = previous * feedback_;
                delay_line_[channel][delay_write_] =
                    std::isfinite(written) ? written : 0.0F;
                tail = previous;
            } else {
                delay_line_[channel][delay_write_] = 0.0F;
            }

            const float output = enabled_
                ? dry * mix_dry_gain_ + wet * mix_wet_gain_
                : dry + tail * mix_wet_gain_;
            const float finite_output = std::isfinite(output) ? output : 0.0F;
            if (channel == 0) {
                output_left[frame] = finite_output;
            } else if (output_right != nullptr) {
                output_right[frame] = finite_output;
            }
        }

        dry_write_ = (dry_write_ + 1) % kDryDelaySize;
        delay_write_ = (delay_write_ + 1) % kDelaySize;
    }
}

void DiatonicHarmonizer::process(std::span<float> interleaved,
                                 std::uint16_t channels) noexcept {
    if (channels == 0 || channels > 2) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        float input_left = interleaved[offset];
        float input_right = channels == 2 ? interleaved[offset + 1] : 0.0F;
        float output_left = 0.0F;
        float output_right = 0.0F;
        process(&input_left, channels == 2 ? &input_right : nullptr,
                &output_left, channels == 2 ? &output_right : nullptr, 1);
        interleaved[offset] = output_left;
        if (channels == 2) {
            interleaved[offset + 1] = output_right;
        }
    }
}

std::size_t DiatonicHarmonizer::memory_bytes() const noexcept {
    return sizeof(*this) + pitch_shifter_.memory_bytes() - sizeof(pitch_shifter_);
}

std::size_t DiatonicHarmonizer::latency_samples() const noexcept {
    return static_cast<std::size_t>(std::lround(
        pitch_shifter_.algorithmic_latency_ms() * sample_rate_ / 1000.0F));
}

float DiatonicHarmonizer::latency_ms() const noexcept {
    return pitch_shifter_.algorithmic_latency_ms();
}

int DiatonicHarmonizer::harmony_midi_note(int midi_note,
                                         int key,
                                         DiatonicScale scale,
                                         int interval_steps) noexcept {
    key = std::clamp(key, 0, 11);
    if (!valid_scale(scale)) {
        scale = DiatonicScale::Major;
    }
    interval_steps = std::clamp(interval_steps, -7, 7);
    const int scale_root = nearest_scale_note(midi_note, key, scale);
    return move_scale_degrees(scale_root, key, scale, interval_steps);
}

const char* DiatonicHarmonizer::scale_name(DiatonicScale scale) noexcept {
    switch (scale) {
        case DiatonicScale::Major: return "Major";
        case DiatonicScale::NaturalMinor: return "Natural Minor";
        case DiatonicScale::Dorian: return "Dorian";
        case DiatonicScale::Phrygian: return "Phrygian";
        case DiatonicScale::Lydian: return "Lydian";
        case DiatonicScale::Mixolydian: return "Mixolydian";
        case DiatonicScale::Locrian: return "Locrian";
        case DiatonicScale::HarmonicMinor: return "Harmonic Minor";
        case DiatonicScale::MelodicMinor: return "Melodic Minor";
        case DiatonicScale::MajorPentatonic: return "Major Pentatonic";
        case DiatonicScale::MinorPentatonic: return "Minor Pentatonic";
        case DiatonicScale::Blues: return "Blues";
        case DiatonicScale::Chromatic: return "Chromatic";
    }
    return "Major";
}

void DiatonicHarmonizer::detect_pitch() noexcept {
    if (detector_count_ < kDetectorFrame) {
        return;
    }

    std::array<float, kDetectorFrame> left{};
    std::array<float, kDetectorFrame> right{};
    double mean_left = 0.0;
    double mean_right = 0.0;
    for (std::size_t i = 0; i < kDetectorFrame; ++i) {
        const std::size_t index = (detector_write_ + i) % kDetectorFrame;
        left[i] = detector_frame_[0][index];
        right[i] = detector_frame_[1][index];
        mean_left += left[i];
        mean_right += right[i];
    }
    mean_left /= static_cast<double>(kDetectorFrame);
    mean_right /= static_cast<double>(kDetectorFrame);

    double energy = 0.0;
    for (std::size_t i = 0; i < kDetectorFrame; ++i) {
        left[i] -= static_cast<float>(mean_left);
        right[i] -= static_cast<float>(mean_right);
        energy += static_cast<double>(left[i]) * left[i] +
                  static_cast<double>(right[i]) * right[i];
    }
    const double rms = std::sqrt(energy / (2.0 * kDetectorFrame));
    if (!(rms > 0.00025) || !std::isfinite(rms)) {
        confidence_ = 0.0F;
        lost_pitch_samples_ += kDetectorHop * decimation_;
        if (had_valid_pitch_ &&
            lost_pitch_samples_ > static_cast<std::size_t>(sample_rate_ * 0.10F)) {
            target_ratio_ = 1.0F;
            had_valid_pitch_ = false;
        }
        return;
    }

    std::array<float, kDetectorFrame> difference{};
    const std::size_t min_lag = std::max<std::size_t>(
        2, static_cast<std::size_t>(detector_rate_ / kMaxTrackedHz));
    const std::size_t max_lag = std::min<std::size_t>(
        kDetectorFrame - 32,
        static_cast<std::size_t>(detector_rate_ / kMinTrackedHz));
    if (max_lag <= min_lag + 2) {
        return;
    }

    for (std::size_t lag = min_lag; lag <= max_lag; ++lag) {
        double sum = 0.0;
        const std::size_t count = kDetectorFrame - lag;
        for (std::size_t i = 0; i < count; ++i) {
            const double dl = static_cast<double>(left[i]) - left[i + lag];
            const double dr = static_cast<double>(right[i]) - right[i + lag];
            sum += dl * dl + dr * dr;
        }
        difference[lag] = static_cast<float>(
            sum / (2.0 * static_cast<double>(count)));
    }

    double cumulative = 0.0;
    float best_cmnd = 1.0F;
    std::size_t best_lag = 0;
    std::size_t selected_lag = 0;
    for (std::size_t lag = min_lag; lag <= max_lag; ++lag) {
        cumulative += difference[lag];
        if (cumulative <= 1.0e-18) {
            continue;
        }
        const float cmnd = static_cast<float>(
            difference[lag] * static_cast<double>(lag) / cumulative);
        if (cmnd < best_cmnd) {
            best_cmnd = cmnd;
            best_lag = lag;
        }
        if (cmnd < 0.18F && lag + 1 <= max_lag &&
            difference[lag] <= difference[lag + 1]) {
            selected_lag = lag;
            break;
        }
    }
    if (selected_lag == 0) {
        selected_lag = best_lag;
    }
    if (selected_lag == 0 || best_cmnd > kMaxPitchDifference) {
        confidence_ = 1.0F - best_cmnd;
        lost_pitch_samples_ += kDetectorHop * decimation_;
        if (had_valid_pitch_ &&
            lost_pitch_samples_ > static_cast<std::size_t>(sample_rate_ * 0.10F)) {
            target_ratio_ = 1.0F;
            had_valid_pitch_ = false;
        }
        return;
    }

    float refined_lag = static_cast<float>(selected_lag);
    if (selected_lag > min_lag && selected_lag < max_lag) {
        const float before = difference[selected_lag - 1];
        const float center = difference[selected_lag];
        const float after = difference[selected_lag + 1];
        const float denominator = before - 2.0F * center + after;
        if (std::abs(denominator) > 1.0e-9F) {
            const float correction = 0.5F * (before - after) / denominator;
            refined_lag += std::clamp(correction, -0.5F, 0.5F);
        }
    }
    if (!(refined_lag > 0.0F) || !std::isfinite(refined_lag)) {
        return;
    }
    const float frequency = detector_rate_ / refined_lag;
    if (!(frequency >= kMinTrackedHz && frequency <= kMaxTrackedHz)) {
        return;
    }

    const float midi = 69.0F + 12.0F * std::log2(frequency / 440.0F);
    if (!std::isfinite(midi)) {
        return;
    }
    const int nearest_midi = static_cast<int>(std::floor(midi + 0.5F));
    const int source_scale_note = nearest_scale_note(
        nearest_midi, config_.key, config_.scale);
    const int target_note = move_scale_degrees(
        source_scale_note, config_.key, config_.scale, config_.pitch_interval);
    // The interval is applied to the detected input pitch, not the snapped
    // scale note. Including the correction from an out-of-scale input note
    // keeps the generated voice on the intended target scale degree.
    const float semitone_shift = static_cast<float>(target_note - nearest_midi);
    target_ratio_ = std::clamp(std::exp2(semitone_shift / 12.0F), 0.5F, 2.0F);
    if (!std::isfinite(target_ratio_)) {
        target_ratio_ = 1.0F;
    }
    confidence_ = 1.0F - best_cmnd;
    lost_pitch_samples_ = 0;
    had_valid_pitch_ = true;
}

float DiatonicHarmonizer::delayed_sample(std::size_t channel,
                                         std::size_t age) const noexcept {
    age = std::clamp<std::size_t>(age, 1, kDelaySize - 1);
    const std::size_t index =
        (delay_write_ + kDelaySize - (age % kDelaySize)) % kDelaySize;
    return delay_line_[channel][index];
}

void DiatonicHarmonizer::clear_delay() noexcept {
    for (auto& channel : delay_line_) {
        channel.fill(0.0F);
    }
    delay_write_ = 0;
}

void DiatonicHarmonizer::update_derived_parameters() noexcept {
    tone_alpha_ = 1.0F - std::exp(-2.0F * kPi * config_.tone_hz / sample_rate_);
    tone_alpha_ = std::clamp(tone_alpha_, 0.0F, 1.0F);
    // A linear crossfade keeps a neutral/unison setting at unity when the dry
    // and shifted paths are correlated.
    mix_dry_gain_ = 1.0F - config_.mix;
    mix_wet_gain_ = config_.mix;
    feedback_ = config_.feedback;

    float requested_delay_ms = config_.delay_ms;
    if (config_.tap_division != TapDivision::Free) {
        requested_delay_ms = (60000.0F / config_.tempo_bpm) *
                             tap_multiplier(config_.tap_division);
    }
    requested_delay_ms = std::clamp(requested_delay_ms, 0.0F, kMaxDelayMs);
    delay_samples_ = static_cast<std::size_t>(
        std::lround(requested_delay_ms * sample_rate_ / 1000.0F));
    delay_samples_ = std::min(delay_samples_, kDelaySize - 1);
}

}  // namespace tabletfx::effects
