#include "tabletfx/effects/resonator.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
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
    if (!condition) {
        throw std::runtime_error(message);
    }
}

float energy(std::span<const float> audio) {
    double sum = 0.0;
    for (const float sample : audio) {
        sum += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return static_cast<float>(sum);
}

std::array<float, 2048> render_impulse(float frequency_hz) {
    tabletfx::ResonatorParameters parameters;
    parameters.manual_frequency_hz.fill(frequency_hz);
    parameters.feedback.fill(0.8F);
    parameters.level.fill(1.0F);
    parameters.mix = 1.0F;
    parameters.level_db = 0.0F;
    tabletfx::Resonator resonator(48000.0F, parameters);
    std::array<float, 2048> audio{};
    audio[0] = 1.0F;
    resonator.process(audio, 1U);
    return audio;
}

void test_defaults_controls_and_memory_bounds() {
    tabletfx::Resonator resonator;
    const auto parameters = resonator.parameters();
    require(resonator.id() == "resonator",
            "stable effect ID changed");
    require(parameters.mode == tabletfx::ResonatorMode::Manual &&
                parameters.manual_frequency_hz[0] == 110.0F &&
                parameters.manual_frequency_hz[3] == 329.63F &&
                parameters.feedback[0] == 0.55F &&
                parameters.filter_q[0] == 2.0F &&
                parameters.level[0] == 0.4F &&
                parameters.master_level == 0.8F &&
                parameters.master_pan == 1.0F &&
                parameters.master_feedback == 1.0F &&
                parameters.master_q == 1.0F &&
                parameters.input_mode == tabletfx::ResonatorInputMode::Mono &&
                parameters.mix == 0.35F && parameters.level_db == 0.0F &&
                parameters.balance == 0.0F &&
                parameters.input_gain_db == 0.0F &&
                !parameters.global_mix_enabled &&
                parameters.global_mix_offset_percentage_points == 0.0F,
            "project-authored defaults changed");
    const std::size_t minimum_storage = sizeof(resonator) +
        4U * 2U * 962U * sizeof(float);
    require(resonator.memory_bytes() >= minimum_storage &&
                resonator.memory_bytes() < minimum_storage + 1024U,
            "memory estimate is not consistent with the bounded 48 kHz ring");
}

void test_global_mix_is_opt_in_preserves_base_mix_and_clamps() {
    tabletfx::ResonatorParameters base;
    base.manual_frequency_hz.fill(500.0F);
    base.feedback.fill(0.8F);
    base.level.fill(1.0F);
    base.mix = 0.4F;

    std::array<float, 1024> input{};
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = 0.2F * std::sin(static_cast<float>(i) * 0.09F);
    }

    const auto render = [&input](tabletfx::ResonatorParameters parameters) {
        tabletfx::Resonator resonator(48000.0F, parameters);
        auto output = input;
        resonator.process(output, 2U);
        return output;
    };

    const auto baseline = render(base);
    auto stored_disabled = base;
    stored_disabled.global_mix_offset_percentage_points = 20.0F;
    require(render(stored_disabled) == baseline,
            "a stored offset must not alter output while Global Mix is disabled");

    auto offset = stored_disabled;
    offset.global_mix_enabled = true;
    const auto enabled = render(offset);
    auto equivalent_static = base;
    equivalent_static.mix = 0.6F;
    require(enabled == render(equivalent_static),
            "enabled Global Mix must match its equivalent static base Mix");
    require(offset.mix == 0.4F &&
                offset.global_mix_offset_percentage_points == 20.0F,
            "Global Mix must preserve the base Mix and stored offset");

    offset.global_mix_offset_percentage_points = 100.0F;
    tabletfx::Resonator upper_bounded(48000.0F, offset);
    require(upper_bounded.parameters().global_mix_offset_percentage_points ==
                50.0F,
            "Global Mix offset must clamp to +50 percentage points");
    offset.global_mix_offset_percentage_points = -100.0F;
    tabletfx::Resonator lower_bounded(48000.0F, offset);
    require(lower_bounded.parameters().global_mix_offset_percentage_points ==
                -50.0F,
            "Global Mix offset must clamp to -50 percentage points");

    auto upper_clamp = base;
    upper_clamp.mix = 0.75F;
    upper_clamp.global_mix_enabled = true;
    upper_clamp.global_mix_offset_percentage_points = 50.0F;
    auto full_wet = upper_clamp;
    full_wet.mix = 1.0F;
    full_wet.global_mix_enabled = false;
    full_wet.global_mix_offset_percentage_points = 0.0F;
    require(render(upper_clamp) == render(full_wet),
            "positive Global Mix offset must clamp effective Mix to 100 percent");

    auto lower_clamp = base;
    lower_clamp.mix = 0.25F;
    lower_clamp.global_mix_enabled = true;
    lower_clamp.global_mix_offset_percentage_points = -50.0F;
    auto full_dry = lower_clamp;
    full_dry.mix = 0.0F;
    full_dry.global_mix_enabled = false;
    full_dry.global_mix_offset_percentage_points = 0.0F;
    require(render(lower_clamp) == render(full_dry),
            "negative Global Mix offset must clamp effective Mix to 0 percent");
}

void test_manual_bounds_and_master_frequency() {
    tabletfx::ResonatorParameters parameters;
    parameters.manual_frequency_hz = {
        std::numeric_limits<float>::quiet_NaN(), -1.0F, 250.0F, 20000.0F};
    parameters.master_frequency = 2.0F;
    tabletfx::Resonator resonator(48000.0F, parameters);
    const auto sanitized = resonator.parameters();
    const auto frequencies = resonator.effective_frequencies_hz();
    require(sanitized.manual_frequency_hz[0] == 110.0F &&
                sanitized.manual_frequency_hz[1] == 100.0F &&
                sanitized.manual_frequency_hz[2] == 250.0F &&
                sanitized.manual_frequency_hz[3] == 10000.0F,
            "manual controls did not clamp to the documented range or default");
    require(frequencies[0] == 220.0F && frequencies[1] == 200.0F &&
                frequencies[2] == 500.0F && frequencies[3] == 20000.0F,
            "MANUAL frequencies were not scaled by Master Frequency");

    parameters.master_frequency = 100.0F;
    tabletfx::Resonator bounded(48000.0F, parameters);
    require(bounded.parameters().master_frequency == 2.0F &&
                bounded.effective_frequencies_hz()[3] <= 21600.0F,
            "project-authored master-frequency and Nyquist bounds failed");
}

void test_chordal_frequency_mapping() {
    tabletfx::ResonatorParameters parameters;
    parameters.mode = tabletfx::ResonatorMode::Chordal;
    parameters.chord = tabletfx::ResonatorChord::Major;
    parameters.master_frequency = 2.0F;
    parameters.chord_root_frequency_hz = 110.0F;
    tabletfx::Resonator resonator(48000.0F, parameters);
    const auto frequencies = resonator.effective_frequencies_hz();
    require(std::abs(frequencies[0] - 110.0F) < 0.01F &&
                std::abs(frequencies[1] - 138.591F) < 0.02F &&
                std::abs(frequencies[2] - 164.814F) < 0.02F &&
                std::abs(frequencies[3] - 220.0F) < 0.02F,
            "CHORDAL mode did not derive its four project-voiced frequencies");
    require(parameters.master_frequency == 2.0F &&
                frequencies[0] == 110.0F,
            "CHORDAL mode must use its root instead of the MANUAL scaler");

    parameters.chord_root_frequency_hz = 10000.0F;
    parameters.chord = static_cast<tabletfx::ResonatorChord>(255U);
    tabletfx::Resonator bounded(48000.0F, parameters);
    require(bounded.parameters().chord == tabletfx::ResonatorChord::Major &&
                bounded.parameters().chord_root_frequency_hz == 5000.0F,
            "invalid chord and out-of-range CHORDAL root were not sanitized");
}

void test_silence_finite_output_and_unsupported_layout() {
    tabletfx::Resonator resonator;
    std::array<float, 512> silence{};
    resonator.process(silence, 1U);
    require(std::all_of(silence.begin(), silence.end(),
                        [](float value) { return value == 0.0F; }),
            "silence must remain silent");

    std::array<float, 6> invalid_input{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 1.0e30F,
        -std::numeric_limits<float>::infinity(), -1.0e30F, 0.0F};
    resonator.process(invalid_input, 1U);
    require(std::all_of(invalid_input.begin(), invalid_input.end(),
                        [](float value) { return std::isfinite(value); }),
            "nonfinite/extreme input produced nonfinite output");

    std::array<float, 6> unsupported{1, 2, 3, 4, 5, 6};
    resonator.process(unsupported, 3U);
    require(unsupported == std::array<float, 6>{1, 2, 3, 4, 5, 6},
            "unsupported channel layouts must be left untouched");
}

void test_resonance_mix_and_bypass_tail() {
    const auto resonant = render_impulse(500.0F);
    require(std::abs(resonant[96]) > 1.0e-4F && energy(resonant) > 1.0e-5F,
            "a 500 Hz setting did not produce the expected resonant delay response");
    require(std::all_of(resonant.begin(), resonant.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "feedback resonance produced nonfinite output");

    tabletfx::ResonatorParameters dry_parameters;
    dry_parameters.mix = 0.0F;
    std::array<float, 128> source{};
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = 0.2F * std::sin(static_cast<float>(i) * 0.07F);
    }
    auto dry_audio = source;
    tabletfx::Resonator dry(48000.0F, dry_parameters);
    dry.process(dry_audio, 1U);
    require(dry_audio == source, "zero Mix must preserve the dry signal");

    tabletfx::ResonatorParameters full_wet_parameters = dry_parameters;
    full_wet_parameters.manual_frequency_hz.fill(500.0F);
    full_wet_parameters.feedback.fill(0.8F);
    full_wet_parameters.level.fill(1.0F);
    full_wet_parameters.mix = 1.0F;
    tabletfx::Resonator full_wet(48000.0F, full_wet_parameters);
    std::array<float, 512> impulse{};
    impulse[0] = 1.0F;
    full_wet.process(impulse, 1U);
    require(impulse[0] == 0.0F,
            "full-wet Mix must not include a direct dry impulse at frame zero");

    std::array<float, 128> bypassed{};
    full_wet.process_bypassed(bypassed, 1U);
    require(energy(bypassed) > 0.0F,
            "bypass should pass the dry signal and continue a stored resonator tail");

    std::array<float, 64> cold_bypass{};
    for (std::size_t i = 0; i < cold_bypass.size(); ++i) {
        cold_bypass[i] = 0.1F * static_cast<float>(i);
    }
    const auto unchanged = cold_bypass;
    tabletfx::Resonator cold;
    cold.process_bypassed(cold_bypass, 1U);
    require(cold_bypass == unchanged,
            "cold bypass must be transparent when no tail is stored");
}

void test_frequency_changes_change_response_without_tracking_input() {
    const auto low = render_impulse(300.0F);
    const auto high = render_impulse(600.0F);
    require(low != high,
            "different configured resonances must produce different responses");

    tabletfx::ResonatorParameters parameters;
    parameters.manual_frequency_hz.fill(300.0F);
    tabletfx::Resonator resonator(48000.0F, parameters);
    const auto before = resonator.effective_frequencies_hz();
    parameters.manual_frequency_hz.fill(600.0F);
    resonator.set_parameters(parameters);
    const auto after = resonator.effective_frequencies_hz();
    require(before[0] == 300.0F && after[0] == 600.0F,
            "frequency controls must set resonances directly without input tracking");
}

void test_stereo_routing_and_panning() {
    tabletfx::ResonatorParameters parameters;
    parameters.input_mode = tabletfx::ResonatorInputMode::Stereo;
    parameters.manual_frequency_hz.fill(500.0F);
    parameters.feedback.fill(0.0F);
    parameters.level = {1.0F, 1.0F, 0.0F, 0.0F};
    parameters.pan.fill(-1.0F);
    parameters.mix = 1.0F;
    tabletfx::Resonator resonator(48000.0F, parameters);

    std::array<float, 512> left_only{};
    left_only[0] = 1.0F;
    resonator.process(left_only, 2U);
    double left_energy = 0.0;
    double right_energy = 0.0;
    for (std::size_t frame = 0; frame < left_only.size() / 2U; ++frame) {
        left_energy += static_cast<double>(left_only[frame * 2U]) *
                       static_cast<double>(left_only[frame * 2U]);
        right_energy += static_cast<double>(left_only[frame * 2U + 1U]) *
                        static_cast<double>(left_only[frame * 2U + 1U]);
    }
    require(left_energy > 0.0 && right_energy == 0.0,
            "stereo mode or hard-left panning did not isolate the selected voices");

    resonator.reset();
    std::array<float, 512> right_only{};
    right_only[1] = 1.0F;
    resonator.process(right_only, 2U);
    require(energy(right_only) == 0.0F,
            "right input must feed voices 3–4, whose test levels are muted");
}

void test_process_paths_do_not_allocate() {
    tabletfx::ResonatorParameters parameters;
    parameters.feedback.fill(0.8F);
    tabletfx::Resonator resonator(48000.0F, parameters);
    std::array<float, 256> audio{};
    audio[0] = 0.7F;
    allocation_count.store(0U, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_relaxed);
    resonator.process(audio, 2U);
    resonator.process_bypassed(audio, 2U);
    count_allocations.store(false, std::memory_order_relaxed);
    require(allocation_count.load(std::memory_order_relaxed) == 0U,
            "audio processing must not allocate");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size == 0U ? 1U : size)) {
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
        test_defaults_controls_and_memory_bounds();
        test_global_mix_is_opt_in_preserves_base_mix_and_clamps();
        test_manual_bounds_and_master_frequency();
        test_chordal_frequency_mapping();
        test_silence_finite_output_and_unsupported_layout();
        test_resonance_mix_and_bypass_tail();
        test_frequency_changes_change_response_without_tracking_input();
        test_stereo_routing_and_panning();
        test_process_paths_do_not_allocate();
        std::cout << "Resonator tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Resonator tests failed: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
