#include "tabletfx/effects/prime_drum_machine.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>

using tabletfx::effects::DrumMachine;
using tabletfx::effects::DrumMachineParameters;
using tabletfx::effects::DrumMachineStyle;

#ifdef assert
#undef assert
#endif
#define assert(...)                                                         \
    do {                                                                    \
        if (!static_cast<bool>((__VA_ARGS__))) {                            \
            std::fprintf(stderr, "TEST FAILURE: %s (%s:%d)\n",             \
                         #__VA_ARGS__, __FILE__, __LINE__);                 \
            std::abort();                                                   \
        }                                                                   \
    } while (false)

namespace {

std::atomic<std::size_t> allocation_count{0U};

constexpr float kSampleRate = 48000.0F;
constexpr std::size_t kFourStepsAndMarginFrames = 24008U;

double energy(const std::span<const float> samples) {
    double sum = 0.0;
    for (const float sample : samples) {
        sum += static_cast<double>(sample) * sample;
    }
    return sum;
}

bool all_finite(const std::span<const float> samples) {
    return std::all_of(samples.begin(), samples.end(),
                       [](const float sample) { return std::isfinite(sample); });
}

template <typename Callable>
bool throws_invalid_argument(Callable&& callable) {
    try {
        callable();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void test_defaults_and_controls_are_bounded() {
    DrumMachine effect(kSampleRate, 2U);
    assert(effect.id() == "drum-machine");
    assert(effect.parameters().tempo_bpm == 120.0);
    assert(effect.parameters().mix == 0.25F);
    assert(effect.parameters().volume == 1.0F);
    assert(effect.parameters().groove == 0.0F);
    assert(effect.current_step() == 0U);
    assert(effect.current_bar() == 0U);
    assert(!effect.running());
    assert(!effect.muted());

    auto parameters = effect.parameters();
    parameters.tempo_bpm = 999.0;
    parameters.mix = -2.0F;
    parameters.volume = -2.0F;
    parameters.intensity = 3.0F;
    parameters.groove = 4.0F;
    parameters.kick_mix = -1.0F;
    parameters.snare_mix = 3.0F;
    parameters.cymbal_mix = 4.0F;
    parameters.tom_mix = -4.0F;
    parameters.eq_db = 48.0F;
    parameters.timbre = -2.0F;
    parameters.tuning_semitones = 48.0F;
    parameters.reverb = -1.0F;
    parameters.drive = 3.0F;
    parameters.length_bars = 0U;
    parameters.style = static_cast<DrumMachineStyle>(99U);
    effect.configure(parameters);
    const auto& bounded = effect.parameters();
    assert(bounded.tempo_bpm == 300.0);
    assert(bounded.mix == 0.0F);
    assert(bounded.volume == 0.0F);
    assert(bounded.intensity == 1.0F);
    assert(bounded.groove == 1.0F);
    assert(bounded.kick_mix == 0.0F);
    assert(bounded.snare_mix == 1.0F);
    assert(bounded.cymbal_mix == 1.0F);
    assert(bounded.tom_mix == 0.0F);
    assert(bounded.eq_db == 12.0F);
    assert(bounded.timbre == 0.0F);
    assert(bounded.tuning_semitones == 12.0F);
    assert(bounded.reverb == 0.0F);
    assert(bounded.drive == 1.0F);
    assert(bounded.length_bars == 1U);
    assert(bounded.style == DrumMachineStyle::rock);

    parameters = bounded;
    parameters.tempo_bpm = std::numeric_limits<double>::quiet_NaN();
    parameters.mix = std::numeric_limits<float>::infinity();
    parameters.volume = std::numeric_limits<float>::infinity();
    parameters.eq_db = std::numeric_limits<float>::quiet_NaN();
    parameters.tuning_semitones = -std::numeric_limits<float>::infinity();
    parameters.style = static_cast<DrumMachineStyle>(255U);
    effect.configure(parameters);
    assert(effect.parameters().tempo_bpm == 300.0);
    assert(effect.parameters().mix == 0.0F);
    assert(effect.parameters().volume == 0.0F);
    assert(effect.parameters().eq_db == 12.0F);
    assert(effect.parameters().tuning_semitones == 12.0F);
    assert(effect.parameters().style == DrumMachineStyle::rock);
}

void test_constructor_rejects_invalid_layout_and_sample_rate() {
    assert(throws_invalid_argument([] { DrumMachine effect(0.0F, 1U); }));
    assert(throws_invalid_argument([] {
        DrumMachine effect(std::numeric_limits<float>::quiet_NaN(), 1U);
    }));
    assert(throws_invalid_argument([] { DrumMachine effect(96001.0F, 1U); }));
    assert(throws_invalid_argument([] { DrumMachine effect(7999.0F, 1U); }));
    assert(throws_invalid_argument([] { DrumMachine effect(kSampleRate, 0U); }));
    assert(throws_invalid_argument([] { DrumMachine effect(kSampleRate, 3U); }));
}

void test_supported_rates_keep_all_reverb_taps_in_bounds() {
    constexpr std::array<float, 4U> delay_ms{17.3F, 21.7F, 25.1F, 29.9F};
    for (const float sample_rate : {44100.0F, 48000.0F, 96000.0F}) {
        DrumMachine effect(sample_rate, 2U);
        for (std::size_t line = 0U; line < delay_ms.size(); ++line) {
            const auto expected = static_cast<std::size_t>(std::lround(
                sample_rate * delay_ms[line] * 0.001F));
            assert(effect.reverb_delay_frames(line) == expected);
            assert(effect.reverb_delay_frames(line) > 0U);
            assert(effect.reverb_delay_frames(line) < 3072U);
        }
        assert(effect.reverb_delay_frames(delay_ms.size()) == 0U);
        std::array<float, 1024U> audio{};
        effect.set_running(true);
        allocation_count.store(0U, std::memory_order_relaxed);
        effect.process(audio, 2U);
        assert(allocation_count.load(std::memory_order_relaxed) == 0U);
        assert(all_finite(audio));
    }
}

void test_generates_audible_finite_stereo_and_mono_output() {
    DrumMachine mono(kSampleRate, 1U);
    mono.set_running(true);
    std::array<float, 48000U> mono_audio{};
    mono.process(mono_audio, 1U);
    assert(energy(mono_audio) > 0.01);
    assert(all_finite(mono_audio));

    DrumMachineParameters parameters;
    parameters.mix = 1.0F;
    DrumMachine stereo(kSampleRate, 2U, parameters);
    stereo.set_running(true);
    std::array<float, 96000U> stereo_audio{};
    stereo.process(stereo_audio, 2U);
    assert(all_finite(stereo_audio));
    double left_energy = 0.0;
    double right_energy = 0.0;
    for (std::size_t frame = 0U; frame < stereo_audio.size() / 2U; ++frame) {
        left_energy += std::abs(stereo_audio[frame * 2U]);
        right_energy += std::abs(stereo_audio[frame * 2U + 1U]);
    }
    assert(left_energy > 1.0);
    assert(right_energy > 1.0);
    assert(std::abs(left_energy - right_energy) > 1.0e-3);
}

void test_silence_mute_and_input_safety() {
    DrumMachineParameters parameters;
    parameters.mix = 1.0F;
    DrumMachine muted(kSampleRate, 1U, parameters);
    muted.set_running(true);
    muted.set_muted(true);
    std::array<float, 4096U> silent{};
    muted.process(silent, 1U);
    for (const float sample : silent) {
        assert(sample == 0.0F);
    }

    std::array<float, 64U> unsafe{};
    unsafe.fill(std::numeric_limits<float>::quiet_NaN());
    muted.set_muted(false);
    muted.process(unsafe, 1U);
    assert(all_finite(unsafe));

    unsafe.fill(std::numeric_limits<float>::infinity());
    muted.process(unsafe, 1U);
    assert(all_finite(unsafe));

    std::array<float, 16U> unsupported{};
    unsupported.fill(0.375F);
    const auto before = unsupported;
    muted.process(unsupported, 3U);
    assert(unsupported == before);
}

void test_volume_and_mix_are_independent_layer_controls() {
    std::array<float, 1024U> passthrough{};
    passthrough.fill(0.125F);
    const auto original = passthrough;

    DrumMachineParameters parameters;
    parameters.mix = 1.0F;
    parameters.volume = 0.0F;
    DrumMachine muted_layer(kSampleRate, 2U, parameters);
    muted_layer.set_running(true);
    muted_layer.process(passthrough, 2U);
    assert(passthrough == original);

    parameters.volume = 1.0F;
    DrumMachine unity_layer(kSampleRate, 2U, parameters);
    unity_layer.set_running(true);
    std::array<float, 1024U> generated{};
    generated.fill(0.125F);
    unity_layer.process(generated, 2U);
    assert(generated != original);
}

void test_timing_pause_variation_and_fill_events() {
    DrumMachine timing(kSampleRate, 1U);
    timing.set_running(true);
    std::array<float, kFourStepsAndMarginFrames> beat{};
    timing.process(beat, 1U);
    assert(timing.current_step() == 4U);
    timing.set_running(false);
    std::array<float, 4096U> paused{};
    timing.process(paused, 1U);
    assert(timing.current_step() == 4U);
    timing.set_running(true);

    DrumMachineParameters phrase_parameters;
    phrase_parameters.length_bars = 2U;
    DrumMachine phrase(kSampleRate, 1U, phrase_parameters);
    phrase.set_running(true);
    std::array<float, 16U * 6000U + 8U> bar{};
    phrase.process(bar, 1U);
    assert(phrase.current_bar() == 1U);
    phrase.process(bar, 1U);
    assert(phrase.current_bar() == 0U);

    for (const auto style : {DrumMachineStyle::rock, DrumMachineStyle::pop,
                             DrumMachineStyle::shuffle,
                             DrumMachineStyle::half_time}) {
        DrumMachineParameters style_parameters;
        style_parameters.mix = 1.0F;
        style_parameters.style = style;
        DrumMachine main(kSampleRate, 1U, style_parameters);
        DrumMachine variation(kSampleRate, 1U, style_parameters);
        main.set_running(true);
        variation.set_running(true);
        variation.set_variation(true);
        std::array<float, 96000U> main_audio{};
        std::array<float, 96000U> variation_audio{};
        main.process(main_audio, 1U);
        variation.process(variation_audio, 1U);
        assert(main_audio != variation_audio);
    }

    DrumMachine no_fill(kSampleRate, 1U);
    DrumMachine fill(kSampleRate, 1U);
    no_fill.set_running(true);
    fill.set_running(true);
    fill.trigger_fill();
    std::array<float, 96000U> no_fill_audio{};
    std::array<float, 96000U> fill_audio{};
    no_fill.process(no_fill_audio, 1U);
    fill.process(fill_audio, 1U);
    assert(no_fill_audio != fill_audio);
}

void test_phrase_length_changes_turnaround_content() {
    DrumMachineParameters one_bar_parameters;
    one_bar_parameters.mix = 1.0F;
    one_bar_parameters.reverb = 0.0F;
    one_bar_parameters.kick_mix = 0.0F;
    one_bar_parameters.snare_mix = 0.0F;
    one_bar_parameters.cymbal_mix = 0.0F;
    one_bar_parameters.tom_mix = 1.0F;
    one_bar_parameters.length_bars = 1U;
    auto two_bar_parameters = one_bar_parameters;
    two_bar_parameters.length_bars = 2U;

    DrumMachine one_bar(kSampleRate, 1U, one_bar_parameters);
    DrumMachine two_bar(kSampleRate, 1U, two_bar_parameters);
    one_bar.set_running(true);
    two_bar.set_running(true);
    std::array<float, 192008U> one_bar_audio{};
    std::array<float, 192008U> two_bar_audio{};
    one_bar.process(one_bar_audio, 1U);
    two_bar.process(two_bar_audio, 1U);
    assert(one_bar_audio != two_bar_audio);
    assert(energy(one_bar_audio) > energy(two_bar_audio));
}

void test_generated_output_is_bounded_at_extreme_project_settings() {
    DrumMachineParameters parameters;
    parameters.mix = 1.0F;
    parameters.intensity = 1.0F;
    parameters.groove = 1.0F;
    parameters.kick_mix = 1.0F;
    parameters.snare_mix = 1.0F;
    parameters.cymbal_mix = 1.0F;
    parameters.tom_mix = 1.0F;
    parameters.eq_db = 12.0F;
    parameters.timbre = 1.0F;
    parameters.tuning_semitones = 12.0F;
    parameters.reverb = 1.0F;
    parameters.drive = 1.0F;
    DrumMachine effect(kSampleRate, 2U, parameters);
    effect.set_running(true);
    std::array<float, 96000U> generated{};
    effect.process(generated, 2U);
    assert(all_finite(generated));
    for (const float sample : generated) {
        assert(std::abs(sample) <= 8.0F);
    }

    generated.fill(std::numeric_limits<float>::max());
    effect.process(generated, 2U);
    assert(all_finite(generated));
    for (const float sample : generated) {
        assert(std::abs(sample) <= 1000008.0F);
    }
}

void test_partition_invariance_and_reset_reproducibility() {
    DrumMachineParameters parameters;
    parameters.tempo_bpm = 127.3;
    parameters.mix = 0.7F;
    parameters.groove = 0.8F;
    parameters.style = DrumMachineStyle::shuffle;
    parameters.length_bars = 3U;
    DrumMachine whole(kSampleRate, 2U, parameters);
    DrumMachine partitioned(kSampleRate, 2U, parameters);
    whole.set_running(true);
    partitioned.set_running(true);
    std::array<float, 32768U * 2U> whole_audio{};
    std::array<float, 32768U * 2U> partitioned_audio{};
    whole.process(whole_audio, 2U);

    std::size_t offset_frames = 0U;
    for (const std::size_t block_frames : {1U, 17U, 128U, 257U, 64U, 511U}) {
        const std::size_t frames = std::min(
            block_frames, 32768U - offset_frames);
        partitioned.process(std::span<float>(
            partitioned_audio.data() + offset_frames * 2U, frames * 2U), 2U);
        offset_frames += frames;
    }
    while (offset_frames < 32768U) {
        const std::size_t frames =
            std::min<std::size_t>(333U, 32768U - offset_frames);
        partitioned.process(std::span<float>(
            partitioned_audio.data() + offset_frames * 2U, frames * 2U), 2U);
        offset_frames += frames;
    }
    assert(whole_audio == partitioned_audio);

    std::array<float, 8192U> after_reset{};
    std::array<float, 8192U> fresh{};
    DrumMachine reset_effect(kSampleRate, 1U, parameters);
    reset_effect.set_running(true);
    reset_effect.process(after_reset, 1U);
    reset_effect.reset();
    reset_effect.process(fresh, 1U);
    assert(after_reset == fresh);
}

void test_process_has_no_allocations_and_reports_memory() {
    DrumMachineParameters parameters;
    parameters.mix = 0.8F;
    parameters.reverb = 0.6F;
    DrumMachine effect(kSampleRate, 2U, parameters);
    effect.set_running(true);
    std::array<float, 4096U> audio{};
    allocation_count.store(0U, std::memory_order_relaxed);
    effect.process(audio, 2U);
    assert(allocation_count.load(std::memory_order_relaxed) == 0U);
    assert(all_finite(audio));
    assert(effect.memory_bytes() == sizeof(effect));
    assert(effect.memory_bytes() >= 49152U);
    assert(effect.memory_bytes() < 55000U);
}

void test_stopped_transport_is_passthrough_until_started() {
    DrumMachineParameters parameters;
    parameters.mix = 1.0F;
    DrumMachine effect(kSampleRate, 2U, parameters);
    std::array<float, 8192U> audio{};
    audio.fill(0.125F);
    const auto original = audio;
    effect.process(audio, 2U);
    assert(audio == original);
    assert(effect.current_step() == 0U);

    effect.set_running(true);
    effect.process(audio, 2U);
    assert(audio != original);
}

}  // namespace

void* operator new(const std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](const std::size_t size) {
    allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    test_defaults_and_controls_are_bounded();
    test_constructor_rejects_invalid_layout_and_sample_rate();
    test_supported_rates_keep_all_reverb_taps_in_bounds();
    test_generates_audible_finite_stereo_and_mono_output();
    test_silence_mute_and_input_safety();
    test_volume_and_mix_are_independent_layer_controls();
    test_timing_pause_variation_and_fill_events();
    test_phrase_length_changes_turnaround_content();
    test_generated_output_is_bounded_at_extreme_project_settings();
    test_partition_invariance_and_reset_reproducibility();
    test_process_has_no_allocations_and_reports_memory();
    test_stopped_transport_is_passthrough_until_started();
    std::fprintf(stdout, "All TabletFX Prime Drum Machine tests passed.\n");
    return 0;
}
