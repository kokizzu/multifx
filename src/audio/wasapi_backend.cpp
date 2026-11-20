#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "tabletfx/audio/wasapi_backend.hpp"

#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <avrt.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <mmreg.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace tabletfx::audio {
namespace {

template <typename T>
class ComPtr final {
public:
    ComPtr() noexcept = default;
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    [[nodiscard]] T* get() const noexcept { return value_; }
    [[nodiscard]] T* operator->() const noexcept { return value_; }
    [[nodiscard]] explicit operator bool() const noexcept { return value_ != nullptr; }

    [[nodiscard]] T** put() noexcept {
        reset();
        return &value_;
    }

    void reset(T* value = nullptr) noexcept {
        if (value_ != nullptr) {
            value_->Release();
        }
        value_ = value;
    }

private:
    T* value_ = nullptr;
};

struct HandleCloser final {
    void operator()(HANDLE handle) const noexcept {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
};

using UniqueHandle = std::unique_ptr<void, HandleCloser>;

struct WaveFormatCloser final {
    void operator()(WAVEFORMATEX* format) const noexcept {
        if (format != nullptr) {
            CoTaskMemFree(format);
        }
    }
};

using UniqueWaveFormat = std::unique_ptr<WAVEFORMATEX, WaveFormatCloser>;

constexpr GUID kPcmSubtype{0x00000001, 0x0000, 0x0010,
                           {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
constexpr GUID kIeeeFloatSubtype{0x00000003, 0x0000, 0x0010,
                                 {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

[[nodiscard]] std::string hresult_message(const char* operation, HRESULT result) {
    std::ostringstream stream;
    stream << operation << " failed (HRESULT 0x" << std::hex << std::uppercase
           << static_cast<unsigned long>(result) << ")";
    return stream.str();
}

[[nodiscard]] std::string win32_message(const char* operation, DWORD result) {
    std::ostringstream stream;
    stream << operation << " failed (Win32 error " << result << ")";
    return stream.str();
}

struct AudioFormat final {
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::uint16_t bits_per_sample = 0;
    std::uint16_t block_align = 0;
    bool is_float = false;
};

[[nodiscard]] bool describe_format(const WAVEFORMATEX* source, AudioFormat& output,
                                   std::string& error) {
    if (source == nullptr || source->nChannels == 0 || source->nChannels > 8 ||
        source->nSamplesPerSec < 8000 || source->nSamplesPerSec > 192000 ||
        source->nBlockAlign == 0) {
        error = "WASAPI endpoint has an unsupported channel count or sample rate";
        return false;
    }

    bool pcm = source->wFormatTag == WAVE_FORMAT_PCM;
    bool is_float = source->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    if (source->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        constexpr std::size_t extensible_size = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        if (source->cbSize < extensible_size) {
            error = "WASAPI endpoint returned a truncated extensible audio format";
            return false;
        }
        const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(source);
        pcm = IsEqualGUID(extensible->SubFormat, kPcmSubtype) != FALSE;
        is_float = IsEqualGUID(extensible->SubFormat, kIeeeFloatSubtype) != FALSE;
    }

    const std::uint16_t bits = source->wBitsPerSample;
    const bool supported_pcm_bits = bits == 8 || bits == 16 || bits == 24 || bits == 32;
    if ((!pcm && !is_float) || (pcm && !supported_pcm_bits) || (is_float && bits != 32)) {
        error = "WASAPI endpoint format is not supported (expected PCM 8/16/24/32 or float32)";
        return false;
    }
    const auto bytes_per_sample = static_cast<std::uint32_t>(bits / 8U);
    const auto expected_align = static_cast<std::uint32_t>(source->nChannels) * bytes_per_sample;
    if (expected_align != source->nBlockAlign) {
        error = "WASAPI endpoint format has an unsupported block alignment";
        return false;
    }

    output.sample_rate = source->nSamplesPerSec;
    output.channels = source->nChannels;
    output.bits_per_sample = bits;
    output.block_align = source->nBlockAlign;
    output.is_float = is_float;
    return true;
}

[[nodiscard]] float read_pcm_sample(const std::byte* source, const AudioFormat& format) noexcept {
    if (format.is_float) {
        float value = 0.0F;
        std::memcpy(&value, source, sizeof(value));
        return std::isfinite(value) ? value : 0.0F;
    }

    switch (format.bits_per_sample) {
    case 8: {
        const auto value = static_cast<unsigned char>(*source);
        return (static_cast<float>(value) - 128.0F) / 128.0F;
    }
    case 16: {
        std::int16_t value = 0;
        std::memcpy(&value, source, sizeof(value));
        return static_cast<float>(value) / 32768.0F;
    }
    case 24: {
        const auto* bytes = reinterpret_cast<const unsigned char*>(source);
        std::int32_t value = static_cast<std::int32_t>(bytes[0]) |
                             (static_cast<std::int32_t>(bytes[1]) << 8U) |
                             (static_cast<std::int32_t>(bytes[2]) << 16U);
        if ((value & 0x00800000) != 0) {
            value |= static_cast<std::int32_t>(0xff000000U);
        }
        return static_cast<float>(value) / 8388608.0F;
    }
    case 32: {
        std::int32_t value = 0;
        std::memcpy(&value, source, sizeof(value));
        return static_cast<float>(static_cast<double>(value) / 2147483648.0);
    }
    default:
        return 0.0F;
    }
}

void write_pcm_sample(std::byte* destination, const AudioFormat& format, float sample) noexcept {
    if (!std::isfinite(sample)) {
        sample = 0.0F;
    }
    if (format.is_float) {
        std::memcpy(destination, &sample, sizeof(sample));
        return;
    }

    const float bounded = std::clamp(sample, -1.0F, 1.0F);
    switch (format.bits_per_sample) {
    case 8: {
        const auto value = static_cast<unsigned char>(std::clamp(
            static_cast<long>(std::lround(bounded * 127.0F + 128.0F)), 0L, 255L));
        std::memcpy(destination, &value, sizeof(value));
        break;
    }
    case 16: {
        const auto value = static_cast<std::int16_t>(std::clamp(
            static_cast<long>(std::lround(bounded * 32768.0F)), -32768L, 32767L));
        std::memcpy(destination, &value, sizeof(value));
        break;
    }
    case 24: {
        const long long quantized = std::clamp(
            std::llround(static_cast<double>(bounded) * 8388608.0),
            -8388608LL, 8388607LL);
        const auto value = static_cast<std::int32_t>(quantized);
        auto* bytes = reinterpret_cast<unsigned char*>(destination);
        const auto unsigned_value = static_cast<std::uint32_t>(value);
        bytes[0] = static_cast<unsigned char>(unsigned_value & 0xffU);
        bytes[1] = static_cast<unsigned char>((unsigned_value >> 8U) & 0xffU);
        bytes[2] = static_cast<unsigned char>((unsigned_value >> 16U) & 0xffU);
        break;
    }
    case 32: {
        const auto value = static_cast<std::int32_t>(std::clamp(
            static_cast<double>(bounded) * 2147483648.0,
            -2147483648.0, 2147483647.0));
        std::memcpy(destination, &value, sizeof(value));
        break;
    }
    default:
        break;
    }
}

} // namespace

struct WasapiBackend::Impl final {
    WasapiConfig config{};
    WasapiProcessCallback callback = nullptr;
    void* callback_context = nullptr;
    WasapiRuntimeInfo runtime{};

    std::thread worker;
    HANDLE stop_event = nullptr;
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> running{false};
    std::atomic<std::uint64_t> capture_underrun_blocks{0};
    std::atomic<std::uint64_t> capture_overrun_frames{0};

    mutable std::mutex error_mutex;
    std::string error;
    std::mutex startup_mutex;
    std::condition_variable startup_condition;
    std::atomic<bool> startup_done{false};
    bool startup_success = false;

    std::vector<float> input_scratch;
    std::vector<float> output_scratch;
    std::vector<float> capture_ring;
    std::size_t ring_capacity_frames = 0;
    std::size_t ring_read_frame = 0;
    std::size_t ring_write_frame = 0;
    std::size_t ring_count_frames = 0;

    AudioFormat input_format{};
    AudioFormat output_format{};
    std::uint32_t input_buffer_frames = 0;
    std::uint32_t output_buffer_frames = 0;

    ~Impl() { stop(); }

    void set_error(std::string message) noexcept {
        try {
            std::lock_guard lock(error_mutex);
            error = std::move(message);
        } catch (...) {
        }
    }

    void clear_error() noexcept {
        std::lock_guard lock(error_mutex);
        error.clear();
    }

    [[nodiscard]] std::string error_message() const {
        std::lock_guard lock(error_mutex);
        return error;
    }

    void signal_startup(bool success) noexcept {
        {
            std::lock_guard lock(startup_mutex);
            startup_success = success;
            startup_done.store(true, std::memory_order_release);
        }
        startup_condition.notify_all();
    }

    [[nodiscard]] bool start(const WasapiConfig& requested,
                             WasapiProcessCallback process,
                             void* context) noexcept {
        stop();
        clear_error();
        if (process == nullptr) {
            set_error("WASAPI requires a processing callback");
            return false;
        }
        if (requested.max_block_frames < 16 || requested.max_block_frames > 4096) {
            set_error("WASAPI max_block_frames must be between 16 and 4096");
            return false;
        }

        config = requested;
        callback = process;
        callback_context = context;
        runtime = {};
        capture_underrun_blocks.store(0, std::memory_order_relaxed);
        capture_overrun_frames.store(0, std::memory_order_relaxed);
        stop_requested.store(false, std::memory_order_release);
        ring_read_frame = 0;
        ring_write_frame = 0;
        ring_count_frames = 0;

        {
            std::lock_guard lock(startup_mutex);
            startup_done.store(false, std::memory_order_release);
            startup_success = false;
        }

        if (stop_event == nullptr) {
            stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (stop_event == nullptr) {
                set_error(win32_message("CreateEventW(stop)", GetLastError()));
                return false;
            }
        } else {
            ResetEvent(stop_event);
        }

        try {
            const std::size_t max_frames = requested.max_block_frames;
            constexpr std::size_t max_channels = 8;
            ring_capacity_frames = std::max<std::size_t>(512, max_frames * 16);
            input_scratch.assign(max_frames * max_channels, 0.0F);
            output_scratch.assign(max_frames * max_channels, 0.0F);
            capture_ring.assign(ring_capacity_frames * max_channels, 0.0F);
            worker = std::thread([this] { run_worker(); });
        } catch (const std::exception& exception) {
            set_error(std::string("Could not prepare WASAPI buffers/thread: ") + exception.what());
            stop();
            return false;
        } catch (...) {
            set_error("Could not prepare WASAPI buffers/thread");
            stop();
            return false;
        }

        std::unique_lock lock(startup_mutex);
        const bool signaled = startup_condition.wait_for(lock, std::chrono::seconds(10),
                                                         [this] { return startup_done.load(std::memory_order_acquire); });
        const bool success = signaled && startup_success;
        lock.unlock();
        if (!success) {
            if (!signaled) {
                set_error("WASAPI startup timed out");
            }
            stop();
            return false;
        }
        return true;
    }

    void stop() noexcept {
        stop_requested.store(true, std::memory_order_release);
        if (stop_event != nullptr) {
            SetEvent(stop_event);
        }
        if (worker.joinable()) {
            worker.join();
        }
        running.store(false, std::memory_order_release);
        if (stop_event != nullptr) {
            CloseHandle(stop_event);
            stop_event = nullptr;
        }
    }

    void run_worker() noexcept {
        const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(com_result)) {
            set_error(hresult_message("CoInitializeEx", com_result));
            signal_startup(false);
            return;
        }
        const bool uninitialize_com = SUCCEEDED(com_result);

        try {
            std::string failure;
            if (!open_and_run(failure)) {
                set_error(std::move(failure));
                if (!startup_done.load(std::memory_order_acquire)) {
                    signal_startup(false);
                }
            }
        } catch (const std::exception& exception) {
            set_error(std::string("WASAPI worker failed: ") + exception.what());
            if (!startup_done.load(std::memory_order_acquire)) {
                signal_startup(false);
            }
        } catch (...) {
            set_error("WASAPI worker failed with an unknown error");
            if (!startup_done.load(std::memory_order_acquire)) {
                signal_startup(false);
            }
        }

        running.store(false, std::memory_order_release);
        if (uninitialize_com) {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool open_and_run(std::string& failure) {
        ComPtr<IMMDeviceEnumerator> enumerator;
        HRESULT result = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                          IID_IMMDeviceEnumerator,
                                          reinterpret_cast<void**>(enumerator.put()));
        if (FAILED(result)) {
            failure = hresult_message("CoCreateInstance(MMDeviceEnumerator)", result);
            return false;
        }

        ComPtr<IMMDevice> capture_device;
        result = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, capture_device.put());
        if (FAILED(result)) {
            failure = hresult_message("GetDefaultAudioEndpoint(capture)", result);
            return false;
        }
        ComPtr<IMMDevice> render_device;
        result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, render_device.put());
        if (FAILED(result)) {
            failure = hresult_message("GetDefaultAudioEndpoint(render)", result);
            return false;
        }

        ComPtr<IAudioClient> capture_audio;
        result = capture_device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr,
                                          reinterpret_cast<void**>(capture_audio.put()));
        if (FAILED(result)) {
            failure = hresult_message("Activate(capture IAudioClient)", result);
            return false;
        }
        ComPtr<IAudioClient> render_audio;
        result = render_device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr,
                                         reinterpret_cast<void**>(render_audio.put()));
        if (FAILED(result)) {
            failure = hresult_message("Activate(render IAudioClient)", result);
            return false;
        }

        WAVEFORMATEX* capture_mix_raw = nullptr;
        result = capture_audio->GetMixFormat(&capture_mix_raw);
        if (FAILED(result)) {
            failure = hresult_message("GetMixFormat(capture)", result);
            return false;
        }
        UniqueWaveFormat capture_mix(capture_mix_raw);
        WAVEFORMATEX* render_mix_raw = nullptr;
        result = render_audio->GetMixFormat(&render_mix_raw);
        if (FAILED(result)) {
            failure = hresult_message("GetMixFormat(render)", result);
            return false;
        }
        UniqueWaveFormat render_mix(render_mix_raw);

        std::string format_error;
        if (!describe_format(capture_mix.get(), input_format, format_error)) {
            failure = "Capture " + format_error;
            return false;
        }
        if (!describe_format(render_mix.get(), output_format, format_error)) {
            failure = "Render " + format_error;
            return false;
        }
        if (input_format.sample_rate != output_format.sample_rate) {
            failure = "Default WASAPI capture and render endpoints use different sample rates; "
                      "sample-rate conversion is not implemented yet";
            return false;
        }

        UniqueHandle capture_event(CreateEventW(nullptr, FALSE, FALSE, nullptr));
        if (!capture_event) {
            failure = win32_message("CreateEventW(capture)", GetLastError());
            return false;
        }
        UniqueHandle render_event(CreateEventW(nullptr, FALSE, FALSE, nullptr));
        if (!render_event) {
            failure = win32_message("CreateEventW(render)", GetLastError());
            return false;
        }

        constexpr DWORD event_flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        result = capture_audio->Initialize(AUDCLNT_SHAREMODE_SHARED, event_flags,
                                          0, 0, capture_mix.get(), nullptr);
        if (FAILED(result)) {
            failure = hresult_message("Initialize(shared capture)", result);
            return false;
        }
        result = render_audio->Initialize(AUDCLNT_SHAREMODE_SHARED, event_flags,
                                          0, 0, render_mix.get(), nullptr);
        if (FAILED(result)) {
            failure = hresult_message("Initialize(shared render)", result);
            return false;
        }
        result = capture_audio->SetEventHandle(capture_event.get());
        if (FAILED(result)) {
            failure = hresult_message("SetEventHandle(capture)", result);
            return false;
        }
        result = render_audio->SetEventHandle(render_event.get());
        if (FAILED(result)) {
            failure = hresult_message("SetEventHandle(render)", result);
            return false;
        }

        ComPtr<IAudioCaptureClient> capture_client;
        result = capture_audio->GetService(IID_IAudioCaptureClient,
                                           reinterpret_cast<void**>(capture_client.put()));
        if (FAILED(result)) {
            failure = hresult_message("GetService(IAudioCaptureClient)", result);
            return false;
        }
        ComPtr<IAudioRenderClient> render_client;
        result = render_audio->GetService(IID_IAudioRenderClient,
                                          reinterpret_cast<void**>(render_client.put()));
        if (FAILED(result)) {
            failure = hresult_message("GetService(IAudioRenderClient)", result);
            return false;
        }

        result = capture_audio->GetBufferSize(&input_buffer_frames);
        if (FAILED(result) || input_buffer_frames == 0) {
            failure = FAILED(result) ? hresult_message("GetBufferSize(capture)", result)
                                     : "Capture endpoint returned a zero-sized buffer";
            return false;
        }
        result = render_audio->GetBufferSize(&output_buffer_frames);
        if (FAILED(result) || output_buffer_frames == 0) {
            failure = FAILED(result) ? hresult_message("GetBufferSize(render)", result)
                                     : "Render endpoint returned a zero-sized buffer";
            return false;
        }

        runtime.sample_rate_hz = input_format.sample_rate;
        runtime.max_block_frames = config.max_block_frames;
        runtime.input_channels = input_format.channels;
        runtime.output_channels = output_format.channels;

        HANDLE task_handle = nullptr;
        DWORD task_index = 0;
        task_handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

        result = render_audio->Start();
        if (FAILED(result)) {
            if (task_handle != nullptr) {
                AvRevertMmThreadCharacteristics(task_handle);
            }
            failure = hresult_message("Start(render)", result);
            return false;
        }
        result = capture_audio->Start();
        if (FAILED(result)) {
            render_audio->Stop();
            if (task_handle != nullptr) {
                AvRevertMmThreadCharacteristics(task_handle);
            }
            failure = hresult_message("Start(capture)", result);
            return false;
        }

        running.store(true, std::memory_order_release);
        signal_startup(true);

        const HANDLE events[3]{stop_event, capture_event.get(), render_event.get()};
        bool keep_running = true;
        while (keep_running && !stop_requested.load(std::memory_order_acquire)) {
            const DWORD wait_result = WaitForMultipleObjects(3, events, FALSE, 1000);
            if (wait_result == WAIT_OBJECT_0) {
                break;
            }
            if (wait_result == WAIT_TIMEOUT) {
                failure = "WASAPI event wait timed out; the audio endpoint may have disconnected";
                keep_running = false;
                break;
            }
            if (wait_result == WAIT_FAILED) {
                failure = win32_message("WaitForMultipleObjects", GetLastError());
                keep_running = false;
                break;
            }
            if (wait_result < WAIT_OBJECT_0 || wait_result >= WAIT_OBJECT_0 + 3) {
                failure = "WASAPI event wait returned an unexpected result";
                keep_running = false;
                break;
            }

            const DWORD event_index = wait_result - WAIT_OBJECT_0;
            if (event_index == 1 && !capture_available(capture_client.get(), failure)) {
                keep_running = false;
            } else if (event_index == 2 && !render_available(render_audio.get(), render_client.get(), failure)) {
                keep_running = false;
            }
        }

        capture_audio->Stop();
        render_audio->Stop();
        if (task_handle != nullptr) {
            AvRevertMmThreadCharacteristics(task_handle);
        }
        if (!failure.empty() && !stop_requested.load(std::memory_order_acquire)) {
            return false;
        }
        return true;
    }

    [[nodiscard]] bool capture_available(IAudioCaptureClient* client, std::string& failure) {
        UINT32 packet_frames = 0;
        HRESULT result = client->GetNextPacketSize(&packet_frames);
        if (FAILED(result)) {
            failure = hresult_message("GetNextPacketSize", result);
            return false;
        }
        while (packet_frames > 0) {
            BYTE* raw_data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            result = client->GetBuffer(&raw_data, &frames, &flags, nullptr, nullptr);
            if (FAILED(result)) {
                failure = hresult_message("GetBuffer(capture)", result);
                return false;
            }
            if (frames > input_buffer_frames || (raw_data == nullptr && (flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0)) {
                client->ReleaseBuffer(frames);
                failure = "Capture endpoint returned an invalid packet";
                return false;
            }

            const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
            const auto* bytes = reinterpret_cast<const std::byte*>(raw_data);
            const std::size_t bytes_per_sample = input_format.bits_per_sample / 8U;
            for (UINT32 frame = 0; frame < frames; ++frame) {
                if (ring_count_frames == ring_capacity_frames) {
                    ring_read_frame = (ring_read_frame + 1) % ring_capacity_frames;
                    --ring_count_frames;
                    capture_overrun_frames.fetch_add(1, std::memory_order_relaxed);
                }
                const std::size_t ring_offset = ring_write_frame * input_format.channels;
                for (std::uint16_t channel = 0; channel < input_format.channels; ++channel) {
                    const std::size_t sample_index = static_cast<std::size_t>(frame) * input_format.channels + channel;
                    capture_ring[ring_offset + channel] = silent
                        ? 0.0F
                        : read_pcm_sample(bytes + sample_index * bytes_per_sample, input_format);
                }
                ring_write_frame = (ring_write_frame + 1) % ring_capacity_frames;
                ++ring_count_frames;
            }

            result = client->ReleaseBuffer(frames);
            if (FAILED(result)) {
                failure = hresult_message("ReleaseBuffer(capture)", result);
                return false;
            }
            result = client->GetNextPacketSize(&packet_frames);
            if (FAILED(result)) {
                failure = hresult_message("GetNextPacketSize", result);
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool render_available(IAudioClient* audio,
                                        IAudioRenderClient* client,
                                        std::string& failure) {
        UINT32 padding = 0;
        HRESULT result = audio->GetCurrentPadding(&padding);
        if (FAILED(result)) {
            failure = hresult_message("GetCurrentPadding", result);
            return false;
        }
        if (padding > output_buffer_frames) {
            failure = "Render endpoint reported padding larger than its buffer";
            return false;
        }
        const UINT32 available = output_buffer_frames - padding;
        if (available == 0) {
            return true;
        }

        BYTE* raw_data = nullptr;
        result = client->GetBuffer(available, &raw_data);
        if (FAILED(result) || raw_data == nullptr) {
            failure = FAILED(result) ? hresult_message("GetBuffer(render)", result)
                                     : "Render endpoint returned a null buffer";
            return false;
        }

        auto* bytes = reinterpret_cast<std::byte*>(raw_data);
        const std::size_t max_frames = config.max_block_frames;
        const std::size_t output_bytes_per_sample = output_format.bits_per_sample / 8U;
        UINT32 offset = 0;
        bool callback_underflow = false;
        while (offset < available) {
            const std::uint32_t frames = static_cast<std::uint32_t>(
                std::min<std::size_t>(max_frames, available - offset));
            const std::size_t input_sample_count = static_cast<std::size_t>(frames) * input_format.channels;
            const std::size_t output_sample_count = static_cast<std::size_t>(frames) * output_format.channels;
            std::fill_n(input_scratch.data(), input_sample_count, 0.0F);
            std::fill_n(output_scratch.data(), output_sample_count, 0.0F);

            for (std::uint32_t frame = 0; frame < frames; ++frame) {
                if (ring_count_frames == 0) {
                    callback_underflow = true;
                    continue;
                }
                const std::size_t ring_offset = ring_read_frame * input_format.channels;
                const std::size_t scratch_offset = static_cast<std::size_t>(frame) * input_format.channels;
                std::copy_n(capture_ring.data() + ring_offset, input_format.channels,
                            input_scratch.data() + scratch_offset);
                ring_read_frame = (ring_read_frame + 1) % ring_capacity_frames;
                --ring_count_frames;
            }
            callback(callback_context, input_scratch.data(), input_format.channels,
                     output_scratch.data(), output_format.channels, frames);

            const std::size_t output_frame_offset = static_cast<std::size_t>(offset);
            for (std::uint32_t frame = 0; frame < frames; ++frame) {
                const std::size_t scratch_offset = static_cast<std::size_t>(frame) * output_format.channels;
                const std::size_t device_frame_offset = output_frame_offset + frame;
                for (std::uint16_t channel = 0; channel < output_format.channels; ++channel) {
                    const std::size_t sample_index = scratch_offset + channel;
                    write_pcm_sample(bytes + device_frame_offset * output_format.block_align +
                                         static_cast<std::size_t>(channel) * output_bytes_per_sample,
                                     output_format, output_scratch[sample_index]);
                }
            }
            offset += frames;
        }
        if (callback_underflow) {
            capture_underrun_blocks.fetch_add(1, std::memory_order_relaxed);
        }

        result = client->ReleaseBuffer(available, 0);
        if (FAILED(result)) {
            failure = hresult_message("ReleaseBuffer(render)", result);
            return false;
        }
        return true;
    }
};

WasapiBackend::WasapiBackend() : impl_(std::make_unique<Impl>()) {}

WasapiBackend::~WasapiBackend() = default;

bool WasapiBackend::start(const WasapiConfig& config,
                          WasapiProcessCallback callback,
                          void* context) noexcept {
    return impl_->start(config, callback, context);
}

void WasapiBackend::stop() noexcept {
    if (impl_) {
        impl_->stop();
    }
}

bool WasapiBackend::running() const noexcept {
    return impl_->running.load(std::memory_order_acquire);
}

WasapiRuntimeInfo WasapiBackend::runtime_info() const noexcept {
    return impl_->runtime;
}

WasapiCounters WasapiBackend::counters() const noexcept {
    return {
        impl_->capture_underrun_blocks.load(std::memory_order_relaxed),
        impl_->capture_overrun_frames.load(std::memory_order_relaxed),
    };
}

std::string WasapiBackend::error_message() const {
    return impl_->error_message();
}

} // namespace tabletfx::audio
