#include "tabletfx/effects/plate_reverb.hpp"
#include "reverb_bypass_test_helpers.hpp"

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
#include <utility>
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
    tabletfx::PlateReverb reverb(48000.0F, 2);
    const auto defaults = reverb.parameters();
    require(reverb.id() == "plate-reverb", "plate reverb id changed unexpectedly");
    require(defaults.decay_seconds == 1.8F && defaults.pre_delay_ms == 0.0F &&
                defaults.damping_hz == 7500.0F && defaults.diffusion == 0.82F &&
                defaults.balance == 0.5F && defaults.mix == 0.2F &&
                defaults.level_db == 0.0F,
            "plate reverb defaults changed unexpectedly");
    require(reverb.memory_bytes() > sizeof(reverb) && reverb.memory_bytes() < 200000,
            "stereo plate reverb memory should stay fixed and below 200 kB at 48 kHz");

    tabletfx::PlateReverbParameters extreme;
    extreme.decay_seconds = 100.0F;
    extreme.pre_delay_ms = 1000.0F;
    extreme.damping_hz = 100000.0F;
    extreme.diffusion = 2.0F;
    extreme.balance = -1.0F;
    extreme.mix = 4.0F;
    extreme.level_db = 100.0F;
    reverb.configure(extreme);
    const auto bounded = reverb.parameters();
    require(bounded.decay_seconds == 12.0F && bounded.pre_delay_ms == 120.0F &&
                bounded.damping_hz == 18000.0F && bounded.diffusion == 1.0F &&
                bounded.balance == 0.0F && bounded.mix == 1.0F &&
                bounded.level_db == 12.0F,
            "plate reverb controls were not clamped to their documented bounds");

    tabletfx::PlateReverbParameters invalid;
    invalid.decay_seconds = std::numeric_limits<float>::quiet_NaN();
    invalid.pre_delay_ms = std::numeric_limits<float>::infinity();
    invalid.damping_hz = -std::numeric_limits<float>::infinity();
    invalid.diffusion = std::numeric_limits<float>::quiet_NaN();
    invalid.balance = std::numeric_limits<float>::infinity();
    invalid.mix = std::numeric_limits<float>::quiet_NaN();
    invalid.level_db = std::numeric_limits<float>::infinity();
    reverb.configure(invalid);
    const auto safe = reverb.parameters();
    require(safe.decay_seconds == defaults.decay_seconds &&
                safe.pre_delay_ms == defaults.pre_delay_ms &&
                safe.damping_hz == defaults.damping_hz &&
                safe.diffusion == defaults.diffusion && safe.balance == defaults.balance &&
                safe.mix == defaults.mix && safe.level_db == defaults.level_db,
            "non-finite plate controls must fall back to defaults");

    tabletfx::PlateReverb low_rate(8000.0F, 1);
    require(low_rate.parameters().damping_hz == 3600.0F,
            "reported damping must reflect the sample-rate-aware cutoff limit");
}

void test_wet_response_predelay_and_dry_path() {
    tabletfx::PlateReverbParameters parameters;
    parameters.mix = 0.0F;
    parameters.pre_delay_ms = 120.0F;
    tabletfx::PlateReverb dry(48000.0F, 1, parameters);
    std::array<float, 32> samples{};
    samples[0] = 0.375F;
    samples[7] = -0.125F;
    const auto original = samples;
    dry.process(samples, 1);
    require(samples == original,
            "mix=0 must preserve the dry signal sample-for-sample without latency");

    parameters.mix = 1.0F;
    parameters.pre_delay_ms = 0.0F;
    parameters.diffusion = 0.0F;
    parameters.balance = 1.0F;
    tabletfx::PlateReverb without_predelay(48000.0F, 1, parameters);
    std::vector<float> impulse(48000, 0.0F);
    impulse[0] = 0.8F;
    without_predelay.process(impulse, 1);
    const std::size_t onset = first_nonzero(impulse);
    require(onset > 800 && onset < 900,
            "late plate response should begin after its shortest feedback line");
    require(energy(impulse, 1, 1000, 48000) > 1.0e-5,
            "plate impulse did not produce a measurable tail");

    parameters.pre_delay_ms = 10.0F;
    tabletfx::PlateReverb with_predelay(48000.0F, 1, parameters);
    std::vector<float> delayed_impulse(48000, 0.0F);
    delayed_impulse[0] = 0.8F;
    with_predelay.process(delayed_impulse, 1);
    const std::size_t delayed_onset = first_nonzero(delayed_impulse);
    require(delayed_onset >= onset + 479 && delayed_onset <= onset + 481,
            "wet response onset should move by the selected 10 ms predelay");
}

void test_decay_damping_and_diffusion_controls() {
    tabletfx::PlateReverbParameters short_parameters;
    short_parameters.mix = 1.0F;
    short_parameters.diffusion = 0.0F;
    short_parameters.balance = 1.0F;
    short_parameters.decay_seconds = 0.3F;
    tabletfx::PlateReverb short_plate(48000.0F, 1, short_parameters);
    auto long_parameters = short_parameters;
    long_parameters.decay_seconds = 4.0F;
    tabletfx::PlateReverb long_plate(48000.0F, 1, long_parameters);
    std::vector<float> short_tail(48000U * 3U, 0.0F);
    std::vector<float> long_tail(short_tail.size(), 0.0F);
    short_tail[0] = 0.8F;
    long_tail[0] = 0.8F;
    short_plate.process(short_tail, 1);
    long_plate.process(long_tail, 1);
    const double short_energy = energy(short_tail, 1, 48000, 144000);
    const double long_energy = energy(long_tail, 1, 48000, 144000);
    require(long_energy > short_energy * 1.5 && long_energy > 1.0e-8,
            "longer plate decay should retain materially more late-tail energy");

    tabletfx::PlateReverbParameters dark_parameters = short_parameters;
    dark_parameters.decay_seconds = 2.0F;
    dark_parameters.damping_hz = 500.0F;
    tabletfx::PlateReverb dark(48000.0F, 1, dark_parameters);
    auto bright_parameters = dark_parameters;
    bright_parameters.damping_hz = 12000.0F;
    tabletfx::PlateReverb bright(48000.0F, 1, bright_parameters);
    std::vector<float> tone(48000, 0.0F);
    for (std::size_t frame = 0; frame < 240; ++frame) {
        tone[frame] = 0.7F * std::sin(6.28318530718F * 6500.0F *
                                       static_cast<float>(frame) / 48000.0F);
    }
    auto dark_tone = tone;
    dark.process(dark_tone, 1);
    bright.process(tone, 1);
    const double dark_energy = energy(dark_tone, 1, 12000, 24000);
    const double bright_energy = energy(tone, 1, 12000, 24000);
    require(bright_energy > dark_energy * 1.5,
            "higher plate damping cutoff should retain more high-frequency tail energy");

    tabletfx::PlateReverbParameters diffuse_parameters = short_parameters;
    diffuse_parameters.balance = 0.0F;
    diffuse_parameters.diffusion = 0.0F;
    tabletfx::PlateReverb sparse(48000.0F, 1, diffuse_parameters);
    diffuse_parameters.diffusion = 1.0F;
    tabletfx::PlateReverb dense(48000.0F, 1, diffuse_parameters);
    std::vector<float> sparse_audio(8192, 0.0F);
    std::vector<float> dense_audio(sparse_audio.size(), 0.0F);
    sparse_audio[0] = 1.0F;
    dense_audio[0] = 1.0F;
    sparse.process(sparse_audio, 1);
    dense.process(dense_audio, 1);
    float maximum_difference = 0.0F;
    for (std::size_t index = 0; index < sparse_audio.size(); ++index) {
        maximum_difference = std::max(maximum_difference,
                                      std::abs(sparse_audio[index] - dense_audio[index]));
    }
    require(maximum_difference > 1.0e-3F,
            "plate diffusion control did not change the early-reflection response");
}

void test_stereo_decorrelation_reset_and_channel_guard() {
    tabletfx::PlateReverbParameters parameters;
    parameters.mix = 1.0F;
    parameters.diffusion = 0.0F;
    parameters.balance = 1.0F;
    tabletfx::PlateReverb stereo_reverb(48000.0F, 2, parameters);
    std::vector<float> stereo(48000U * 2U, 0.0F);
    stereo[0] = 0.5F;
    stereo[1] = 0.5F;
    stereo_reverb.process(stereo, 2);
    double channel_difference = 0.0;
    for (std::size_t frame = 0; frame < stereo.size() / 2U; ++frame) {
        require(std::isfinite(stereo[frame * 2U]) && std::isfinite(stereo[frame * 2U + 1U]),
                "stereo plate output must remain finite");
        channel_difference += std::abs(stereo[frame * 2U] - stereo[frame * 2U + 1U]);
    }
    require(channel_difference > 1.0e-3,
            "stereo plate channels should decorrelate identical input impulses");

    stereo_reverb.reset();
    std::array<float, 256> silence{};
    stereo_reverb.process(silence, 2);
    require(std::all_of(silence.begin(), silence.end(), [](float sample) { return sample == 0.0F; }),
            "plate reset must clear every reverb tail buffer");

    tabletfx::PlateReverb isolated(48000.0F, 2, parameters);
    std::vector<float> left_only(4096U * 2U, 0.0F);
    left_only[0] = 0.5F;
    isolated.process(left_only, 2);
    for (std::size_t frame = 0; frame < left_only.size() / 2U; ++frame) {
        require(left_only[frame * 2U + 1U] == 0.0F,
                "plate reverb leaked left input into the independent right channel");
    }

    std::array<float, 6> mismatched{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F};
    const auto before = mismatched;
    isolated.process(mismatched, 3);
    require(mismatched == before, "unsupported channel layout must leave input unchanged");
}

void test_partition_invariance_invalid_samples_and_construction() {
    tabletfx::PlateReverbParameters parameters;
    parameters.decay_seconds = 2.3F;
    parameters.pre_delay_ms = 13.25F;
    parameters.damping_hz = 4100.0F;
    parameters.diffusion = 0.61F;
    parameters.balance = 0.72F;
    parameters.mix = 0.67F;
    parameters.level_db = 2.0F;
    tabletfx::PlateReverb contiguous(44100.0F, 2, parameters);
    tabletfx::PlateReverb partitioned(44100.0F, 2, parameters);
    std::vector<float> source(2501U * 2U);
    for (std::size_t frame = 0; frame < source.size() / 2U; ++frame) {
        const float sample = 0.65F * std::sin(0.017F * static_cast<float>(frame));
        source[frame * 2U] = sample;
        source[frame * 2U + 1U] = -0.5F * sample;
    }
    auto whole = source;
    auto split = source;
    contiguous.process(whole, 2);
    constexpr std::array<std::size_t, 7> block_sizes{1, 11, 3, 71, 8, 127, 29};
    std::size_t frame_offset = 0;
    std::size_t block_index = 0;
    while (frame_offset < split.size() / 2U) {
        const std::size_t count = std::min(block_sizes[block_index % block_sizes.size()],
                                           split.size() / 2U - frame_offset);
        partitioned.process(std::span<float>(split.data() + frame_offset * 2U, count * 2U), 2);
        frame_offset += count;
        ++block_index;
    }
    for (std::size_t index = 0; index < whole.size(); ++index) {
        require(std::abs(whole[index] - split[index]) < 1.0e-6F,
                "plate output changed with processing block partitioning");
    }

    std::array<float, 8> exceptional{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
        8.0F, -8.0F, 0.25F};
    contiguous.process(exceptional, 2);
    require(std::all_of(exceptional.begin(), exceptional.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "non-finite input must be recovered to finite plate output");

    for (const auto [rate, channels] : std::array{
             std::pair{std::numeric_limits<float>::quiet_NaN(), std::uint16_t{1}},
             std::pair{7999.0F, std::uint16_t{1}},
             std::pair{384001.0F, std::uint16_t{1}},
             std::pair{48000.0F, std::uint16_t{0}},
             std::pair{48000.0F, std::uint16_t{3}}}) {
        bool rejected = false;
        try {
            tabletfx::PlateReverb invalid(rate, channels);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "plate reverb accepted an unsupported sample rate or channel count");
    }
}

void test_process_does_not_allocate() {
    tabletfx::PlateReverbParameters parameters;
    parameters.mix = 0.5F;
    parameters.spillover = true;
    tabletfx::PlateReverb reverb(48000.0F, 2, parameters);
    std::array<float, 1024> audio{};
    for (std::size_t frame = 0; frame < audio.size() / 2U; ++frame) {
        audio[frame * 2U] = 0.2F;
        audio[frame * 2U + 1U] = -0.1F;
    }
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    reverb.process(audio, 2);
    reverb.process_bypassed(audio, 2);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "plate reverb processing allocated memory");
}

void test_bypass_spillover() {
    tabletfx::PlateReverbParameters parameters;
    parameters.decay_seconds = 1.8F;
    parameters.pre_delay_ms = 0.0F;
    parameters.mix = 0.55F;
    auto tails_parameters = parameters;
    tails_parameters.spillover = true;
    tabletfx::PlateReverb no_tails(48000.0F, 1, parameters);
    tabletfx::PlateReverb tails(48000.0F, 1, tails_parameters);
    tabletfx::PlateReverb partitioned(48000.0F, 1, tails_parameters);
    tabletfx::PlateReverb dry_input(48000.0F, 1, tails_parameters);
    tabletfx::PlateReverb reenabled(48000.0F, 1, tails_parameters);
    reverb_test_support::verify_bypass_spillover(
        no_tails, tails, partitioned, dry_input, reenabled, "plate reverb");
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
        test_wet_response_predelay_and_dry_path();
        test_decay_damping_and_diffusion_controls();
        test_stereo_decorrelation_reset_and_channel_guard();
        test_partition_invariance_invalid_samples_and_construction();
        test_bypass_spillover();
        test_process_does_not_allocate();
        std::cout << "All plate reverb tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Plate reverb test failure: " << error.what() << '\n';
        return 1;
    }
}
