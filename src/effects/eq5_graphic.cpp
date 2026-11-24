#include "tabletfx/effects/eq5_graphic.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tabletfx {
namespace {

constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kMinimumSampleRate = 16000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kProjectBandQ = 1.0F;
constexpr float kDenormalThreshold = 1.0e-20F;
constexpr double kPi = 3.141592653589793238462643383279502884;

} // namespace

EQ5Graphic::EQ5Graphic(float sample_rate, std::uint16_t channels) noexcept
    : sample_rate_(valid_sample_rate(sample_rate)),
      channels_(valid_channels(channels)) {
    for (std::size_t band = 0; band < kBandCount; ++band) {
        coefficients_[band] = make_peak(kCenterFrequenciesHz[band], 0.0F,
                                        sample_rate_);
    }
}

float EQ5Graphic::band_frequency_hz(std::size_t band) const noexcept {
    return band < kBandCount ? kCenterFrequenciesHz[band] : 0.0F;
}

float EQ5Graphic::band_gain_db(std::size_t band) const noexcept {
    return band < kBandCount ? gains_db_[band] : 0.0F;
}

bool EQ5Graphic::set_band_gain_db(std::size_t band, float gain_db) noexcept {
    if (band >= kBandCount) return false;
    gains_db_[band] = bounded_gain(gain_db);
    coefficients_[band] = make_peak(kCenterFrequenciesHz[band],
                                    gains_db_[band], sample_rate_);
    return true;
}

void EQ5Graphic::set_overall_gain_db(float gain_db) noexcept {
    overall_gain_db_ = bounded_gain(gain_db);
    overall_gain_ = std::pow(10.0F, overall_gain_db_ / 20.0F);
}

void EQ5Graphic::configure(
    const std::array<float, kBandCount>& band_gains_db,
    float overall_gain_db) noexcept {
    for (std::size_t band = 0; band < kBandCount; ++band) {
        (void)set_band_gain_db(band, band_gains_db[band]);
    }
    set_overall_gain_db(overall_gain_db);
}

void EQ5Graphic::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    if (channels == 0 || channels != channels_) return;

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

            bool valid = true;
            for (std::size_t band = 0; band < kBandCount; ++band) {
                if (!process_section(sample, coefficients_[band],
                                     states_[channel][band])) {
                    valid = false;
                    break;
                }
            }
            if (!valid || !std::isfinite(sample)) {
                sample = 0.0F;
                reset_channel(channel);
                continue;
            }

            sample *= overall_gain_;
            if (!std::isfinite(sample)) {
                sample = 0.0F;
                reset_channel(channel);
            }
        }
    }
}

void EQ5Graphic::reset() noexcept {
    for (ChannelState& channel_state : states_) channel_state = {};
}

float EQ5Graphic::valid_sample_rate(float sample_rate) noexcept {
    if (!std::isfinite(sample_rate) || sample_rate < kMinimumSampleRate ||
        sample_rate > kMaximumSampleRate) {
        return kDefaultSampleRate;
    }
    return sample_rate;
}

std::uint16_t EQ5Graphic::valid_channels(std::uint16_t channels) noexcept {
    return channels == 1 ? 1 : 2;
}

float EQ5Graphic::bounded_gain(float gain_db) noexcept {
    if (!std::isfinite(gain_db)) return 0.0F;
    return std::clamp(gain_db, kMinimumGainDb, kMaximumGainDb);
}

EQ5Graphic::Coefficients EQ5Graphic::make_peak(float frequency_hz,
                                              float gain_db,
                                              float sample_rate) noexcept {
    if (gain_db == 0.0F) return {};

    const double frequency = static_cast<double>(frequency_hz);
    const double rate = static_cast<double>(sample_rate);
    const double omega = 2.0 * kPi * frequency / rate;
    const double cosine = std::cos(omega);
    const double alpha = std::sin(omega) / (2.0 * kProjectBandQ);
    const double amplitude = std::pow(10.0, static_cast<double>(gain_db) / 40.0);
    const double a0 = 1.0 + alpha / amplitude;
    if (!std::isfinite(a0) ||
        std::abs(a0) < std::numeric_limits<double>::min()) {
        return {};
    }

    const double inverse_a0 = 1.0 / a0;
    const Coefficients result{
        static_cast<float>((1.0 + alpha * amplitude) * inverse_a0),
        static_cast<float>((-2.0 * cosine) * inverse_a0),
        static_cast<float>((1.0 - alpha * amplitude) * inverse_a0),
        static_cast<float>((-2.0 * cosine) * inverse_a0),
        static_cast<float>((1.0 - alpha / amplitude) * inverse_a0),
    };
    if (!std::isfinite(result.b0) || !std::isfinite(result.b1) ||
        !std::isfinite(result.b2) || !std::isfinite(result.a1) ||
        !std::isfinite(result.a2)) {
        return {};
    }
    return result;
}

bool EQ5Graphic::process_section(float& sample,
                                 const Coefficients& coefficients,
                                 SectionState& state) noexcept {
    const float output = coefficients.b0 * sample + state.z1;
    const float next_z1 = coefficients.b1 * sample -
                          coefficients.a1 * output + state.z2;
    const float next_z2 = coefficients.b2 * sample -
                          coefficients.a2 * output;
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

void EQ5Graphic::reset_channel(std::uint16_t channel) noexcept {
    states_[channel] = {};
}

} // namespace tabletfx
