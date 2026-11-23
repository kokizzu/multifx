#include "tabletfx/effects/convolution_reverb.hpp"

#include <algorithm>
#include <cmath>
#include <new>
#include <numbers>
#include <stdexcept>

namespace tabletfx {
namespace {

float checked_sample_rate(float sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate < 8000.0F ||
        sample_rate > 192000.0F) {
        throw std::invalid_argument(
            "ConvolutionReverb sample rate must be between 8 kHz and 192 kHz");
    }
    return sample_rate;
}

std::uint16_t checked_channels(std::uint16_t channels) {
    if (channels != 1 && channels != 2) {
        throw std::invalid_argument(
            "ConvolutionReverb supports mono or stereo only");
    }
    return channels;
}

float finite_internal_sample(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, -16.0F, 16.0F) : 0.0F;
}

} // namespace

ConvolutionReverb::ConvolutionReverb(float sample_rate, std::uint16_t channels,
                                     ConvolutionReverbParameters parameters)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)) {
    const double ramp_frames = static_cast<double>(sample_rate_) *
                               static_cast<double>(kControlSmoothingMs) / 1000.0;
    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(ramp_frames)));

    for (std::size_t index = 0; index < kFftFrames; ++index) {
        std::size_t value = index;
        std::size_t reversed = 0;
        for (std::size_t bits = kFftFrames; bits > 1; bits >>= 1U) {
            reversed = (reversed << 1U) | (value & 1U);
            value >>= 1U;
        }
        bit_reversed_[index] = static_cast<std::uint16_t>(reversed);
    }
    for (std::size_t index = 0; index < twiddles_.size(); ++index) {
        const float phase = -2.0F * std::numbers::pi_v<float> *
                            static_cast<float>(index) /
                            static_cast<float>(kFftFrames);
        twiddles_[index] = Complex(std::cos(phase), std::sin(phase));
    }

    configure(parameters);
    reset();
}

std::size_t ConvolutionReverb::memory_bytes() const noexcept {
    return sizeof(*this) +
           (impulse_spectra_.capacity() + input_spectra_.capacity()) * sizeof(Complex);
}

void ConvolutionReverb::configure(ConvolutionReverbParameters parameters) noexcept {
    const ConvolutionReverbParameters defaults{};
    parameters.mix = bounded(parameters.mix, defaults.mix, 0.0F, 1.0F);
    parameters.level_db = bounded(parameters.level_db, defaults.level_db,
                                  -24.0F, 12.0F);
    parameters.high_pass_hz = bounded(parameters.high_pass_hz,
                                      defaults.high_pass_hz, 0.0F, 10000.0F);
    parameters.low_pass_hz = bounded(parameters.low_pass_hz,
                                     defaults.low_pass_hz, 0.0F, 20000.0F);

    const float previous_mix = mix_;
    const float previous_level = level_;
    const float previous_high_pass = high_pass_hz_;
    const float previous_low_pass = low_pass_hz_;
    parameters_ = parameters;
    const float ramp = static_cast<float>(ramp_total_);
    const float target_mix = parameters_.mix;
    target_level_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    const float target_high_pass = parameters_.high_pass_hz;
    const float target_low_pass = parameters_.low_pass_hz;
    mix_step_ = (target_mix - previous_mix) / ramp;
    level_step_ = (target_level_ - previous_level) / ramp;
    high_pass_step_ = (target_high_pass - previous_high_pass) / ramp;
    low_pass_step_ = (target_low_pass - previous_low_pass) / ramp;
    ramp_remaining_ = ramp_total_;
}

bool ConvolutionReverb::load_impulse_response(
    std::span<const float> interleaved, std::uint16_t ir_channels,
    std::uint32_t ir_sample_rate) noexcept {
    if ((ir_channels != 1 && ir_channels != 2) || ir_sample_rate < 8000 ||
        ir_sample_rate > 192000 || interleaved.empty() ||
        interleaved.size() % ir_channels != 0) {
        return false;
    }

    const std::size_t source_frames = interleaved.size() / ir_channels;
    const double scaled_frames = static_cast<double>(source_frames) *
                                 static_cast<double>(sample_rate_) /
                                 static_cast<double>(ir_sample_rate);
    if (source_frames == 0 || !std::isfinite(scaled_frames) ||
        scaled_frames < 1.0 ||
        scaled_frames > static_cast<double>(kMaximumImpulseFrames) + 0.5) {
        return false;
    }
    const std::size_t target_frames = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::llround(scaled_frames)));
    if (target_frames > kMaximumImpulseFrames) return false;
    for (float sample : interleaved) {
        if (!std::isfinite(sample)) return false;
    }

    const std::size_t partitions =
        (target_frames + kPartitionFrames - 1) / kPartitionFrames;
    const std::uint16_t stored_ir_channels =
        (ir_channels == 1 || channels_ == 1) ? 1U : 2U;
    std::vector<Complex> impulse_spectra;
    std::vector<Complex> input_spectra;
    try {
        const std::size_t spectrum_count =
            static_cast<std::size_t>(channels_) * partitions * kSpectrumBins;
        const std::size_t impulse_spectrum_count =
            static_cast<std::size_t>(stored_ir_channels) * partitions * kSpectrumBins;
        impulse_spectra.resize(impulse_spectrum_count);
        input_spectra.resize(spectrum_count);
    } catch (const std::bad_alloc&) {
        return false;
    } catch (const std::length_error&) {
        return false;
    }

    for (std::uint16_t channel = 0; channel < stored_ir_channels; ++channel) {
        const std::size_t channel_offset =
            static_cast<std::size_t>(channel) * partitions * kSpectrumBins;
        for (std::size_t partition = 0; partition < partitions; ++partition) {
            transform_buffer_.fill(Complex{});
            for (std::size_t frame = 0; frame < kPartitionFrames; ++frame) {
                const std::size_t target_frame =
                    partition * kPartitionFrames + frame;
                if (target_frame >= target_frames) break;
                const double source_position =
                    static_cast<double>(target_frame) *
                    static_cast<double>(ir_sample_rate) /
                    static_cast<double>(sample_rate_);
                transform_buffer_[frame] = impulse_sample(
                    interleaved, source_position, source_frames, ir_channels, channel);
            }
            transform(transform_buffer_, false);
            const std::size_t offset =
                channel_offset + partition * kSpectrumBins;
            std::copy_n(transform_buffer_.begin(), kSpectrumBins,
                        impulse_spectra.begin() +
                            static_cast<std::ptrdiff_t>(offset));
        }
    }

    impulse_spectra_.swap(impulse_spectra);
    input_spectra_.swap(input_spectra);
    impulse_frames_ = target_frames;
    partition_count_ = partitions;
    impulse_channels_ = stored_ir_channels;
    reset();
    return true;
}

void ConvolutionReverb::clear_impulse_response() noexcept {
    std::vector<Complex>().swap(impulse_spectra_);
    std::vector<Complex>().swap(input_spectra_);
    impulse_frames_ = 0;
    partition_count_ = 0;
    impulse_channels_ = 0;
    history_write_partition_ = 0;
    reset();
}

void ConvolutionReverb::process(std::span<float> interleaved,
                                std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0 || partition_count_ == 0) return;
    bypassed_ = false;
    tail_partitions_remaining_ = 0;
    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        advance_controls();
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = frame_cursor_ * channels_ + channel;
            const float input = finite_internal_sample(
                interleaved[frame * channels_ + channel]);
            const float delayed_dry = dry_delay_[index];
            dry_delay_[index] = input;
            input_block_[index] = input;
            const float wet = filter_wet_sample(output_block_[index], channel);
            const float output = delayed_dry * (1.0F - mix_) +
                                 wet * mix_ * level_;
            interleaved[frame * channels_ + channel] =
                finite_internal_sample(output);
        }
        ++frame_cursor_;
        if (frame_cursor_ == kPartitionFrames) {
            process_partition();
            frame_cursor_ = 0;
        }
    }
}

void ConvolutionReverb::process_bypassed(std::span<float> interleaved,
                                         std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;
    if (!parameters_.tails) {
        if (!bypassed_ || tail_partitions_remaining_ != 0) reset();
        bypassed_ = true;
        return;
    }
    if (!bypassed_) {
        bypassed_ = true;
        tail_partitions_remaining_ = partition_count_;
    }
    if (partition_count_ == 0 || tail_partitions_remaining_ == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        advance_controls();
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = frame_cursor_ * channels_ + channel;
            input_block_[index] = 0.0F;
            const float wet = filter_wet_sample(output_block_[index], channel);
            interleaved[frame * channels_ + channel] = finite_internal_sample(
                interleaved[frame * channels_ + channel] + wet * mix_ * level_);
        }
        ++frame_cursor_;
        if (frame_cursor_ == kPartitionFrames) {
            process_partition();
            frame_cursor_ = 0;
            if (tail_partitions_remaining_ > 0) --tail_partitions_remaining_;
        }
    }
}

void ConvolutionReverb::reset() noexcept {
    std::fill(input_spectra_.begin(), input_spectra_.end(), Complex{});
    input_block_.fill(0.0F);
    output_block_.fill(0.0F);
    overlap_.fill(0.0F);
    dry_delay_.fill(0.0F);
    high_pass_previous_input_.fill(0.0F);
    high_pass_previous_output_.fill(0.0F);
    low_pass_previous_output_.fill(0.0F);
    history_write_partition_ = 0;
    frame_cursor_ = 0;
    mix_ = parameters_.mix;
    level_ = target_level_;
    high_pass_hz_ = parameters_.high_pass_hz;
    low_pass_hz_ = parameters_.low_pass_hz;
    mix_step_ = 0.0F;
    level_step_ = 0.0F;
    high_pass_step_ = 0.0F;
    low_pass_step_ = 0.0F;
    ramp_remaining_ = 0;
    tail_partitions_remaining_ = 0;
    bypassed_ = false;
}

float ConvolutionReverb::bounded(float value, float fallback,
                                 float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

float ConvolutionReverb::impulse_sample(
    std::span<const float> interleaved, double source_position,
    std::size_t source_frames, std::uint16_t ir_channels,
    std::uint16_t output_channel) const noexcept {
    const double bounded_position = std::clamp(
        source_position, 0.0, static_cast<double>(source_frames - 1));
    const std::size_t first = static_cast<std::size_t>(bounded_position);
    const std::size_t second = std::min(first + 1, source_frames - 1);
    const float fraction = static_cast<float>(bounded_position -
                                              static_cast<double>(first));

    auto channel_sample = [&](std::size_t frame, std::uint16_t channel) noexcept {
        return interleaved[frame * ir_channels + channel];
    };
    float first_sample = 0.0F;
    float second_sample = 0.0F;
    if (channels_ == 1 && ir_channels == 2) {
        first_sample = 0.5F * (channel_sample(first, 0) +
                                channel_sample(first, 1));
        second_sample = 0.5F * (channel_sample(second, 0) +
                                 channel_sample(second, 1));
    } else {
        const std::uint16_t source_channel = ir_channels == 1 ? 0 : output_channel;
        first_sample = channel_sample(first, source_channel);
        second_sample = channel_sample(second, source_channel);
    }
    return finite_internal_sample(first_sample +
                                  (second_sample - first_sample) * fraction);
}

void ConvolutionReverb::transform(
    std::array<Complex, kFftFrames>& values, bool inverse) const noexcept {
    for (std::size_t index = 0; index < kFftFrames; ++index) {
        const std::size_t reversed = bit_reversed_[index];
        if (reversed > index) std::swap(values[index], values[reversed]);
    }

    for (std::size_t length = 2; length <= kFftFrames; length <<= 1U) {
        const std::size_t half = length / 2;
        const std::size_t twiddle_stride = kFftFrames / length;
        for (std::size_t base = 0; base < kFftFrames; base += length) {
            for (std::size_t offset = 0; offset < half; ++offset) {
                const Complex twiddle = inverse
                    ? std::conj(twiddles_[offset * twiddle_stride])
                    : twiddles_[offset * twiddle_stride];
                const Complex even = values[base + offset];
                const Complex odd = values[base + offset + half] * twiddle;
                values[base + offset] = even + odd;
                values[base + offset + half] = even - odd;
            }
        }
    }

    if (inverse) {
        constexpr float scale = 1.0F / static_cast<float>(kFftFrames);
        for (Complex& value : values) value *= scale;
    }
}

void ConvolutionReverb::process_partition() noexcept {
    for (std::uint16_t channel = 0; channel < channels_; ++channel) {
        transform_buffer_.fill(Complex{});
        for (std::size_t frame = 0; frame < kPartitionFrames; ++frame) {
            transform_buffer_[frame] = input_block_[frame * channels_ + channel];
        }
        transform(transform_buffer_, false);

        const std::size_t channel_offset =
            static_cast<std::size_t>(channel) * partition_count_ * kSpectrumBins;
        const std::size_t history_offset = channel_offset +
            history_write_partition_ * kSpectrumBins;
        std::copy_n(transform_buffer_.begin(), kSpectrumBins,
                    input_spectra_.begin() +
                        static_cast<std::ptrdiff_t>(history_offset));

        sum_buffer_.fill(Complex{});
        for (std::size_t partition = 0; partition < partition_count_; ++partition) {
            const std::size_t history_partition =
                (history_write_partition_ + partition_count_ - partition) %
                partition_count_;
            const std::size_t input_offset = channel_offset +
                history_partition * kSpectrumBins;
        const std::size_t impulse_channel_offset =
            static_cast<std::size_t>(impulse_channels_ == 1 ? 0 : channel) *
            partition_count_ * kSpectrumBins;
        const std::size_t impulse_offset = impulse_channel_offset +
                partition * kSpectrumBins;
            for (std::size_t bin = 0; bin < kSpectrumBins; ++bin) {
                sum_buffer_[bin] +=
                    impulse_spectra_[impulse_offset + bin] *
                    input_spectra_[input_offset + bin];
            }
        }
        sum_buffer_[0] = Complex(sum_buffer_[0].real(), 0.0F);
        sum_buffer_[kFftFrames / 2] =
            Complex(sum_buffer_[kFftFrames / 2].real(), 0.0F);
        for (std::size_t bin = 1; bin < kFftFrames / 2; ++bin) {
            sum_buffer_[kFftFrames - bin] = std::conj(sum_buffer_[bin]);
        }
        transform(sum_buffer_, true);

        for (std::size_t frame = 0; frame < kPartitionFrames; ++frame) {
            const std::size_t overlap_index =
                static_cast<std::size_t>(channel) * kPartitionFrames + frame;
            const std::size_t output_index = frame * channels_ + channel;
            output_block_[output_index] = finite_internal_sample(
                sum_buffer_[frame].real() + overlap_[overlap_index]);
            overlap_[overlap_index] = finite_internal_sample(
                sum_buffer_[frame + kPartitionFrames].real());
        }
    }
    history_write_partition_ =
        (history_write_partition_ + 1) % partition_count_;
}

void ConvolutionReverb::advance_controls() noexcept {
    if (ramp_remaining_ == 0) return;
    --ramp_remaining_;
    if (ramp_remaining_ == 0) {
        mix_ = parameters_.mix;
        level_ = target_level_;
        high_pass_hz_ = parameters_.high_pass_hz;
        low_pass_hz_ = parameters_.low_pass_hz;
        mix_step_ = 0.0F;
        level_step_ = 0.0F;
        high_pass_step_ = 0.0F;
        low_pass_step_ = 0.0F;
        return;
    }
    mix_ += mix_step_;
    level_ += level_step_;
    high_pass_hz_ += high_pass_step_;
    low_pass_hz_ += low_pass_step_;
}

float ConvolutionReverb::filter_wet_sample(float sample,
                                          std::uint16_t channel) noexcept {
    constexpr float kTwoPi = 6.28318530717958647692F;
    float filtered = finite_internal_sample(sample);
    if (high_pass_hz_ > 0.0F) {
        const float cutoff = std::min(high_pass_hz_, sample_rate_ * 0.45F);
        const float coefficient = std::exp(-kTwoPi * cutoff / sample_rate_);
        const float output = coefficient *
            (high_pass_previous_output_[channel] + filtered -
             high_pass_previous_input_[channel]);
        high_pass_previous_input_[channel] = filtered;
        high_pass_previous_output_[channel] = finite_internal_sample(output);
        filtered = high_pass_previous_output_[channel];
    } else {
        high_pass_previous_input_[channel] = 0.0F;
        high_pass_previous_output_[channel] = 0.0F;
    }
    if (low_pass_hz_ > 0.0F) {
        const float cutoff = std::min(low_pass_hz_, sample_rate_ * 0.45F);
        const float coefficient = std::exp(-kTwoPi * cutoff / sample_rate_);
        const float output = (1.0F - coefficient) * filtered +
                             coefficient * low_pass_previous_output_[channel];
        low_pass_previous_output_[channel] = finite_internal_sample(output);
        filtered = low_pass_previous_output_[channel];
    } else {
        low_pass_previous_output_[channel] = 0.0F;
    }
    return filtered;
}

} // namespace tabletfx
