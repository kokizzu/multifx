#include "tabletfx/tuner.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kDefaultReferenceHz = 440.0F;
constexpr float kMuteRampMs = 10.0F;
constexpr float kMinimumRms = 0.0003F;
constexpr float kMinimumConfidence = 0.60F;
constexpr float kDifferenceThreshold = 0.40F;
constexpr float kMinimumFrequencyHz = 50.0F;
constexpr float kMinimumAnalysisFrequencyHz = 45.0F;
constexpr float kMaximumFrequencyHz = 1500.0F;

} // namespace

static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "Tuner telemetry needs lock-free 32-bit atomics");

ChromaticTuner::ChromaticTuner(float sample_rate)
    : sample_rate_(sample_rate), analysis_rate_(0.0F), decimation_factor_(1) {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        throw std::invalid_argument("Tuner sample rate must be in [8000, 384000] Hz");
    }
    const float factor = std::max(1.0F, std::round(sample_rate / 12000.0F));
    decimation_factor_ = static_cast<std::uint32_t>(factor);
    analysis_rate_ = sample_rate_ / static_cast<float>(decimation_factor_);
    output_ramp_frames_ = static_cast<std::size_t>(
        std::llround(static_cast<double>(sample_rate_) * kMuteRampMs / 1000.0));
    reference_bits_.store(float_bits(kDefaultReferenceHz), std::memory_order_relaxed);
}

float ChromaticTuner::reference_a4_hz() const noexcept {
    return bits_float(reference_bits_.load(std::memory_order_relaxed));
}

float ChromaticTuner::analysis_window_ms() const noexcept {
    return static_cast<float>(kWindowSize) * 1000.0F / analysis_rate_;
}

void ChromaticTuner::set_reference_a4_hz(float frequency_hz) noexcept {
    if (!std::isfinite(frequency_hz)) {
        return;
    }
    reference_bits_.store(float_bits(std::clamp(frequency_hz, 400.0F, 480.0F)),
                          std::memory_order_relaxed);
}

void ChromaticTuner::set_muted_output(bool muted) noexcept {
    mute_requested_.store(muted, std::memory_order_relaxed);
}

bool ChromaticTuner::muted_output() const noexcept {
    return mute_requested_.load(std::memory_order_relaxed);
}

TunerReading ChromaticTuner::reading() const noexcept {
    TunerReading result;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        const std::uint32_t before = reading_version_.load(std::memory_order_acquire);
        if ((before & 1U) != 0U) {
            continue;
        }
        result.valid = reading_valid_.load(std::memory_order_relaxed);
        result.frequency_hz = bits_float(reading_frequency_bits_.load(std::memory_order_relaxed));
        result.cents = bits_float(reading_cents_bits_.load(std::memory_order_relaxed));
        result.confidence = bits_float(reading_confidence_bits_.load(std::memory_order_relaxed));
        result.midi_note = static_cast<std::int16_t>(
            reading_midi_note_.load(std::memory_order_relaxed));
        result.generation = reading_generation_.load(std::memory_order_relaxed);
        const std::uint32_t after = reading_version_.load(std::memory_order_acquire);
        if (before == after) {
            return result;
        }
    }
    return {};
}

void ChromaticTuner::process(std::span<float> interleaved,
                             std::uint16_t channels) noexcept {
    analyze(interleaved, channels);
    apply_output_mute(interleaved, channels);
}

void ChromaticTuner::analyze(std::span<const float> interleaved,
                             std::uint16_t channels) noexcept {
    if (channels == 0 || channels > 2) {
        return;
    }
    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        float mono = interleaved[offset];
        if (channels == 2) {
            mono = 0.5F * (mono + interleaved[offset + 1]);
        }
        if (!std::isfinite(mono)) {
            mono = 0.0F;
        }
        decimation_sum_ += mono;
        ++decimation_phase_;
        if (decimation_phase_ >= decimation_factor_) {
            accept_downsampled(decimation_sum_ / static_cast<float>(decimation_phase_));
            decimation_phase_ = 0;
            decimation_sum_ = 0.0F;
        }
    }
    advance_analysis();
}

void ChromaticTuner::apply_output_mute(std::span<float> interleaved,
                                       std::uint16_t channels) noexcept {
    if (channels == 0 || channels > 2) {
        return;
    }
    const bool requested_mute = mute_requested_.load(std::memory_order_relaxed);
    if (requested_mute != muted_mode_) {
        muted_mode_ = requested_mute;
        output_gain_target_ = muted_mode_ ? 0.0F : 1.0F;
        const std::size_t ramp_frames = output_ramp_frames_;
        output_gain_step_ = ramp_frames == 0
                                ? 0.0F
                                : (output_gain_target_ - output_gain_) /
                                      static_cast<float>(ramp_frames);
        output_ramp_remaining_ = ramp_frames;
        if (ramp_frames == 0) {
            output_gain_ = output_gain_target_;
        }
    }
    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (output_ramp_remaining_ > 0) {
            output_gain_ += output_gain_step_;
            --output_ramp_remaining_;
            if (output_ramp_remaining_ == 0) {
                output_gain_ = output_gain_target_;
                output_gain_step_ = 0.0F;
            }
        }
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            interleaved[offset + channel] *= output_gain_;
        }
    }
}

void ChromaticTuner::reset() noexcept {
    ring_.fill(0.0F);
    analysis_window_.fill(0.0F);
    difference_.fill(0.0F);
    ring_write_ = 0;
    ring_filled_ = 0;
    samples_since_snapshot_ = 0;
    analysis_tau_ = 0;
    analysis_sample_ = 0;
    minimum_lag_ = 0;
    maximum_lag_ = 0;
    partial_difference_ = 0.0F;
    window_rms_ = 0.0F;
    decimation_phase_ = 0;
    decimation_sum_ = 0.0F;
    analysis_active_ = false;
    output_gain_ = mute_requested_.load(std::memory_order_relaxed) ? 0.0F : 1.0F;
    output_gain_target_ = output_gain_;
    output_gain_step_ = 0.0F;
    output_ramp_remaining_ = 0;
    muted_mode_ = mute_requested_.load(std::memory_order_relaxed);
    publish_reading({});
}

void ChromaticTuner::accept_downsampled(float sample) noexcept {
    ring_[ring_write_] = sample;
    ring_write_ = (ring_write_ + 1U) % kWindowSize;
    ring_filled_ = std::min(ring_filled_ + 1U, kWindowSize);
    ++samples_since_snapshot_;
    if (!analysis_active_ && ring_filled_ == kWindowSize &&
        samples_since_snapshot_ >= kWindowSize) {
        begin_analysis();
    }
}

void ChromaticTuner::begin_analysis() noexcept {
    double square_sum = 0.0;
    std::size_t read = ring_write_;
    for (std::size_t index = 0; index < kWindowSize; ++index) {
        const float sample = ring_[read];
        analysis_window_[index] = sample;
        square_sum += static_cast<double>(sample) * static_cast<double>(sample);
        read = (read + 1U) % kWindowSize;
    }
    window_rms_ = static_cast<float>(std::sqrt(square_sum / static_cast<double>(kWindowSize)));

    const float highest_frequency = std::min(kMaximumFrequencyHz, analysis_rate_ * 0.45F);
    minimum_lag_ = static_cast<std::size_t>(std::max(
        2.0F, std::floor(analysis_rate_ / highest_frequency) - 1.0F));
    maximum_lag_ = static_cast<std::size_t>(std::min(
        static_cast<float>(kMaximumLag),
        std::ceil(analysis_rate_ / kMinimumAnalysisFrequencyHz)));
    maximum_lag_ = std::min(maximum_lag_, kWindowSize - 2U);
    difference_.fill(0.0F);
    analysis_tau_ = minimum_lag_;
    analysis_sample_ = 0;
    partial_difference_ = 0.0F;
    samples_since_snapshot_ = 0;
    analysis_active_ = true;
}

void ChromaticTuner::advance_analysis() noexcept {
    if (!analysis_active_) {
        return;
    }
    std::size_t budget = kWorkPairsPerBlock;
    while (budget > 0 && analysis_tau_ <= maximum_lag_) {
        const std::size_t pair_count = kWindowSize - analysis_tau_;
        const std::size_t remaining = pair_count - analysis_sample_;
        const std::size_t count = std::min(budget, remaining);
        const std::size_t tau = analysis_tau_;
        for (std::size_t index = 0; index < count; ++index) {
            const float delta = analysis_window_[analysis_sample_ + index] -
                                analysis_window_[analysis_sample_ + index + tau];
            partial_difference_ += delta * delta;
        }
        analysis_sample_ += count;
        budget -= count;
        if (analysis_sample_ == pair_count) {
            difference_[tau] = partial_difference_;
            ++analysis_tau_;
            analysis_sample_ = 0;
            partial_difference_ = 0.0F;
        }
    }
    if (analysis_tau_ > maximum_lag_) {
        finish_analysis();
    }
}

void ChromaticTuner::finish_analysis() noexcept {
    analysis_active_ = false;
    TunerReading result;
    result.generation = reading_generation_.load(std::memory_order_relaxed) + 1U;
    if (window_rms_ < kMinimumRms || maximum_lag_ <= minimum_lag_) {
        publish_reading(result);
        return;
    }

    double cumulative = 0.0;
    float best_score = 1.0F;
    std::size_t best_lag = 0;
    std::size_t selected_lag = 0;
    float previous_score = 1.0F;
    for (std::size_t lag = minimum_lag_; lag <= maximum_lag_; ++lag) {
        cumulative += difference_[lag];
        const float score = cumulative <= 1.0e-20
                                ? 1.0F
                                : static_cast<float>(
                                      static_cast<double>(difference_[lag]) *
                                      static_cast<double>(lag - minimum_lag_ + 1U) /
                                      cumulative);
        if (score < best_score) {
            best_score = score;
            best_lag = lag;
        }
        if (score < kDifferenceThreshold && score <= previous_score) {
            selected_lag = lag;
            while (selected_lag < maximum_lag_ &&
                   difference_[selected_lag + 1U] < difference_[selected_lag]) {
                ++selected_lag;
            }
            break;
        }
        previous_score = score;
    }
    if (selected_lag == 0) {
        selected_lag = best_lag;
        best_score = best_lag == 0 ? 1.0F : best_score;
    } else {
        cumulative = 0.0;
        for (std::size_t lag = minimum_lag_; lag <= selected_lag; ++lag) {
            cumulative += difference_[lag];
        }
        best_score = cumulative <= 1.0e-20
                         ? 1.0F
                         : static_cast<float>(
                               static_cast<double>(difference_[selected_lag]) *
                               static_cast<double>(selected_lag - minimum_lag_ + 1U) /
                               cumulative);
    }

    if (selected_lag > minimum_lag_ && selected_lag < maximum_lag_) {
        const float left = difference_[selected_lag - 1U];
        const float center = difference_[selected_lag];
        const float right = difference_[selected_lag + 1U];
        const float denominator = left - 2.0F * center + right;
        if (std::abs(denominator) > 1.0e-20F) {
            const float adjustment = std::clamp(0.5F * (left - right) / denominator,
                                                -0.5F, 0.5F);
            const float refined_lag = static_cast<float>(selected_lag) + adjustment;
            const float frequency = analysis_rate_ / refined_lag;
            const float confidence = std::clamp(1.0F - best_score, 0.0F, 1.0F);
            if (frequency >= kMinimumFrequencyHz &&
                frequency <= kMaximumFrequencyHz &&
                confidence >= kMinimumConfidence) {
                const float reference = reference_a4_hz();
                const float note_float = 69.0F +
                    12.0F * std::log2(frequency / reference);
                const long note = std::lround(note_float);
                const float nearest = reference * std::exp2(
                    static_cast<float>(note - 69L) / 12.0F);
                result.valid = true;
                result.frequency_hz = frequency;
                result.midi_note = static_cast<std::int16_t>(note);
                result.cents = 1200.0F * std::log2(frequency / nearest);
                result.confidence = confidence;
            }
        }
    }
    publish_reading(result);
}

void ChromaticTuner::publish_reading(TunerReading value) noexcept {
    reading_version_.fetch_add(1U, std::memory_order_relaxed);
    reading_generation_.store(value.generation, std::memory_order_relaxed);
    reading_frequency_bits_.store(float_bits(value.frequency_hz), std::memory_order_relaxed);
    reading_cents_bits_.store(float_bits(value.cents), std::memory_order_relaxed);
    reading_confidence_bits_.store(float_bits(value.confidence), std::memory_order_relaxed);
    reading_midi_note_.store(value.midi_note, std::memory_order_relaxed);
    reading_valid_.store(value.valid, std::memory_order_relaxed);
    reading_version_.fetch_add(1U, std::memory_order_release);
}

std::uint32_t ChromaticTuner::float_bits(float value) noexcept {
    return std::bit_cast<std::uint32_t>(value);
}

float ChromaticTuner::bits_float(std::uint32_t value) noexcept {
    return std::bit_cast<float>(value);
}

} // namespace tabletfx
