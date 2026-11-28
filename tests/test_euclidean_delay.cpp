#include "tabletfx/effects/euclidean_delay.hpp"

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

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float actual, float expected, float tolerance = 1.0e-5F) {
    return std::abs(actual - expected) <= tolerance;
}

tabletfx::EuclideanDelayParameters impulse_parameters() {
    tabletfx::EuclideanDelayParameters parameters;
    parameters.step_time_ms = 10.0F;
    parameters.steps = 8U;
    parameters.pulses = 3U;
    parameters.rotation = 0U;
    parameters.feedback = 0.5F;
    parameters.mix = 1.0F;
    parameters.level_db = 0.0F;
    return parameters;
}

std::vector<float> render_steady_tone(std::uint8_t rotation) {
    auto parameters = impulse_parameters();
    parameters.rotation = rotation;
    parameters.feedback = 0.0F;
    tabletfx::EuclideanDelay effect(8000.0F, parameters);
    std::vector<float> audio(800U, 0.0F);
    constexpr float kTwoPi = 6.28318530717958647692F;
    constexpr float kCycleFrames = 80.0F;
    for (std::size_t frame = 0U; frame < audio.size(); ++frame) {
        audio[frame] = 0.5F * std::sin(
            kTwoPi * static_cast<float>(frame) / kCycleFrames);
    }
    effect.process(audio, 1U);
    return audio;
}

float step_window_rms(const std::vector<float>& audio,
                      std::size_t step) {
    constexpr std::size_t kFirstBoundary = 80U;
    constexpr std::size_t kStepFrames = 80U;
    constexpr std::size_t kSettledOffset = 20U;
    constexpr std::size_t kWindowFrames = 40U;
    const std::size_t first = kFirstBoundary + step * kStepFrames +
                              kSettledOffset;
    double sum_squares = 0.0;
    for (std::size_t frame = first; frame < first + kWindowFrames; ++frame) {
        const double sample = audio[frame];
        sum_squares += sample * sample;
    }
    return static_cast<float>(std::sqrt(sum_squares / kWindowFrames));
}

void test_defaults_storage_and_disabled_mix_identity() {
    tabletfx::EuclideanDelay effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "euclidean-delay",
            "Euclidean Delay effect id changed");
    require(defaults.step_time_ms == 125.0F && defaults.steps == 8U &&
                defaults.pulses == 3U && defaults.rotation == 0U &&
                defaults.feedback == 0.25F && defaults.mix == 0.25F &&
                defaults.level_db == 0.0F && defaults.low_cut_hz == 20.0F &&
                defaults.high_cut_hz == 20000.0F && !defaults.trails,
            "project-authored defaults changed unexpectedly");
    require(effect.fixed_latency_frames() == 0U,
            "the delay must not add fixed dry-path latency");
    require(effect.memory_bytes() <= sizeof(effect) + 2U * 1024U * 1024U,
            "stereo ring exceeded the fixed memory cap");
    require(near(effect.maximum_step_time_ms(), 2000.0F, 0.05F),
            "48 kHz instance should expose the requested two-second step cap");

    auto dry_parameters = defaults;
    dry_parameters.mix = 0.0F;
    tabletfx::EuclideanDelay dry(48000.0F, dry_parameters);
    std::array<float, 10U> audio{0.1F, -0.2F, 0.3F, -0.4F, 0.0F,
                                 0.8F, -0.7F, 0.2F, 0.05F, -0.05F};
    const auto original = audio;
    dry.process(audio, 1U);
    require(audio == original,
            "Mix 0 at unity Level must preserve the input exactly");
}

void test_euclidean_pattern_distributes_and_rotates_hits() {
    const auto unrotated = render_steady_tone(0U);
    const auto rotated = render_steady_tone(1U);

    for (std::size_t step = 0U; step < 8U; ++step) {
        const float unrotated_rms = step_window_rms(unrotated, step);
        const float rotated_rms = step_window_rms(rotated, step);
        const bool unrotated_hit = step == 0U || step == 3U || step == 6U;
        const bool rotated_hit = step == 1U || step == 4U || step == 7U;
        require(unrotated_hit ? unrotated_rms > 0.2F
                              : unrotated_rms < 1.0e-5F,
                "three pulses over eight steps must occupy only slots 0, 3, and 6");
        require(rotated_hit ? rotated_rms > 0.2F
                            : rotated_rms < 1.0e-5F,
                "one-step rotation must move hits to slots 1, 4, and 7 only");
    }
}

void test_zero_pulses_silences_wet_path_and_full_density_repeats() {
    auto parameters = impulse_parameters();
    parameters.pulses = 0U;
    tabletfx::EuclideanDelay silent(8000.0F, parameters);
    std::array<float, 256U> audio{};
    audio[0U] = 1.0F;
    silent.process(audio, 1U);
    require(std::all_of(audio.begin(), audio.end(), [](float sample) {
                return std::abs(sample) < 1.0e-6F;
            }),
            "zero pulses should mute the fully wet output");

    parameters.steps = 1U;
    parameters.pulses = 1U;
    parameters.feedback = 0.5F;
    tabletfx::EuclideanDelay dense(8000.0F, parameters);
    std::array<float, 320U> echoes{};
    echoes[0U] = 1.0F;
    dense.process(echoes, 1U);
    require(echoes[80U] > 0.01F && echoes[160U] > 0.01F &&
                echoes[240U] > 0.001F,
            "one pulse per one-step cycle should produce repeated echoes");
}

void test_parameters_are_bounded_and_nonfinite_values_keep_previous() {
    tabletfx::EuclideanDelay effect(8000.0F);
    auto parameters = effect.parameters();
    parameters.step_time_ms = 0.0F;
    parameters.steps = 255U;
    parameters.pulses = 255U;
    parameters.rotation = 255U;
    parameters.feedback = 5.0F;
    parameters.mix = -1.0F;
    parameters.level_db = 99.0F;
    effect.configure(parameters);
    auto bounded = effect.parameters();
    require(bounded.step_time_ms == 10.0F && bounded.steps == 16U &&
                bounded.pulses == 16U && bounded.rotation == 15U &&
                bounded.feedback == 0.85F && bounded.mix == 0.0F &&
                bounded.level_db == 12.0F,
            "out-of-range controls did not clamp to the project limits");

    parameters = bounded;
    parameters.step_time_ms = std::numeric_limits<float>::quiet_NaN();
    parameters.feedback = std::numeric_limits<float>::infinity();
    parameters.mix = -std::numeric_limits<float>::infinity();
    parameters.level_db = std::numeric_limits<float>::quiet_NaN();
    effect.configure(parameters);
    const auto retained = effect.parameters();
    require(retained.step_time_ms == bounded.step_time_ms &&
                retained.feedback == bounded.feedback &&
                retained.mix == bounded.mix &&
                retained.level_db == bounded.level_db,
            "non-finite controls must retain the previous valid values");

    auto over_memory = bounded;
    over_memory.step_time_ms = 2000.0F;
    tabletfx::EuclideanDelay high_rate(384000.0F, over_memory);
    require(high_rate.maximum_step_time_ms() < 700.0F &&
                high_rate.parameters().step_time_ms <=
                    high_rate.maximum_step_time_ms() &&
                high_rate.memory_bytes() <=
                    sizeof(high_rate) + 2U * 1024U * 1024U,
            "high-rate delay duration must respect the fixed ring cap");
}

void test_stereo_isolation_and_reset_clear_echoes() {
    auto parameters = impulse_parameters();
    parameters.steps = 1U;
    parameters.pulses = 1U;
    parameters.feedback = 0.0F;
    tabletfx::EuclideanDelay effect(8000.0F, parameters);
    std::array<float, 256U> stereo{};
    stereo[0U] = 1.0F;
    effect.process(stereo, 2U);
    require(stereo[80U * 2U] > 0.01F &&
                std::abs(stereo[80U * 2U + 1U]) < 1.0e-7F,
            "stereo channels should have independent delay histories");

    effect.reset();
    std::array<float, 256U> silence{};
    effect.process(silence, 2U);
    require(std::all_of(silence.begin(), silence.end(), [](float sample) {
                return std::abs(sample) < 1.0e-7F;
            }),
            "reset must clear buffered echoes and restart the gate clock");
}

void test_irregular_partitioning_matches_single_block_render() {
    auto parameters = impulse_parameters();
    parameters.step_time_ms = 17.5F;
    parameters.steps = 7U;
    parameters.pulses = 3U;
    parameters.rotation = 2U;
    parameters.mix = 0.63F;

    tabletfx::EuclideanDelay whole(8000.0F, parameters);
    tabletfx::EuclideanDelay partitioned(8000.0F, parameters);
    std::vector<float> reference(3001U * 2U);
    for (std::size_t frame = 0U; frame < reference.size() / 2U; ++frame) {
        const float phase = static_cast<float>(frame) * 0.071F;
        reference[frame * 2U] = 0.4F * std::sin(phase);
        reference[frame * 2U + 1U] = 0.3F * std::sin(phase + 0.2F);
    }
    auto split = reference;
    whole.process(reference, 2U);

    constexpr std::array<std::size_t, 9U> block_sizes{
        1U, 17U, 64U, 3U, 129U, 255U, 512U, 7U, 2013U};
    std::size_t offset_frames = 0U;
    for (const auto block_frames : block_sizes) {
        const std::size_t bounded_frames =
            std::min(block_frames, split.size() / 2U - offset_frames);
        if (bounded_frames == 0U) break;
        const std::size_t first = offset_frames * 2U;
        partitioned.process(
            std::span<float>(split.data() + first, bounded_frames * 2U), 2U);
        offset_frames += bounded_frames;
    }
    if (offset_frames < split.size() / 2U) {
        const std::size_t first = offset_frames * 2U;
        partitioned.process(
            std::span<float>(split.data() + first, split.size() - first), 2U);
    }

    for (std::size_t index = 0U; index < reference.size(); ++index) {
        require(near(reference[index], split[index], 1.0e-6F),
                "changing callback partitions altered delay output");
    }
}

void test_nonfinite_input_recovers_and_processing_does_not_allocate() {
    tabletfx::EuclideanDelay effect(8000.0F, impulse_parameters());
    std::array<float, 64U> audio{};
    audio[0U] = std::numeric_limits<float>::quiet_NaN();
    audio[1U] = std::numeric_limits<float>::infinity();
    audio[2U] = 1000000.0F;
    count_allocations.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    effect.process(audio, 2U);
    count_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "process allocated memory in the audio callback");
    require(std::all_of(audio.begin(), audio.end(), [](float sample) {
                return std::isfinite(sample) && std::abs(sample) <= 8.0F;
            }),
            "invalid or extreme input poisoned the delay output");

    effect.reset();
    std::array<float, 64U> recovered{};
    recovered.fill(0.125F);
    effect.process(recovered, 2U);
    require(std::all_of(recovered.begin(), recovered.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "processor did not recover to finite output after reset");
}

void test_invalid_sample_rate_is_rejected() {
    bool rejected = false;
    try {
        tabletfx::EuclideanDelay invalid(1000.0F);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "sample rate below the supported range was accepted");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0U ? 1U : size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0U ? 1U : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

void test_project_authored_cuts_and_bypass_trails() {
    const auto render_tone = [](float frequency_hz, float low_cut_hz,
                                float high_cut_hz) {
        auto parameters = impulse_parameters();
        parameters.steps = 1U;
        parameters.pulses = 1U;
        parameters.feedback = 0.0F;
        parameters.mix = 1.0F;
        parameters.low_cut_hz = low_cut_hz;
        parameters.high_cut_hz = high_cut_hz;
        tabletfx::EuclideanDelay effect(8000.0F, parameters);
        std::array<float, 1024U> audio{};
        constexpr float two_pi = 6.28318530717958647692F;
        for (std::size_t frame = 0U; frame < audio.size(); ++frame) {
            audio[frame] = 0.5F * std::sin(
                two_pi * frequency_hz * static_cast<float>(frame) / 8000.0F);
        }
        effect.process(audio, 1U);
        double sum_squares = 0.0;
        std::size_t count = 0U;
        for (std::size_t frame = 160U; frame < 960U; ++frame) {
            sum_squares += static_cast<double>(audio[frame]) * audio[frame];
            ++count;
        }
        return static_cast<float>(std::sqrt(sum_squares / count));
    };

    const float low_tone_open = render_tone(100.0F, 20.0F, 20000.0F);
    const float low_tone_cut = render_tone(100.0F, 1000.0F, 20000.0F);
    require(low_tone_cut < low_tone_open * 0.35F,
            "Low Cut must attenuate low-frequency delay repeats");
    const float high_tone_open = render_tone(2000.0F, 20.0F, 20000.0F);
    const float high_tone_cut = render_tone(2000.0F, 20.0F, 1000.0F);
    require(high_tone_cut < high_tone_open * 0.7F,
            "High Cut must attenuate high-frequency delay repeats");

    auto parameters = impulse_parameters();
    parameters.steps = 1U;
    parameters.pulses = 1U;
    parameters.trails = true;
    tabletfx::EuclideanDelay tail_only(8000.0F, parameters);
    tabletfx::EuclideanDelay dry_plus_tail(8000.0F, parameters);
    std::array<float, 256U> excitation{};
    excitation[0U] = 1.0F;
    auto other_excitation = excitation;
    tail_only.process(excitation, 1U);
    dry_plus_tail.process(other_excitation, 1U);

    std::array<float, 256U> tail{};
    std::array<float, 256U> mixed{};
    for (std::size_t frame = 0U; frame < mixed.size(); ++frame) {
        mixed[frame] = 0.2F;
    }
    const auto dry = mixed;
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    tail_only.process_bypassed(tail, 1U);
    dry_plus_tail.process_bypassed(mixed, 1U);
    count_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "bypassed Trails processing allocated memory");
    require(std::any_of(tail.begin(), tail.end(), [](float sample) {
                return std::abs(sample) > 1.0e-4F;
            }),
            "Trails on must emit the stored delay tail while bypassed");
    for (std::size_t frame = 0U; frame < mixed.size(); ++frame) {
        require(near(mixed[frame], dry[frame] + tail[frame]),
                "Trails must add the stored wet tail to unity dry bypass");
    }

    std::array<float, 256U> tail_after_dry{};
    std::array<float, 256U> tail_without_dry{};
    tail_only.process_bypassed(tail_without_dry, 1U);
    dry_plus_tail.process_bypassed(tail_after_dry, 1U);
    require(tail_after_dry == tail_without_dry,
            "bypassed dry input must not feed back into Trails");

    parameters.trails = false;
    tabletfx::EuclideanDelay no_trails(8000.0F, parameters);
    auto no_trails_excitation = excitation;
    no_trails.process(no_trails_excitation, 1U);
    std::array<float, 256U> bypass_dry{};
    for (std::size_t frame = 0U; frame < bypass_dry.size(); ++frame) {
        bypass_dry[frame] = 0.2F;
    }
    const auto bypass_original = bypass_dry;
    no_trails.process_bypassed(bypass_dry, 1U);
    require(bypass_dry == bypass_original,
            "Trails off must preserve exact dry bypass identity");
}

int main() {
    try {
        test_defaults_storage_and_disabled_mix_identity();
        test_euclidean_pattern_distributes_and_rotates_hits();
        test_zero_pulses_silences_wet_path_and_full_density_repeats();
        test_parameters_are_bounded_and_nonfinite_values_keep_previous();
        test_stereo_isolation_and_reset_clear_echoes();
        test_irregular_partitioning_matches_single_block_render();
        test_nonfinite_input_recovers_and_processing_does_not_allocate();
        test_project_authored_cuts_and_bypass_trails();
        test_invalid_sample_rate_is_rejected();
        std::cout << "All Euclidean Delay tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Euclidean Delay test failure: " << error.what() << '\n';
        return 1;
    }
}
