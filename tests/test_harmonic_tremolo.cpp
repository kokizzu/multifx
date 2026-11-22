#include "tabletfx/effects/harmonic_tremolo.hpp"

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
    if (!condition) {
        throw std::runtime_error(message);
    }
}

float rms(const std::vector<float>& samples, std::size_t first,
          std::size_t last) {
    double sum = 0.0;
    for (std::size_t i = first; i < last; ++i) {
        const double value = static_cast<double>(samples[i]);
        sum += value * value;
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(last - first)));
}

std::vector<float> render_mono_tone(float frequency_hz,
                                   float crossover_hz = 700.0F) {
    constexpr float sample_rate = 48000.0F;
    tabletfx::HarmonicTremoloParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.crossover_hz = crossover_hz;
    parameters.smoothing_ms = 0.0F;
    tabletfx::HarmonicTremolo effect(sample_rate, parameters);
    std::vector<float> samples(48000);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = std::sin(2.0F * 3.14159265358979323846F * frequency_hz *
                              static_cast<float>(i) / sample_rate);
    }
    effect.process(samples, 1);
    return samples;
}

void test_defaults_bounds_and_memory() {
    tabletfx::HarmonicTremolo effect;
    const auto parameters = effect.parameters();
    require(effect.id() == "harmonic-tremolo",
            "harmonic tremolo stable ID changed");
    require(parameters.rate_hz == 3.0F && parameters.depth == 0.65F &&
                parameters.crossover_hz == 700.0F &&
                parameters.waveform ==
                    tabletfx::HarmonicTremoloWaveform::Sine &&
                !parameters.tempo_sync && parameters.tempo_bpm == 120.0F &&
                parameters.beats_per_cycle == 1.0F &&
                parameters.stereo_alternation &&
                parameters.smoothing_ms == 10.0F &&
                effect.effective_rate_hz() == 3.0F,
            "project-authored harmonic tremolo defaults changed");
    require(effect.memory_bytes() == sizeof(effect),
            "harmonic tremolo memory report is inaccurate");

    effect.set_rate_hz(-1.0F);
    require(effect.parameters().rate_hz == 0.05F &&
                effect.effective_rate_hz() == 0.05F,
            "minimum free rate was not enforced");
    effect.set_rate_hz(100.0F);
    require(effect.parameters().rate_hz == 20.0F &&
                effect.effective_rate_hz() == 20.0F,
            "maximum free rate was not enforced");
    effect.set_rate_hz(std::numeric_limits<float>::quiet_NaN());
    require(effect.parameters().rate_hz == 20.0F,
            "non-finite rate changed the last valid rate");

    effect.set_depth(-1.0F);
    require(effect.parameters().depth == 0.0F,
            "negative depth did not clamp to zero");
    effect.set_depth(2.0F);
    require(effect.parameters().depth == 1.0F,
            "depth above unity did not clamp to one");
    effect.set_depth(std::numeric_limits<float>::infinity());
    require(effect.parameters().depth == 1.0F,
            "non-finite depth changed the last valid value");

    effect.set_crossover_hz(1.0F);
    require(effect.parameters().crossover_hz == 80.0F,
            "crossover lower bound was not enforced");
    effect.set_crossover_hz(20000.0F);
    require(effect.parameters().crossover_hz == 8000.0F,
            "crossover upper bound was not enforced at 48 kHz");
    effect.set_crossover_hz(std::numeric_limits<float>::quiet_NaN());
    require(effect.parameters().crossover_hz == 8000.0F,
            "non-finite crossover changed the last valid value");

    effect.set_smoothing_ms(-1.0F);
    require(effect.parameters().smoothing_ms == 0.0F,
            "negative smoothing did not clamp to immediate response");
    effect.set_smoothing_ms(2000.0F);
    require(effect.parameters().smoothing_ms == 1000.0F,
            "smoothing upper bound was not enforced");
    effect.set_smoothing_ms(std::numeric_limits<float>::quiet_NaN());
    require(effect.parameters().smoothing_ms == 10.0F,
            "non-finite smoothing did not restore the safe default");

    tabletfx::HarmonicTremolo low_rate(8000.0F);
    low_rate.set_crossover_hz(8000.0F);
    require(low_rate.parameters().crossover_hz <= 3600.0F,
            "crossover exceeded the sample-rate-relative bound");
}

void test_complementary_modulation_direction() {
    constexpr std::size_t first_quarter_start = 10800;
    constexpr std::size_t first_quarter_end = 13200;
    constexpr std::size_t third_quarter_start = 34800;
    constexpr std::size_t third_quarter_end = 37200;

    const auto low = render_mono_tone(60.0F, 700.0F);
    const auto high = render_mono_tone(10000.0F, 700.0F);
    const float low_at_positive_lfo = rms(low, first_quarter_start,
                                          first_quarter_end);
    const float low_at_negative_lfo = rms(low, third_quarter_start,
                                          third_quarter_end);
    const float high_at_positive_lfo = rms(high, first_quarter_start,
                                           first_quarter_end);
    const float high_at_negative_lfo = rms(high, third_quarter_start,
                                           third_quarter_end);

    require(low_at_positive_lfo > low_at_negative_lfo * 5.0F,
            "positive LFO phase did not preserve the low band over the high band");
    require(high_at_negative_lfo > high_at_positive_lfo * 5.0F,
            "negative LFO phase did not preserve the high band over the low band");
}

void test_crossover_changes_band_assignment() {
    constexpr std::size_t window_start = 10800;
    constexpr std::size_t window_end = 13200;
    const auto low_crossover = render_mono_tone(1000.0F, 200.0F);
    const auto high_crossover = render_mono_tone(1000.0F, 3000.0F);
    const float low_cut_result = rms(low_crossover, window_start, window_end);
    const float high_cut_result = rms(high_crossover, window_start, window_end);
    require(high_cut_result > low_cut_result * 2.0F,
            "crossover frequency did not change how much of the test tone occupied the low band");

    tabletfx::HarmonicTremoloParameters parameters;
    parameters.depth = 0.0F;
    parameters.crossover_hz = 1200.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::HarmonicTremolo neutral(48000.0F, parameters);
    std::array<float, 8> samples{0.25F, -0.5F, 0.75F, -1.0F,
                                 0.125F, -0.25F, 0.4F, -0.2F};
    const auto original = samples;
    neutral.process(samples, 2);
    require(samples == original,
            "zero depth did not provide exact unity passthrough");
}

void test_stereo_alternation_and_mono_policy() {
    constexpr float sample_rate = 48000.0F;
    tabletfx::HarmonicTremoloParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.crossover_hz = 700.0F;
    parameters.smoothing_ms = 0.0F;
    parameters.stereo_alternation = true;
    tabletfx::HarmonicTremolo stereo(sample_rate, parameters);
    std::vector<float> samples(48000 * 2);
    for (std::size_t i = 0; i < samples.size() / 2; ++i) {
        const float tone = std::sin(2.0F * 3.14159265358979323846F * 60.0F *
                                    static_cast<float>(i) / sample_rate);
        samples[i * 2] = tone;
        samples[i * 2 + 1] = tone;
    }
    stereo.process(samples, 2);
    std::vector<float> left(48000);
    std::vector<float> right(48000);
    for (std::size_t i = 0; i < left.size(); ++i) {
        left[i] = samples[i * 2];
        right[i] = samples[i * 2 + 1];
    }

    const float left_positive = rms(left, 10800, 13200);
    const float right_positive = rms(right, 10800, 13200);
    const float left_negative = rms(left, 34800, 37200);
    const float right_negative = rms(right, 34800, 37200);
    require(left_positive > right_positive * 4.0F,
            "stereo alternation did not put the low band on the left in the first phase");
    require(right_negative > left_negative * 4.0F,
            "stereo alternation did not move the low band to the right in the next phase");

    tabletfx::HarmonicTremolo mono(sample_rate, parameters);
    std::vector<float> mono_samples(24000, 0.5F);
    mono.process(mono_samples, 1);
    require(mono_samples.size() == 24000,
            "mono processing changed the caller's mono channel layout");
    for (float sample : mono_samples) {
        require(std::isfinite(sample), "mono processing emitted a non-finite sample");
    }

    tabletfx::HarmonicTremolo stereo_linked(sample_rate, parameters);
    stereo_linked.set_stereo_alternation(false);
    std::array<float, 2> linked_input{0.75F, 0.75F};
    stereo_linked.process(linked_input, 2);
    require(std::abs(linked_input[0] - linked_input[1]) < 1.0e-6F,
            "disabled stereo alternation should keep dual-mono channels linked");

    std::array<float, 3> unsupported{0.25F, -0.5F, 0.75F};
    const auto original = unsupported;
    stereo_linked.process(unsupported, 3);
    require(unsupported == original,
            "unsupported multichannel layout should remain untouched");
}

void test_tempo_sync_reset_and_variable_blocks() {
    tabletfx::HarmonicTremolo effect;
    effect.set_tempo_sync(true, 120.0F, 1.0F);
    require(std::abs(effect.effective_rate_hz() - 2.0F) < 1.0e-6F,
            "quarter-note sync at 120 BPM should be 2 Hz");
    effect.set_tempo_sync(true, 120.0F, 0.5F);
    require(std::abs(effect.effective_rate_hz() - 4.0F) < 1.0e-6F,
            "eighth-note sync at 120 BPM should be 4 Hz");

    tabletfx::HarmonicTremoloParameters parameters;
    parameters.rate_hz = 7.3F;
    parameters.depth = 0.73F;
    parameters.crossover_hz = 900.0F;
    tabletfx::HarmonicTremolo whole(44100.0F, parameters);
    tabletfx::HarmonicTremolo chunked(44100.0F, parameters);
    std::vector<float> expected(8194);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        expected[i] = 0.6F * std::sin(static_cast<float>(i) * 0.013F);
    }
    auto actual = expected;
    whole.process(expected, 1);
    const std::array<std::size_t, 5> block_sizes{1, 127, 53, 300, 17};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < actual.size()) {
        const std::size_t size = std::min(block_sizes[block % block_sizes.size()],
                                          actual.size() - offset);
        chunked.process(std::span<float>(actual).subspan(offset, size), 1);
        offset += size;
        ++block;
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        require(std::abs(expected[i] - actual[i]) < 2.0e-6F,
                "irregular blocks changed harmonic tremolo phase or filter state");
    }

    tabletfx::HarmonicTremolo replay(48000.0F, parameters);
    std::array<float, 4> input{0.2F, -0.4F, 0.6F, -0.8F};
    auto first = input;
    replay.process(first, 1);
    replay.reset();
    auto second = input;
    replay.process(second, 1);
    for (std::size_t i = 0; i < first.size(); ++i) {
        require(std::abs(first[i] - second[i]) < 1.0e-7F,
                "reset did not restore deterministic LFO and filter state");
    }
}

void test_invalid_audio_and_zero_process_allocations() {
    tabletfx::HarmonicTremolo effect;
    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        0.25F,
        std::numeric_limits<float>::infinity(),
        -0.5F,
        std::numeric_limits<float>::max(),
        0.75F,
        -std::numeric_limits<float>::max(),
        -0.25F,
    };
    effect.process(invalid, 2);
    for (float sample : invalid) {
        require(std::isfinite(sample),
                "invalid/extreme input did not recover to finite output");
    }

    std::array<float, 256> block{};
    for (std::size_t i = 0; i < block.size(); ++i) {
        block[i] = 0.5F * std::sin(static_cast<float>(i) * 0.07F);
    }
    effect.reset();
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        effect.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before,
            "harmonic tremolo allocated memory during process()");
    for (float sample : block) {
        require(std::isfinite(sample),
                "harmonic tremolo emitted non-finite audio");
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
        test_defaults_bounds_and_memory();
        test_complementary_modulation_direction();
        test_crossover_changes_band_assignment();
        test_stereo_alternation_and_mono_policy();
        test_tempo_sync_reset_and_variable_blocks();
        test_invalid_audio_and_zero_process_allocations();
        std::cout << "All TabletFX harmonic tremolo tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
