#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tabletfx {

struct ConvolutionReverbParameters {
    // Project-authored wet controls; these are not copied from a device.
    float mix = 0.25F;
    float level_db = 0.0F;
};

// User-IR convolution reverb using uniform 128-frame overlap-add partitions.
// Loading and FFT preparation happen outside process(); audio processing uses
// fixed-capacity storage and adds one partition (128 frames) of latency.
// Mono IRs are applied independently to each output channel. Stereo IRs use
// the corresponding left/right IR on each channel; cross-channel matrix
// convolution is not implemented.
class ConvolutionReverb final : public Effect {
public:
    static constexpr std::size_t kMaximumImpulseFrames = 24000;
    static constexpr std::size_t kPartitionFrames = 128;
    static constexpr std::size_t kFftFrames = 2 * kPartitionFrames;
    static constexpr std::size_t kMaximumPartitions =
        (kMaximumImpulseFrames + kPartitionFrames - 1) / kPartitionFrames;

    explicit ConvolutionReverb(float sample_rate = 48000.0F,
                               std::uint16_t channels = 2,
                               ConvolutionReverbParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "convolution-reverb";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;
    [[nodiscard]] ConvolutionReverbParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] std::size_t impulse_frames() const noexcept {
        return impulse_frames_;
    }
    [[nodiscard]] std::size_t partition_count() const noexcept {
        return partition_count_;
    }
    [[nodiscard]] static constexpr std::size_t maximum_impulse_frames() noexcept {
        return kMaximumImpulseFrames;
    }

    void configure(ConvolutionReverbParameters parameters) noexcept;
    // IR input is interleaved mono or stereo float PCM. Returns false without
    // replacing the current IR when shape, rate, length, or samples are invalid.
    [[nodiscard]] bool load_impulse_response(std::span<const float> interleaved,
                                             std::uint16_t ir_channels,
                                             std::uint32_t ir_sample_rate) noexcept;
    void clear_impulse_response() noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    using Complex = std::complex<float>;
    static constexpr float kControlSmoothingMs = 10.0F;
    static constexpr float kMaximumInternalSample = 16.0F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] float impulse_sample(std::span<const float> interleaved,
                                       double source_position,
                                       std::size_t source_frames,
                                       std::uint16_t ir_channels,
                                       std::uint16_t output_channel) const noexcept;
    void transform(std::array<Complex, kFftFrames>& values,
                   bool inverse) const noexcept;
    void process_partition() noexcept;
    void advance_controls() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    ConvolutionReverbParameters parameters_{};
    std::vector<Complex> impulse_spectra_;
    std::vector<Complex> input_spectra_;
    std::array<Complex, kFftFrames> transform_buffer_{};
    std::array<Complex, kFftFrames> sum_buffer_{};
    std::array<Complex, kFftFrames / 2> twiddles_{};
    std::array<std::uint16_t, kFftFrames> bit_reversed_{};
    std::array<float, 2 * kPartitionFrames> input_block_{};
    std::array<float, 2 * kPartitionFrames> output_block_{};
    std::array<float, 2 * kPartitionFrames> overlap_{};
    std::array<float, 2 * kPartitionFrames> dry_delay_{};
    std::size_t impulse_frames_ = 0;
    std::size_t partition_count_ = 0;
    std::size_t history_write_partition_ = 0;
    std::size_t frame_cursor_ = 0;
    float mix_ = 0.25F;
    float mix_step_ = 0.0F;
    float level_ = 1.0F;
    float target_level_ = 1.0F;
    float level_step_ = 0.0F;
    std::size_t ramp_total_ = 1;
    std::size_t ramp_remaining_ = 0;
};

} // namespace tabletfx
