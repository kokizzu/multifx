#include "tabletfx/effects/tremolo.hpp"

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
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_defaults_bounds_and_memory() {
    tabletfx::Tremolo tremolo;
    const auto parameters = tremolo.parameters();
    require(tremolo.id() == "tremolo", "tremolo stable ID changed");
    require(parameters.rate_hz == 5.0F && parameters.depth == 0.5F &&
                parameters.lfo_clip_percent == 0.0F &&
                parameters.duty_cycle_percent == 50.0F &&
                parameters.waveform == tabletfx::TremoloWaveform::Sine &&
                !parameters.tempo_sync && parameters.depth_smoothing_ms == 10.0F &&
                tremolo.effective_rate_hz() == 5.0F,
            "sane tremolo defaults changed");
    require(tremolo.memory_bytes() == sizeof(tremolo),
            "tremolo memory report is inaccurate");

    std::vector<float> samples(9600, 1.0F);
    tremolo.process(samples, 1);
    const auto limits = std::minmax_element(samples.begin(), samples.end());
    require(*limits.first >= 0.499F && *limits.first < 0.51F,
            "default sine tremolo minimum level is outside the depth setting");
    require(*limits.second > 0.99F && *limits.second <= 1.0F,
            "tremolo increased signal above its full-volume ceiling");

    tremolo.set_rate_hz(-2.0F);
    require(tremolo.parameters().rate_hz == 0.05F &&
                tremolo.effective_rate_hz() == 0.05F,
            "low free-rate bound was not enforced");
    tremolo.set_rate_hz(100.0F);
    require(tremolo.parameters().rate_hz == 20.0F &&
                tremolo.effective_rate_hz() == 20.0F,
            "high free-rate bound was not enforced");
    tremolo.set_rate_hz(std::numeric_limits<float>::quiet_NaN());
    require(tremolo.parameters().rate_hz == 20.0F,
            "non-finite free rate changed the last valid setting");
    tremolo.set_depth(-1.0F);
    require(tremolo.parameters().depth == 0.0F,
            "negative depth was not clamped to zero");
    tremolo.set_depth(2.0F);
    require(tremolo.parameters().depth == 1.0F,
            "depth above unity was not clamped");
    tremolo.set_depth(std::numeric_limits<float>::infinity());
    require(tremolo.parameters().depth == 1.0F,
            "non-finite depth changed the last valid setting");
    tremolo.set_lfo_clip_percent(-1.0F);
    require(tremolo.parameters().lfo_clip_percent == 0.0F,
            "negative LFO clip amount was not clamped to zero");
    tremolo.set_lfo_clip_percent(150.0F);
    require(tremolo.parameters().lfo_clip_percent == 100.0F,
            "LFO clip amount above 100 percent was not clamped");
    tremolo.set_lfo_clip_percent(std::numeric_limits<float>::quiet_NaN());
    require(tremolo.parameters().lfo_clip_percent == 100.0F,
            "non-finite LFO clip amount changed the last valid setting");
    tremolo.set_duty_cycle_percent(0.0F);
    require(tremolo.parameters().duty_cycle_percent == 5.0F,
            "square duty cycle lower bound was not enforced");
    tremolo.set_duty_cycle_percent(100.0F);
    require(tremolo.parameters().duty_cycle_percent == 95.0F,
            "square duty cycle upper bound was not enforced");
    tremolo.set_duty_cycle_percent(std::numeric_limits<float>::quiet_NaN());
    require(tremolo.parameters().duty_cycle_percent == 95.0F,
            "non-finite square duty cycle changed the last valid setting");
    tremolo.set_depth_smoothing_ms(-1.0F);
    require(tremolo.parameters().depth_smoothing_ms == 0.0F,
            "negative depth smoothing was not bounded to immediate response");
    tremolo.set_depth_smoothing_ms(2000.0F);
    require(tremolo.parameters().depth_smoothing_ms == 1000.0F,
            "depth smoothing upper bound was not enforced");
    tremolo.set_depth_smoothing_ms(std::numeric_limits<float>::quiet_NaN());
    require(tremolo.parameters().depth_smoothing_ms == 10.0F,
            "non-finite depth smoothing did not restore its safe default");
}

void test_lfo_clip_shape_and_default_identity() {
    tabletfx::TremoloParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.depth_smoothing_ms = 0.0F;
    std::vector<float> unmodified_samples(8000, 1.0F);
    tabletfx::Tremolo unmodified(8000.0F, parameters);
    unmodified.process(unmodified_samples, 1);

    parameters.lfo_clip_percent = 100.0F;
    std::vector<float> clipped_samples(8000, 1.0F);
    tabletfx::Tremolo clipped(8000.0F, parameters);
    clipped.process(clipped_samples, 1);
    require(std::abs(unmodified_samples[1000] - 0.8535534F) < 0.002F &&
                clipped_samples[1000] > 0.999F,
            "maximum LFO clipping did not flatten the positive sine peak");
    require(unmodified_samples[5000] > 0.146F &&
                clipped_samples[5000] < 0.001F,
            "maximum LFO clipping did not flatten the negative sine peak");
    require(*std::min_element(clipped_samples.begin(), clipped_samples.end()) >= 0.0F &&
                *std::max_element(clipped_samples.begin(), clipped_samples.end()) <= 1.0F,
            "LFO clipping exceeded the tremolo gain limits");

    parameters.lfo_clip_percent = 0.0F;
    std::vector<float> default_samples(8000, 1.0F);
    tabletfx::Tremolo default_clip(8000.0F, parameters);
    default_clip.process(default_samples, 1);
    require(default_samples == unmodified_samples,
            "zero LFO clipping changed the existing tremolo waveform");
}

void test_waveforms_and_depth_endpoints() {
    tabletfx::TremoloParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.waveform = tabletfx::TremoloWaveform::Square;
    tabletfx::Tremolo square(8000.0F, parameters);
    std::vector<float> square_samples(8000, 1.0F);
    square.process(square_samples, 1);
    require(square_samples[100] == 1.0F && square_samples[3900] == 1.0F,
            "square-wave tremolo high phase should preserve full volume");
    require(square_samples[4100] == 0.0F && square_samples[7900] == 0.0F,
            "square-wave tremolo low phase should reach its depth floor");

    parameters.waveform = tabletfx::TremoloWaveform::Triangle;
    tabletfx::Tremolo triangle(8000.0F, parameters);
    std::vector<float> triangle_samples(8000, 1.0F);
    triangle.process(triangle_samples, 1);
    require(triangle_samples[0] < 0.001F &&
                std::abs(triangle_samples[2000] - 0.5F) < 0.002F &&
                triangle_samples[4000] > 0.999F,
            "triangle waveform did not sweep continuously between depth endpoints");

    parameters.waveform = tabletfx::TremoloWaveform::Sine;
    tabletfx::Tremolo sine(8000.0F, parameters);
    std::vector<float> sine_samples(8000, 1.0F);
    sine.process(sine_samples, 1);
    require(std::abs(sine_samples[0] - 0.5F) < 1.0e-6F &&
                sine_samples[2000] > 0.999F && sine_samples[6000] < 0.001F,
            "sine waveform did not reach expected center/peak/trough values");

    parameters.depth = 0.0F;
    tabletfx::Tremolo dry(8000.0F, parameters);
    std::array<float, 6> unchanged{0.25F, -0.5F, 0.75F, -1.0F, 0.125F, -0.25F};
    const auto original = unchanged;
    dry.process(unchanged, 2);
    require(unchanged == original, "zero tremolo depth changed the signal");
}

void test_square_duty_cycle_and_other_waveforms_unaffected() {
    tabletfx::TremoloParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.waveform = tabletfx::TremoloWaveform::Square;
    parameters.depth_smoothing_ms = 0.0F;

    std::vector<float> neutral(8000, 1.0F);
    tabletfx::Tremolo default_square(8000.0F, parameters);
    default_square.process(neutral, 1);
    std::vector<float> old_fixed_threshold(8000, 1.0F);
    float old_phase = 0.0F;
    const float old_phase_increment = 1.0F / 8000.0F;
    for (float& sample : old_fixed_threshold) {
        sample *= old_phase < 0.5F ? 1.0F : 0.0F;
        old_phase += old_phase_increment;
        if (old_phase >= 1.0F) old_phase -= 1.0F;
    }
    require(neutral == old_fixed_threshold,
            "50% Square duty changed the exact pre-change fixed-threshold output");
    require(neutral[100] == 1.0F && neutral[3900] == 1.0F &&
                neutral[4100] == 0.0F && neutral[7900] == 0.0F,
            "50% duty did not preserve the existing symmetric square waveform");

    parameters.duty_cycle_percent = 20.0F;
    std::vector<float> skewed(8000, 1.0F);
    tabletfx::Tremolo skewed_square(8000.0F, parameters);
    skewed_square.process(skewed, 1);
    const auto on_samples = static_cast<std::size_t>(
        std::count(skewed.begin(), skewed.end(), 1.0F));
    require(on_samples >= 1598U && on_samples <= 1602U &&
                skewed[1000] == 1.0F && skewed[3000] == 0.0F &&
                skewed_square.parameters().duty_cycle_percent == 20.0F,
            "20% square duty did not set the expected on-time proportion");

    for (const auto waveform : {tabletfx::TremoloWaveform::Sine,
                                tabletfx::TremoloWaveform::Triangle}) {
        parameters.waveform = waveform;
        parameters.duty_cycle_percent = 50.0F;
        tabletfx::Tremolo unchanged(8000.0F, parameters);
        parameters.duty_cycle_percent = 95.0F;
        tabletfx::Tremolo different_duty(8000.0F, parameters);
        std::vector<float> unchanged_audio(8000, 0.8F);
        std::vector<float> different_duty_audio = unchanged_audio;
        unchanged.process(unchanged_audio, 1);
        different_duty.set_duty_cycle_percent(95.0F);
        different_duty.process(different_duty_audio, 1);
        require(unchanged_audio == different_duty_audio,
                "square duty setting changed a non-square tremolo waveform");
    }
}

void test_tempo_sync_conversion_and_bounds() {
    tabletfx::Tremolo tremolo;
    tremolo.set_tempo_sync(true, 120.0F, 1.0F);
    require(std::abs(tremolo.effective_rate_hz() - 2.0F) < 1.0e-6F,
            "one quarter-note beat per cycle at 120 BPM should be 2 Hz");
    tremolo.set_tempo_sync(true, 120.0F, 0.5F);
    require(std::abs(tremolo.effective_rate_hz() - 4.0F) < 1.0e-6F,
            "one eighth-note cycle at 120 BPM should be 4 Hz");
    tremolo.set_tempo_sync(true, 60.0F, 2.0F);
    require(std::abs(tremolo.effective_rate_hz() - 0.5F) < 1.0e-6F,
            "two quarter-note beats per cycle at 60 BPM should be 0.5 Hz");

    tremolo.set_tempo_sync(true, 300.0F, 0.0625F);
    require(tremolo.effective_rate_hz() == 20.0F,
            "tempo-synced rate should respect the maximum-rate safety bound");
    tremolo.set_tempo_sync(true, std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity());
    require(tremolo.parameters().tempo_bpm == 300.0F &&
                tremolo.parameters().beats_per_cycle == 0.0625F &&
                tremolo.effective_rate_hz() == 20.0F,
            "non-finite sync controls did not preserve the prior valid values");
    tremolo.set_tempo_sync(false, 120.0F, 1.0F);
    require(tremolo.effective_rate_hz() == tremolo.parameters().rate_hz,
            "disabling tempo sync did not restore the saved free-running rate");
}

void test_variable_blocks_phase_continuity_and_stereo_link() {
    tabletfx::TremoloParameters parameters;
    parameters.rate_hz = 7.3F;
    parameters.depth = 0.73F;
    parameters.waveform = tabletfx::TremoloWaveform::Sine;
    tabletfx::Tremolo whole(44100.0F, parameters);
    tabletfx::Tremolo chunked(44100.0F, parameters);
    std::vector<float> expected(8194);
    for (std::size_t i = 0; i < expected.size(); i += 2) {
        const float sample = 0.6F * std::sin(static_cast<float>(i) * 0.013F);
        expected[i] = sample;
        expected[i + 1] = sample * 0.5F;
    }
    auto actual = expected;
    whole.process(expected, 2);

    const std::array<std::size_t, 5> block_sizes{1, 127, 53, 300, 17};
    std::size_t frame = 0;
    std::size_t block = 0;
    while (frame < actual.size() / 2) {
        const std::size_t frames = std::min(block_sizes[block % block_sizes.size()],
                                            actual.size() / 2 - frame);
        chunked.process(std::span<float>(actual).subspan(frame * 2, frames * 2), 2);
        frame += frames;
        ++block;
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        require(std::abs(expected[i] - actual[i]) < 1.0e-7F,
                "variable block sizes changed tremolo phase or output");
    }

    tabletfx::TremoloParameters linked_parameters;
    linked_parameters.rate_hz = 1.0F;
    linked_parameters.depth = 1.0F;
    linked_parameters.waveform = tabletfx::TremoloWaveform::Square;
    tabletfx::Tremolo linked(8000.0F, linked_parameters);
    std::vector<float> stereo(10002);
    for (std::size_t i = 0; i < stereo.size(); i += 2) {
        stereo[i] = 1.0F;
        stereo[i + 1] = 0.5F;
    }
    linked.process(stereo, 2);
    require(stereo[3000] == 1.0F && stereo[3001] == 0.5F &&
                stereo[9000] == 0.0F && stereo[9001] == 0.0F,
            "stereo channels did not share the same square-wave LFO phase");

    const auto phase_before_reset = linked.effective_rate_hz();
    linked.reset();
    std::array<float, 2> first_frame{1.0F, 1.0F};
    linked.process(first_frame, 2);
    require(first_frame[0] == 1.0F && first_frame[1] == 1.0F &&
                phase_before_reset == 1.0F,
            "reset did not restore the deterministic initial phase");
}

void test_modulation_phase_reset_preserves_smoothing() {
    for (const auto waveform : {tabletfx::TremoloWaveform::Sine,
                                tabletfx::TremoloWaveform::Triangle,
                                tabletfx::TremoloWaveform::Square}) {
        tabletfx::TremoloParameters parameters;
        parameters.rate_hz = 3.7F;
        parameters.depth = 0.82F;
        parameters.depth_smoothing_ms = 0.0F;
        parameters.waveform = waveform;
        tabletfx::Tremolo restarted(8000.0F, parameters);
        std::vector<float> prefix(347, 1.0F);
        restarted.process(prefix, 1);
        restarted.reset_modulation_phase();

        std::vector<float> actual(513, 1.0F);
        restarted.process(actual, 1);
        tabletfx::Tremolo initial_phase(8000.0F, parameters);
        std::vector<float> expected(actual.size(), 1.0F);
        initial_phase.process(expected, 1);
        require(actual == expected,
                "phase reset did not restart the selected tremolo waveform");
    }

    tabletfx::TremoloParameters ramp_parameters;
    ramp_parameters.depth = 0.1F;
    ramp_parameters.depth_smoothing_ms = 100.0F;
    ramp_parameters.waveform = tabletfx::TremoloWaveform::Square;
    tabletfx::Tremolo ramp(8000.0F, ramp_parameters);
    ramp.set_depth(0.9F);
    ramp.set_duty_cycle_percent(80.0F);
    std::array<float, 160> prefix{};
    prefix.fill(1.0F);
    ramp.process(prefix, 1);
    const float depth_before_reset = ramp.current_depth();
    const float duty_before_reset = ramp.current_duty_cycle_percent();
    require(depth_before_reset > 0.1F && depth_before_reset < 0.9F &&
                duty_before_reset > 50.0F && duty_before_reset < 80.0F,
            "test setup did not leave parameter ramps in progress");

    ramp.reset_modulation_phase();
    require(ramp.current_depth() == depth_before_reset &&
                ramp.current_duty_cycle_percent() == duty_before_reset,
            "phase reset changed the current smoothed parameter values");
    std::array<float, 80> ramp_continuation{};
    ramp_continuation.fill(1.0F);
    ramp.process(ramp_continuation, 1);
    require(ramp.current_depth() > depth_before_reset &&
                ramp.current_duty_cycle_percent() > duty_before_reset,
            "phase reset canceled in-progress parameter smoothing");
}

void test_depth_ramp_is_monotonic_retargetable_and_block_invariant() {
    tabletfx::TremoloParameters parameters;
    parameters.rate_hz = 0.05F;
    parameters.depth = 0.0F;
    parameters.depth_smoothing_ms = 10.0F;
    tabletfx::Tremolo whole(48000.0F, parameters);
    tabletfx::Tremolo chunked(48000.0F, parameters);
    whole.set_depth(1.0F);
    chunked.set_depth(1.0F);

    std::vector<float> whole_audio(480, 1.0F);
    std::vector<float> chunked_audio = whole_audio;
    whole.process(whole_audio, 1);
    const std::array<std::size_t, 5> block_sizes{1, 127, 53, 211, 88};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < chunked_audio.size()) {
        const std::size_t frames = std::min(block_sizes[block % block_sizes.size()],
                                            chunked_audio.size() - offset);
        chunked.process(std::span<float>(chunked_audio).subspan(offset, frames), 1);
        offset += frames;
        ++block;
    }
    for (std::size_t i = 0; i < whole_audio.size(); ++i) {
        require(std::abs(whole_audio[i] - chunked_audio[i]) < 1.0e-7F,
                "depth-ramp output changed with variable block sizes");
        if (i > 0) {
            require(whole_audio[i] <= whole_audio[i - 1] + 1.0e-7F,
                    "increasing depth did not produce a monotonic amplitude ramp");
        }
    }
    require(std::abs(whole.current_depth() - 1.0F) < 1.0e-7F &&
                std::abs(whole_audio.back() - 0.5F) < 0.002F,
            "10 ms depth ramp did not reach its target after 480 frames");

    whole.set_depth(0.0F);
    std::array<float, 240> first_half{};
    first_half.fill(1.0F);
    whole.process(first_half, 1);
    require(std::abs(whole.current_depth() - 0.5F) < 1.0e-4F,
            "depth did not reach the expected midpoint after 5 ms");
    whole.set_depth(0.5F);
    std::array<float, 480> retargeted{};
    retargeted.fill(1.0F);
    whole.process(retargeted, 1);
    require(std::abs(whole.current_depth() - 0.5F) < 1.0e-7F,
            "retargeted depth ramp did not reach the new target");

    whole.set_depth_smoothing_ms(0.0F);
    whole.set_depth(0.25F);
    require(std::abs(whole.current_depth() - 0.25F) < 1.0e-7F,
            "zero-duration depth smoothing should apply immediately");
}

void test_duty_cycle_ramp_is_smoothed_and_block_invariant() {
    tabletfx::TremoloParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.depth = 1.0F;
    parameters.waveform = tabletfx::TremoloWaveform::Square;
    parameters.depth_smoothing_ms = 10.0F;
    tabletfx::Tremolo whole(48000.0F, parameters);
    tabletfx::Tremolo chunked(48000.0F, parameters);
    std::array<float, 12000> pre_roll{};
    whole.process(pre_roll, 1);
    chunked.process(pre_roll, 1);
    whole.set_duty_cycle_percent(5.0F);
    chunked.set_duty_cycle_percent(5.0F);
    require(whole.current_duty_cycle_percent() == 50.0F,
            "duty-cycle control jumped instead of starting its smoothing ramp");

    std::vector<float> whole_audio(480, 1.0F);
    std::vector<float> chunked_audio = whole_audio;
    whole.process(whole_audio, 1);
    const std::array<std::size_t, 5> block_sizes{1, 127, 53, 211, 88};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < chunked_audio.size()) {
        const std::size_t frames = std::min(block_sizes[block % block_sizes.size()],
                                            chunked_audio.size() - offset);
        chunked.process(std::span<float>(chunked_audio).subspan(offset, frames), 1);
        offset += frames;
        ++block;
    }
    require(whole_audio == chunked_audio &&
                whole.current_duty_cycle_percent() == 5.0F &&
                whole_audio.front() == 1.0F && whole_audio.back() == 0.0F,
            "duty-cycle ramp changed with block partitioning or missed its target");
}

void test_invalid_samples_and_zero_process_allocations() {
    tabletfx::Tremolo tremolo;
    std::array<float, 5> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.25F,
        -0.5F,
    };
    tremolo.process(invalid, 1);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F && invalid[2] == 0.0F,
            "non-finite audio input did not produce finite silence");

    std::array<float, 4> untouched{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before_zero_channels = untouched;
    tremolo.process(untouched, 0);
    require(untouched == before_zero_channels,
            "zero-channel processing should leave the buffer untouched");

    std::array<float, 512> block{};
    block.fill(0.5F);
    tremolo.set_waveform(tabletfx::TremoloWaveform::Square);
    tremolo.set_depth_smoothing_ms(0.0F);
    tremolo.set_duty_cycle_percent(37.0F);
    tremolo.reset();
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        tremolo.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "tremolo allocated memory during process()");
    for (float sample : block) {
        require(std::isfinite(sample), "tremolo emitted non-finite audio");
    }
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) {
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
        test_defaults_bounds_and_memory();
        test_waveforms_and_depth_endpoints();
        test_square_duty_cycle_and_other_waveforms_unaffected();
        test_lfo_clip_shape_and_default_identity();
        test_tempo_sync_conversion_and_bounds();
        test_variable_blocks_phase_continuity_and_stereo_link();
        test_modulation_phase_reset_preserves_smoothing();
        test_depth_ramp_is_monotonic_retargetable_and_block_invariant();
        test_duty_cycle_ramp_is_smoothed_and_block_invariant();
        test_invalid_samples_and_zero_process_allocations();
        std::cout << "All TabletFX tremolo tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
