#include "tabletfx/effects/pedal_detune.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kShallowDepthMs = 0.35F;
constexpr float kDeepDepthMs = 1.4F;
constexpr float kProjectRateHz = 0.25F;
constexpr float kProjectWidth = 0.75F;
constexpr float kPositionSmoothingMs = 10.0F;

} // namespace

PedalDetune::PedalDetune(float sample_rate, std::uint16_t channels,
                         PedalDetuneParameters parameters)
    : parameters_{sanitize_mode(parameters.mode),
                  bounded(parameters.position, 0.5F, 0.0F, 1.0F)},
      core_(sample_rate, channels, core_parameters(parameters_)) {}

std::size_t PedalDetune::memory_bytes() const noexcept {
    return sizeof(*this) + core_.memory_bytes() - sizeof(PolyDetune);
}

void PedalDetune::set_parameters(PedalDetuneParameters parameters) noexcept {
    parameters_.mode = sanitize_mode(parameters.mode);
    parameters_.position = bounded(parameters.position, parameters_.position,
                                   0.0F, 1.0F);
    core_.set_parameters(core_parameters(parameters_));
}

void PedalDetune::set_mode(PedalDetuneMode mode) noexcept {
    parameters_.mode = sanitize_mode(mode);
    core_.set_parameters(core_parameters(parameters_));
}

void PedalDetune::set_position(float position) noexcept {
    parameters_.position = bounded(position, parameters_.position, 0.0F, 1.0F);
    core_.set_parameters(core_parameters(parameters_));
}

void PedalDetune::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    core_.process(interleaved, channels);
}

void PedalDetune::reset() noexcept {
    core_.reset();
}

PedalDetuneMode PedalDetune::sanitize_mode(PedalDetuneMode mode) noexcept {
    switch (mode) {
    case PedalDetuneMode::Shallow:
    case PedalDetuneMode::Deep:
        return mode;
    }
    return PedalDetuneMode::Shallow;
}

float PedalDetune::bounded(float value, float fallback,
                           float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

PolyDetuneParameters PedalDetune::core_parameters(
    PedalDetuneParameters parameters) noexcept {
    const bool deep = sanitize_mode(parameters.mode) == PedalDetuneMode::Deep;
    PolyDetuneParameters result;
    result.rate_hz = kProjectRateHz;
    result.depth_ms = deep ? kDeepDepthMs : kShallowDepthMs;
    result.voice_count = 2U;
    result.width = kProjectWidth;
    result.mix = bounded(parameters.position, 0.5F, 0.0F, 1.0F);
    result.level_db = 0.0F;
    result.smoothing_ms = kPositionSmoothingMs;
    return result;
}

} // namespace tabletfx
