#pragma once

#include "tabletfx/effects/envelope_wah.hpp"

namespace tabletfx {

// A project-authored envelope-filter effect inspired by the Line 6 Stadium
// Mutant Filter listing. The public Stadium parameter table is unavailable;
// these controls and their ranges are not claims of Line 6 parameter parity.
class MutantFilter final : public Effect {
public:
    explicit MutantFilter(float sample_rate = 48000.0F,
                          std::uint16_t channels = 2,
                          EnvelopeWahParameters parameters = {});

    [[nodiscard]] std::string_view id() const noexcept override {
        return "mutant-filter";
    }
    [[nodiscard]] std::size_t memory_bytes() const noexcept override {
        return sizeof(*this);
    }
    [[nodiscard]] EnvelopeWahParameters parameters() const noexcept {
        return filter_.parameters();
    }

    void configure(float sample_rate, std::uint16_t channels,
                   EnvelopeWahParameters parameters) noexcept;
    void process(std::span<float> interleaved,
                 std::uint16_t channels) noexcept override;
    void reset() noexcept override;

private:
    EnvelopeWah filter_;
};

} // namespace tabletfx
