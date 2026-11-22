#include "tabletfx/effects/shimmer_reverb.hpp"

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
            2.0 * kPi * static_cast<double>(i) /
            static_cast<double>(length - 1U));
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

double energy(const std::vector<float>& samples, std::size_t start,
              std::size_t end) {
    double sum = 0.0;
    for (std::size_t frame = start; frame < end; ++frame) {
        const double value = samples[frame];
        sum += value * value;
    }
    return sum;
}

void render_in_chunks(tabletfx::ShimmerReverb& effect,
                      std::vector<float>& audio,
                      std::span<const std::size_t> chunk_sizes) {
    std::size_t offset = 0;
    std::size_t index = 0;
    while (offset < audio.size()) {
        const std::size_t count = std::min(
            chunk_sizes[index % chunk_sizes.size()], audio.size() - offset);
        effect.process(std::span<float>(audio.data() + offset, count), 1);
        offset += count;
        ++index;
    }
}

std::vector<float> render_feedback_tail(float feedback) {
    tabletfx::ShimmerReverbParameters parameters;
    parameters.decay_seconds = 0.8F;
    parameters.damping_hz = 7000.0F;
    parameters.feedback = feedback;
    parameters.high_pass_hz = 40.0F;
    parameters.pitch_semitones = 12.0F;
    parameters.shimmer_intensity = 1.0F;
    parameters.mix = 1.0F;
    tabletfx::ShimmerReverb effect(kSampleRate, 1, parameters);

    constexpr std::size_t driven_frames = 9600;
    constexpr std::size_t total_frames = 96000;
    std::vector<float> output(total_frames, 0.0F);
    for (std::size_t frame = 0; frame < driven_frames; ++frame) {
        output[frame] = static_cast<float>(0.22 * std::sin(
            2.0 * kPi * 220.0 * static_cast<double>(frame) /
            static_cast<double>(kSampleRate)));
    }
    effect.process(output, 1);
    return output;
}

void test_identity_bounds_and_constructor_validation() {
    tabletfx::ShimmerReverb effect;
    require(effect.id() == "shimmer-reverb",
            "Shimmer Reverb ID is not stable");
    require(effect.memory_bytes() > sizeof(effect),
            "memory accounting must include the reverb and pitch-shifter state");
    const auto defaults = effect.parameters();
    require(defaults.decay_seconds == 3.0F && defaults.feedback == 0.25F &&
                defaults.pitch_semitones == 12.0F && defaults.mix == 0.25F,
            "project-authored defaults changed unexpectedly");

    auto parameters = defaults;
    parameters.decay_seconds = 20.0F;
    parameters.damping_hz = 40000.0F;
    parameters.diffusion = -1.0F;
    parameters.feedback = 1.0F;
    parameters.high_pass_hz = 9000.0F;
    parameters.pitch_semitones = 20.0F;
    parameters.shimmer_intensity = 2.0F;
    parameters.mix = -1.0F;
    parameters.level_db = 24.0F;
    effect.configure(parameters);
    const auto bounded = effect.parameters();
    require(bounded.decay_seconds == 12.0F && bounded.damping_hz == 18000.0F &&
                bounded.diffusion == 0.0F && bounded.feedback == 0.65F &&
                bounded.high_pass_hz == 1500.0F &&
                bounded.pitch_semitones == 12.0F &&
                bounded.shimmer_intensity == 1.0F && bounded.mix == 0.0F &&
                bounded.level_db == 12.0F,
            "controls must remain inside project-authored bounds");

    parameters = defaults;
    parameters.decay_seconds = std::numeric_limits<float>::quiet_NaN();
    parameters.feedback = std::numeric_limits<float>::infinity();
    parameters.pitch_semitones = std::numeric_limits<float>::quiet_NaN();
    parameters.mix = -std::numeric_limits<float>::infinity();
    effect.configure(parameters);
    const auto recovered = effect.parameters();
    require(recovered.decay_seconds == defaults.decay_seconds &&
                recovered.feedback == defaults.feedback &&
                recovered.pitch_semitones == defaults.pitch_semitones &&
                recovered.mix == defaults.mix,
            "non-finite controls must fall back to project defaults");

    const auto rejects = [](auto&& construct) {
        try {
            construct();
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    require(rejects([] { tabletfx::ShimmerReverb invalid(7999.0F, 1); }),
            "unsupported sample rate must be rejected");
    require(rejects([] { tabletfx::ShimmerReverb invalid(48000.0F, 3); }),
            "unsupported channel count must be rejected");
}

void test_dry_mix_and_channel_guard() {
    tabletfx::ShimmerReverbParameters parameters;
    parameters.mix = 0.0F;
    tabletfx::ShimmerReverb effect(kSampleRate, 1, parameters);
    std::array<float, 8> input{0.2F, -0.3F, 0.0F, 0.5F,
                               -0.1F, 0.4F, -0.2F, 0.1F};
    const auto expected = input;
    effect.process(input, 1);
    for (std::size_t i = 0; i < input.size(); ++i) {
        require(input[i] == expected[i],
                "zero Mix must preserve the dry input exactly");
    }

    std::array<float, 6> wrong_layout{0.1F, 0.2F, 0.3F,
                                      0.4F, 0.5F, 0.6F};
    const auto before = wrong_layout;
    effect.process(wrong_layout, 2);
    require(wrong_layout == before,
            "a mismatched channel layout must not change the buffer");
}

void test_pitch_shifts_the_wet_reverb() {
    tabletfx::ShimmerReverbParameters parameters;
    parameters.decay_seconds = 2.0F;
    parameters.feedback = 0.0F;
    parameters.pitch_semitones = 12.0F;
    parameters.shimmer_intensity = 1.0F;
    parameters.mix = 1.0F;
    tabletfx::ShimmerReverb effect(kSampleRate, 1, parameters);

    constexpr std::size_t driven_frames = 24000;
    constexpr std::size_t total_frames = 48000;
    std::vector<float> audio(total_frames, 0.0F);
    for (std::size_t frame = 0; frame < driven_frames; ++frame) {
        audio[frame] = static_cast<float>(0.2 * std::sin(
            2.0 * kPi * 220.0 * static_cast<double>(frame) /
            static_cast<double>(kSampleRate)));
    }
    effect.process(audio, 1);

    constexpr std::size_t analysis_start = 12000;
    constexpr std::size_t analysis_length = 9600;
    const double shifted = amplitude_near(audio, 440.0, analysis_start,
                                          analysis_length);
    const double unshifted = amplitude_near(audio, 220.0, analysis_start,
                                            analysis_length);
    require(shifted > 1.0e-3,
            "the wet output should contain audible energy at the selected octave");
    require(shifted > unshifted * 1.25,
            "full Shimmer Intensity should favor the pitch-shifted wet signal");
}

void test_feedback_extends_the_reverb_tail() {
    const auto dry_feedback = render_feedback_tail(0.0F);
    const auto high_feedback = render_feedback_tail(0.65F);
    constexpr std::size_t tail_start = 36000;
    constexpr std::size_t tail_end = 84000;
    const double dry_energy = energy(dry_feedback, tail_start, tail_end);
    const double feedback_energy = energy(high_feedback, tail_start, tail_end);
    require(dry_energy > 1.0e-12,
            "the test stimulus must leave a measurable reverb tail");
    require(feedback_energy > dry_energy * 1.02,
            "positive pitch feedback should add energy to the later reverb tail");
    require(std::all_of(high_feedback.begin(), high_feedback.end(),
                        [](float value) {
                            return std::isfinite(value) && std::abs(value) <= 16.0F;
                        }),
            "bounded feedback must keep all output finite and inside its safety limit");
}

void test_irregular_blocks_stereo_isolation_and_reset() {
    tabletfx::ShimmerReverbParameters parameters;
    parameters.decay_seconds = 1.4F;
    parameters.feedback = 0.3F;
    parameters.mix = 0.8F;
    tabletfx::ShimmerReverb whole(kSampleRate, 1, parameters);
    tabletfx::ShimmerReverb partitioned(kSampleRate, 1, parameters);

    constexpr std::size_t frames = 24000;
    std::vector<float> source(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        source[frame] = static_cast<float>(0.12 * std::sin(
            2.0 * kPi * 220.0 * static_cast<double>(frame) /
            static_cast<double>(kSampleRate)));
    }
    auto whole_output = source;
    auto partitioned_output = source;
    whole.process(whole_output, 1);
    constexpr std::array<std::size_t, 7> chunks{1, 7, 127, 128, 513, 31, 64};
    render_in_chunks(partitioned, partitioned_output, chunks);
    for (std::size_t i = 0; i < frames; ++i) {
        require(std::abs(whole_output[i] - partitioned_output[i]) <= 1.0e-7F,
                "output must not depend on callback block partitioning");
    }

    whole.reset();
    tabletfx::ShimmerReverb fresh(kSampleRate, 1, parameters);
    auto after_reset = source;
    auto fresh_output = source;
    whole.process(after_reset, 1);
    fresh.process(fresh_output, 1);
    for (std::size_t i = 0; i < frames; ++i) {
        require(std::abs(after_reset[i] - fresh_output[i]) <= 1.0e-7F,
                "reset must clear plate, pitch, and feedback state");
    }

    tabletfx::ShimmerReverb stereo(kSampleRate, 2, parameters);
    std::array<float, 2048> stereo_audio{};
    for (std::size_t frame = 0; frame < stereo_audio.size() / 2U; ++frame) {
        stereo_audio[frame * 2U] = static_cast<float>(0.1 * std::sin(
            2.0 * kPi * 330.0 * static_cast<double>(frame) /
            static_cast<double>(kSampleRate)));
    }
    stereo.process(stereo_audio, 2);
    for (std::size_t frame = 0; frame < stereo_audio.size() / 2U; ++frame) {
        require(stereo_audio[frame * 2U + 1U] == 0.0F,
                "left-channel input must not leak into the right channel");
        require(std::isfinite(stereo_audio[frame * 2U]),
                "stereo processing must remain finite");
    }
}

void test_process_does_not_allocate_and_recovers_from_invalid_samples() {
    tabletfx::ShimmerReverbParameters parameters;
    parameters.feedback = 0.65F;
    parameters.mix = 0.9F;
    tabletfx::ShimmerReverb effect(kSampleRate, 2, parameters);

    std::array<float, 256> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
    };
    effect.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(), [](float value) {
                return std::isfinite(value) && std::abs(value) <= 16.0F;
            }),
            "invalid input must be sanitized before output");

    std::array<float, 256> block{};
    for (int i = 0; i < 64; ++i) effect.process(block, 2);
    allocation_count.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    constexpr std::array<std::size_t, 8> frames{1, 7, 15, 31, 32, 63, 64, 127};
    for (int i = 0; i < 80; ++i) {
        const std::size_t count = frames[static_cast<std::size_t>(i) % frames.size()];
        auto span = std::span<float>(block.data(), count * 2U);
        effect.process(span, 2);
        for (float value : span) {
            if (!std::isfinite(value) || std::abs(value) > 16.0F) {
                count_allocations.store(false, std::memory_order_release);
                throw std::runtime_error(
                    "irregular process callback produced unsafe output");
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
        test_identity_bounds_and_constructor_validation();
        test_dry_mix_and_channel_guard();
        test_pitch_shifts_the_wet_reverb();
        test_feedback_extends_the_reverb_tail();
        test_irregular_blocks_stereo_isolation_and_reset();
        test_process_does_not_allocate_and_recovers_from_invalid_samples();
        std::cout << "All shimmer reverb tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        count_allocations.store(false, std::memory_order_release);
        std::cerr << "Shimmer Reverb test failure: " << error.what() << '\n';
        return 1;
    }
}
