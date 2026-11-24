#include "tabletfx/effects/poly_pitch_shifter.hpp"

#include <StftPitchShift/RFFT.h>
#include <StftPitchShift/STFT.h>
#include <StftPitchShift/StftPitchShiftCore.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace tabletfx {
namespace {

constexpr std::size_t kDftSize = 1024;
constexpr std::size_t kAnalysisSamples = 2 * kDftSize;
constexpr std::size_t kProcessFrames = 128;
constexpr std::size_t kOverlap = 2;
constexpr std::size_t kAnalysisHop = kProcessFrames / kOverlap;
constexpr std::size_t kProcessorBufferSamples = kAnalysisSamples + kProcessFrames;
constexpr std::size_t kOutputQueueFrames = 2 * kProcessFrames;

float checked_sample_rate(float sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate < 8000.0F ||
        sample_rate > 384000.0F) {
        throw std::invalid_argument("PolyPitchShifter sample rate must be 8–384 kHz");
    }
    return sample_rate;
}

std::uint16_t checked_channels(std::uint16_t channels) {
    if (channels == 0 || channels > 2) {
        throw std::invalid_argument("PolyPitchShifter supports one or two channels");
    }
    return channels;
}

float checked_max_shift(float semitones) {
    if (!std::isfinite(semitones) ||
        (semitones != 12.0F && semitones != 24.0F &&
         semitones != 60.0F)) {
        throw std::invalid_argument(
            "PolyPitchShifter maximum range must be 12, 24, or 60 semitones");
    }
    return semitones;
}

} // namespace

struct PolyPitchShifter::Impl {
    struct ChannelProcessor {
        using Core = stftpitchshift::StftPitchShiftCore<double>;
        using Transform = stftpitchshift::STFT<double>;

        ChannelProcessor(std::shared_ptr<stftpitchshift::FFT> fft,
                         double sample_rate)
            : input(kProcessorBufferSamples, 0.0),
              output(kProcessorBufferSamples, 0.0),
              transform(fft, std::make_tuple(kAnalysisSamples, kProcessFrames),
                        kAnalysisHop),
              core(std::move(fft), std::make_tuple(kAnalysisSamples, kProcessFrames),
                   kAnalysisHop, sample_rate) {
            core.factors(factor);
            prewarm();
        }

        void set_factor(double value) {
            factor[0] = value;
            core.factors(factor);
        }

        void reset() noexcept {
            std::fill(input.begin(), input.end(), 0.0);
            std::fill(output.begin(), output.end(), 0.0);
            core.reset();
        }

        void process(std::span<const float, kProcessFrames> source,
                     std::span<float, kProcessFrames> destination) {
            std::copy(input.begin() + static_cast<std::ptrdiff_t>(kProcessFrames),
                      input.end(), input.begin());
            std::transform(source.begin(), source.end(),
                           input.begin() + static_cast<std::ptrdiff_t>(kAnalysisSamples),
                           [](float value) { return static_cast<double>(value); });

            transform(input, output, [this](std::span<std::complex<double>> spectrum) {
                core.shiftpitch(spectrum);
            });

            const auto output_begin = output.begin() +
                static_cast<std::ptrdiff_t>(kAnalysisSamples - kProcessFrames);
            std::transform(output_begin, output_begin +
                               static_cast<std::ptrdiff_t>(kProcessFrames),
                           destination.begin(),
                           [](double value) {
                               return std::isfinite(value) ? static_cast<float>(value) : 0.0F;
                           });

            std::copy(output.begin() + static_cast<std::ptrdiff_t>(kProcessFrames),
                      output.end(), output.begin());
            std::fill(output.begin() + static_cast<std::ptrdiff_t>(kAnalysisSamples),
                      output.end(), 0.0);
        }

        std::size_t storage_bytes() const noexcept {
            constexpr std::size_t bins = kAnalysisSamples / 2 + 1;
            const std::size_t channel_buffers =
                2 * input.capacity() * sizeof(double) +
                2 * kAnalysisSamples * sizeof(double) +
                kAnalysisSamples * sizeof(double) +
                bins * sizeof(std::complex<double>) +
                3 * bins * sizeof(double) +
                bins * sizeof(double) +
                bins * sizeof(std::complex<double>) +
                kAnalysisSamples * sizeof(double) +
                bins * sizeof(std::complex<double>) +
                sizeof(factor[0]);
            return sizeof(*this) + channel_buffers;
        }

    private:
        void prewarm() {
            std::array<double, kProcessorBufferSamples> warm_input{};
            std::array<double, kProcessorBufferSamples> warm_output{};
            transform(warm_input, warm_output,
                      [this](std::span<std::complex<double>> spectrum) {
                          core.shiftpitch(spectrum);
                      });
            core.factors(factor);
        }

        std::vector<double> input;
        std::vector<double> output;
        Transform transform;
        Core core;
        std::vector<double> factor{1.0};
    };

    explicit Impl(float sample_rate, std::uint16_t channel_count)
        : channels(channel_count), fft(std::make_shared<stftpitchshift::RFFT>()) {
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            processors[channel] = std::make_unique<ChannelProcessor>(fft, sample_rate);
        }
        output_count = kProcessFrames;
    }

    void set_factor(double factor) {
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            processors[channel]->set_factor(factor);
        }
    }

    void reset() noexcept {
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            processors[channel]->reset();
        }
        input_block.fill(0.0F);
        output_block.fill(0.0F);
        channel_input.fill(0.0F);
        channel_output.fill(0.0F);
        output_queue.fill(0.0F);
        input_count = 0;
        output_head = 0;
        output_count = kProcessFrames;
    }

    void process_block() {
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            for (std::size_t frame = 0; frame < kProcessFrames; ++frame) {
                channel_input[frame] = input_block[frame * channels + channel];
            }
            processors[channel]->process(channel_input, channel_output);
            for (std::size_t frame = 0; frame < kProcessFrames; ++frame) {
                output_block[frame * channels + channel] = channel_output[frame];
            }
        }

        for (std::size_t frame = 0; frame < kProcessFrames; ++frame) {
            const std::size_t tail = (output_head + output_count) % kOutputQueueFrames;
            for (std::uint16_t channel = 0; channel < channels; ++channel) {
                output_queue[tail * channels + channel] =
                    output_block[frame * channels + channel];
            }
            ++output_count;
        }
    }

    std::size_t storage_bytes() const noexcept {
        std::size_t total = sizeof(*this);
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            total += processors[channel]->storage_bytes();
        }
        constexpr std::size_t fft_size = kAnalysisSamples / 2;
        total += fft_size * (sizeof(std::complex<double>) + sizeof(std::size_t) +
                             6 * sizeof(std::complex<double>));
        return total;
    }

    std::uint16_t channels;
    std::shared_ptr<stftpitchshift::FFT> fft;
    std::array<std::unique_ptr<ChannelProcessor>, 2> processors{};
    std::array<float, kProcessFrames * 2> input_block{};
    std::array<float, kProcessFrames * 2> output_block{};
    std::array<float, kProcessFrames> channel_input{};
    std::array<float, kProcessFrames> channel_output{};
    std::array<float, kOutputQueueFrames * 2> output_queue{};
    std::size_t input_count = 0;
    std::size_t output_head = 0;
    std::size_t output_count = 0;
};

PolyPitchShifter::PolyPitchShifter(float sample_rate, std::uint16_t channels,
                                   float max_shift_semitones)
    : impl_(std::make_unique<Impl>(checked_sample_rate(sample_rate),
                                  checked_channels(channels))),
      sample_rate_(sample_rate), channels_(channels),
      max_shift_semitones_(checked_max_shift(max_shift_semitones)) {}

PolyPitchShifter::~PolyPitchShifter() = default;

std::size_t PolyPitchShifter::memory_bytes() const noexcept {
    return sizeof(*this) + impl_->storage_bytes();
}

float PolyPitchShifter::algorithmic_latency_ms() const noexcept {
    return 1000.0F * static_cast<float>(2 * kProcessFrames) / sample_rate_;
}

std::uint32_t PolyPitchShifter::analysis_block_samples() const noexcept {
    return static_cast<std::uint32_t>(kAnalysisSamples);
}

std::uint32_t PolyPitchShifter::analysis_interval_samples() const noexcept {
    return static_cast<std::uint32_t>(kAnalysisHop);
}

void PolyPitchShifter::set_pitch_semitones(float semitones) noexcept {
    if (!std::isfinite(semitones)) return;
    pitch_semitones_ = std::clamp(semitones, -max_shift_semitones_,
                                  max_shift_semitones_);
    impl_->set_factor(std::exp2(static_cast<double>(pitch_semitones_) / 12.0));
}

void PolyPitchShifter::process(std::span<float> interleaved,
                               std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) {
        return;
    }

    const std::size_t total_frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < total_frames; ++frame) {
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            impl_->input_block[impl_->input_count * channels_ + channel] =
                interleaved[frame * channels_ + channel];
        }
        ++impl_->input_count;
        if (impl_->input_count == kProcessFrames) {
            impl_->process_block();
            impl_->input_count = 0;
        }

        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            interleaved[frame * channels_ + channel] =
                impl_->output_queue[impl_->output_head * channels_ + channel];
        }
        impl_->output_head = (impl_->output_head + 1) % kOutputQueueFrames;
        --impl_->output_count;
    }
}

void PolyPitchShifter::reset() noexcept {
    impl_->reset();
}

} // namespace tabletfx
