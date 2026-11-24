#include "tabletfx/effects/pedal_detune.hpp"

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
std::atomic<std::size_t> allocation_count{0U};

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

std::vector<float> render_mode(tabletfx::PedalDetuneMode mode,
                               float position = 1.0F) {
    tabletfx::PedalDetuneParameters parameters;
    parameters.mode = mode;
    parameters.position = position;
    tabletfx::PedalDetune effect(8000.0F, 1U, parameters);
    std::vector<float> audio(8000U);
    for (std::size_t frame = 0; frame < audio.size(); ++frame) {
        audio[frame] = 0.4F * std::sin(
            2.0F * 3.14159265358979323846F * 220.0F *
            static_cast<float>(frame) / 8000.0F);
    }
    effect.process(audio, 1U);
    return audio;
}

void test_defaults_bounds_and_memory() {
    tabletfx::PedalDetune effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "pedal-detune", "Pedal Detune stable ID changed");
    require(defaults.mode == tabletfx::PedalDetuneMode::Shallow &&
                defaults.position == 0.5F && effect.current_position() == 0.5F,
            "project-authored Pedal Detune defaults changed");
    require(effect.memory_bytes() == sizeof(effect) + 434U * 2U * sizeof(float),
            "Pedal Detune memory must include only its shared stereo ring buffer");

    effect.set_position(-1.0F);
    require(effect.parameters().position == 0.0F,
            "negative position did not clamp to zero");
    effect.set_position(2.0F);
    require(effect.parameters().position == 1.0F,
            "position above unity did not clamp to one");
    effect.set_position(std::numeric_limits<float>::quiet_NaN());
    require(effect.parameters().position == 1.0F,
            "non-finite position did not retain its last valid value");
    effect.set_mode(static_cast<tabletfx::PedalDetuneMode>(255U));
    require(effect.parameters().mode == tabletfx::PedalDetuneMode::Shallow,
            "invalid mode did not fall back to Shallow");

    tabletfx::PedalDetuneParameters invalid;
    invalid.mode = static_cast<tabletfx::PedalDetuneMode>(255U);
    invalid.position = std::numeric_limits<float>::infinity();
    effect.set_parameters(invalid);
    require(effect.parameters().mode == tabletfx::PedalDetuneMode::Shallow &&
                effect.parameters().position == 1.0F,
            "parameter configuration did not retain the last safe position");
}

void test_position_blend_and_modes() {
    tabletfx::PedalDetuneParameters dry_parameters;
    dry_parameters.position = 0.0F;
    tabletfx::PedalDetune dry(48000.0F, 2U, dry_parameters);
    std::array<float, 16> dry_audio{0.1F, -0.2F, 0.3F, -0.4F,
                                    0.5F, -0.6F, 0.7F, -0.8F,
                                    0.125F, -0.25F, 0.375F, -0.5F,
                                    0.625F, -0.75F, 0.875F, -1.0F};
    const auto original = dry_audio;
    dry.process(dry_audio, 2U);
    require(dry_audio == original,
            "zero POSITION should preserve the dry signal exactly");

    const auto shallow = render_mode(tabletfx::PedalDetuneMode::Shallow);
    const auto deep = render_mode(tabletfx::PedalDetuneMode::Deep);
    const auto muted = render_mode(tabletfx::PedalDetuneMode::Deep, 0.0F);
    require(squared_difference(shallow, muted, 128U) > 0.01,
            "full POSITION did not blend in the Shallow detuned signal");
    require(squared_difference(deep, muted, 128U) > 0.01,
            "full POSITION did not blend in the Deep detuned signal");
    require(squared_difference(shallow, deep, 128U) > 0.01,
            "Shallow and Deep modes produced identical detune behavior");
}

void test_smoothing_block_invariance_and_reset() {
    tabletfx::PedalDetune whole(48000.0F, 2U);
    tabletfx::PedalDetune chunked(48000.0F, 2U);
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
    whole.set_position(1.0F);
    chunked.set_position(1.0F);
    require(whole.current_position() < 0.51F,
            "POSITION change should ramp rather than jump");
    whole.set_mode(tabletfx::PedalDetuneMode::Deep);
    chunked.set_mode(tabletfx::PedalDetuneMode::Deep);
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
            "variable block sizes changed Pedal Detune output");
    require(std::abs(whole.current_position() - 1.0F) < 1.0e-6F,
            "POSITION ramp did not reach its target");

    whole.reset();
    tabletfx::PedalDetune fresh(48000.0F, 2U,
        {tabletfx::PedalDetuneMode::Deep, 1.0F});
    std::array<float, 512> after_reset{};
    std::array<float, 512> from_fresh{};
    for (std::size_t i = 0; i < after_reset.size(); ++i) {
        after_reset[i] = from_fresh[i] = source[i];
    }
    whole.process(after_reset, 2U);
    fresh.process(from_fresh, 2U);
    require(after_reset == from_fresh,
            "reset output differed from a fresh Pedal Detune instance");
}

void test_finite_recovery_and_no_process_allocations() {
    tabletfx::PedalDetune mono(48000.0F, 1U);
    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.25F, -0.5F, 25.0F, -25.0F, 0.0F};
    mono.process(invalid, 1U);
    require(std::all_of(invalid.begin(), invalid.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "invalid input created non-finite output");

    std::array<float, 1024> block{};
    block.fill(0.25F);
    tabletfx::PedalDetune stereo;
    allocation_count = 0U;
    count_allocations = true;
    for (int iteration = 0; iteration < 100; ++iteration) {
        stereo.process(block, 2U);
    }
    count_allocations = false;
    require(allocation_count == 0U,
            "Pedal Detune allocated memory in process()");
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
        test_position_blend_and_modes();
        test_smoothing_block_invariance_and_reset();
        test_finite_recovery_and_no_process_allocations();
        std::cout << "Pedal Detune tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        count_allocations = false;
        std::cerr << "Pedal Detune test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
