#include "tabletfx/wav.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace tabletfx {
namespace {

std::uint16_t read_u16(const unsigned char* bytes) noexcept {
    return static_cast<std::uint16_t>(bytes[0] | (static_cast<std::uint16_t>(bytes[1]) << 8U));
}

std::uint32_t read_u32(const unsigned char* bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

void write_u16(std::ostream& stream, std::uint16_t value) {
    const std::array<unsigned char, 2> bytes{
        static_cast<unsigned char>(value & 0xffU),
        static_cast<unsigned char>((value >> 8U) & 0xffU),
    };
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void write_u32(std::ostream& stream, std::uint32_t value) {
    const std::array<unsigned char, 4> bytes{
        static_cast<unsigned char>(value & 0xffU),
        static_cast<unsigned char>((value >> 8U) & 0xffU),
        static_cast<unsigned char>((value >> 16U) & 0xffU),
        static_cast<unsigned char>((value >> 24U) & 0xffU),
    };
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void require_read(std::istream& stream, char* destination, std::streamsize bytes, const std::filesystem::path& path) {
    stream.read(destination, bytes);
    if (stream.gcount() != bytes) {
        throw std::runtime_error("Unexpected end of WAV file: " + path.string());
    }
}

} // namespace

AudioBuffer read_wav(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Could not open WAV file: " + path.string());
    }
    std::array<char, 12> header{};
    require_read(stream, header.data(), static_cast<std::streamsize>(header.size()), path);
    if (std::string(header.data(), 4) != "RIFF" || std::string(header.data() + 8, 4) != "WAVE") {
        throw std::runtime_error("Only little-endian RIFF/WAVE files are supported: " + path.string());
    }

    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t block_align = 0;
    std::uint16_t bits_per_sample = 0;
    std::vector<unsigned char> data;
    bool have_format = false;
    bool have_data = false;

    while (stream) {
        std::array<unsigned char, 8> chunk_header{};
        stream.read(reinterpret_cast<char*>(chunk_header.data()), static_cast<std::streamsize>(chunk_header.size()));
        if (stream.gcount() == 0) {
            break;
        }
        if (stream.gcount() != static_cast<std::streamsize>(chunk_header.size())) {
            throw std::runtime_error("Truncated WAV chunk header: " + path.string());
        }
        const std::string chunk_id(reinterpret_cast<const char*>(chunk_header.data()), 4);
        const std::uint32_t chunk_size = read_u32(chunk_header.data() + 4);
        if (chunk_id == "fmt ") {
            if (chunk_size < 16 || chunk_size > 65536) {
                throw std::runtime_error("Unsupported WAV format chunk size: " + path.string());
            }
            std::vector<unsigned char> fmt(chunk_size);
            require_read(stream, reinterpret_cast<char*>(fmt.data()), static_cast<std::streamsize>(fmt.size()), path);
            format = read_u16(fmt.data());
            channels = read_u16(fmt.data() + 2);
            sample_rate = read_u32(fmt.data() + 4);
            block_align = read_u16(fmt.data() + 12);
            bits_per_sample = read_u16(fmt.data() + 14);
            have_format = true;
        } else if (chunk_id == "data") {
            data.resize(chunk_size);
            require_read(stream, reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()), path);
            have_data = true;
        } else {
            stream.seekg(static_cast<std::streamoff>(chunk_size), std::ios::cur);
            if (!stream) {
                throw std::runtime_error("Invalid WAV chunk size: " + path.string());
            }
        }
        if ((chunk_size & 1U) != 0U) {
            stream.seekg(1, std::ios::cur);
        }
    }

    if (!have_format || !have_data || channels == 0 || channels > 8 || sample_rate < 8000 || sample_rate > 192000) {
        throw std::runtime_error("Missing or unsupported WAV format/data: " + path.string());
    }
    const bool pcm = format == 1 && (bits_per_sample == 16 || bits_per_sample == 24 || bits_per_sample == 32);
    const bool float32 = format == 3 && bits_per_sample == 32;
    if (!pcm && !float32) {
        throw std::runtime_error("WAV must be PCM 16/24/32-bit or IEEE float32: " + path.string());
    }
    const std::uint16_t bytes_per_sample = static_cast<std::uint16_t>(bits_per_sample / 8);
    const std::uint16_t expected_align = static_cast<std::uint16_t>(channels * bytes_per_sample);
    if (block_align != expected_align || block_align == 0) {
        throw std::runtime_error("Invalid WAV block alignment: " + path.string());
    }
    const std::size_t unaligned_bytes = data.size() % block_align;
    if (unaligned_bytes != 0) {
        // A small number of PCM writers include the RIFF word-padding byte in
        // the data chunk length. Accept only one terminal zero byte; reject
        // incomplete samples or frames of any other shape.
        if (unaligned_bytes != 1 || data.empty() || data.back() != 0 || !pcm) {
            throw std::runtime_error("Invalid WAV block alignment: " + path.string());
        }
        data.pop_back();
    }

    AudioBuffer audio{sample_rate, channels, {}};
    audio.interleaved.resize(data.size() / bytes_per_sample);
    for (std::size_t i = 0, offset = 0; i < audio.interleaved.size(); ++i, offset += bytes_per_sample) {
        float sample = 0.0F;
        if (float32) {
            sample = std::bit_cast<float>(read_u32(data.data() + offset));
        } else if (bits_per_sample == 16) {
            const auto value = static_cast<std::int16_t>(read_u16(data.data() + offset));
            sample = static_cast<float>(value) / 32768.0F;
        } else if (bits_per_sample == 24) {
            std::int32_t value = static_cast<std::int32_t>(data[offset]) |
                                 (static_cast<std::int32_t>(data[offset + 1]) << 8U) |
                                 (static_cast<std::int32_t>(data[offset + 2]) << 16U);
            if ((value & 0x00800000) != 0) {
                value |= static_cast<std::int32_t>(0xff000000U);
            }
            sample = static_cast<float>(value) / 8388608.0F;
        } else {
            const auto value = static_cast<std::int32_t>(read_u32(data.data() + offset));
            sample = static_cast<float>(static_cast<double>(value) / 2147483648.0);
        }
        if (!std::isfinite(sample)) {
            throw std::runtime_error("WAV contains a non-finite audio sample: " + path.string());
        }
        audio.interleaved[i] = sample;
    }
    return audio;
}

void write_wav_float32(const std::filesystem::path& path, const AudioBuffer& audio) {
    if (audio.sample_rate < 8000 || audio.sample_rate > 192000 || audio.channels == 0 || audio.channels > 8 ||
        audio.interleaved.empty() || audio.interleaved.size() % audio.channels != 0) {
        throw std::invalid_argument("Invalid audio buffer for WAV output");
    }
    const std::uint64_t data_bytes_64 = static_cast<std::uint64_t>(audio.interleaved.size()) * sizeof(float);
    if (data_bytes_64 > std::numeric_limits<std::uint32_t>::max() - 36U) {
        throw std::length_error("WAV output exceeds RIFF size limit");
    }
    const auto data_bytes = static_cast<std::uint32_t>(data_bytes_64);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("Could not create WAV file: " + path.string());
    }
    stream.write("RIFF", 4);
    write_u32(stream, 36U + data_bytes);
    stream.write("WAVEfmt ", 8);
    write_u32(stream, 16U);
    write_u16(stream, 3U);
    write_u16(stream, audio.channels);
    write_u32(stream, audio.sample_rate);
    write_u32(stream, audio.sample_rate * audio.channels * static_cast<std::uint32_t>(sizeof(float)));
    write_u16(stream, static_cast<std::uint16_t>(audio.channels * sizeof(float)));
    write_u16(stream, 32U);
    stream.write("data", 4);
    write_u32(stream, data_bytes);
    for (float sample : audio.interleaved) {
        write_u32(stream, std::bit_cast<std::uint32_t>(sample));
    }
    if (!stream) {
        throw std::runtime_error("Failed while writing WAV file: " + path.string());
    }
}

} // namespace tabletfx
