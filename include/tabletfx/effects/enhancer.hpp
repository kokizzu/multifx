#pragma once

#include "tabletfx/effect.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace tabletfx {

enum class EnhancerMode : std::uint8_t {
    Modern,
    Classic,
    Stereoizer,
};

enum class EnhancerPhaseInvert : std::uint8_t {
    None,
    Left,
    Right,
    Both,
};

struct EnhancerParameters {
    EnhancerMode mode = EnhancerMode::Modern;
    // Project-authored side-width multiplier; 1.0 preserves existing side level.
    float width = 1.0F;
    // Project-authored amount of synthetic side information.
    float depth = 0.25F;
    float low_cut_hz = 120.0F;
    float high_cut_hz = 12000.0F;
    // The Fractal guide calls this Classic-mode Width and documents 0–20 ms.
    // The implementation exposes the guide's 0–20 ms documented range.
    float classic_delay_ms = 7.0F;
    EnhancerPhaseInvert phase_invert = EnhancerPhaseInvert::None;
    // 0 = hard left, 0.5 = center, 1 = hard right.
    float pan_left = 0.0F;
    float pan_right = 1.0F;
    // Project-authored output balance; negative favors left, positive right.
    float balance = 0.0F;
    float mix = 1.0F;
};

/*
 * Project-authored stereo enhancer with Modern, Classic, and Stereoizer modes.
 * The mode names and control concepts follow the FM9-family Blocks Guide, but
 * these DSP algorithms are independent approximations and do not claim Fractal
 * sound or parameter parity. Modern mode widens a selected band in the M/S
 * side signal while preserving the mid signal. Stereoizer makes a decorrelated
 * side signal with a small all-pass cascade. Classic mode delays only the right
 * input before applying phase, pan, and balance controls. Mono in-place buffers
 * are sanitized pass-through: channel expansion belongs in a channel converter.
 * Delay storage is allocated at construction; process() does not allocate.
 */
class Enhancer final : public Effect {
public:
    explicit Enhancer(float sample_rate = 48000.0F,
                      std::uint16_t channels = 2,
                      EnhancerParameters parameters = {})
        : sample_rate_(sample_rate), channels_(channels) {
        if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
            sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
            throw std::invalid_argument(
                "Enhancer requires 8–384 kHz and mono or stereo channels");
        }

        if (channels_ == 2) {
            const double maximum_delay_frames = std::ceil(
                static_cast<double>(sample_rate_) *
                static_cast<double>(kMaximumClassicDelayMs) / 1000.0);
            if (!std::isfinite(maximum_delay_frames) || maximum_delay_frames < 1.0 ||
                maximum_delay_frames >
                    static_cast<double>(delay_buffer_.max_size() - 2U)) {
                throw std::length_error("Enhancer delay buffer is too large");
            }
            delay_buffer_.assign(
                static_cast<std::size_t>(maximum_delay_frames) + 2U, 0.0F);
        }

        set_parameters(parameters);
        reset();
    }

    [[nodiscard]] std::string_view id() const noexcept override {
        return "enhancer";
    }

    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) + delay_buffer_.capacity() * sizeof(float);
    }

    [[nodiscard]] EnhancerParameters parameters() const noexcept {
        return parameters_;
    }

    void set_parameters(EnhancerParameters parameters) noexcept {
        if (!valid_mode(parameters.mode)) parameters.mode = EnhancerMode::Modern;
        if (!valid_phase(parameters.phase_invert)) {
            parameters.phase_invert = EnhancerPhaseInvert::None;
        }

        parameters.width = bounded(parameters.width, 1.0F, 0.0F, kMaximumWidth);
        parameters.depth = bounded(parameters.depth, 0.25F, 0.0F, kMaximumDepth);
        parameters.high_cut_hz = bounded(
            parameters.high_cut_hz, 12000.0F, kMinimumHighCutHz,
            std::min(kMaximumCutHz, sample_rate_ * 0.45F));
        const float maximum_low_cut = std::max(
            kMinimumLowCutHz,
            std::min(kMaximumLowCutHz, parameters.high_cut_hz - kCutSeparationHz));
        parameters.low_cut_hz = bounded(parameters.low_cut_hz, 120.0F,
                                        kMinimumLowCutHz, maximum_low_cut);
        parameters.classic_delay_ms = bounded(parameters.classic_delay_ms, 7.0F,
                                              0.0F, kMaximumClassicDelayMs);
        parameters.pan_left = bounded(parameters.pan_left, 0.0F, 0.0F, 1.0F);
        parameters.pan_right = bounded(parameters.pan_right, 1.0F, 0.0F, 1.0F);
        parameters.balance = bounded(parameters.balance, 0.0F, -1.0F, 1.0F);
        parameters.mix = bounded(parameters.mix, 1.0F, 0.0F, 1.0F);

        parameters_ = parameters;
        low_cut_alpha_ = lowpass_alpha(parameters_.low_cut_hz);
        high_cut_alpha_ = lowpass_alpha(parameters_.high_cut_hz);
        update_pan_matrix();
    }

    void configure(EnhancerParameters parameters) noexcept {
        set_parameters(parameters);
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override {
        if (channels == 0 || channels != channels_) return;
        const std::size_t frames = interleaved.size() / channels_;
        if (channels_ == 1) {
            for (std::size_t index = 0; index < frames; ++index) {
                if (!std::isfinite(interleaved[index])) interleaved[index] = 0.0F;
            }
            return;
        }

        for (std::size_t frame = 0; frame < frames; ++frame) {
            const std::size_t index = frame * 2U;
            const float left = finite_sample(interleaved[index]);
            const float right = finite_sample(interleaved[index + 1U]);
            if (!delay_buffer_.empty()) delay_buffer_[delay_write_] = right;

            float effected_left = left;
            float effected_right = right;
            switch (parameters_.mode) {
            case EnhancerMode::Modern:
                process_modern(left, right, effected_left, effected_right);
                break;
            case EnhancerMode::Classic:
                process_classic(left, right, effected_left, effected_right);
                break;
            case EnhancerMode::Stereoizer:
                process_stereoizer(left, right, effected_left, effected_right);
                break;
            }

            const float wet = parameters_.mix;
            const float dry = 1.0F - wet;
            interleaved[index] = bounded_output(left * dry + effected_left * wet);
            interleaved[index + 1U] =
                bounded_output(right * dry + effected_right * wet);

            if (!delay_buffer_.empty()) {
                ++delay_write_;
                if (delay_write_ == delay_buffer_.size()) delay_write_ = 0;
            }
        }
    }

    void reset() noexcept override {
        std::fill(delay_buffer_.begin(), delay_buffer_.end(), 0.0F);
        delay_write_ = 0;
        modern_mid_filter_.reset();
        modern_side_filter_.reset();
        allpass_one_.reset();
        allpass_two_.reset();
        allpass_three_.reset();
    }

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMaximumClassicDelayMs = 20.0F;
    static constexpr float kMinimumLowCutHz = 20.0F;
    static constexpr float kMaximumLowCutHz = 2000.0F;
    static constexpr float kMinimumHighCutHz = 200.0F;
    static constexpr float kMaximumCutHz = 20000.0F;
    static constexpr float kCutSeparationHz = 20.0F;
    static constexpr float kMaximumWidth = 2.0F;
    static constexpr float kMaximumDepth = 1.0F;
    static constexpr float kMaximumInternalSample = 16.0F;
    static constexpr float kPi = 3.14159265358979323846F;

    struct BandpassState {
        float low_cut_state = 0.0F;
        float high_cut_state = 0.0F;

        [[nodiscard]] float process(float input, float low_alpha,
                                    float high_alpha) noexcept {
            low_cut_state += low_alpha * (input - low_cut_state);
            const float high_passed = input - low_cut_state;
            high_cut_state += high_alpha * (high_passed - high_cut_state);
            return high_cut_state;
        }

        void reset() noexcept {
            low_cut_state = 0.0F;
            high_cut_state = 0.0F;
        }
    };

    struct AllpassState {
        float previous_input = 0.0F;
        float previous_output = 0.0F;

        [[nodiscard]] float process(float input, float coefficient) noexcept {
            const float output = coefficient * input + previous_input -
                                 coefficient * previous_output;
            previous_input = input;
            previous_output = finite_sample(output);
            return previous_output;
        }

        void reset() noexcept {
            previous_input = 0.0F;
            previous_output = 0.0F;
        }
    };

    [[nodiscard]] static bool valid_mode(EnhancerMode mode) noexcept {
        return mode == EnhancerMode::Modern || mode == EnhancerMode::Classic ||
               mode == EnhancerMode::Stereoizer;
    }

    [[nodiscard]] static bool valid_phase(EnhancerPhaseInvert phase) noexcept {
        return phase == EnhancerPhaseInvert::None ||
               phase == EnhancerPhaseInvert::Left ||
               phase == EnhancerPhaseInvert::Right ||
               phase == EnhancerPhaseInvert::Both;
    }

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept {
        const float safe_fallback = std::clamp(fallback, minimum, maximum);
        return std::isfinite(value) ? std::clamp(value, minimum, maximum)
                                    : safe_fallback;
    }

    [[nodiscard]] static float finite_sample(float sample) noexcept {
        return std::isfinite(sample) ? sample : 0.0F;
    }

    [[nodiscard]] static float bounded_output(float sample) noexcept {
        return std::isfinite(sample)
                   ? std::clamp(sample, -kMaximumInternalSample,
                                kMaximumInternalSample)
                   : 0.0F;
    }

    [[nodiscard]] float lowpass_alpha(float frequency_hz) const noexcept {
        const double exponent = -2.0 * static_cast<double>(kPi) *
                                static_cast<double>(frequency_hz) /
                                static_cast<double>(sample_rate_);
        return static_cast<float>(1.0 - std::exp(exponent));
    }

    void update_pan_matrix() noexcept {
        const float left_angle = parameters_.pan_left * (kPi * 0.5F);
        const float right_angle = parameters_.pan_right * (kPi * 0.5F);
        left_to_left_ = std::cos(left_angle);
        left_to_right_ = std::sin(left_angle);
        right_to_left_ = std::cos(right_angle);
        right_to_right_ = std::sin(right_angle);
        left_balance_gain_ = parameters_.balance > 0.0F
                                 ? 1.0F - parameters_.balance
                                 : 1.0F;
        right_balance_gain_ = parameters_.balance < 0.0F
                                  ? 1.0F + parameters_.balance
                                  : 1.0F;
    }

    void process_modern(float left, float right, float& output_left,
                        float& output_right) noexcept {
        const float mid = 0.5F * (left + right);
        const float side = 0.5F * (left - right);
        const float mid_band = modern_mid_filter_.process(
            mid, low_cut_alpha_, high_cut_alpha_);
        const float side_band = modern_side_filter_.process(
            side, low_cut_alpha_, high_cut_alpha_);
        const float shaped_side = side - side_band +
                                  parameters_.width * side_band +
                                  parameters_.depth * mid_band;
        output_left = mid + shaped_side;
        output_right = mid - shaped_side;
    }

    [[nodiscard]] float delayed_right(float input) const noexcept {
        const float delay_samples = parameters_.classic_delay_ms *
                                    (sample_rate_ * 0.001F);
        if (delay_samples <= 0.0F || delay_buffer_.empty()) return input;

        const float maximum_samples =
            static_cast<float>(delay_buffer_.size() - 2U);
        const float delay = std::clamp(delay_samples, 0.0F, maximum_samples);
        const std::size_t whole = static_cast<std::size_t>(delay);
        const float fraction = delay - static_cast<float>(whole);
        const auto read_age = [this, input](std::size_t age) noexcept {
            if (age == 0U) return input;
            if (age >= delay_buffer_.size()) return 0.0F;
            const std::size_t index = delay_write_ >= age
                                          ? delay_write_ - age
                                          : delay_buffer_.size() -
                                                (age - delay_write_);
            return delay_buffer_[index];
        };
        const float recent = read_age(whole);
        const float older = read_age(whole + 1U);
        return recent + (older - recent) * fraction;
    }

    void process_classic(float left, float right, float& output_left,
                         float& output_right) const noexcept {
        float phase_left = left;
        float phase_right = delayed_right(right);
        if (parameters_.phase_invert == EnhancerPhaseInvert::Left ||
            parameters_.phase_invert == EnhancerPhaseInvert::Both) {
            phase_left = -phase_left;
        }
        if (parameters_.phase_invert == EnhancerPhaseInvert::Right ||
            parameters_.phase_invert == EnhancerPhaseInvert::Both) {
            phase_right = -phase_right;
        }

        output_left = (phase_left * left_to_left_ +
                       phase_right * right_to_left_) * left_balance_gain_;
        output_right = (phase_left * left_to_right_ +
                        phase_right * right_to_right_) * right_balance_gain_;
    }

    void process_stereoizer(float left, float right, float& output_left,
                            float& output_right) noexcept {
        const float mid = 0.5F * (left + right);
        const float side = 0.5F * (left - right);
        const float first = allpass_one_.process(mid, 0.63F);
        const float second = allpass_two_.process(first, -0.41F);
        const float third = allpass_three_.process(second, 0.78F);
        const float decorrelated =
            0.25F * ((first - second) + (third - mid));
        const float shaped_side = parameters_.width * side +
                                  parameters_.depth * decorrelated;
        output_left = mid + shaped_side;
        output_right = mid - shaped_side;
    }

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    std::vector<float> delay_buffer_;
    std::size_t delay_write_ = 0;
    EnhancerParameters parameters_{};
    float low_cut_alpha_ = 0.0F;
    float high_cut_alpha_ = 0.0F;
    float left_to_left_ = 1.0F;
    float left_to_right_ = 0.0F;
    float right_to_left_ = 0.0F;
    float right_to_right_ = 1.0F;
    float left_balance_gain_ = 1.0F;
    float right_balance_gain_ = 1.0F;
    BandpassState modern_mid_filter_{};
    BandpassState modern_side_filter_{};
    AllpassState allpass_one_{};
    AllpassState allpass_two_{};
    AllpassState allpass_three_{};
};

} // namespace tabletfx
