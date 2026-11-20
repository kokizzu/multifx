#include "tabletfx/effects/pedal_compressor.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<float> render_constant(tabletfx::PedalCompressorParameters parameters,
                                   float level, std::size_t frames = 48000) {
    tabletfx::PedalCompressor compressor(48000.0F, parameters);
    std::vector<float> samples(frames, level);
    compressor.process(samples, 1);
    return samples;
}

void test_defaults_and_parameter_bounds() {
    tabletfx::PedalCompressor compressor;
    const auto defaults = compressor.parameters();
    require(defaults.sensitivity == 0.45F && defaults.sustain == 0.45F &&
            defaults.attack == 0.25F && defaults.level_db == 0.0F,
            "sane pedal compressor defaults changed unexpectedly");
    require(compressor.memory_bytes() >= sizeof(void*),
            "pedal compressor did not report its instance memory");

    tabletfx::PedalCompressorParameters extreme;
    extreme.sensitivity = 5.0F;
    extreme.sustain = -3.0F;
    extreme.attack = 2.0F;
    extreme.level_db = 100.0F;
    compressor.set_parameters(extreme);
    const auto bounded = compressor.parameters();
    require(bounded.sensitivity == 1.0F && bounded.sustain == 0.0F &&
            bounded.attack == 1.0F && bounded.level_db == 12.0F,
            "out-of-range pedal controls were not bounded");

    extreme.sensitivity = std::numeric_limits<float>::quiet_NaN();
    extreme.sustain = std::numeric_limits<float>::infinity();
    extreme.attack = 0.0F;
    extreme.level_db = std::numeric_limits<float>::quiet_NaN();
    compressor.configure(0.0F, extreme);
    const auto recovered = compressor.parameters();
    require(recovered.sensitivity == 0.45F && recovered.sustain == 0.45F &&
            recovered.level_db == 0.0F,
            "non-finite pedal controls did not fall back to safe defaults");
}

void test_sensitivity_and_sustain_increase_compression() {
    tabletfx::PedalCompressorParameters low_sensitivity;
    low_sensitivity.sensitivity = 0.1F;
    low_sensitivity.sustain = 0.8F;
    low_sensitivity.attack = 0.1F;
    tabletfx::PedalCompressorParameters high_sensitivity = low_sensitivity;
    high_sensitivity.sensitivity = 0.9F;
    const auto low_sensitivity_audio = render_constant(low_sensitivity, 0.5F);
    const auto high_sensitivity_audio = render_constant(high_sensitivity, 0.5F);
    require(high_sensitivity_audio.back() < low_sensitivity_audio.back() * 0.6F,
            "higher sensitivity did not lower the detector threshold");

    tabletfx::PedalCompressorParameters no_sustain = high_sensitivity;
    no_sustain.sustain = 0.0F;
    tabletfx::PedalCompressorParameters more_sustain = high_sensitivity;
    more_sustain.sustain = 1.0F;
    const auto no_sustain_audio = render_constant(no_sustain, 0.5F);
    const auto more_sustain_audio = render_constant(more_sustain, 0.5F);
    require(more_sustain_audio.back() < no_sustain_audio.back() * 0.4F,
            "higher sustain did not increase compression");
}

void test_attack_and_level_controls() {
    tabletfx::PedalCompressorParameters fast;
    fast.sensitivity = 0.9F;
    fast.sustain = 1.0F;
    fast.attack = 0.0F;
    tabletfx::PedalCompressorParameters slow = fast;
    slow.attack = 1.0F;
    const auto fast_audio = render_constant(fast, 0.8F, 12000);
    const auto slow_audio = render_constant(slow, 0.8F, 12000);
    const std::size_t at_five_ms = 240;
    require(slow_audio[at_five_ms] > fast_audio[at_five_ms] * 1.5F,
            "slow attack did not preserve more of the initial transient");

    tabletfx::PedalCompressorParameters unity;
    unity.sensitivity = 0.0F;
    unity.sustain = 1.0F;
    unity.attack = 0.0F;
    const auto unity_level = render_constant(unity, 0.01F);
    unity.level_db = 6.0F;
    const auto boosted_level = render_constant(unity, 0.01F);
    require(std::abs(boosted_level.back() / unity_level.back() -
                     std::pow(10.0F, 6.0F / 20.0F)) < 2.0e-3F,
            "level control did not apply the requested makeup gain");
}

void test_stereo_link_reset_and_nonfinite_audio() {
    tabletfx::PedalCompressorParameters parameters;
    parameters.sensitivity = 0.9F;
    parameters.sustain = 0.9F;
    parameters.attack = 0.0F;
    tabletfx::PedalCompressor compressor(48000.0F, parameters);
    std::vector<float> stereo(48000 * 2, 0.0F);
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        stereo[frame * 2] = 0.2F;
        stereo[frame * 2 + 1] = 0.8F;
    }
    compressor.process(stereo, 2);
    const float left_gain = stereo[stereo.size() - 2] / 0.2F;
    const float right_gain = stereo.back() / 0.8F;
    require(left_gain < 0.9F && std::abs(left_gain - right_gain) < 1.0e-5F,
            "stereo channels did not share detector gain reduction");

    compressor.reset();
    std::array<float, 2> quiet{0.01F, 0.01F};
    compressor.process(quiet, 1);
    require(std::abs(quiet.back() - 0.01F) < 1.0e-5F,
            "reset did not clear compressor envelope and gain state");

    std::array<float, 4> bad_audio{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max()};
    compressor.process(bad_audio, 1);
    for (float sample : bad_audio) {
        require(std::isfinite(sample), "non-finite signal escaped the pedal compressor");
    }
}

void test_irregular_blocks_and_no_processing_allocations() {
    tabletfx::PedalCompressor compressor(44100.0F);
    std::array<float, 1> mono_one{0.1F};
    std::array<float, 9> mono_irregular{0.1F, -0.2F, 0.3F, -0.4F, 0.5F,
                                         -0.6F, 0.7F, -0.8F, 0.9F};
    std::array<float, 256 * 2> stereo{};
    stereo.fill(0.2F);
    compressor.process(mono_one, 1);
    compressor.process(mono_irregular, 1);
    compressor.process(stereo, 2);

    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        compressor.process(mono_one, 1);
        compressor.process(mono_irregular, 1);
        compressor.process(stereo, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "pedal compressor allocated memory during process()");
    for (float sample : stereo) {
        require(std::isfinite(sample), "pedal compressor emitted a non-finite sample");
    }
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_and_parameter_bounds();
        test_sensitivity_and_sustain_increase_compression();
        test_attack_and_level_controls();
        test_stereo_link_reset_and_nonfinite_audio();
        test_irregular_blocks_and_no_processing_allocations();
        std::cout << "All TabletFX pedal compressor tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
