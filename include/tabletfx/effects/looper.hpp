#pragma once

#include "tabletfx/effect.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace tabletfx {

enum class LooperState : std::uint8_t {
    Stopped,
    Recording,
    Playing,
    Overdubbing,
};

/*
 * A small project-authored mono/stereo phrase looper. At 48 kHz, stereo
 * capacity is four seconds. A second equally sized buffer supports one-level
 * undo/redo; all storage is reserved in the constructor, and process() does
 * not allocate. Undo snapshots are copied when a recording, overdub, or clear
 * is requested, outside the audio callback. The fixed frame cap keeps memory
 * bounded at other sample rates too (so duration is shorter above 48 kHz).
 *
 * This is an intentionally small core, not a recreation of a commercial
 * looper. It has one layer, no quantization, no loop-file or preset saving,
 * no auto-record threshold, and no overdub while reversed or half-speed.
 * Stored and overdubbed samples are sanitized and bounded to +/-16.0 to keep
 * accumulated floating-point audio finite while allowing internal headroom.
 * Reverse and half-speed affect playback only; half-speed uses linear
 * interpolation and lowers pitch by an octave. The owner should serialize
 * control calls with audio processing or issue them at callback boundaries.
 * The browser worklet handles controls between process() calls on the audio
 * thread; snapshot copies may delay a later callback and have not been
 * profiled for glitch-free real-time operation.
 *
 * Manufacturer comparison context checked 2026-11-22 (these limits are not
 * inherited by this class):
 * - MELOFTY Axom Max User Manual V1.7.4 documents a stereo phrase looper up
 *   to 3,600 seconds, plus reverse and half speed:
 *   https://cdn.rockboard.de/nos/data/erp/Artikell_PDF/Manual_PDF/Melofty/EN/MEL_AXOM_MAX_Manual_EN.pdf
 * - Fender's v1.7 update article says the looper was extended to three
 *   minutes, while its product page still advertises 60 seconds stereo.
 *   Firmware notes v1.4.71 list reverse, half-speed, and undo assignments
 *   for the Global Looper. Keep these source/version claims distinct:
 *   https://www.fender.com/articles/parts-and-accessories/tone-master-pros-latest-firmware-update-is-one
 *   https://support.fender.com/hc/en-us/articles/46768129660315-Fender-Tone-Master-Pro-Firmware-and-Pro-Control-App-Updates
 * - Helix Stadium Models manual rev-d-v1.3 documents multiple looper block
 *   types and different mono/stereo and half-speed limits: Deluxe/1 Switch
 *   up to 120 s mono or 60 s stereo (double at half speed); Shuffling up to
 *   60 s mono or 30 s stereo. One looper block is allowed per preset:
 *   https://manuals.line6.com/en/helix-stadium/live/looper-block
 * - Fractal's FM9 Blocks Guide documents a stereo looper up to 120 seconds
 *   with undo. The guide's exact applicability to FM9 Mk II Turbo/current
 *   firmware has not been established:
 *   https://www.fractalaudio.com/downloads/manuals/fas-guides/Fractal-Audio-Blocks-Guide.pdf
 * - HeadRush Prime's official FAQ documents up to 50 layers, five minutes
 *   per layer, and 20 minutes total; the FAQ's exact firmware applicability
 *   was not stated:
 *   https://support.headrushfx.com/en/support/solutions/articles/69000834532-headrush-prime-frequently-asked-questions
 * - XRecer's official XR1 FAQ documents up to four minutes per loop and
 *   eight saved Loop Files; its effect is a one-block-per-preset Looper or
 *   Simple Looper. Current firmware applicability is not stated:
 *   https://www.xrecer.com/pages/faq
 */
class Looper final : public Effect {
public:
    static constexpr std::size_t kMaximumFrames = 4U * 48000U;
    static constexpr std::uint16_t kMinimumChannels = 1U;
    static constexpr std::uint16_t kMaximumChannels = 2U;
    static constexpr float kSafetyCeiling = 16.0F;

    explicit Looper(std::uint16_t channels = 2U)
        : channels_(validate_channels(channels)),
          first_buffer_(std::make_unique<float[]>(kMaximumFrames * channels_)),
          second_buffer_(std::make_unique<float[]>(kMaximumFrames * channels_)),
          active_buffer_(first_buffer_.get()),
          history_buffer_(second_buffer_.get()) {}

    Looper(const Looper&) = delete;
    Looper& operator=(const Looper&) = delete;
    Looper(Looper&&) = delete;
    Looper& operator=(Looper&&) = delete;

    [[nodiscard]] std::string_view id() const noexcept override {
        return "phrase-looper";
    }

    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this) +
               2U * kMaximumFrames * static_cast<std::size_t>(channels_) *
                   sizeof(float);
    }

    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] LooperState state() const noexcept { return state_; }
    [[nodiscard]] std::size_t loop_frames() const noexcept { return loop_frames_; }
    [[nodiscard]] static constexpr std::size_t maximum_frames() noexcept {
        return kMaximumFrames;
    }
    [[nodiscard]] bool can_undo() const noexcept { return undo_available_; }
    [[nodiscard]] bool can_redo() const noexcept { return redo_available_; }
    [[nodiscard]] bool reverse() const noexcept { return reverse_; }
    [[nodiscard]] bool half_speed() const noexcept { return half_speed_; }
    [[nodiscard]] float overdub_level() const noexcept { return overdub_level_; }

    // Starts a replacement recording. The prior loop remains undoable.
    [[nodiscard]] bool record() noexcept {
        if (state_ == LooperState::Recording) return false;
        snapshot_for_edit();
        loop_frames_ = 0U;
        record_position_ = 0U;
        playback_position_ = 0.0;
        reverse_ = false;
        half_speed_ = false;
        state_ = LooperState::Recording;
        return true;
    }

    // Starts playback at the beginning (or end when reverse is selected).
    [[nodiscard]] bool play() noexcept {
        if (loop_frames_ == 0U || state_ == LooperState::Recording) return false;
        playback_position_ = reverse_
                                 ? static_cast<double>(loop_frames_ - 1U)
                                 : 0.0;
        state_ = LooperState::Playing;
        return true;
    }

    // Starts a forward, full-speed overdub of the existing loop.
    [[nodiscard]] bool overdub() noexcept {
        if (loop_frames_ == 0U || state_ == LooperState::Recording ||
            state_ == LooperState::Overdubbing) {
            return false;
        }
        snapshot_for_edit();
        record_position_ = 0U;
        playback_position_ = 0.0;
        state_ = LooperState::Overdubbing;
        return true;
    }

    void stop() noexcept {
        state_ = LooperState::Stopped;
        record_position_ = 0U;
    }

    [[nodiscard]] bool undo() noexcept {
        if (!undo_available_) return false;
        swap_current_and_history();
        undo_available_ = false;
        redo_available_ = true;
        stop();
        return true;
    }

    [[nodiscard]] bool redo() noexcept {
        if (!redo_available_) return false;
        swap_current_and_history();
        undo_available_ = true;
        redo_available_ = false;
        stop();
        return true;
    }

    // Clearing is an undoable edit. Clearing an already empty loop is a no-op.
    [[nodiscard]] bool clear() noexcept {
        stop();
        if (loop_frames_ == 0U) return false;
        snapshot_for_edit();
        loop_frames_ = 0U;
        playback_position_ = 0.0;
        reverse_ = false;
        half_speed_ = false;
        return true;
    }

    // Full host reset: erase loop and history and return to dry pass-through.
    void reset() noexcept override {
        // Lengths gate all reads; stale samples are unreachable until a new
        // recording writes them, so reset is constant-time.
        active_buffer_ = first_buffer_.get();
        history_buffer_ = second_buffer_.get();
        loop_frames_ = 0U;
        history_frames_ = 0U;
        record_position_ = 0U;
        playback_position_ = 0.0;
        state_ = LooperState::Stopped;
        reverse_ = false;
        half_speed_ = false;
        undo_available_ = false;
        redo_available_ = false;
    }

    void set_reverse(bool enabled) noexcept { reverse_ = enabled; }
    void set_half_speed(bool enabled) noexcept { half_speed_ = enabled; }
    void set_overdub_level(float level) noexcept {
        overdub_level_ = std::isfinite(level)
                             ? std::clamp(level, 0.0F, 1.0F)
                             : 1.0F;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override {
        if (channels != channels_ || channels == 0U ||
            interleaved.size() % channels != 0U) {
            return;
        }
        if (state_ == LooperState::Stopped) return;

        const std::size_t frames = interleaved.size() / channels_;
        for (std::size_t frame = 0U; frame < frames; ++frame) {
            const std::size_t sample_offset = frame * channels_;
            if (state_ == LooperState::Recording) {
                if (loop_frames_ >= kMaximumFrames) {
                    state_ = LooperState::Playing;
                    playback_position_ = reverse_
                                             ? static_cast<double>(loop_frames_ - 1U)
                                             : 0.0;
                    render_playback_frame(interleaved, sample_offset);
                    continue;
                }
                const std::size_t destination = loop_frames_ * channels_;
                for (std::size_t channel = 0U; channel < channels_; ++channel) {
                    active_buffer_[destination + channel] = bounded_sample(
                        interleaved[sample_offset + channel]);
                }
                ++loop_frames_;
                if (loop_frames_ == kMaximumFrames) {
                    state_ = LooperState::Playing;
                    playback_position_ = reverse_
                                             ? static_cast<double>(loop_frames_ - 1U)
                                             : 0.0;
                }
                continue;
            }

            if (state_ == LooperState::Overdubbing) {
                const std::size_t destination = record_position_ * channels_;
                for (std::size_t channel = 0U; channel < channels_; ++channel) {
                    const std::size_t index = sample_offset + channel;
                    const float input = bounded_sample(interleaved[index]);
                    const float old_loop = active_buffer_[destination + channel];
                    active_buffer_[destination + channel] = bounded_sample(
                        old_loop + input * overdub_level_);
                    interleaved[index] = input + old_loop;
                }
                ++record_position_;
                if (record_position_ == loop_frames_) record_position_ = 0U;
                continue;
            }

            render_playback_frame(interleaved, sample_offset);
        }
    }

private:
    [[nodiscard]] static std::uint16_t validate_channels(
        std::uint16_t channels) {
        if (channels < kMinimumChannels || channels > kMaximumChannels) {
            throw std::invalid_argument("Looper supports mono or stereo only");
        }
        return channels;
    }

    [[nodiscard]] static float bounded_sample(float sample) noexcept {
        if (!std::isfinite(sample)) return 0.0F;
        return std::clamp(sample, -kSafetyCeiling, kSafetyCeiling);
    }

    void snapshot_for_edit() noexcept {
        const std::size_t samples = loop_frames_ * channels_;
        std::copy_n(active_buffer_, samples, history_buffer_);
        history_frames_ = loop_frames_;
        undo_available_ = true;
        redo_available_ = false;
    }

    void swap_current_and_history() noexcept {
        std::swap(active_buffer_, history_buffer_);
        std::swap(loop_frames_, history_frames_);
    }

    [[nodiscard]] float loop_sample(std::size_t frame,
                                    std::size_t channel) const noexcept {
        const std::size_t next = frame + 1U == loop_frames_ ? 0U : frame + 1U;
        const double fraction = playback_position_ -
                                static_cast<double>(frame);
        const float first = active_buffer_[frame * channels_ + channel];
        const float second = active_buffer_[next * channels_ + channel];
        return static_cast<float>(static_cast<double>(first) * (1.0 - fraction) +
                                  static_cast<double>(second) * fraction);
    }

    void render_playback_frame(std::span<float> interleaved,
                               std::size_t sample_offset) noexcept {
        if (loop_frames_ == 0U) {
            state_ = LooperState::Stopped;
            return;
        }
        const std::size_t frame = static_cast<std::size_t>(playback_position_);
        for (std::size_t channel = 0U; channel < channels_; ++channel) {
            const std::size_t index = sample_offset + channel;
            const float input = std::isfinite(interleaved[index])
                                    ? interleaved[index]
                                    : 0.0F;
            interleaved[index] = input + loop_sample(frame, channel);
        }

        const double step = half_speed_ ? 0.5 : 1.0;
        if (reverse_) {
            playback_position_ -= step;
            if (playback_position_ < 0.0) {
                playback_position_ += static_cast<double>(loop_frames_);
            }
        } else {
            playback_position_ += step;
            if (playback_position_ >= static_cast<double>(loop_frames_)) {
                playback_position_ -= static_cast<double>(loop_frames_);
            }
        }
    }

    const std::uint16_t channels_;
    std::unique_ptr<float[]> first_buffer_;
    std::unique_ptr<float[]> second_buffer_;
    float* active_buffer_;
    float* history_buffer_;
    std::size_t loop_frames_ = 0U;
    std::size_t history_frames_ = 0U;
    std::size_t record_position_ = 0U;
    double playback_position_ = 0.0;
    float overdub_level_ = 1.0F;
    LooperState state_ = LooperState::Stopped;
    bool reverse_ = false;
    bool half_speed_ = false;
    bool undo_available_ = false;
    bool redo_available_ = false;
};

}  // namespace tabletfx
