#include "tabletfx/effects/shelf_tilt_eq.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tabletfx {
namespace {

constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kDefaultFrequencyHz = 1000.0F;
constexpr float kDefaultGainDb = 0.0F;
constexpr float kShelfSlope = 1.0F;
constexpr float kDenormalThreshold = 1.0e-20F;
constexpr double kPi = 3.141592653589793238462643383279502884;

float valid_sample_rate(float sample_rate) noexcept {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        return kDefaultSampleRate;
    }
    return sample_rate;
}

std::uint16_t valid_channels(std::uint16_t channels) noexcept {
    return channels == 1 ? 1 : 2;
}

float bounded(float value, float fallback, float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return std::clamp(value, minimum, maximum);
}

} // namespace

ShelfTiltEQ::ShelfTiltEQ(float sample_rate, std::uint16_t channels,
                         ShelfTiltMode mode, float frequency_hz,
                         float gain_db) noexcept
    : sample_rate_(valid_sample_rate(sample_rate)),
      channels_(valid_channels(channels)) {
    configure(mode, frequency_hz, gain_db);
}

std::string_view ShelfTiltEQ::id() const noexcept {
    return "shelf-tilt-eq";
}

std::size_t ShelfTiltEQ::memory_bytes() const noexcept {
    return sizeof(*this);
}

float ShelfTiltEQ::sample_rate() const noexcept {
    return sample_rate_;
}

std::uint16_t ShelfTiltEQ::channels() const noexcept {
    return channels_;
}

ShelfTiltMode ShelfTiltEQ::mode() const noexcept {
    return mode_;
}

float ShelfTiltEQ::frequency_hz() const noexcept {
    return frequency_hz_;
}

float ShelfTiltEQ::gain_db() const noexcept {
    return gain_db_;
}

void ShelfTiltEQ::configure(ShelfTiltMode mode, float frequency_hz,
                            float gain_db) noexcept {
    const ShelfTiltMode new_mode = valid_mode(mode);
    const float max_frequency_hz = std::max(kMinimumFrequencyHz,
                                            sample_rate_ * 0.45F);
    const float new_frequency_hz = bounded(frequency_hz, kDefaultFrequencyHz,
                                           kMinimumFrequencyHz, max_frequency_hz);
    const float new_gain_db = bounded(gain_db, kDefaultGainDb,
                                      kMinimumGainDb, kMaximumGainDb);
    if (new_mode == mode_ && new_frequency_hz == frequency_hz_ &&
        new_gain_db == gain_db_) {
        return;
    }

    mode_ = new_mode;
    frequency_hz_ = new_frequency_hz;
    gain_db_ = new_gain_db;
    rebuild_coefficients();
}

void ShelfTiltEQ::set_mode(ShelfTiltMode mode) noexcept {
    configure(mode, frequency_hz_, gain_db_);
}

void ShelfTiltEQ::set_frequency_hz(float frequency_hz) noexcept {
    configure(mode_, frequency_hz, gain_db_);
}

void ShelfTiltEQ::set_gain_db(float gain_db) noexcept {
    configure(mode_, frequency_hz_, gain_db);
}

void ShelfTiltEQ::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels == 0 || channels != channels_) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = frame * channels_ + channel;
            float& sample = interleaved[index];
            if (!std::isfinite(sample)) {
                sample = 0.0F;
                reset_channel(channel);
                continue;
            }

            ChannelState& state = states_[channel];
            bool valid = true;
            if (mode_ != ShelfTiltMode::HighShelf) {
                valid = process_section(sample, low_coefficients_, state.low);
            }
            if (valid && mode_ != ShelfTiltMode::LowShelf) {
                valid = process_section(sample, high_coefficients_, state.high);
            }
            if (!valid || !std::isfinite(sample)) {
                sample = 0.0F;
                reset_channel(channel);
            }
        }
    }
}

void ShelfTiltEQ::reset() noexcept {
    for (ChannelState& state : states_) {
        state = {};
    }
}

ShelfTiltMode ShelfTiltEQ::valid_mode(ShelfTiltMode mode) noexcept {
    switch (mode) {
    case ShelfTiltMode::LowShelf:
    case ShelfTiltMode::HighShelf:
    case ShelfTiltMode::Tilt:
        return mode;
    }
    return ShelfTiltMode::LowShelf;
}

ShelfTiltEQ::Coefficients ShelfTiltEQ::make_shelf(float frequency_hz,
                                                   float gain_db,
                                                   float sample_rate,
                                                   bool high_shelf) noexcept {
    if (gain_db == 0.0F) {
        return {};
    }

    const double amplitude = std::pow(10.0, static_cast<double>(gain_db) / 40.0);
    const double omega = 2.0 * kPi * static_cast<double>(frequency_hz) /
                         static_cast<double>(sample_rate);
    const double cosine = std::cos(omega);
    const double sine = std::sin(omega);
    const double slope = static_cast<double>(kShelfSlope);
    const double alpha = (sine / 2.0) * std::sqrt(
        (amplitude + 1.0 / amplitude) * (1.0 / slope - 1.0) + 2.0);
    const double beta = 2.0 * std::sqrt(amplitude) * alpha;

    double b0 = 0.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a0 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    if (high_shelf) {
        b0 = amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine + beta);
        b1 = -2.0 * amplitude * ((amplitude - 1.0) + (amplitude + 1.0) * cosine);
        b2 = amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine - beta);
        a0 = (amplitude + 1.0) - (amplitude - 1.0) * cosine + beta;
        a1 = 2.0 * ((amplitude - 1.0) - (amplitude + 1.0) * cosine);
        a2 = (amplitude + 1.0) - (amplitude - 1.0) * cosine - beta;
    } else {
        b0 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine + beta);
        b1 = 2.0 * amplitude * ((amplitude - 1.0) - (amplitude + 1.0) * cosine);
        b2 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine - beta);
        a0 = (amplitude + 1.0) + (amplitude - 1.0) * cosine + beta;
        a1 = -2.0 * ((amplitude - 1.0) + (amplitude + 1.0) * cosine);
        a2 = (amplitude + 1.0) + (amplitude - 1.0) * cosine - beta;
    }

    if (!std::isfinite(a0) || std::abs(a0) < std::numeric_limits<double>::min()) {
        return {};
    }
    const double inverse_a0 = 1.0 / a0;
    const Coefficients result{
        static_cast<float>(b0 * inverse_a0),
        static_cast<float>(b1 * inverse_a0),
        static_cast<float>(b2 * inverse_a0),
        static_cast<float>(a1 * inverse_a0),
        static_cast<float>(a2 * inverse_a0),
    };
    if (!std::isfinite(result.b0) || !std::isfinite(result.b1) ||
        !std::isfinite(result.b2) || !std::isfinite(result.a1) ||
        !std::isfinite(result.a2)) {
        return {};
    }
    return result;
}

bool ShelfTiltEQ::process_section(float& sample,
                                  const Coefficients& coefficients,
                                  SectionState& state) noexcept {
    const float output = coefficients.b0 * sample + state.z1;
    const float next_z1 = coefficients.b1 * sample - coefficients.a1 * output + state.z2;
    const float next_z2 = coefficients.b2 * sample - coefficients.a2 * output;
    if (!std::isfinite(output) || !std::isfinite(next_z1) ||
        !std::isfinite(next_z2)) {
        state = {};
        sample = 0.0F;
        return false;
    }
    state.z1 = std::abs(next_z1) < kDenormalThreshold ? 0.0F : next_z1;
    state.z2 = std::abs(next_z2) < kDenormalThreshold ? 0.0F : next_z2;
    sample = output;
    return true;
}

void ShelfTiltEQ::rebuild_coefficients() noexcept {
    low_coefficients_ = mode_ == ShelfTiltMode::HighShelf
        ? Coefficients{}
        : make_shelf(frequency_hz_, gain_db_, sample_rate_, false);
    const float high_gain_db = mode_ == ShelfTiltMode::Tilt ? -gain_db_ : gain_db_;
    high_coefficients_ = mode_ == ShelfTiltMode::LowShelf
        ? Coefficients{}
        : make_shelf(frequency_hz_, high_gain_db, sample_rate_, true);
    reset();
}

void ShelfTiltEQ::reset_channel(std::uint16_t channel) noexcept {
    states_[channel] = {};
}

} // namespace tabletfx
