#include "tabletfx/effects/feedback_generator.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {
namespace {

constexpr float kMaximumInputMagnitude = 1000000.0F;

}  // namespace

FeedbackGenerator::FeedbackGenerator(
    float sample_rate, std::uint16_t channels,
    FeedbackGeneratorParameters parameters)
    : channels_(channels),
      synth_(sample_rate),
      pitch_shifter_(sample_rate, channels),
      dry_buffer_(kMaximumChunkFrames * channels, 0.0F),
      generated_buffer_(kMaximumChunkFrames * channels, 0.0F) {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        throw std::invalid_argument("feedback generator sample rate is unsupported");
    }
    if (channels == 0U || channels > 2U) {
        throw std::invalid_argument("feedback generator supports mono or stereo");
    }
    configure(parameters);
}

std::size_t FeedbackGenerator::memory_bytes() const noexcept {
    return sizeof(*this) +
           (dry_buffer_.capacity() + generated_buffer_.capacity()) *
               sizeof(float) +
           synth_.memory_bytes() - sizeof(synth_) +
           pitch_shifter_.memory_bytes() - sizeof(pitch_shifter_);
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
           static_cast<unsigned>(FeedbackGeneratorType::OctaveUp);
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

float FeedbackGenerator::pitch_for_type() const noexcept {
    switch (parameters_.type) {
        case FeedbackGeneratorType::OctaveDown:
            return -12.0F;
        case FeedbackGeneratorType::Unison:
            return 0.0F;
        case FeedbackGeneratorType::OctaveUp:
            return 12.0F;
    }
    return 0.0F;
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
    synth_parameters.mix = 1.0F;
    synth_parameters.level = 1.0F;
    synth_parameters.gate_threshold_db = parameters_.silence_threshold_db;
    synth_parameters.attack_ms = parameters_.attack_ms;
    synth_parameters.release_ms = parameters_.release_ms;
    synth_parameters.portamento_ms = 0.0F;
    synth_.configure(synth_parameters);
    pitch_shifter_.set_pitch_semitones(pitch_for_type());
}

void FeedbackGenerator::reset() noexcept {
    synth_.reset();
    pitch_shifter_.reset();
    std::fill(dry_buffer_.begin(), dry_buffer_.end(), 0.0F);
    std::fill(generated_buffer_.begin(), generated_buffer_.end(), 0.0F);
}

void FeedbackGenerator::process(std::span<float> interleaved,
                                std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U || channels > 2U) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    std::size_t frame_offset = 0U;
    while (frame_offset < frames) {
        const std::size_t chunk_frames = std::min(
            kMaximumChunkFrames, frames - frame_offset);
        const std::size_t chunk_samples = chunk_frames * channels;
        const std::size_t sample_offset = frame_offset * channels;
        auto dry = std::span<float>(dry_buffer_.data(), chunk_samples);
        auto generated = std::span<float>(generated_buffer_.data(), chunk_samples);

        for (std::size_t index = 0; index < chunk_samples; ++index) {
            const float input = finite_input(interleaved[sample_offset + index]);
            dry[index] = input;
            generated[index] = input;
        }

        synth_.process(generated, channels);
        if (parameters_.type != FeedbackGeneratorType::Unison) {
            pitch_shifter_.process(generated, channels);
        }

        const float dry_gain = parameters_.dry_kill == FeedbackGeneratorDryKill::On
            ? 0.0F
            : parameters_.dry_level;
        const float generated_gain = parameters_.feedback_gain;
        for (std::size_t index = 0; index < chunk_samples; ++index) {
            const double output = static_cast<double>(dry[index]) * dry_gain +
                static_cast<double>(finite_input(generated[index])) * generated_gain;
            const float bounded_output = static_cast<float>(std::clamp(
                output, -static_cast<double>(kSafetyCeiling),
                static_cast<double>(kSafetyCeiling)));
            interleaved[sample_offset + index] = bounded_output;
        }
        frame_offset += chunk_frames;
    }
}

}  // namespace tabletfx::effects
