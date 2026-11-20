#include "tabletfx/audio/wasapi_backend.hpp"

int main() {
    tabletfx::audio::WasapiBackend backend;
    const auto runtime = backend.runtime_info();
    const auto counters = backend.counters();
    return runtime.sample_rate_hz == 0 && counters.capture_underrun_blocks == 0 ? 0 : 1;
}
