#include "tabletfx/effects/spring_reverb.hpp"

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
#include <vector>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::size_t first_nonzero(const std::vector<float>& audio) {
    for (std::size_t index = 0; index < audio.size(); ++index) {
        if (std::abs(audio[index]) > 1.0e-7F) return index;
    }
    return audio.size();
}

double energy(std::span<const float> audio, std::size_t channels,
              std::size_t first_frame, std::size_t last_frame) {
    double total = 0.0;
    for (std::size_t frame = first_frame; frame < last_frame; ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const double sample = audio[frame * channels + channel];
            total += sample * sample;
        }
    }
    return total;
}

void test_defaults_bounds_and_memory() {
    tabletfx::SpringReverb reverb(48000.0F, 2);
    const auto parameters = reverb.parameters();
    require(parameters.decay_seconds == 2.4F && parameters.pre_delay_ms == 0.0F &&
                parameters.damping_hz == 8000.0F && parameters.dwell == 0.35F &&
                parameters.mix == 0.2F && parameters.level_db == 0.0F,
            "spring reverb defaults changed unexpectedly");
    require(reverb.id() == "spring-reverb", "spring reverb id changed unexpectedly");
    require(reverb.memory_bytes() > sizeof(reverb) && reverb.memory_bytes() < 100000,
            "stereo spring reverb should use fixed storage below 100 kB at 48 kHz");

    tabletfx::SpringReverbParameters extreme;
    extreme.decay_seconds = 100.0F;
    extreme.pre_delay_ms = 1000.0F;
    extreme.damping_hz = 100000.0F;
    extreme.dwell = 4.0F;
    extreme.mix = 3.0F;
    extreme.level_db = 100.0F;
    reverb.configure(extreme);
    const auto bounded = reverb.parameters();
    require(bounded.decay_seconds == 8.0F && bounded.pre_delay_ms == 80.0F &&
                bounded.damping_hz == 18000.0F && bounded.dwell == 1.0F &&
                bounded.mix == 1.0F && bounded.level_db == 12.0F,
            "spring reverb controls were not clamped to their documented limits");

    tabletfx::SpringReverbParameters invalid;
    invalid.decay_seconds = std::numeric_limits<float>::quiet_NaN();
    invalid.pre_delay_ms = std::numeric_limits<float>::infinity();
    invalid.damping_hz = -std::numeric_limits<float>::infinity();
    invalid.dwell = std::numeric_limits<float>::quiet_NaN();
    invalid.mix = std::numeric_limits<float>::quiet_NaN();
    invalid.level_db = std::numeric_limits<float>::infinity();
    reverb.configure(invalid);
    const auto safe = reverb.parameters();
    require(safe.decay_seconds == 2.4F && safe.pre_delay_ms == 0.0F &&
                safe.damping_hz == 8000.0F && safe.dwell == 0.35F &&
                safe.mix == 0.2F && safe.level_db == 0.0F,
            "non-finite spring reverb controls must fall back to defaults");

    tabletfx::SpringReverb low_rate(8000.0F, 1);
    require(low_rate.parameters().damping_hz == 3600.0F,
            "damping cutoff should respect the low-rate Nyquist margin");
    bool bad_rate_threw = false;
    try {
        tabletfx::SpringReverb bad_rate(7999.0F, 1);
    } catch (const std::invalid_argument&) {
        bad_rate_threw = true;
    }
    require(bad_rate_threw, "unsupported sample rates must be rejected");
    bool bad_channels_threw = false;
    try {
        tabletfx::SpringReverb bad_channels(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        bad_channels_threw = true;
    }
    require(bad_channels_threw, "unsupported channel layouts must be rejected");
}

void test_response_dry_path_and_wet_predelay() {
    tabletfx::SpringReverbParameters parameters;
    parameters.mix = 0.0F;
    parameters.pre_delay_ms = 80.0F;
    tabletfx::SpringReverb dry(48000.0F, 1, parameters);
    std::array<float, 32> dry_audio{};
    dry_audio[0] = 0.375F;
    dry_audio[7] = -0.125F;
    const auto original = dry_audio;
    dry.process(dry_audio, 1);
    require(dry_audio == original,
            "mix=0 must preserve the dry signal sample-for-sample without latency");

    parameters.mix = 1.0F;
    parameters.pre_delay_ms = 0.0F;
    tabletfx::SpringReverb immediate(48000.0F, 1, parameters);
    std::vector<float> impulse(48000, 0.0F);
    impulse[0] = 0.8F;
    immediate.process(impulse, 1);
    const auto onset = first_nonzero(impulse);
    require(onset >= 500 && onset <= 700,
            "spring response should begin after its short tank propagation delay");
    require(std::any_of(impulse.begin(), impulse.end(),
                        [](float sample) { return sample != 0.0F; }),
            "an impulse should produce a nonzero spring response");

    parameters.pre_delay_ms = 10.0F;
    tabletfx::SpringReverb delayed(48000.0F, 1, parameters);
    std::vector<float> delayed_impulse(48000, 0.0F);
    delayed_impulse[0] = 0.8F;
    delayed.process(delayed_impulse, 1);
    const auto delayed_onset = first_nonzero(delayed_impulse);
    require(delayed_onset >= onset + 479 && delayed_onset <= onset + 481,
            "wet onset should move by the selected 10 ms predelay");
}

void test_decay_and_damping_change_tail() {
    tabletfx::SpringReverbParameters short_parameters;
    short_parameters.mix = 1.0F;
    short_parameters.decay_seconds = 0.35F;
    tabletfx::SpringReverb short_reverb(48000.0F, 1, short_parameters);
    auto long_parameters = short_parameters;
    long_parameters.decay_seconds = 2.6F;
    tabletfx::SpringReverb long_reverb(48000.0F, 1, long_parameters);
    std::vector<float> short_tail(48000 * 4, 0.0F);
    std::vector<float> long_tail(short_tail.size(), 0.0F);
    short_tail[0] = 0.8F;
    long_tail[0] = 0.8F;
    short_reverb.process(short_tail, 1);
    long_reverb.process(long_tail, 1);
    const double short_energy = energy(short_tail, 1, 48000, 144000);
    const double long_energy = energy(long_tail, 1, 48000, 144000);
    require(long_energy > short_energy * 5.0,
            "longer decay should retain materially more late spring energy");

    tabletfx::SpringReverbParameters dark_parameters;
    dark_parameters.mix = 1.0F;
    dark_parameters.decay_seconds = 2.0F;
    dark_parameters.damping_hz = 700.0F;
    tabletfx::SpringReverb dark(48000.0F, 1, dark_parameters);
    auto bright_parameters = dark_parameters;
    bright_parameters.damping_hz = 12000.0F;
    tabletfx::SpringReverb bright(48000.0F, 1, bright_parameters);
    std::vector<float> high_tone(48000 * 2, 0.0F);
    for (std::size_t frame = 0; frame < 480; ++frame) {
        high_tone[frame] = 0.7F * std::sin(6.28318530718F * 7000.0F *
                                            static_cast<float>(frame) / 48000.0F);
    }
    auto dark_tone = high_tone;
    dark.process(dark_tone, 1);
    bright.process(high_tone, 1);
    const double dark_energy = energy(dark_tone, 1, 12000, 48000);
    const double bright_energy = energy(high_tone, 1, 12000, 48000);
    require(bright_energy > dark_energy * 1.25,
            "higher damping cutoff should retain more high-frequency tail energy");
}

void test_stereo_isolation_reset_and_channel_guard() {
    tabletfx::SpringReverbParameters parameters;
    parameters.mix = 1.0F;
    tabletfx::SpringReverb reverb(48000.0F, 2, parameters);
    std::vector<float> stereo(4096 * 2, 0.0F);
    stereo[0] = 0.5F;
    reverb.process(stereo, 2);
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        require(stereo[frame * 2 + 1] == 0.0F,
                "left input must not leak into the independent right spring channel");
    }
    require(std::any_of(stereo.begin(), stereo.end(),
                        [](float sample) { return sample != 0.0F; }),
            "left input should produce a reverberant output");

    reverb.reset();
    std::array<float, 256> silence{};
    reverb.process(silence, 2);
    require(std::all_of(silence.begin(), silence.end(),
                        [](float sample) { return sample == 0.0F; }),
            "reset must clear the complete spring tail");

    std::array<float, 6> unchanged{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F};
    const auto before = unchanged;
    reverb.process(unchanged, 3);
    require(unchanged == before,
            "unsupported channel layouts must leave caller audio unchanged");
}

void test_block_partition_and_invalid_input() {
    tabletfx::SpringReverbParameters parameters;
    parameters.decay_seconds = 1.4F;
    parameters.pre_delay_ms = 17.25F;
    parameters.damping_hz = 4200.0F;
    parameters.dwell = 0.7F;
    parameters.mix = 0.66F;
    parameters.level_db = 2.0F;
    tabletfx::SpringReverb contiguous(44100.0F, 2, parameters);
    tabletfx::SpringReverb partitioned(44100.0F, 2, parameters);
    std::vector<float> source(1501 * 2);
    for (std::size_t frame = 0; frame < source.size() / 2; ++frame) {
        const float sample = 0.65F * std::sin(0.017F * static_cast<float>(frame));
        source[frame * 2] = sample;
        source[frame * 2 + 1] = -0.5F * sample;
    }
    auto whole = source;
    auto split = source;
    contiguous.process(whole, 2);
    constexpr std::array<std::size_t, 7> block_sizes{1, 11, 3, 71, 8, 127, 29};
    std::size_t frame_offset = 0;
    std::size_t block_index = 0;
    while (frame_offset < split.size() / 2) {
        const auto count = std::min(block_sizes[block_index % block_sizes.size()],
                                    split.size() / 2 - frame_offset);
        partitioned.process(std::span<float>(split.data() + frame_offset * 2,
                                             count * 2), 2);
        frame_offset += count;
        ++block_index;
    }
    for (std::size_t index = 0; index < whole.size(); ++index) {
        require(std::abs(whole[index] - split[index]) < 1.0e-6F,
                "variable processing blocks changed the spring result");
    }

    std::array<float, 12> extreme{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(), 8.0F, 1.0F, -1.0F, 0.5F, -0.5F, 0.0F, 0.0F};
    contiguous.process(extreme, 2);
    require(std::all_of(extreme.begin(), extreme.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "non-finite and extreme inputs must not produce non-finite output");
}

void test_process_does_not_allocate() {
    tabletfx::SpringReverbParameters parameters;
    parameters.mix = 0.5F;
    tabletfx::SpringReverb reverb(48000.0F, 2, parameters);
    std::array<float, 1024> audio{};
    for (std::size_t frame = 0; frame < audio.size() / 2; ++frame) {
        audio[frame * 2] = 0.2F;
        audio[frame * 2 + 1] = -0.1F;
    }
    const auto before = allocation_count.load(std::memory_order_relaxed);
    reverb.process(audio, 2);
    const auto after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "spring reverb process() allocated memory");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
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
        test_response_dry_path_and_wet_predelay();
        test_decay_and_damping_change_tail();
        test_stereo_isolation_reset_and_channel_guard();
        test_block_partition_and_invalid_input();
        test_process_does_not_allocate();
        std::cout << "All spring reverb tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Spring reverb test failure: " << error.what() << '\n';
        return 1;
    }
}
