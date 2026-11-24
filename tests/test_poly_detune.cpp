#include "tabletfx/effects/poly_detune.hpp"

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
#include <vector>

namespace {

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

double squared_difference(std::span<const float> a,
                          std::span<const float> b,
                          std::size_t offset = 0U) {
    double sum = 0.0;
    for (std::size_t i = offset; i < a.size(); ++i) {
        const double difference = static_cast<double>(a[i]) - b[i];
        sum += difference * difference;
    }
    return sum;
}

void test_defaults_bounds_and_memory() {
    tabletfx::PolyDetune effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "poly-detune", "Poly Detune stable ID changed");
    require(defaults.rate_hz == 0.25F && defaults.depth_ms == 0.75F &&
                defaults.voice_count == 2U && defaults.width == 0.75F &&
                defaults.mix == 0.25F && defaults.level_db == 0.0F &&
                defaults.smoothing_ms == 10.0F,
            "project-authored Poly Detune defaults changed");
    require(effect.memory_bytes() == sizeof(effect) + 434U * 2U * sizeof(float),
            "Poly Detune shared-ring memory report is incorrect at 48 kHz");

    auto parameters = defaults;
    parameters.rate_hz = 100.0F;
    parameters.depth_ms = 100.0F;
    parameters.voice_count = 0U;
    parameters.width = -1.0F;
    parameters.mix = 2.0F;
    parameters.level_db = 30.0F;
    parameters.smoothing_ms = 2000.0F;
    effect.set_parameters(parameters);
    const auto bounded = effect.parameters();
    require(bounded.rate_hz == 2.0F && bounded.depth_ms == 2.0F &&
                bounded.voice_count == 2U && bounded.width == 0.0F &&
                bounded.mix == 1.0F && bounded.level_db == 12.0F &&
                bounded.smoothing_ms == 1000.0F,
            "Poly Detune controls did not clamp to project bounds");

    parameters = defaults;
    parameters.rate_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.depth_ms = std::numeric_limits<float>::infinity();
    parameters.width = std::numeric_limits<float>::quiet_NaN();
    parameters.mix = std::numeric_limits<float>::infinity();
    parameters.level_db = std::numeric_limits<float>::quiet_NaN();
    parameters.smoothing_ms = std::numeric_limits<float>::infinity();
    effect.set_parameters(parameters);
    const auto safe = effect.parameters();
    require(safe.rate_hz == defaults.rate_hz &&
                safe.depth_ms == defaults.depth_ms &&
                safe.width == defaults.width && safe.mix == defaults.mix &&
                safe.level_db == defaults.level_db &&
                safe.smoothing_ms == defaults.smoothing_ms,
            "non-finite Poly Detune controls did not use safe defaults");
}

void test_delay_timing_mono_stereo_and_exact_dry() {
    tabletfx::PolyDetuneParameters parameters;
    parameters.rate_hz = 0.25F;
    parameters.depth_ms = 0.0F;
    parameters.voice_count = 1U;
    parameters.width = 1.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::PolyDetune mono(8000.0F, 1U, parameters);
    std::array<float, 64> impulse{};
    impulse[0] = 1.0F;
    mono.process(impulse, 1U);
    require(impulse[40] == 1.0F,
            "wet impulse did not appear at the project 5 ms base delay");
    require(std::all_of(impulse.begin(), impulse.begin() + 40,
                        [](float sample) { return sample == 0.0F; }),
            "wet-only output appeared before its short delay");

    parameters.mix = 0.0F;
    tabletfx::PolyDetune dry(48000.0F, 2U, parameters);
    std::array<float, 16> dry_block{0.1F, -0.2F, 0.3F, -0.4F,
                                    0.5F, -0.6F, 0.7F, -0.8F,
                                    0.125F, -0.25F, 0.375F, -0.5F,
                                    0.625F, -0.75F, 0.875F, -1.0F};
    const auto original = dry_block;
    dry.process(dry_block, 2U);
    require(dry_block == original, "zero-mix Poly Detune was not exact dry");

    parameters.mix = 1.0F;
    parameters.depth_ms = 0.0F;
    tabletfx::PolyDetune stereo(8000.0F, 2U, parameters);
    std::array<float, 128> stereo_impulse{};
    stereo_impulse[0] = 1.0F;
    stereo.process(stereo_impulse, 2U);
    require(stereo_impulse[80] == 1.0F,
            "stereo left wet impulse missed the configured delay");
    for (std::size_t i = 1U; i < stereo_impulse.size(); i += 2U) {
        require(stereo_impulse[i] == 0.0F,
                "Poly Detune leaked audio between stereo channels");
    }

    const auto before = stereo_impulse;
    stereo.process(stereo_impulse, 1U);
    require(stereo_impulse == before,
            "mismatched channel layout should be left untouched");
    stereo.process(stereo_impulse, 0U);
    require(stereo_impulse == before,
            "zero-channel processing should be left untouched");
}

std::vector<float> render_mono(float rate_hz, float depth_ms,
                              std::uint8_t voice_count = 2U) {
    tabletfx::PolyDetuneParameters parameters;
    parameters.rate_hz = rate_hz;
    parameters.depth_ms = depth_ms;
    parameters.voice_count = voice_count;
    parameters.width = 1.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::PolyDetune effect(8000.0F, 1U, parameters);
    std::vector<float> audio(8000U);
    for (std::size_t i = 0; i < audio.size(); ++i) {
        audio[i] = 0.4F * std::sin(2.0F * 3.14159265358979323846F * 220.0F *
                                   static_cast<float>(i) / 8000.0F);
    }
    effect.process(audio, 1U);
    return audio;
}

void test_rate_depth_width_and_voice_controls() {
    const auto fixed = render_mono(0.25F, 0.0F);
    const auto deep = render_mono(0.25F, 2.0F);
    const auto fast = render_mono(1.5F, 2.0F);
    const auto single = render_mono(0.25F, 2.0F, 1U);
    require(squared_difference(fixed, deep, 128U) > 0.01,
            "detune depth did not modulate the wet delay");
    require(squared_difference(deep, fast, 128U) > 0.01,
            "detune rate did not alter the modulation cycle");
    require(squared_difference(deep, single, 128U) > 0.01,
            "one/two project voice choices produced identical output");

    tabletfx::PolyDetuneParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.depth_ms = 2.0F;
    parameters.voice_count = 2U;
    parameters.width = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::PolyDetune narrow(8000.0F, 2U, parameters);
    parameters.width = 1.0F;
    tabletfx::PolyDetune wide(8000.0F, 2U, parameters);
    std::vector<float> a(8000U * 2U);
    std::vector<float> b(a.size());
    for (std::size_t frame = 0; frame < a.size() / 2U; ++frame) {
        const float sample = 0.4F * std::sin(
            2.0F * 3.14159265358979323846F * 220.0F *
            static_cast<float>(frame) / 8000.0F);
        a[frame * 2U] = a[frame * 2U + 1U] = sample;
        b[frame * 2U] = b[frame * 2U + 1U] = sample;
    }
    narrow.process(a, 2U);
    wide.process(b, 2U);
    double narrow_side_difference = 0.0;
    double wide_side_difference = 0.0;
    for (std::size_t frame = 128U; frame < a.size() / 2U; ++frame) {
        narrow_side_difference += std::abs(a[frame * 2U] - a[frame * 2U + 1U]);
        wide_side_difference += std::abs(b[frame * 2U] - b[frame * 2U + 1U]);
    }
    require(narrow_side_difference < 1.0e-4,
            "zero width should keep identical stereo inputs aligned");
    require(wide_side_difference > 10.0,
            "full width did not produce opposite-channel modulation");
}

void test_parameter_ramp_block_invariance_and_reset() {
    tabletfx::PolyDetuneParameters initial;
    tabletfx::PolyDetune whole(48000.0F, 2U, initial);
    tabletfx::PolyDetune chunked(48000.0F, 2U, initial);
    std::vector<float> source(12000U);
    for (std::size_t frame = 0; frame < source.size() / 2U; ++frame) {
        const float sample = 0.35F * std::sin(
            2.0F * 3.14159265358979323846F * 330.0F *
            static_cast<float>(frame) / 48000.0F);
        source[frame * 2U] = sample;
        source[frame * 2U + 1U] = -0.7F * sample;
    }
    auto a = source;
    auto b = source;
    constexpr std::size_t prefix_frames = 127U;
    whole.process(std::span<float>(a.data(), prefix_frames * 2U), 2U);
    chunked.process(std::span<float>(b.data(), prefix_frames * 2U), 2U);
    auto changed = initial;
    changed.rate_hz = 1.5F;
    changed.depth_ms = 1.8F;
    changed.width = 0.25F;
    changed.mix = 0.8F;
    changed.level_db = -2.0F;
    whole.set_parameters(changed);
    chunked.set_parameters(changed);
    require(std::abs(whole.current_mix() - initial.mix) < 1.0e-7F,
            "mix changed immediately instead of using the parameter ramp");
    whole.process(std::span<float>(a.data() + prefix_frames * 2U,
                                   a.size() - prefix_frames * 2U), 2U);

    std::size_t offset = prefix_frames;
    for (const std::size_t block : {1U, 17U, 128U, 257U, 64U, 511U}) {
        const std::size_t count = std::min(block, source.size() / 2U - offset);
        chunked.process(std::span<float>(b.data() + offset * 2U,
                                         count * 2U), 2U);
        offset += count;
        if (offset == source.size() / 2U) break;
    }
    if (offset < source.size() / 2U) {
        chunked.process(std::span<float>(b.data() + offset * 2U,
                                         source.size() - offset * 2U), 2U);
    }
    require(squared_difference(a, b) < 1.0e-8,
            "Poly Detune output changed with callback block partitioning");
    require(std::abs(whole.current_mix() - changed.mix) < 1.0e-6F,
            "Poly Detune parameter ramp did not reach its target");

    whole.reset();
    tabletfx::PolyDetune fresh(48000.0F, 2U, changed);
    std::array<float, 512> after_reset{};
    std::array<float, 512> from_fresh{};
    for (std::size_t i = 0; i < after_reset.size(); ++i) {
        after_reset[i] = from_fresh[i] = source[i];
    }
    whole.process(after_reset, 2U);
    fresh.process(from_fresh, 2U);
    require(after_reset == from_fresh,
            "reset output differed from a fresh Poly Detune instance");
}

void test_non_finite_audio_bounded_output_and_no_allocations() {
    tabletfx::PolyDetune mono(48000.0F, 1U);
    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.25F, -0.5F, 25.0F, -25.0F, 0.0F};
    mono.process(invalid, 1U);
    require(std::all_of(invalid.begin(), invalid.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "non-finite input created non-finite output");

    tabletfx::PolyDetuneParameters loud;
    loud.level_db = 12.0F;
    loud.mix = 0.0F;
    loud.smoothing_ms = 0.0F;
    tabletfx::PolyDetune bounded(48000.0F, 2U, loud);
    std::array<float, 256> hot{};
    hot.fill(16.0F);
    bounded.process(hot, 2U);
    require(std::all_of(hot.begin(), hot.end(), [](float sample) {
                return std::isfinite(sample) && std::abs(sample) <= 8.0F;
            }),
            "output safety ceiling was not enforced");

    std::array<float, 1024> block{};
    block.fill(0.25F);
    tabletfx::PolyDetune stereo;
    allocation_count = 0U;
    count_allocations = true;
    for (int iteration = 0; iteration < 100; ++iteration) {
        stereo.process(block, 2U);
    }
    count_allocations = false;
    require(allocation_count == 0U,
            "Poly Detune allocated memory during process()");
}

void test_invalid_construction() {
    bool rejected_rate = false;
    try {
        tabletfx::PolyDetune invalid(100.0F, 1U);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    require(rejected_rate, "Poly Detune accepted an unsupported sample rate");

    bool rejected_channels = false;
    try {
        tabletfx::PolyDetune invalid(48000.0F, 3U);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    require(rejected_channels, "Poly Detune accepted an invalid channel count");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_bounds_and_memory();
        test_delay_timing_mono_stereo_and_exact_dry();
        test_rate_depth_width_and_voice_controls();
        test_parameter_ramp_block_invariance_and_reset();
        test_non_finite_audio_bounded_output_and_no_allocations();
        test_invalid_construction();
        std::cout << "Poly Detune tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        count_allocations = false;
        std::cerr << "Poly Detune test failure: " << exception.what() << '\n';
        return 1;
    }
}
