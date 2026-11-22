#pragma once

#include "tabletfx/effect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tabletfx {

enum class RingModulatorType : std::uint8_t {
    classic,
    ssb_upper,
    ssb_lower,
};

struct RingModulatorParameters {
    // Fractal documents Classic, SSB Upper, and SSB Lower. This project's SSB
    // signal path is a FIR Hilbert shifter, not Fractal DSP.
    RingModulatorType type = RingModulatorType::classic;
    // Project-authored 2–2,000 Hz range. The 440 Hz default is also
    // project-authored; neither is claimed to match an FM9 default or range.
    float frequency_hz = 440.0F;
    // The Fractal Blocks Guide lists a 0.25–4x multiplier; applicability to
    // FM9 Mk II Turbo firmware v12.0 is unverified. This is project-authored.
    float frequency_multiplier = 1.0F;
    // The FM9 guide names High Cut Frequency but gives no range. This project's
    // normalized mix and 20–20,000 Hz cutoff range are project-authored.
    float high_cut_hz = 20000.0F;
    float mix = 1.0F;
};

// Project-authored ring modulation: a free-running sine carrier
// multiplies the input, producing sum/difference sidebands. Mono and stereo
// instances share one carrier phase across channels to preserve stereo image.
// Optional SSB modes use a 63-tap windowed Hilbert FIR and add 31 samples of
// algorithmic latency to both wet and dry paths. The FIR and its response are
// project choices, not a Fractal model implementation.
// Processing uses fixed state, performs no allocation, and makes no per-sample
// trigonometric calls. FM9 pitch tracking and Guitar/Bass tracking modes remain
// unimplemented; this is not a model of Fractal DSP.
class RingModulator final : public Effect {
public:
    explicit RingModulator(float sample_rate = 48000.0F,
                           std::uint16_t channels = 2,
                           RingModulatorParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "ring-mod";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }

    void set_parameters(RingModulatorParameters parameters) noexcept;
    [[nodiscard]] RingModulatorParameters parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] float effective_frequency_hz() const noexcept {
        return effective_frequency_hz_;
    }
    [[nodiscard]] float effective_high_cut_hz() const noexcept {
        return effective_high_cut_hz_;
    }
    [[nodiscard]] std::size_t latency_samples() const noexcept {
        return parameters_.type == RingModulatorType::classic ? 0U
                                                              : kHilbertDelay;
    }

    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    static constexpr float kMinimumSampleRate = 8000.0F;
    static constexpr float kMaximumSampleRate = 384000.0F;
    static constexpr float kMinimumFrequencyHz = 2.0F;
    static constexpr float kMaximumFrequencyHz = 2000.0F;
    static constexpr float kMinimumMultiplier = 0.25F;
    static constexpr float kMaximumMultiplier = 4.0F;
    static constexpr float kMinimumHighCutHz = 20.0F;
    static constexpr float kMaximumHighCutHz = 20000.0F;
    static constexpr std::size_t kHilbertTaps = 63U;
    static constexpr std::size_t kHilbertDelay = (kHilbertTaps - 1U) / 2U;
    static constexpr float kPi = 3.14159265358979323846F;
    static constexpr float kTwoPi = 2.0F * kPi;

    [[nodiscard]] static float bounded(float value, float fallback,
                                       float minimum, float maximum) noexcept;
    void prepare_oscillator() noexcept;
    void prepare_high_cut() noexcept;
    void prepare_hilbert() noexcept;
    void advance_oscillator() noexcept;

    float sample_rate_ = 48000.0F;
    std::uint16_t channels_ = 2;
    RingModulatorParameters parameters_{};
    std::array<float, 2> high_cut_state_{};
    std::array<std::array<float, kHilbertTaps>, 2> input_history_{};
    std::array<float, kHilbertTaps> hilbert_coefficients_{};
    std::size_t history_write_index_ = 0;
    float effective_frequency_hz_ = 440.0F;
    float effective_high_cut_hz_ = 20000.0F;
    float high_cut_alpha_ = 1.0F;
    float phase_sine_ = 0.0F;
    float phase_cosine_ = 1.0F;
    float increment_sine_ = 0.0F;
    float increment_cosine_ = 1.0F;
    std::uint32_t normalization_counter_ = 0;
};

} // namespace tabletfx
