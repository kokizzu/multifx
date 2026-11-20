#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

struct TunerReading {
    bool valid = false;
    float frequency_hz = 0.0F;
    float cents = 0.0F;
    float confidence = 0.0F;
    std::int16_t midi_note = -1;
    std::uint32_t generation = 0;
};

// Low-cost mono pitch monitor for guitar-range tuning. It analyzes a decimated
// rolling window and spreads the YIN-style difference search across calls, so
// work per audio block is bounded. It passes audio unchanged unless mute mode
// is enabled; mute transitions use a short ramp. Readings can be polled from a
// non-audio thread. Call reset only while processing is stopped.
class ChromaticTuner final {
public:
    explicit ChromaticTuner(float sample_rate = 48000.0F);

    [[nodiscard]] std::size_t memory_bytes() const noexcept { return sizeof(*this); }
    [[nodiscard]] float sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] float reference_a4_hz() const noexcept;
    [[nodiscard]] float analysis_window_ms() const noexcept;

    void set_reference_a4_hz(float frequency_hz) noexcept;
    void set_muted_output(bool muted) noexcept;
    [[nodiscard]] bool muted_output() const noexcept;
    [[nodiscard]] TunerReading reading() const noexcept;

    void analyze(std::span<const float> interleaved,
                 std::uint16_t channels) noexcept;
    void apply_output_mute(std::span<float> interleaved,
                           std::uint16_t channels) noexcept;
    // Analyze the original signal, then pass it through or ramp it to silence.
    // Supports mono/stereo interleaved float buffers and performs no allocation.
    void process(std::span<float> interleaved, std::uint16_t channels) noexcept;
    void reset() noexcept;

private:
    static constexpr std::size_t kWindowSize = 1024;
    static constexpr std::size_t kMaximumLag = 512;
    static constexpr std::size_t kWorkPairsPerBlock = 16384;

    void accept_downsampled(float sample) noexcept;
    void begin_analysis() noexcept;
    void advance_analysis() noexcept;
    void finish_analysis() noexcept;
    void publish_reading(TunerReading value) noexcept;
    [[nodiscard]] static std::uint32_t float_bits(float value) noexcept;
    [[nodiscard]] static float bits_float(std::uint32_t value) noexcept;

    float sample_rate_;
    float analysis_rate_;
    std::uint32_t decimation_factor_;
    std::uint32_t decimation_phase_ = 0;
    float decimation_sum_ = 0.0F;
    std::array<float, kWindowSize> ring_{};
    std::array<float, kWindowSize> analysis_window_{};
    std::array<float, kMaximumLag + 1> difference_{};
    std::size_t ring_write_ = 0;
    std::size_t ring_filled_ = 0;
    std::size_t samples_since_snapshot_ = 0;
    std::size_t analysis_tau_ = 0;
    std::size_t analysis_sample_ = 0;
    std::size_t minimum_lag_ = 0;
    std::size_t maximum_lag_ = 0;
    float partial_difference_ = 0.0F;
    float window_rms_ = 0.0F;
    bool analysis_active_ = false;

    float output_gain_ = 1.0F;
    float output_gain_target_ = 1.0F;
    float output_gain_step_ = 0.0F;
    std::size_t output_ramp_remaining_ = 0;
    std::size_t output_ramp_frames_ = 480;
    bool muted_mode_ = false;

    std::atomic<std::uint32_t> reference_bits_{0};
    std::atomic<bool> mute_requested_{false};
    std::atomic<std::uint32_t> reading_version_{0};
    std::atomic<std::uint32_t> reading_generation_{0};
    std::atomic<std::uint32_t> reading_frequency_bits_{0};
    std::atomic<std::uint32_t> reading_cents_bits_{0};
    std::atomic<std::uint32_t> reading_confidence_bits_{0};
    std::atomic<std::int32_t> reading_midi_note_{-1};
    std::atomic<bool> reading_valid_{false};
};

} // namespace tabletfx
