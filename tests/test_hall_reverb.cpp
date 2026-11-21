#include "tabletfx/effects/hall_reverb.hpp"

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

std::size_t first_nonzero(std::span<const float> audio) {
    for (std::size_t index = 0; index < audio.size(); ++index) {
        if (std::abs(audio[index]) > 1.0e-7F) return index;
    }
    return audio.size();
}

double energy(std::span<const float> audio, std::size_t channels,
              std::size_t first_frame, std::size_t last_frame,
              std::size_t channel = 0) {
    double total = 0.0;
    for (std::size_t frame = first_frame; frame < last_frame; ++frame) {
        const double sample = audio[frame * channels + channel];
        total += sample * sample;
    }
    return total;
}

void test_defaults_bounds_and_owned_memory() {
    tabletfx::HallReverb reverb(48000.0F, 2);
    const auto defaults = reverb.parameters();
    require(defaults.decay_seconds == 3.2F && defaults.pre_delay_ms == 32.0F &&
                defaults.damping_hz == 9000.0F && defaults.low_cut_hz == 100.0F &&
                defaults.mix == 0.25F && defaults.level_db == 0.0F,
            "Hall reverb defaults changed unexpectedly");
    require(reverb.id() == "hall-reverb", "Hall reverb id changed unexpectedly");
    require(reverb.memory_bytes() > sizeof(reverb) && reverb.memory_bytes() < 210000,
            "stereo Hall reverb should stay fixed and below 210 kB at 48 kHz");

    tabletfx::HallReverbParameters extreme;
    extreme.decay_seconds = 100.0F;
    extreme.pre_delay_ms = 1000.0F;
    extreme.damping_hz = 100000.0F;
    extreme.low_cut_hz = 100000.0F;
    extreme.mix = 3.0F;
    extreme.level_db = 50.0F;
    reverb.configure(extreme);
    const auto bounded = reverb.parameters();
    require(bounded.decay_seconds == 12.0F && bounded.pre_delay_ms == 120.0F &&
                bounded.damping_hz == 18000.0F && bounded.low_cut_hz == 1500.0F &&
                bounded.mix == 1.0F && bounded.level_db == 12.0F,
            "Hall reverb controls were not clamped to documented safe ranges");

    tabletfx::HallReverbParameters invalid;
    invalid.decay_seconds = std::numeric_limits<float>::quiet_NaN();
    invalid.pre_delay_ms = std::numeric_limits<float>::infinity();
    invalid.damping_hz = -std::numeric_limits<float>::infinity();
    invalid.low_cut_hz = std::numeric_limits<float>::quiet_NaN();
    invalid.mix = std::numeric_limits<float>::quiet_NaN();
    invalid.level_db = std::numeric_limits<float>::infinity();
    reverb.configure(invalid);
    const auto safe = reverb.parameters();
    require(safe.decay_seconds == 3.2F && safe.pre_delay_ms == 32.0F &&
                safe.damping_hz == 9000.0F && safe.low_cut_hz == 100.0F &&
                safe.mix == 0.25F && safe.level_db == 0.0F,
            "non-finite Hall reverb controls must fall back to defaults");
}

void test_dry_path_and_wet_predelay() {
    tabletfx::HallReverbParameters parameters;
    parameters.mix = 0.0F;
    parameters.pre_delay_ms = 120.0F;
    tabletfx::HallReverb dry(48000.0F, 1, parameters);
    std::array<float, 48> samples{};
    samples[0] = 0.375F;
    samples[17] = -0.125F;
    const auto original = samples;
    dry.process(samples, 1);
    require(samples == original,
            "mix=0 must preserve the dry path sample-for-sample with no added latency");

    parameters.mix = 1.0F;
    parameters.pre_delay_ms = 0.0F;
    tabletfx::HallReverb no_predelay(48000.0F, 1, parameters);
    std::vector<float> direct_wet(48000, 0.0F);
    direct_wet[0] = 1.0F;
    no_predelay.process(direct_wet, 1);
    const auto onset_without = first_nonzero(direct_wet);
    require(onset_without > 2000 && onset_without < 2600,
            "Hall wet response should begin after its first comb delay");

    parameters.pre_delay_ms = 20.0F;
    tabletfx::HallReverb delayed(48000.0F, 1, parameters);
    std::vector<float> predelayed(48000, 0.0F);
    predelayed[0] = 1.0F;
    delayed.process(predelayed, 1);
    const auto onset_with = first_nonzero(predelayed);
    require(onset_with >= onset_without + 959 && onset_with <= onset_without + 961,
            "wet onset should move by approximately the configured 20 ms predelay");
}

void test_decay_and_damping_change_the_tail() {
    tabletfx::HallReverbParameters short_parameters;
    short_parameters.decay_seconds = 0.6F;
    short_parameters.pre_delay_ms = 0.0F;
    short_parameters.mix = 1.0F;
    tabletfx::HallReverb short_hall(48000.0F, 1, short_parameters);
    auto long_parameters = short_parameters;
    long_parameters.decay_seconds = 4.0F;
    tabletfx::HallReverb long_hall(48000.0F, 1, long_parameters);
    std::vector<float> short_tail(48000 * 4, 0.0F);
    std::vector<float> long_tail(short_tail.size(), 0.0F);
    short_tail[0] = 0.8F;
    long_tail[0] = 0.8F;
    short_hall.process(short_tail, 1);
    long_hall.process(long_tail, 1);
    const double short_energy = energy(short_tail, 1, 48000, 96000);
    const double long_energy = energy(long_tail, 1, 48000, 96000);
    require(long_energy > short_energy * 1.5,
            "long Hall decay should retain materially more late energy");

    tabletfx::HallReverbParameters dark_parameters;
    dark_parameters.decay_seconds = 3.0F;
    dark_parameters.pre_delay_ms = 0.0F;
    dark_parameters.damping_hz = 700.0F;
    dark_parameters.mix = 1.0F;
    tabletfx::HallReverb dark(48000.0F, 1, dark_parameters);
    auto bright_parameters = dark_parameters;
    bright_parameters.damping_hz = 14000.0F;
    tabletfx::HallReverb bright(48000.0F, 1, bright_parameters);
    std::vector<float> bright_signal(48000, 0.0F);
    for (std::size_t frame = 0; frame < 240; ++frame) {
        bright_signal[frame] = 0.7F * std::sin(6.28318530718F * 7000.0F *
                                                static_cast<float>(frame) / 48000.0F);
    }
    auto dark_signal = bright_signal;
    dark.process(dark_signal, 1);
    bright.process(bright_signal, 1);
    const double dark_energy = energy(dark_signal, 1, 12000, 24000);
    const double bright_energy = energy(bright_signal, 1, 12000, 24000);
    require(bright_energy > dark_energy * 1.5,
            "higher damping frequency should preserve more high-frequency tail");
}

void test_stereo_isolation_reset_and_channel_guard() {
    tabletfx::HallReverbParameters parameters;
    parameters.pre_delay_ms = 0.0F;
    parameters.mix = 1.0F;
    tabletfx::HallReverb reverb(48000.0F, 2, parameters);
    std::vector<float> stereo(8192 * 2, 0.0F);
    stereo[0] = 0.5F;
    reverb.process(stereo, 2);
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        require(stereo[frame * 2 + 1] == 0.0F,
                "left reverb input must not leak into the independent right channel");
    }
    require(std::any_of(stereo.begin(), stereo.end(), [](float value) { return value != 0.0F; }),
            "left input should produce a Hall reverb tail");

    reverb.reset();
    std::array<float, 256> silence{};
    reverb.process(silence, 2);
    require(std::all_of(silence.begin(), silence.end(), [](float value) { return value == 0.0F; }),
            "reset must clear predelay, comb, diffuser, and filter state");

    std::array<float, 6> unchanged{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F};
    const auto before = unchanged;
    reverb.process(unchanged, 3);
    require(unchanged == before, "unsupported layouts must leave caller audio unchanged");
}

void test_block_partition_parameter_smoothing_and_invalid_audio() {
    tabletfx::HallReverbParameters parameters;
    parameters.decay_seconds = 2.4F;
    parameters.pre_delay_ms = 12.25F;
    parameters.damping_hz = 4600.0F;
    parameters.low_cut_hz = 80.0F;
    parameters.mix = 0.4F;
    tabletfx::HallReverb contiguous(44100.0F, 2, parameters);
    tabletfx::HallReverb partitioned(44100.0F, 2, parameters);
    std::vector<float> source(1603 * 2);
    for (std::size_t frame = 0; frame < source.size() / 2; ++frame) {
        const float sample = 0.6F * std::sin(0.021F * static_cast<float>(frame));
        source[frame * 2] = sample;
        source[frame * 2 + 1] = -0.4F * sample;
    }
    auto whole = source;
    auto split = source;
    contiguous.process(std::span<float>(whole.data(), 173 * 2), 2);
    partitioned.process(std::span<float>(split.data(), 173 * 2), 2);

    parameters.decay_seconds = 5.0F;
    parameters.pre_delay_ms = 23.75F;
    parameters.damping_hz = 10000.0F;
    parameters.mix = 0.7F;
    contiguous.configure(parameters);
    partitioned.configure(parameters);

    contiguous.process(std::span<float>(whole.data() + 173 * 2, whole.size() - 173 * 2), 2);
    constexpr std::array<std::size_t, 7> block_sizes{1, 9, 31, 3, 121, 17, 5};
    std::size_t frame_offset = 173;
    std::size_t block_index = 0;
    while (frame_offset < split.size() / 2) {
        const auto count = std::min(block_sizes[block_index % block_sizes.size()],
                                    split.size() / 2 - frame_offset);
        partitioned.process(
            std::span<float>(split.data() + frame_offset * 2, count * 2), 2);
        frame_offset += count;
        ++block_index;
    }
    for (std::size_t index = 0; index < whole.size(); ++index) {
        require(std::abs(whole[index] - split[index]) < 1.0e-6F,
                "variable block sizes changed the Hall reverb result");
    }

    std::array<float, 12> invalid{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(), 8.0F, -8.0F, 0.75F, -0.75F, 0.0F, 0.0F, 0.0F};
    contiguous.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(), [](float value) {
                return std::isfinite(value);
            }),
            "non-finite and extreme input must not produce non-finite output");
}

void test_process_does_not_allocate() {
    tabletfx::HallReverbParameters parameters;
    tabletfx::HallReverb reverb(48000.0F, 2, parameters);
    std::array<float, 1024> audio{};
    for (std::size_t frame = 0; frame < audio.size() / 2; ++frame) {
        audio[frame * 2] = 0.2F;
        audio[frame * 2 + 1] = -0.1F;
    }
    const auto before = allocation_count.load(std::memory_order_relaxed);
    reverb.process(audio, 2);
    const auto after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "Hall reverb process() allocated memory");
}

void test_invalid_constructor_inputs_are_rejected() {
    bool rate_rejected = false;
    try {
        tabletfx::HallReverb invalid_rate(1000.0F, 1);
    } catch (const std::invalid_argument&) {
        rate_rejected = true;
    }
    require(rate_rejected, "unsupported sample rate must be rejected before allocation");

    bool channels_rejected = false;
    try {
        tabletfx::HallReverb invalid_channels(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        channels_rejected = true;
    }
    require(channels_rejected, "unsupported channel layout must be rejected before allocation");
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
        test_dry_path_and_wet_predelay();
        test_decay_and_damping_change_the_tail();
        test_stereo_isolation_reset_and_channel_guard();
        test_block_partition_parameter_smoothing_and_invalid_audio();
        test_process_does_not_allocate();
        test_invalid_constructor_inputs_are_rejected();
        std::cout << "All Hall reverb tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Hall reverb test failure: " << error.what() << '\n';
        return 1;
    }
}
