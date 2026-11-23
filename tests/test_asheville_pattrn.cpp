#include "tabletfx/effects/asheville_pattrn.hpp"

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
#include <string>

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float actual, float expected, float tolerance = 1.0e-5F) {
    return std::abs(actual - expected) <= tolerance;
}

float rms(const std::array<float, 24000>& values, std::size_t skip) {
    double sum = 0.0;
    std::size_t count = 0U;
    for (std::size_t i = skip; i < values.size(); ++i) {
        const double value = values[i];
        sum += value * value;
        ++count;
    }
    return count == 0U ? 0.0F
                       : static_cast<float>(std::sqrt(sum /
                                                     static_cast<double>(count)));
}

void fill_sine(std::array<float, 24000>& samples, float frequency_hz) {
    constexpr float sample_rate = 48000.0F;
    constexpr float amplitude = 0.25F;
    constexpr float two_pi = 6.28318530717958647692F;
    for (std::size_t i = 0U; i < samples.size(); ++i) {
        samples[i] = amplitude * std::sin(
            two_pi * frequency_hz * static_cast<float>(i) / sample_rate);
    }
}

void test_defaults_bounds_and_memory() {
    tabletfx::ProjectAshevillePattrn effect;
    const auto parameters = effect.parameters();
    require(effect.id() == "project-asheville-pattrn",
            "project Asheville Pattrn stable ID changed");
    require(parameters.bank == tabletfx::AshevilleFrequencyBank::Bass &&
                parameters.pattern == 0U && parameters.drive_db == 0.0F &&
                parameters.output_db == 0.0F && parameters.mix == 0.5F &&
                parameters.pattern_rate_hz == 0.75F &&
                parameters.resonance_q == 3.0F &&
                parameters.envelope_shape == 0.5F &&
                parameters.smoothing_ms == 10.0F,
            "project-authored defaults changed");
    require(tabletfx::project_asheville_band_count == 8U &&
                tabletfx::project_asheville_pattern_count == 8U,
            "project pattern/band counts changed");
    require(effect.memory_bytes() == sizeof(effect),
            "fixed-state memory report is inaccurate");
    for (float level : parameters.band_level) {
        require(level == 1.0F, "band-level defaults should be unity");
    }

    tabletfx::ProjectAshevillePattrnParameters invalid;
    invalid.bank = static_cast<tabletfx::AshevilleFrequencyBank>(255U);
    invalid.pattern = 255U;
    invalid.band_level[0] = -1.0F;
    invalid.band_level[1] = std::numeric_limits<float>::quiet_NaN();
    invalid.drive_db = 100.0F;
    invalid.output_db = -100.0F;
    invalid.mix = 2.0F;
    invalid.pattern_rate_hz = 0.0F;
    invalid.resonance_q = 100.0F;
    invalid.envelope_shape = -1.0F;
    invalid.smoothing_ms = std::numeric_limits<float>::infinity();
    effect.set_parameters(invalid);
    const auto bounded = effect.parameters();
    require(bounded.bank == tabletfx::AshevilleFrequencyBank::Bass &&
                bounded.pattern == 7U && bounded.band_level[0] == 0.0F &&
                bounded.band_level[1] == 1.0F && bounded.drive_db == 18.0F &&
                bounded.output_db == -24.0F && bounded.mix == 1.0F &&
                bounded.pattern_rate_hz == 0.05F &&
                bounded.resonance_q == 12.0F &&
                bounded.envelope_shape == 0.0F &&
                bounded.smoothing_ms == 10.0F,
            "project-authored bounds or non-finite fallbacks failed");

    std::array<float, 2> extreme{1.0e30F, -1.0e30F};
    effect.process(extreme, 2U);
    for (float sample : extreme) {
        require(std::isfinite(sample) && std::abs(sample) <= 8.0F,
                "extreme input escaped project input/output clamps");
    }
}

void test_mix_and_band_controls() {
    tabletfx::ProjectAshevillePattrnParameters parameters;
    parameters.mix = 0.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::ProjectAshevillePattrn dry_effect(48000.0F, parameters);
    std::array<float, 8> dry{0.1F, -0.2F, 0.3F, -0.4F,
                             0.5F, -0.6F, 0.7F, -0.8F};
    const auto original = dry;
    dry_effect.process(dry, 2U);
    require(dry == original, "zero wet mix did not preserve dry input");

    parameters.mix = 1.0F;
    parameters.band_level.fill(0.0F);
    tabletfx::ProjectAshevillePattrn muted_bank(48000.0F, parameters);
    std::array<float, 8> wet_only{0.1F, -0.2F, 0.3F, -0.4F,
                                  0.5F, -0.6F, 0.7F, -0.8F};
    muted_bank.process(wet_only, 2U);
    for (float sample : wet_only) {
        require(sample == 0.0F,
                "zero band levels did not mute the wet filter bank");
    }
}

void test_pattern_and_frequency_banks_change_response() {
    tabletfx::ProjectAshevillePattrnParameters bass_parameters;
    bass_parameters.mix = 1.0F;
    bass_parameters.pattern_rate_hz = 0.05F;
    bass_parameters.envelope_shape = 0.0F;
    bass_parameters.resonance_q = 4.0F;
    tabletfx::ProjectAshevillePattrn bass(48000.0F, bass_parameters);

    auto mids_parameters = bass_parameters;
    mids_parameters.bank = tabletfx::AshevilleFrequencyBank::Mids;
    tabletfx::ProjectAshevillePattrn mids(48000.0F, mids_parameters);
    std::array<float, 24000> bass_audio{};
    std::array<float, 24000> mids_audio{};
    fill_sine(bass_audio, 120.0F);
    mids_audio = bass_audio;
    bass.process(bass_audio, 1U);
    mids.process(mids_audio, 1U);
    const float bass_rms = rms(bass_audio, 6000U);
    const float mids_rms = rms(mids_audio, 6000U);
    require(bass_rms > mids_rms * 1.5F,
            "Bass/Mids project banks did not select distinct frequency centers");

    auto alternate_parameters = bass_parameters;
    alternate_parameters.pattern = 1U;
    tabletfx::ProjectAshevillePattrn alternate(48000.0F,
                                               alternate_parameters);
    std::array<float, 24000> default_pattern_audio{};
    std::array<float, 24000> alternate_pattern_audio{};
    fill_sine(default_pattern_audio, 775.0F);
    alternate_pattern_audio = default_pattern_audio;
    bass.reset();
    alternate.process(alternate_pattern_audio, 1U);
    bass.process(default_pattern_audio, 1U);
    double pattern_difference = 0.0;
    for (std::size_t i = 6000U; i < default_pattern_audio.size(); ++i) {
        pattern_difference += std::abs(static_cast<double>(
            default_pattern_audio[i] - alternate_pattern_audio[i]));
    }
    require(pattern_difference > 1.0,
            "project-authored pattern choices did not change the response");

    auto fast_parameters = bass_parameters;
    fast_parameters.pattern_rate_hz = 4.0F;
    tabletfx::ProjectAshevillePattrn fast(48000.0F, fast_parameters);
    std::array<float, 24000> slow_rate_audio{};
    std::array<float, 24000> fast_rate_audio{};
    fill_sine(slow_rate_audio, 775.0F);
    fast_rate_audio = slow_rate_audio;
    bass.reset();
    fast.process(fast_rate_audio, 1U);
    bass.process(slow_rate_audio, 1U);
    double rate_difference = 0.0;
    for (std::size_t i = 6000U; i < slow_rate_audio.size(); ++i) {
        rate_difference += std::abs(static_cast<double>(
            slow_rate_audio[i] - fast_rate_audio[i]));
    }
    require(rate_difference > 1.0,
            "project-authored pattern rate did not change the response");
}

void test_bass_lowpass_rejects_off_frequency_input() {
    tabletfx::ProjectAshevillePattrnParameters parameters;
    parameters.bank = tabletfx::AshevilleFrequencyBank::Bass;
    parameters.mix = 1.0F;
    parameters.pattern_rate_hz = 0.05F;
    parameters.envelope_shape = 0.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::ProjectAshevillePattrn low_input_effect(48000.0F, parameters);
    tabletfx::ProjectAshevillePattrn high_input_effect(48000.0F, parameters);

    std::array<float, 24000> low_input{};
    std::array<float, 24000> off_frequency_input{};
    fill_sine(low_input, 60.0F);
    fill_sine(off_frequency_input, 1000.0F);
    low_input_effect.process(low_input, 1U);
    high_input_effect.process(off_frequency_input, 1U);

    const float low_rms = rms(low_input, 6000U);
    const float off_frequency_rms = rms(off_frequency_input, 6000U);
    require(low_rms > off_frequency_rms * 8.0F,
            "BASS 110 Hz low-pass did not attenuate off-frequency input");
}

void test_smoothing_reset_stereo_and_no_allocations() {
    tabletfx::ProjectAshevillePattrn whole_effect;
    tabletfx::ProjectAshevillePattrn chunked_effect;
    tabletfx::ProjectAshevillePattrnParameters parameters;
    parameters.pattern_rate_hz = 1.0F;
    parameters.smoothing_ms = 10.0F;
    parameters.mix = 0.8F;
    whole_effect.set_parameters(parameters);
    chunked_effect.set_parameters(parameters);

    std::array<float, 997> whole{};
    whole.fill(0.2F);
    auto chunked = whole;
    whole_effect.process(whole, 1U);
    constexpr std::array<std::size_t, 5> blocks{1U, 127U, 53U, 300U, 17U};
    std::size_t offset = 0U;
    std::size_t block = 0U;
    while (offset < chunked.size()) {
        const std::size_t count = std::min(blocks[block % blocks.size()],
                                           chunked.size() - offset);
        chunked_effect.process(
            std::span<float>(chunked).subspan(offset, count), 1U);
        offset += count;
        ++block;
    }
    for (std::size_t i = 0U; i < whole.size(); ++i) {
        require(near(whole[i], chunked[i], 1.0e-6F),
                "variable blocks changed sequencer/envelope state");
    }

    tabletfx::ProjectAshevillePattrn stereo;
    std::array<float, 8> pair{0.25F, 0.0F, 0.5F, 0.0F,
                              -0.25F, 0.0F, -0.5F, 0.0F};
    stereo.process(pair, 2U);
    require(pair[1] == 0.0F && pair[3] == 0.0F &&
                pair[5] == 0.0F && pair[7] == 0.0F,
            "left-channel filter state leaked into the right channel");

    std::array<float, 512> audio{};
    audio.fill(0.1F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        stereo.process(audio, 2U);
    }
    count_allocations.store(false, std::memory_order_relaxed);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "process allocated memory");

    stereo.reset();
    auto first = audio;
    auto second = audio;
    stereo.process(first, 2U);
    stereo.reset();
    stereo.process(second, 2U);
    require(first == second, "reset did not restore deterministic state");
}

void test_invalid_samples_and_layout_guard() {
    tabletfx::ProjectAshevillePattrn effect;
    std::array<float, 3> unchanged{0.1F, -0.2F, 0.3F};
    const auto original = unchanged;
    effect.process(unchanged, 0U);
    effect.process(unchanged, 3U);
    require(unchanged == original, "unsupported channel layout was modified");

    std::array<float, 4> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 100.0F, -100.0F};
    effect.process(invalid, 2U);
    for (float sample : invalid) {
        require(std::isfinite(sample), "invalid input produced non-finite output");
    }
    require(invalid[0] == 0.0F && invalid[1] == 0.0F,
            "non-finite samples were not silenced");

    std::array<float, 32> recovery{};
    recovery.fill(0.1F);
    effect.process(recovery, 2U);
    for (float sample : recovery) {
        require(std::isfinite(sample),
                "processor did not recover after non-finite input");
    }
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
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
        test_defaults_bounds_and_memory();
        test_mix_and_band_controls();
        test_pattern_and_frequency_banks_change_response();
        test_bass_lowpass_rejects_off_frequency_input();
        test_smoothing_reset_stereo_and_no_allocations();
        test_invalid_samples_and_layout_guard();
        std::cout << "Project Asheville Pattrn tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        count_allocations.store(false, std::memory_order_relaxed);
        std::cerr << "Project Asheville Pattrn test failure: " << error.what()
                  << '\n';
        return 1;
    }
}
