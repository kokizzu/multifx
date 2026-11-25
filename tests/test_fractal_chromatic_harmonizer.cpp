#include "tabletfx/effects/fractal_chromatic_harmonizer.hpp"
#include "tabletfx/effects/poly_pitch_shifter.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

using Harmonizer = tabletfx::effects::FractalChromaticHarmonizer;
using Voice = Harmonizer::VoiceParameters;

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};
constexpr float kSampleRate = 48000.0F;
constexpr std::size_t kCallbackFrames = 128;
constexpr double kPi = 3.14159265358979323846;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void configure_single_voice(Harmonizer& effect, float shift, float cents,
                            float level = 1.0F, float pan = 0.0F,
                            float delay_ms = 0.0F, float feedback = 0.0F) {
    for (std::uint8_t voice = 0; voice < effect.voice_count(); ++voice) {
        Voice parameters{};
        require(effect.set_voice_parameters(voice, parameters),
                "valid voice parameters were rejected");
    }
    Voice first{};
    first.shift_semitones = shift;
    first.detune_cents = cents;
    first.level = level;
    first.pan = pan;
    first.delay_ms = delay_ms;
    first.feedback = feedback;
    require(effect.set_voice_parameters(0, first),
            "voice zero parameters were rejected");
}

std::vector<float> render_mono(Harmonizer& effect, double frequency,
                               double duration_seconds = 1.0) {
    const std::size_t frames = static_cast<std::size_t>(
        std::lround(kSampleRate * duration_seconds));
    std::vector<float> rendered(frames);
    std::array<float, kCallbackFrames> block{};
    double phase = 0.0;
    for (std::size_t offset = 0; offset < frames;) {
        const std::size_t count = std::min(kCallbackFrames, frames - offset);
        for (std::size_t frame = 0; frame < count; ++frame) {
            block[frame] = static_cast<float>(0.2 * std::sin(phase));
            phase += 2.0 * kPi * frequency / kSampleRate;
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
        }
        auto span = std::span<float>(block.data(), count);
        effect.process(span, 1);
        std::copy_n(block.begin(), count,
                    rendered.begin() + static_cast<std::ptrdiff_t>(offset));
        offset += count;
    }
    return rendered;
}

std::vector<float> render_reference(tabletfx::PolyPitchShifter& effect,
                                   double frequency,
                                   double duration_seconds = 1.0) {
    const std::size_t frames = static_cast<std::size_t>(
        std::lround(kSampleRate * duration_seconds));
    std::vector<float> rendered(frames);
    std::array<float, kCallbackFrames> block{};
    double phase = 0.0;
    for (std::size_t offset = 0; offset < frames;) {
        const std::size_t count = std::min(kCallbackFrames, frames - offset);
        for (std::size_t frame = 0; frame < count; ++frame) {
            block[frame] = static_cast<float>(0.2 * std::sin(phase));
            phase += 2.0 * kPi * frequency / kSampleRate;
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
        }
        auto span = std::span<float>(block.data(), count);
        effect.process(span, 1);
        std::copy_n(block.begin(), count,
                    rendered.begin() + static_cast<std::ptrdiff_t>(offset));
        offset += count;
    }
    return rendered;
}

double amplitude_near(const std::vector<float>& samples, double frequency,
                      std::size_t start = 12000, std::size_t length = 24000) {
    double cosine = 0.0;
    double sine = 0.0;
    double window_sum = 0.0;
    for (std::size_t index = 0; index < length; ++index) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * kPi * static_cast<double>(index) /
            static_cast<double>(length - 1));
        const double angle = 2.0 * kPi * frequency *
                             static_cast<double>(index) / kSampleRate;
        const double value = static_cast<double>(samples[start + index]) * window;
        cosine += value * std::cos(angle);
        sine += value * std::sin(angle);
        window_sum += window;
    }
    return 2.0 * std::hypot(cosine, sine) / window_sum;
}

double amplitude_near_channel(const std::vector<float>& samples, double frequency,
                              std::uint16_t channel,
                              std::size_t start = 12000,
                              std::size_t length = 24000) {
    double cosine = 0.0;
    double sine = 0.0;
    double window_sum = 0.0;
    for (std::size_t index = 0; index < length; ++index) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * kPi * static_cast<double>(index) /
            static_cast<double>(length - 1));
        const double angle = 2.0 * kPi * frequency *
                             static_cast<double>(index) / kSampleRate;
        const double value = static_cast<double>(samples[(start + index) * 2 + channel]) *
                             window;
        cosine += value * std::cos(angle);
        sine += value * std::sin(angle);
        window_sum += window;
    }
    return 2.0 * std::hypot(cosine, sine) / window_sum;
}

std::pair<double, double> peak_near(const std::vector<float>& samples,
                                    double expected_frequency) {
    double best_frequency = expected_frequency;
    double best_amplitude = 0.0;
    for (double frequency = expected_frequency * 0.75;
         frequency <= expected_frequency * 1.25; frequency += 1.0) {
        const double amplitude = amplitude_near(samples, frequency);
        if (amplitude > best_amplitude) {
            best_frequency = frequency;
            best_amplitude = amplitude;
        }
    }
    return {best_frequency, best_amplitude};
}

void test_profile_controls_latency_and_memory() {
    Harmonizer dual(kSampleRate, 1, 2);
    Harmonizer quad(kSampleRate, 2, 4);
    require(dual.id() == "fractal-chromatic-harmonizer",
            "effect ID is not stable");
    require(dual.voice_count() == 2 && quad.voice_count() == 4,
            "two- and four-voice profiles must be distinct");
    require(dual.latency_samples() == 256 && quad.latency_samples() == 256,
            "shared analysis must preserve the 256-sample latency target");
    require(std::abs(quad.algorithmic_latency_ms() - 5.333333F) < 0.01F,
            "48 kHz algorithmic latency must remain about 5.33 ms");
    const std::size_t memory_without_delay = quad.memory_bytes();
    require(memory_without_delay > sizeof(quad) &&
                memory_without_delay < 1024U * 1024U,
            "four-voice stereo state should defer unused delay buffers");
    Voice delayed_voice = quad.voice_parameters(0);
    delayed_voice.delay_ms = 10.0F;
    require(quad.set_voice_parameters(0, delayed_voice) &&
                quad.memory_bytes() > memory_without_delay + 380'000U,
            "enabling one voice delay should allocate its ring on demand");
    std::cout << "Four-voice stereo state without delay: "
              << memory_without_delay << " bytes\n";

    bool rejected = false;
    try {
        Harmonizer invalid_voice_count(kSampleRate, 1, 3);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "only two- and four-voice profiles are supported");
}

void test_parameter_bounds_and_pitch_output() {
    Harmonizer effect(kSampleRate, 1, 4);
    Voice clamped{};
    clamped.shift_semitones = 40.0F;
    clamped.detune_cents = -80.0F;
    clamped.level = 4.0F;
    clamped.pan = -4.0F;
    clamped.delay_ms = 5000.0F;
    clamped.feedback = 2.0F;
    require(effect.set_voice_parameters(0, clamped),
            "finite parameters should be safely clamped");
    const Voice actual = effect.voice_parameters(0);
    require(actual.shift_semitones == 24.0F && actual.detune_cents == -50.0F &&
                actual.level == 2.0F && actual.pan == -1.0F &&
                actual.delay_ms == 2000.0F && actual.feedback == 0.9F,
            "voice parameters did not clamp to documented project limits");

    clamped.shift_semitones = std::numeric_limits<float>::quiet_NaN();
    require(!effect.set_voice_parameters(0, clamped) &&
                effect.voice_parameters(0).shift_semitones == 24.0F,
            "non-finite parameters must be rejected without changing state");
    require(!effect.set_voice_parameters(4, Voice{}),
            "out-of-range voice index must be rejected");

    configure_single_voice(effect, 0.0F, 0.0F);
    const auto neutral = render_mono(effect, 220.0);
    const double neutral_amplitude = amplitude_near(neutral, 220.0);
    tabletfx::PolyPitchShifter neutral_reference(kSampleRate, 1, 24.0F);
    const auto neutral_reference_output = render_reference(neutral_reference, 220.0);
    const double neutral_reference_amplitude =
        amplitude_near(neutral_reference_output, 220.0);
    require(neutral_reference_amplitude > 0.1 &&
                neutral_amplitude >= neutral_reference_amplitude * 0.9,
            "unison output level is materially below the existing shifter path");

    effect.reset();
    configure_single_voice(effect, 12.0F, 50.0F);
    require(effect.voice_parameters(0).level == 1.0F,
            "voice level update was not retained");
    const auto rendered = render_mono(effect, 220.0);
    tabletfx::PolyPitchShifter reference(kSampleRate, 1, 24.0F);
    reference.set_pitch_semitones(12.5F);
    const auto reference_output = render_reference(reference, 220.0);
    const double expected = 220.0 * std::exp2(12.5 / 12.0);
    const double measured = amplitude_near(rendered, expected);
    const auto [peak_hz, peak_amplitude] = peak_near(rendered, expected);
    const double reference_amplitude = amplitude_near(reference_output, expected);
    const double cents_error = 1200.0 * std::log2(peak_hz / expected);
    std::cout << "Shift comparison: custom=" << measured
              << " peak=" << peak_amplitude << " reference="
              << reference_amplitude << " peak_hz=" << peak_hz << '\n';
    require(std::abs(cents_error) <= 30.0,
            "the selected voice's dominant output pitch is inaccurate");
    require(reference_amplitude > 0.1 && measured >= reference_amplitude * 0.9 &&
                peak_amplitude >= reference_amplitude * 0.9,
            "shifted output level is materially below the existing shifter path");
}

void test_independent_voices_render_distinct_pitch_peaks() {
    Harmonizer effect(kSampleRate, 1, 2);
    Voice upper{};
    upper.shift_semitones = 7.0F;
    upper.level = 0.5F;
    Voice lower{};
    lower.shift_semitones = -5.0F;
    lower.level = 0.5F;
    require(effect.set_voice_parameters(0, upper) &&
                effect.set_voice_parameters(1, lower),
            "two independent voice settings were rejected");

    const auto rendered = render_mono(effect, 220.0);
    const double upper_frequency = 220.0 * std::exp2(7.0 / 12.0);
    const double lower_frequency = 220.0 * std::exp2(-5.0 / 12.0);
    require(amplitude_near(rendered, upper_frequency) > 0.04,
            "the upper chromatic voice is missing its expected spectral peak");
    require(amplitude_near(rendered, lower_frequency) > 0.04,
            "the lower chromatic voice is missing its expected spectral peak");
}

void test_quad_delay_feedback_repeats_post_shift_voice() {
    constexpr std::size_t frames = 4096;
    constexpr std::size_t delay_samples = 480;
    auto render = [](float delay_ms, float feedback) {
        Harmonizer effect(kSampleRate, 1, 4);
        for (std::uint8_t index = 0; index < 4; ++index) {
            Voice parameters{};
            if (index == 0) {
                parameters.level = 1.0F;
                parameters.delay_ms = delay_ms;
                parameters.feedback = feedback;
            }
            require(effect.set_voice_parameters(index, parameters),
                    "delay test voice setup failed");
        }
        std::vector<float> rendered(frames, 0.0F);
        std::array<float, kCallbackFrames> block{};
        block[0] = 1.0F;
        for (std::size_t offset = 0; offset < frames;
             offset += kCallbackFrames) {
            if (offset != 0) block.fill(0.0F);
            effect.process(block, 1);
            std::copy(block.begin(), block.end(),
                      rendered.begin() + static_cast<std::ptrdiff_t>(offset));
        }
        return rendered;
    };

    const auto direct = render(0.0F, 0.0F);
    const auto delayed = render(10.0F, 0.5F);
    double largest_echo = 0.0;
    for (std::size_t frame = delay_samples; frame < frames; ++frame) {
        largest_echo = std::max(largest_echo,
                                std::abs(static_cast<double>(delayed[frame] -
                                                             direct[frame])));
        const double expected_feedback = 0.5 * delayed[frame - delay_samples];
        require(std::abs(static_cast<double>(delayed[frame] - direct[frame]) -
                         expected_feedback) < 1.0e-5,
                "post-shift delay feedback did not follow its bounded recurrence");
    }
    require(largest_echo > 1.0e-4,
            "configured post-shift delay produced no audible repeat");
}

void test_mono_stereo_pan_and_mismatched_channel_safety() {
    Harmonizer effect(kSampleRate, 2, 2);
    configure_single_voice(effect, 0.0F, 0.0F, 1.0F, -1.0F);
    std::array<float, kCallbackFrames * 2> block{};
    for (std::size_t frame = 0; frame < kCallbackFrames; ++frame) {
        const float sample = static_cast<float>(0.2 * std::sin(
            2.0 * kPi * 220.0 * static_cast<double>(frame) / kSampleRate));
        block[2 * frame] = sample;
        block[2 * frame + 1] = sample;
    }
    effect.process(block, 2);
    for (std::size_t frame = 0; frame < kCallbackFrames; ++frame) {
        require(std::isfinite(block[2 * frame]) &&
                    std::isfinite(block[2 * frame + 1]),
                "stereo processing produced a non-finite sample");
        require(std::abs(block[2 * frame + 1]) <= 1.0e-6F,
                "hard-left pan leaked into the right output");
    }

    std::array<float, kCallbackFrames> mono{};
    mono.fill(0.25F);
    const auto before = mono;
    effect.process(mono, 1);
    require(mono == before,
            "a channel-count mismatch must leave the input unchanged");
}

void test_stereo_route_uses_one_input_per_voice_pair() {
    Harmonizer effect(kSampleRate, 2, 4, Harmonizer::InputMode::StereoRoute);
    require(effect.input_mode() == Harmonizer::InputMode::StereoRoute,
            "stereo route mode was not retained");
    for (std::uint8_t index = 0; index < 4; ++index) {
        Voice parameters{};
        if (index == 1) {
            parameters.level = 1.0F;
            parameters.shift_semitones = 12.0F;
            parameters.pan = 1.0F;
        }
        require(effect.set_voice_parameters(index, parameters),
                "stereo route test voice setup failed");
    }

    constexpr std::size_t frames = 48000;
    std::vector<float> rendered(frames * 2, 0.0F);
    std::array<float, kCallbackFrames * 2> block{};
    double phase = 0.0;
    for (std::size_t offset = 0; offset < frames; offset += kCallbackFrames) {
        for (std::size_t frame = 0; frame < kCallbackFrames; ++frame) {
            block[2 * frame] = 0.0F;
            block[2 * frame + 1] = static_cast<float>(0.2 * std::sin(phase));
            phase += 2.0 * kPi * 220.0 / kSampleRate;
            if (phase >= 2.0 * kPi) phase -= 2.0 * kPi;
        }
        effect.process(block, 2);
        std::copy(block.begin(), block.end(),
                  rendered.begin() + static_cast<std::ptrdiff_t>(offset * 2));
    }
    require(amplitude_near_channel(rendered, 440.0, 1) > 0.035,
            "right input was not routed to the second chromatic voice");
    require(amplitude_near_channel(rendered, 440.0, 0) < 1.0e-5,
            "stereo route leaked a right-only source into the left output");

    bool rejected_mono = false;
    try {
        Harmonizer invalid(kSampleRate, 1, 2,
                           Harmonizer::InputMode::StereoRoute);
    } catch (const std::invalid_argument&) {
        rejected_mono = true;
    }
    require(rejected_mono, "StereoRoute must reject a mono channel configuration");
}

void test_process_is_allocation_free_and_finite() {
    Harmonizer effect(kSampleRate, 2, 4);
    std::array<float, kCallbackFrames * 2> block{};
    for (std::size_t frame = 0; frame < kCallbackFrames; ++frame) {
        const float value = static_cast<float>(0.15 * std::sin(0.17 * frame));
        block[2 * frame] = value;
        block[2 * frame + 1] = -value;
    }
    for (int iteration = 0; iteration < 16; ++iteration) effect.process(block, 2);

    allocation_count.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    for (int iteration = 0; iteration < 64; ++iteration) {
        Voice parameters = effect.voice_parameters(
            static_cast<std::uint8_t>(iteration % 4));
        parameters.shift_semitones = static_cast<float>((iteration % 49) - 24);
        parameters.detune_cents = static_cast<float>((iteration % 101) - 50);
        (void)effect.set_voice_parameters(
            static_cast<std::uint8_t>(iteration % 4), parameters);
        effect.process(block, 2);
        for (float sample : block) {
            if (!std::isfinite(sample)) {
                count_allocations.store(false, std::memory_order_release);
                throw std::runtime_error("process produced a non-finite sample");
            }
        }
    }
    count_allocations.store(false, std::memory_order_release);
    require(allocation_count.load(std::memory_order_relaxed) == 0,
            "process or parameter updates allocated after construction");
}

void test_zero_level_voices_skip_and_reactivate_cleanly() {
    Harmonizer dual(kSampleRate, 1, 2);
    Harmonizer quad(kSampleRate, 1, 4);
    Voice active{};
    active.shift_semitones = 7.0F;
    active.level = 0.5F;
    require(dual.set_voice_parameters(0, active) &&
                quad.set_voice_parameters(0, active),
            "active voice setup failed");

    constexpr double duration = 1.0;
    const auto dual_only = render_mono(dual, 220.0, duration);
    const auto quad_with_inactive = render_mono(quad, 220.0, duration);
    require(dual_only.size() == quad_with_inactive.size(),
            "two- and four-voice render lengths differ");
    const double upper_frequency = 220.0 * std::exp2(7.0 / 12.0);
    const double dual_upper = amplitude_near(dual_only, upper_frequency);
    const double quad_upper = amplitude_near(quad_with_inactive, upper_frequency);
    require(dual_upper > 0.04 && quad_upper > 0.04 &&
                std::abs(dual_upper - quad_upper) < 0.02,
            "silent quad voices changed the active two-voice pitch result");

    Voice reactivated{};
    reactivated.shift_semitones = -5.0F;
    reactivated.level = 0.5F;
    require(quad.set_voice_parameters(3, reactivated),
            "quad voice activation was rejected");
    quad.reset();
    const auto with_reactivated_voice = render_mono(quad, 220.0, duration);
    const double lower_frequency = 220.0 * std::exp2(-5.0 / 12.0);
    const double reactivated_lower = amplitude_near(
        with_reactivated_voice, lower_frequency);
    require(reactivated_lower > 0.025,
            "a previously skipped voice did not produce its shifted output");

    reactivated.level = 0.0F;
    require(quad.set_voice_parameters(3, reactivated),
            "quad voice deactivation was rejected");
    quad.reset();
    const auto quad_after_deactivation = render_mono(quad, 220.0, duration);
    const double deactivated_lower = amplitude_near(
        quad_after_deactivation, lower_frequency);
    std::cout << "Reactivated voice amplitude: " << reactivated_lower
              << "; after deactivation: " << deactivated_lower << '\n';
    require(reactivated_lower > deactivated_lower * 1.5,
            "reactivated voice did not contribute distinctly to its target pitch");
    require(amplitude_near(quad_after_deactivation, upper_frequency) > 0.04,
            "deactivating one voice disrupted the remaining active voice");
}

void test_reset_matches_fresh_instance_and_latency_is_bounded() {
    Harmonizer reused(kSampleRate, 1, 2);
    Harmonizer fresh(kSampleRate, 1, 2);
    configure_single_voice(reused, 0.0F, 0.0F);
    configure_single_voice(fresh, 0.0F, 0.0F);

    std::array<float, kCallbackFrames> impulse{};
    impulse[0] = 1.0F;
    std::vector<float> output(1024, 0.0F);
    for (std::size_t offset = 0; offset < output.size(); offset += kCallbackFrames) {
        impulse.fill(0.0F);
        if (offset == 0) impulse[0] = 1.0F;
        reused.process(impulse, 1);
        std::copy(impulse.begin(), impulse.end(),
                  output.begin() + static_cast<std::ptrdiff_t>(offset));
    }
    const auto first = std::find_if(output.begin(), output.end(), [](float sample) {
        return std::abs(sample) > 1.0e-7F;
    });
    require(first != output.end(), "neutral pitch path lost the impulse");
    const auto first_sample = static_cast<std::size_t>(
        std::distance(output.begin(), first));
    require(first_sample >= kCallbackFrames && first_sample <= 3 * kCallbackFrames,
            "first audible output fell outside the low-latency window");

    reused.reset();
    std::array<float, kCallbackFrames> reused_block{};
    std::array<float, kCallbackFrames> fresh_block{};
    for (std::size_t frame = 0; frame < kCallbackFrames; ++frame) {
        const float sample = static_cast<float>(0.2 * std::sin(0.1 * frame));
        reused_block[frame] = sample;
        fresh_block[frame] = sample;
    }
    reused.process(reused_block, 1);
    fresh.process(fresh_block, 1);
    require(reused_block == fresh_block,
            "reset state differs from a fresh instance with the same settings");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
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
        test_profile_controls_latency_and_memory();
        test_parameter_bounds_and_pitch_output();
        test_independent_voices_render_distinct_pitch_peaks();
        test_quad_delay_feedback_repeats_post_shift_voice();
        test_mono_stereo_pan_and_mismatched_channel_safety();
        test_stereo_route_uses_one_input_per_voice_pair();
        test_process_is_allocation_free_and_finite();
        test_zero_level_voices_skip_and_reactivate_cleanly();
        test_reset_matches_fresh_instance_and_latency_is_bounded();
        std::cout << "All Fractal chromatic harmonizer tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        count_allocations.store(false, std::memory_order_release);
        std::cerr << "Fractal chromatic harmonizer test failed: "
                  << error.what() << '\n';
        return 1;
    }
}
