#include "tabletfx/effects/rotary_speaker.hpp"

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

void test_defaults_ranges_and_bounded_storage() {
    tabletfx::RotarySpeaker rotary;
    const auto defaults = rotary.parameters();
    require(rotary.id() == "rotary-speaker",
            "rotary speaker stable ID changed");
    require(defaults.rate_hz == 6.0F &&
                defaults.speed == tabletfx::RotarySpeed::slow &&
                defaults.low_rate_multi == 0.72F &&
                defaults.mix == 1.0F && defaults.level == 1.0F &&
                rotary.current_hi_rate_hz() == 0.0F,
            "project-authored rotary defaults changed");
    require(rotary.memory_bytes() > sizeof(rotary),
            "rotor delay storage is missing from memory accounting");

    tabletfx::RotarySpeakerParameters parameters;
    parameters.rate_hz = 100.0F;
    parameters.tempo_bpm = 500.0F;
    parameters.beats_per_rotation = 0.0F;
    parameters.slow_speed_multiplier = 0.0F;
    parameters.drive = 2.0F;
    parameters.hi_depth = -1.0F;
    parameters.high_level = 3.0F;
    parameters.low_depth = 2.0F;
    parameters.rotor_length = 5.0F;
    parameters.low_rate_multi = 2.0F;
    parameters.low_time_constant_ms = 1.0F;
    parameters.hi_time_constant_ms = 10000.0F;
    parameters.low_mic_spacing = 2.0F;
    parameters.high_mic_spacing = 3.0F;
    parameters.mic_distance = -1.0F;
    parameters.stereo_spread = 4.0F;
    parameters.mix = -1.0F;
    parameters.level = 3.0F;
    parameters.balance = -3.0F;
    parameters.speed = static_cast<tabletfx::RotarySpeed>(255);
    parameters.input_select = static_cast<tabletfx::RotaryInputSelect>(255);
    rotary.set_parameters(parameters);
    const auto bounded = rotary.parameters();
    require(bounded.rate_hz == 10.0F && bounded.tempo_bpm == 300.0F &&
                bounded.beats_per_rotation == 0.125F &&
                bounded.slow_speed_multiplier == 0.02F &&
                bounded.drive == 1.0F && bounded.hi_depth == 0.0F &&
                bounded.high_level == 2.0F && bounded.low_depth == 1.0F &&
                bounded.rotor_length == 1.0F &&
                bounded.low_rate_multi == 1.0F &&
                bounded.low_time_constant_ms == 10.0F &&
                bounded.hi_time_constant_ms == 5000.0F &&
                bounded.low_mic_spacing == 1.0F &&
                bounded.high_mic_spacing == 1.0F &&
                bounded.mic_distance == 0.0F &&
                bounded.stereo_spread == 2.0F && bounded.mix == 0.0F &&
                bounded.level == 2.0F && bounded.balance == -1.0F &&
                bounded.speed == tabletfx::RotarySpeed::slow &&
                bounded.input_select == tabletfx::RotaryInputSelect::stereo,
            "project-authored rotary parameter bounds or enum fallbacks changed");

    parameters.tempo_sync = true;
    parameters.tempo_bpm = 120.0F;
    parameters.beats_per_rotation = 0.5F;
    parameters.speed = tabletfx::RotarySpeed::fast;
    parameters.low_rate_multi = 0.75F;
    rotary.set_parameters(parameters);
    require(std::abs(rotary.target_hi_rate_hz() - 4.0F) < 1.0e-6F &&
                std::abs(rotary.target_low_rate_hz() - 3.0F) < 1.0e-6F,
            "project-authored tempo rate or low-rate multiplier mapping changed");
}

void test_slow_fast_acceleration_and_deceleration() {
    tabletfx::RotarySpeakerParameters parameters;
    parameters.rate_hz = 8.0F;
    parameters.speed = tabletfx::RotarySpeed::slow;
    parameters.slow_speed_multiplier = 0.1F;
    parameters.low_rate_multi = 0.7F;
    parameters.low_time_constant_ms = 200.0F;
    parameters.hi_time_constant_ms = 100.0F;
    parameters.mix = 0.0F;
    tabletfx::RotarySpeaker rotary(48000.0F, 1, parameters);

    std::vector<float> audio(4800, 0.0F);
    rotary.process(audio, 1);
    const float slow_rate = rotary.current_hi_rate_hz();
    require(slow_rate > 0.3F && slow_rate < rotary.target_hi_rate_hz(),
            "slow rotor did not accelerate gradually from rest");
    require(rotary.current_low_rate_hz() > 0.0F &&
                rotary.current_low_rate_hz() < rotary.target_low_rate_hz(),
            "drum did not use its separate acceleration time constant");

    parameters.speed = tabletfx::RotarySpeed::fast;
    rotary.set_parameters(parameters);
    require(rotary.target_hi_rate_hz() == 8.0F,
            "fast speed did not select the full Rate target");
    rotary.process(audio, 1);
    const float accelerating_rate = rotary.current_hi_rate_hz();
    require(accelerating_rate > slow_rate && accelerating_rate < 8.0F,
            "fast selection did not ramp toward the faster rotor speed");

    std::vector<float> longer_audio(48000, 0.0F);
    rotary.process(longer_audio, 1);
    require(rotary.current_hi_rate_hz() > 7.9F,
            "fast rotor did not approach its target after the time constant");

    parameters.speed = tabletfx::RotarySpeed::slow;
    rotary.set_parameters(parameters);
    rotary.process(audio, 1);
    require(rotary.current_hi_rate_hz() < 8.0F &&
                rotary.current_hi_rate_hz() > rotary.target_hi_rate_hz(),
            "slow selection did not decelerate toward the slow target");
}

void test_mix_bypass_input_select_and_stereo_width() {
    tabletfx::RotarySpeakerParameters parameters;
    parameters.mix = 0.0F;
    tabletfx::RotarySpeaker dry(48000.0F, 2, parameters);
    std::array<float, 32> original{};
    for (std::size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<float>(static_cast<int>(i % 9U) - 4) / 4.0F;
    }
    auto unchanged = original;
    dry.process(unchanged, 2);
    require(unchanged == original, "zero rotary mix changed the dry signal");

    parameters.mix = 1.0F;
    parameters.bypass = true;
    tabletfx::RotarySpeaker bypassed(48000.0F, 2, parameters);
    auto bypass_audio = original;
    bypassed.process(bypass_audio, 2);
    require(bypass_audio == original,
            "rotary bypass changed output while advancing its internal state");

    parameters.bypass = false;
    parameters.input_select = tabletfx::RotaryInputSelect::left;
    parameters.stereo_spread = 0.0F;
    parameters.speed = tabletfx::RotarySpeed::fast;
    parameters.hi_time_constant_ms = 10.0F;
    parameters.low_time_constant_ms = 10.0F;
    parameters.low_mic_spacing = 0.5F;
    parameters.high_mic_spacing = 1.0F;
    tabletfx::RotarySpeaker mono_image(48000.0F, 2, parameters);
    std::array<float, 4800> stereo{};
    for (std::size_t frame = 0; frame < stereo.size() / 2U; ++frame) {
        stereo[frame * 2U] = 0.5F;
        stereo[frame * 2U + 1U] = -0.25F;
    }
    mono_image.process(stereo, 2);
    for (std::size_t frame = 0; frame < stereo.size() / 2U; ++frame) {
        require(std::abs(stereo[frame * 2U] - stereo[frame * 2U + 1U]) < 1.0e-6F,
                "zero stereo spread did not collapse the wet image to mono");
    }

    parameters.stereo_spread = 1.0F;
    tabletfx::RotarySpeaker wide(48000.0F, 2, parameters);
    stereo.fill(0.5F);
    wide.process(stereo, 2);
    bool found_width = false;
    for (std::size_t frame = 100; frame < stereo.size() / 2U; ++frame) {
        if (std::abs(stereo[frame * 2U] - stereo[frame * 2U + 1U]) > 0.05F) {
            found_width = true;
            break;
        }
    }
    require(found_width,
            "mic spacing and stereo spread did not create rotary stereo movement");
}

void test_low_and_high_mic_spacing_are_independent() {
    const auto render = [](float frequency_hz, float low_spacing,
                          float high_spacing, bool isolate_low_band) {
        tabletfx::RotarySpeakerParameters parameters;
        parameters.speed = tabletfx::RotarySpeed::fast;
        parameters.rate_hz = 5.0F;
        parameters.low_time_constant_ms = 10.0F;
        parameters.hi_time_constant_ms = 10.0F;
        parameters.low_mic_spacing = low_spacing;
        parameters.high_mic_spacing = high_spacing;
        parameters.low_depth = isolate_low_band ? 1.0F : 0.0F;
        parameters.hi_depth = isolate_low_band ? 0.0F : 1.0F;
        parameters.rotor_length = 0.8F;
        tabletfx::RotarySpeaker rotary(48000.0F, 2, parameters);
        std::vector<float> audio(8192U * 2U);
        for (std::size_t frame = 0; frame < audio.size() / 2U; ++frame) {
            const float phase = 2.0F * 3.14159265358979323846F * frequency_hz *
                                static_cast<float>(frame) / 48000.0F;
            const float sample = 0.5F * std::sin(phase);
            audio[frame * 2U] = sample;
            audio[frame * 2U + 1U] = sample;
        }
        rotary.process(audio, 2);
        return audio;
    };

    const auto low_spacing_off = render(120.0F, 0.0F, 0.0F, true);
    const auto low_spacing_on = render(120.0F, 1.0F, 0.0F, true);
    const auto high_spacing_off = render(3000.0F, 0.0F, 0.0F, false);
    const auto high_spacing_on = render(3000.0F, 0.0F, 1.0F, false);

    const auto differs = [](const std::vector<float>& lhs,
                            const std::vector<float>& rhs) {
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            if (std::abs(lhs[i] - rhs[i]) > 1.0e-3F) {
                return true;
            }
        }
        return false;
    };
    require(differs(low_spacing_off, low_spacing_on),
            "LF mic spacing did not affect the drum modulation independently");
    require(differs(high_spacing_off, high_spacing_on),
            "HF mic spacing did not affect the horn modulation independently");
}

void test_rotor_controls_and_delay_are_finite() {
    tabletfx::RotarySpeakerParameters dry_controls;
    dry_controls.speed = tabletfx::RotarySpeed::fast;
    dry_controls.mix = 1.0F;
    dry_controls.hi_depth = 0.0F;
    dry_controls.low_depth = 0.0F;
    dry_controls.high_level = 1.0F;
    dry_controls.rotor_length = 0.0F;
    dry_controls.hi_time_constant_ms = 10.0F;
    dry_controls.low_time_constant_ms = 10.0F;
    tabletfx::RotarySpeaker reference(48000.0F, 1, dry_controls);

    auto changed_controls = dry_controls;
    changed_controls.rotor_length = 1.0F;
    changed_controls.high_level = 0.25F;
    changed_controls.drive = 0.8F;
    tabletfx::RotarySpeaker changed(48000.0F, 1, changed_controls);

    constexpr std::size_t frames = 12000;
    std::vector<float> source(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        source[frame] = 0.6F * std::sin(
            2.0F * 3.14159265358979323846F * 4000.0F *
            static_cast<float>(frame) / 48000.0F);
    }
    auto reference_audio = source;
    auto changed_audio = source;
    reference.process(reference_audio, 1);
    changed.process(changed_audio, 1);
    bool different = false;
    for (std::size_t frame = 100; frame < frames; ++frame) {
        require(std::isfinite(reference_audio[frame]) &&
                    std::isfinite(changed_audio[frame]),
                "rotor controls generated non-finite samples");
        if (std::abs(reference_audio[frame] - changed_audio[frame]) > 0.01F) {
            different = true;
        }
    }
    require(different,
            "rotor length, high level, and drive controls did not affect output");
}

void test_variable_blocks_preserve_motor_and_audio_state() {
    tabletfx::RotarySpeakerParameters parameters;
    parameters.speed = tabletfx::RotarySpeed::fast;
    parameters.rate_hz = 7.1F;
    parameters.low_rate_multi = 0.63F;
    parameters.low_time_constant_ms = 310.0F;
    parameters.hi_time_constant_ms = 170.0F;
    parameters.drive = 0.35F;
    parameters.rotor_length = 0.8F;
    parameters.low_mic_spacing = 0.42F;
    parameters.high_mic_spacing = 0.92F;
    parameters.stereo_spread = 1.4F;
    parameters.mix = 0.76F;
    tabletfx::RotarySpeaker whole(44100.0F, 2, parameters);
    tabletfx::RotarySpeaker chunks(44100.0F, 2, parameters);

    std::vector<float> expected(16384);
    for (std::size_t frame = 0; frame < expected.size() / 2U; ++frame) {
        const float phase = 2.0F * 3.14159265358979323846F * 330.0F *
                            static_cast<float>(frame) / 44100.0F;
        expected[frame * 2U] = 0.45F * std::sin(phase);
        expected[frame * 2U + 1U] = 0.32F * std::sin(phase + 0.4F);
    }
    auto actual = expected;
    whole.process(expected, 2);

    const std::array<std::size_t, 5> block_frames{1, 47, 128, 19, 256};
    std::size_t frame = 0;
    std::size_t block = 0;
    while (frame < actual.size() / 2U) {
        const std::size_t count = std::min(
            block_frames[block % block_frames.size()],
            actual.size() / 2U - frame);
        chunks.process(std::span<float>(actual).subspan(frame * 2U, count * 2U), 2);
        frame += count;
        ++block;
    }
    require(expected == actual,
            "rotary output changed with variable callback block sizes");
    require(whole.current_hi_rate_hz() == chunks.current_hi_rate_hz() &&
                whole.current_low_rate_hz() == chunks.current_low_rate_hz(),
            "rotary motor acceleration changed with variable block sizes");
}

void test_reset_invalid_samples_layout_and_no_allocations() {
    tabletfx::RotarySpeaker rotary;
    std::array<float, 8> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        0.5F,
        0.25F,
        -0.5F,
        std::numeric_limits<float>::quiet_NaN(),
        0.75F,
    };
    rotary.process(invalid, 2);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "rotary effect did not replace non-finite audio with finite output");

    std::array<float, 5> mismatch{0.1F, -0.2F, 0.3F, -0.4F, 0.5F};
    const auto before_mismatch = mismatch;
    rotary.process(mismatch, 1);
    require(mismatch == before_mismatch,
            "mismatched channel layout should leave the buffer untouched");

    std::array<float, 512> block{};
    block.fill(0.25F);
    const std::size_t before_allocations =
        allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        rotary.process(block, 2);
    }
    const std::size_t after_allocations =
        allocation_count.load(std::memory_order_relaxed);
    require(before_allocations == after_allocations,
            "rotary effect allocated during process()");
    require(std::all_of(block.begin(), block.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "rotary effect generated non-finite output");

    rotary.reset();
    require(rotary.current_low_rate_hz() == 0.0F &&
                rotary.current_hi_rate_hz() == 0.0F,
            "reset did not stop both rotor states");
    std::array<float, 16> after_reset{};
    after_reset.fill(0.4F);
    rotary.process(after_reset, 2);
    require(std::all_of(after_reset.begin(), after_reset.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "rotary reset did not restore usable processing state");
}

void test_extreme_finite_stereo_sum_does_not_poison_state() {
    tabletfx::RotarySpeakerParameters parameters;
    parameters.input_select = tabletfx::RotaryInputSelect::sum;
    parameters.speed = tabletfx::RotarySpeed::fast;
    parameters.hi_time_constant_ms = 10.0F;
    parameters.low_time_constant_ms = 10.0F;
    tabletfx::RotarySpeaker rotary(48000.0F, 2, parameters);

    const float largest = std::numeric_limits<float>::max();
    std::array<float, 2> extreme{largest, largest};
    rotary.process(extreme, 2);
    require(std::all_of(extreme.begin(), extreme.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "extreme finite stereo sum produced a non-finite output");

    std::array<float, 2048> normal{};
    for (std::size_t frame = 0; frame < normal.size() / 2U; ++frame) {
        const float sample = 0.5F * std::sin(
            2.0F * 3.14159265358979323846F * 440.0F *
            static_cast<float>(frame) / 48000.0F);
        normal[frame * 2U] = sample;
        normal[frame * 2U + 1U] = -0.75F * sample;
    }
    rotary.process(normal, 2);
    require(std::all_of(normal.begin(), normal.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "extreme finite input poisoned later rotary output");
    require(std::any_of(normal.begin(), normal.end(),
                        [](float sample) { return std::abs(sample) > 1.0e-4F; }),
            "rotary state stayed silent after recovering from extreme input");
}

void test_invalid_construction() {
    bool bad_rate = false;
    try {
        tabletfx::RotarySpeaker invalid(100.0F, 1);
    } catch (const std::invalid_argument&) {
        bad_rate = true;
    }
    require(bad_rate, "rotary effect accepted an unsupported sample rate");

    bool bad_channels = false;
    try {
        tabletfx::RotarySpeaker invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        bad_channels = true;
    }
    require(bad_channels, "rotary effect accepted more than two channels");
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
        test_defaults_ranges_and_bounded_storage();
        test_slow_fast_acceleration_and_deceleration();
        test_mix_bypass_input_select_and_stereo_width();
        test_low_and_high_mic_spacing_are_independent();
        test_rotor_controls_and_delay_are_finite();
        test_variable_blocks_preserve_motor_and_audio_state();
        test_reset_invalid_samples_layout_and_no_allocations();
        test_extreme_finite_stereo_sum_does_not_poison_state();
        test_invalid_construction();
        std::cout << "All TabletFX rotary speaker tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
