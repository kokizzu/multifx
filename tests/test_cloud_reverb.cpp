#include "tabletfx/effects/cloud_reverb.hpp"

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

double energy(std::span<const float> audio, std::size_t first,
              std::size_t last) {
    double total = 0.0;
    for (std::size_t index = first; index < last; ++index) {
        const double sample = audio[index];
        total += sample * sample;
    }
    return total;
}

std::vector<float> render_impulse(tabletfx::CloudReverbParameters parameters,
                                  std::size_t frames = 16384) {
    parameters.mix = 1.0F;
    tabletfx::CloudReverb reverb(48000.0F, parameters);
    std::vector<float> audio(frames * 2U, 0.0F);
    audio[0] = 0.8F;
    audio[1] = 0.8F;
    reverb.process(audio, 2);
    return audio;
}

float maximum_difference(std::span<const float> left,
                         std::span<const float> right) {
    float result = 0.0F;
    for (std::size_t index = 0; index < left.size(); ++index) {
        result = std::max(result, std::abs(left[index] - right[index]));
    }
    return result;
}

void test_defaults_bounds_and_memory() {
    tabletfx::CloudReverb reverb;
    const auto defaults = reverb.parameters();
    require(reverb.id() == "cloud-reverb",
            "Cloud Reverb identifier changed unexpectedly");
    require(defaults.mix == 0.25F && defaults.tone == 0.5F &&
                defaults.decay_seconds == 7.0F && defaults.diffusion == 0.8F &&
                defaults.high_damp_hz == 9000.0F &&
                defaults.low_damp_hz == 80.0F &&
                defaults.modulation_rate_hz == 0.3F &&
                defaults.modulation_depth == 0.15F && !defaults.spillover,
            "Cloud Reverb project defaults changed unexpectedly");
    require(reverb.memory_bytes() > sizeof(reverb) &&
                reverb.memory_bytes() < 500000U,
            "Cloud Reverb stereo storage should be fixed and bounded");

    tabletfx::CloudReverbParameters extreme;
    extreme.mix = 2.0F;
    extreme.tone = -1.0F;
    extreme.decay_seconds = 100.0F;
    extreme.diffusion = 2.0F;
    extreme.high_damp_hz = 100000.0F;
    extreme.low_damp_hz = -1.0F;
    extreme.modulation_rate_hz = 100.0F;
    extreme.modulation_depth = 2.0F;
    extreme.spillover = true;
    reverb.configure(extreme);
    const auto bounded = reverb.parameters();
    require(bounded.mix == 1.0F && bounded.tone == 0.0F &&
                bounded.decay_seconds == 20.0F && bounded.diffusion == 1.0F &&
                bounded.high_damp_hz == 20000.0F &&
                bounded.low_damp_hz == 20.0F &&
                bounded.modulation_rate_hz == 8.0F &&
                bounded.modulation_depth == 1.0F && bounded.spillover,
            "Cloud Reverb controls were not clamped to project bounds");

    tabletfx::CloudReverbParameters invalid;
    invalid.mix = std::numeric_limits<float>::quiet_NaN();
    invalid.tone = std::numeric_limits<float>::infinity();
    invalid.decay_seconds = -std::numeric_limits<float>::infinity();
    invalid.diffusion = std::numeric_limits<float>::quiet_NaN();
    invalid.high_damp_hz = std::numeric_limits<float>::infinity();
    invalid.low_damp_hz = std::numeric_limits<float>::quiet_NaN();
    invalid.modulation_rate_hz = std::numeric_limits<float>::quiet_NaN();
    invalid.modulation_depth = std::numeric_limits<float>::infinity();
    reverb.configure(invalid);
    const auto safe = reverb.parameters();
    require(safe.mix == defaults.mix && safe.tone == defaults.tone &&
                safe.decay_seconds == defaults.decay_seconds &&
                safe.diffusion == defaults.diffusion &&
                safe.high_damp_hz == defaults.high_damp_hz &&
                safe.low_damp_hz == defaults.low_damp_hz &&
                safe.modulation_rate_hz == defaults.modulation_rate_hz &&
                safe.modulation_depth == defaults.modulation_depth,
            "non-finite Cloud Reverb controls must fall back to defaults");

    tabletfx::CloudReverb low_rate(8000.0F);
    require(low_rate.parameters().high_damp_hz == 3600.0F,
            "high damping must respect the sample-rate-aware cutoff ceiling");

    bool rejected = false;
    try {
        tabletfx::CloudReverb invalid_rate(7999.0F);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Cloud Reverb accepted an unsupported sample rate");

    for (const std::uint16_t channels : {std::uint16_t{0}, std::uint16_t{3}}) {
        rejected = false;
        try {
            tabletfx::CloudReverb invalid_channels(48000.0F, channels);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "Cloud Reverb accepted an unsupported channel count");
    }
}

void test_stereo_wet_response_and_dry_mix() {
    tabletfx::CloudReverbParameters dry_parameters;
    dry_parameters.mix = 0.0F;
    tabletfx::CloudReverb dry(48000.0F, dry_parameters);
    std::array<float, 16> samples{
        0.25F, -0.125F, 0.5F, 0.25F, -0.4F, 0.1F, 0.2F, -0.6F,
        0.0F, 0.3F, -0.2F, -0.1F, 0.7F, -0.8F, 0.05F, 0.1F};
    const auto original = samples;
    dry.process(samples, 2);
    require(samples == original,
            "mix=0 must preserve stereo dry input sample-for-sample");

    tabletfx::CloudReverbParameters wet_parameters;
    wet_parameters.mix = 1.0F;
    wet_parameters.decay_seconds = 4.0F;
    tabletfx::CloudReverb wet(48000.0F, wet_parameters);
    std::vector<float> stereo(48000U * 2U, 0.0F);
    stereo[0] = 0.8F;
    stereo[1] = 0.8F;
    wet.process(stereo, 2);
    double channel_difference = 0.0;
    for (std::size_t frame = 0; frame < stereo.size() / 2U; ++frame) {
        require(std::isfinite(stereo[frame * 2U]) &&
                    std::isfinite(stereo[frame * 2U + 1U]),
                "Cloud Reverb output must remain finite");
        channel_difference += std::abs(stereo[frame * 2U] -
                                       stereo[frame * 2U + 1U]);
    }
    require(energy(stereo, 0, stereo.size()) > 1.0e-5,
            "Cloud Reverb did not produce a measurable wet response");
    require(channel_difference > 1.0e-3,
            "identical stereo inputs should produce decorrelated wet channels");

    tabletfx::CloudReverb mono(48000.0F, 1, wet_parameters);
    std::vector<float> mono_audio(24000U, 0.0F);
    mono_audio[0] = 0.8F;
    mono.process(mono_audio, 1);
    require(energy(mono_audio, 0, mono_audio.size()) > 1.0e-5,
            "mono Cloud Reverb did not produce a wet response");

    std::array<float, 4> mismatched{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before = mismatched;
    wet.process(mismatched, 1);
    require(mismatched == before,
            "Cloud Reverb must leave non-stereo input untouched");
}

void test_tone_damping_diffusion_modulation_and_decay_controls() {
    tabletfx::CloudReverbParameters base;
    base.decay_seconds = 3.0F;
    base.diffusion = 0.6F;
    base.high_damp_hz = 9000.0F;
    base.low_damp_hz = 80.0F;
    base.modulation_rate_hz = 0.3F;
    base.modulation_depth = 0.15F;
    const auto reference = render_impulse(base);

    auto dark = base;
    dark.tone = 0.0F;
    auto bright = base;
    bright.tone = 1.0F;
    require(maximum_difference(render_impulse(dark), render_impulse(bright)) > 1.0e-4F,
            "Tone control did not change the wet response");

    auto low_high_damp = base;
    low_high_damp.high_damp_hz = 1000.0F;
    auto high_high_damp = base;
    high_high_damp.high_damp_hz = 20000.0F;
    require(maximum_difference(render_impulse(low_high_damp),
                               render_impulse(high_high_damp)) > 1.0e-4F,
            "High Damp control did not change the feedback response");

    auto low_damp = base;
    low_damp.low_damp_hz = 20.0F;
    auto high_damp = base;
    high_damp.low_damp_hz = 1000.0F;
    require(maximum_difference(render_impulse(low_damp),
                               render_impulse(high_damp)) > 1.0e-4F,
            "Low Damp control did not change the feedback response");

    auto sparse = base;
    sparse.diffusion = 0.0F;
    auto diffuse = base;
    diffuse.diffusion = 1.0F;
    require(maximum_difference(render_impulse(sparse),
                               render_impulse(diffuse)) > 1.0e-4F,
            "Diffuse control did not change the wet response");

    auto unmodulated = base;
    unmodulated.modulation_depth = 0.0F;
    auto modulated = base;
    modulated.modulation_depth = 1.0F;
    modulated.modulation_rate_hz = 2.0F;
    require(maximum_difference(render_impulse(unmodulated),
                               render_impulse(modulated)) > 1.0e-5F,
            "Rate/Depth controls did not change the wet response");

    auto short_decay = base;
    short_decay.decay_seconds = 0.2F;
    auto long_decay = base;
    long_decay.decay_seconds = 20.0F;
    const auto short_tail = render_impulse(short_decay, 48000);
    const auto long_tail = render_impulse(long_decay, 48000);
    require(energy(long_tail, 24000, long_tail.size()) >
                energy(short_tail, 24000, short_tail.size()) * 1.5,
            "longer decay should retain more late-tail energy");

    require(reference.size() == 32768U,
            "Cloud Reverb impulse helper returned an unexpected buffer size");
}

void test_spillover_reset_and_allocation_free_processing() {
    tabletfx::CloudReverbParameters parameters;
    parameters.mix = 1.0F;
    parameters.spillover = true;
    tabletfx::CloudReverb reverb(48000.0F, parameters);
    std::array<float, 4096> impulse{};
    impulse[0] = 0.8F;
    impulse[1] = 0.8F;
    reverb.process(impulse, 2);

    std::array<float, 4096> bypassed{};
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    reverb.process_bypassed(bypassed, 2);
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before,
            "Cloud Reverb processing and spillover bypass must not allocate");
    require(energy(bypassed, 0, bypassed.size()) > 1.0e-8,
            "enabled spillover should emit the stored wet tail during bypass");

    reverb.reset();
    std::array<float, 128> silence{};
    reverb.process(silence, 2);
    require(std::all_of(silence.begin(), silence.end(),
                        [](float sample) { return sample == 0.0F; }),
            "reset must clear the Cloud Reverb tail");

    parameters.spillover = false;
    tabletfx::CloudReverb no_spillover(48000.0F, parameters);
    std::array<float, 4096> excited{};
    excited[0] = 0.8F;
    excited[1] = 0.8F;
    no_spillover.process(excited, 2);
    std::array<float, 4> bypass_input{0.25F, -0.125F, 0.5F, -0.25F};
    const auto dry_bypass = bypass_input;
    no_spillover.process_bypassed(bypass_input, 2);
    require(bypass_input == dry_bypass,
            "disabled spillover should clear its tail and pass bypass audio unchanged");
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
        test_stereo_wet_response_and_dry_mix();
        test_tone_damping_diffusion_modulation_and_decay_controls();
        test_spillover_reset_and_allocation_free_processing();
        std::cout << "All Cloud Reverb tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Cloud Reverb test failure: " << error.what() << '\n';
        return 1;
    }
}
