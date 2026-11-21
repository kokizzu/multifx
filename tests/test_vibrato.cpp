#include "tabletfx/effects/vibrato.hpp"

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
#include <string>
#include <vector>

namespace {

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void test_defaults_bounds_and_memory() {
    tabletfx::Vibrato vibrato;
    const auto defaults = vibrato.parameters();
    require(vibrato.id() == "vibrato", "vibrato stable ID changed");
    require(defaults.rate_hz == 5.0F && defaults.depth_ms == 0.35F &&
                defaults.mix == 1.0F && defaults.smoothing_ms == 10.0F,
            "sane vibrato defaults changed");
    const std::size_t buffer_bytes = vibrato.memory_bytes() - sizeof(vibrato);
    require(buffer_bytes >= 194U * 2U * sizeof(float) &&
                buffer_bytes <= 210U * 2U * sizeof(float),
            "vibrato storage is not bounded near the documented 4 ms ring");

    tabletfx::VibratoParameters parameters;
    parameters.rate_hz = 100.0F;
    parameters.depth_ms = 10.0F;
    parameters.mix = -1.0F;
    parameters.smoothing_ms = 2000.0F;
    vibrato.set_parameters(parameters);
    const auto bounded = vibrato.parameters();
    require(bounded.rate_hz == 10.0F && bounded.depth_ms == 1.5F &&
                bounded.mix == 0.0F && bounded.smoothing_ms == 1000.0F,
            "vibrato parameter bounds changed");

    parameters.rate_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.depth_ms = std::numeric_limits<float>::infinity();
    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    parameters.smoothing_ms = std::numeric_limits<float>::quiet_NaN();
    vibrato.set_parameters(parameters);
    const auto finite = vibrato.parameters();
    require(finite.rate_hz == 5.0F && finite.depth_ms == 0.35F &&
                finite.mix == 1.0F && finite.smoothing_ms == 10.0F,
            "non-finite vibrato parameters did not use safe defaults");
}

void test_center_delay_mix_and_stereo_isolation() {
    tabletfx::VibratoParameters parameters;
    parameters.depth_ms = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::Vibrato mono(48000.0F, 1, parameters);
    std::array<float, 128> impulse{};
    impulse[0] = 1.0F;
    mono.process(impulse, 1);
    require(std::all_of(impulse.begin(), impulse.begin() + 95,
                        [](float value) { return std::abs(value) < 1.0e-7F; }),
            "vibrato produced output before its 2 ms wet-path delay");
    require(std::abs(impulse[96] - 1.0F) < 1.0e-5F,
            "vibrato impulse at 2 ms was " + std::to_string(impulse[96]) +
                " (sample 95 was " + std::to_string(impulse[95]) + ")");

    parameters.mix = 0.0F;
    tabletfx::Vibrato dry(48000.0F, 1, parameters);
    std::array<float, 96> dry_input{};
    for (std::size_t i = 0; i < dry_input.size(); ++i) {
        dry_input[i] = static_cast<float>(static_cast<int>(i % 13) - 6) / 6.0F;
    }
    const auto expected_dry = dry_input;
    dry.process(dry_input, 1);
    require(dry_input == expected_dry, "zero-mix vibrato changed the dry signal");

    parameters.depth_ms = 0.0F;
    parameters.mix = 1.0F;
    tabletfx::Vibrato stereo(48000.0F, 2, parameters);
    std::array<float, 256> stereo_impulse{};
    stereo_impulse[0] = 1.0F;
    stereo.process(stereo_impulse, 2);
    require(std::abs(stereo_impulse[192] - 1.0F) < 1.0e-5F,
            "stereo vibrato sample at 2 ms was " +
                std::to_string(stereo_impulse[192]) + " (nearby " +
                std::to_string(stereo_impulse[190]) + ", " +
                std::to_string(stereo_impulse[194]) + ")");
    for (std::size_t i = 1; i < stereo_impulse.size(); i += 2) {
        require(std::abs(stereo_impulse[i]) < 1.0e-7F,
                "stereo vibrato leaked the left input into the right channel");
    }
}

void test_pitch_modulation_and_stereo_image() {
    tabletfx::VibratoParameters parameters;
    parameters.rate_hz = 5.0F;
    parameters.depth_ms = 0.35F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::Vibrato vibrato(48000.0F, 2, parameters);
    constexpr std::size_t frames = 48000;
    constexpr float frequency_hz = 1000.0F;
    std::vector<float> signal(frames * 2U);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float phase = 2.0F * std::numbers::pi_v<float> * frequency_hz *
                            static_cast<float>(frame) / 48000.0F;
        signal[frame * 2U] = std::sin(phase);
        signal[frame * 2U + 1U] = std::cos(phase);
    }
    vibrato.process(signal, 2);

    double previous_phase = 0.0;
    double unwrapped_phase = 0.0;
    double window_start_phase = 0.0;
    float minimum_frequency = std::numeric_limits<float>::max();
    float maximum_frequency = 0.0F;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const double phase = std::atan2(static_cast<double>(signal[frame * 2U]),
                                        static_cast<double>(signal[frame * 2U + 1U]));
        if (frame > 0) {
            unwrapped_phase += std::remainder(phase - previous_phase,
                                               2.0 * std::numbers::pi_v<double>);
        }
        previous_phase = phase;
        if ((frame + 1U) % 480U == 0U && frame >= 480U) {
            const float estimate = static_cast<float>(
                (unwrapped_phase - window_start_phase) * 48000.0 /
                (2.0 * std::numbers::pi_v<double> * 480U));
            minimum_frequency = std::min(minimum_frequency, estimate);
            maximum_frequency = std::max(maximum_frequency, estimate);
            window_start_phase = unwrapped_phase;
        }
    }
    require(minimum_frequency < 995.0F && maximum_frequency > 1005.0F,
            "vibrato did not produce measurable pitch modulation in both directions");

    for (std::size_t frame = 256; frame < frames; ++frame) {
        const float left = signal[frame * 2U];
        const float right = signal[frame * 2U + 1U];
        require(std::isfinite(left) && std::isfinite(right) &&
                    std::abs(std::hypot(left, right) - std::hypot(
                        std::sin(2.0F * std::numbers::pi_v<float> * frequency_hz *
                                 static_cast<float>(frame) / 48000.0F),
                        std::cos(2.0F * std::numbers::pi_v<float> * frequency_hz *
                                 static_cast<float>(frame) / 48000.0F))) < 0.01F,
                "stereo vibrato altered the shared modulation phase");
    }
}

void test_smoothing_block_invariance_and_reset() {
    tabletfx::Vibrato whole(48000.0F, 2);
    tabletfx::Vibrato chunks(48000.0F, 2);
    std::vector<float> input(4096);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = 0.3F * std::sin(2.0F * std::numbers::pi_v<float> * 330.0F *
                                   static_cast<float>(i / 2U) / 48000.0F);
    }
    auto a = input;
    auto b = input;
    whole.process(a, 2);
    for (std::size_t offset = 0; offset < b.size();) {
        const std::size_t count = std::min<std::size_t>(318, b.size() - offset);
        chunks.process(std::span<float>(b.data() + offset, count), 2);
        offset += count;
    }
    require(a == b, "vibrato output changed with block partitioning");

    tabletfx::VibratoParameters changed;
    changed.rate_hz = 2.0F;
    changed.depth_ms = 1.0F;
    changed.mix = 0.6F;
    whole.set_parameters(changed);
    chunks.set_parameters(changed);
    std::array<float, 128> change_a{};
    change_a.fill(0.2F);
    auto change_b = change_a;
    whole.process(change_a, 2);
    chunks.process(change_b, 2);
    require(change_a == change_b, "smoothed vibrato retarget differs across blocks");

    whole.reset();
    tabletfx::Vibrato fresh(48000.0F, 2, changed);
    std::array<float, 256> after_reset{};
    after_reset[0] = 1.0F;
    auto from_fresh = after_reset;
    whole.process(after_reset, 2);
    fresh.process(from_fresh, 2);
    require(after_reset == from_fresh, "vibrato reset does not match fresh state");
}

void test_invalid_samples_layout_and_no_allocations() {
    tabletfx::Vibrato vibrato;
    std::array<float, 64> wrong_layout{};
    wrong_layout.fill(0.25F);
    const auto before = wrong_layout;
    vibrato.process(wrong_layout, 1);
    require(wrong_layout == before, "vibrato modified a mismatched channel layout");

    std::array<float, 96> non_finite{};
    non_finite[0] = std::numeric_limits<float>::infinity();
    non_finite[2] = std::numeric_limits<float>::quiet_NaN();
    vibrato.process(non_finite, 2);
    require(std::all_of(non_finite.begin(), non_finite.end(),
                        [](float value) { return std::isfinite(value); }),
            "vibrato did not replace non-finite input with finite output");

    std::array<float, 128> samples{};
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 0.4F * std::sin(static_cast<float>(i) * 0.13F);
    }
    const std::size_t before_allocations = allocation_count.load(std::memory_order_relaxed);
    for (int i = 0; i < 100; ++i) vibrato.process(samples, 2);
    const std::size_t after_allocations = allocation_count.load(std::memory_order_relaxed);
    require(before_allocations == after_allocations, "vibrato allocated during process()");
    require(std::all_of(samples.begin(), samples.end(),
                        [](float value) { return std::isfinite(value); }),
            "vibrato generated non-finite samples");
}

void test_invalid_construction() {
    bool bad_rate = false;
    try { tabletfx::Vibrato invalid(100.0F, 1); }
    catch (const std::invalid_argument&) { bad_rate = true; }
    require(bad_rate, "vibrato accepted an unsupported sample rate");

    bool bad_channels = false;
    try { tabletfx::Vibrato invalid(48000.0F, 3); }
    catch (const std::invalid_argument&) { bad_channels = true; }
    require(bad_channels, "vibrato accepted an unsupported channel count");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_bounds_and_memory();
        test_center_delay_mix_and_stereo_isolation();
        test_pitch_modulation_and_stereo_image();
        test_smoothing_block_invariance_and_reset();
        test_invalid_samples_layout_and_no_allocations();
        test_invalid_construction();
        std::cout << "All TabletFX vibrato tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
