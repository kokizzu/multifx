#include "tabletfx/effects/four_band_resonator.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kDefaultSampleRate = 48000.0F;
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

FourBandResonator::FourBandResonator(
    float sample_rate, FourBandResonatorParameters parameters) {
    const bool valid_rate = std::isfinite(sample_rate) &&
                            sample_rate >= kMinimumSampleRate &&
                            sample_rate <= kMaximumSampleRate;
    sample_rate_ = valid_rate ? sample_rate : kDefaultSampleRate;

    const double maximum_delay = std::ceil(
        static_cast<double>(sample_rate_) /
        static_cast<double>(kChordRootMinimumFrequencyHz));
    capacity_frames_ = static_cast<std::size_t>(maximum_delay) + 2U;
    delay_storage_.assign(kBands * kMaximumChannels * capacity_frames_, 0.0F);

    set_parameters(parameters);
    reset();
}

void FourBandResonator::set_parameters(
    FourBandResonatorParameters parameters) noexcept {
    if (parameters.mode != FourBandResonatorMode::Manual &&
        parameters.mode != FourBandResonatorMode::Chord) {
        parameters.mode = FourBandResonatorMode::Manual;
    }
    if (static_cast<std::uint8_t>(parameters.chord) >
        static_cast<std::uint8_t>(FourBandResonatorChord::Augmented)) {
        parameters.chord = FourBandResonatorChord::Major;
    }
    parameters.master_frequency = bounded(
        parameters.master_frequency, 1.0F, kMinimumMasterFrequency,
        kMaximumMasterFrequency);
    parameters.chord_root_frequency_hz = bounded(
        parameters.chord_root_frequency_hz, 110.0F,
        kChordRootMinimumFrequencyHz, kChordRootMaximumFrequencyHz);
    if (parameters.input_mode != FourBandResonatorInputMode::Mono &&
        parameters.input_mode != FourBandResonatorInputMode::Stereo) {
        parameters.input_mode = FourBandResonatorInputMode::Mono;
    }
    parameters.master_level = bounded(parameters.master_level, 1.0F,
                                      kMinimumMasterLevel,
                                      kMaximumMasterLevel);
    parameters.master_pan = bounded(parameters.master_pan, 1.0F, -1.0F,
                                    1.0F);
    parameters.master_feedback = bounded(
        parameters.master_feedback, 1.0F, kMinimumMasterFeedback,
        kMaximumMasterFeedback);
    parameters.master_q = bounded(parameters.master_q, 1.0F,
                                  kMinimumMasterQ, kMaximumMasterQ);
    parameters.mix = bounded(parameters.mix, 0.5F, 0.0F, 1.0F);
    parameters.input_gain_db = bounded(
        parameters.input_gain_db, 0.0F, kMinimumInputGainDb,
        kMaximumInputGainDb);
    input_gain_linear_ = std::pow(10.0F, parameters.input_gain_db / 20.0F);
    for (std::size_t band = 0; band < kBands; ++band) {
        if (parameters.filter_location[band] !=
                FourBandResonatorFilterLocation::Pre &&
            parameters.filter_location[band] !=
                FourBandResonatorFilterLocation::Post) {
            parameters.filter_location[band] =
                FourBandResonatorFilterLocation::Post;
        }
        parameters.manual_frequency_hz[band] = bounded(
            parameters.manual_frequency_hz[band],
            FourBandResonatorParameters{}.manual_frequency_hz[band],
            kManualMinimumFrequencyHz, kManualMaximumFrequencyHz);
        parameters.feedback[band] = bounded(
            parameters.feedback[band], 0.75F, kMinimumFeedback,
            kMaximumFeedback);
        parameters.q[band] = bounded(parameters.q[band], 2.0F, kMinimumQ,
                                     kMaximumQ);
        parameters.level[band] = bounded(parameters.level[band], 0.5F, 0.0F,
                                          kMaximumLevel);
        parameters.pan[band] = bounded(parameters.pan[band], 0.0F, -1.0F,
                                        1.0F);
    }

    parameters_ = parameters;
    prepare_resonators();
}

void FourBandResonator::process(std::span<float> interleaved,
                                std::uint16_t channels) noexcept {
    if (channels == 0 || channels > kMaximumChannels) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        std::array<float, kMaximumChannels> original{};
        std::array<float, kMaximumChannels> input{};
        std::array<float, kMaximumChannels> effect_input{};
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const float sample = interleaved[offset + channel];
            original[channel] = std::isfinite(sample) ? sample : 0.0F;
            input[channel] = std::clamp(original[channel],
                                        -kMaximumInputMagnitude,
                                        kMaximumInputMagnitude);
            effect_input[channel] = input[channel] * input_gain_linear_;
        }

        const float mono_input = channels == 2
            ? 0.5F * (effect_input[0] + effect_input[1])
            : effect_input[0];
        double wet_mono = 0.0;
        double wet_left = 0.0;
        double wet_right = 0.0;
        for (std::size_t band = 0; band < kBands; ++band) {
            const std::uint16_t source_channel =
                channels == 2 &&
                        parameters_.input_mode ==
                            FourBandResonatorInputMode::Stereo &&
                        band >= 2U
                    ? 1U
                    : 0U;
            const float band_input =
                channels == 2 && parameters_.input_mode ==
                                     FourBandResonatorInputMode::Mono
                    ? mono_input
                    : effect_input[source_channel];
            const float delayed = delayed_sample(
                band, source_channel, delay_samples_[band]);
            const bool filter_before =
                parameters_.filter_location[band] ==
                FourBandResonatorFilterLocation::Pre;
            const float resonator_input =
                filter_before
                    ? filter(band_input, coefficients_[band],
                             filter_states_[source_channel][band])
                    : band_input;
            const float resonant =
                filter_before
                    ? delayed
                    : filter(delayed, coefficients_[band],
                             filter_states_[source_channel][band]);
            const float feedback = std::clamp(
                parameters_.feedback[band] * parameters_.master_feedback,
                kMinimumFeedback, kMaximumFeedback);
            const float stored = resonator_input + feedback * delayed;
            buffer_sample(band, source_channel, write_frame_) =
                std::isfinite(stored)
                    ? std::clamp(stored, -kMaximumStateMagnitude,
                                 kMaximumStateMagnitude)
                    : 0.0F;

            const float output = std::isfinite(resonant) ? resonant : 0.0F;
            const float level = parameters_.level[band] /
                                static_cast<float>(kBands);
            const float band_wet = output * level;
            wet_mono += static_cast<double>(band_wet);
            if (channels == 2) {
                const float effective_pan =
                    parameters_.pan[band] * parameters_.master_pan;
                const float pan_angle = (effective_pan + 1.0F) * kPi * 0.25F;
                const float left_gain = std::sqrt(2.0F) * std::cos(pan_angle);
                const float right_gain = std::sqrt(2.0F) * std::sin(pan_angle);
                wet_left += static_cast<double>(band_wet * left_gain);
                wet_right += static_cast<double>(band_wet * right_gain);
            }
        }

        wet_mono *= static_cast<double>(parameters_.master_level);
        wet_left *= static_cast<double>(parameters_.master_level);
        wet_right *= static_cast<double>(parameters_.master_level);
        if (channels == 1) {
            const double output =
                static_cast<double>(original[0]) *
                    static_cast<double>(1.0F - parameters_.mix) +
                wet_mono * static_cast<double>(parameters_.mix);
            interleaved[offset] =
                std::isfinite(output)
                    ? std::clamp(static_cast<float>(output),
                                 -kMaximumStateMagnitude,
                                 kMaximumStateMagnitude)
                    : 0.0F;
        } else {
            for (std::uint16_t channel = 0; channel < channels; ++channel) {
                const double wet = channel == 0 ? wet_left : wet_right;
                const double output =
                    static_cast<double>(original[channel]) *
                        static_cast<double>(1.0F - parameters_.mix) +
                    wet * static_cast<double>(parameters_.mix);
                interleaved[offset + channel] =
                    std::isfinite(output)
                        ? std::clamp(static_cast<float>(output),
                                     -kMaximumStateMagnitude,
                                     kMaximumStateMagnitude)
                        : 0.0F;
            }
        }

        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
    }
}

void FourBandResonator::reset() noexcept {
    std::fill(delay_storage_.begin(), delay_storage_.end(), 0.0F);
    for (auto& channel : filter_states_) {
        channel.fill(FilterState{});
    }
    write_frame_ = 0;
}

float FourBandResonator::bounded(float value, float fallback, float minimum,
                                 float maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

std::array<int, FourBandResonator::kBands>
FourBandResonator::chord_intervals(FourBandResonatorChord chord) noexcept {
    switch (chord) {
    case FourBandResonatorChord::Major: return kMajor;
    case FourBandResonatorChord::Major6: return kMajor6;
    case FourBandResonatorChord::Dominant7: return kDominant7;
    case FourBandResonatorChord::Major7: return kMajor7;
    case FourBandResonatorChord::Minor: return kMinor;
    case FourBandResonatorChord::Minor6: return kMinor6;
    case FourBandResonatorChord::Minor7: return kMinor7;
    case FourBandResonatorChord::Diminished: return kDiminished;
    case FourBandResonatorChord::Augmented: return kAugmented;
    }
    return kMajor;
}

void FourBandResonator::prepare_resonators() noexcept {
    std::array<float, kBands> frequencies = parameters_.manual_frequency_hz;
    if (parameters_.mode == FourBandResonatorMode::Chord) {
        const auto intervals = chord_intervals(parameters_.chord);
        for (std::size_t band = 0; band < kBands; ++band) {
            const float ratio = std::exp2(
                static_cast<float>(intervals[band]) / 12.0F);
            frequencies[band] = parameters_.chord_root_frequency_hz * ratio;
        }
    } else {
        for (std::size_t band = 0; band < kBands; ++band) {
            frequencies[band] *= parameters_.master_frequency;
        }
    }

    const float nyquist_guard = sample_rate_ * 0.45F;
    for (std::size_t band = 0; band < kBands; ++band) {
        const float frequency = std::clamp(frequencies[band], 1.0F,
                                           nyquist_guard);
        effective_frequencies_hz_[band] = frequency;
        delay_samples_[band] = std::clamp(
            sample_rate_ / frequency, 2.0F,
            static_cast<float>(capacity_frames_ - 2U));

        const float omega = 2.0F * kPi * frequency / sample_rate_;
        const float cosine = std::cos(omega);
        const float effective_q = std::clamp(
            parameters_.q[band] * parameters_.master_q, kMinimumQ,
            kMaximumQ);
        const float alpha = std::sin(omega) / (2.0F * effective_q);
        const float a0 = 1.0F + alpha;
        coefficients_[band] = {alpha / a0, 0.0F, -alpha / a0,
                               -2.0F * cosine / a0,
                               (1.0F - alpha) / a0};
    }
}

float FourBandResonator::filter(float input,
                               const Coefficients& coefficients,
                               FilterState& state) const noexcept {
    const float output = coefficients.b0 * input + state.z1;
    const float next_z1 = coefficients.b1 * input - coefficients.a1 * output +
                          state.z2;
    const float next_z2 = coefficients.b2 * input - coefficients.a2 * output;
    if (!std::isfinite(output) || !std::isfinite(next_z1) ||
        !std::isfinite(next_z2) || std::abs(next_z1) > kMaximumStateMagnitude ||
        std::abs(next_z2) > kMaximumStateMagnitude) {
        state = {};
        return 0.0F;
    }
    state = {next_z1, next_z2};
    return output;
}

float FourBandResonator::delayed_sample(std::size_t band,
                                        std::uint16_t channel,
                                        float delay_samples) const noexcept {
    float position = static_cast<float>(write_frame_) - delay_samples;
    if (position < 0.0F) {
        position += static_cast<float>(capacity_frames_);
    }
    const float integral = std::floor(position);
    const std::size_t first = static_cast<std::size_t>(integral);
    const std::size_t second = (first + 1U) % capacity_frames_;
    const float fraction = position - integral;
    const float a = buffer_sample(band, channel, first);
    const float b = buffer_sample(band, channel, second);
    return a + (b - a) * fraction;
}

float& FourBandResonator::buffer_sample(std::size_t band,
                                        std::uint16_t channel,
                                        std::size_t frame) noexcept {
    const std::size_t voice = band * kMaximumChannels + channel;
    return delay_storage_[voice * capacity_frames_ + frame];
}

const float& FourBandResonator::buffer_sample(
    std::size_t band, std::uint16_t channel, std::size_t frame) const noexcept {
    const std::size_t voice = band * kMaximumChannels + channel;
    return delay_storage_[voice * capacity_frames_ + frame];
}

} // namespace tabletfx
