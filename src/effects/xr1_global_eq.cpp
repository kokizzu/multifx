#include "tabletfx/effects/xr1_global_eq.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr std::array<float, XR1GlobalEQ::kBandCount> kDefaultFrequenciesHz{
    80.0F, 250.0F, 750.0F, 2200.0F, 6600.0F,
};

float bounded(float value, float fallback, float minimum,
              float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

} // namespace

XR1GlobalEQ::XR1GlobalEQ(float sample_rate, std::uint16_t channels)
    : sample_rate_(sample_rate), channels_(channels),
      left_filter_(sample_rate, 1), right_filter_(sample_rate, 1) {
    if (channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument("XR1GlobalEQ requires mono or stereo");
    }
    configure(parameters_);
}

void XR1GlobalEQ::configure(const XR1GlobalEQParameters& parameters) noexcept {
    const float max_frequency_hz = std::min(
        kMaximumFrequencyHz, sample_rate_ * 0.45F);
    for (std::size_t band = 0; band < kBandCount; ++band) {
        XR1GlobalEQBand& destination = parameters_.bands[band];
        const XR1GlobalEQBand& source = parameters.bands[band];
        destination.assign = valid_assign(source.assign);
        destination.type = valid_type(source.type);
        destination.gain_db = bounded(source.gain_db, 0.0F,
                                      kMinimumGainDb, kMaximumGainDb);
        destination.frequency_hz = bounded(
            source.frequency_hz, kDefaultFrequenciesHz[band],
            kMinimumFrequencyHz, max_frequency_hz);
        destination.q = bounded(source.q, 0.707F, kMinimumQ, kMaximumQ);
        destination.bypass = source.bypass;
    }
    parameters_.bypass = parameters.bypass;
    configure_filters();
}

void XR1GlobalEQ::reset_parameters() noexcept {
    configure(XR1GlobalEQParameters{});
}

void XR1GlobalEQ::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels != channels_ || parameters_.bypass || neutral_) return;

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t index = frame * channels_;
        interleaved[index] = left_filter_.process_sample(interleaved[index]);
        if (channels_ == 2) {
            interleaved[index + 1] =
                right_filter_.process_sample(interleaved[index + 1]);
        }
    }
}

void XR1GlobalEQ::reset() noexcept {
    left_filter_.reset();
    right_filter_.reset();
}

XR1GlobalEQAssign XR1GlobalEQ::valid_assign(
    XR1GlobalEQAssign assign) noexcept {
    switch (assign) {
    case XR1GlobalEQAssign::Stereo:
    case XR1GlobalEQAssign::Left:
    case XR1GlobalEQAssign::Right:
        return assign;
    }
    return XR1GlobalEQAssign::Stereo;
}

XR1GlobalEQType XR1GlobalEQ::valid_type(XR1GlobalEQType type) noexcept {
    switch (type) {
    case XR1GlobalEQType::Peak:
    case XR1GlobalEQType::HighPass:
    case XR1GlobalEQType::LowPass:
    case XR1GlobalEQType::HighShelf:
    case XR1GlobalEQType::LowShelf:
        return type;
    }
    return XR1GlobalEQType::Peak;
}

FiveBandEQShape XR1GlobalEQ::filter_shape(XR1GlobalEQType type) const noexcept {
    switch (type) {
    case XR1GlobalEQType::Peak:
        return FiveBandEQShape::Peaking;
    case XR1GlobalEQType::HighPass:
        return FiveBandEQShape::LowCut;
    case XR1GlobalEQType::LowPass:
        return FiveBandEQShape::HighCut;
    case XR1GlobalEQType::HighShelf:
        return FiveBandEQShape::HighShelf;
    case XR1GlobalEQType::LowShelf:
        return FiveBandEQShape::LowShelf;
    }
    return FiveBandEQShape::Peaking;
}

void XR1GlobalEQ::configure_filters() noexcept {
    std::array<FiveBandEQBand, kBandCount> left{};
    std::array<FiveBandEQBand, kBandCount> right{};
    bool left_is_neutral = true;
    bool right_is_neutral = true;
    for (std::size_t band = 0; band < kBandCount; ++band) {
        const XR1GlobalEQBand& source = parameters_.bands[band];
        FiveBandEQBand neutral;
        neutral.frequency_hz = source.frequency_hz;
        neutral.gain_db = 0.0F;
        neutral.q = source.q;
        neutral.shape = FiveBandEQShape::Peaking;
        neutral.slope_db_per_octave = 12.0F;
        left[band] = neutral;
        right[band] = neutral;

        if (source.bypass) continue;
        FiveBandEQBand active = neutral;
        active.gain_db = source.gain_db;
        active.shape = filter_shape(source.type);
        const bool use_left = source.assign != XR1GlobalEQAssign::Right;
        const bool use_right = source.assign != XR1GlobalEQAssign::Left;
        if (use_left) {
            left[band] = active;
            left_is_neutral = left_is_neutral &&
                source.type != XR1GlobalEQType::HighPass &&
                source.type != XR1GlobalEQType::LowPass &&
                source.gain_db == 0.0F;
        }
        if (use_right) {
            right[band] = active;
            right_is_neutral = right_is_neutral &&
                source.type != XR1GlobalEQType::HighPass &&
                source.type != XR1GlobalEQType::LowPass &&
                source.gain_db == 0.0F;
        }
    }
    left_filter_.configure_global_eq(left);
    right_filter_.configure_global_eq(right);
    neutral_ = left_is_neutral && right_is_neutral;
}

} // namespace tabletfx
