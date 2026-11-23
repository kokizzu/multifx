#pragma once

#include "tabletfx/effect.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace tabletfx {

// Project-authored Deluxe Looper core inspired by the Helix Stadium workflow.
// Cutoff, level, and overdub ranges below are project choices; Line 6 does not
// publish numeric ranges/defaults for those controls in its Looper manual.
class DeluxeLooper final : public Effect {
public:
    enum class State : std::uint8_t {
        Stopped = 0,
        Recording = 1,
        Playing = 2,
        Overdubbing = 3,
    };

    static constexpr std::uint32_t kReferenceSampleRate = 48'000U;
    static constexpr std::size_t kMonoCapacityFrames =
        static_cast<std::size_t>(kReferenceSampleRate) * 120U;
    static constexpr std::size_t kStereoCapacityFrames =
        static_cast<std::size_t>(kReferenceSampleRate) * 60U;
    static constexpr float kPlaybackLevelMin = 0.0F;
    static constexpr float kPlaybackLevelMax = 1.0F;
    static constexpr float kOverdubDecayMinDb = -24.0F;
    static constexpr float kOverdubDecayMaxDb = 0.0F;
    static constexpr float kCutoffMinHz = 0.0F;
    static constexpr float kCutoffAbsoluteMaxHz = 20'000.0F;

    explicit DeluxeLooper(std::uint16_t channels = 2U,
                          float sample_rate = 48'000.0F)
        : channels_(validate_channels(channels)),
          sample_rate_(validate_sample_rate(sample_rate)),
          capacity_frames_(channels_ == 1U ? kMonoCapacityFrames
                                           : kStereoCapacityFrames),
          capacity_samples_(capacity_frames_ * channels_),
          loop_(std::make_unique<float[]>(capacity_samples_)),
          undo_buffer_(std::make_unique<float[]>(capacity_samples_)) {
        update_filter_coefficients();
    }

    [[nodiscard]] std::string_view id() const noexcept override {
        return "deluxe-looper";
    }

    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        // Includes this object and both fixed sample buffers. Allocator
        // bookkeeping is implementation-specific and is not included.
        return sizeof(*this) + capacity_samples_ * sizeof(float) * 2U;
    }

    void reset() noexcept override {
        clear();
        playback_position_ = 0.0;
        overdub_gain_ = 1.0F;
        undo_gain_ = 1.0F;
        clear_filter_state();
    }

    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] std::size_t loop_frames() const noexcept { return loop_frames_; }
    [[nodiscard]] std::size_t capacity_frames() const noexcept {
        return capacity_frames_;
    }
    [[nodiscard]] bool reverse() const noexcept { return reverse_; }
    [[nodiscard]] bool half_speed() const noexcept { return half_speed_; }
    [[nodiscard]] bool one_shot() const noexcept { return one_shot_; }
    [[nodiscard]] float playback_level() const noexcept {
        return playback_level_;
    }
    [[nodiscard]] float overdub_decay_db() const noexcept {
        return overdub_decay_db_;
    }
    [[nodiscard]] float low_cut_hz() const noexcept { return low_cut_hz_; }
    [[nodiscard]] float high_cut_hz() const noexcept { return high_cut_hz_; }
    [[nodiscard]] float maximum_cutoff_hz() const noexcept {
        return std::min(kCutoffAbsoluteMaxHz, 0.45F * sample_rate_);
    }

    // Start a new loop recording. Input remains audible unchanged while it is
    // captured. A recording can be finalized with stop() or play().
    [[nodiscard]] bool record() noexcept {
        if (state_ == State::Recording || loop_frames_ != 0U) {
            return false;
        }
        state_ = State::Recording;
        write_frame_ = 0U;
        loop_frames_ = 0U;
        playback_position_ = 0.0;
        one_shot_ = false;
        undo_available_ = false;
        redo_available_ = false;
        overdub_gain_ = 1.0F;
        clear_filter_state();
        return true;
    }

    // Start cyclic playback at the beginning (or end when reversed).
    [[nodiscard]] bool play() noexcept {
        if (state_ == State::Recording) {
            finish_recording();
        }
        if (loop_frames_ == 0U) {
            return false;
        }
        state_ = State::Playing;
        one_shot_ = false;
        position_at_start();
        overdub_gain_ = 1.0F;
        clear_filter_state();
        return true;
    }

    // Play exactly one captured loop, then enter stopped state. This is a
    // project-authored behavior corresponding to Helix Stadium's Play x1.
    [[nodiscard]] bool play_once() noexcept {
        if (state_ == State::Recording) {
            finish_recording();
        }
        if (loop_frames_ == 0U) {
            return false;
        }
        state_ = State::Playing;
        one_shot_ = true;
        position_at_start();
        overdub_gain_ = 1.0F;
        clear_filter_state();
        return true;
    }

    // Enter overdub while playing, or start playback and overdub if stopped.
    // A one-level snapshot allows one undo and one redo without another buffer.
    [[nodiscard]] bool overdub() noexcept {
        if (state_ == State::Recording) {
            finish_recording();
        }
        if (loop_frames_ == 0U) {
            return false;
        }
        if (state_ != State::Playing && state_ != State::Overdubbing) {
            position_at_start();
        }
        std::copy_n(loop_.get(), loop_frames_ * channels_, undo_buffer_.get());
        undo_gain_ = overdub_gain_;
        undo_available_ = true;
        redo_available_ = false;
        state_ = State::Overdubbing;
        one_shot_ = false;
        return true;
    }

    // Finishing a recording starts playback. Finishing overdub leaves the loop
    // playing; stopping ordinary playback preserves the recorded loop.
    void stop() noexcept {
        if (state_ == State::Recording) {
            finish_recording();
            if (loop_frames_ != 0U) {
                state_ = State::Playing;
                one_shot_ = false;
                position_at_start();
            }
            return;
        }
        if (state_ == State::Overdubbing) {
            state_ = State::Playing;
            one_shot_ = false;
            return;
        }
        if (state_ == State::Playing) {
            state_ = State::Stopped;
            one_shot_ = false;
        }
    }

    [[nodiscard]] bool undo() noexcept {
        if (!undo_available_ || loop_frames_ == 0U) {
            return false;
        }
        loop_.swap(undo_buffer_);
        std::swap(overdub_gain_, undo_gain_);
        undo_available_ = false;
        redo_available_ = true;
        return true;
    }

    [[nodiscard]] bool redo() noexcept {
        if (!redo_available_ || loop_frames_ == 0U) {
            return false;
        }
        loop_.swap(undo_buffer_);
        std::swap(overdub_gain_, undo_gain_);
        redo_available_ = false;
        undo_available_ = true;
        return true;
    }

    void clear() noexcept {
        state_ = State::Stopped;
        loop_frames_ = 0U;
        write_frame_ = 0U;
        playback_position_ = 0.0;
        overdub_gain_ = 1.0F;
        undo_gain_ = 1.0F;
        undo_available_ = false;
        redo_available_ = false;
        one_shot_ = false;
        clear_filter_state();
    }

    void set_reverse(bool enabled) noexcept { reverse_ = enabled; }
    void set_half_speed(bool enabled) noexcept { half_speed_ = enabled; }

    void set_playback_level(float level) noexcept {
        playback_level_ = clamp_finite(level, kPlaybackLevelMin,
                                       kPlaybackLevelMax, 1.0F);
    }

    void set_overdub_decay_db(float decay_db) noexcept {
        overdub_decay_db_ = clamp_finite(decay_db, kOverdubDecayMinDb,
                                         kOverdubDecayMaxDb, 0.0F);
        overdub_decay_gain_ = std::pow(10.0F, overdub_decay_db_ / 20.0F);
    }

    void set_low_cut_hz(float cutoff_hz) noexcept {
        low_cut_hz_ = clamp_finite(cutoff_hz, kCutoffMinHz,
                                   maximum_cutoff_hz(), 0.0F);
        update_filter_coefficients();
        clear_filter_state();
    }

    void set_high_cut_hz(float cutoff_hz) noexcept {
        high_cut_hz_ = clamp_finite(cutoff_hz, kCutoffMinHz,
                                    maximum_cutoff_hz(), 0.0F);
        update_filter_coefficients();
        clear_filter_state();
    }

    void process(std::span<float> samples,
                 std::uint16_t channels) noexcept override {
        if (channels != channels_ || channels == 0U ||
            (loop_frames_ == 0U && state_ != State::Recording)) {
            return;
        }

        const std::size_t frames = samples.size() / channels_;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const std::size_t input_offset = frame * channels_;
            if (state_ == State::Recording) {
                capture_frame(samples.data() + input_offset);
                continue;
            }
            if (state_ != State::Playing && state_ != State::Overdubbing) {
                continue;
            }

            const auto [first_frame, fraction] = read_position();
            const std::size_t next_frame = (first_frame + 1U) % loop_frames_;
            const std::size_t first_offset = first_frame * channels_;
            const std::size_t next_offset = next_frame * channels_;
            for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
                const float dry_input = samples[input_offset + channel];
                const float first = loop_[first_offset + channel];
                const float next = loop_[next_offset + channel];
                float playback = first +
                                 (next - first) * static_cast<float>(fraction);
                playback = filter_sample(playback, channel);
                const float wet_output = playback * playback_level_ * overdub_gain_;
                samples[input_offset + channel] = dry_input + wet_output;
                if (state_ == State::Overdubbing) {
                    loop_[first_offset + channel] += dry_input;
                }
            }
            advance_playback();
        }
    }

private:
    static std::uint16_t validate_channels(std::uint16_t channels) {
        if (channels != 1U && channels != 2U) {
            throw std::invalid_argument("DeluxeLooper supports mono or stereo");
        }
        return channels;
    }

    static float validate_sample_rate(float sample_rate) {
        if (!std::isfinite(sample_rate) || sample_rate <= 0.0F) {
            throw std::invalid_argument("DeluxeLooper sample rate must be positive");
        }
        return sample_rate;
    }

    static float clamp_finite(float value, float low, float high,
                              float fallback) noexcept {
        if (!std::isfinite(value)) {
            return fallback;
        }
        return std::clamp(value, low, high);
    }

    void capture_frame(const float* input) noexcept {
        if (write_frame_ >= capacity_frames_) {
            finish_recording();
            state_ = loop_frames_ != 0U ? State::Playing : State::Stopped;
            if (loop_frames_ != 0U) {
                position_at_start();
            }
            return;
        }
        const std::size_t offset = write_frame_ * channels_;
        for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
            loop_[offset + channel] = input[channel];
        }
        ++write_frame_;
        loop_frames_ = write_frame_;
        if (write_frame_ == capacity_frames_) {
            state_ = State::Playing;
            position_at_start();
        }
    }

    void finish_recording() noexcept {
        if (state_ != State::Recording) {
            return;
        }
        loop_frames_ = write_frame_;
        state_ = loop_frames_ == 0U ? State::Stopped : State::Playing;
        position_at_start();
        overdub_gain_ = 1.0F;
        clear_filter_state();
    }

    void position_at_start() noexcept {
        playback_position_ = reverse_ && loop_frames_ > 0U
                                 ? static_cast<double>(loop_frames_ - 1U)
                                 : 0.0;
    }

    [[nodiscard]] std::pair<std::size_t, double> read_position() const noexcept {
        double position = playback_position_;
        if (position < 0.0) {
            position += static_cast<double>(loop_frames_);
        }
        if (position >= static_cast<double>(loop_frames_)) {
            position = std::fmod(position, static_cast<double>(loop_frames_));
        }
        const auto first = static_cast<std::size_t>(position);
        return {first, position - static_cast<double>(first)};
    }

    void advance_playback() noexcept {
        const double step = half_speed_ ? 0.5 : 1.0;
        const double length = static_cast<double>(loop_frames_);
        playback_position_ += reverse_ ? -step : step;
        bool wrapped = false;
        if (reverse_ && playback_position_ < 0.0) {
            playback_position_ += length;
            wrapped = true;
        } else if (!reverse_ && playback_position_ >= length) {
            playback_position_ -= length;
            wrapped = true;
        }
        if (wrapped && state_ == State::Overdubbing) {
            overdub_gain_ *= overdub_decay_gain_;
        }
        if (wrapped && one_shot_) {
            state_ = State::Stopped;
            one_shot_ = false;
            playback_position_ = 0.0;
        }
    }

    void update_filter_coefficients() noexcept {
        constexpr float two_pi = 6.2831853071795864769F;
        if (low_cut_hz_ <= 0.0F) {
            high_pass_alpha_ = 1.0F;
        } else {
            const float rc = 1.0F / (two_pi * low_cut_hz_);
            const float dt = 1.0F / sample_rate_;
            high_pass_alpha_ = rc / (rc + dt);
        }
        if (high_cut_hz_ <= 0.0F) {
            low_pass_alpha_ = 1.0F;
        } else {
            low_pass_alpha_ = 1.0F -
                              std::exp(-two_pi * high_cut_hz_ / sample_rate_);
        }
    }

    float filter_sample(float input, std::uint16_t channel) noexcept {
        float value = input;
        if (low_cut_hz_ > 0.0F) {
            const float high_pass = high_pass_alpha_ *
                                    (high_pass_output_[channel] + value -
                                     high_pass_input_[channel]);
            high_pass_input_[channel] = value;
            high_pass_output_[channel] = high_pass;
            value = high_pass;
        }
        if (high_cut_hz_ > 0.0F) {
            low_pass_state_[channel] += low_pass_alpha_ *
                                        (value - low_pass_state_[channel]);
            value = low_pass_state_[channel];
        }
        return value;
    }

    void clear_filter_state() noexcept {
        high_pass_input_[0] = high_pass_input_[1] = 0.0F;
        high_pass_output_[0] = high_pass_output_[1] = 0.0F;
        low_pass_state_[0] = low_pass_state_[1] = 0.0F;
    }

    std::uint16_t channels_;
    float sample_rate_;
    std::size_t capacity_frames_;
    std::size_t capacity_samples_;
    std::unique_ptr<float[]> loop_;
    std::unique_ptr<float[]> undo_buffer_;
    State state_{State::Stopped};
    std::size_t loop_frames_{0U};
    std::size_t write_frame_{0U};
    double playback_position_{0.0};
    bool reverse_{false};
    bool half_speed_{false};
    bool one_shot_{false};
    bool undo_available_{false};
    bool redo_available_{false};
    float playback_level_{1.0F};
    float overdub_decay_db_{0.0F};
    float overdub_decay_gain_{1.0F};
    float overdub_gain_{1.0F};
    float undo_gain_{1.0F};
    float low_cut_hz_{0.0F};
    float high_cut_hz_{0.0F};
    float high_pass_alpha_{1.0F};
    float low_pass_alpha_{1.0F};
    float high_pass_input_[2]{0.0F, 0.0F};
    float high_pass_output_[2]{0.0F, 0.0F};
    float low_pass_state_[2]{0.0F, 0.0F};
};

} // namespace tabletfx
