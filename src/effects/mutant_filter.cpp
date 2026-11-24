#include "tabletfx/effects/mutant_filter.hpp"

namespace tabletfx {

MutantFilter::MutantFilter(float sample_rate, std::uint16_t channels,
                           EnvelopeWahParameters parameters)
    : filter_(sample_rate, channels, parameters) {}

void MutantFilter::configure(float sample_rate, std::uint16_t channels,
                             EnvelopeWahParameters parameters) noexcept {
    filter_.configure(sample_rate, channels, parameters);
}

void MutantFilter::process(std::span<float> interleaved,
                           std::uint16_t channels) noexcept {
    filter_.process(interleaved, channels);
}

void MutantFilter::reset() noexcept {
    filter_.reset();
}

} // namespace tabletfx
