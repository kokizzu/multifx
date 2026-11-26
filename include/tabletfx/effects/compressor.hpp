#pragma once

#include "tabletfx/effect.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace tabletfx {

struct CompressorParameters {
    float threshold_db = -18.0F;
    float ratio = 3.0F;
    float knee_db = 6.0F;
    float attack_ms = 8.0F;
    float release_ms = 100.0F;
    float makeup_db = 0.0F;
};

// A low-cost, stereo-linked feed-forward compressor. Configure it while preparing
// a graph (off the real-time callback); process() uses only fixed state and the
// caller's buffer. The same detector and gain are applied to every channel so a
// stereo image does not move when one channel has a stronger transient.
class Compressor final : public Effect {
public:
    explicit Compressor(float sample_rate = 48000.0F,
                        CompressorParameters parameters = {}) noexcept {
        configure(sample_rate, parameters);
    }

    [[nodiscard]] std::string_view id() const noexcept override { return "compressor"; }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override { return sizeof(*this); }

    // Safe to call off the audio thread before activating a prepared processor.
    // Parameter changes preserve the detector and gain envelope; reset() is
    // available when a fresh state is required.
    void configure(float sample_rate, CompressorParameters parameters) noexcept {
        if (!std::isfinite(sample_rate) || sample_rate < 8000.0F || sample_rate > 192000.0F) {
            sample_rate = 48000.0F;
        }
        sample_rate_ = sample_rate;

        parameters_.threshold_db = bounded(parameters.threshold_db, -18.0F, -60.0F, 0.0F);
        parameters_.ratio = bounded(parameters.ratio, 3.0F, 1.0F, 20.0F);
        parameters_.knee_db = bounded(parameters.knee_db, 6.0F, 0.0F, 18.0F);
        parameters_.attack_ms = bounded(parameters.attack_ms, 8.0F, 0.1F, 200.0F);
        parameters_.release_ms = bounded(parameters.release_ms, 100.0F, 5.0F, 2000.0F);
        parameters_.makeup_db = bounded(parameters.makeup_db, 0.0F, -12.0F, 18.0F);

        makeup_gain_ = decibels_to_linear(parameters_.makeup_db);
        attack_coefficient_ = time_coefficient(parameters_.attack_ms, sample_rate_);
        release_coefficient_ = time_coefficient(parameters_.release_ms, sample_rate_);
        compression_slope_ = 1.0F - 1.0F / parameters_.ratio;
        configured_ = true;
        if (!state_initialized_) {
            gain_ = makeup_gain_;
            state_initialized_ = true;
        }
    }

    void set_parameters(CompressorParameters parameters) noexcept {
        configure(sample_rate_, parameters);
    }

    [[nodiscard]] CompressorParameters parameters() const noexcept { return parameters_; }

    void process(std::span<float> interleaved, std::uint16_t channels) noexcept override {
        process_with_detector(
            interleaved,
            std::span<const float>(interleaved.data(), interleaved.size()),
            channels);
    }

    // Uses a separate detector signal while applying gain to the program audio.
    // The caller supplies equally sized, interleaved buffers in real-time use;
    // if they differ, only their shared complete frames are processed. Audio
    // and detector may alias: each frame's detector is read before its audio is
    // changed. No scratch storage or allocations are used.
    void process_with_detector(std::span<float> interleaved,
                               std::span<const float> detector,
                               std::uint16_t channels) noexcept {
        if (!configured_ || channels == 0) {
            return;
        }

        const std::size_t frames = interleaved.size() / channels;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const std::size_t offset = frame * channels;
            float linked_peak = 0.0F;
            for (std::uint16_t channel = 0; channel < channels; ++channel) {
                float& sample = interleaved[offset + channel];
                if (!std::isfinite(sample)) {
                    sample = 0.0F;
                }
                linked_peak = std::max(linked_peak, std::abs(sample));
            }

            const float detector_coefficient = linked_peak > envelope_
                ? attack_coefficient_
                : release_coefficient_;
            envelope_ = detector_coefficient * envelope_ +
                        (1.0F - detector_coefficient) * linked_peak;
            if (envelope_ < kDenormalGuard) {
                envelope_ = 0.0F;
            }

            const float desired_gain = makeup_gain_ * compression_gain(envelope_);
            const float gain_coefficient = desired_gain < gain_
                ? attack_coefficient_
                : release_coefficient_;
            gain_ = gain_coefficient * gain_ + (1.0F - gain_coefficient) * desired_gain;
            if (gain_ < kDenormalGuard) {
                gain_ = 0.0F;
            }

            for (std::uint16_t channel = 0; channel < channels; ++channel) {
                interleaved[offset + channel] *= gain_;
            }
        }
    }

    void reset() noexcept override {
        envelope_ = 0.0F;
        gain_ = makeup_gain_;
    }

private:
    static constexpr float kDenormalGuard = 1.0e-20F;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept {
        return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
    }

    [[nodiscard]] static float decibels_to_linear(float db) noexcept {
        return std::pow(10.0F, db / 20.0F);
    }

    [[nodiscard]] static float time_coefficient(float milliseconds, float sample_rate) noexcept {
        return std::exp(-1.0F / (milliseconds * 0.001F * sample_rate));
    }

    [[nodiscard]] float compression_gain(float detector_level) const noexcept {
        if (detector_level <= 0.0F || compression_slope_ <= 0.0F) {
            return 1.0F;
        }

        const float level_db = 20.0F * std::log10(detector_level);
        const float knee = parameters_.knee_db;
        const float threshold = parameters_.threshold_db;
        float gain_reduction_db = 0.0F;

        if (knee <= 0.0F) {
            if (level_db > threshold) {
                gain_reduction_db = -compression_slope_ * (level_db - threshold);
            }
        } else {
            const float lower_knee = threshold - 0.5F * knee;
            const float upper_knee = threshold + 0.5F * knee;
            if (level_db >= upper_knee) {
                gain_reduction_db = -compression_slope_ * (level_db - threshold);
            } else if (level_db > lower_knee) {
                const float knee_position = level_db - lower_knee;
                gain_reduction_db = -compression_slope_ * knee_position * knee_position /
                                    (2.0F * knee);
            }
        }

        return decibels_to_linear(gain_reduction_db);
    }

    float sample_rate_ = 48000.0F;
    CompressorParameters parameters_{};
    float makeup_gain_ = 1.0F;
    float attack_coefficient_ = 0.0F;
    float release_coefficient_ = 0.0F;
    float compression_slope_ = 2.0F / 3.0F;
    float envelope_ = 0.0F;
    float gain_ = 1.0F;
    bool configured_ = false;
    bool state_initialized_ = false;
};

} // namespace tabletfx
