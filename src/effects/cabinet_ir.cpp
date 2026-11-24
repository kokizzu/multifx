#include "tabletfx/effects/cabinet_ir.hpp"

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
            "CabinetIr sample rate must be between 8 kHz and 192 kHz");
    }
    return sample_rate;
}

std::uint16_t checked_channels(std::uint16_t channels) {
    if (channels != 1 && channels != 2) {
        throw std::invalid_argument("CabinetIr supports mono or stereo only");
    }
    return channels;
}

float finite_internal_sample(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, -16.0F, 16.0F) : 0.0F;
}

} // namespace

CabinetIr::CabinetIr(float sample_rate, std::uint16_t channels)
    : sample_rate_(checked_sample_rate(sample_rate)),
      channels_(checked_channels(channels)) {
    for (std::size_t index = 0; index < kFftFrames; ++index) {
        std::size_t value = index;
        std::size_t reversed = 0;
        for (std::size_t bits = kFftFrames; bits > 1; bits >>= 1U) {
            reversed = (reversed << 1U) | (value & 1U);
            value >>= 1U;
        }
        bit_reversed_[index] = static_cast<std::uint8_t>(reversed);
    }
    for (std::size_t index = 0; index < twiddles_.size(); ++index) {
        const float phase = -2.0F * std::numbers::pi_v<float> *
                            static_cast<float>(index) /
                            static_cast<float>(kFftFrames);
        twiddles_[index] = Complex(std::cos(phase), std::sin(phase));
    }
}

std::size_t CabinetIr::memory_bytes() const noexcept {
    return sizeof(*this) +
           (impulse_spectra_.capacity() + input_spectra_.capacity()) *
               sizeof(Complex);
}

bool CabinetIr::load_impulse_response(std::span<const float> mono_pcm,
                                      std::uint16_t ir_channels,
                                      std::uint32_t ir_sample_rate,
                                      std::uint16_t bits_per_sample) noexcept {
    if (ir_channels != 1 || ir_sample_rate != kRequiredIrSampleRate ||
        bits_per_sample != kRequiredIrBitsPerSample ||
        mono_pcm.size() != kRequiredIrFrames) {
        return false;
    }
    if (!std::all_of(mono_pcm.begin(), mono_pcm.end(), [](float value) {
            return std::isfinite(value) && std::abs(value) <= 16.0F;
        })) {
        return false;
    }

    const double target_frame_count =
        static_cast<double>(kRequiredIrFrames) *
        static_cast<double>(sample_rate_) /
        static_cast<double>(kRequiredIrSampleRate);
    if (!std::isfinite(target_frame_count) || target_frame_count < 1.0 ||
        target_frame_count > static_cast<double>(kMaximumResampledFrames)) {
        return false;
    }
    const std::size_t target_frames = static_cast<std::size_t>(
        std::max<long long>(1, std::llround(target_frame_count)));
    const std::size_t partitions =
        (target_frames + kPartitionFrames - 1) / kPartitionFrames;

    std::vector<float> resampled;
    std::vector<Complex> new_impulse_spectra;
    std::vector<Complex> new_input_spectra;
    try {
        resampled.resize(target_frames);
        new_impulse_spectra.resize(partitions * kSpectrumBins);
        new_input_spectra.assign(
            static_cast<std::size_t>(channels_) * partitions * kSpectrumBins,
            Complex{});
    } catch (const std::bad_alloc&) {
        return false;
    } catch (const std::length_error&) {
        return false;
    }

    const double source_step = static_cast<double>(kRequiredIrSampleRate) /
                               static_cast<double>(sample_rate_);
    for (std::size_t frame = 0; frame < target_frames; ++frame) {
        const double source_position = static_cast<double>(frame) * source_step;
        const std::size_t first = std::min(
            static_cast<std::size_t>(source_position), kRequiredIrFrames - 1);
        const std::size_t second = std::min(first + 1, kRequiredIrFrames - 1);
        const float fraction = static_cast<float>(
            std::clamp(source_position - static_cast<double>(first), 0.0, 1.0));
        resampled[frame] = mono_pcm[first] +
                           (mono_pcm[second] - mono_pcm[first]) * fraction;
    }

    for (std::size_t partition = 0; partition < partitions; ++partition) {
        transform_buffer_.fill(Complex{});
        const std::size_t start = partition * kPartitionFrames;
        const std::size_t count = std::min(kPartitionFrames, target_frames - start);
        for (std::size_t frame = 0; frame < count; ++frame) {
            transform_buffer_[frame] = resampled[start + frame];
        }
        transform(transform_buffer_, false);
        const std::size_t destination = partition * kSpectrumBins;
        std::copy_n(transform_buffer_.begin(), kSpectrumBins,
                    new_impulse_spectra.begin() +
                        static_cast<std::ptrdiff_t>(destination));
    }

    impulse_spectra_.swap(new_impulse_spectra);
    input_spectra_.swap(new_input_spectra);
    impulse_frames_ = target_frames;
    partition_count_ = partitions;
    reset();
    return true;
}

void CabinetIr::clear_impulse_response() noexcept {
    std::vector<Complex>().swap(impulse_spectra_);
    std::vector<Complex>().swap(input_spectra_);
    impulse_frames_ = 0;
    partition_count_ = 0;
    history_write_partition_ = 0;
    reset();
}

void CabinetIr::process(std::span<float> interleaved,
                        std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0 || partition_count_ == 0) return;
    bypassed_ = false;

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = frame_cursor_ * channels_ + channel;
            const float input = finite_internal_sample(
                interleaved[frame * channels_ + channel]);
            input_block_[index] = input;
            interleaved[frame * channels_ + channel] = output_block_[index];
        }
        ++frame_cursor_;
        if (frame_cursor_ == kPartitionFrames) {
            process_partition();
            frame_cursor_ = 0;
        }
    }
}

void CabinetIr::process_bypassed(std::span<float>,
                                 std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;
    if (!bypassed_) {
        reset();
        bypassed_ = true;
    }
}

void CabinetIr::reset() noexcept {
    std::fill(input_spectra_.begin(), input_spectra_.end(), Complex{});
    input_block_.fill(0.0F);
    output_block_.fill(0.0F);
    overlap_.fill(0.0F);
    history_write_partition_ = 0;
    frame_cursor_ = 0;
    bypassed_ = false;
}

void CabinetIr::transform(std::array<Complex, kFftFrames>& values,
                          bool inverse) const noexcept {
    for (std::size_t index = 0; index < kFftFrames; ++index) {
        const std::size_t reversed = bit_reversed_[index];
        if (reversed > index) std::swap(values[index], values[reversed]);
    }

    for (std::size_t length = 2; length <= kFftFrames; length <<= 1U) {
        const std::size_t half = length / 2;
        const std::size_t twiddle_stride = kFftFrames / length;
        for (std::size_t block = 0; block < kFftFrames; block += length) {
            for (std::size_t offset = 0; offset < half; ++offset) {
                Complex twiddle = twiddles_[offset * twiddle_stride];
                if (inverse) twiddle = std::conj(twiddle);
                const Complex even = values[block + offset];
                const Complex odd =
                    values[block + offset + half] * twiddle;
                values[block + offset] = even + odd;
                values[block + offset + half] = even - odd;
            }
        }
    }

    if (inverse) {
        constexpr float scale = 1.0F / static_cast<float>(kFftFrames);
        for (Complex& value : values) value *= scale;
    }
}

void CabinetIr::process_partition() noexcept {
    for (std::uint16_t channel = 0; channel < channels_; ++channel) {
        transform_buffer_.fill(Complex{});
        for (std::size_t frame = 0; frame < kPartitionFrames; ++frame) {
            transform_buffer_[frame] =
                input_block_[frame * channels_ + channel];
        }
        transform(transform_buffer_, false);

        const std::size_t input_offset =
            (static_cast<std::size_t>(channel) * partition_count_ +
             history_write_partition_) * kSpectrumBins;
        std::copy_n(transform_buffer_.begin(), kSpectrumBins,
                    input_spectra_.begin() +
                        static_cast<std::ptrdiff_t>(input_offset));

        sum_buffer_.fill(Complex{});
        for (std::size_t partition = 0; partition < partition_count_; ++partition) {
            const std::size_t history_partition =
                (history_write_partition_ + partition_count_ - partition) %
                partition_count_;
            const std::size_t ir_offset = partition * kSpectrumBins;
            const std::size_t signal_offset =
                (static_cast<std::size_t>(channel) * partition_count_ +
                 history_partition) * kSpectrumBins;
            for (std::size_t bin = 0; bin < kSpectrumBins; ++bin) {
                sum_buffer_[bin] += impulse_spectra_[ir_offset + bin] *
                                    input_spectra_[signal_offset + bin];
            }
        }

        for (std::size_t bin = 0; bin < kSpectrumBins; ++bin) {
            transform_buffer_[bin] = sum_buffer_[bin];
        }
        for (std::size_t bin = 1; bin < kPartitionFrames; ++bin) {
            transform_buffer_[kFftFrames - bin] =
                std::conj(sum_buffer_[bin]);
        }
        transform(transform_buffer_, true);

        for (std::size_t frame = 0; frame < kPartitionFrames; ++frame) {
            const std::size_t index = frame * channels_ + channel;
            output_block_[index] = finite_internal_sample(
                transform_buffer_[frame].real() + overlap_[index]);
            overlap_[index] = finite_internal_sample(
                transform_buffer_[frame + kPartitionFrames].real());
        }
    }

    ++history_write_partition_;
    if (history_write_partition_ == partition_count_) {
        history_write_partition_ = 0;
    }
}

} // namespace tabletfx
