#include "tabletfx/effects/convolution_reverb.hpp"

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
#include <vector>

namespace {

int failures = 0;
std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};
constexpr std::size_t kLatencyFrames =
    tabletfx::ConvolutionReverb::kPartitionFrames;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(float actual, float expected, float tolerance = 2.0e-4F) {
    return std::abs(actual - expected) <= tolerance;
}

tabletfx::ConvolutionReverb make_wet_mono() {
    tabletfx::ConvolutionReverbParameters parameters;
    parameters.mix = 1.0F;
    return tabletfx::ConvolutionReverb(48000.0F, 1, parameters);
}

void impulse_response_has_fixed_partition_latency() {
    auto effect = make_wet_mono();
    const std::array<float, 4> ir{1.0F, 0.5F, -0.25F, 0.125F};
    check(effect.load_impulse_response(ir, 1, 48000),
          "mono impulse response should load");
    check(effect.impulse_frames() == ir.size(),
          "loaded IR frame count should be retained");
    check(effect.partition_count() == 1,
          "short impulse should use one partition");

    std::array<float, kLatencyFrames + 4> audio{};
    audio[0] = 1.0F;
    effect.process(audio, 1);
    for (std::size_t frame = 0; frame < kLatencyFrames; ++frame) {
        check(std::abs(audio[frame]) < 2.0e-5F,
              "wet-only impulse should be silent before the declared latency");
    }
    for (std::size_t frame = 0; frame < ir.size(); ++frame) {
        check(near(audio[kLatencyFrames + frame], ir[frame]),
              "loaded IR samples should appear after one partition");
    }
}

void impulse_response_crosses_partition_boundary() {
    auto effect = make_wet_mono();
    std::array<float, kLatencyFrames + 3> ir{};
    ir[0] = 1.0F;
    ir[kLatencyFrames - 1] = 0.25F;
    ir[kLatencyFrames] = -0.5F;
    ir[kLatencyFrames + 2] = 0.125F;
    check(effect.load_impulse_response(ir, 1, 48000),
          "two-partition impulse response should load");

    std::array<float, 2 * kLatencyFrames + 3> audio{};
    audio[0] = 1.0F;
    effect.process(audio, 1);
    for (std::size_t frame = 0; frame < ir.size(); ++frame) {
        check(near(audio[kLatencyFrames + frame], ir[frame]),
              "overlap-add should preserve the IR across partition boundaries");
    }
}

void stereo_ir_uses_independent_channels_and_mono_ir_duplicates() {
    tabletfx::ConvolutionReverbParameters parameters;
    parameters.mix = 1.0F;
    tabletfx::ConvolutionReverb effect(48000.0F, 2, parameters);
    const std::array<float, 4> stereo_ir{1.0F, 0.5F, 0.0F, 0.0F};
    check(effect.load_impulse_response(stereo_ir, 2, 48000),
          "stereo IR should load");
    const std::size_t stereo_ir_memory = effect.memory_bytes();
    std::array<float, 2 * (kLatencyFrames + 1)> stereo_input{};
    stereo_input[0] = 0.4F;
    stereo_input[1] = 0.8F;
    effect.process(stereo_input, 2);
    check(near(stereo_input[2 * kLatencyFrames], 0.4F),
          "left IR should process only the left channel");
    check(near(stereo_input[2 * kLatencyFrames + 1], 0.4F),
          "right IR should process only the right channel");

    const std::array<float, 2> mono_ir{0.25F, 0.5F};
    check(effect.load_impulse_response(mono_ir, 1, 48000),
          "mono IR should load into stereo processing");
    check(effect.memory_bytes() < stereo_ir_memory,
          "a mono IR should share one spectrum across stereo output channels");
    effect.reset();
    std::array<float, 2 * (kLatencyFrames + 1)> mono_ir_input{};
    mono_ir_input[0] = 0.4F;
    mono_ir_input[1] = -0.8F;
    effect.process(mono_ir_input, 2);
    const std::size_t delayed = 2 * kLatencyFrames;
    check(near(mono_ir_input[delayed], 0.1F),
          "mono IR should be duplicated to the left output");
    check(near(mono_ir_input[delayed + 1], -0.2F),
          "mono IR should be duplicated to the right output");
}

void dry_mix_is_aligned_with_convolved_wet_path() {
    tabletfx::ConvolutionReverbParameters parameters;
    parameters.mix = 0.5F;
    tabletfx::ConvolutionReverb effect(48000.0F, 1, parameters);
    const std::array<float, 1> ir{1.0F};
    check(effect.load_impulse_response(ir, 1, 48000),
          "single-sample IR should load");
    std::array<float, kLatencyFrames + 1> audio{};
    audio[0] = 1.0F;
    effect.process(audio, 1);
    check(near(audio[kLatencyFrames], 1.0F),
          "dry and wet impulse should sum without a relative timing offset");
}

void wet_filter_controls_shape_the_convolved_signal() {
    tabletfx::ConvolutionReverbParameters high_pass_parameters;
    high_pass_parameters.mix = 1.0F;
    high_pass_parameters.high_pass_hz = 1000.0F;
    high_pass_parameters.low_pass_hz = 0.0F;
    tabletfx::ConvolutionReverb high_pass(48000.0F, 1, high_pass_parameters);
    const std::array<float, 1> impulse{1.0F};
    check(high_pass.load_impulse_response(impulse, 1, 48000),
          "IR should load for high-pass test");
    std::array<float, 4096> low_tone{};
    for (std::size_t frame = 0; frame < low_tone.size(); ++frame) {
        low_tone[frame] = std::sin(2.0F * 3.14159265358979323846F *
                                  100.0F * static_cast<float>(frame) / 48000.0F);
    }
    high_pass.process(low_tone, 1);
    float high_pass_rms = 0.0F;
    constexpr std::size_t settle_frames = kLatencyFrames + 512;
    for (std::size_t frame = settle_frames; frame < low_tone.size(); ++frame) {
        high_pass_rms += low_tone[frame] * low_tone[frame];
    }
    high_pass_rms = std::sqrt(high_pass_rms /
                              static_cast<float>(low_tone.size() - settle_frames));
    check(high_pass_rms < 0.15F,
          "wet high-pass should attenuate frequencies below its cutoff");

    tabletfx::ConvolutionReverbParameters low_pass_parameters;
    low_pass_parameters.mix = 1.0F;
    low_pass_parameters.high_pass_hz = 0.0F;
    low_pass_parameters.low_pass_hz = 1000.0F;
    tabletfx::ConvolutionReverb low_pass(48000.0F, 1, low_pass_parameters);
    check(low_pass.load_impulse_response(impulse, 1, 48000),
          "IR should load for low-pass test");
    std::array<float, 4096> high_tone{};
    for (std::size_t frame = 0; frame < high_tone.size(); ++frame) {
        high_tone[frame] = std::sin(2.0F * 3.14159265358979323846F *
                                   5000.0F * static_cast<float>(frame) / 48000.0F);
    }
    low_pass.process(high_tone, 1);
    float low_pass_rms = 0.0F;
    for (std::size_t frame = settle_frames; frame < high_tone.size(); ++frame) {
        low_pass_rms += high_tone[frame] * high_tone[frame];
    }
    low_pass_rms = std::sqrt(low_pass_rms /
                              static_cast<float>(high_tone.size() - settle_frames));
    check(low_pass_rms < 0.2F,
          "wet low-pass should attenuate frequencies above its cutoff");
}

void bypass_can_preserve_or_clear_the_convolution_tail() {
    const std::array<float, 4> ir{1.0F, 0.5F, 0.25F, 0.125F};

    tabletfx::ConvolutionReverbParameters tails_parameters;
    tails_parameters.mix = 1.0F;
    tails_parameters.tails = true;
    tabletfx::ConvolutionReverb with_tails(48000.0F, 1, tails_parameters);
    check(with_tails.load_impulse_response(ir, 1, 48000),
          "IR should load for tails-on test");
    std::array<float, kLatencyFrames> onset{};
    onset[0] = 1.0F;
    with_tails.process(onset, 1);
    std::array<float, kLatencyFrames> bypassed{};
    with_tails.process_bypassed(bypassed, 1);
    check(near(bypassed[0], ir[0]) && near(bypassed[1], ir[1]),
          "tails-on bypass should pass dry input and continue the convolved tail");

    tails_parameters.tails = false;
    tabletfx::ConvolutionReverb without_tails(48000.0F, 1, tails_parameters);
    check(without_tails.load_impulse_response(ir, 1, 48000),
          "IR should load for tails-off test");
    std::array<float, kLatencyFrames> second_onset{};
    second_onset[0] = 1.0F;
    without_tails.process(second_onset, 1);
    std::array<float, kLatencyFrames> cleared{};
    without_tails.process_bypassed(cleared, 1);
    check(std::all_of(cleared.begin(), cleared.end(), [](float value) {
              return std::abs(value) < 2.0e-5F;
          }),
          "tails-off bypass should clear the pending convolution tail");
}

void reset_clears_pending_tail_and_keeps_loaded_ir() {
    auto effect = make_wet_mono();
    const std::array<float, 4> ir{1.0F, 0.5F, -0.25F, 0.125F};
    check(effect.load_impulse_response(ir, 1, 48000),
          "IR should load for tail-reset test");
    std::array<float, kLatencyFrames> onset{};
    onset[0] = 1.0F;
    effect.process(onset, 1);
    effect.reset();
    std::array<float, kLatencyFrames> silence{};
    effect.process(silence, 1);
    check(std::all_of(silence.begin(), silence.end(), [](float value) {
              return std::abs(value) < 2.0e-5F;
          }),
          "reset should discard queued convolution tail and delay state");
    check(effect.impulse_frames() == ir.size(),
          "reset should retain the selected user impulse response");
}

void resampling_bounds_and_failed_load_preserve_loaded_ir() {
    auto effect = make_wet_mono();
    const std::array<float, 2> half_rate_ir{1.0F, 0.5F};
    check(effect.load_impulse_response(half_rate_ir, 1, 24000),
          "supported IR sample rate should be resampled");
    check(effect.impulse_frames() == 4,
          "resampled IR duration should follow the target sample rate");
    std::vector<float> too_long(
        tabletfx::ConvolutionReverb::kMaximumImpulseFrames + 1, 0.25F);
    check(!effect.load_impulse_response(too_long, 1, 48000),
          "IR beyond the fixed frame limit should be rejected");
    check(effect.impulse_frames() == 4,
          "failed IR load should leave the previous IR installed");
    const std::array<float, 2> invalid_ir{
        1.0F, std::numeric_limits<float>::quiet_NaN()};
    check(!effect.load_impulse_response(invalid_ir, 1, 48000),
          "non-finite IR samples should be rejected");
    check(!effect.load_impulse_response(half_rate_ir, 3, 48000),
          "unsupported IR channel count should be rejected");
}

void cverb_duration_limit_accepts_ten_seconds_at_48khz_and_keeps_fixed_latency() {
    constexpr std::size_t ten_seconds_frames = 10U * 48000U;
    constexpr std::size_t project_latency_frames =
        tabletfx::ConvolutionReverb::kPartitionFrames;
    check(tabletfx::ConvolutionReverb::maximum_impulse_frames() >=
              ten_seconds_frames,
          "C-Verb-compatible IR limit should permit 10 seconds at 48 kHz");

    // A mono IR keeps this native regression bounded while exercising the
    // documented duration at the target sample rate. The nonzero last sample
    // ensures the accepted input really spans the full ten seconds.
    std::vector<float> ir(ten_seconds_frames, 0.0F);
    ir.front() = 1.0F;
    ir.back() = 0.25F;

    tabletfx::ConvolutionReverbParameters parameters;
    parameters.mix = 1.0F;
    auto effect = tabletfx::ConvolutionReverb(48000.0F, 2, parameters);
    check(effect.load_impulse_response(ir, 1, 48000),
          "10-second mono IR should load for stereo output at 48 kHz");
    check(effect.impulse_frames() == ten_seconds_frames,
          "the complete 10-second IR length should be retained");

    // The project partition latency remains independent of the selected IR's
    // duration. Prime's C-Verb guide does not specify processing latency.
    std::vector<float> input(4 * project_latency_frames, 0.0F);
    input[0] = 1.0F;
    input[1] = 0.5F;
    effect.process(input, 2);
    for (std::size_t frame = 0; frame < project_latency_frames; ++frame) {
        check(std::abs(input[frame * 2]) < 2.0e-5F &&
                  std::abs(input[frame * 2 + 1]) < 2.0e-5F,
              "10-second IR should remain silent before the fixed partition latency");
    }
    check(near(input[project_latency_frames * 2], 1.0F) &&
              near(input[project_latency_frames * 2 + 1], 0.5F),
          "10-second IR should preserve the project fixed-partition wet latency");
}

void irregular_process_chunks_match_single_call() {
    auto single = make_wet_mono();
    auto chunked = make_wet_mono();
    std::array<float, 257> ir{};
    for (std::size_t frame = 0; frame < ir.size(); ++frame) {
        ir[frame] = std::exp(-static_cast<float>(frame) / 45.0F) *
                    std::sin(static_cast<float>(frame) * 0.17F);
    }
    check(single.load_impulse_response(ir, 1, 48000) &&
              chunked.load_impulse_response(ir, 1, 48000),
          "IR should load for chunk-equivalence test");
    std::array<float, 4096> source{};
    for (std::size_t frame = 0; frame < source.size(); ++frame) {
        source[frame] = 0.3F * std::sin(static_cast<float>(frame) * 0.031F);
    }
    auto expected = source;
    auto actual = source;
    single.process(expected, 1);
    std::size_t cursor = 0;
    constexpr std::array<std::size_t, 7> chunks{17, 128, 3, 211, 64, 509, 31};
    std::size_t chunk_index = 0;
    while (cursor < actual.size()) {
        const std::size_t count = std::min(chunks[chunk_index++ % chunks.size()],
                                           actual.size() - cursor);
        chunked.process(std::span<float>(actual.data() + cursor, count), 1);
        cursor += count;
    }
    for (std::size_t frame = 0; frame < actual.size(); ++frame) {
        check(near(actual[frame], expected[frame], 3.0e-5F),
              "processing must be invariant to caller block sizes");
    }
}

void process_does_not_allocate_after_construction_and_ir_load() {
    auto effect = make_wet_mono();
    std::array<float, 513> ir{};
    for (std::size_t frame = 0; frame < ir.size(); ++frame) {
        ir[frame] = std::exp(-static_cast<float>(frame) / 90.0F) *
                    std::sin(static_cast<float>(frame) * 0.11F);
    }
    check(effect.load_impulse_response(ir, 1, 48000),
          "IR should load before allocation measurement");
    std::array<float, 512> block{};
    for (std::size_t frame = 0; frame < block.size(); ++frame) {
        block[frame] = 0.2F * std::sin(static_cast<float>(frame) * 0.03F);
    }
    for (int warmup = 0; warmup < 4; ++warmup) effect.process(block, 1);

    allocation_count.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    for (int iteration = 0; iteration < 64; ++iteration) {
        effect.process(block, 1);
    }
    count_allocations.store(false, std::memory_order_release);
    check(allocation_count.load(std::memory_order_relaxed) == 0,
          "process() must not allocate after construction and IR loading");
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
    impulse_response_has_fixed_partition_latency();
    impulse_response_crosses_partition_boundary();
    stereo_ir_uses_independent_channels_and_mono_ir_duplicates();
    dry_mix_is_aligned_with_convolved_wet_path();
    wet_filter_controls_shape_the_convolved_signal();
    bypass_can_preserve_or_clear_the_convolution_tail();
    reset_clears_pending_tail_and_keeps_loaded_ir();
    resampling_bounds_and_failed_load_preserve_loaded_ir();
    cverb_duration_limit_accepts_ten_seconds_at_48khz_and_keeps_fixed_latency();
    irregular_process_chunks_match_single_call();
    process_does_not_allocate_after_construction_and_ir_load();
    if (failures != 0) {
        std::cerr << failures << " convolution reverb test(s) failed\n";
        return 1;
    }
    std::cout << "All convolution reverb tests passed\n";
    return 0;
}
