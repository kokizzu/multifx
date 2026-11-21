#include "tabletfx/effects.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx {

FilteredDelay::FilteredDelay(float sample_rate, std::uint16_t channels,
                             float max_delay_ms,
                             FilteredDelayParameters parameters)
    : sample_rate_(sample_rate), max_delay_ms_(max_delay_ms),
      delay_(sample_rate, channels, max_delay_ms, 350.0F, 0.25F, 0.25F) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < 8000.0F ||
        sample_rate_ > 384000.0F) {
        throw std::invalid_argument(
            "Filtered Delay requires a sample rate between 8 kHz and 384 kHz");
    }
    configure(parameters);
    delay_.reset();
}

std::size_t FilteredDelay::memory_bytes() const noexcept {
    return sizeof(*this) + delay_.memory_bytes() - sizeof(Delay);
}

void FilteredDelay::configure(FilteredDelayParameters parameters) noexcept {
    const float maximum_time = std::min(2000.0F, max_delay_ms_);
    parameters.time_ms = std::isfinite(parameters.time_ms)
        ? std::clamp(parameters.time_ms, 1.0F, maximum_time)
        : 350.0F;
    parameters.feedback = std::isfinite(parameters.feedback)
        ? std::clamp(parameters.feedback, 0.0F, 0.95F)
        : 0.25F;
    parameters.mix = std::isfinite(parameters.mix)
        ? std::clamp(parameters.mix, 0.0F, 1.0F)
        : 0.25F;
    const float low_max = std::max(20.0F, std::min(1000.0F, sample_rate_ * 0.15F));
    const float high_max = std::max(20.0F, std::min(20000.0F, sample_rate_ * 0.45F));
    const float high_min = std::min(2000.0F, high_max);
    parameters.low_cut_hz = std::isfinite(parameters.low_cut_hz)
        ? std::clamp(parameters.low_cut_hz, 20.0F, low_max)
        : 120.0F;
    parameters.high_cut_hz = std::isfinite(parameters.high_cut_hz)
        ? std::clamp(parameters.high_cut_hz, high_min, high_max)
        : std::clamp(6000.0F, high_min, high_max);
    parameters_ = parameters;
    delay_.configure(parameters_.time_ms, parameters_.feedback, parameters_.mix);
    delay_.configure_repeat_filters(true, parameters_.low_cut_hz,
                                    parameters_.high_cut_hz);
}

void FilteredDelay::process(std::span<float> interleaved,
                            std::uint16_t channels) noexcept {
    delay_.process(interleaved, channels);
}

void FilteredDelay::reset() noexcept {
    delay_.reset();
}

} // namespace tabletfx
