#include "tabletfx/effects/am_ring_mod.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void fill_sine(std::span<float> samples, float frequency_hz,
               float sample_rate, float amplitude = 0.5F,
               std::uint16_t channels = 1U) {
    const std::size_t frames = samples.size() / channels;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const float value = amplitude * std::sin(
            2.0F * std::numbers::pi_v<float> * frequency_hz *
            static_cast<float>(frame) / sample_rate);
        for (std::uint16_t channel = 0U; channel < channels; ++channel) {
            samples[frame * channels + channel] = value;
        }
    }
}

double amplitude_near(std::span<const float> samples, float frequency_hz,
                      float sample_rate, std::size_t start_frame,
                      std::size_t length) {
    double in_phase = 0.0;
    double quadrature = 0.0;
    double window_sum = 0.0;
    for (std::size_t index = 0U; index < length; ++index) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * std::numbers::pi_v<double> * static_cast<double>(index) /
            static_cast<double>(length - 1U));
        const double phase = 2.0 * std::numbers::pi_v<double> *
            static_cast<double>(frequency_hz) *
            static_cast<double>(start_frame + index) /
            static_cast<double>(sample_rate);
        const double sample =
            static_cast<double>(samples[start_frame + index]) * window;
        in_phase += sample * std::cos(phase);
        quadrature += sample * std::sin(phase);
        window_sum += window;
    }
    return 2.0 * std::hypot(in_phase, quadrature) / window_sum;
}

void test_defaults_bounds_and_construction() {
    using namespace tabletfx::effects;
    AmRingMod effect;
    require(effect.id() == "am-ring-mod", "stable AM ring-mod ID changed");
    require(effect.parameters().rate_hz == 440.0F &&
                effect.parameters().depth == 0.5F &&
                effect.parameters().mix == 1.0F &&
                effect.parameters().level_db == 0.0F,
            "project-authored AM defaults changed");
    require(effect.effective_rate_hz() == 440.0F &&
                effect.latency_samples() == 0U &&
                effect.memory_bytes() == sizeof(effect),
            "AM ring-mod reported rate, latency, or memory changed");

    auto parameters = effect.parameters();
    parameters.rate_hz = -10.0F;
    parameters.depth = 2.0F;
    parameters.mix = -1.0F;
    parameters.level_db = 100.0F;
    effect.set_parameters(parameters);
    require(effect.parameters().rate_hz == 0.1F &&
                effect.parameters().depth == 1.0F &&
                effect.parameters().mix == 0.0F &&
                effect.parameters().level_db == 12.0F,
            "project-authored AM bounds were not enforced");

    parameters.rate_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.depth = std::numeric_limits<float>::infinity();
    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    parameters.level_db = -std::numeric_limits<float>::infinity();
    effect.set_parameters(parameters);
    require(effect.parameters().rate_hz == 0.1F &&
                effect.parameters().depth == 1.0F &&
                effect.parameters().mix == 0.0F &&
                effect.parameters().level_db == 12.0F,
            "non-finite controls did not fall back safely");

    bool bad_rate = false;
    try {
        AmRingMod unsupported(7999.0F, 1U);
    } catch (const std::invalid_argument&) {
        bad_rate = true;
    }
    require(bad_rate, "AM ring-mod accepted an unsupported sample rate");

    bool bad_channels = false;
    try {
        AmRingMod unsupported(48000.0F, 3U);
    } catch (const std::invalid_argument&) {
        bad_channels = true;
    }
    require(bad_channels, "AM ring-mod accepted more than two channels");
}

void test_am_carrier_and_sidebands_differ_from_bipolar_ring_mod() {
    using namespace tabletfx::effects;
    constexpr float sample_rate = 48000.0F;
    constexpr float input_frequency = 3000.0F;
    constexpr float rate_hz = 440.0F;
    constexpr std::size_t frames = 48000U;
    constexpr std::size_t start = 8000U;
    constexpr std::size_t length = 24000U;

    std::vector<float> audio(frames, 0.0F);
    fill_sine(audio, input_frequency, sample_rate);
    AmRingModParameters parameters;
    parameters.rate_hz = rate_hz;
    parameters.depth = 1.0F;
    parameters.mix = 1.0F;
    AmRingMod effect(sample_rate, 1U, parameters);
    effect.process(audio, 1U);

    const double carrier = amplitude_near(
        audio, input_frequency, sample_rate, start, length);
    const double lower_sideband = amplitude_near(
        audio, input_frequency - rate_hz, sample_rate, start, length);
    const double upper_sideband = amplitude_near(
        audio, input_frequency + rate_hz, sample_rate, start, length);
    require(carrier > 0.45 && carrier < 0.55,
            "AM modulation should retain the input carrier component");
    require(lower_sideband > 0.20 && lower_sideband < 0.30 &&
                upper_sideband > 0.20 && upper_sideband < 0.30,
            "AM modulation did not generate its expected paired sidebands");
}

void test_depth_mix_and_level_controls() {
    using namespace tabletfx::effects;
    constexpr float sample_rate = 48000.0F;
    constexpr std::size_t frames = 1024U;
    std::array<float, frames> original{};
    fill_sine(original, 613.0F, sample_rate, 0.4F);

    AmRingModParameters parameters;
    parameters.rate_hz = 300.0F;
    parameters.depth = 0.0F;
    parameters.mix = 1.0F;
    AmRingMod no_depth(sample_rate, 1U, parameters);
    auto unchanged = original;
    no_depth.process(unchanged, 1U);
    require(unchanged == original, "zero AM depth changed the input");

    parameters.depth = 1.0F;
    parameters.mix = 0.0F;
    AmRingMod dry(sample_rate, 1U, parameters);
    auto dry_audio = original;
    dry.process(dry_audio, 1U);
    require(dry_audio == original, "zero AM mix changed the dry input");

    parameters.level_db = 6.0205999F;
    AmRingMod level(sample_rate, 1U, parameters);
    auto level_audio = original;
    level.process(level_audio, 1U);
    for (std::size_t index = 0U; index < frames; ++index) {
        require(std::abs(level_audio[index] - 2.0F * original[index]) < 1.0e-5F,
                "overall output level control did not apply dB gain");
    }
}

void test_shared_stereo_phase_and_channel_mismatch() {
    using namespace tabletfx::effects;
    constexpr float sample_rate = 48000.0F;
    std::array<float, 1024U> stereo{};
    fill_sine(stereo, 713.0F, sample_rate, 0.4F, 2U);
    for (std::size_t frame = 0U; frame < stereo.size() / 2U; ++frame) {
        stereo[frame * 2U + 1U] *= 0.5F;
    }
    AmRingMod effect(sample_rate, 2U);
    effect.process(stereo, 2U);
    for (std::size_t frame = 0U; frame < stereo.size() / 2U; ++frame) {
        require(std::abs(stereo[frame * 2U] -
                         2.0F * stereo[frame * 2U + 1U]) < 1.0e-6F,
                "stereo channels did not share the AM oscillator phase");
    }

    std::array<float, 5U> mono{0.1F, -0.2F, 0.3F, -0.4F, 0.5F};
    const auto before = mono;
    effect.process(mono, 1U);
    require(mono == before, "channel mismatch should leave samples unchanged");
}

void test_partition_reset_invalid_samples_and_no_allocation() {
    using namespace tabletfx::effects;
    constexpr float sample_rate = 44100.0F;
    constexpr std::size_t sample_count = 4096U;
    AmRingMod whole(sample_rate, 2U);
    AmRingMod chunks(sample_rate, 2U);
    std::vector<float> input(sample_count, 0.0F);
    for (std::size_t index = 0U; index < input.size(); ++index) {
        input[index] = 0.4F * std::sin(static_cast<float>(index) * 0.017F);
    }
    auto expected = input;
    auto actual = input;
    whole.process(expected, 2U);
    const std::array<std::size_t, 4U> block_sizes{2U, 106U, 254U, 422U};
    std::size_t offset = 0U;
    std::size_t block = 0U;
    while (offset < actual.size()) {
        std::size_t count = std::min(block_sizes[block % block_sizes.size()],
                                     actual.size() - offset);
        count -= count % 2U;
        if (count == 0U) count = 2U;
        chunks.process(std::span<float>(actual).subspan(offset, count), 2U);
        offset += count;
        ++block;
    }
    require(actual == expected, "output depends on block partitioning");

    whole.reset();
    chunks.reset();
    std::array<float, 32U> reset_a{};
    reset_a.fill(0.4F);
    auto reset_b = reset_a;
    whole.process(reset_a, 2U);
    chunks.process(reset_b, 2U);
    require(reset_a == reset_b, "reset did not restore deterministic phase");

    std::array<float, 8U> exceptional{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
        0.25F, -0.25F, 0.5F, -0.5F};
    AmRingMod mono(sample_rate, 1U);
    mono.process(exceptional, 1U);
    require(std::all_of(exceptional.begin(), exceptional.end(),
                        [](float value) { return std::isfinite(value); }),
            "non-finite or extreme input produced non-finite output");

    AmRingMod allocation_test(sample_rate, 2U);
    std::array<float, 512U> audio_block{};
    audio_block.fill(0.25F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        allocation_test.process(audio_block, 2U);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "process() allocated after construction");
}

}  // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
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
        test_defaults_bounds_and_construction();
        test_am_carrier_and_sidebands_differ_from_bipolar_ring_mod();
        test_depth_mix_and_level_controls();
        test_shared_stereo_phase_and_channel_mismatch();
        test_partition_reset_invalid_samples_and_no_allocation();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "AM Ring Mod test failed: " << error.what() << '\n';
        return 1;
    }
}
