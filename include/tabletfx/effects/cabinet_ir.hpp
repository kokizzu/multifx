#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <complex>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

// Project-authored mono cabinet convolution. The XR1 FAQ specifies a 24-bit,
// 48 kHz WAV of exactly 2,048 samples for its Cabinet IR Loader. Its public
// material does not define the dual-loader channel behavior, so this effect
// accepts one mono IR and applies it independently to mono or stereo input.
// The processor uses 64-frame uniform FFT partitions, separate from the
// 1,024-frame room/reverb ConvolutionReverb. It adds one partition of latency
// and keeps a single 2,048-sample 48 kHz mono IR below 60 KiB of owned memory
// for a stereo host. This is not an XR1 DSP or CPU claim.
class CabinetIr final : public Effect {
public:
    static constexpr std::uint32_t kRequiredIrSampleRate = 48000;
    static constexpr std::uint16_t kRequiredIrBitsPerSample = 24;
    static constexpr std::size_t kRequiredIrFrames = 2048;
    static constexpr std::size_t kPartitionFrames = 64;
    static constexpr std::size_t kFftFrames = 2 * kPartitionFrames;
    static constexpr std::size_t kSpectrumBins = kPartitionFrames + 1;
    static constexpr std::size_t kMaximumResampledFrames = 8192;
    static constexpr std::size_t kMaximumPartitions =
        kMaximumResampledFrames / kPartitionFrames;
    static constexpr std::size_t kLatencyFrames = kPartitionFrames;

    explicit CabinetIr(float sample_rate = 48000.0F,
                       std::uint16_t channels = 2);

    [[nodiscard]] std::string_view id() const noexcept override {
        return "cabinet-ir";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] bool has_impulse_response() const noexcept {
        return impulse_frames_ != 0;
    }
    // Returns the resampled frame count for the configured processing rate.
    [[nodiscard]] std::size_t impulse_frames() const noexcept {
        return impulse_frames_;
    }

    // The WAV reader must verify RIFF/PCM structure and provide the original
    // bits-per-sample value. This method enforces the manufacturer's documented
    // mono, 24-bit, 48 kHz, 2,048-frame shape and resamples the accepted IR to
    // the processor rate off the audio callback. Call only while this processor
    // is not active in process(); the owning graph should be built/swapped off
    // the real-time thread.
    [[nodiscard]] bool load_impulse_response(std::span<const float> mono_pcm,
                                             std::uint16_t ir_channels,
                                             std::uint32_t ir_sample_rate,
                                             std::uint16_t bits_per_sample) noexcept;
    // Like loading, clear only while the processor is inactive; this releases
    // coefficient/history storage and therefore may deallocate.
    void clear_impulse_response() noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    // Cabinet IR has no spillover mode; bypass is transparent and drops the
    // finite convolution history so re-enabling it cannot replay stale input.
    void process_bypassed(std::span<float> interleaved,
                          std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    using Complex = std::complex<float>;

    void transform(std::array<Complex, kFftFrames>& values,
                   bool inverse) const noexcept;
    void process_partition() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    // Mono IR spectra and channel-major input spectra, allocated at IR load.
    std::vector<Complex> impulse_spectra_;
    std::vector<Complex> input_spectra_;
    std::array<Complex, kFftFrames> transform_buffer_{};
    std::array<Complex, kFftFrames> sum_buffer_{};
    std::array<Complex, kPartitionFrames> twiddles_{};
    std::array<std::uint8_t, kFftFrames> bit_reversed_{};
    std::array<float, kPartitionFrames * 2> input_block_{};
    std::array<float, kPartitionFrames * 2> output_block_{};
    std::array<float, kPartitionFrames * 2> overlap_{};
    std::size_t impulse_frames_ = 0;
    std::size_t partition_count_ = 0;
    std::size_t history_write_partition_ = 0;
    std::size_t frame_cursor_ = 0;
    bool bypassed_ = false;
};

} // namespace tabletfx
