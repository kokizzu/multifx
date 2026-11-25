#include "tabletfx/effects/feedback_generator.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

constexpr float kSampleRate = 48000.0F;
constexpr double kPi = 3.14159265358979323846;

void fill_sine(std::span<float> samples, double frequency_hz,
               double amplitude, std::size_t frame_offset = 0U) {
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const double frame = static_cast<double>(frame_offset + index);
        samples[index] = static_cast<float>(amplitude * std::sin(
            2.0 * kPi * frequency_hz * frame / kSampleRate));
    }
}

double amplitude_near(std::span<const float> samples, double frequency_hz,
                      std::size_t start, std::size_t length) {
    double cosine = 0.0;
    double sine = 0.0;
    double window_sum = 0.0;
    for (std::size_t index = 0; index < length; ++index) {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * kPi * static_cast<double>(index) /
            static_cast<double>(length - 1U));
        const double angle = 2.0 * kPi * frequency_hz *
                             static_cast<double>(start + index) / kSampleRate;
        const double value = static_cast<double>(samples[start + index]) * window;
        cosine += value * std::cos(angle);
        sine += value * std::sin(angle);
        window_sum += window;
    }
    return 2.0 * std::hypot(cosine, sine) / window_sum;
}

void require_finite(std::span<const float> samples) {
    assert(std::all_of(samples.begin(), samples.end(), [](float sample) {
        return std::isfinite(sample);
    }));
}

tabletfx::effects::FeedbackGeneratorParameters clean_parameters() {
    tabletfx::effects::FeedbackGeneratorParameters parameters;
    parameters.feedback_gain = 0.5F;
    parameters.attack_ms = 0.0F;
    parameters.release_ms = 0.0F;
    parameters.dry_level = 0.0F;
    parameters.silence_threshold_db = -72.0F;
    return parameters;
}

void test_defaults_bounds_and_invalid_formats() {
    using namespace tabletfx::effects;
    FeedbackGenerator effect;
    assert(effect.id() == "feedback-generator");
    assert(effect.parameters().type == FeedbackGeneratorType::Unison);
    assert(effect.parameters().feedback_gain == 0.35F);
    assert(effect.parameters().dry_level == 1.0F);
    assert(effect.memory_bytes() > sizeof(effect));

    auto parameters = effect.parameters();
    parameters.feedback_gain = 2.0F;
    parameters.attack_ms = -10.0F;
    parameters.release_ms = 9000.0F;
    parameters.dry_level = 2.0F;
    parameters.silence_threshold_db = -120.0F;
    effect.configure(parameters);
    assert(effect.parameters().feedback_gain == 1.0F);
    assert(effect.parameters().attack_ms == 0.0F);
    assert(effect.parameters().release_ms == 5000.0F);
    assert(effect.parameters().dry_level == 1.0F);
    assert(effect.parameters().silence_threshold_db == -90.0F);

    parameters.feedback_gain = std::numeric_limits<float>::quiet_NaN();
    parameters.release_ms = std::numeric_limits<float>::infinity();
    effect.configure(parameters);
    assert(effect.parameters().feedback_gain == 0.35F);
    assert(effect.parameters().release_ms == 600.0F);

    bool rejected_channels = false;
    try {
        FeedbackGenerator unsupported(kSampleRate, 3U);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    assert(rejected_channels);
    bool rejected_sample_rate = false;
    try {
        FeedbackGenerator unsupported(7999.0F, 1U);
    } catch (const std::invalid_argument&) {
        rejected_sample_rate = true;
    }
    assert(rejected_sample_rate);
}

void test_unison_generates_a_stable_mono_tracked_tone() {
    using namespace tabletfx::effects;
    constexpr std::size_t frames = 12000U;
    std::vector<float> audio(frames, 0.0F);
    fill_sine(audio, 220.0, 0.25);
    auto parameters = clean_parameters();
    parameters.type = FeedbackGeneratorType::Unison;
    FeedbackGenerator effect(kSampleRate, 1U, parameters);
    const std::size_t response_samples =
        effect.pitch_response_latency_samples();
    assert(response_samples > 0U);
    const float expected_response_ms =
        static_cast<float>(response_samples) * 1000.0F / kSampleRate;
    assert(std::abs(effect.pitch_response_latency_ms() - expected_response_ms) <
           1.0e-4F);
    effect.process(audio, 1U);
    require_finite(audio);
    const double level = amplitude_near(audio, 220.0, 6000U, 4000U);
    assert(level > 0.1);
}

void test_octave_modes_shift_generated_feedback() {
    using namespace tabletfx::effects;
    constexpr std::size_t frames = 24000U;
    constexpr std::size_t analysis_start = 12000U;
    constexpr std::size_t analysis_length = 8000U;
    std::vector<float> input(frames, 0.0F);
    std::vector<float> down(frames, 0.0F);
    std::vector<float> up(frames, 0.0F);
    fill_sine(input, 220.0, 0.25);
    down = input;
    up = input;

    auto down_parameters = clean_parameters();
    down_parameters.type = FeedbackGeneratorType::OctaveDown;
    auto up_parameters = clean_parameters();
    up_parameters.type = FeedbackGeneratorType::OctaveUp;
    FeedbackGenerator down_effect(kSampleRate, 1U, down_parameters);
    FeedbackGenerator up_effect(kSampleRate, 1U, up_parameters);
    down_effect.process(down, 1U);
    up_effect.process(up, 1U);
    require_finite(down);
    require_finite(up);

    const double down_level = amplitude_near(
        down, 110.0, analysis_start, analysis_length);
    const double down_unison = amplitude_near(
        down, 220.0, analysis_start, analysis_length);
    const double up_level = amplitude_near(
        up, 440.0, analysis_start, analysis_length);
    const double up_unison = amplitude_near(
        up, 220.0, analysis_start, analysis_length);
    assert(down_level > down_unison * 1.5);
    assert(up_level > up_unison * 1.5);
}

void test_upper_fixed_intervals_generate_the_selected_pitch() {
    using namespace tabletfx::effects;
    struct IntervalCase {
        FeedbackGeneratorType type;
        int semitones;
    };
    constexpr std::array<IntervalCase, 5U> cases{{
        {FeedbackGeneratorType::OctaveAndFifth, 19},
        {FeedbackGeneratorType::TwoOctaves, 24},
        {FeedbackGeneratorType::TwoOctavesAndThird, 28},
        {FeedbackGeneratorType::TwoOctavesAndFifth, 31},
        {FeedbackGeneratorType::TwoOctavesAndSeventh, 35},
    }};
    constexpr std::size_t frames = 36000U;
    constexpr std::size_t analysis_start = 18000U;
    constexpr std::size_t analysis_length = 16000U;

    for (const auto& interval : cases) {
        std::vector<float> audio(frames, 0.0F);
        fill_sine(audio, 220.0, 0.25);
        auto parameters = clean_parameters();
        parameters.type = interval.type;
        FeedbackGenerator effect(kSampleRate, 1U, parameters);
        effect.process(audio, 1U);
        require_finite(audio);

        const double selected_hz = 220.0 * std::exp2(
            static_cast<double>(interval.semitones) / 12.0);
        const double selected_level = amplitude_near(
            audio, selected_hz, analysis_start, analysis_length);
        const double unison_level = amplitude_near(
            audio, 220.0, analysis_start, analysis_length);
        assert(selected_level > 0.08);
        assert(selected_level > unison_level * 4.0);
    }
}

void test_dry_kill_silence_and_safety_ceiling() {
    using namespace tabletfx::effects;
    constexpr std::size_t frames = 8192U;
    std::vector<float> silence(frames, 0.0F);
    auto parameters = clean_parameters();
    parameters.dry_kill = FeedbackGeneratorDryKill::On;
    parameters.feedback_gain = 1.0F;
    FeedbackGenerator effect(kSampleRate, 1U, parameters);
    effect.process(silence, 1U);
    require_finite(silence);
    assert(std::all_of(silence.begin(), silence.end(), [](float sample) {
        return sample == 0.0F;
    }));

    std::array<float, 8192U> exceptional{};
    exceptional.fill(1.0e30F);
    exceptional[3] = std::numeric_limits<float>::quiet_NaN();
    exceptional[9] = std::numeric_limits<float>::infinity();
    exceptional[17] = -std::numeric_limits<float>::infinity();
    auto pass_parameters = clean_parameters();
    pass_parameters.dry_level = 1.0F;
    FeedbackGenerator safety(kSampleRate, 1U, pass_parameters);
    safety.process(exceptional, 1U);
    require_finite(exceptional);
    assert(std::all_of(exceptional.begin(), exceptional.end(), [](float sample) {
        return std::abs(sample) <= 1.0F;
    }));
}

void test_stereo_tracking_and_channel_mismatch_guard() {
    using namespace tabletfx::effects;
    constexpr std::size_t frames = 8192U;
    std::vector<float> stereo(frames * 2U, 0.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float sample = static_cast<float>(0.2 * std::sin(
            2.0 * kPi * 220.0 * static_cast<double>(frame) / kSampleRate));
        stereo[frame * 2U] = sample;
        stereo[frame * 2U + 1U] = -sample;
    }
    auto parameters = clean_parameters();
    FeedbackGenerator effect(kSampleRate, 2U, parameters);
    effect.process(stereo, 2U);
    require_finite(stereo);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        assert(std::abs(stereo[frame * 2U] - stereo[frame * 2U + 1U]) <
               1.0e-6F);
    }

    std::array<float, 32U> unsupported{};
    unsupported.fill(0.25F);
    const auto before = unsupported;
    effect.process(unsupported, 1U);
    assert(unsupported == before);
}

void test_reset_clears_tracker_and_pitch_state() {
    using namespace tabletfx::effects;
    std::vector<float> audio(12000U, 0.0F);
    fill_sine(audio, 220.0, 0.25);
    auto parameters = clean_parameters();
    parameters.type = FeedbackGeneratorType::OctaveUp;
    FeedbackGenerator effect(kSampleRate, 1U, parameters);
    effect.process(audio, 1U);
    effect.reset();
    std::array<float, 8192U> silence{};
    effect.process(silence, 1U);
    assert(std::all_of(silence.begin(), silence.end(), [](float sample) {
        return sample == 0.0F;
    }));
}

}  // namespace

int main() {
    test_defaults_bounds_and_invalid_formats();
    test_unison_generates_a_stable_mono_tracked_tone();
    test_octave_modes_shift_generated_feedback();
    test_upper_fixed_intervals_generate_the_selected_pitch();
    test_dry_kill_silence_and_safety_ceiling();
    test_stereo_tracking_and_channel_mismatch_guard();
    test_reset_clears_tracker_and_pitch_state();
}
