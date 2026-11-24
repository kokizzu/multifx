#include "tabletfx/effects/vibe.hpp"

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

std::vector<float> make_tone(float frequency_hz, std::size_t frames,
                             std::uint16_t channels = 1,
                             float sample_rate = 48000.0F) {
    std::vector<float> samples(frames * channels);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float value = 0.25F * std::sin(
            2.0F * 3.14159265358979323846F * frequency_hz *
            static_cast<float>(frame) / sample_rate);
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            samples[frame * channels + channel] = value;
        }
    }
    return samples;
}

float rms(std::span<const float> samples) {
    double energy = 0.0;
    for (float sample : samples) {
        energy += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return static_cast<float>(std::sqrt(energy / static_cast<double>(samples.size())));
}

void test_defaults_bounds_and_memory() {
    tabletfx::Vibe vibe;
    const auto defaults = vibe.parameters();
    require(vibe.id() == "vibe", "Vibe stable ID changed");
    require(defaults.rate_hz == 1.5F && defaults.depth == 0.7F &&
                defaults.trem == 0.2F && defaults.stages == 2 &&
                defaults.spread == 0.5F && defaults.level == 1.0F &&
                defaults.rate_mode == tabletfx::VibeRateMode::Free &&
                defaults.note_division == tabletfx::VibeNoteDivision::Quarter &&
                defaults.tempo_bpm == 120.0F &&
                defaults.tone_mode == tabletfx::VibeToneMode::Modern &&
                defaults.modulation_mode ==
                    tabletfx::VibeModulationMode::Chorus,
            "project Vibe defaults changed");
    require(vibe.effective_rate_hz() == defaults.rate_hz,
            "free-rate mode changed the selected Vibe rate");
    require(vibe.memory_bytes() == sizeof(vibe),
            "Vibe fixed-state memory estimate is inaccurate");

    tabletfx::VibeParameters parameters;
    parameters.rate_hz = 100.0F;
    parameters.depth = -1.0F;
    parameters.trem = 2.0F;
    parameters.stages = 0;
    parameters.spread = 2.0F;
    parameters.level = -1.0F;
    vibe.set_parameters(parameters);
    const auto bounded = vibe.parameters();
    require(bounded.rate_hz == 10.0F && bounded.depth == 0.0F &&
                bounded.trem == 1.0F && bounded.stages == 2 &&
                bounded.spread == 1.0F && bounded.level == 0.0F,
            "Vibe control bounds or stage fallback changed");

    parameters.rate_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.depth = std::numeric_limits<float>::infinity();
    parameters.trem = std::numeric_limits<float>::quiet_NaN();
    parameters.spread = std::numeric_limits<float>::infinity();
    parameters.level = std::numeric_limits<float>::quiet_NaN();
    parameters.stages = 1;
    parameters.rate_mode = static_cast<tabletfx::VibeRateMode>(255U);
    parameters.note_division =
        static_cast<tabletfx::VibeNoteDivision>(255U);
    parameters.tempo_bpm = std::numeric_limits<float>::infinity();
    vibe.set_parameters(parameters);
    const auto finite = vibe.parameters();
    require(finite.rate_hz == 1.5F && finite.depth == 0.7F &&
                finite.trem == 0.2F && finite.stages == 1 &&
                finite.spread == 0.5F && finite.level == 1.0F &&
                finite.rate_mode == tabletfx::VibeRateMode::Free &&
                finite.note_division == tabletfx::VibeNoteDivision::Quarter &&
                finite.tempo_bpm == 120.0F &&
                finite.tone_mode == tabletfx::VibeToneMode::Modern &&
                finite.modulation_mode ==
                    tabletfx::VibeModulationMode::Chorus,
            "non-finite Vibe controls did not use safe project defaults");
}

void test_universe_modes_and_project_defaults() {
    tabletfx::VibeParameters parameters;
    parameters.depth = 0.0F;
    parameters.trem = 1.0F;
    parameters.spread = 1.0F;
    tabletfx::Vibe universe(48000.0F, 1, parameters,
                            tabletfx::VibeModel::UniVerse);
    require(universe.id() == "uni-verse-vibe",
            "Uni-Verse Vibe stable ID changed");
    require(universe.parameters().trem == 0.0F &&
                universe.parameters().spread == 0.0F &&
                universe.parameters().stages == 2 &&
                universe.memory_bytes() == sizeof(universe),
            "Uni-Verse Vibe exposed unrelated controls or misreported state");

    auto high_tone = make_tone(8000.0F, 48000);
    auto modern_output = high_tone;
    auto vintage_output = high_tone;
    tabletfx::VibeParameters modern;
    modern.depth = 0.0F;
    modern.trem = 0.0F;
    modern.modulation_mode = tabletfx::VibeModulationMode::Chorus;
    tabletfx::Vibe modern_vibe(48000.0F, 1, modern,
                               tabletfx::VibeModel::UniVerse);
    modern_vibe.process(modern_output, 1);
    auto vintage = modern;
    vintage.tone_mode = tabletfx::VibeToneMode::Vintage;
    tabletfx::Vibe vintage_vibe(48000.0F, 1, vintage,
                                tabletfx::VibeModel::UniVerse);
    vintage_vibe.process(vintage_output, 1);
    require(rms(vintage_output) < rms(modern_output) * 0.82F,
            "project Vintage tone did not attenuate high frequencies");

    auto note = make_tone(440.0F, 48000);
    auto chorus_output = note;
    auto vibrato_output = note;
    tabletfx::VibeParameters chorus;
    chorus.rate_hz = 4.0F;
    chorus.depth = 0.8F;
    chorus.trem = 0.0F;
    chorus.spread = 0.0F;
    tabletfx::Vibe chorus_vibe(48000.0F, 1, chorus,
                               tabletfx::VibeModel::UniVerse);
    chorus_vibe.process(chorus_output, 1);
    auto vibrato = chorus;
    vibrato.modulation_mode = tabletfx::VibeModulationMode::Vibrato;
    tabletfx::Vibe vibrato_vibe(48000.0F, 1, vibrato,
                                tabletfx::VibeModel::UniVerse);
    vibrato_vibe.process(vibrato_output, 1);
    double mode_difference = 0.0;
    for (std::size_t index = 0; index < note.size(); ++index) {
        const double delta = static_cast<double>(chorus_output[index]) -
                             static_cast<double>(vibrato_output[index]);
        mode_difference += delta * delta;
    }
    const double mode_difference_rms =
        std::sqrt(mode_difference / static_cast<double>(note.size()));
    require(mode_difference_rms > 1.0e-5,
            "project Chorus/Vibrato switch did not change the wet/dry path; RMS=" +
                std::to_string(mode_difference_rms));

    vibrato.tone_mode = static_cast<tabletfx::VibeToneMode>(255U);
    vibrato.modulation_mode =
        static_cast<tabletfx::VibeModulationMode>(255U);
    vibrato_vibe.set_parameters(vibrato);
    require(vibrato_vibe.parameters().tone_mode ==
                tabletfx::VibeToneMode::Modern &&
                vibrato_vibe.parameters().modulation_mode ==
                    tabletfx::VibeModulationMode::Chorus,
            "invalid Uni-Verse mode controls did not select safe defaults");

    std::array<float, 512> block{};
    block.fill(0.2F);
    const std::size_t allocations_before =
        allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        universe.process(block, 1);
    }
    const std::size_t allocations_after =
        allocation_count.load(std::memory_order_relaxed);
    require(allocations_before == allocations_after,
            "Uni-Verse Vibe allocated during process()");
}

void test_tempo_note_rate_and_bounds() {
    tabletfx::VibeParameters parameters;
    parameters.rate_mode = tabletfx::VibeRateMode::TempoNote;
    parameters.tempo_bpm = 120.0F;
    parameters.note_division = tabletfx::VibeNoteDivision::Quarter;
    tabletfx::Vibe vibe(48000.0F, 1, parameters);
    require(std::abs(vibe.effective_rate_hz() - 2.0F) < 1.0e-6F,
            "quarter-note Vibe rate did not follow 120 BPM");

    parameters.tempo_bpm = 90.0F;
    parameters.note_division = tabletfx::VibeNoteDivision::Eighth;
    vibe.set_parameters(parameters);
    require(std::abs(vibe.effective_rate_hz() - 3.0F) < 1.0e-6F,
            "eighth-note Vibe rate did not follow 90 BPM");

    parameters.tempo_bpm = 120.0F;
    parameters.note_division = tabletfx::VibeNoteDivision::EighthTriplet;
    vibe.set_parameters(parameters);
    require(std::abs(vibe.effective_rate_hz() - 6.0F) < 1.0e-6F,
            "triplet Vibe rate did not use its project note length");

    parameters.tempo_bpm = 10.0F;
    parameters.note_division = tabletfx::VibeNoteDivision::Whole;
    vibe.set_parameters(parameters);
    require(vibe.parameters().tempo_bpm == 20.0F &&
                std::abs(vibe.effective_rate_hz() - (20.0F / 240.0F)) <
                    1.0e-6F,
            "tempo-note Vibe did not clamp its project BPM range");

    parameters.tempo_bpm = 400.0F;
    parameters.note_division = tabletfx::VibeNoteDivision::SixteenthTriplet;
    vibe.set_parameters(parameters);
    require(vibe.parameters().tempo_bpm == 300.0F &&
                std::abs(vibe.effective_rate_hz() - 30.0F) < 1.0e-6F,
            "fastest project Vibe note rate or BPM ceiling changed");

    parameters.rate_mode = tabletfx::VibeRateMode::Free;
    parameters.rate_hz = 1.5F;
    vibe.set_parameters(parameters);
    require(vibe.effective_rate_hz() == 1.5F,
            "returning to free-rate mode did not restore its stored Hz value");

    auto input = make_tone(330.0F, 24000);
    auto free_output = input;
    auto synced_output = input;
    tabletfx::Vibe free_vibe(48000.0F, 1);
    parameters.rate_mode = tabletfx::VibeRateMode::TempoNote;
    parameters.tempo_bpm = 120.0F;
    parameters.note_division = tabletfx::VibeNoteDivision::Quarter;
    tabletfx::Vibe synced_vibe(48000.0F, 1, parameters);
    free_vibe.process(free_output, 1);
    synced_vibe.process(synced_output, 1);
    require(free_output != synced_output,
            "tempo-note rate did not affect Vibe modulation processing");
}

void test_depth_and_selectable_stages() {
    tabletfx::VibeParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.depth = 0.0F;
    parameters.trem = 0.0F;
    parameters.stages = 1;
    parameters.spread = 0.0F;
    parameters.level = 1.0F;
    auto input = make_tone(440.0F, 12000);
    auto dry_output = input;
    tabletfx::Vibe dry(48000.0F, 1, parameters);
    dry.process(dry_output, 1);
    require(dry_output == input,
            "zero-depth/zero-trem Vibe should be transparent at unity level");

    parameters.depth = 1.0F;
    parameters.stages = 1;
    auto one_stage_output = input;
    tabletfx::Vibe one_stage(48000.0F, 1, parameters);
    one_stage.process(one_stage_output, 1);
    parameters.stages = 2;
    auto two_stage_output = input;
    tabletfx::Vibe two_stage(48000.0F, 1, parameters);
    two_stage.process(two_stage_output, 1);
    require(one_stage_output != input,
            "nonzero Vibe depth did not engage its phase network");
    require(one_stage_output != two_stage_output,
            "one and two Vibe phase stages did not produce distinct responses");
}

void test_trem_spread_and_mono_behavior() {
    tabletfx::VibeParameters parameters;
    parameters.rate_hz = 2.0F;
    parameters.depth = 0.5F;
    parameters.trem = 0.0F;
    parameters.spread = 0.0F;
    parameters.level = 1.0F;
    const auto input = make_tone(330.0F, 48000);
    auto no_trem_output = input;
    tabletfx::Vibe no_trem(48000.0F, 1, parameters);
    no_trem.process(no_trem_output, 1);

    parameters.trem = 1.0F;
    auto trem_output = input;
    tabletfx::Vibe trem(48000.0F, 1, parameters);
    trem.process(trem_output, 1);
    require(rms(trem_output) < rms(no_trem_output) * 0.8F,
            "full-depth Vibe trem did not lower the average output level");

    parameters.spread = 0.0F;
    auto mono_zero_spread = input;
    tabletfx::Vibe mono_zero(48000.0F, 1, parameters);
    mono_zero.process(mono_zero_spread, 1);
    parameters.spread = 1.0F;
    auto mono_full_spread = input;
    tabletfx::Vibe mono_full(48000.0F, 1, parameters);
    mono_full.process(mono_full_spread, 1);
    require(mono_zero_spread == mono_full_spread,
            "stereo-only Spread changed the mono signal");

    auto stereo_input = make_tone(330.0F, 24000, 2);
    auto stereo_output = stereo_input;
    tabletfx::Vibe stereo(48000.0F, 2, parameters);
    stereo.process(stereo_output, 2);
    double channel_difference = 0.0;
    for (std::size_t frame = 0; frame < stereo_output.size() / 2; ++frame) {
        channel_difference += std::abs(
            static_cast<double>(stereo_output[frame * 2] -
                                stereo_output[frame * 2 + 1]));
    }
    require(channel_difference > 1.0,
            "stereo Spread did not produce different left/right tremolo motion");
}

void test_level_reset_and_block_partitioning() {
    tabletfx::VibeParameters parameters;
    parameters.rate_hz = 3.0F;
    parameters.depth = 0.8F;
    parameters.trem = 0.6F;
    parameters.stages = 2;
    parameters.spread = 0.75F;
    parameters.level = 0.5F;
    auto half_input = make_tone(220.0F, 4096, 2);
    auto half_output = half_input;
    tabletfx::Vibe half(48000.0F, 2, parameters);
    half.process(half_output, 2);

    parameters.level = 1.0F;
    auto full_input = make_tone(220.0F, 4096, 2);
    auto full_output = full_input;
    tabletfx::Vibe full(48000.0F, 2, parameters);
    full.process(full_output, 2);
    for (std::size_t i = 0; i < full_output.size(); ++i) {
        require(std::abs(half_output[i] * 2.0F - full_output[i]) < 1.0e-6F,
                "Vibe Level is not linear output gain");
    }

    parameters.level = 1.0F;
    auto whole_samples = make_tone(220.0F, 8193, 2);
    auto chunks_samples = whole_samples;
    tabletfx::Vibe whole(48000.0F, 2, parameters);
    tabletfx::Vibe chunks(48000.0F, 2, parameters);
    whole.process(whole_samples, 2);
    std::size_t frame = 0;
    while (frame < chunks_samples.size() / 2) {
        const std::size_t frames = std::min<std::size_t>(137, chunks_samples.size() / 2 - frame);
        chunks.process(std::span<float>(chunks_samples.data() + frame * 2,
                                        frames * 2), 2);
        frame += frames;
    }
    require(whole_samples == chunks_samples,
            "Vibe output changed with block partitioning");

    std::array<float, 512> impulse{};
    impulse[0] = 1.0F;
    auto after_reset = impulse;
    whole.process(after_reset, 2);
    whole.reset();
    after_reset = impulse;
    whole.process(after_reset, 2);
    tabletfx::Vibe fresh(48000.0F, 2, parameters);
    auto fresh_output = impulse;
    fresh.process(fresh_output, 2);
    require(after_reset == fresh_output,
            "Vibe reset does not reproduce fresh processing state");
}

void test_invalid_samples_layout_and_no_allocations() {
    tabletfx::Vibe mono(48000.0F, 1);
    std::array<float, 5> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.25F, -0.5F};
    mono.process(invalid, 1);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F && invalid[2] == 0.0F,
            "Vibe did not replace non-finite input with silence");
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "Vibe emitted non-finite output");

    std::array<float, 4> wrong_layout{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before = wrong_layout;
    mono.process(wrong_layout, 2);
    require(wrong_layout == before,
            "Vibe modified a mismatched channel layout");

    tabletfx::Vibe stereo;
    std::array<float, 512> block{};
    block.fill(0.25F);
    const std::size_t allocations_before =
        allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        stereo.process(block, 2);
    }
    const std::size_t allocations_after =
        allocation_count.load(std::memory_order_relaxed);
    require(allocations_before == allocations_after,
            "Vibe allocated during process()");
}

void test_invalid_construction() {
    bool bad_rate = false;
    try {
        tabletfx::Vibe invalid(100.0F, 1);
    } catch (const std::invalid_argument&) {
        bad_rate = true;
    }
    require(bad_rate, "Vibe accepted an unsupported sample rate");

    bool bad_channels = false;
    try {
        tabletfx::Vibe invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        bad_channels = true;
    }
    require(bad_channels, "Vibe accepted an unsupported channel count");
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
        test_universe_modes_and_project_defaults();
        test_tempo_note_rate_and_bounds();
        test_depth_and_selectable_stages();
        test_trem_spread_and_mono_behavior();
        test_level_reset_and_block_partitioning();
        test_invalid_samples_layout_and_no_allocations();
        test_invalid_construction();
        std::cout << "Vibe tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Vibe test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
