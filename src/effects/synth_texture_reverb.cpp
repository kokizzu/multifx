#include "tabletfx/effects/synth_texture_reverb.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {
namespace {

constexpr float kTwoPi = 6.28318530718F;
constexpr float kInternalLimit = 64.0F;
constexpr float kOutputLimit = 64.0F;
constexpr float kControlRampMs = 20.0F;
constexpr float kAttackMs = 3.0F;
constexpr float kInjectionGain = 0.25F;
constexpr float kStereoMix = 0.18F;
constexpr std::array<float, 4U> kDelayMs{17.3F, 23.9F, 31.1F, 37.7F};
constexpr std::array<float, 4U> kStereoDelayOffsetMs{0.0F, 0.43F};
constexpr std::array<double, 4U> kTextureFrequenciesHz{
    98.0, 146.83, 220.0, 329.63};
constexpr std::array<float, 4U> kTextureWeights{0.37F, 0.28F, 0.21F, 0.14F};

std::size_t samples_for_ms(float sample_rate, float milliseconds) noexcept {
    const double samples = static_cast<double>(sample_rate) *
                           static_cast<double>(milliseconds) / 1000.0;
    return std::max<std::size_t>(1U,
        static_cast<std::size_t>(std::llround(samples)));
}

float finite_sample(float value) noexcept {
    if (!std::isfinite(value)) return 0.0F;
    return std::clamp(value, -8.0F, 8.0F);
}

}  // namespace

SynthTextureReverb::SynthTextureReverb(
    float sample_rate, std::uint16_t channels,
    SynthTextureReverbParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
        sample_rate_ > 192000.0F) {
        throw std::invalid_argument(
            "Synth texture reverb sample rate must be between 8 kHz and 192 kHz");
    }
    if (channels_ != 1U && channels_ != 2U) {
        throw std::invalid_argument(
            "Synth texture reverb supports mono or stereo only");
    }

    for (std::size_t index = 0U; index < kTextureFrequenciesHz.size(); ++index) {
        oscillator_increment_[index] =
            static_cast<double>(kTwoPi) * kTextureFrequenciesHz[index] /
            static_cast<double>(sample_rate_);
    }

    std::size_t storage_samples = 0U;
    for (std::size_t channel = 0U; channel < channels_; ++channel) {
        for (std::size_t index = 0U; index < kLinesPerChannel; ++index) {
            auto& line = lines_[channel * kLinesPerChannel + index];
            line.offset = storage_samples;
            line.length = samples_for_ms(
                sample_rate_, kDelayMs[index] + kStereoDelayOffsetMs[channel]);
            storage_samples += line.length;
        }
    }
    storage_.assign(storage_samples, 0.0F);

    const double ramp_samples = static_cast<double>(sample_rate_) *
                                static_cast<double>(kControlRampMs) / 1000.0;
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(ramp_samples)));
    configure(parameters);
    settle_parameters();
}

std::size_t SynthTextureReverb::memory_bytes() const noexcept {
    return sizeof(*this) + storage_.capacity() * sizeof(float);
}

float SynthTextureReverb::bounded(float value, float fallback,
                                  float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) return fallback;
    return std::clamp(value, minimum, maximum);
}

float SynthTextureReverb::time_coefficient(float milliseconds) const noexcept {
    if (!(milliseconds > 0.0F)) return 1.0F;
    const float exponent = -1000.0F / (milliseconds * sample_rate_);
    return std::clamp(1.0F - std::exp(exponent), 0.0F, 1.0F);
}

void SynthTextureReverb::configure(
    SynthTextureReverbParameters parameters) noexcept {
    const SynthTextureReverbParameters defaults{};
    const float maximum_damping = std::min(18000.0F, sample_rate_ * 0.45F);
    parameters.mix = bounded(parameters.mix, defaults.mix, 0.0F, 1.0F);
    parameters.texture = bounded(parameters.texture, defaults.texture, 0.0F, 1.0F);
    parameters.decay_seconds = bounded(
        parameters.decay_seconds, defaults.decay_seconds, 0.2F, 12.0F);
    parameters.damping_hz = bounded(
        parameters.damping_hz, defaults.damping_hz, 500.0F, maximum_damping);
    parameters.level_db = bounded(
        parameters.level_db, defaults.level_db, -24.0F, 12.0F);
    parameters.envelope_release_ms = bounded(
        parameters.envelope_release_ms, defaults.envelope_release_ms,
        20.0F, 2000.0F);
    parameters_ = parameters;
    attack_coefficient_ = time_coefficient(kAttackMs);
    release_coefficient_ = time_coefficient(parameters_.envelope_release_ms);
    target_damping_alpha_ = std::exp(
        -kTwoPi * parameters_.damping_hz / sample_rate_);
    target_level_ = std::pow(10.0F, parameters_.level_db / 20.0F);

    const float ramp = static_cast<float>(ramp_total_);
    mix_step_ = (parameters_.mix - mix_) / ramp;
    texture_step_ = (parameters_.texture - texture_) / ramp;
    damping_alpha_step_ = (target_damping_alpha_ - damping_alpha_) / ramp;
    level_step_ = (target_level_ - level_) / ramp;
    for (std::size_t channel = 0U; channel < channels_; ++channel) {
        for (std::size_t index = 0U; index < kLinesPerChannel; ++index) {
            auto& line = lines_[channel * kLinesPerChannel + index];
            const float delay_seconds = static_cast<float>(line.length) /
                                        sample_rate_;
            line.feedback_target = std::min(
                0.995F, std::pow(10.0F,
                    -3.0F * delay_seconds / parameters_.decay_seconds));
            line.feedback_step =
                (line.feedback_target - line.feedback) / ramp;
        }
    }
    ramp_remaining_ = ramp_total_;
}

void SynthTextureReverb::advance_parameters() noexcept {
    if (ramp_remaining_ == 0U) return;
    mix_ += mix_step_;
    texture_ += texture_step_;
    damping_alpha_ += damping_alpha_step_;
    level_ += level_step_;
    for (std::size_t index = 0U; index < channels_ * kLinesPerChannel; ++index) {
        lines_[index].feedback += lines_[index].feedback_step;
    }
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) settle_parameters();
}

void SynthTextureReverb::settle_parameters() noexcept {
    mix_ = parameters_.mix;
    texture_ = parameters_.texture;
    damping_alpha_ = target_damping_alpha_;
    level_ = target_level_;
    for (std::size_t index = 0U; index < channels_ * kLinesPerChannel; ++index) {
        lines_[index].feedback = lines_[index].feedback_target;
        lines_[index].feedback_step = 0.0F;
    }
    ramp_remaining_ = 0U;
}

float SynthTextureReverb::texture_sample() noexcept {
    float sum = 0.0F;
    for (std::size_t index = 0U; index < oscillator_phase_.size(); ++index) {
        sum += kTextureWeights[index] *
               std::sin(static_cast<float>(oscillator_phase_[index]));
        oscillator_phase_[index] += oscillator_increment_[index];
        if (oscillator_phase_[index] >= static_cast<double>(kTwoPi)) {
            oscillator_phase_[index] -= static_cast<double>(kTwoPi);
        }
    }
    return std::isfinite(sum) ? sum * input_envelope_ : 0.0F;
}

float SynthTextureReverb::process_channel(std::size_t channel,
                                          float excitation) noexcept {
    std::array<float, kLinesPerChannel> delayed{};
    std::array<float, kLinesPerChannel> damped{};
    const std::size_t base = channel * kLinesPerChannel;
    for (std::size_t index = 0U; index < kLinesPerChannel; ++index) {
        auto& line = lines_[base + index];
        const float value = storage_[line.offset + line.index];
        delayed[index] = std::isfinite(value) ? value : 0.0F;
        line.damping_state += (1.0F - damping_alpha_) *
                              (delayed[index] - line.damping_state);
        if (!std::isfinite(line.damping_state)) line.damping_state = 0.0F;
        damped[index] = line.damping_state;
    }

    // A normalized four-way Hadamard feedback mix spreads each excitation
    // among the delay times without storing a large convolution or IR.
    const float feedback[4U]{
        0.5F * (damped[0] + damped[1] + damped[2] + damped[3]),
        0.5F * (damped[0] - damped[1] + damped[2] - damped[3]),
        0.5F * (damped[0] + damped[1] - damped[2] - damped[3]),
        0.5F * (damped[0] - damped[1] - damped[2] + damped[3]),
    };

    float wet = 0.0F;
    for (std::size_t index = 0U; index < kLinesPerChannel; ++index) {
        auto& line = lines_[base + index];
        float write = kInjectionGain * excitation +
                      line.feedback * feedback[index];
        if (!std::isfinite(write)) write = 0.0F;
        storage_[line.offset + line.index] =
            std::clamp(write, -kInternalLimit, kInternalLimit);
        line.index = line.index + 1U == line.length ? 0U : line.index + 1U;
        wet += delayed[index];
    }
    wet *= 0.25F;
    return std::isfinite(wet) ? wet : 0.0F;
}

void SynthTextureReverb::process(std::span<float> interleaved,
                                 std::uint16_t channels) noexcept {
    if (channels != channels_) return;
    bypass_cleared_ = false;
    process_internal(interleaved, false);
}

void SynthTextureReverb::process_bypassed(
    std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels != channels_) return;
    if (!parameters_.spillover) {
        if (!bypass_cleared_) reset();
        bypass_cleared_ = true;
        return;
    }
    bypass_cleared_ = false;
    process_internal(interleaved, true);
}

void SynthTextureReverb::process_internal(std::span<float> interleaved,
                                          bool bypassed) noexcept {
    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        advance_parameters();
        const std::size_t offset = frame * channels_;
        const float left = finite_sample(interleaved[offset]);
        const float right = channels_ == 2U
            ? finite_sample(interleaved[offset + 1U])
            : left;
        const float level = channels_ == 1U
            ? std::abs(left)
            : std::max(std::abs(left), std::abs(right));
        const float envelope_coefficient = level > input_envelope_
            ? attack_coefficient_
            : release_coefficient_;
        input_envelope_ += envelope_coefficient * (level - input_envelope_);
        if (!std::isfinite(input_envelope_)) input_envelope_ = 0.0F;
        input_envelope_ = std::clamp(input_envelope_, 0.0F, 8.0F);

        const float synthetic = texture_sample() * texture_ * 0.7F;
        const float texture_share = texture_ * 0.35F;
        const float excitation_left = bypassed
            ? 0.0F
            : left * (1.0F - texture_share) + synthetic;
        const float excitation_right = bypassed
            ? 0.0F
            : right * (1.0F - texture_share) + synthetic;
        float wet_left = process_channel(0U, excitation_left);
        float wet_right = wet_left;
        if (channels_ == 2U) {
            wet_right = process_channel(1U, excitation_right);
            const float cross_left = wet_left * (1.0F - kStereoMix) +
                                     wet_right * kStereoMix;
            const float cross_right = wet_right * (1.0F - kStereoMix) +
                                      wet_left * kStereoMix;
            wet_left = cross_left;
            wet_right = cross_right;
        }

        const float output_left = bypassed
            ? left + wet_left * mix_ * level_
            : left * (1.0F - mix_) + wet_left * mix_ * level_;
        const float safe_left = std::isfinite(output_left)
            ? std::clamp(output_left, -kOutputLimit, kOutputLimit)
            : 0.0F;
        interleaved[offset] = safe_left;
        if (channels_ == 2U) {
            const float output_right = bypassed
                ? right + wet_right * mix_ * level_
                : right * (1.0F - mix_) + wet_right * mix_ * level_;
            interleaved[offset + 1U] = std::isfinite(output_right)
                ? std::clamp(output_right, -kOutputLimit, kOutputLimit)
                : 0.0F;
        }
    }
}

void SynthTextureReverb::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    for (auto& line : lines_) {
        line.index = 0U;
        line.damping_state = 0.0F;
    }
    oscillator_phase_.fill(0.0);
    input_envelope_ = 0.0F;
    settle_parameters();
    bypass_cleared_ = true;
}

}  // namespace tabletfx::effects
