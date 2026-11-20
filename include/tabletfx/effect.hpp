#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace tabletfx {

class Effect {
public:
    virtual ~Effect() = default;
    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    virtual void process(std::span<float> interleaved, std::uint16_t channels) noexcept = 0;
    virtual void reset() noexcept {}
};

[[nodiscard]] inline std::size_t frame_count(std::span<const float> interleaved,
                                              std::uint16_t channels) noexcept {
    return channels == 0 ? 0 : interleaved.size() / channels;
}

} // namespace tabletfx
