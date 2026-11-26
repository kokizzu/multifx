#include "tabletfx/effects/tone_master_pro_3d_chorus.hpp"

#include <cstddef>
#include <cstdint>

namespace tabletfx {

ToneMasterPro3DChorus::ToneMasterPro3DChorus(float sample_rate,
                                             std::uint16_t channels)
    : chorus_(sample_rate, channels,
              parameters_for_mode(ToneMasterPro3DChorusMode::Mode1)) {}

std::size_t ToneMasterPro3DChorus::memory_bytes() const noexcept {
    // The Chorus member is already included in sizeof(*this); add only its
    // owned delay ring, as Chorus::memory_bytes includes its own object size.
    return sizeof(*this) + chorus_.memory_bytes() - sizeof(Chorus);
}

void ToneMasterPro3DChorus::set_mode(ToneMasterPro3DChorusMode mode) noexcept {
    if (static_cast<std::uint8_t>(mode) >
        static_cast<std::uint8_t>(ToneMasterPro3DChorusMode::Mode4)) {
        mode = ToneMasterPro3DChorusMode::Mode1;
    }
    mode_ = mode;
    chorus_.set_parameters(parameters_for_mode(mode_));
}

void ToneMasterPro3DChorus::process(std::span<float> interleaved,
                                    std::uint16_t channels) noexcept {
    chorus_.process(interleaved, channels);
}

void ToneMasterPro3DChorus::reset() noexcept {
    chorus_.reset();
}

ChorusParameters ToneMasterPro3DChorus::parameters_for_mode(
    ToneMasterPro3DChorusMode mode) noexcept {
    ChorusParameters parameters;
    parameters.tempo_sync = false;
    parameters.number_of_voices = 1U;
    parameters.smoothing_ms = 10.0F;
    parameters.low_cut_hz = 0.0F;
    parameters.high_cut_hz = 0.0F;

    // Project-authored voicings ordered from subtle to pronounced. These are
    // not Fender's unpublished Mode curves or numeric settings. Keeping one
    // chorus voice bounds the extra mode cost while varying motion and width.
    switch (mode) {
    case ToneMasterPro3DChorusMode::Mode1:
        parameters.rate_hz = 0.45F;
        parameters.minimum_delay_ms = 11.0F;
        parameters.delay_range_ms = 1.5F;
        parameters.mix = 0.20F;
        parameters.stereo_width = 0.8F;
        break;
    case ToneMasterPro3DChorusMode::Mode2:
        parameters.rate_hz = 0.65F;
        parameters.minimum_delay_ms = 9.0F;
        parameters.delay_range_ms = 4.0F;
        parameters.mix = 0.28F;
        parameters.stereo_width = 1.1F;
        break;
    case ToneMasterPro3DChorusMode::Mode3:
        parameters.rate_hz = 0.85F;
        parameters.minimum_delay_ms = 7.0F;
        parameters.delay_range_ms = 8.0F;
        parameters.mix = 0.36F;
        parameters.stereo_width = 1.45F;
        break;
    case ToneMasterPro3DChorusMode::Mode4:
        parameters.rate_hz = 1.2F;
        parameters.minimum_delay_ms = 5.0F;
        parameters.delay_range_ms = 12.0F;
        parameters.mix = 0.44F;
        parameters.stereo_width = 1.8F;
        break;
    }
    return parameters;
}

} // namespace tabletfx
