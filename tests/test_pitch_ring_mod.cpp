#include "tabletfx/effects/pitch_ring_mod.hpp"

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

using tabletfx::effects::PitchRingMod;
using tabletfx::effects::PitchRingModParameters;

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0U};
constexpr float kSampleRate = 48000.0F;
constexpr double kPi = 3.14159265358979323846;
constexpr std::size_t kFramesPerBlock = 128U;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void fill_sine(std::span<float> samples, double frequency_hz,
               double sample_rate, double amplitude, std::uint16_t channels,
               std::size_t start_frame = 0U) {
    const std::size_t frames = samples.size() / channels;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const double absolute_frame =
            static_cast<double>(start_frame + frame);
        const float value = static_cast<float>(
            amplitude * std::sin(2.0 * kPi * frequency_hz * absolute_frame /
                                 sample_rate));
        for (std::uint16_t channel = 0U; channel < channels; ++channel) {
            samples[frame * channels + channel] = value;
        }
    }
}

void feed_tone(PitchRingMod& effect, double frequency_hz,
               std::size_t frame_count, std::uint16_t channels,
               double amplitude = 0.4, double sample_rate = kSampleRate) {
    std::array<float, kFramesPerBlock * 2U> block{};
    std::size_t frame_offset = 0U;
    while (frame_offset < frame_count) {
        const std::size_t frames = std::min(
            kFramesPerBlock, frame_count - frame_offset);
        auto samples = std::span<float>(block.data(), frames * channels);
        fill_sine(samples, frequency_hz, sample_rate, amplitude, channels,
                  frame_offset);
        effect.process(samples, channels);
        frame_offset += frames;
    }
}

void feed_silence(PitchRingMod& effect, std::size_t frame_count,
                  std::uint16_t channels) {
    std::array<float, kFramesPerBlock * 2U> block{};
    std::size_t remaining = frame_count;
    while (remaining != 0U) {
        const std::size_t frames = std::min(kFramesPerBlock, remaining);
        auto samples = std::span<float>(block.data(), frames * channels);
        std::fill(samples.begin(), samples.end(), 0.0F);
        effect.process(samples, channels);
        remaining -= frames;
    }
}

double tone_amplitude(std::span<const float> samples, double frequency_hz,
                      std::size_t first_frame, std::size_t end_frame,
                      std::uint16_t channels = 1U) {
    double cosine = 0.0;
    double sine = 0.0;
    for (std::size_t frame = first_frame; frame < end_frame; ++frame) {
        const double phase = 2.0 * kPi * frequency_hz *
                             static_cast<double>(frame) /
                             static_cast<double>(kSampleRate);
        const double value = samples[frame * channels];
        cosine += value * std::cos(phase);
        sine += value * std::sin(phase);
    }
    const double count = static_cast<double>(end_frame - first_frame);
    return 2.0 * std::hypot(cosine, sine) / count;
}

void test_project_defaults_bounds_and_construction() {
    PitchRingMod effect;
    require(effect.id() == "pitch-ring-mod", "stable effect ID changed");
    require(effect.parameters().carrier_ratio == 1.0F &&
                effect.parameters().fallback_pitch_hz == 110.0F &&
                effect.parameters().tracking_confidence == 0.55F &&
                effect.parameters().mix == 0.5F &&
                effect.parameters().level_db == 0.0F,
            "project-authored defaults changed");
    require(effect.memory_bytes() == sizeof(effect) &&
                !effect.has_tracked_pitch() &&
                effect.carrier_frequency_hz() == 110.0F,
            "initial fallback state or memory report is incorrect");

    auto parameters = effect.parameters();
    parameters.carrier_ratio = 12.0F;
    parameters.fallback_pitch_hz = 10.0F;
    parameters.tracking_confidence = 1.0F;
    parameters.mix = -1.0F;
    parameters.level_db = 50.0F;
    effect.configure(parameters);
    require(effect.parameters().carrier_ratio == 4.0F &&
                effect.parameters().fallback_pitch_hz == 55.0F &&
                effect.parameters().tracking_confidence == 0.98F &&
                effect.parameters().mix == 0.0F &&
                effect.parameters().level_db == 12.0F &&
                effect.carrier_frequency_hz() == 220.0F,
            "project-authored bounds were not enforced");

    parameters = effect.parameters();
    parameters.carrier_ratio = std::numeric_limits<float>::quiet_NaN();
    parameters.fallback_pitch_hz = std::numeric_limits<float>::infinity();
    parameters.tracking_confidence = std::numeric_limits<float>::quiet_NaN();
    parameters.mix = std::numeric_limits<float>::infinity();
    parameters.level_db = -std::numeric_limits<float>::infinity();
    effect.configure(parameters);
    require(effect.parameters().carrier_ratio == 1.0F &&
                effect.parameters().fallback_pitch_hz == 110.0F &&
                effect.parameters().tracking_confidence == 0.55F &&
                effect.parameters().mix == 0.5F &&
                effect.parameters().level_db == 0.0F,
            "non-finite controls did not fall back to safe project defaults");

    bool rejected_rate = false;
    try {
        PitchRingMod unsupported(7999.0F, 1U);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    require(rejected_rate, "unsupported sample rate was accepted");

    bool rejected_channels = false;
    try {
        PitchRingMod unsupported(kSampleRate, 3U);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    require(rejected_channels, "more than two channels were accepted");
}

void test_monophonic_tracking_warmup_and_ring_sidebands() {
    PitchRingModParameters parameters;
    parameters.mix = 1.0F;
    PitchRingMod effect(kSampleRate, 1U, parameters);

    feed_tone(effect, 220.0, 96000U, 1U);
    require(effect.has_tracked_pitch(), "valid monophonic tone was not tracked");
    require(std::abs(effect.tracked_pitch_hz() - 220.0F) < 4.0F,
            "monophonic tone tracking is outside tolerance");
    require(effect.pitch_confidence() >= parameters.tracking_confidence &&
                std::abs(effect.carrier_frequency_hz() - 220.0F) < 4.0F,
            "tracked carrier or confidence is incorrect");

    PitchRingModParameters doubled_parameters = parameters;
    doubled_parameters.carrier_ratio = 2.0F;
    PitchRingMod doubled(kSampleRate, 1U, doubled_parameters);
    std::vector<float> audio(48000U, 0.0F);
    fill_sine(audio, 220.0, kSampleRate, 0.4, 1U);
    doubled.process(audio, 1U);
    require(std::abs(doubled.carrier_frequency_hz() - 440.0F) < 8.0F,
            "carrier ratio did not scale the tracked pitch");
    const double upper_sideband = tone_amplitude(audio, 660.0, 12000U,
                                                 45000U);
    require(upper_sideband > 0.12,
            "ring modulation did not create the expected upper sideband");

    PitchRingMod warmup(kSampleRate, 1U);
    feed_tone(warmup, 220.0, 2048U, 1U);
    require(warmup.has_tracked_pitch(),
            "default tracker did not complete its 512-frame warm-up");
}

void test_weak_input_and_silence_return_to_configured_fallback() {
    PitchRingModParameters parameters;
    parameters.fallback_pitch_hz = 146.83F;
    PitchRingMod effect(kSampleRate, 1U, parameters);
    feed_tone(effect, 220.0, 48000U, 1U);
    require(effect.has_tracked_pitch(), "strong tone did not establish tracking");

    feed_tone(effect, 440.0, 12000U, 1U, 0.0001);
    require(!effect.has_tracked_pitch(),
            "very weak input should release tracking to fallback");
    require(effect.pitch_confidence() == 0.0F &&
                std::abs(effect.tracked_pitch_hz() - 146.83F) < 0.01F &&
                std::abs(effect.carrier_frequency_hz() - 146.83F) < 0.01F,
            "weak-input fallback pitch or confidence is incorrect");

    feed_tone(effect, 220.0, 48000U, 1U);
    require(effect.has_tracked_pitch(), "tracker did not recover after weak input");
    feed_silence(effect, 24000U, 1U);
    require(!effect.has_tracked_pitch() &&
                std::abs(effect.carrier_frequency_hz() - 146.83F) < 0.01F,
            "silence did not return to the configured fallback");
}

void test_stereo_linking_phase_cancellation_and_channel_mismatch() {
    PitchRingMod stereo(kSampleRate, 2U);
    std::array<float, 24000U> audio{};
    for (std::size_t frame = 0U; frame < audio.size() / 2U; ++frame) {
        const float sample = 0.4F * std::sin(
            static_cast<float>(2.0 * kPi * 220.0 *
                               static_cast<double>(frame) / kSampleRate));
        audio[frame * 2U] = sample;
        audio[frame * 2U + 1U] = 0.5F * sample;
    }
    stereo.process(audio, 2U);
    require(stereo.has_tracked_pitch() &&
                std::abs(stereo.tracked_pitch_hz() - 220.0F) < 5.0F,
            "linked stereo input was not tracked");
    for (std::size_t frame = 0U; frame < audio.size() / 2U; ++frame) {
        require(std::abs(audio[frame * 2U] -
                         2.0F * audio[frame * 2U + 1U]) < 1.0e-6F,
                "stereo channels did not share the same carrier");
    }

    PitchRingMod canceled(kSampleRate, 2U);
    for (std::size_t frame = 0U; frame < audio.size() / 2U; ++frame) {
        const float sample = 0.4F * std::sin(
            static_cast<float>(2.0 * kPi * 220.0 *
                               static_cast<double>(frame) / kSampleRate));
        audio[frame * 2U] = sample;
        audio[frame * 2U + 1U] = -sample;
    }
    canceled.process(audio, 2U);
    require(!canceled.has_tracked_pitch() &&
                canceled.carrier_frequency_hz() == 110.0F,
            "out-of-phase stereo sum should use the fallback pitch");

    std::array<float, 7U> mismatch{0.1F, -0.2F, 0.3F, -0.4F,
                                   0.5F, -0.6F, 0.7F};
    const auto before = mismatch;
    stereo.process(mismatch, 1U);
    require(mismatch == before,
            "channel mismatch should leave samples unchanged");
}

void test_note_response_reset_and_block_partitioning() {
    PitchRingMod step(kSampleRate, 1U);
    feed_tone(step, 220.0, 48000U, 1U);
    require(step.has_tracked_pitch(), "initial note was not tracked");
    const float old_pitch = step.tracked_pitch_hz();
    std::array<float, kFramesPerBlock> step_block{};
    std::size_t response_frames = 0U;
    constexpr std::size_t kStepResponseLimitFrames = 4800U;
    while (response_frames < kStepResponseLimitFrames &&
           std::abs(step.tracked_pitch_hz() - 440.0F) > 5.0F) {
        fill_sine(step_block, 440.0, kSampleRate, 0.4, 1U, response_frames);
        step.process(step_block, 1U);
        response_frames += kFramesPerBlock;
    }
    require(std::abs(step.tracked_pitch_hz() - 440.0F) <= 5.0F,
            "tracker did not settle on the higher note");
    require(response_frames <= kStepResponseLimitFrames &&
                response_frames != 0U && old_pitch < 300.0F,
            "220-to-440 Hz step exceeded the 100 ms project response bound");

    constexpr std::size_t frames = 24000U;
    std::vector<float> whole(frames * 2U, 0.0F);
    std::vector<float> partitioned(frames * 2U, 0.0F);
    fill_sine(whole, 196.0, 44100.0, 0.3, 2U);
    partitioned = whole;
    PitchRingMod whole_effect(44100.0F, 2U);
    PitchRingMod split_effect(44100.0F, 2U);
    whole_effect.process(whole, 2U);

    std::size_t frame_offset = 0U;
    std::size_t chunk_index = 0U;
    constexpr std::array<std::size_t, 7U> chunk_frames{
        13U, 97U, 5U, 256U, 61U, 389U, 1024U};
    while (frame_offset < frames) {
        const std::size_t count = std::min(
            chunk_frames[chunk_index % chunk_frames.size()],
            frames - frame_offset);
        const std::size_t sample_offset = frame_offset * 2U;
        split_effect.process(
            std::span<float>(partitioned).subspan(sample_offset, count * 2U),
            2U);
        frame_offset += count;
        ++chunk_index;
    }
    require(whole == partitioned,
            "output depends on callback partitioning");
    require(whole_effect.has_tracked_pitch() && split_effect.has_tracked_pitch() &&
                std::abs(whole_effect.tracked_pitch_hz() -
                         split_effect.tracked_pitch_hz()) < 1.0e-6F,
            "tracking state depends on callback partitioning");

    split_effect.reset();
    PitchRingMod fresh_effect(44100.0F, 2U);
    std::array<float, 512U> reset_a{};
    std::array<float, 512U> reset_b{};
    fill_sine(reset_a, 196.0, 44100.0, 0.3, 2U);
    reset_b = reset_a;
    split_effect.process(reset_a, 2U);
    fresh_effect.process(reset_b, 2U);
    require(reset_a == reset_b,
            "reset did not restore deterministic tracker and phase state");
}

void test_finite_samples_and_process_allocations() {
    PitchRingMod effect(kSampleRate, 2U);
    std::array<float, 256U> invalid{};
    invalid[4U] = std::numeric_limits<float>::quiet_NaN();
    invalid[9U] = std::numeric_limits<float>::infinity();
    invalid[13U] = -std::numeric_limits<float>::infinity();
    invalid[17U] = std::numeric_limits<float>::max();
    invalid[19U] = -std::numeric_limits<float>::max();
    effect.process(invalid, 2U);
    require(std::all_of(invalid.begin(), invalid.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "non-finite or extreme input produced non-finite output");

    PitchRingMod no_allocation(kSampleRate, 2U);
    std::array<float, kFramesPerBlock * 2U> block{};
    fill_sine(block, 220.0, kSampleRate, 0.4, 2U);
    const std::size_t before =
        allocation_count.load(std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    for (std::size_t iteration = 0U; iteration < 2000U; ++iteration) {
        no_allocation.process(block, 2U);
    }
    count_allocations.store(false, std::memory_order_release);
    const std::size_t after =
        allocation_count.load(std::memory_order_relaxed);
    require(after == before, "process() allocated after construction");
}

}  // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
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
    try {
        test_project_defaults_bounds_and_construction();
        test_monophonic_tracking_warmup_and_ring_sidebands();
        test_weak_input_and_silence_return_to_configured_fallback();
        test_stereo_linking_phase_cancellation_and_channel_mismatch();
        test_note_response_reset_and_block_partitioning();
        test_finite_samples_and_process_allocations();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Pitch Ring Mod test failed: " << error.what() << '\n';
        return 1;
    }
}
