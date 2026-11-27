#include "tabletfx/effects/resonator.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kMinimumMasterFrequency = 0.5F;
constexpr float kMaximumMasterFrequency = 2.0F;
constexpr float kMinimumQ = 0.5F;
constexpr float kMaximumQ = 12.0F;
constexpr float kMinimumMasterQ = 0.1F;
constexpr float kMaximumMasterQ = 4.0F;
constexpr float kMinimumMasterLevel = 0.0F;
constexpr float kMaximumMasterLevel = 2.0F;
constexpr float kMinimumLevelDb = -24.0F;
constexpr float kMaximumLevelDb = 12.0F;
constexpr float kMinimumInputGainDb = -24.0F;
constexpr float kMaximumInputGainDb = 12.0F;

constexpr std::array<int, 4> kMajor{0, 4, 7, 12};
constexpr std::array<int, 4> kMajor6{0, 4, 7, 9};
constexpr std::array<int, 4> kDominant7{0, 4, 7, 10};
constexpr std::array<int, 4> kMajor7{0, 4, 7, 11};
constexpr std::array<int, 4> kMinor{0, 3, 7, 12};
constexpr std::array<int, 4> kMinor6{0, 3, 7, 9};
constexpr std::array<int, 4> kMinor7{0, 3, 7, 10};
constexpr std::array<int, 4> kDiminished{0, 3, 6, 9};
constexpr std::array<int, 4> kAugmented{0, 4, 8, 12};

} // namespace

Resonator::Resonator(float sample_rate, ResonatorParameters parameters) {
    const bool valid_rate = std::isfinite(sample_rate) &&
                            sample_rate >= kMinimumSampleRate &&
                            sample_rate <= kMaximumSampleRate;
    sample_rate_ = valid_rate ? sample_rate : kDefaultSampleRate;

    const auto maximum_delay = static_cast<std::size_t>(std::ceil(
        static_cast<double>(sample_rate_) /
        static_cast<double>(kChordRootMinimumFrequencyHz)));
    capacity_frames_ = maximum_delay + 2U;
    delay_storage_.assign(kVoiceCount * kStorageChannels * capacity_frames_,
                          0.0F);

    set_parameters(parameters);
    reset();
}

void Resonator::set_parameters(ResonatorParameters parameters) noexcept {
    if (parameters.mode != ResonatorMode::Manual &&
        parameters.mode != ResonatorMode::Chordal) {
        parameters.mode = ResonatorMode::Manual;
    }
    if (static_cast<std::uint8_t>(parameters.chord) >
        static_cast<std::uint8_t>(ResonatorChord::Augmented)) {
        parameters.chord = ResonatorChord::Major;
    }
    if (parameters.input_mode != ResonatorInputMode::Mono &&
        parameters.input_mode != ResonatorInputMode::Stereo) {
        parameters.input_mode = ResonatorInputMode::Mono;
    }

    parameters.master_frequency = bounded(
        parameters.master_frequency, 1.0F, kMinimumMasterFrequency,
        kMaximumMasterFrequency);
    parameters.chord_root_frequency_hz = bounded(
        parameters.chord_root_frequency_hz, 110.0F,
        kChordRootMinimumFrequencyHz, kChordRootMaximumFrequencyHz);
    parameters.master_level = bounded(parameters.master_level, 0.8F,
                                      kMinimumMasterLevel,
                                      kMaximumMasterLevel);
    parameters.master_pan = bounded(parameters.master_pan, 1.0F, -1.0F, 1.0F);
    parameters.master_feedback = bounded(parameters.master_feedback, 1.0F,
                                         0.0F, 1.0F);
    parameters.master_q = bounded(parameters.master_q, 1.0F,
                                  kMinimumMasterQ, kMaximumMasterQ);
    parameters.mix = bounded(parameters.mix, 0.35F, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, 0.0F,
                                  kMinimumLevelDb, kMaximumLevelDb);
    parameters.balance = bounded(parameters.balance, 0.0F, -1.0F, 1.0F);
    parameters.input_gain_db = bounded(parameters.input_gain_db, 0.0F,
                                       kMinimumInputGainDb,
                                       kMaximumInputGainDb);
    parameters.global_mix_offset_percentage_points = bounded(
        parameters.global_mix_offset_percentage_points, 0.0F, -50.0F, 50.0F);

    input_gain_linear_ = std::pow(10.0F, parameters.input_gain_db / 20.0F);
    output_level_linear_ = std::pow(10.0F, parameters.level_db / 20.0F);
    effective_mix_ = parameters.mix;
    if (parameters.global_mix_enabled) {
        effective_mix_ = std::clamp(
            parameters.mix +
                parameters.global_mix_offset_percentage_points * 0.01F,
            0.0F, 1.0F);
    }
    for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
        parameters.manual_frequency_hz[voice] = bounded(
            parameters.manual_frequency_hz[voice],
            ResonatorParameters{}.manual_frequency_hz[voice],
            kManualMinimumFrequencyHz, kManualMaximumFrequencyHz);
        parameters.feedback[voice] = bounded(parameters.feedback[voice], 0.55F,
                                             0.0F, kMaximumFeedback);
        parameters.filter_q[voice] = bounded(parameters.filter_q[voice], 2.0F,
                                             kMinimumQ, kMaximumQ);
        parameters.level[voice] = bounded(parameters.level[voice], 0.4F,
                                          0.0F, 1.0F);
        parameters.pan[voice] = bounded(parameters.pan[voice], 0.0F, -1.0F,
                                         1.0F);
        if (parameters.filter_location[voice] !=
                ResonatorFilterLocation::Pre &&
            parameters.filter_location[voice] !=
                ResonatorFilterLocation::Post) {
            parameters.filter_location[voice] =
                ResonatorFilterLocation::Post;
        }
    }

    parameters_ = parameters;
    prepare_voices();
}

void Resonator::process(std::span<float> interleaved,
                        std::uint16_t channels) noexcept {
    process_internal(interleaved, channels, false);
}

void Resonator::process_bypassed(std::span<float> interleaved,
                                 std::uint16_t channels) noexcept {
    process_internal(interleaved, channels, true);
}

void Resonator::reset() noexcept {
    std::fill(delay_storage_.begin(), delay_storage_.end(), 0.0F);
    for (auto& channel : filter_states_) {
        channel.fill(BiquadState{});
    }
    write_frame_ = 0U;
}

float Resonator::bounded(float value, float fallback, float minimum,
                         float maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

std::array<int, Resonator::kVoiceCount> Resonator::chord_intervals(
    ResonatorChord chord) noexcept {
    switch (chord) {
    case ResonatorChord::Major: return kMajor;
    case ResonatorChord::Major6: return kMajor6;
    case ResonatorChord::Dominant7: return kDominant7;
    case ResonatorChord::Major7: return kMajor7;
    case ResonatorChord::Minor: return kMinor;
    case ResonatorChord::Minor6: return kMinor6;
    case ResonatorChord::Minor7: return kMinor7;
    case ResonatorChord::Diminished: return kDiminished;
    case ResonatorChord::Augmented: return kAugmented;
    }
    return kMajor;
}

void Resonator::prepare_voices() noexcept {
    std::array<float, kVoiceCount> frequencies =
        parameters_.manual_frequency_hz;
    if (parameters_.mode == ResonatorMode::Chordal) {
        const auto intervals = chord_intervals(parameters_.chord);
        for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
            frequencies[voice] = parameters_.chord_root_frequency_hz *
                std::exp2(static_cast<float>(intervals[voice]) / 12.0F);
        }
    } else {
        for (float& frequency : frequencies) {
            frequency *= parameters_.master_frequency;
        }
    }

    const float maximum_frequency = sample_rate_ * 0.45F;
    for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
        const float frequency = std::clamp(frequencies[voice], 1.0F,
                                           maximum_frequency);
        effective_frequencies_hz_[voice] = frequency;
        delay_samples_[voice] = std::clamp(
            sample_rate_ / frequency, 2.0F,
            static_cast<float>(capacity_frames_ - 2U));
        effective_feedback_[voice] = std::clamp(
            parameters_.feedback[voice] * parameters_.master_feedback,
            0.0F, kMaximumFeedback);

        const float q = std::clamp(parameters_.filter_q[voice] *
                                       parameters_.master_q,
                                   kMinimumQ, kMaximumQ);
        const float omega = 2.0F * kPi * frequency / sample_rate_;
        const float sine = std::sin(omega);
        const float cosine = std::cos(omega);
        const float alpha = sine / (2.0F * q);
        const float a0 = 1.0F + alpha;
        coefficients_[voice] = BiquadCoefficients{
            alpha / a0,
            0.0F,
            -alpha / a0,
            (-2.0F * cosine) / a0,
            (1.0F - alpha) / a0,
        };

        const float effective_pan = std::clamp(
            parameters_.pan[voice] * parameters_.master_pan, -1.0F, 1.0F);
        const float angle = (effective_pan + 1.0F) * kPi * 0.25F;
        pan_left_gain_[voice] = std::cos(angle);
        pan_right_gain_[voice] = std::sin(angle);
    }
}

void Resonator::process_internal(std::span<float> interleaved,
                                 std::uint16_t channels,
                                 bool bypassed) noexcept {
    if (channels == 0U || channels > kStorageChannels) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        std::array<float, kStorageChannels> dry{};
        std::array<float, kStorageChannels> effect_input{};
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const float value = interleaved[offset + channel];
            dry[channel] = std::isfinite(value) ? value : 0.0F;
            const float bounded_input = std::clamp(
                dry[channel], -kMaximumInputMagnitude, kMaximumInputMagnitude);
            effect_input[channel] = bypassed
                ? 0.0F
                : bounded_input * input_gain_linear_;
        }

        const float mono_input = channels == 2U
            ? 0.5F * (effect_input[0] + effect_input[1])
            : effect_input[0];
        double wet_mono = 0.0;
        double wet_left = 0.0;
        double wet_right = 0.0;
        for (std::size_t voice = 0; voice < kVoiceCount; ++voice) {
            const std::uint16_t source_channel =
                channels == 2U &&
                        parameters_.input_mode == ResonatorInputMode::Stereo &&
                        voice >= 2U
                    ? 1U
                    : 0U;
            const float voice_input =
                channels == 2U &&
                        parameters_.input_mode == ResonatorInputMode::Mono
                    ? mono_input
                    : effect_input[source_channel];
            const float delayed = delayed_sample(voice, source_channel);
            const bool filter_pre = parameters_.filter_location[voice] ==
                                    ResonatorFilterLocation::Pre;
            const float delay_input = filter_pre
                ? filter(voice_input, coefficients_[voice],
                         filter_states_[source_channel][voice])
                : voice_input;
            const float output = filter_pre
                ? delayed
                : filter(delayed, coefficients_[voice],
                         filter_states_[source_channel][voice]);
            const float stored = delay_input + effective_feedback_[voice] * delayed;
            buffer_sample(voice, source_channel, write_frame_) =
                std::isfinite(stored)
                    ? std::clamp(stored, -kMaximumStateMagnitude,
                                 kMaximumStateMagnitude)
                    : 0.0F;

            const float safe_output = std::isfinite(output) ? output : 0.0F;
            const float voice_output = safe_output * parameters_.level[voice] *
                                       (1.0F / static_cast<float>(kVoiceCount));
            wet_mono += static_cast<double>(voice_output);
            if (channels == 2U) {
                wet_left += static_cast<double>(
                    voice_output * pan_left_gain_[voice]);
                wet_right += static_cast<double>(
                    voice_output * pan_right_gain_[voice]);
            }
        }

        wet_mono *= static_cast<double>(parameters_.master_level);
        wet_left *= static_cast<double>(parameters_.master_level);
        wet_right *= static_cast<double>(parameters_.master_level);
        if (channels == 2U) {
            const float left_balance = parameters_.balance > 0.0F
                ? 1.0F - parameters_.balance
                : 1.0F;
            const float right_balance = parameters_.balance < 0.0F
                ? 1.0F + parameters_.balance
                : 1.0F;
            wet_left *= static_cast<double>(left_balance);
            wet_right *= static_cast<double>(right_balance);
        }

        if (channels == 1U) {
            const double result = bypassed
                ? static_cast<double>(dry[0]) +
                      wet_mono * static_cast<double>(effective_mix_) *
                          static_cast<double>(output_level_linear_)
                : (static_cast<double>(dry[0]) *
                       static_cast<double>(1.0F - effective_mix_) +
                   wet_mono * static_cast<double>(effective_mix_)) *
                      static_cast<double>(output_level_linear_);
            interleaved[offset] = std::isfinite(result)
                ? std::clamp(static_cast<float>(result),
                             -kMaximumOutputMagnitude, kMaximumOutputMagnitude)
                : 0.0F;
        } else {
            for (std::uint16_t channel = 0; channel < channels; ++channel) {
                const double wet = channel == 0U ? wet_left : wet_right;
                const double result = bypassed
                    ? static_cast<double>(dry[channel]) +
                          wet * static_cast<double>(effective_mix_) *
                              static_cast<double>(output_level_linear_)
                    : (static_cast<double>(dry[channel]) *
                           static_cast<double>(1.0F - effective_mix_) +
                       wet * static_cast<double>(effective_mix_)) *
                          static_cast<double>(output_level_linear_);
                interleaved[offset + channel] = std::isfinite(result)
                    ? std::clamp(static_cast<float>(result),
                                 -kMaximumOutputMagnitude,
                                 kMaximumOutputMagnitude)
                    : 0.0F;
            }
        }

        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
    }
}

float Resonator::filter(float input,
                        const BiquadCoefficients& coefficients,
                        BiquadState& state) const noexcept {
    const float output = coefficients.b0 * input + state.z1;
    const float next_z1 = coefficients.b1 * input - coefficients.a1 * output +
                          state.z2;
    const float next_z2 = coefficients.b2 * input - coefficients.a2 * output;
    if (!std::isfinite(output) || !std::isfinite(next_z1) ||
        !std::isfinite(next_z2)) {
        state = BiquadState{};
        return 0.0F;
    }
    state.z1 = std::clamp(next_z1, -kMaximumStateMagnitude,
                          kMaximumStateMagnitude);
    state.z2 = std::clamp(next_z2, -kMaximumStateMagnitude,
                          kMaximumStateMagnitude);
    return std::clamp(output, -kMaximumStateMagnitude,
                      kMaximumStateMagnitude);
}

float Resonator::delayed_sample(std::size_t voice,
                                std::uint16_t channel) const noexcept {
    const float delay = delay_samples_[voice];
    const std::size_t whole = static_cast<std::size_t>(std::floor(delay));
    const float fraction = delay - static_cast<float>(whole);
    const std::size_t newest_read =
        (write_frame_ + capacity_frames_ - whole) % capacity_frames_;
    const std::size_t older_read =
        (newest_read + capacity_frames_ - 1U) % capacity_frames_;
    return buffer_sample(voice, channel, newest_read) * (1.0F - fraction) +
           buffer_sample(voice, channel, older_read) * fraction;
}

float& Resonator::buffer_sample(std::size_t voice, std::uint16_t channel,
                                std::size_t frame) noexcept {
    const std::size_t index =
        (voice * kStorageChannels + channel) * capacity_frames_ + frame;
    return delay_storage_[index];
}

const float& Resonator::buffer_sample(std::size_t voice,
                                     std::uint16_t channel,
                                     std::size_t frame) const noexcept {
    const std::size_t index =
        (voice * kStorageChannels + channel) * capacity_frames_ + frame;
    return delay_storage_[index];
}

} // namespace tabletfx
