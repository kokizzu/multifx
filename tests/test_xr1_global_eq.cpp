#include "tabletfx/effects/xr1_global_eq.hpp"

#include <algorithm>
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
#include <utility>
#include <vector>

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};
constexpr float kSampleRate = 48000.0F;
constexpr double kPi = 3.14159265358979323846;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> sine(float frequency_hz, std::size_t frames,
                       std::uint16_t channels = 1) {
    std::vector<float> audio(frames * channels, 0.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float sample = static_cast<float>(
            0.2 * std::sin(2.0 * kPi * frequency_hz * frame / kSampleRate));
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            audio[frame * channels + channel] = sample;
        }
    }
    return audio;
}

double rms(std::span<const float> audio, std::uint16_t channels,
           std::uint16_t channel = 0) {
    const std::size_t frames = audio.size() / channels;
    double energy = 0.0;
    for (std::size_t frame = frames / 2; frame < frames; ++frame) {
        const double sample = audio[frame * channels + channel];
        energy += sample * sample;
    }
    const std::size_t count = frames - frames / 2;
    return count == 0 ? 0.0 : std::sqrt(energy / static_cast<double>(count));
}

tabletfx::XR1GlobalEQParameters parameters_for(
    tabletfx::XR1GlobalEQType type, float gain_db, float frequency_hz,
    float q = 0.707F) {
    tabletfx::XR1GlobalEQParameters parameters;
    parameters.bands[2].type = type;
    parameters.bands[2].gain_db = gain_db;
    parameters.bands[2].frequency_hz = frequency_hz;
    parameters.bands[2].q = q;
    return parameters;
}

void test_default_identity_and_reported_memory() {
    tabletfx::XR1GlobalEQ eq;
    require(eq.id() == "xr1-global-eq-project-authored",
            "effect ID changed");
    require(eq.memory_bytes() == sizeof(eq),
            "memory report must include fixed processor state");
    const auto defaults = eq.parameters();
    require(!defaults.bypass && defaults.bands.size() == 5,
            "unexpected Global EQ defaults");
    for (const auto& band : defaults.bands) {
        require(band.assign == tabletfx::XR1GlobalEQAssign::Stereo &&
                    band.type == tabletfx::XR1GlobalEQType::Peak &&
                    band.gain_db == 0.0F && !band.bypass,
                "default bands should be neutral stereo peaks");
    }
    std::array<float, 12> audio{
        0.1F, -0.2F, 0.3F, -0.4F, 1.0F, -1.0F,
        0.7F, 0.2F, -0.5F, 0.8F, 0.0F, 0.1F,
    };
    const auto original = audio;
    eq.process(audio, 2);
    require(audio == original, "neutral Global EQ must be exact passthrough");
}

void test_project_bounds_and_invalid_enums() {
    tabletfx::XR1GlobalEQ eq;
    auto parameters = eq.parameters();
    parameters.bands[0].gain_db = 100.0F;
    parameters.bands[0].frequency_hz = 50000.0F;
    parameters.bands[0].q = -1.0F;
    parameters.bands[0].assign =
        static_cast<tabletfx::XR1GlobalEQAssign>(255U);
    parameters.bands[0].type =
        static_cast<tabletfx::XR1GlobalEQType>(255U);
    parameters.bands[1].gain_db = -100.0F;
    parameters.bands[1].frequency_hz =
        std::numeric_limits<float>::quiet_NaN();
    parameters.bands[1].q = std::numeric_limits<float>::infinity();
    eq.configure(parameters);

    const auto actual = eq.parameters();
    require(actual.bands[0].gain_db == tabletfx::XR1GlobalEQ::kMaximumGainDb &&
                actual.bands[0].frequency_hz ==
                    tabletfx::XR1GlobalEQ::kMaximumFrequencyHz &&
                actual.bands[0].q == tabletfx::XR1GlobalEQ::kMinimumQ,
            "finite out-of-range parameters were not clamped");
    require(actual.bands[0].assign == tabletfx::XR1GlobalEQAssign::Stereo &&
                actual.bands[0].type == tabletfx::XR1GlobalEQType::Peak,
            "invalid enum values should select safe defaults");
    require(actual.bands[1].gain_db == tabletfx::XR1GlobalEQ::kMinimumGainDb &&
                actual.bands[1].frequency_hz == 250.0F &&
                actual.bands[1].q == 0.707F,
            "non-finite parameters should return to project defaults");

    tabletfx::XR1GlobalEQ low_rate(8000.0F, 1);
    auto low_rate_parameters = low_rate.parameters();
    low_rate_parameters.bands[0].frequency_hz = 20000.0F;
    low_rate.configure(low_rate_parameters);
    require(low_rate.parameters().bands[0].frequency_hz == 3600.0F,
            "frequency ceiling must remain below Nyquist");
}

void test_peak_and_filter_types() {
    constexpr std::size_t frames = 48000;
    auto peak_audio = sine(750.0F, frames);
    const double peak_input = rms(peak_audio, 1);
    tabletfx::XR1GlobalEQ peak(kSampleRate, 1);
    peak.configure(parameters_for(tabletfx::XR1GlobalEQType::Peak, 6.0F,
                                  750.0F));
    peak.process(peak_audio, 1);
    require(rms(peak_audio, 1) > peak_input * 1.75 &&
                rms(peak_audio, 1) < peak_input * 2.25,
            "Peak band did not boost near its frequency");

    auto low_tone = sine(100.0F, frames);
    auto high_tone = sine(1000.0F, frames);
    tabletfx::XR1GlobalEQ high_pass(kSampleRate, 1);
    high_pass.configure(parameters_for(tabletfx::XR1GlobalEQType::HighPass,
                                       0.0F, 500.0F));
    high_pass.process(low_tone, 1);
    high_pass.process(high_tone, 1);
    require(rms(low_tone, 1) < rms(high_tone, 1) * 0.35,
            "High-pass band did not attenuate low frequencies");

    auto pass_low = sine(500.0F, frames);
    auto pass_high = sine(5000.0F, frames);
    tabletfx::XR1GlobalEQ low_pass(kSampleRate, 1);
    low_pass.configure(parameters_for(tabletfx::XR1GlobalEQType::LowPass,
                                      0.0F, 1500.0F));
    low_pass.process(pass_low, 1);
    low_pass.process(pass_high, 1);
    require(rms(pass_high, 1) < rms(pass_low, 1) * 0.35,
            "Low-pass band did not attenuate high frequencies");

    auto low_shelf_low = sine(100.0F, frames);
    auto low_shelf_high = sine(5000.0F, frames);
    tabletfx::XR1GlobalEQ low_shelf(kSampleRate, 1);
    low_shelf.configure(parameters_for(tabletfx::XR1GlobalEQType::LowShelf,
                                       6.0F, 500.0F));
    low_shelf.process(low_shelf_low, 1);
    low_shelf.process(low_shelf_high, 1);
    require(rms(low_shelf_low, 1) > rms(low_shelf_high, 1) * 1.45,
            "Low shelf did not distinguish its low-frequency region");

    auto high_shelf_low = sine(100.0F, frames);
    auto high_shelf_high = sine(5000.0F, frames);
    tabletfx::XR1GlobalEQ high_shelf(kSampleRate, 1);
    high_shelf.configure(parameters_for(tabletfx::XR1GlobalEQType::HighShelf,
                                        6.0F, 1000.0F));
    high_shelf.process(high_shelf_low, 1);
    high_shelf.process(high_shelf_high, 1);
    require(rms(high_shelf_high, 1) > rms(high_shelf_low, 1) * 1.45,
            "High shelf did not distinguish its high-frequency region");
}

void test_assignment_band_bypass_and_master_bypass() {
    auto parameters = parameters_for(tabletfx::XR1GlobalEQType::Peak,
                                     10.0F, 750.0F);
    parameters.bands[2].assign = tabletfx::XR1GlobalEQAssign::Left;
    auto stereo = sine(750.0F, 12000, 2);
    const auto original = stereo;
    tabletfx::XR1GlobalEQ eq(kSampleRate, 2);
    eq.configure(parameters);
    eq.process(stereo, 2);
    require(rms(stereo, 2, 0) > rms(original, 2, 0) * 2.2,
            "Left-assigned band did not process the left channel");
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        require(stereo[frame * 2 + 1] == original[frame * 2 + 1],
                "Left assignment changed the right channel");
    }

    parameters.bands[2].assign = tabletfx::XR1GlobalEQAssign::Right;
    auto right = original;
    eq.configure(parameters);
    eq.process(right, 2);
    require(rms(right, 2, 1) > rms(original, 2, 1) * 2.2,
            "Right-assigned band did not process the right channel");
    for (std::size_t frame = 0; frame < right.size() / 2; ++frame) {
        require(right[frame * 2] == original[frame * 2],
                "Right assignment changed the left channel");
    }

    parameters.bands[2].assign = tabletfx::XR1GlobalEQAssign::Stereo;
    parameters.bands[2].bypass = true;
    auto band_bypassed = original;
    eq.configure(parameters);
    eq.process(band_bypassed, 2);
    require(band_bypassed == original, "band bypass should be transparent");

    parameters.bands[2].bypass = false;
    parameters.bypass = true;
    auto master_bypassed = original;
    eq.configure(parameters);
    eq.process(master_bypassed, 2);
    require(master_bypassed == original, "master bypass should be transparent");

    eq.reset_parameters();
    const auto reset = eq.parameters();
    require(!reset.bypass && reset.bands[2].gain_db == 0.0F &&
                reset.bands[2].type == tabletfx::XR1GlobalEQType::Peak,
            "Reset should restore project-authored neutral defaults");
    auto after_reset = original;
    eq.process(after_reset, 2);
    require(after_reset == original, "Reset did not restore neutral processing");

    auto right_only_mono_parameters = parameters_for(
        tabletfx::XR1GlobalEQType::Peak, 10.0F, 750.0F);
    right_only_mono_parameters.bands[2].assign =
        tabletfx::XR1GlobalEQAssign::Right;
    auto mono = sine(750.0F, 12000);
    const auto mono_original = mono;
    tabletfx::XR1GlobalEQ mono_eq(kSampleRate, 1);
    mono_eq.configure(right_only_mono_parameters);
    mono_eq.process(mono, 1);
    require(mono == mono_original,
            "Right assignment should not remap to mono's left channel");
}

void test_zero_fixed_latency_finite_recovery_and_block_partitioning() {
    tabletfx::XR1GlobalEQ eq(kSampleRate, 1);
    eq.configure(parameters_for(tabletfx::XR1GlobalEQType::HighPass, 0.0F,
                               300.0F));
    std::array<float, 8> impulse{};
    impulse[0] = 1.0F;
    eq.process(impulse, 1);
    require(std::abs(impulse[0]) > 1.0e-5F,
            "IIR Global EQ must not add fixed block latency");

    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(), 0.2F,
        std::numeric_limits<float>::infinity(), -0.2F,
        0.1F, -0.1F, 0.05F, 0.0F,
    };
    eq.process(invalid, 1);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float value) { return std::isfinite(value); }),
            "non-finite input/output should not poison IIR state");

    constexpr std::size_t frames = 4097;
    auto source = sine(750.0F, frames, 2);
    auto whole = source;
    auto chunked = source;
    tabletfx::XR1GlobalEQ whole_eq(kSampleRate, 2);
    tabletfx::XR1GlobalEQ chunked_eq(kSampleRate, 2);
    auto configuration = parameters_for(tabletfx::XR1GlobalEQType::LowShelf,
                                        4.0F, 350.0F, 1.1F);
    configuration.bands[4].type = tabletfx::XR1GlobalEQType::HighPass;
    configuration.bands[4].frequency_hz = 10000.0F;
    configuration.bands[4].assign = tabletfx::XR1GlobalEQAssign::Right;
    whole_eq.configure(configuration);
    chunked_eq.configure(configuration);
    whole_eq.process(whole, 2);

    constexpr std::array<std::size_t, 6> chunks{1, 63, 257, 128, 509, 17};
    std::size_t frame = 0;
    std::size_t block = 0;
    while (frame < frames) {
        const std::size_t count = std::min(chunks[block % chunks.size()],
                                           frames - frame);
        chunked_eq.process(
            std::span<float>(chunked.data() + frame * 2, count * 2), 2);
        frame += count;
        ++block;
    }
    for (std::size_t sample = 0; sample < whole.size(); ++sample) {
        require(std::abs(whole[sample] - chunked[sample]) < 1.0e-6F,
                "irregular processing blocks changed IIR output");
    }
}

void test_no_process_allocations() {
    tabletfx::XR1GlobalEQ eq(kSampleRate, 2);
    eq.configure(parameters_for(tabletfx::XR1GlobalEQType::HighShelf, 5.0F,
                                4000.0F));
    std::array<float, 1024> audio{};
    for (std::size_t sample = 0; sample < audio.size(); ++sample) {
        audio[sample] = static_cast<float>(std::sin(sample * 0.07) * 0.25);
    }
    allocation_count.store(0);
    count_allocations.store(true);
    eq.process(audio, 2);
    count_allocations.store(false);
    require(allocation_count.load() == 0,
            "audio processing must not allocate");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load()) allocation_count.fetch_add(1);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load()) allocation_count.fetch_add(1);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_default_identity_and_reported_memory();
        test_project_bounds_and_invalid_enums();
        test_peak_and_filter_types();
        test_assignment_band_bypass_and_master_bypass();
        test_zero_fixed_latency_finite_recovery_and_block_partitioning();
        test_no_process_allocations();
        std::cout << "XR1 Global EQ tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        count_allocations.store(false);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
