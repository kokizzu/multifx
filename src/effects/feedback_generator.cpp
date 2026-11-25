#include "tabletfx/effects/feedback_generator.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {
namespace {

float checked_sample_rate(float sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate < 8000.0F ||
        sample_rate > 96000.0F) {
        throw std::invalid_argument("feedback generator sample rate is unsupported");
    }
    return sample_rate;
}

std::uint16_t checked_channels(std::uint16_t channels) {
    if (channels == 0U || channels > 2U) {
        throw std::invalid_argument("feedback generator supports mono or stereo");
    }
    return channels;
}

constexpr float kMaximumInputMagnitude = 1000000.0F;
constexpr float kMinimumTrackedHz = 55.0F;
constexpr float kMaximumTrackedHz = 1100.0F;
constexpr float kTau = 6.28318530717958647692F;

}  // namespace

FeedbackGenerator::FeedbackGenerator(
    float sample_rate, std::uint16_t channels,
    FeedbackGeneratorParameters parameters)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)),
      synth_(sample_rate_),
      dry_buffer_(kMaximumChunkFrames * channels_, 0.0F) {
    configure(parameters);
}

std::size_t FeedbackGenerator::memory_bytes() const noexcept {
    return sizeof(*this) + dry_buffer_.capacity() * sizeof(float) +
           synth_.memory_bytes() - sizeof(synth_);
}

float FeedbackGenerator::bounded(float value, float fallback,
                                 float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

bool FeedbackGenerator::valid_type(FeedbackGeneratorType type) noexcept {
    return static_cast<unsigned>(type) <=
           static_cast<unsigned>(FeedbackGeneratorType::TwoOctavesAndSeventh);
}

bool FeedbackGenerator::valid_dry_kill(
    FeedbackGeneratorDryKill dry_kill) noexcept {
    return static_cast<unsigned>(dry_kill) <=
           static_cast<unsigned>(FeedbackGeneratorDryKill::On);
}

float FeedbackGenerator::finite_input(float sample) noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return std::clamp(sample, -kMaximumInputMagnitude,
                      kMaximumInputMagnitude);
}

float FeedbackGenerator::interval_for_type() const noexcept {
    switch (parameters_.type) {
        case FeedbackGeneratorType::OctaveDown:
            return -12.0F;
        case FeedbackGeneratorType::Unison:
            return 0.0F;
        case FeedbackGeneratorType::OctaveUp:
            return 12.0F;
        case FeedbackGeneratorType::OctaveAndFifth:
            return 19.0F;
        case FeedbackGeneratorType::TwoOctaves:
            return 24.0F;
        case FeedbackGeneratorType::TwoOctavesAndThird:
            return 28.0F;
        case FeedbackGeneratorType::TwoOctavesAndFifth:
            return 31.0F;
        case FeedbackGeneratorType::TwoOctavesAndSeventh:
            return 35.0F;
    }
    return 0.0F;
}

float FeedbackGenerator::pitch_glide_coefficient() const noexcept {
    return 1.0F - std::exp(
        -1000.0F / (kPitchGlideMs * sample_rate_));
}

float FeedbackGenerator::oscillator_sample(float frequency_hz) noexcept {
    if (!(frequency_hz > 0.0F) || !std::isfinite(frequency_hz)) {
        return 0.0F;
    }
    const float limited_frequency = std::clamp(
        frequency_hz, 0.0F, sample_rate_ * 0.45F);
    const float sample = std::sin(kTau * static_cast<float>(phase_));
    phase_ += static_cast<double>(limited_frequency / sample_rate_);
    phase_ -= std::floor(phase_);
    return std::isfinite(sample) ? sample : 0.0F;
}

void FeedbackGenerator::configure(
    const FeedbackGeneratorParameters& requested) noexcept {
    parameters_ = requested;
    if (!valid_type(parameters_.type)) {
        parameters_.type = FeedbackGeneratorType::Unison;
    }
    if (!valid_dry_kill(parameters_.dry_kill)) {
        parameters_.dry_kill = FeedbackGeneratorDryKill::Off;
    }
    parameters_.feedback_gain = bounded(
        parameters_.feedback_gain, 0.35F, 0.0F, 1.0F);
    parameters_.attack_ms = bounded(
        parameters_.attack_ms, 15.0F, 0.0F, 2000.0F);
    parameters_.release_ms = bounded(
        parameters_.release_ms, 600.0F, 0.0F, 5000.0F);
    parameters_.dry_level = bounded(
        parameters_.dry_level, 1.0F, 0.0F, 1.0F);
    parameters_.silence_threshold_db = bounded(
        parameters_.silence_threshold_db, -48.0F, -90.0F, 0.0F);

    MonoGuitarSynthConfig synth_parameters;
    synth_parameters.waveform = MonoGuitarSynthWaveform::Sine;
    synth_parameters.mix = 0.0F;
    synth_parameters.level = 0.0F;
    synth_parameters.gate_threshold_db = parameters_.silence_threshold_db;
    synth_parameters.attack_ms = parameters_.attack_ms;
    synth_parameters.release_ms = parameters_.release_ms;
    synth_parameters.portamento_ms = kPitchGlideMs;
    synth_.configure(synth_parameters);
    pitch_interval_semitones_ = interval_for_type();
    pitch_glide_coefficient_ = pitch_glide_coefficient();
}

void FeedbackGenerator::reset() noexcept {
    synth_.reset();
    current_frequency_hz_ = 0.0F;
    target_frequency_hz_ = 0.0F;
    phase_ = 0.0;
    std::fill(dry_buffer_.begin(), dry_buffer_.end(), 0.0F);
}

void FeedbackGenerator::process(std::span<float> interleaved,
                                std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U || channels > 2U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    const float dry_gain = parameters_.dry_kill == FeedbackGeneratorDryKill::On
        ? 0.0F
        : parameters_.dry_level;
    const float generated_gain = parameters_.feedback_gain;
    const float pitch_ratio = std::exp2(pitch_interval_semitones_ / 12.0F);

    for (std::size_t frame_offset = 0U; frame_offset < frames;
         frame_offset += kMaximumChunkFrames) {
        const std::size_t chunk_frames = std::min(
            kMaximumChunkFrames, frames - frame_offset);
        const std::size_t chunk_samples = chunk_frames * channels;
        const std::size_t sample_offset = frame_offset * channels;

        for (std::size_t index = 0; index < chunk_samples; ++index) {
            const float input = finite_input(interleaved[sample_offset + index]);
            dry_buffer_[index] = input;
            interleaved[sample_offset + index] = input;
        }

        for (std::size_t frame = 0U; frame < chunk_frames; ++frame) {
            const std::size_t frame_sample_offset = sample_offset + frame * channels;
            std::span<float> tracker_frame(
                interleaved.data() + frame_sample_offset, channels);
            synth_.process(tracker_frame, channels);

            float detected_frequency = synth_.detected_input_frequency_hz();
            if (std::isfinite(detected_frequency) && detected_frequency > 0.0F) {
                target_frequency_hz_ = std::clamp(
                    detected_frequency, kMinimumTrackedHz, kMaximumTrackedHz);
                if (!(current_frequency_hz_ > 0.0F)) {
                    current_frequency_hz_ = target_frequency_hz_;
                } else {
                    current_frequency_hz_ += pitch_glide_coefficient_ *
                        (target_frequency_hz_ - current_frequency_hz_);
                }
            } else if (current_frequency_hz_ > 0.0F) {
                current_frequency_hz_ += pitch_glide_coefficient_ *
                    (target_frequency_hz_ - current_frequency_hz_);
            }

            if (!std::isfinite(current_frequency_hz_) ||
                current_frequency_hz_ < 0.0F) {
                current_frequency_hz_ = 0.0F;
                target_frequency_hz_ = 0.0F;
            }
            const float envelope = std::clamp(synth_.envelope(), 0.0F, 1.0F);
            const float oscillator = oscillator_sample(
                current_frequency_hz_ * pitch_ratio);
            const float generated = oscillator * envelope * generated_gain;

            for (std::uint16_t channel = 0U; channel < channels; ++channel) {
                const std::size_t index = frame_sample_offset + channel;
                const double output =
                    static_cast<double>(dry_buffer_[frame * channels + channel]) *
                        dry_gain +
                    static_cast<double>(generated);
                interleaved[index] = static_cast<float>(std::clamp(
                    output, -static_cast<double>(kSafetyCeiling),
                    static_cast<double>(kSafetyCeiling)));
            }
        }
    }
}

}  // namespace tabletfx::effects
