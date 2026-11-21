#include "tabletfx/effects.hpp"

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

void test_defaults_bounds_and_owned_memory() {
    tabletfx::RoomReverb reverb(48000.0F, 2);
    const auto parameters = reverb.parameters();
    require(parameters.size == tabletfx::RoomSize::small &&
                parameters.decay_seconds == 0.8F && parameters.pre_delay_ms == 10.0F &&
                parameters.damping_hz == 6000.0F && parameters.mix == 0.2F &&
                parameters.level_db == 0.0F,
            "room reverb defaults changed unexpectedly");
    require(reverb.id() == "room-reverb", "room reverb id changed unexpectedly");
    require(reverb.memory_bytes() > sizeof(reverb) && reverb.memory_bytes() < 200000,
            "stereo room reverb memory should stay fixed and below 200 kB at 48 kHz");

    tabletfx::RoomReverbParameters extreme;
    extreme.size = tabletfx::RoomSize::large;
    extreme.decay_seconds = 100.0F;
    extreme.pre_delay_ms = 1000.0F;
    extreme.damping_hz = 100000.0F;
    extreme.mix = 4.0F;
    extreme.level_db = 100.0F;
    reverb.configure(extreme);
    const auto bounded = reverb.parameters();
    require(bounded.size == tabletfx::RoomSize::large && bounded.decay_seconds == 8.0F &&
                bounded.pre_delay_ms == 80.0F && bounded.damping_hz == 18000.0F &&
                bounded.mix == 1.0F && bounded.level_db == 12.0F,
            "room reverb controls were not clamped to their documented limits");

    tabletfx::RoomReverbParameters invalid;
    invalid.size = static_cast<tabletfx::RoomSize>(255);
    invalid.decay_seconds = std::numeric_limits<float>::quiet_NaN();
    invalid.pre_delay_ms = std::numeric_limits<float>::infinity();
    invalid.damping_hz = -std::numeric_limits<float>::infinity();
    invalid.mix = std::numeric_limits<float>::quiet_NaN();
    invalid.level_db = std::numeric_limits<float>::infinity();
    reverb.configure(invalid);
    const auto safe = reverb.parameters();
    require(safe.size == tabletfx::RoomSize::small && safe.decay_seconds == 0.8F &&
                safe.pre_delay_ms == 10.0F && safe.damping_hz == 6000.0F &&
                safe.mix == 0.2F && safe.level_db == 0.0F,
            "non-finite room reverb controls must fall back to defaults");
}

void test_damping_parameter_reports_rate_aware_bound() {
    tabletfx::RoomReverb reverb(8000.0F, 1);
    require(reverb.parameters().damping_hz == 3600.0F,
            "reported damping must reflect the effective low-rate cutoff limit");
    tabletfx::RoomReverbParameters parameters;
    parameters.damping_hz = 18000.0F;
    reverb.configure(parameters);
    require(reverb.parameters().damping_hz == 3600.0F,
            "configured damping must report its effective sample-rate bound");
}

void test_dry_path_is_immediate_and_predelay_is_wet_only() {
    tabletfx::RoomReverbParameters parameters;
    parameters.pre_delay_ms = 80.0F;
    parameters.mix = 0.0F;
    tabletfx::RoomReverb dry(48000.0F, 1, parameters);
    std::array<float, 32> samples{};
    samples[0] = 0.375F;
    samples[7] = -0.125F;
    const auto original = samples;
    dry.process(samples, 1);
    require(samples == original,
            "mix=0 must preserve the dry signal sample-for-sample without latency");

    parameters.mix = 1.0F;
    parameters.pre_delay_ms = 0.0F;
    tabletfx::RoomReverb no_predelay(48000.0F, 1, parameters);
    std::vector<float> impulse(48000, 0.0F);
    impulse[0] = 1.0F;
    no_predelay.process(impulse, 1);
    const auto onset_without_predelay = first_nonzero(impulse);
    require(onset_without_predelay > 1000 && onset_without_predelay < 2000,
            "wet room response should begin after its shortest room comb, not on the dry sample");

    parameters.pre_delay_ms = 10.0F;
    tabletfx::RoomReverb with_predelay(48000.0F, 1, parameters);
    std::vector<float> delayed_impulse(48000, 0.0F);
    delayed_impulse[0] = 1.0F;
    with_predelay.process(delayed_impulse, 1);
    const auto onset_with_predelay = first_nonzero(delayed_impulse);
    require(onset_with_predelay >= onset_without_predelay + 479 &&
                onset_with_predelay <= onset_without_predelay + 481,
            "wet onset should move by the selected 10 ms predelay");
}

void test_decay_and_damping_change_tail() {
    tabletfx::RoomReverbParameters short_parameters;
    short_parameters.mix = 1.0F;
    short_parameters.pre_delay_ms = 0.0F;
    short_parameters.decay_seconds = 0.45F;
    tabletfx::RoomReverb short_room(48000.0F, 1, short_parameters);
    auto long_parameters = short_parameters;
    long_parameters.decay_seconds = 2.0F;
    tabletfx::RoomReverb long_room(48000.0F, 1, long_parameters);
    std::vector<float> short_tail(48000 * 3, 0.0F);
    std::vector<float> long_tail(short_tail.size(), 0.0F);
    short_tail[0] = 0.8F;
    long_tail[0] = 0.8F;
    short_room.process(short_tail, 1);
    long_room.process(long_tail, 1);
    const double short_energy = energy(short_tail, 1, 24000, 57600);
    const double long_energy = energy(long_tail, 1, 24000, 57600);
    require(long_energy > short_energy * 1.4,
            "longer decay setting should retain materially more late reverb energy");

    tabletfx::RoomReverbParameters dark_parameters;
    dark_parameters.mix = 1.0F;
    dark_parameters.pre_delay_ms = 0.0F;
    dark_parameters.decay_seconds = 1.5F;
    dark_parameters.damping_hz = 700.0F;
    tabletfx::RoomReverb dark(48000.0F, 1, dark_parameters);
    auto bright_parameters = dark_parameters;
    bright_parameters.damping_hz = 12000.0F;
    tabletfx::RoomReverb bright(48000.0F, 1, bright_parameters);
    std::vector<float> high_tone(48000, 0.0F);
    for (std::size_t frame = 0; frame < 240; ++frame) {
        high_tone[frame] = 0.7F * std::sin(6.28318530718F * 7000.0F *
                                             static_cast<float>(frame) / 48000.0F);
    }
    auto dark_tone = high_tone;
    dark.process(dark_tone, 1);
    bright.process(high_tone, 1);
    const double dark_energy = energy(dark_tone, 1, 12000, 24000);
    const double bright_energy = energy(high_tone, 1, 12000, 24000);
    require(bright_energy > dark_energy * 1.5,
            "higher damping cutoff should retain more high-frequency tail energy");
}

void test_stereo_isolation_reset_and_channel_guard() {
    tabletfx::RoomReverbParameters parameters;
    parameters.mix = 1.0F;
    parameters.pre_delay_ms = 0.0F;
    tabletfx::RoomReverb reverb(48000.0F, 2, parameters);
    std::vector<float> stereo(4096 * 2, 0.0F);
    stereo[0] = 0.5F;
    reverb.process(stereo, 2);
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        require(stereo[frame * 2 + 1] == 0.0F,
                "a left input must not leak into the independent right reverb channel");
    }
    require(std::any_of(stereo.begin(), stereo.end(), [](float sample) { return sample != 0.0F; }),
            "left input should produce a reverberant output");

    reverb.reset();
    std::array<float, 256> silence{};
    reverb.process(silence, 2);
    require(std::all_of(silence.begin(), silence.end(), [](float sample) { return sample == 0.0F; }),
            "reset must clear every reverb tail buffer");

    std::array<float, 6> unchanged{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F};
    const auto before = unchanged;
    reverb.process(unchanged, 3);
    require(unchanged == before, "unsupported channel layouts must leave caller audio unchanged");
}

void test_variable_block_partition_and_finite_recovery() {
    tabletfx::RoomReverbParameters parameters;
    parameters.size = tabletfx::RoomSize::large;
    parameters.decay_seconds = 1.4F;
    parameters.pre_delay_ms = 17.25F;
    parameters.damping_hz = 4200.0F;
    parameters.mix = 0.66F;
    parameters.level_db = 2.0F;
    tabletfx::RoomReverb contiguous(44100.0F, 2, parameters);
    tabletfx::RoomReverb partitioned(44100.0F, 2, parameters);
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
        partitioned.process(std::span<float>(split.data() + frame_offset * 2, count * 2), 2);
        frame_offset += count;
        ++block_index;
    }
    for (std::size_t index = 0; index < whole.size(); ++index) {
        require(std::abs(whole[index] - split[index]) < 1.0e-6F,
                "variable processing blocks changed the sample result");
    }

    std::array<float, 12> extreme{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(), 8.0F, 1.0F, -1.0F, 0.5F, -0.5F, 0.0F, 0.0F};
    contiguous.process(extreme, 2);
    require(std::all_of(extreme.begin(), extreme.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "non-finite and extreme input must not create non-finite output");
}

void test_process_does_not_allocate() {
    tabletfx::RoomReverbParameters parameters;
    parameters.mix = 0.5F;
    tabletfx::RoomReverb reverb(48000.0F, 2, parameters);
    std::array<float, 1024> audio{};
    for (std::size_t frame = 0; frame < audio.size() / 2; ++frame) {
        audio[frame * 2] = 0.2F;
        audio[frame * 2 + 1] = -0.1F;
    }
    const auto before = allocation_count.load(std::memory_order_relaxed);
    reverb.process(audio, 2);
    const auto after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "room reverb process() allocated memory");
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
        test_defaults_bounds_and_owned_memory();
        test_damping_parameter_reports_rate_aware_bound();
        test_dry_path_is_immediate_and_predelay_is_wet_only();
        test_decay_and_damping_change_tail();
        test_stereo_isolation_reset_and_channel_guard();
        test_variable_block_partition_and_finite_recovery();
        test_process_does_not_allocate();
        std::cout << "All room reverb tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Room reverb test failure: " << error.what() << '\n';
        return 1;
    }
}
