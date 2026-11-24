#include "tabletfx/effects/cabinet_ir.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <complex>
#include <iostream>
#include <limits>
#include <new>
#include <span>
#include <vector>

namespace {

int failures = 0;
std::atomic<std::size_t> allocation_count{0};
std::atomic<bool> count_allocations{false};

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(float actual, float expected, float tolerance = 3.0e-5F) {
    return std::abs(actual - expected) <= tolerance;
}

void exact_xr1_format_is_required_and_valid_ir_is_immediate() {
    tabletfx::CabinetIr effect(48000.0F, 1);
    check(effect.id() == "cabinet-ir",
          "cabinet IR must be a distinct cabinet processor");

    std::array<float, tabletfx::CabinetIr::kRequiredIrFrames> ir{};
    ir[0] = 1.0F;
    ir[1] = 0.25F;
    check(effect.load_impulse_response(ir, 1, 48000, 24),
          "24-bit 48 kHz 2,048-frame mono IR should load");
    check(effect.has_impulse_response() &&
              effect.impulse_frames() == tabletfx::CabinetIr::kRequiredIrFrames,
          "successful load should report the source IR duration at 48 kHz");
    check(effect.memory_bytes() >= sizeof(effect) &&
              effect.memory_bytes() <= 48 * 1024,
          "48 kHz mono IR should stay below 48 KiB including the FFT state");

    std::array<float, 3 * tabletfx::CabinetIr::kPartitionFrames> audio{};
    audio[0] = 0.5F;
    effect.process(audio, 1);
    check(std::all_of(audio.begin(),
                      audio.begin() + tabletfx::CabinetIr::kLatencyFrames,
                      [](float sample) { return sample == 0.0F; }),
          "partitioned cabinet processing should delay the signal by one partition");
    check(near(audio[tabletfx::CabinetIr::kLatencyFrames], 0.5F) &&
              near(audio[tabletfx::CabinetIr::kLatencyFrames + 1], 0.125F),
          "partitioned convolution should preserve the first two IR taps");
}

void invalid_format_is_rejected_without_replacing_loaded_ir() {
    tabletfx::CabinetIr effect(48000.0F, 2);
    std::array<float, tabletfx::CabinetIr::kRequiredIrFrames> mono_ir{};
    mono_ir[0] = 1.0F;
    check(effect.load_impulse_response(mono_ir, 1, 48000, 24),
          "valid mono IR should load before rejection checks");

    std::array<float, tabletfx::CabinetIr::kRequiredIrFrames - 1> short_ir{};
    std::array<float, tabletfx::CabinetIr::kRequiredIrFrames> non_finite_ir{};
    non_finite_ir[12] = std::numeric_limits<float>::quiet_NaN();
    check(!effect.load_impulse_response(short_ir, 1, 48000, 24),
          "non-2,048-frame IR should be rejected");
    check(!effect.load_impulse_response(mono_ir, 1, 44100, 24),
          "non-48 kHz IR should be rejected");
    check(!effect.load_impulse_response(mono_ir, 1, 48000, 16),
          "non-24-bit WAV should be rejected");
    check(!effect.load_impulse_response(mono_ir, 2, 48000, 24),
          "stereo IR should not be accepted by the documented single-mono path");
    check(!effect.load_impulse_response(mono_ir, 3, 48000, 24),
          "unsupported IR channel count should be rejected");
    check(!effect.load_impulse_response(non_finite_ir, 1, 48000, 24),
          "non-finite decoded samples should be rejected");
    check(effect.has_impulse_response() &&
              effect.impulse_frames() == tabletfx::CabinetIr::kRequiredIrFrames,
          "failed loads should preserve the previous IR");
}

void mono_ir_processes_stereo_channels_independently() {
    tabletfx::CabinetIr effect(48000.0F, 2);
    std::array<float, tabletfx::CabinetIr::kRequiredIrFrames> ir{};
    ir[0] = 0.25F;
    ir[2] = 0.5F;
    check(effect.load_impulse_response(ir, 1, 48000, 24),
          "mono IR should load into a stereo processor");

    std::array<float, 2 * tabletfx::CabinetIr::kPartitionFrames> audio{};
    audio[0] = 0.4F;
    audio[1] = -0.8F;
    effect.process(audio, 2);
    std::array<float, 2 * tabletfx::CabinetIr::kPartitionFrames> output{};
    effect.process(output, 2);
    check(near(output[0], 0.1F) && near(output[1], -0.2F),
          "mono IR should preserve independent left/right input after one partition");
    check(near(output[4], 0.2F) && near(output[5], -0.4F),
          "mono IR taps should be applied independently to both channels");
}

void processor_resamples_only_after_valid_source_format() {
    tabletfx::CabinetIr effect(96000.0F, 1);
    std::array<float, tabletfx::CabinetIr::kRequiredIrFrames> ir{};
    ir[0] = 1.0F;
    check(effect.load_impulse_response(ir, 1, 48000, 24),
          "valid source-format IR should load at a different host rate");
    check(effect.impulse_frames() == 4096,
          "48 kHz 2,048-frame IR should double in frames at a 96 kHz host rate");
    std::array<float, tabletfx::CabinetIr::kPartitionFrames> input{};
    input[0] = 0.75F;
    effect.process(input, 1);
    std::array<float, tabletfx::CabinetIr::kPartitionFrames> output{};
    effect.process(output, 1);
    check(near(output[0], 0.75F),
          "resampling must preserve the first tap after the documented partition latency");
}

void partition_boundaries_match_the_loaded_impulse_response() {
    tabletfx::CabinetIr effect(48000.0F, 1);
    std::array<float, tabletfx::CabinetIr::kRequiredIrFrames> ir{};
    ir[0] = 0.75F;
    ir[1] = -0.25F;
    ir[63] = 0.125F;
    ir[64] = -0.5F;
    ir[65] = 0.375F;
    ir[127] = -0.0625F;
    ir[128] = 0.25F;
    ir[tabletfx::CabinetIr::kRequiredIrFrames - 1] = 0.03125F;
    check(effect.load_impulse_response(ir, 1, 48000, 24),
          "sparse boundary IR should load before convolution check");

    std::vector<float> audio(tabletfx::CabinetIr::kLatencyFrames + ir.size() +
                                 tabletfx::CabinetIr::kPartitionFrames,
                             0.0F);
    audio[0] = 1.0F;
    effect.process(audio, 1);
    const std::array<std::size_t, 8> taps{
        0, 1, 63, 64, 65, 127, 128,
        tabletfx::CabinetIr::kRequiredIrFrames - 1};
    for (const std::size_t tap : taps) {
        check(near(audio[tabletfx::CabinetIr::kLatencyFrames + tap], ir[tap],
                   1.0e-4F),
              "partitioned IR loader should preserve taps at and across partition edges");
    }
}

void reset_clear_and_channel_mismatch_are_safe() {
    tabletfx::CabinetIr effect(48000.0F, 1);
    std::array<float, tabletfx::CabinetIr::kRequiredIrFrames> ir{};
    ir[0] = 1.0F;
    check(effect.load_impulse_response(ir, 1, 48000, 24),
          "IR should load before reset checks");

    std::array<float, tabletfx::CabinetIr::kPartitionFrames> first{};
    first[0] = 0.5F;
    effect.process(first, 1);
    effect.reset();
    std::array<float, tabletfx::CabinetIr::kPartitionFrames> after_reset{};
    effect.process(after_reset, 1);
    std::array<float, tabletfx::CabinetIr::kPartitionFrames> after_reset_output{};
    effect.process(after_reset_output, 1);
    check(std::all_of(after_reset_output.begin(), after_reset_output.end(),
                      [](float sample) { return sample == 0.0F; }),
          "reset should clear convolution history while retaining the IR");

    std::array<float, 2> wrong_channels{0.25F, 0.5F};
    effect.process(wrong_channels, 2);
    check(wrong_channels == std::array<float, 2>{0.25F, 0.5F},
          "channel mismatch should leave the buffer untouched");

    effect.clear_impulse_response();
    check(!effect.has_impulse_response() && effect.impulse_frames() == 0,
          "clear should unload coefficient and history storage");
    std::array<float, 1> unloaded{0.25F};
    effect.process(unloaded, 1);
    check(unloaded[0] == 0.25F,
          "an unloaded cabinet processor should remain transparent");
}

void bypass_is_transparent_and_discards_stale_history() {
    tabletfx::CabinetIr effect(48000.0F, 1);
    std::array<float, tabletfx::CabinetIr::kRequiredIrFrames> ir{};
    ir[1] = 1.0F;
    check(effect.load_impulse_response(ir, 1, 48000, 24),
          "IR should load before bypass checks");

    std::array<float, tabletfx::CabinetIr::kPartitionFrames> input{};
    input[0] = 0.5F;
    effect.process(input, 1);
    check(std::all_of(input.begin(), input.end(),
                      [](float sample) { return sample == 0.0F; }),
          "the first partition is delayed while its convolution is prepared");

    std::array<float, tabletfx::CabinetIr::kPartitionFrames> bypassed{};
    bypassed.fill(0.25F);
    effect.process_bypassed(bypassed, 1);
    check(bypassed.front() == 0.25F && bypassed.back() == 0.25F,
          "bypass should leave its audio buffer unchanged");

    std::array<float, tabletfx::CabinetIr::kPartitionFrames> reenabled{};
    effect.process(reenabled, 1);
    std::array<float, tabletfx::CabinetIr::kPartitionFrames> reenabled_output{};
    effect.process(reenabled_output, 1);
    check(std::all_of(reenabled_output.begin(), reenabled_output.end(),
                      [](float sample) { return sample == 0.0F; }),
          "re-enabling after bypass must not emit pre-bypass convolution history");
}

void process_is_block_invariant_finite_and_allocation_free() {
    tabletfx::CabinetIr single(48000.0F, 2);
    tabletfx::CabinetIr chunked(48000.0F, 2);
    std::array<float, tabletfx::CabinetIr::kRequiredIrFrames> ir{};
    for (std::size_t frame = 0; frame < ir.size(); ++frame) {
        ir[frame] = 0.5F * std::exp(-static_cast<float>(frame) / 280.0F) *
                    std::sin(static_cast<float>(frame) * 0.071F);
    }
    ir[0] = 0.75F;
    check(single.load_impulse_response(ir, 1, 48000, 24) &&
              chunked.load_impulse_response(ir, 1, 48000, 24),
          "test IR should load before block-invariance check");

    std::array<float, 4096> source{};
    for (std::size_t index = 0; index < source.size(); ++index) {
        source[index] = 0.2F * std::sin(static_cast<float>(index) * 0.023F);
    }
    auto expected = source;
    auto actual = source;
    single.process(expected, 2);
    constexpr std::array<std::size_t, 7> chunks{17, 128, 3, 211, 64, 509, 31};
    std::size_t cursor = 0;
    std::size_t chunk_index = 0;
    while (cursor < actual.size()) {
        const std::size_t remaining_frames = (actual.size() - cursor) / 2;
        const std::size_t count =
            std::min(chunks[chunk_index++ % chunks.size()], remaining_frames);
        chunked.process(std::span<float>(actual.data() + cursor, count * 2), 2);
        cursor += count * 2;
    }
    for (std::size_t index = 0; index < actual.size(); ++index) {
        check(near(actual[index], expected[index], 1.0e-4F),
              "convolution output must not depend on caller block boundaries");
        check(std::isfinite(actual[index]),
              "cabinet processing must not emit a non-finite sample");
    }

    std::array<float, 256> block{};
    for (int warmup = 0; warmup < 3; ++warmup) single.process(block, 2);
    allocation_count.store(0, std::memory_order_relaxed);
    count_allocations.store(true, std::memory_order_release);
    for (int iteration = 0; iteration < 16; ++iteration) {
        single.process(block, 2);
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
    exact_xr1_format_is_required_and_valid_ir_is_immediate();
    invalid_format_is_rejected_without_replacing_loaded_ir();
    mono_ir_processes_stereo_channels_independently();
    processor_resamples_only_after_valid_source_format();
    partition_boundaries_match_the_loaded_impulse_response();
    reset_clear_and_channel_mismatch_are_safe();
    bypass_is_transparent_and_discards_stale_history();
    process_is_block_invariant_finite_and_allocation_free();
    if (failures != 0) {
        std::cerr << failures << " cabinet IR test(s) failed\n";
        return 1;
    }
    std::cout << "All cabinet IR tests passed\n";
    return 0;
}
