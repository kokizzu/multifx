#include "tabletfx/effects/pugilist.hpp"

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
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float actual, float expected, float tolerance = 2.0e-5F) {
    return std::abs(actual - expected) <= tolerance;
}

std::vector<float> test_signal(std::size_t frames,
                               float frequency = 220.0F) {
    std::vector<float> samples(frames);
    constexpr float sample_rate = 48000.0F;
    constexpr float amplitude = 0.35F;
    constexpr float two_pi = 6.28318530717958647692F;
    for (std::size_t i = 0; i < frames; ++i) {
        samples[i] = amplitude * std::sin(
            two_pi * frequency * static_cast<float>(i) / sample_rate);
    }
    return samples;
}

double rms(const std::vector<float>& samples, std::size_t skip = 0U) {
    double sum = 0.0;
    for (std::size_t i = skip; i < samples.size(); ++i) {
        const double value = samples[i];
        sum += value * value;
    }
    return std::sqrt(sum / static_cast<double>(samples.size() - skip));
}

void test_defaults_bounds_and_memory() {
    tabletfx::DualEngineDistortion effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "dual-engine-distortion",
            "dual-engine distortion stable ID changed");
    require(defaults.gain_a == 0.25F && defaults.tone_a == 0.7F &&
                defaults.gain_b == 0.6F && defaults.tone_b == 0.5F &&
                defaults.blend == 0.5F &&
                defaults.mode == tabletfx::DualEngineDistortionMode::Blend &&
                !defaults.bass_boost && defaults.level_db == 0.0F &&
                defaults.smoothing_ms == 10.0F,
            "project-authored dual-engine defaults changed");
    require(effect.memory_bytes() == sizeof(effect),
            "dual-engine memory report is inaccurate");

    tabletfx::DualEngineDistortionParameters p;
    p.gain_a = -1.0F;
    p.tone_a = 2.0F;
    p.gain_b = std::numeric_limits<float>::quiet_NaN();
    p.tone_b = std::numeric_limits<float>::infinity();
    p.blend = 3.0F;
    p.mode = static_cast<tabletfx::DualEngineDistortionMode>(255);
    p.level_db = 100.0F;
    p.smoothing_ms = -10.0F;
    effect.set_parameters(p);
    const auto bounded = effect.parameters();
    require(bounded.gain_a == 0.0F && bounded.tone_a == 1.0F &&
                bounded.gain_b == 0.6F && bounded.tone_b == 0.5F &&
                bounded.blend == 1.0F &&
                bounded.mode == tabletfx::DualEngineDistortionMode::Blend &&
                bounded.level_db == 12.0F && bounded.smoothing_ms == 0.0F,
            "project-authored bounds or finite fallbacks failed");
}

void test_parallel_blend_endpoints_and_midpoint() {
    tabletfx::DualEngineDistortionParameters p;
    p.gain_a = 0.15F;
    p.tone_a = 0.9F;
    p.gain_b = 0.85F;
    p.tone_b = 0.25F;
    p.level_db = 0.0F;
    p.smoothing_ms = 0.0F;

    auto a_only_parameters = p;
    a_only_parameters.blend = 0.0F;
    tabletfx::DualEngineDistortion a_only(48000.0F, a_only_parameters);
    auto b_changed_parameters = a_only_parameters;
    b_changed_parameters.gain_b = 0.1F;
    b_changed_parameters.tone_b = 1.0F;
    tabletfx::DualEngineDistortion a_only_b_changed(48000.0F,
                                                     b_changed_parameters);
    auto b_only_parameters = p;
    b_only_parameters.blend = 1.0F;
    tabletfx::DualEngineDistortion b_only(48000.0F, b_only_parameters);
    auto a_changed_parameters = b_only_parameters;
    a_changed_parameters.gain_a = 1.0F;
    a_changed_parameters.tone_a = 0.0F;
    tabletfx::DualEngineDistortion b_only_a_changed(48000.0F,
                                                    a_changed_parameters);
    auto midpoint_parameters = p;
    midpoint_parameters.blend = 0.5F;
    tabletfx::DualEngineDistortion midpoint(48000.0F, midpoint_parameters);

    const auto original = test_signal(12000U);
    auto output_a = original;
    auto output_a_changed = original;
    auto output_b = original;
    auto output_b_changed = original;
    auto output_mid = original;
    a_only.process(output_a, 1);
    a_only_b_changed.process(output_a_changed, 1);
    b_only.process(output_b, 1);
    b_only_a_changed.process(output_b_changed, 1);
    midpoint.process(output_mid, 1);

    double endpoint_difference = 0.0;
    for (std::size_t i = 0; i < original.size(); ++i) {
        require(near(output_a[i], output_a_changed[i]),
                "blend endpoint A should ignore engine B settings");
        require(near(output_b[i], output_b_changed[i]),
                "blend endpoint B should ignore engine A settings");
        require(near(output_mid[i], 0.5F * (output_a[i] + output_b[i]),
                     5.0e-5F),
                "blend midpoint was not the linear A/B mix");
        endpoint_difference += std::abs(
            static_cast<double>(output_a[i] - output_b[i]));
    }
    require(endpoint_difference > 1.0,
            "independent engine controls did not produce distinct outputs");
}

void test_series_routing_and_bass_boost() {
    tabletfx::DualEngineDistortionParameters p;
    p.gain_a = 0.25F;
    p.tone_a = 0.8F;
    p.gain_b = 0.75F;
    p.tone_b = 0.2F;
    p.blend = 0.0F;
    p.level_db = 0.0F;
    p.smoothing_ms = 0.0F;
    auto parallel = p;
    auto series = p;
    series.mode = tabletfx::DualEngineDistortionMode::Series;

    tabletfx::DualEngineDistortion blend_effect(48000.0F, parallel);
    tabletfx::DualEngineDistortion series_effect(48000.0F, series);
    auto blend_signal = test_signal(12000U);
    auto series_signal = blend_signal;
    blend_effect.process(blend_signal, 1);
    series_effect.process(series_signal, 1);

    double routing_difference = 0.0;
    for (std::size_t i = 0; i < blend_signal.size(); ++i) {
        require(std::isfinite(series_signal[i]),
                "series route generated non-finite audio");
        routing_difference += std::abs(
            static_cast<double>(blend_signal[i] - series_signal[i]));
    }
    require(routing_difference > 1.0,
            "series route did not cascade engine A into engine B");

    tabletfx::DualEngineDistortionParameters low_boost = p;
    low_boost.gain_a = 0.25F;
    low_boost.gain_b = 0.25F;
    low_boost.tone_a = 1.0F;
    low_boost.tone_b = 1.0F;
    low_boost.blend = 0.5F;
    low_boost.smoothing_ms = 0.0F;
    auto with_boost = low_boost;
    with_boost.bass_boost = true;
    tabletfx::DualEngineDistortion normal(48000.0F, low_boost);
    tabletfx::DualEngineDistortion boosted(48000.0F, with_boost);
    std::vector<float> low_frequency(24000U);
    constexpr float two_pi = 6.28318530717958647692F;
    for (std::size_t i = 0; i < low_frequency.size(); ++i) {
        low_frequency[i] = 0.1F * std::sin(
            two_pi * 100.0F * static_cast<float>(i) / 48000.0F);
    }
    auto boosted_signal = low_frequency;
    normal.process(low_frequency, 1);
    boosted.process(boosted_signal, 1);
    require(rms(boosted_signal, 6000U) > rms(low_frequency, 6000U),
            "bass boost did not increase low-frequency signal level");
}

void test_tone_control_and_stereo_channel_independence() {
    tabletfx::DualEngineDistortionParameters dark;
    dark.gain_a = 0.1F;
    dark.gain_b = 0.1F;
    dark.tone_a = 0.0F;
    dark.tone_b = 0.0F;
    dark.blend = 0.5F;
    dark.level_db = 0.0F;
    dark.smoothing_ms = 0.0F;
    auto bright = dark;
    bright.tone_a = 1.0F;
    bright.tone_b = 1.0F;
    tabletfx::DualEngineDistortion dark_effect(48000.0F, dark);
    tabletfx::DualEngineDistortion bright_effect(48000.0F, bright);
    auto dark_signal = test_signal(24000U, 4000.0F);
    auto bright_signal = dark_signal;
    dark_effect.process(dark_signal, 1);
    bright_effect.process(bright_signal, 1);
    require(rms(bright_signal, 6000U) > rms(dark_signal, 6000U) * 1.15,
            "tone controls did not produce darker and brighter responses");

    tabletfx::DualEngineDistortion stereo(48000.0F, bright);
    std::array<float, 8> pair{0.2F, -0.2F, 0.4F, -0.4F,
                              -0.6F, 0.6F, 0.8F, -0.8F};
    stereo.process(pair, 2);
    for (std::size_t i = 0; i < pair.size(); i += 2U) {
        require(near(pair[i], -pair[i + 1U]),
                "stereo channels should maintain independent symmetric state");
    }
}

void test_reset_smoothing_finite_output_and_no_allocations() {
    tabletfx::DualEngineDistortionParameters p;
    p.smoothing_ms = 1.0F;
    p.gain_a = 0.0F;
    p.gain_b = 0.0F;
    tabletfx::DualEngineDistortion effect(8000.0F, p);
    p.gain_a = 1.0F;
    p.gain_b = 1.0F;
    p.bass_boost = true;
    effect.set_parameters(p);

    std::array<float, 16> audio{};
    audio.fill(0.5F);
    allocation_count = 0U;
    count_allocations = true;
    effect.process(audio, 2);
    count_allocations = false;
    require(allocation_count == 0U,
            "dual-engine processing allocated memory");
    for (float sample : audio) {
        require(std::isfinite(sample),
                "dual-engine output contains a non-finite sample");
    }

    effect.reset();
    auto first = test_signal(128U);
    auto second = first;
    effect.process(first, 1);
    effect.reset();
    effect.process(second, 1);
    require(first == second,
            "reset did not restore deterministic filter state");
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
        test_parallel_blend_endpoints_and_midpoint();
        test_series_routing_and_bass_boost();
        test_tone_control_and_stereo_channel_independence();
        test_reset_smoothing_finite_output_and_no_allocations();
        std::cout << "Dual-engine distortion tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        count_allocations = false;
        std::cerr << "Dual-engine distortion test failure: "
                  << exception.what() << '\n';
        return 1;
    }
}
