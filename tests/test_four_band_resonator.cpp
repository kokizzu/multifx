#include "tabletfx/effects/four_band_resonator.hpp"

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

std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

float energy(std::span<const float> audio) {
    double sum = 0.0;
    for (float sample : audio) {
        sum += static_cast<double>(sample) * sample;
    }
    return static_cast<float>(sum);
}

void test_defaults_bounds_and_memory() {
    tabletfx::FourBandResonator resonator;
    const auto defaults = resonator.parameters();
    require(resonator.id() == "four-band-resonator",
            "stable effect ID changed");
    require(defaults.mode == tabletfx::FourBandResonatorMode::Manual &&
                defaults.master_frequency == 1.0F &&
                defaults.chord_root_frequency_hz == 110.0F &&
                defaults.manual_frequency_hz[0] == 110.0F &&
                defaults.manual_frequency_hz[3] == 329.63F &&
                defaults.feedback[0] == 0.75F && defaults.q[0] == 2.0F &&
                defaults.level[0] == 0.5F && defaults.master_level == 1.0F &&
                defaults.master_pan == 1.0F &&
                defaults.master_feedback == 1.0F &&
                defaults.master_q == 1.0F &&
                defaults.input_mode ==
                    tabletfx::FourBandResonatorInputMode::Mono &&
                defaults.mix == 0.5F,
            "project-authored defaults changed");
    require(resonator.memory_bytes() ==
                sizeof(resonator) + 4U * 2U * 962U * sizeof(float),
            "delay-buffer memory estimate is inaccurate at 48 kHz");
}

void test_manual_frequency_and_parameter_bounds() {
    tabletfx::FourBandResonatorParameters parameters;
    parameters.manual_frequency_hz = {
        std::numeric_limits<float>::quiet_NaN(), -1.0F, 250.0F, 20000.0F};
    parameters.feedback = {-1.0F, 0.5F, 1.0F, 3.0F};
    parameters.q = {0.1F, 1.0F, 6.0F, 100.0F};
    parameters.level = {-1.0F, 0.25F, 1.0F, 100.0F};
    parameters.pan = {-5.0F, -0.25F, 0.5F, 5.0F};
    parameters.master_level = 8.0F;
    parameters.master_pan = -4.0F;
    parameters.master_feedback = -1.0F;
    parameters.master_q = 8.0F;
    parameters.mix = 2.0F;
    tabletfx::FourBandResonator resonator(48000.0F, parameters);
    const auto bounded = resonator.parameters();
    const auto frequencies = resonator.effective_frequencies_hz();
    require(bounded.manual_frequency_hz[0] == 110.0F &&
                bounded.manual_frequency_hz[1] == 100.0F &&
                bounded.manual_frequency_hz[2] == 250.0F &&
                bounded.manual_frequency_hz[3] == 10000.0F,
            "manual frequency bounds or nonfinite fallback failed");
    require(frequencies[0] == 110.0F && frequencies[1] == 100.0F &&
                frequencies[2] == 250.0F && frequencies[3] == 10000.0F,
            "manual mode did not use its bounded frequencies");
    require(bounded.feedback[0] == 0.0F && bounded.feedback[2] == 0.98F &&
                bounded.feedback[3] == 0.98F && bounded.q[0] == 0.5F &&
                bounded.q[3] == 12.0F && bounded.level[0] == 0.0F &&
                bounded.level[3] == 2.0F && bounded.pan[0] == -1.0F &&
                bounded.pan[3] == 1.0F && bounded.master_level == 2.0F &&
                bounded.master_pan == -1.0F &&
                bounded.master_feedback == 0.0F &&
                bounded.master_q == 4.0F && bounded.mix == 1.0F,
            "project-authored parameter bounds failed");

    parameters.master_frequency = 2.0F;
    tabletfx::FourBandResonator scaled(48000.0F, parameters);
    const auto scaled_frequencies = scaled.effective_frequencies_hz();
    require(scaled_frequencies[0] == 220.0F &&
                scaled_frequencies[1] == 200.0F &&
                scaled_frequencies[2] == 500.0F &&
                scaled_frequencies[3] == 20000.0F,
            "Manual master-frequency scaler was not applied");

    parameters.master_frequency = 100.0F;
    tabletfx::FourBandResonator bounded_scaler(48000.0F, parameters);
    require(bounded_scaler.parameters().master_frequency == 10.0F &&
                bounded_scaler.effective_frequencies_hz()[3] <= 21600.0F,
            "project-authored master-frequency bounds failed");
}

void test_chord_frequency_mapping_and_sample_rate_limit() {
    tabletfx::FourBandResonatorParameters parameters;
    parameters.mode = tabletfx::FourBandResonatorMode::Chord;
    parameters.chord = tabletfx::FourBandResonatorChord::Major;
    parameters.master_frequency = 10.0F;
    parameters.chord_root_frequency_hz = 110.0F;
    tabletfx::FourBandResonator major(48000.0F, parameters);
    auto major_frequencies = major.effective_frequencies_hz();
    require(std::abs(major_frequencies[0] - 110.0F) < 0.01F &&
                std::abs(major_frequencies[1] - 138.591F) < 0.02F &&
                std::abs(major_frequencies[2] - 164.814F) < 0.02F &&
                std::abs(major_frequencies[3] - 220.0F) < 0.02F,
            "CHORD mode did not use its root instead of the Manual scaler");

    parameters.chord = tabletfx::FourBandResonatorChord::Diminished;
    parameters.chord_root_frequency_hz = 10000.0F;
    tabletfx::FourBandResonator low_rate(8000.0F, parameters);
    require(low_rate.parameters().chord_root_frequency_hz == 5000.0F,
            "CHORD root frequency did not obey its documented range");
    const auto limited = low_rate.effective_frequencies_hz();
    require(std::all_of(limited.begin(), limited.end(),
                        [](float frequency) {
                            return frequency > 0.0F && frequency <= 3600.1F;
                        }),
            "CHORD frequencies exceeded the sample-rate Nyquist guard");

    parameters.chord = static_cast<tabletfx::FourBandResonatorChord>(255);
    parameters.chord_root_frequency_hz =
        std::numeric_limits<float>::infinity();
    tabletfx::FourBandResonator invalid(48000.0F, parameters);
    require(invalid.parameters().chord ==
                tabletfx::FourBandResonatorChord::Major &&
                invalid.parameters().chord_root_frequency_hz == 110.0F,
            "invalid chord or nonfinite root did not use safe defaults");
}

void test_filter_location_is_independent_per_band() {
    tabletfx::FourBandResonatorParameters parameters;
    parameters.manual_frequency_hz = {220.0F, 330.0F, 440.0F, 660.0F};
    parameters.feedback.fill(0.5F);
    parameters.filter_location.fill(
        tabletfx::FourBandResonatorFilterLocation::Pre);
    parameters.filter_location[1] =
        tabletfx::FourBandResonatorFilterLocation::Post;
    parameters.filter_location[3] =
        static_cast<tabletfx::FourBandResonatorFilterLocation>(255);
    tabletfx::FourBandResonator resonator(48000.0F, parameters);
    const auto normalized = resonator.parameters();
    require(normalized.filter_location[0] ==
                tabletfx::FourBandResonatorFilterLocation::Pre &&
                normalized.filter_location[1] ==
                    tabletfx::FourBandResonatorFilterLocation::Post &&
                normalized.filter_location[2] ==
                    tabletfx::FourBandResonatorFilterLocation::Pre &&
                normalized.filter_location[3] ==
                    tabletfx::FourBandResonatorFilterLocation::Post,
            "per-band PRE/POST filter locations or invalid fallback failed");

    std::array<float, 1024> audio{};
    audio[0] = 1.0F;
    resonator.process(audio, 1);
    require(energy(audio) > 0.0F,
            "mixed PRE/POST resonators produced no output");
}

void test_master_input_routing_and_invalid_input_mode() {
    tabletfx::FourBandResonatorParameters parameters;
    parameters.mode = tabletfx::FourBandResonatorMode::Manual;
    parameters.manual_frequency_hz.fill(220.0F);
    parameters.feedback.fill(0.0F);
    parameters.level = {0.0F, 0.0F, 1.0F, 1.0F};
    parameters.pan = {0.0F, 0.0F, 1.0F, 1.0F};
    parameters.input_mode = tabletfx::FourBandResonatorInputMode::Stereo;
    parameters.master_level = 0.0F;
    parameters.mix = 1.0F;
    tabletfx::FourBandResonator silent(48000.0F, parameters);
    std::array<float, 1024> left_only{};
    left_only[0] = 1.0F;
    silent.process(left_only, 2);
    require(std::all_of(left_only.begin(), left_only.end(),
                        [](float value) { return value == 0.0F; }),
            "Master Level zero did not mute the wet-only signal");

    parameters.master_level = 1.0F;
    parameters.input_mode = tabletfx::FourBandResonatorInputMode::Stereo;
    tabletfx::FourBandResonator routed(48000.0F, parameters);
    std::array<float, 1024> left_input{};
    left_input[0] = 1.0F;
    routed.process(left_input, 2);
    const auto first_left_leak = std::find_if(
        left_input.begin(), left_input.end(),
        [](float value) { return std::abs(value) > 1.0e-8F; });
    if (first_left_leak != left_input.end()) {
        throw std::runtime_error(
            "STEREO input mode routed left input into bands 3 and 4 at sample " +
            std::to_string(first_left_leak - left_input.begin()) +
            " (value " + std::to_string(*first_left_leak) + ")");
    }

    tabletfx::FourBandResonator right_routed(48000.0F, parameters);
    std::array<float, 1024> right_input{};
    right_input[1] = 1.0F;
    right_routed.process(right_input, 2);
    bool right_wet = false;
    for (std::size_t frame = 160U; frame < right_input.size() / 2U; ++frame) {
        require(std::abs(right_input[frame * 2U]) < 1.0e-8F,
                "right-panned bands unexpectedly leaked wet signal left");
        right_wet = right_wet ||
                    std::abs(right_input[frame * 2U + 1U]) > 1.0e-8F;
    }
    require(right_wet, "STEREO input mode produced no right-side resonator tail");

    parameters.input_mode = tabletfx::FourBandResonatorInputMode::Mono;
    tabletfx::FourBandResonator mono_sum(48000.0F, parameters);
    std::array<float, 1024> opposite_polarity{};
    opposite_polarity[0] = 1.0F;
    opposite_polarity[1] = -1.0F;
    mono_sum.process(opposite_polarity, 2);
    require(std::all_of(opposite_polarity.begin(), opposite_polarity.end(),
                        [](float value) { return value == 0.0F; }),
            "MONO input mode did not sum L/R before feeding all four bands");

    parameters.input_mode =
        static_cast<tabletfx::FourBandResonatorInputMode>(255);
    tabletfx::FourBandResonator invalid(48000.0F, parameters);
    require(invalid.parameters().input_mode ==
                tabletfx::FourBandResonatorInputMode::Mono,
            "invalid input mode did not fall back safely");
}

void test_master_pan_scales_and_reverses_band_pans() {
    tabletfx::FourBandResonatorParameters parameters;
    parameters.feedback.fill(0.0F);
    parameters.level = {1.0F, 0.7F, 0.4F, 0.2F};
    parameters.pan = {-1.0F, -0.25F, 0.5F, 1.0F};
    parameters.mix = 1.0F;
    parameters.input_mode = tabletfx::FourBandResonatorInputMode::Mono;

    const auto render = [&parameters](float master_pan) {
        parameters.master_pan = master_pan;
        tabletfx::FourBandResonator resonator(48000.0F, parameters);
        std::array<float, 8192> audio{};
        audio[0] = 1.0F;
        audio[1] = 1.0F;
        resonator.process(audio, 2);
        return audio;
    };

    const auto unchanged = render(1.0F);
    const auto centered = render(0.0F);
    const auto reversed = render(-1.0F);
    for (std::size_t frame = 0; frame < unchanged.size() / 2U; ++frame) {
        const std::size_t left = frame * 2U;
        const std::size_t right = left + 1U;
        require(std::abs(centered[left] - centered[right]) < 1.0e-6F,
                "zero Master Pan did not center the stereo image");
        require(std::abs(unchanged[left] - reversed[right]) < 1.0e-6F &&
                    std::abs(unchanged[right] - reversed[left]) < 1.0e-6F,
                "negative Master Pan did not reverse the complete stereo image");
    }
}

void test_delay_response_mix_and_stereo_isolation() {
    tabletfx::FourBandResonatorParameters parameters;
    parameters.manual_frequency_hz = {220.0F, 330.0F, 440.0F, 660.0F};
    parameters.feedback.fill(0.0F);
    parameters.level.fill(1.0F);
    parameters.pan = {-1.0F, -1.0F, 1.0F, 1.0F};
    parameters.input_mode = tabletfx::FourBandResonatorInputMode::Stereo;
    parameters.mix = 1.0F;

    tabletfx::FourBandResonator mono(48000.0F, parameters);
    std::array<float, 2048> impulse{};
    impulse[0] = 1.0F;
    mono.process(impulse, 1);
    require(std::all_of(impulse.begin(), impulse.begin() + 50,
                        [](float value) { return std::abs(value) < 1.0e-8F; }),
            "resonator produced output before its shortest configured delay");
    require(energy(std::span<const float>(impulse).subspan(50U)) > 1.0e-8F,
            "resonator produced no delayed band-pass output");

    tabletfx::FourBandResonator stereo(48000.0F, parameters);
    std::array<float, 4096> stereo_impulse{};
    stereo_impulse[0] = 1.0F;
    stereo.process(stereo_impulse, 2);
    bool left_wet = false;
    for (std::size_t frame = 0; frame < stereo_impulse.size() / 2U; ++frame) {
        left_wet = left_wet || std::abs(stereo_impulse[frame * 2U]) > 1.0e-7F;
        require(std::abs(stereo_impulse[frame * 2U + 1U]) < 1.0e-8F,
                "independent right-channel resonators received left input");
    }
    require(left_wet, "stereo left channel produced no resonator tail");

    parameters.mix = 0.0F;
    tabletfx::FourBandResonator dry(48000.0F, parameters);
    std::array<float, 8> original{0.1F, -0.2F, 0.3F, -0.4F,
                                  0.5F, -0.6F, 0.7F, -0.8F};
    const auto expected = original;
    dry.process(original, 2);
    require(original == expected, "zero mix changed the dry signal");
}

void test_reset_block_invariance_and_finite_recovery() {
    tabletfx::FourBandResonator whole;
    tabletfx::FourBandResonator split;
    std::vector<float> input(4800U, 0.0F);
    input[0] = 1.0F;
    input[333U] = -0.75F;
    input[1333U] = 0.5F;
    auto whole_audio = input;
    auto split_audio = input;
    whole.process(whole_audio, 1);
    for (std::size_t offset = 0; offset < split_audio.size();) {
        const std::size_t count =
            std::min<std::size_t>(79U, split_audio.size() - offset);
        split.process(std::span<float>(split_audio).subspan(offset, count), 1);
        offset += count;
    }
    require(std::equal(whole_audio.begin(), whole_audio.end(),
                       split_audio.begin(),
                       [](float a, float b) {
                           return std::abs(a - b) < 1.0e-7F;
                       }),
            "resonator output changed across irregular block boundaries");
    require(std::all_of(whole_audio.begin(), whole_audio.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "resonator produced nonfinite output");

    tabletfx::FourBandResonator resettable;
    auto first = input;
    resettable.process(first, 1);
    resettable.reset();
    auto second = input;
    resettable.process(second, 1);
    require(first == second, "reset did not restore deterministic response");

    std::array<float, 6> invalid{0.25F,
                                 std::numeric_limits<float>::quiet_NaN(),
                                 0.5F,
                                 std::numeric_limits<float>::infinity(),
                                 -0.25F,
                                 0.0F};
    resettable.reset();
    resettable.process(invalid, 1);
    require(invalid[1] == 0.0F && invalid[3] == 0.0F &&
                std::all_of(invalid.begin(), invalid.end(),
                            [](float value) { return std::isfinite(value); }),
            "nonfinite input was not contained");
}

void test_process_does_not_allocate() {
    tabletfx::FourBandResonator resonator;
    std::array<float, 256> audio{};
    const std::size_t before = allocation_count.load();
    for (int i = 0; i < 20; ++i) {
        resonator.process(audio, 2);
    }
    const bool allocation_free = allocation_count.load() == before;
    require(allocation_free,
            "process() allocated memory");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1U);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_bounds_and_memory();
        test_manual_frequency_and_parameter_bounds();
        test_chord_frequency_mapping_and_sample_rate_limit();
        test_filter_location_is_independent_per_band();
        test_master_input_routing_and_invalid_input_mode();
        test_master_pan_scales_and_reverses_band_pans();
        test_delay_response_mix_and_stereo_isolation();
        test_reset_block_invariance_and_finite_recovery();
        test_process_does_not_allocate();
        std::cout << "four-band-resonator tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "four-band-resonator test failure: " << error.what() << '\n';
        return 1;
    }
}
