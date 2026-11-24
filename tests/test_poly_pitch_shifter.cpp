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

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};
constexpr float kSampleRate = 48000.0F;
constexpr std::size_t kBlockFrames = 128;
constexpr double kPi = 3.14159265358979323846;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> render_tones(tabletfx::PolyPitchShifter& shifter,
                                const std::vector<double>& frequencies,
                                std::uint16_t channels = 1,
                                double duration_seconds = 1.5) {
    const std::size_t total_frames = static_cast<std::size_t>(
        std::lround(static_cast<double>(kSampleRate) * duration_seconds));
    std::vector<float> output(total_frames * channels);
    std::vector<double> phases(frequencies.size(), 0.0);
    std::array<float, kBlockFrames * 2> block{};
    std::size_t offset = 0;
    while (offset < total_frames) {
        const std::size_t frames = std::min(kBlockFrames, total_frames - offset);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            double sample = 0.0;
            for (std::size_t tone = 0; tone < frequencies.size(); ++tone) {
                sample += 0.16 * std::sin(phases[tone]);
                phases[tone] += 2.0 * kPi * frequencies[tone] /
                                static_cast<double>(kSampleRate);
                if (phases[tone] >= 2.0 * kPi) phases[tone] -= 2.0 * kPi;
            }
            for (std::uint16_t channel = 0; channel < channels; ++channel) {
                block[frame * channels + channel] = static_cast<float>(sample);
            }
        }
        auto span = std::span<float>(block.data(), frames * channels);
        shifter.process(span, channels);
        std::copy_n(block.begin(), frames * channels,
                    output.begin() + static_cast<std::ptrdiff_t>(offset * channels));
        offset += frames;
    }
    return output;
}

double amplitude_near(const std::vector<float>& samples, double frequency,
                      std::uint16_t channel = 0, std::uint16_t channels = 1) {
    constexpr std::size_t start_frame = 24000;
    constexpr std::size_t length = 24000;
    double cosine = 0.0;
    double sine = 0.0;
    double window_sum = 0.0;
    for (std::size_t i = 0; i < length; ++i) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * kPi * static_cast<double>(i) / static_cast<double>(length - 1));
        const double angle = 2.0 * kPi * frequency * static_cast<double>(i) /
                             static_cast<double>(kSampleRate);
        const double value = static_cast<double>(
            samples[(start_frame + i) * channels + channel]) * window;
        cosine += value * std::cos(angle);
        sine += value * std::sin(angle);
        window_sum += window;
    }
    return 2.0 * std::hypot(cosine, sine) / window_sum;
}

double peak_frequency_near(const std::vector<float>& samples,
                           double expected_frequency) {
    double peak_frequency = expected_frequency;
    double peak_amplitude = 0.0;
    const double lower = expected_frequency * 0.92;
    const double upper = expected_frequency * 1.08;
    for (double frequency = lower; frequency <= upper; frequency += 0.5) {
        const double amplitude = amplitude_near(samples, frequency);
        if (amplitude > peak_amplitude) {
            peak_amplitude = amplitude;
            peak_frequency = frequency;
        }
    }
    return peak_frequency;
}

void test_neutral_default_latency_and_memory() {
    tabletfx::PolyPitchShifter shifter(kSampleRate, 1);
    require(shifter.id() == "poly-pitch-shifter", "effect ID is not stable");
    require(shifter.max_shift_semitones() == 12.0F,
            "existing pitch-shifter instances must retain the ±12 default");
    require(shifter.pitch_semitones() == 0.0F,
            "sane default must be neutral pitch");
    require(shifter.analysis_block_samples() == 2048,
            "analysis window does not match the selected low-latency profile");
    require(shifter.analysis_interval_samples() == 64,
            "48 kHz analysis interval does not match the selected profile");
    require(std::abs(shifter.algorithmic_latency_ms() -
                     (1000.0F * 256.0F / kSampleRate)) < 0.01F,
            "reported algorithmic and streaming-buffer latency is incorrect");
    require(shifter.memory_bytes() >= sizeof(shifter) &&
                shifter.memory_bytes() < 1024 * 1024,
            "pitch-shifter state exceeds its 1 MiB memory budget");

    const auto neutral = render_tones(shifter, {220.0});
    const double neutral_frequency = peak_frequency_near(neutral, 220.0);
    const double neutral_cents = 1200.0 * std::log2(neutral_frequency / 220.0);
    require(std::abs(neutral_cents) <= 30.0,
            "neutral setting should preserve steady-state pitch");
    require(amplitude_near(neutral, 220.0) > 0.1,
            "neutral setting should retain usable output level");

    shifter.set_pitch_semitones(100.0F);
    require(shifter.pitch_semitones() == 12.0F, "upper semitone bound failed");
    shifter.set_pitch_semitones(-100.0F);
    require(shifter.pitch_semitones() == -12.0F, "lower semitone bound failed");
    shifter.set_pitch_semitones(std::numeric_limits<float>::quiet_NaN());
    require(shifter.pitch_semitones() == -12.0F,
            "non-finite pitch should preserve the last valid setting");

    std::cout << "Pitch latency: " << shifter.algorithmic_latency_ms()
              << " ms; state: " << shifter.memory_bytes() << " bytes\n";
}

void test_wider_pitch_range_is_opt_in_and_per_instance() {
    tabletfx::PolyPitchShifter default_range(kSampleRate, 1);
    tabletfx::PolyPitchShifter wide_range(kSampleRate, 1, 24.0F);
    tabletfx::PolyPitchShifter scale_degree_range(kSampleRate, 1, 60.0F);
    require(default_range.max_shift_semitones() == 12.0F &&
                wide_range.max_shift_semitones() == 24.0F &&
                scale_degree_range.max_shift_semitones() == 60.0F,
            "a wider range must be opt-in for that processor instance only");

    for (const float semitones : {-24.0F, 24.0F}) {
        wide_range.reset();
        wide_range.set_pitch_semitones(semitones);
        require(wide_range.pitch_semitones() == semitones,
                "the opt-in instance must accept both two-octave endpoints");
        const double input_frequency = 220.0;
        const double expected = input_frequency *
            std::exp2(static_cast<double>(semitones) / 12.0);
        const auto output = render_tones(wide_range, {input_frequency});
        const double actual = peak_frequency_near(output, expected);
        const double cents = 1200.0 * std::log2(actual / expected);
        require(std::abs(cents) <= 45.0,
                "±24-semitone output deviates more than 45 cents on a steady tone");
        require(amplitude_near(output, expected) > 0.025,
                "±24-semitone output level is too weak for the focused test tone");

        constexpr std::array<double, 3> chord{220.0, 277.18263, 329.62756};
        wide_range.reset();
        const auto chord_output = render_tones(wide_range, {chord[0], chord[1], chord[2]});
        for (const double input_frequency : chord) {
            const double target = input_frequency *
                std::exp2(static_cast<double>(semitones) / 12.0);
            require(amplitude_near(chord_output, target) > 0.005,
                    "a polyphonic chord component was lost at a ±24-semitone endpoint");
        }
    }

    default_range.set_pitch_semitones(24.0F);
    require(default_range.pitch_semitones() == 12.0F,
            "existing processor instances must remain clamped to ±12");
    wide_range.set_pitch_semitones(std::numeric_limits<float>::infinity());
    require(wide_range.pitch_semitones() == 24.0F,
            "non-finite updates must preserve the last valid wide-range pitch");

    scale_degree_range.set_pitch_semitones(60.0F);
    require(scale_degree_range.pitch_semitones() == 60.0F,
            "the scale-degree profile must accept its +60-semitone limit");
    const auto extended_output = render_tones(scale_degree_range, {220.0});
    require(amplitude_near(extended_output, 7040.0) > 0.015,
            "the scale-degree profile must render its +60-semitone limit");
    scale_degree_range.set_pitch_semitones(-60.0F);
    require(scale_degree_range.pitch_semitones() == -60.0F,
            "the scale-degree profile must accept its -60-semitone limit");

    bool rejected = false;
    try {
        tabletfx::PolyPitchShifter invalid(kSampleRate, 1, 18.0F);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected,
            "only the documented 12- and opt-in 24-semitone profiles are valid");
}

void test_expression_position_mapping() {
    tabletfx::PolyPitchShifter shifter(kSampleRate, 1);

    shifter.set_pitch_semitones(4.0F);
    shifter.set_expression_position(12.0F, 0.0F);
    require(shifter.pitch_semitones() == 0.0F,
            "expression heel must map to unison");
    shifter.set_expression_position(12.0F, 0.5F);
    require(shifter.pitch_semitones() == 6.0F,
            "expression midpoint must map halfway to the positive target");
    shifter.set_expression_position(12.0F, 1.0F);
    require(shifter.pitch_semitones() == 12.0F,
            "expression toe must reach the positive target");

    shifter.set_expression_position(-12.0F, 0.5F);
    require(shifter.pitch_semitones() == -6.0F,
            "expression midpoint must map halfway to a negative target");
    shifter.set_expression_position(-12.0F, 1.0F);
    require(shifter.pitch_semitones() == -12.0F,
            "expression toe must reach a negative target");

    shifter.set_expression_position(24.0F, 0.5F);
    require(shifter.pitch_semitones() == 6.0F,
            "target must clamp to this instance range before interpolation");
    shifter.set_expression_position(-24.0F, 1.0F);
    require(shifter.pitch_semitones() == -12.0F,
            "reverse target beyond the instance range must clamp safely");
    shifter.set_expression_position(12.0F, -0.5F);
    require(shifter.pitch_semitones() == 0.0F,
            "expression position below zero must clamp to unison");
    shifter.set_expression_position(12.0F, 1.5F);
    require(shifter.pitch_semitones() == 12.0F,
            "expression position above one must clamp to the target");

    shifter.set_pitch_semitones(4.0F);
    shifter.set_expression_position(std::numeric_limits<float>::quiet_NaN(),
                                    0.5F);
    require(shifter.pitch_semitones() == 4.0F,
            "NaN target must preserve the last valid pitch");
    shifter.set_expression_position(12.0F,
                                    std::numeric_limits<float>::quiet_NaN());
    require(shifter.pitch_semitones() == 4.0F,
            "NaN position must preserve the last valid pitch");
    shifter.set_expression_position(
        std::numeric_limits<float>::infinity(), 1.0F);
    require(shifter.pitch_semitones() == 12.0F,
            "infinite positive target must saturate at the supported limit");
    shifter.set_expression_position(
        -std::numeric_limits<float>::infinity(), 1.0F);
    require(shifter.pitch_semitones() == -12.0F,
            "infinite negative target must saturate at the supported limit");
}

void test_pedal_sweep_matches_static_pitch_without_extra_state() {
    tabletfx::PolyPitchShifter pedal_mode(kSampleRate, 1, 24.0F);
    tabletfx::PolyPitchShifter static_mode(kSampleRate, 1, 24.0F);
    require(pedal_mode.memory_bytes() == static_mode.memory_bytes(),
            "expression control must not add shifter state or another processing path");

    for (const float position : {0.0F, 0.5F, 1.0F}) {
        pedal_mode.reset();
        static_mode.reset();
        pedal_mode.set_expression_position(12.0F, position);
        static_mode.set_pitch_semitones(12.0F * position);
        const auto pedal_output = render_tones(pedal_mode, {110.0, 220.0}, 1, 0.3);
        const auto static_output = render_tones(static_mode, {110.0, 220.0}, 1, 0.3);
        require(pedal_output.size() == static_output.size(),
                "pedal sweep and static render lengths differ");
        for (std::size_t i = 0; i < pedal_output.size(); ++i) {
            require(std::abs(pedal_output[i] - static_output[i]) <= 1.0e-6F,
                    "expression sweep output must match the equivalent static pitch");
        }
    }
}

void test_six_semitone_pitch_accuracy() {
    for (const float semitones : {-6.0F, 6.0F}) {
        tabletfx::PolyPitchShifter shifter(kSampleRate, 1);
        shifter.set_pitch_semitones(semitones);
        const auto output = render_tones(shifter, {220.0});
        const double expected = 220.0 * std::exp2(static_cast<double>(semitones) / 12.0);
        const double actual = peak_frequency_near(output, expected);
        const double cents = 1200.0 * std::log2(actual / expected);
        std::cout << "Pitch " << semitones << " semitones: " << actual
                  << " Hz (" << cents << " cents), target-bin amplitude "
                  << amplitude_near(output, expected) << '\n';
        require(std::abs(cents) <= 35.0,
                "steady tone deviates by more than 35 cents after pitch shifting");
        require(amplitude_near(output, expected) > 0.03,
                "shifted fundamental is too weak in the output");
    }
}

void test_chord_components_and_stereo_independence() {
    constexpr std::array<double, 3> chord{110.0, 164.81378, 220.0};
    tabletfx::PolyPitchShifter mono(kSampleRate, 1);
    mono.set_pitch_semitones(5.0F);
    const auto output = render_tones(mono, {chord[0], chord[1], chord[2]});
    bool chord_preserved = true;
    for (const double input_frequency : chord) {
        const double shifted = input_frequency * std::exp2(5.0 / 12.0);
        const double component = amplitude_near(output, shifted);
        std::cout << "Chord component " << input_frequency << " -> " << shifted
                  << " Hz: " << component << '\n';
        chord_preserved = chord_preserved && component > 0.02;
    }
    require(chord_preserved, "a chord tone was lost at its expected shifted frequency");

    for (const float semitones : {-6.0F, 6.0F}) {
        tabletfx::PolyPitchShifter shifted_chord(kSampleRate, 1);
        shifted_chord.set_pitch_semitones(semitones);
        const auto shifted_output = render_tones(
            shifted_chord, {chord[0], chord[1], chord[2]});
        for (const double input_frequency : chord) {
            const double target = input_frequency *
                std::exp2(static_cast<double>(semitones) / 12.0);
            require(amplitude_near(shifted_output, target) > 0.015,
                    "a required -6/+6 semitone chord component was lost");
        }
    }

    tabletfx::PolyPitchShifter stereo(kSampleRate, 2);
    stereo.set_pitch_semitones(7.0F);
    std::vector<float> stereo_output(48000 * 2);
    std::array<float, kBlockFrames * 2> block{};
    double left_phase = 0.0;
    double right_phase = 0.0;
    std::size_t offset = 0;
    while (offset < 48000) {
        for (std::size_t i = 0; i < kBlockFrames; ++i) {
            block[2 * i] = static_cast<float>(0.2 * std::sin(left_phase));
            block[2 * i + 1] = static_cast<float>(0.2 * std::sin(right_phase));
            left_phase += 2.0 * kPi * 220.0 / kSampleRate;
            right_phase += 2.0 * kPi * 330.0 / kSampleRate;
        }
        stereo.process(block, 2);
        std::copy(block.begin(), block.end(), stereo_output.begin() +
                  static_cast<std::ptrdiff_t>(offset * 2));
        offset += kBlockFrames;
    }
    const double left_target = 220.0 * std::exp2(7.0 / 12.0);
    const double right_target = 330.0 * std::exp2(7.0 / 12.0);
    require(amplitude_near(stereo_output, left_target, 0, 2) > 0.04,
            "left stereo pitch component is missing");
    require(amplitude_near(stereo_output, right_target, 1, 2) > 0.04,
            "right stereo pitch component is missing");
}

void test_process_does_not_allocate() {
    tabletfx::PolyPitchShifter shifter(kSampleRate, 2);
    shifter.set_pitch_semitones(-5.0F);
    std::array<float, kBlockFrames * 2> block{};
    for (std::size_t i = 0; i < block.size(); ++i) {
        block[i] = static_cast<float>(0.2 * std::sin(0.13 * static_cast<double>(i)));
    }
    for (int i = 0; i < 64; ++i) shifter.process(block, 2);

    allocation_count.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    constexpr std::array<std::size_t, 10> frame_counts{1, 7, 15, 31, 32, 33, 63, 64, 127, 128};
    for (int i = 0; i < 256; ++i) {
        const std::size_t frames = frame_counts[static_cast<std::size_t>(i) % frame_counts.size()];
        auto variable_block = std::span<float>(block.data(), frames * 2);
        shifter.set_expression_position(12.0F,
                                        static_cast<float>(i % 101) / 100.0F);
        shifter.process(variable_block, 2);
        for (float sample : variable_block) {
            if (!std::isfinite(sample)) {
                count_allocations.store(false, std::memory_order_release);
                throw std::runtime_error("irregular callback produced non-finite output");
            }
        }
    }
    count_allocations.store(false, std::memory_order_release);
    require(allocation_count.load(std::memory_order_relaxed) == 0,
            "process() allocated after constructor warm-up");
}

void test_reset_matches_fresh_instance() {
    tabletfx::PolyPitchShifter reused(kSampleRate, 1);
    tabletfx::PolyPitchShifter fresh(kSampleRate, 1);
    reused.set_pitch_semitones(5.0F);
    fresh.set_pitch_semitones(5.0F);
    (void)render_tones(reused, {440.0}, 1, 0.25);
    reused.reset();

    const auto after_reset = render_tones(reused, {220.0}, 1, 0.75);
    const auto from_fresh = render_tones(fresh, {220.0}, 1, 0.75);
    require(after_reset.size() == from_fresh.size(),
            "reset comparison produced different output lengths");
    for (std::size_t i = 0; i < after_reset.size(); ++i) {
        if (std::abs(after_reset[i] - from_fresh[i]) > 1.0e-6F) {
            throw std::runtime_error("reset pitch state differs from a fresh instance");
        }
    }
}

void test_impulse_latency_is_bounded_by_reported_delay() {
    tabletfx::PolyPitchShifter shifter(kSampleRate, 1);
    constexpr std::size_t total_frames = 2048;
    constexpr std::size_t reported_samples = 256;
    std::array<float, kBlockFrames> block{};
    std::vector<float> output(total_frames, 0.0F);
    for (std::size_t offset = 0; offset < total_frames; offset += kBlockFrames) {
        block.fill(0.0F);
        if (offset == 0) block[0] = 1.0F;
        shifter.process(block, 1);
        std::copy(block.begin(), block.end(),
                  output.begin() + static_cast<std::ptrdiff_t>(offset));
    }
    const auto first = std::find_if(output.begin(), output.end(), [](float value) {
        return std::abs(value) > 1.0e-7F;
    });
    const auto peak = std::max_element(output.begin(), output.end(), [](float lhs, float rhs) {
        return std::abs(lhs) < std::abs(rhs);
    });
    require(first != output.end(), "neutral pitch path lost the impulse");
    const auto first_sample = static_cast<std::size_t>(std::distance(output.begin(), first));
    const auto peak_sample = static_cast<std::size_t>(std::distance(output.begin(), peak));
    require(first_sample >= kBlockFrames && first_sample <= reported_samples + kBlockFrames,
            "first pitch output fell outside the reported latency window");
    require(peak_sample >= reported_samples - kBlockFrames &&
                peak_sample <= reported_samples + 2 * kBlockFrames,
            "neutral pitch impulse peak fell outside the reported latency window");
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
        test_neutral_default_latency_and_memory();
        test_wider_pitch_range_is_opt_in_and_per_instance();
        test_expression_position_mapping();
        test_pedal_sweep_matches_static_pitch_without_extra_state();
        test_six_semitone_pitch_accuracy();
        test_chord_components_and_stereo_independence();
        test_process_does_not_allocate();
        test_reset_matches_fresh_instance();
        test_impulse_latency_is_bounded_by_reported_delay();
        std::cout << "All polyphonic pitch-shifter tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        count_allocations.store(false, std::memory_order_release);
        std::cerr << "Polyphonic pitch-shifter test failed: " << error.what() << '\n';
        return 1;
    }
}
