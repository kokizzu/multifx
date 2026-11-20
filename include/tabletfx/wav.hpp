#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace tabletfx {

struct AudioBuffer {
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::vector<float> interleaved;

    [[nodiscard]] std::size_t frames() const noexcept {
        return channels == 0 ? 0 : interleaved.size() / channels;
    }
};

[[nodiscard]] AudioBuffer read_wav(const std::filesystem::path& path);
void write_wav_float32(const std::filesystem::path& path, const AudioBuffer& audio);

} // namespace tabletfx
