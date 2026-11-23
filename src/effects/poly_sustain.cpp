#include "tabletfx/effects/poly_sustain.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 96000.0F;
constexpr float kMinimumAttackMs = 0.0F;
constexpr float kMaximumAttackMs = 2000.0F;
constexpr float kMinimumDecayMs = 0.0F;
constexpr float kMaximumDecayMs = 10000.0F;
constexpr float kMinimumLevelDb = -24.0F;
constexpr float kMaximumLevelDb = 12.0F;
constexpr float kLoopWindowMs = 80.0F;
constexpr float kSemitoneRange = 12.0F;
constexpr float kAttackTarget = 0.99F;
constexpr float kTailTarget = 0.001F;

float checked_sample_rate(const float sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        throw std::invalid_argument("PolySustain requires an 8–96 kHz sample rate");
    }
    return sample_rate;
}

std::uint16_t checked_channels(const std::uint16_t channels) {
    if (channels == 0U || channels > 2U) {
        throw std::invalid_argument("PolySustain supports mono or stereo only");
    }
    return channels;
}

float bounded(const float requested, const float fallback,
              const float minimum, const float maximum) noexcept {
    return std::isfinite(requested)
        ? std::clamp(requested, minimum, maximum)
        : fallback;
}

}  // namespace

PolySustain::PolySustain(const float sample_rate, const std::uint16_t channels,
                         const PolySustainParameters parameters)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)),
      capture_engine_(sample_rate_, channels_) {
    configure(parameters);
    configure_capture_engine(false);
}

std::size_t PolySustain::memory_bytes() const noexcept {
    return sizeof(*this) + capture_engine_.memory_bytes() - sizeof(capture_engine_);
}

float PolySustain::finite_sample(const float sample) noexcept {
    if (!std::isfinite(sample)) return 0.0F;
    return std::clamp(sample, -kMinimumOutputMagnitude, kMinimumOutputMagnitude);
}

void PolySustain::configure(const PolySustainParameters& requested) noexcept {
    parameters_ = requested;
    parameters_.interval_semitones = bounded(
        parameters_.interval_semitones, 0.0F, -kSemitoneRange, kSemitoneRange);
    parameters_.attack_ms = bounded(
        parameters_.attack_ms, 222.0F, kMinimumAttackMs, kMaximumAttackMs);
    parameters_.decay_ms = bounded(
        parameters_.decay_ms, 2000.0F, kMinimumDecayMs, kMaximumDecayMs);
    parameters_.fx_level = bounded(parameters_.fx_level, 1.0F, 0.0F, 1.0F);
    parameters_.level_db = bounded(
        parameters_.level_db, 0.0F, kMinimumLevelDb, kMaximumLevelDb);
    if (static_cast<unsigned>(parameters_.operation) >
        static_cast<unsigned>(PolySustainOperation::MuteAll)) {
        parameters_.operation = PolySustainOperation::Normal;
    }

    if (parameters_.attack_ms <= 0.0F) {
        attack_coefficient_ = 1.0F;
    } else {
        const double samples = static_cast<double>(parameters_.attack_ms) *
            static_cast<double>(sample_rate_) / 1000.0;
        attack_coefficient_ = static_cast<float>(
            1.0 - std::exp(std::log(1.0 - static_cast<double>(kAttackTarget)) /
                           std::max(1.0, samples)));
    }

    if (parameters_.decay_ms <= 0.0F) {
        decay_coefficient_ = 0.0F;
    } else {
        const double samples = static_cast<double>(parameters_.decay_ms) *
            static_cast<double>(sample_rate_) / 1000.0;
        decay_coefficient_ = static_cast<float>(
            std::exp(std::log(static_cast<double>(kTailTarget)) /
                     std::max(1.0, samples)));
    }
    output_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    configure_capture_engine(holding_);
}

void PolySustain::configure_capture_engine(const bool running) noexcept {
    PitchSequenceParameters capture{};
    capture.steps = 1U;
    capture.run = running;
    capture.mode[0] = PitchSequenceStepMode::Freeze;
    capture.semitones[0] = static_cast<int>(std::lround(
        parameters_.interval_semitones));
    capture.freeze_window_ms = kLoopWindowMs;
    capture.mix = 1.0F;
    capture.level = 1.0F;
    capture_engine_.configure(capture);
}

void PolySustain::process_in_chunks(std::span<float> interleaved,
                                    const bool wet_only) noexcept {
    if (interleaved.size() % channels_ != 0U) return;
    const std::size_t total_frames = interleaved.size() / channels_;
    std::size_t start = 0U;
    while (start < total_frames) {
        const std::size_t frames = std::min(kMaximumFrames, total_frames - start);
        const std::size_t samples = frames * channels_;
        auto chunk = interleaved.subspan(start * channels_, samples);
        for (std::size_t sample = 0U; sample < samples; ++sample) {
            dry_buffer_[sample] = finite_sample(chunk[sample]);
        }
        capture_engine_.process(chunk, channels_);

        for (std::size_t frame = 0U; frame < frames; ++frame) {
            float envelope = 1.0F;
            if (wet_only) {
                if (attack_gain_ < 1.0F) {
                    attack_gain_ += attack_coefficient_ * (1.0F - attack_gain_);
                    attack_gain_ = std::min(attack_gain_, 1.0F);
                }
                envelope = attack_gain_;
            } else {
                envelope = tail_gain_;
                tail_gain_ *= decay_coefficient_;
                if (tail_gain_ < kTailTarget) tail_gain_ = 0.0F;
            }

            for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
                const std::size_t index = frame * channels_ + channel;
                const float dry = dry_buffer_[index];
                const float wet = finite_sample(chunk[index]);
                const float effect = wet * envelope * parameters_.fx_level * output_gain_;
                chunk[index] = finite_sample(dry + effect);
            }
        }
        start += frames;
    }
}

void PolySustain::process(std::span<float> interleaved,
                          const std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U ||
        interleaved.size() % channels_ != 0U) {
        return;
    }
    if (!active_) {
        if (!holding_) {
            configure_capture_engine(true);
            holding_ = true;
            attack_gain_ = 0.0F;
            tail_gain_ = 0.0F;
        } else {
            attack_gain_ = tail_gain_;
        }
        active_ = true;
    }

    const std::size_t total_frames = interleaved.size() / channels_;
    std::size_t start = 0U;
    while (start < total_frames) {
        const std::size_t frames = std::min(kMaximumFrames, total_frames - start);
        const std::size_t samples = frames * channels_;
        auto chunk = interleaved.subspan(start * channels_, samples);
        for (std::size_t sample = 0U; sample < samples; ++sample) {
            dry_buffer_[sample] = finite_sample(chunk[sample]);
        }
        capture_engine_.process(chunk, channels_);

        for (std::size_t frame = 0U; frame < frames; ++frame) {
            if (attack_gain_ < 1.0F) {
                attack_gain_ += attack_coefficient_ * (1.0F - attack_gain_);
                attack_gain_ = std::min(attack_gain_, 1.0F);
            }
            for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
                const std::size_t index = frame * channels_ + channel;
                const float dry = dry_buffer_[index];
                const float wet = finite_sample(chunk[index]) * attack_gain_ *
                                  parameters_.fx_level * output_gain_;
                switch (parameters_.operation) {
                    case PolySustainOperation::Normal:
                        chunk[index] = finite_sample((dry + wet) * output_gain_);
                        break;
                    case PolySustainOperation::DryKill:
                        chunk[index] = finite_sample(wet);
                        break;
                    case PolySustainOperation::MuteAll:
                        chunk[index] = 0.0F;
                        break;
                }
            }
        }
        start += frames;
    }
}

void PolySustain::process_bypassed(std::span<float> interleaved,
                                   const std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U ||
        interleaved.size() % channels_ != 0U) {
        return;
    }
    if (active_) {
        active_ = false;
        if (holding_) tail_gain_ = attack_gain_;
    }

    if (!holding_) {
        const std::size_t total_frames = interleaved.size() / channels_;
        std::size_t start = 0U;
        while (start < total_frames) {
            const std::size_t frames = std::min(kMaximumFrames, total_frames - start);
            const std::size_t samples = frames * channels_;
            auto chunk = interleaved.subspan(start * channels_, samples);
            for (std::size_t sample = 0U; sample < samples; ++sample) {
                dry_buffer_[sample] = finite_sample(chunk[sample]);
            }
            capture_engine_.record_input_history(chunk, channels_);
            std::copy_n(dry_buffer_.begin(), samples, chunk.begin());
            start += frames;
        }
        return;
    }

    process_in_chunks(interleaved, false);
    if (tail_gain_ == 0.0F) {
        capture_engine_.stop();
        holding_ = false;
        attack_gain_ = 0.0F;
        configure_capture_engine(false);
    }
}

void PolySustain::reset() noexcept {
    capture_engine_.stop();
    capture_engine_.reset();
    active_ = false;
    holding_ = false;
    attack_gain_ = 0.0F;
    tail_gain_ = 0.0F;
}

}  // namespace tabletfx::effects
