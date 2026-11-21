#include "tabletfx/effects/bass_graphic_eq.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tabletfx {
namespace {

constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kMinimumSampleRate = 16000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kFourBandQ = 1.0F;
constexpr float kSevenBandQ = 1.41421356F;
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

float bounded_gain(float gain_db) noexcept {
    if (!std::isfinite(gain_db)) {
        return 0.0F;
    }
    return std::clamp(gain_db, BassGraphicEQ::kMinimumGainDb,
                      BassGraphicEQ::kMaximumGainDb);
}

} // namespace

BassGraphicEQ::BassGraphicEQ(float sample_rate, std::uint16_t channels,
                             BassGraphicEQLayout layout) noexcept
    : sample_rate_(valid_sample_rate(sample_rate)),
      channels_(valid_channels(channels)),
      layout_(valid_layout(layout)),
      band_count_(band_count_for_layout(layout_)) {
    rebuild_coefficients();
}

std::string_view BassGraphicEQ::id() const noexcept {
    return "bass-graphic-eq";
}

std::size_t BassGraphicEQ::memory_bytes() const noexcept {
    return sizeof(*this);
}

float BassGraphicEQ::sample_rate() const noexcept {
    return sample_rate_;
}

std::uint16_t BassGraphicEQ::channels() const noexcept {
    return channels_;
}

BassGraphicEQLayout BassGraphicEQ::layout() const noexcept {
    return layout_;
}

std::size_t BassGraphicEQ::band_count() const noexcept {
    return band_count_;
}

float BassGraphicEQ::band_frequency_hz(std::size_t band) const noexcept {
    if (band >= band_count_) {
        return 0.0F;
    }
    return center_frequency_hz(layout_, band);
}

float BassGraphicEQ::band_gain_db(std::size_t band) const noexcept {
    if (band >= band_count_) {
        return 0.0F;
    }
    return gains_db_[band];
}

bool BassGraphicEQ::set_band_gain_db(std::size_t band, float gain_db) noexcept {
    if (band >= band_count_) {
        return false;
    }
    gains_db_[band] = bounded_gain(gain_db);
    const float q = layout_ == BassGraphicEQLayout::FourBand
        ? kFourBandQ
        : kSevenBandQ;
    coefficients_[band] = make_peak(center_frequency_hz(layout_, band),
                                    gains_db_[band], q, sample_rate_);
    reset();
    return true;
}

void BassGraphicEQ::set_layout(BassGraphicEQLayout layout) noexcept {
    layout = valid_layout(layout);
    if (layout == layout_) {
        return;
    }
    layout_ = layout;
    band_count_ = band_count_for_layout(layout_);
    gains_db_.fill(0.0F);
    rebuild_coefficients();
}

void BassGraphicEQ::process(std::span<float> interleaved,
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

            bool valid = true;
            for (std::size_t band = 0; band < band_count_; ++band) {
                if (!process_section(sample, coefficients_[band],
                                     states_[channel].bands[band])) {
                    valid = false;
                    break;
                }
            }
            if (!valid || !std::isfinite(sample)) {
                sample = 0.0F;
                reset_channel(channel);
            }
        }
    }
}

void BassGraphicEQ::reset() noexcept {
    for (ChannelState& state : states_) {
        state = {};
    }
}

float BassGraphicEQ::center_frequency_hz(BassGraphicEQLayout layout,
                                         std::size_t band) noexcept {
    constexpr std::array<float, 4> kFourBandCenters{
        50.0F, 250.0F, 1000.0F, 4000.0F,
    };
    constexpr std::array<float, 7> kSevenBandCenters{
        40.0F, 80.0F, 160.0F, 320.0F, 640.0F, 1600.0F, 5000.0F,
    };
    if (layout == BassGraphicEQLayout::FourBand) {
        return band < kFourBandCenters.size() ? kFourBandCenters[band] : 0.0F;
    }
    return band < kSevenBandCenters.size() ? kSevenBandCenters[band] : 0.0F;
}

std::size_t BassGraphicEQ::band_count_for_layout(
    BassGraphicEQLayout layout) noexcept {
    return layout == BassGraphicEQLayout::FourBand ? 4 : 7;
}

BassGraphicEQLayout BassGraphicEQ::valid_layout(BassGraphicEQLayout layout) noexcept {
    return layout == BassGraphicEQLayout::FourBand
        ? BassGraphicEQLayout::FourBand
        : BassGraphicEQLayout::SevenBand;
}

BassGraphicEQ::Coefficients BassGraphicEQ::make_peak(float frequency_hz,
                                                      float gain_db,
                                                      float q,
                                                      float sample_rate) noexcept {
    if (gain_db == 0.0F) {
        return {};
    }

    const double frequency = static_cast<double>(frequency_hz);
    const double rate = static_cast<double>(sample_rate);
    const double omega = 2.0 * kPi * frequency / rate;
    const double cosine = std::cos(omega);
    const double alpha = std::sin(omega) / (2.0 * static_cast<double>(q));
    const double amplitude = std::pow(10.0, static_cast<double>(gain_db) / 40.0);
    const double a0 = 1.0 + alpha / amplitude;
    if (!std::isfinite(a0) || std::abs(a0) < std::numeric_limits<double>::min()) {
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

bool BassGraphicEQ::process_section(float& sample,
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

void BassGraphicEQ::rebuild_coefficients() noexcept {
    const float q = layout_ == BassGraphicEQLayout::FourBand
        ? kFourBandQ
        : kSevenBandQ;
    for (std::size_t band = 0; band < kMaximumBands; ++band) {
        if (band < band_count_) {
            coefficients_[band] = make_peak(center_frequency_hz(layout_, band),
                                            gains_db_[band], q, sample_rate_);
        } else {
            coefficients_[band] = {};
        }
    }
    reset();
}

void BassGraphicEQ::reset_channel(std::uint16_t channel) noexcept {
    states_[channel] = {};
}

} // namespace tabletfx
