#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>

namespace tabletfx::effects {

enum class ShufflingLooperState : std::uint8_t {
    Stopped,
    Recording,
    Playing,
    WaitingToRecord,
    WaitingToStop,
};

enum class ShufflingLooperCapacityStatus : std::uint8_t {
    Applied,
    ClampedByModelLimit,
    ClampedByMemoryLimit,
    ClampedByModelAndMemoryLimit,
    RejectedInvalidRequest,
    RejectedLoopWouldBeTruncated,
    AllocationFailed,
    RejectedTransitionMemoryLimit,
};

struct ShufflingLooperParameters {
    // These are project-authored bounds. The Helix Stadium live manual does
    // not publish numeric bounds/defaults for most Shuffling Looper controls.
    std::uint8_t slices = 8U;                 // [2, 16]
    std::uint8_t sequence_length = 8U;        // [1, 32]
    float shuffle = 0.5F;                     // [0, 1]
    float pitch_probability = 0.0F;           // [0, 1]
    float reverse_probability = 0.0F;         // [0, 1]
    float repeat_probability = 0.0F;          // [0, 1]
    float smoothing = 0.0F;                   // [0, 1]
    float sequence_drift = 0.0F;              // [0, 1]
    float playback = 0.7F;                    // [0, 1]
    float low_cut_hz = 0.0F;                  // 0 disables; otherwise [20, 20000]
    float high_cut_hz = 0.0F;                 // 0 disables; otherwise [20, 20000]
    std::int8_t interval1_semitones = -12;    // [-12, 12]
    std::int8_t interval2_semitones = 12;     // [-12, 12]
    // Project-authored beat quantization. XR1 documentation confirms beat
    // alignment but does not publish subdivision choices or defaults.
    bool quantize_recording = false;
    float tempo_bpm = 120.0F;                 // [40, 300], project tempo input
};

/*
 * A small, project-authored sliced-loop instrument. This is not a Line 6
 * implementation or a claim of sonic/control parity. The official Helix
 * Stadium manual selector identifies firmware v1.4.1 / Rev. E; its live
 * Looper Block subpage still displays a Rev D header, checked 2026-11-22. The
 * current v1.4 manual content documents the Shuffling Looper: it records,
 * starts sliced playback on the next switch
 * press, randomizes its sequence on another press, stops on a quick double
 * press, and clears on a press-and-hold. Its documented controls are Slices,
 * SeqLength, Shuffle, Pitch, Reverse, Repeat, Smoothing, Seq Drift, Playback,
 * Low Cut, High Cut, and Interval 1/2. Shuffle, Pitch, Reverse, Repeat, and
 * Seq Drift are likelihood controls; the manual gives 0–100% for Shuffle,
 * Reverse, Repeat, and Seq Drift and describes intervals from one octave down
 * to one octave up. It does not publish exact numeric bounds/defaults for most
 * controls. This class uses the explicit project ranges above.
 * Sources checked 2026-11-22:
 * https://manuals.line6.com/en/helix-stadium/live/looper-block
 * https://kb.line6.com/helix-stadium-1-4-release-notes
 * The current v1.4 release notes list no Shuffling Looper behavior/control
 * changes; the live manual remains the cited control reference.
 *
 * Record replaces the previous loop (destructive). Playback adds the stored
 * loop to the live input and never changes the recorded samples. Storage is
 * four seconds at 48 kHz by default. configure_capacity_seconds() can request
 * larger buffers up to 60 seconds mono or 30 seconds stereo, subject to the
 * per-instance memory limit. Loop sample storage, including old and replacement
 * buffers during a capacity change, is bounded to 16 MiB. Storage is allocated
 * during construction or capacity configuration; process() never allocates.
 * Loops live only in this object and are not persisted. Calls that change
 * transport, parameters, or capacity must be serialized with process() by the
 * host.
 *
 * The implementation uses a simple resampling pitch change per slice. It
 * does not preserve slice duration like a time-stretching pitch shifter.
 * Shuffle may replace the upcoming slice at each sequence transition; drift
 * may regenerate the whole sequence at its boundary. These are project
 * interpretations of the manual's likelihood descriptions.
 */
class ShufflingLooper final : public tabletfx::Effect {
public:
    static constexpr std::size_t kDefaultCapacityFramesAt48k = 4U * 48000U;
    static constexpr std::size_t kMaximumSlices = 16U;
    static constexpr std::size_t kMaximumSequenceSteps = 32U;
    static constexpr std::uint16_t kMinimumChannels = 1U;
    static constexpr std::uint16_t kMaximumChannels = 2U;
    static constexpr float kDefaultSampleRate = 48000.0F;
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 96000.0F;
    static constexpr float kMinimumTempoBpm = 40.0F;
    static constexpr float kMaximumTempoBpm = 300.0F;
    static constexpr float kDefaultTempoBpm = 120.0F;
    static constexpr float kDefaultCapacitySeconds = 4.0F;
    static constexpr float kMaximumMonoCapacitySeconds = 60.0F;
    static constexpr float kMaximumStereoCapacitySeconds = 30.0F;
    static constexpr std::size_t kMaximumTransitionSampleStorageBytes =
        16U * 1024U * 1024U;
    static constexpr std::size_t kMaximumInstanceMemoryBytes = 24U * 1024U * 1024U;
    static constexpr std::size_t kDefaultInstanceMemoryBytes =
        kMaximumInstanceMemoryBytes;
    static constexpr float kMinimumCutHz = 20.0F;
    static constexpr float kMaximumCutHz = 20000.0F;
    static constexpr float kSampleCeiling = 16.0F;

    explicit ShufflingLooper(
        float sample_rate = kDefaultSampleRate,
        std::uint16_t channels = 2U,
        std::uint32_t seed = 0x6D2B79F5U,
        std::size_t instance_memory_limit_bytes =
            kDefaultInstanceMemoryBytes);

    ShufflingLooper(const ShufflingLooper&) = delete;
    ShufflingLooper& operator=(const ShufflingLooper&) = delete;
    ShufflingLooper(ShufflingLooper&&) = delete;
    ShufflingLooper& operator=(ShufflingLooper&&) = delete;

    [[nodiscard]] std::string_view id() const noexcept override;
    [[nodiscard]] std::size_t memory_bytes() const noexcept override;

    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] ShufflingLooperState state() const noexcept { return state_; }
    [[nodiscard]] std::size_t loop_frames() const noexcept { return loop_frames_; }
    [[nodiscard]] std::size_t capacity_frames() const noexcept {
        return capacity_frames_;
    }
    [[nodiscard]] float capacity_seconds() const noexcept {
        return static_cast<float>(capacity_frames_) / sample_rate_;
    }
    [[nodiscard]] float maximum_capacity_seconds() const noexcept {
        return channels_ == 1U ? kMaximumMonoCapacitySeconds
                               : kMaximumStereoCapacitySeconds;
    }
    [[nodiscard]] std::size_t capacity_storage_bytes() const noexcept {
        return capacity_frames_ * static_cast<std::size_t>(channels_) *
               sizeof(float);
    }
    [[nodiscard]] std::size_t instance_memory_limit_bytes() const noexcept {
        return instance_memory_limit_bytes_;
    }
    [[nodiscard]] ShufflingLooperCapacityStatus capacity_status() const noexcept {
        return capacity_status_;
    }
    [[nodiscard]] std::size_t sequence_steps() const noexcept {
        return sequence_step_count_;
    }
    [[nodiscard]] std::uint8_t sequence_slice(std::size_t step) const noexcept;
    [[nodiscard]] const ShufflingLooperParameters& parameters() const noexcept {
        return parameters_;
    }

    // Starts destructive replacement recording. Audio passed to process()
    // while recording is stored and also passed through to the output.
    [[nodiscard]] bool record() noexcept;

    // Finalizes a recording and immediately starts sequence playback.
    [[nodiscard]] bool stop_recording() noexcept;

    // Starts playback from sequence step zero, retaining recorded audio.
    [[nodiscard]] bool play() noexcept;

    // Stops playback but retains the loop and generated sequence.
    void stop() noexcept;

    // Regenerates the sequence using the configured deterministic RNG seed.
    [[nodiscard]] bool randomize_sequence() noexcept;

    // Clears the loop. Returns false if there is no loop to clear.
    [[nodiscard]] bool clear() noexcept;

    // Parameter edits are bounded to project-authored ranges. If a loop is
    // present, its sequence is rebuilt and playback restarts at step zero.
    void configure(const ShufflingLooperParameters& parameters) noexcept;
    // Changes the preallocated capture capacity outside process(). Requests
    // above the channel-specific maximum or memory budget are clamped and
    // reported. Invalid requests, allocations that fail, and requests that
    // would truncate an existing loop are rejected without changing it.
    [[nodiscard]] ShufflingLooperCapacityStatus configure_capacity_seconds(
        float requested_seconds) noexcept;
    void set_seed(std::uint32_t seed) noexcept;

    // Host reset clears transient loop, sequence, transport, and filter state;
    // current parameters and seed are retained.
    void reset() noexcept override;

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;

private:
    struct SequenceStep {
        std::uint8_t slice = 0U;
        std::int8_t semitones = 0;
        bool reverse = false;
        std::size_t source_start = 0U;
        std::size_t source_frames = 0U;
        std::size_t output_frames = 0U;
        double source_step = 1.0;
    };

    [[nodiscard]] static std::uint16_t validate_channels(std::uint16_t channels);
    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    [[nodiscard]] float random_unit() noexcept;
    [[nodiscard]] std::uint32_t random_bounded(std::uint32_t limit) noexcept;
    [[nodiscard]] bool chance(float probability) noexcept;
    void rebuild_sequence(bool force_shuffle) noexcept;
    void set_sequence_step(std::size_t index, std::uint8_t slice) noexcept;
    void finish_recording() noexcept;
    [[nodiscard]] std::size_t quantized_capacity_frames(
        std::uint32_t tempo_milli_bpm) const noexcept;
    void advance_beat_clock() noexcept;
    void advance_step() noexcept;
    [[nodiscard]] float read_slice_sample(const SequenceStep& step,
                                          std::size_t output_frame,
                                          std::size_t channel) const noexcept;
    [[nodiscard]] float filter_playback(float sample,
                                        std::size_t channel) noexcept;
    [[nodiscard]] float smoothing_gain(const SequenceStep& step,
                                       std::size_t output_frame) const noexcept;
    [[nodiscard]] std::size_t effective_slice_count() const noexcept;

    float sample_rate_ = kDefaultSampleRate;
    std::uint64_t beat_phase_limit_units_ = 0U;
    std::uint64_t beat_phase_units_ = 0U;
    bool next_frame_is_beat_ = true;
    bool quantized_capture_ = false;
    std::uint32_t tempo_milli_bpm_ = 120000U;
    std::uint32_t capture_tempo_milli_bpm_ = 120000U;
    std::size_t quantized_capture_limit_frames_ = 0U;
    const std::uint16_t channels_;
    std::unique_ptr<float[]> samples_;
    std::size_t capacity_frames_ = 0U;
    const std::size_t instance_memory_limit_bytes_;
    ShufflingLooperCapacityStatus capacity_status_ =
        ShufflingLooperCapacityStatus::Applied;
    ShufflingLooperParameters parameters_{};
    ShufflingLooperState state_ = ShufflingLooperState::Stopped;
    std::array<SequenceStep, kMaximumSequenceSteps> sequence_{};
    std::size_t loop_frames_ = 0U;
    std::size_t sequence_step_count_ = 0U;
    std::size_t sequence_step_ = 0U;
    std::size_t step_frame_ = 0U;
    std::uint32_t seed_ = 1U;
    std::uint32_t rng_state_ = 1U;
    std::array<float, kMaximumChannels> lowpass_state_{};
    std::array<float, kMaximumChannels> highpass_input_{};
    std::array<float, kMaximumChannels> highpass_output_{};
};

}  // namespace tabletfx::effects
