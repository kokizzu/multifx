#include "tabletfx/effects/pitch_feedback_delay.hpp"

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
std::atomic<std::size_t> allocation_count{0};
constexpr float kSampleRate = 48000.0F;
constexpr double kPi = 3.14159265358979323846;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

double amplitude_near(const std::vector<float>& samples, double frequency,
                      std::size_t start_frame, std::size_t length,
                      std::uint16_t channel = 0,
                      std::uint16_t channels = 1) {
    double cosine = 0.0;
    double sine = 0.0;
    double window_sum = 0.0;
    for (std::size_t i = 0; i < length; ++i) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * kPi * static_cast<double>(i) / static_cast<double>(length - 1));
        const double angle = 2.0 * kPi * frequency *
                             static_cast<double>(start_frame + i) /
                             static_cast<double>(kSampleRate);
        const double value = static_cast<double>(
            samples[(start_frame + i) * channels + channel]) * window;
        cosine += value * std::cos(angle);
        sine += value * std::sin(angle);
        window_sum += window;
    }
    return 2.0 * std::hypot(cosine, sine) / window_sum;
}

std::vector<float> impulse_response(float feedback, std::size_t total_frames) {
    tabletfx::PitchFeedbackDelayParameters parameters;
    parameters.time_ms = 200.0F;
    parameters.feedback = feedback;
    parameters.mix = 1.0F;
    parameters.pitch_semitones = 0.0F;
    tabletfx::PitchFeedbackDelay effect(kSampleRate, 1, 500.0F, parameters);
    std::vector<float> output(total_frames, 0.0F);
    output[0] = 1.0F;
    effect.process(output, 1);
    return output;
}

void test_project_authored_bounds_and_identity() {
    tabletfx::PitchFeedbackDelay effect;
    require(effect.id() == "pitch-feedback-delay",
            "Pitch Feedback Delay ID is not stable");
    require(effect.memory_bytes() > sizeof(effect),
            "memory accounting must include ring and pitch-shifter storage");
    require(effect.parameters().time_ms == 350.0F &&
                effect.parameters().feedback == 0.35F &&
                effect.parameters().mix == 0.25F &&
                effect.parameters().pitch_semitones == 7.0F,
            "project-authored defaults changed unexpectedly");

    auto parameters = effect.parameters();
    parameters.time_ms = 9000.0F;
    parameters.feedback = 2.0F;
    parameters.mix = -1.0F;
    parameters.pitch_semitones = 100.0F;
    effect.configure(parameters);
    const auto bounded = effect.parameters();
    require(bounded.time_ms == 2000.0F && bounded.feedback == 0.95F &&
                bounded.mix == 0.0F && bounded.pitch_semitones == 12.0F,
            "controls must remain inside the project-authored bounds");

    parameters.time_ms = std::numeric_limits<float>::quiet_NaN();
    parameters.feedback = std::numeric_limits<float>::infinity();
    parameters.mix = -std::numeric_limits<float>::infinity();
    parameters.pitch_semitones = std::numeric_limits<float>::quiet_NaN();
    effect.configure(parameters);
    require(effect.parameters().time_ms == 350.0F &&
                effect.parameters().feedback == 0.35F &&
                effect.parameters().mix == 0.25F &&
                effect.parameters().pitch_semitones == 7.0F,
            "non-finite controls must fall back to documented defaults");

    tabletfx::PitchFeedbackDelay short_delay(kSampleRate, 1, 100.0F);
    auto short_parameters = short_delay.parameters();
    short_parameters.time_ms = 9000.0F;
    short_delay.configure(short_parameters);
    require(short_delay.parameters().time_ms == 100.0F,
            "the time control must respect the instance's smaller buffer limit");
    short_parameters.time_ms = 1.0F;
    short_delay.configure(short_parameters);
    require(short_delay.parameters().time_ms == 40.0F,
            "the time control must respect its project-authored minimum");
    short_parameters.time_ms = std::numeric_limits<float>::quiet_NaN();
    short_delay.configure(short_parameters);
    require(short_delay.parameters().time_ms == 100.0F,
            "a non-finite time must use a safe fallback within the instance limit");
}

void test_constructor_rejects_invalid_format_and_capacity() {
    const auto throws_invalid_argument = [](auto&& construct) {
        try {
            construct();
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    require(throws_invalid_argument([] {
                tabletfx::PitchFeedbackDelay effect(7999.0F, 1, 100.0F);
            }),
            "sample rates below the supported range must be rejected");
    require(throws_invalid_argument([] {
                tabletfx::PitchFeedbackDelay effect(48000.0F, 3, 100.0F);
            }),
            "channel layouts outside mono/stereo must be rejected");
    require(throws_invalid_argument([] {
                tabletfx::PitchFeedbackDelay effect(48000.0F, 1, 39.0F);
            }),
            "maximum delay capacity below the minimum time must be rejected");
}

void test_repeat_timing_and_stereo_independence() {
    tabletfx::PitchFeedbackDelayParameters parameters;
    parameters.time_ms = 50.0F;
    parameters.feedback = 0.0F;
    parameters.mix = 1.0F;
    parameters.pitch_semitones = 0.0F;
    tabletfx::PitchFeedbackDelay effect(kSampleRate, 2, 250.0F, parameters);

    constexpr std::size_t total_frames = 10000;
    constexpr std::size_t expected_peak = 2400;
    std::vector<float> stereo(total_frames * 2U, 0.0F);
    stereo[0] = 1.0F;
    effect.process(stereo, 2);

    const auto first = std::find_if(stereo.begin(), stereo.end(), [](float sample) {
        return std::abs(sample) > 1.0e-6F;
    });
    require(first != stereo.end(), "the pitch-shifted delay lost its impulse");
    const std::size_t first_sample =
        static_cast<std::size_t>(std::distance(stereo.begin(), first)) / 2U;
    require(first_sample >= expected_peak - 256U &&
                first_sample < expected_peak + 128U,
            "wet onset must follow the configured repeat time and shifter latency");

    std::size_t peak_frame = 0;
    float peak = 0.0F;
    for (std::size_t frame = expected_peak - 256U;
         frame < expected_peak + 512U; ++frame) {
        const float value = std::abs(stereo[frame * 2U]);
        if (value > peak) {
            peak = value;
            peak_frame = frame;
        }
        require(stereo[frame * 2U + 1U] == 0.0F,
                "left input must not leak into the right delay channel");
    }
    require(peak > 1.0e-3F, "the first repeat must have usable wet level");
    require(peak_frame >= expected_peak - 128U &&
                peak_frame <= expected_peak + 256U,
            "first-repeat peak must be near the selected time after latency compensation");
}

void test_feedback_creates_later_repeats() {
    constexpr std::size_t total_frames = 26000;
    constexpr std::size_t first_repeat = 9600;
    constexpr std::size_t second_repeat = 19200;
    const auto no_feedback = impulse_response(0.0F, total_frames);
    const auto with_feedback = impulse_response(0.5F, total_frames);

    const auto peak_in = [](const std::vector<float>& samples,
                            std::size_t start, std::size_t end) {
        float peak = 0.0F;
        for (std::size_t i = start; i < end; ++i) {
            peak = std::max(peak, std::abs(samples[i]));
        }
        return peak;
    };
    const float first_peak = peak_in(with_feedback, first_repeat - 256U,
                                     first_repeat + 512U);
    const float later_without = peak_in(no_feedback, second_repeat - 256U,
                                        second_repeat + 512U);
    const float later_with = peak_in(with_feedback, second_repeat - 256U,
                                     second_repeat + 512U);
    require(first_peak > 1.0e-3F,
            "the impulse must produce a first repeat for feedback comparison");
    require(later_without < 1.0e-4F,
            "zero feedback must suppress a later repeat");
    require(later_with > later_without + 1.0e-4F &&
                later_with > first_peak * 0.05F,
            "positive feedback must produce a later repeat");
}

void test_maximum_feedback_remains_finite_and_bounded() {
    tabletfx::PitchFeedbackDelayParameters parameters;
    parameters.time_ms = 40.0F;
    parameters.feedback = 0.95F;
    parameters.mix = 1.0F;
    parameters.pitch_semitones = 0.0F;
    tabletfx::PitchFeedbackDelay effect(kSampleRate, 1, 100.0F, parameters);

    constexpr std::size_t driven_frames = 48000U;
    constexpr std::size_t tail_frames = 4U * 48000U;
    std::vector<float> audio(driven_frames + tail_frames, 0.0F);
    for (std::size_t frame = 0; frame < driven_frames; ++frame) {
        audio[frame] = static_cast<float>(0.5 * std::sin(
            2.0 * kPi * 110.0 * static_cast<double>(frame) /
            static_cast<double>(kSampleRate)));
    }
    effect.process(audio, 1);
    require(std::all_of(audio.begin(), audio.end(), [](float sample) {
                return std::isfinite(sample) && std::abs(sample) <= 16.0F;
            }),
            "maximum feedback must remain finite and inside the internal safety bound");
}

void test_successive_repeats_accumulate_pitch_shift() {
    tabletfx::PitchFeedbackDelayParameters parameters;
    parameters.time_ms = 250.0F;
    parameters.feedback = 0.5F;
    parameters.mix = 1.0F;
    parameters.pitch_semitones = 7.0F;
    tabletfx::PitchFeedbackDelay effect(kSampleRate, 1, 500.0F, parameters);

    constexpr std::size_t total_frames = 42000;
    constexpr std::size_t input_frames = 9600;
    constexpr std::size_t first_start = 14400;
    constexpr std::size_t first_length = 4800;
    constexpr std::size_t second_start = 26400;
    constexpr std::size_t second_length = 4800;
    std::vector<float> audio(total_frames, 0.0F);
    for (std::size_t frame = 0; frame < input_frames; ++frame) {
        audio[frame] = static_cast<float>(0.2 * std::sin(
            2.0 * kPi * 220.0 * static_cast<double>(frame) /
            static_cast<double>(kSampleRate)));
    }
    effect.process(audio, 1);

    const double first_frequency = 220.0 * std::exp2(7.0 / 12.0);
    const double second_frequency = 220.0 * std::exp2(14.0 / 12.0);
    const double first_shift = amplitude_near(audio, first_frequency,
                                              first_start, first_length);
    const double original_at_first = amplitude_near(audio, 220.0,
                                                    first_start, first_length);
    const double second_shift = amplitude_near(audio, second_frequency,
                                               second_start, second_length);
    const double first_tone_at_second = amplitude_near(audio, first_frequency,
                                                      second_start, second_length);
    require(first_shift > 0.02 && first_shift > original_at_first * 2.0,
            "the first delayed pass must use the configured pitch shift");
    require(second_shift > 0.01 && second_shift > first_tone_at_second * 1.5,
            "feedback repeats must accumulate the configured transposition");
}

void test_block_partitioning_and_reset_are_deterministic() {
    tabletfx::PitchFeedbackDelayParameters parameters;
    parameters.time_ms = 70.0F;
    parameters.feedback = 0.4F;
    parameters.mix = 0.7F;
    parameters.pitch_semitones = -4.0F;
    tabletfx::PitchFeedbackDelay whole(kSampleRate, 2, 200.0F, parameters);
    tabletfx::PitchFeedbackDelay partitioned(kSampleRate, 2, 200.0F, parameters);

    constexpr std::size_t frames = 12000;
    std::vector<float> source(frames * 2U);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        source[frame * 2U] = static_cast<float>(0.2 * std::sin(
            2.0 * kPi * 220.0 * static_cast<double>(frame) /
            static_cast<double>(kSampleRate)));
        source[frame * 2U + 1U] = static_cast<float>(0.17 * std::sin(
            2.0 * kPi * 330.0 * static_cast<double>(frame) /
            static_cast<double>(kSampleRate)));
    }

    auto whole_output = source;
    whole.process(whole_output, 2);
    auto partitioned_output = source;
    constexpr std::array<std::size_t, 7> chunks{1, 7, 127, 128, 513, 31, 64};
    std::size_t offset = 0;
    std::size_t chunk_index = 0;
    while (offset < frames) {
        const std::size_t chunk = std::min(chunks[chunk_index % chunks.size()],
                                           frames - offset);
        auto span = std::span<float>(partitioned_output.data() + offset * 2U,
                                     chunk * 2U);
        partitioned.process(span, 2);
        offset += chunk;
        ++chunk_index;
    }
    require(whole_output.size() == partitioned_output.size(),
            "partition comparison must preserve the stream length");
    for (std::size_t i = 0; i < whole_output.size(); ++i) {
        require(std::abs(whole_output[i] - partitioned_output[i]) <= 1.0e-7F,
                "output must not depend on callback block partitioning");
    }

    whole.reset();
    partitioned.reset();
    tabletfx::PitchFeedbackDelay fresh(kSampleRate, 2, 200.0F, parameters);
    constexpr std::size_t fresh_frames = 12000U;
    std::vector<float> reset_output(fresh_frames * 2U);
    std::vector<float> fresh_output(fresh_frames * 2U);
    for (std::size_t frame = 0; frame < fresh_frames; ++frame) {
        const float left = static_cast<float>(0.1 * std::sin(0.11 * frame));
        const float right = static_cast<float>(0.08 * std::cos(0.07 * frame));
        reset_output[frame * 2U] = fresh_output[frame * 2U] = left;
        reset_output[frame * 2U + 1U] = fresh_output[frame * 2U + 1U] = right;
    }
    whole.process(reset_output, 2);
    fresh.process(fresh_output, 2);
    for (std::size_t i = 0; i < reset_output.size(); ++i) {
        require(std::abs(reset_output[i] - fresh_output[i]) <= 1.0e-7F,
                "reset must clear the full delay and pitch-shifter history");
    }
}

void test_non_finite_extreme_input_recovers_and_process_does_not_allocate() {
    tabletfx::PitchFeedbackDelayParameters parameters;
    parameters.time_ms = 40.0F;
    parameters.feedback = 0.95F;
    parameters.mix = 0.8F;
    parameters.pitch_semitones = -12.0F;
    tabletfx::PitchFeedbackDelay effect(kSampleRate, 2, 100.0F, parameters);

    std::array<float, 512> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
    };
    effect.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(), [](float sample) {
                return std::isfinite(sample) && std::abs(sample) <= 16.0F;
            }),
            "non-finite and extreme samples must be bounded before output");

    std::array<float, 256> zeros{};
    effect.process(zeros, 2);
    require(std::all_of(zeros.begin(), zeros.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "invalid input must not poison later processing state");

    std::array<float, 512> block{};
    for (int i = 0; i < 64; ++i) effect.process(block, 2);
    allocation_count.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    constexpr std::array<std::size_t, 9> frame_counts{1, 7, 15, 31, 32,
                                                      63, 64, 127, 128};
    for (int i = 0; i < 128; ++i) {
        const std::size_t frames =
            frame_counts[static_cast<std::size_t>(i) % frame_counts.size()];
        auto span = std::span<float>(block.data(), frames * 2U);
        effect.process(span, 2);
        for (float sample : span) {
            if (!std::isfinite(sample)) {
                count_allocations.store(false, std::memory_order_release);
                throw std::runtime_error(
                    "irregular process callback produced non-finite output");
            }
        }
    }
    count_allocations.store(false, std::memory_order_release);
    require(allocation_count.load(std::memory_order_relaxed) == 0,
            "process() must not allocate after construction");
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
        test_project_authored_bounds_and_identity();
        test_constructor_rejects_invalid_format_and_capacity();
        test_repeat_timing_and_stereo_independence();
        test_feedback_creates_later_repeats();
        test_maximum_feedback_remains_finite_and_bounded();
        test_successive_repeats_accumulate_pitch_shift();
        test_block_partitioning_and_reset_are_deterministic();
        test_non_finite_extreme_input_recovers_and_process_does_not_allocate();
        std::cout << "Pitch Feedback Delay tests passed\n";
    } catch (const std::exception& exception) {
        count_allocations.store(false, std::memory_order_release);
        std::cerr << "Pitch Feedback Delay test failure: " << exception.what()
                  << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
