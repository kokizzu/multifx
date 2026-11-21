#include "tabletfx/effects/dimension_chorus.hpp"

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

void test_defaults_and_bounds() {
    tabletfx::DimensionChorus chorus;
    const auto defaults = chorus.parameters();
    require(chorus.id() == "dimension-chorus", "Dimension Chorus stable ID changed");
    require(defaults.version == tabletfx::DimensionChorusVersion::C &&
                defaults.mode == tabletfx::DimensionChorusMode::Mode1 &&
                defaults.mix == 0.5F && defaults.level_db == 0.0F,
            "project-authored Dimension Chorus defaults changed");

    auto parameters = defaults;
    parameters.version = static_cast<tabletfx::DimensionChorusVersion>(255U);
    parameters.mode = static_cast<tabletfx::DimensionChorusMode>(255U);
    parameters.mix = 2.0F;
    parameters.level_db = 30.0F;
    chorus.set_parameters(parameters);
    auto bounded = chorus.parameters();
    require(bounded.version == tabletfx::DimensionChorusVersion::C &&
                bounded.mode == tabletfx::DimensionChorusMode::Mode1 &&
                bounded.mix == 1.0F && bounded.level_db == 12.0F,
            "Dimension Chorus enum or upper parameter bounds were not enforced");

    parameters.version = tabletfx::DimensionChorusVersion::D;
    parameters.mode = tabletfx::DimensionChorusMode::Modes34;
    parameters.mix = -1.0F;
    parameters.level_db = -30.0F;
    chorus.set_parameters(parameters);
    bounded = chorus.parameters();
    require(bounded.version == tabletfx::DimensionChorusVersion::D &&
                bounded.mode == tabletfx::DimensionChorusMode::Modes34 &&
                bounded.mix == 0.0F && bounded.level_db == -24.0F,
            "Dimension Chorus lower parameter bounds were not enforced");

    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    parameters.level_db = std::numeric_limits<float>::infinity();
    chorus.set_parameters(parameters);
    const auto finite_defaults = chorus.parameters();
    require(finite_defaults.mix == 0.5F && finite_defaults.level_db == 0.0F,
            "non-finite Dimension Chorus parameters did not use safe defaults");
}

void test_mix_level_and_channel_isolation() {
    tabletfx::DimensionChorusParameters parameters;
    parameters.mix = 0.0F;
    parameters.level_db = 6.0F;
    tabletfx::DimensionChorus mono(8000.0F, 1, parameters);
    std::array<float, 8> dry{0.1F, -0.2F, 0.3F, -0.4F,
                             0.5F, -0.6F, 0.7F, -0.8F};
    const auto original = dry;
    mono.process(dry, 1);
    const float gain = std::pow(10.0F, 6.0F / 20.0F);
    for (std::size_t i = 0; i < dry.size(); ++i) {
        require(std::abs(dry[i] - original[i] * gain) < 1.0e-6F,
                "zero-mix Dimension Chorus changed dry signal or Level");
    }

    parameters.mix = 1.0F;
    parameters.level_db = 0.0F;
    tabletfx::DimensionChorus mono_wet(8000.0F, 1, parameters);
    std::array<float, 192> mono_impulse{};
    mono_impulse[0] = 1.0F;
    mono_wet.process(mono_impulse, 1);
    require(std::abs(mono_impulse[96] - 1.0F) < 0.06F,
            "mono Dimension Chorus wet tap missed its fractionally modulated 12 ms center");

    tabletfx::DimensionChorus stereo(8000.0F, 2, parameters);
    std::array<float, 384> stereo_impulse{};
    stereo_impulse[0] = 1.0F;
    stereo.process(stereo_impulse, 2);
    require(std::abs(stereo_impulse[192] - 1.0F) < 0.06F,
            "stereo Dimension Chorus left wet tap missed its fractionally modulated 12 ms center");
    for (std::size_t i = 1; i < stereo_impulse.size(); i += 2) {
        require(std::abs(stereo_impulse[i]) < 1.0e-7F,
                "Dimension Chorus leaked one input channel into the other");
    }

    std::array<float, 8> wrong_layout{0.1F, -0.2F, 0.3F, -0.4F,
                                      0.5F, -0.6F, 0.7F, -0.8F};
    const auto before = wrong_layout;
    stereo.process(wrong_layout, 1);
    require(wrong_layout == before, "Dimension Chorus modified a mismatched channel layout");
    mono.process(wrong_layout, 0);
    require(wrong_layout == before, "Dimension Chorus modified zero-channel data");
}

std::vector<float> render(tabletfx::DimensionChorusVersion version,
                          tabletfx::DimensionChorusMode mode) {
    tabletfx::DimensionChorusParameters parameters;
    parameters.version = version;
    parameters.mode = mode;
    parameters.mix = 1.0F;
    tabletfx::DimensionChorus chorus(48000.0F, 1, parameters);
    std::vector<float> samples(8192);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 0.4F * std::sin(2.0F * 3.14159265358979323846F * 220.0F *
                                    static_cast<float>(i) / 48000.0F);
    }
    chorus.process(samples, 1);
    return samples;
}

double squared_difference(const std::vector<float>& a,
                          const std::vector<float>& b) {
    double sum = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double difference = static_cast<double>(a[i]) -
                                  static_cast<double>(b[i]);
        sum += difference * difference;
    }
    return sum;
}

void test_documented_selectors_with_project_authored_profiles() {
    const auto mode1 = render(tabletfx::DimensionChorusVersion::C,
                              tabletfx::DimensionChorusMode::Mode1);
    const auto mode2 = render(tabletfx::DimensionChorusVersion::C,
                              tabletfx::DimensionChorusMode::Mode2);
    const auto mode3 = render(tabletfx::DimensionChorusVersion::C,
                              tabletfx::DimensionChorusMode::Mode3);
    const auto mode4 = render(tabletfx::DimensionChorusVersion::C,
                              tabletfx::DimensionChorusMode::Mode4);
    const auto mode12 = render(tabletfx::DimensionChorusVersion::C,
                               tabletfx::DimensionChorusMode::Modes12);
    const auto mode13 = render(tabletfx::DimensionChorusVersion::C,
                               tabletfx::DimensionChorusMode::Modes13);
    const auto mode14 = render(tabletfx::DimensionChorusVersion::C,
                               tabletfx::DimensionChorusMode::Modes14);
    const auto mode23 = render(tabletfx::DimensionChorusVersion::C,
                               tabletfx::DimensionChorusMode::Modes23);
    const auto mode24 = render(tabletfx::DimensionChorusVersion::C,
                               tabletfx::DimensionChorusMode::Modes24);
    const auto mode34 = render(tabletfx::DimensionChorusVersion::C,
                               tabletfx::DimensionChorusMode::Modes34);
    const std::array<const std::vector<float>*, 10> modes{
        &mode1, &mode2, &mode3, &mode4, &mode12,
        &mode13, &mode14, &mode23, &mode24, &mode34};
    for (std::size_t i = 1; i < modes.size(); ++i) {
        require(squared_difference(*modes[i - 1], *modes[i]) > 0.01,
                "project-authored Mode choices did not produce distinct output");
    }
    const auto version_c = render(tabletfx::DimensionChorusVersion::C,
                                  tabletfx::DimensionChorusMode::Mode2);
    const auto version_d = render(tabletfx::DimensionChorusVersion::D,
                                  tabletfx::DimensionChorusMode::Mode2);
    require(squared_difference(version_c, version_d) > 0.01,
            "project-authored Version C/D choices did not produce distinct output");
}

void test_block_partitioning_and_reset() {
    tabletfx::DimensionChorusParameters initial;
    tabletfx::DimensionChorus whole(48000.0F, 2, initial);
    tabletfx::DimensionChorus chunked(48000.0F, 2, initial);
    std::vector<float> source(6000);
    for (std::size_t frame = 0; frame < source.size() / 2U; ++frame) {
        const float sample = 0.35F * std::sin(
            2.0F * 3.14159265358979323846F * 330.0F *
            static_cast<float>(frame) / 48000.0F);
        source[frame * 2U] = sample;
        source[frame * 2U + 1U] = -0.7F * sample;
    }
    auto a = source;
    auto b = source;

    constexpr std::size_t prefix_frames = 127;
    whole.process(std::span<float>(a.data(), prefix_frames * 2U), 2);
    chunked.process(std::span<float>(b.data(), prefix_frames * 2U), 2);
    auto changed = initial;
    changed.version = tabletfx::DimensionChorusVersion::D;
    changed.mode = tabletfx::DimensionChorusMode::Modes34;
    changed.mix = 0.82F;
    changed.level_db = -2.0F;
    whole.set_parameters(changed);
    chunked.set_parameters(changed);
    require(std::abs(whole.current_mix() - initial.mix) < 1.0e-7F,
            "Dimension Chorus changed Mix without the smoothing ramp");

    whole.process(std::span<float>(a.data() + prefix_frames * 2U,
                                   a.size() - prefix_frames * 2U), 2);
    std::size_t frame_offset = prefix_frames;
    for (const std::size_t block_frames : {1U, 17U, 128U, 257U, 64U, 511U}) {
        const std::size_t count = std::min(block_frames,
                                           source.size() / 2U - frame_offset);
        chunked.process(std::span<float>(b.data() + frame_offset * 2U,
                                         count * 2U), 2);
        frame_offset += count;
        if (frame_offset == source.size() / 2U) break;
    }
    if (frame_offset < source.size() / 2U) {
        chunked.process(std::span<float>(b.data() + frame_offset * 2U,
                                         source.size() - frame_offset * 2U), 2);
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        require(std::abs(a[i] - b[i]) < 1.0e-6F,
                "Dimension Chorus output depends on callback block boundaries");
    }
    require(std::abs(whole.current_mix() - changed.mix) < 1.0e-6F,
            "Dimension Chorus parameter ramp did not reach its target");

    whole.reset();
    tabletfx::DimensionChorus fresh(48000.0F, 2, changed);
    std::array<float, 512> reset_output{};
    std::array<float, 512> fresh_output{};
    reset_output[0] = fresh_output[0] = 1.0F;
    whole.process(reset_output, 2);
    fresh.process(fresh_output, 2);
    require(reset_output == fresh_output,
            "Dimension Chorus reset did not match a fresh processor");
}

void test_non_finite_samples_and_zero_allocations() {
    tabletfx::DimensionChorus chorus;
    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.25F,
        -0.5F, 0.0F, 0.1F, -0.1F};
    chorus.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "Dimension Chorus emitted non-finite output");

    std::array<float, 256> block{};
    block.fill(0.25F);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        chorus.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "Dimension Chorus allocated memory during process()");
}

void test_invalid_construction() {
    bool rejected_rate = false;
    try {
        tabletfx::DimensionChorus invalid(100.0F, 1);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    require(rejected_rate, "Dimension Chorus accepted an unsupported sample rate");

    bool rejected_channels = false;
    try {
        tabletfx::DimensionChorus invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    require(rejected_channels, "Dimension Chorus accepted an unsupported channel count");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size); memory != nullptr) {
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
        test_defaults_and_bounds();
        test_mix_level_and_channel_isolation();
        test_documented_selectors_with_project_authored_profiles();
        test_block_partitioning_and_reset();
        test_non_finite_samples_and_zero_allocations();
        test_invalid_construction();
        std::cout << "Dimension Chorus tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Dimension Chorus test failure: " << error.what() << '\n';
        return 1;
    }
}
