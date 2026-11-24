#include "tabletfx/effects/eq5_graphic.hpp"

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
                       std::uint16_t channels = 1,
                       std::uint16_t active_channel = 0) {
    std::vector<float> audio(frames * channels, 0.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        audio[frame * channels + active_channel] = static_cast<float>(
            0.2 * std::sin(2.0 * kPi * frequency_hz * frame / kSampleRate));
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

void test_identity_fixed_centers_and_project_authored_bounds() {
    tabletfx::EQ5Graphic eq;
    constexpr std::array<float, 5> expected{80.0F, 240.0F, 750.0F,
                                            2200.0F, 6600.0F};
    require(eq.id() == "eq5-graphic", "EQ5 Graphic ID changed");
    require(eq.memory_bytes() == sizeof(eq),
            "fixed EQ memory report is inaccurate");
    for (std::size_t band = 0; band < expected.size(); ++band) {
        require(eq.band_frequency_hz(band) == expected[band],
                "fixed EQ center frequency changed");
        require(eq.band_gain_db(band) == 0.0F,
                "fixed EQ default band gain should be neutral");
    }
    require(eq.band_frequency_hz(expected.size()) == 0.0F,
            "invalid band should report no center frequency");
    require(!eq.set_band_gain_db(expected.size(), 3.0F),
            "invalid band index should be rejected");

    require(eq.set_band_gain_db(0, 100.0F) &&
                eq.band_gain_db(0) == 12.0F,
            "band boost did not clamp to the project maximum");
    require(eq.set_band_gain_db(1, -100.0F) &&
                eq.band_gain_db(1) == -12.0F,
            "band cut did not clamp to the project minimum");
    require(eq.set_band_gain_db(2, std::numeric_limits<float>::quiet_NaN()) &&
                eq.band_gain_db(2) == 0.0F,
            "non-finite band gain should return to 0 dB");
    eq.set_overall_gain_db(100.0F);
    require(eq.overall_gain_db() == 12.0F,
            "overall gain did not clamp to the project maximum");
    eq.set_overall_gain_db(-100.0F);
    require(eq.overall_gain_db() == -12.0F,
            "overall gain did not clamp to the project minimum");
    eq.set_overall_gain_db(std::numeric_limits<float>::infinity());
    require(eq.overall_gain_db() == 0.0F,
            "non-finite overall gain should return to 0 dB");
}

void test_each_band_boosts_its_fixed_center() {
    constexpr std::size_t frames = 48000;
    for (std::size_t band = 0; band < tabletfx::EQ5Graphic::kBandCount; ++band) {
        const float center = tabletfx::EQ5Graphic::kCenterFrequenciesHz[band];
        auto audio = sine(center, frames);
        const double input_level = rms(audio, 1);
        tabletfx::EQ5Graphic eq(kSampleRate, 1);
        require(eq.set_band_gain_db(band, 6.0F),
                "valid fixed-center band was rejected");
        eq.process(audio, 1);
        const double output_level = rms(audio, 1);
        require(output_level > input_level * 1.85 &&
                    output_level < input_level * 2.15,
                "6 dB boost did not act at the documented center frequency");
    }
}

void test_overall_gain_and_zero_fixed_latency() {
    tabletfx::EQ5Graphic neutral(kSampleRate, 1);
    std::array<float, 16> impulse{};
    impulse[0] = 0.5F;
    const auto original = impulse;
    neutral.process(impulse, 1);
    require(impulse == original,
            "neutral EQ must be exact zero-latency passthrough");

    tabletfx::EQ5Graphic gained(kSampleRate, 1);
    gained.set_overall_gain_db(6.0F);
    auto tone = sine(1000.0F, 48000);
    const double input_level = rms(tone, 1);
    gained.process(tone, 1);
    require(rms(tone, 1) > input_level * 1.95 &&
                rms(tone, 1) < input_level * 2.05,
            "overall Gain did not apply its project-authored value");

    tabletfx::EQ5Graphic filtered(kSampleRate, 1);
    filtered.set_band_gain_db(0, 6.0F);
    std::array<float, 8> filtered_impulse{};
    filtered_impulse[0] = 1.0F;
    filtered.process(filtered_impulse, 1);
    require(std::abs(filtered_impulse[0]) > 1.0e-5F,
            "peaking EQ must not introduce fixed buffering latency");
}

void test_mono_stereo_channel_isolation_and_layout_mismatch() {
    tabletfx::EQ5Graphic stereo_eq(kSampleRate, 2);
    require(stereo_eq.set_band_gain_db(1, 9.0F),
            "stereo test band setup failed");
    auto stereo = sine(240.0F, 12000, 2, 0);
    stereo_eq.process(stereo, 2);
    require(rms(stereo, 2, 0) > 0.3,
            "active left channel was not processed");
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        require(stereo[frame * 2 + 1] == 0.0F,
                "left-channel signal leaked into the right channel");
    }

    tabletfx::EQ5Graphic mono_eq(kSampleRate, 1);
    auto mono = sine(240.0F, 12000);
    require(mono_eq.set_band_gain_db(1, 9.0F),
            "mono test band setup failed");
    mono_eq.process(mono, 1);
    require(mono.size() == stereo.size() / 2,
            "mono/stereo comparison has mismatched lengths");
    for (std::size_t frame = 0; frame < mono.size(); ++frame) {
        require(std::abs(mono[frame] - stereo[frame * 2]) < 1.0e-6F,
                "mono and isolated stereo channel responses differ");
    }

    std::array<float, 4> unchanged{0.1F, -0.2F, 0.3F, -0.4F};
    const auto original = unchanged;
    stereo_eq.process(unchanged, 1);
    require(unchanged == original,
            "channel mismatch should leave the input buffer unchanged");
}

void test_irregular_blocks_and_reset_are_deterministic() {
    constexpr std::size_t frames = 4097;
    auto source = sine(750.0F, frames, 2, 1);
    auto whole = source;
    auto chunked = source;
    tabletfx::EQ5Graphic whole_eq(kSampleRate, 2);
    tabletfx::EQ5Graphic chunked_eq(kSampleRate, 2);
    for (const auto [band, gain] : std::array<std::pair<std::size_t, float>, 3>{
             std::pair{0U, 4.0F}, std::pair{2U, -3.0F}, std::pair{4U, 2.0F}}) {
        require(whole_eq.set_band_gain_db(band, gain),
                "whole-buffer band setup failed");
        require(chunked_eq.set_band_gain_db(band, gain),
                "chunked-buffer band setup failed");
    }
    whole_eq.process(whole, 2);

    constexpr std::array<std::size_t, 6> chunks{1, 63, 257, 128, 509, 17};
    std::size_t frame = 0;
    std::size_t block = 0;
    while (frame < frames) {
        const std::size_t count = std::min(chunks[block % chunks.size()],
                                           frames - frame);
        chunked_eq.process(std::span<float>(chunked).subspan(frame * 2,
                                                              count * 2), 2);
        frame += count;
        ++block;
    }
    for (std::size_t i = 0; i < whole.size(); ++i) {
        require(std::abs(whole[i] - chunked[i]) < 1.0e-6F,
                "irregular callback sizes changed EQ output");
    }

    whole_eq.reset();
    chunked_eq.reset();
    auto after_reset = source;
    auto fresh = source;
    whole_eq.process(after_reset, 2);
    chunked_eq.process(fresh, 2);
    require(after_reset == fresh,
            "reset should restore deterministic filter state");
}

void test_finite_recovery_and_no_process_allocation() {
    tabletfx::EQ5Graphic eq(kSampleRate, 1);
    require(eq.set_band_gain_db(2, 12.0F), "finite test band setup failed");
    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        0.1F, -0.2F, 0.3F,
    };
    eq.process(invalid, 1);
    require(std::all_of(invalid.begin(), invalid.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "non-finite or extreme samples produced non-finite output");

    std::array<float, 64> recovery{};
    recovery.fill(0.1F);
    eq.process(recovery, 1);
    require(std::all_of(recovery.begin(), recovery.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "filter state did not recover after invalid input");

    tabletfx::EQ5Graphic realtime_eq(kSampleRate, 2);
    require(realtime_eq.set_band_gain_db(2, 5.0F),
            "real-time allocation test band setup failed");
    std::array<float, 512> block{};
    block.fill(0.125F);
    for (int i = 0; i < 8; ++i) realtime_eq.process(block, 2);
    allocation_count.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    constexpr std::array<std::size_t, 8> frame_counts{1, 7, 31, 63,
                                                       64, 127, 128, 255};
    for (int i = 0; i < 128; ++i) {
        const std::size_t frames =
            frame_counts[static_cast<std::size_t>(i) % frame_counts.size()];
        auto span = std::span<float>(block.data(), frames * 2);
        realtime_eq.process(span, 2);
        for (float sample : span) {
            if (!std::isfinite(sample)) {
                count_allocations.store(false, std::memory_order_release);
                throw std::runtime_error(
                    "irregular process callback produced non-finite output");
            }
        }
    }
    count_allocations.store(false, std::memory_order_release);
    require(allocation_count.load(std::memory_order_relaxed) == 0,
            "process() allocated after construction");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_identity_fixed_centers_and_project_authored_bounds();
        test_each_band_boosts_its_fixed_center();
        test_overall_gain_and_zero_fixed_latency();
        test_mono_stereo_channel_isolation_and_layout_mismatch();
        test_irregular_blocks_and_reset_are_deterministic();
        test_finite_recovery_and_no_process_allocation();
        std::cout << "EQ5 Graphic tests passed\n";
    } catch (const std::exception& exception) {
        count_allocations.store(false, std::memory_order_release);
        std::cerr << "EQ5 Graphic test failure: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
