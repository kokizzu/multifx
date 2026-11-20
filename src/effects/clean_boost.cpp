#include "tabletfx/effects/clean_boost.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {

CleanBoost::CleanBoost(float gain_db, float sample_rate, float smoothing_ms) noexcept
    : gain_(bounded_gain(gain_db), sample_rate, smoothing_ms) {}

void CleanBoost::set_gain_db(float gain_db) noexcept {
    gain_.set_gain_db(bounded_gain(gain_db));
}

void CleanBoost::set_gain_db_immediate(float gain_db) noexcept {
    gain_.set_gain_db_immediate(bounded_gain(gain_db));
}

void CleanBoost::configure_smoothing(float sample_rate, float smoothing_ms) noexcept {
    gain_.configure_smoothing(sample_rate, smoothing_ms);
}

void CleanBoost::process(std::span<float> interleaved,
                         std::uint16_t channels) noexcept {
    gain_.process(interleaved, channels);
}

void CleanBoost::reset() noexcept { gain_.reset(); }

float CleanBoost::bounded_gain(float gain_db) noexcept {
    return std::isfinite(gain_db) ? std::clamp(gain_db, 0.0F, 24.0F) : 0.0F;
}

} // namespace tabletfx
