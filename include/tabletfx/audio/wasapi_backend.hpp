#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace tabletfx::audio {

struct WasapiConfig {
    // Maximum frames passed to one processing callback. Device event sizes may
    // differ; larger events are split into bounded calls of at most this size.
    std::uint32_t max_block_frames = 128;
};

struct WasapiRuntimeInfo {
    std::uint32_t sample_rate_hz = 0;
    std::uint32_t max_block_frames = 0;
    std::uint16_t input_channels = 0;
    std::uint16_t output_channels = 0;
};

struct WasapiCounters {
    std::uint64_t capture_underrun_blocks = 0;
    std::uint64_t capture_overrun_frames = 0;
};

// The callback runs on the WASAPI event thread. It must be noexcept, bounded,
// and must not allocate, lock, wait, log, access files/network, or call UI code.
// Input and output are interleaved float32 buffers. Missing capture frames are
// zero-filled; output is cleared before every callback invocation.
using WasapiProcessCallback = void (*)(void* context,
                                       const float* input_interleaved,
                                       std::uint16_t input_channels,
                                       float* output_interleaved,
                                       std::uint16_t output_channels,
                                       std::uint32_t frames) noexcept;

// Initial Windows backend: default capture/render endpoints in shared mode.
// It accepts the endpoint mix formats when both use the same sample rate and
// converts supported PCM/float samples to/from float32. Device selection,
// sample-rate conversion, exclusive mode, and real-device latency measurement
// are intentionally left to later platform work.
class WasapiBackend final {
public:
    WasapiBackend();
    ~WasapiBackend();

    WasapiBackend(const WasapiBackend&) = delete;
    WasapiBackend& operator=(const WasapiBackend&) = delete;
    WasapiBackend(WasapiBackend&&) = delete;
    WasapiBackend& operator=(WasapiBackend&&) = delete;

    // Opens the default microphone/input and render/output endpoints and starts
    // event-driven streams. Returns false with error_message() populated if
    // the route cannot be opened or the endpoints use different sample rates.
    [[nodiscard]] bool start(const WasapiConfig& config,
                             WasapiProcessCallback callback,
                             void* context) noexcept;
    void stop() noexcept;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] WasapiRuntimeInfo runtime_info() const noexcept;
    [[nodiscard]] WasapiCounters counters() const noexcept;
    [[nodiscard]] std::string error_message() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tabletfx::audio
