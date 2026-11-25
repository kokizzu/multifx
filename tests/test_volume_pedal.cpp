#include "tabletfx/effects/volume_pedal.hpp"

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

float linear_gain(float decibels) {
    return std::pow(10.0F, decibels / 20.0F);
}

void test_defaults_endpoints_and_memory() {
    tabletfx::VolumePedal pedal;
    const auto defaults = pedal.parameters();
    require(defaults.position == 1.0F && defaults.minimum_db == -96.0F &&
                defaults.maximum_db == 0.0F &&
                defaults.taper == tabletfx::VolumePedalTaper::AudioTaperDb &&
                defaults.smoothing_ms == 10.0F,
            "safe default volume pedal parameters changed");
    require(pedal.current_gain_linear() == 1.0F,
            "default toe position should pass audio at unity");
    require(pedal.id() == "volume-pedal", "volume pedal stable id changed");
    require(pedal.memory_bytes() == sizeof(pedal),
            "volume pedal memory report is inaccurate");

    pedal.set_position(0.0F);
    pedal.reset();
    require(pedal.current_gain_linear() == 0.0F,
            "heel position should fully mute rather than stop at -96 dB");
    std::array<float, 4> heel_audio{1.0F, -0.5F, 0.25F, -0.125F};
    pedal.process(heel_audio, 2);
    require(heel_audio == std::array<float, 4>{0.0F, 0.0F, 0.0F, 0.0F},
            "fully muted heel position should output exact silence");

    tabletfx::VolumePedalParameters custom_endpoint;
    custom_endpoint.position = 0.0F;
    custom_endpoint.minimum_db = -12.0F;
    custom_endpoint.maximum_db = 0.0F;
    tabletfx::VolumePedal custom_minimum(48000.0F, custom_endpoint);
    require(custom_minimum.current_gain_linear() == 0.0F,
            "0% position must mute even with a nonzero configured minimum");

    custom_minimum.set_position(0.001F);
    custom_minimum.reset();
    require(custom_minimum.current_gain_linear() > 0.0F,
            "positive expression position should remain audible");
    pedal.set_position(1.0F);
    pedal.reset();
    require(pedal.current_gain_linear() == 1.0F,
            "toe position did not reach the configured maximum endpoint");
}

void test_curves_and_monotonicity() {
    tabletfx::VolumePedalParameters parameters;
    parameters.minimum_db = -96.0F;
    parameters.maximum_db = 0.0F;
    parameters.position = 0.5F;
    parameters.taper = tabletfx::VolumePedalTaper::AudioTaperDb;
    tabletfx::VolumePedal audio_taper(48000.0F, parameters);
    require(std::abs(audio_taper.current_gain_linear() - linear_gain(-48.0F)) < 1.0e-8F,
            "audio taper must interpolate endpoint dB values");

    parameters.taper = tabletfx::VolumePedalTaper::LinearAmplitude;
    tabletfx::VolumePedal linear_taper(48000.0F, parameters);
    require(std::abs(linear_taper.current_gain_linear() -
                     (linear_gain(-96.0F) + 1.0F) * 0.5F) < 1.0e-6F,
            "linear-amplitude taper did not interpolate endpoint amplitudes");
    require(linear_taper.current_gain_linear() >
                audio_taper.current_gain_linear() * 100.0F,
            "the two selectable taper laws are not meaningfully distinct");

    float previous_gain = 0.0F;
    for (int step = 0; step <= 100; ++step) {
        linear_taper.set_position(static_cast<float>(step) / 100.0F);
        linear_taper.reset();
        const float current_gain = linear_taper.current_gain_linear();
        require(current_gain >= previous_gain,
                "volume-pedal gain was not monotonic with expression position");
        previous_gain = current_gain;
    }
}

void test_bounds_nonfinite_and_reversed_endpoints() {
    tabletfx::VolumePedal pedal;
    pedal.set_position(-2.0F);
    require(pedal.parameters().position == 0.0F,
            "position lower bound was not enforced");
    pedal.set_position(3.0F);
    require(pedal.parameters().position == 1.0F,
            "position upper bound was not enforced");
    pedal.set_position(std::numeric_limits<float>::quiet_NaN());
    require(pedal.parameters().position == 1.0F,
            "non-finite position should preserve the last valid setting");

    pedal.set_endpoints_db(-200.0F, 40.0F);
    require(pedal.parameters().minimum_db == -96.0F &&
                pedal.parameters().maximum_db == 12.0F,
            "dB endpoint bounds were not enforced");
    pedal.set_endpoints_db(6.0F, -18.0F);
    require(pedal.parameters().minimum_db == -18.0F &&
                pedal.parameters().maximum_db == 6.0F,
            "reversed endpoints were not normalized into ascending order");

    pedal.set_endpoints_db(std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity());
    require(pedal.parameters().minimum_db == -18.0F &&
                pedal.parameters().maximum_db == 6.0F,
            "non-finite endpoints should preserve their previous valid values");

    pedal.set_smoothing_ms(-1.0F);
    require(pedal.parameters().smoothing_ms == 0.0F,
            "negative smoothing time was not bounded to immediate response");
    pedal.set_smoothing_ms(std::numeric_limits<float>::quiet_NaN());
    require(pedal.parameters().smoothing_ms == 10.0F,
            "non-finite smoothing time did not use the safe default");
}

void test_smoothing_retarget_and_variable_blocks() {
    tabletfx::VolumePedal whole_pedal;
    tabletfx::VolumePedal chunked_pedal;
    whole_pedal.set_position(0.0F);
    chunked_pedal.set_position(0.0F);
    std::vector<float> whole(480, 1.0F);
    std::vector<float> chunked = whole;
    whole_pedal.process(whole, 1);

    const std::array<std::size_t, 5> blocks{1, 127, 53, 211, 17};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < chunked.size()) {
        const std::size_t frames = std::min(blocks[block % blocks.size()],
                                            chunked.size() - offset);
        chunked_pedal.process(std::span<float>(chunked).subspan(offset, frames), 1);
        offset += frames;
        ++block;
    }
    for (std::size_t i = 0; i < whole.size(); ++i) {
        require(std::abs(whole[i] - chunked[i]) < 1.0e-7F,
                "variable block sizes changed the control ramp");
        if (i > 0) {
            require(whole[i] <= whole[i - 1],
                    "decreasing position produced a non-monotonic ramp");
        }
    }
    require(whole.back() == 0.0F,
            "10 ms volume control ramp did not reach its target");

    whole_pedal.set_position(0.0F);
    std::array<float, 240> first_half{};
    first_half.fill(1.0F);
    whole_pedal.process(first_half, 1);
    const float before_retarget = first_half.back();
    whole_pedal.set_position(1.0F);
    std::array<float, 480> second_half{};
    second_half.fill(1.0F);
    whole_pedal.process(second_half, 1);
    require(second_half.front() > before_retarget &&
                std::abs(second_half.back() - 1.0F) < 1.0e-7F,
            "retargeting did not smoothly reach the new expression position");
    whole_pedal.reset();
    require(whole_pedal.current_gain_linear() == 1.0F,
            "reset did not settle to the target gain");
}

void test_processing_channel_guard_and_no_allocations() {
    tabletfx::VolumePedal pedal;
    std::array<float, 4> unchanged{0.1F, -0.2F, 0.3F, -0.4F};
    pedal.process(unchanged, 0);
    require(unchanged == std::array<float, 4>{0.1F, -0.2F, 0.3F, -0.4F},
            "zero-channel input should be ignored");

    std::array<float, 512> audio{};
    audio.fill(0.25F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        pedal.set_position(static_cast<float>(iteration % 101) / 100.0F);
        pedal.process(audio, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "volume pedal allocated memory while processing");
    for (float sample : audio) {
        require(std::isfinite(sample), "volume pedal emitted non-finite output");
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
        test_defaults_endpoints_and_memory();
        test_curves_and_monotonicity();
        test_bounds_nonfinite_and_reversed_endpoints();
        test_smoothing_retarget_and_variable_blocks();
        test_processing_channel_guard_and_no_allocations();
        std::cout << "All TabletFX volume pedal tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
