#include "tabletfx/effects/chorus.hpp"

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

std::vector<float> sine_tone(float frequency_hz, std::size_t frames,
                             std::uint16_t channels = 1U) {
    constexpr double sample_rate = 48000.0;
    constexpr double pi = 3.14159265358979323846;
    std::vector<float> result(frames * channels);
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        const float sample = 0.5F * static_cast<float>(std::sin(
            2.0 * pi * static_cast<double>(frequency_hz) *
            static_cast<double>(frame) / sample_rate));
        for (std::uint16_t channel = 0U; channel < channels; ++channel) {
            result[frame * channels + channel] = sample;
        }
    }
    return result;
}

double rms_after(std::span<const float> samples, std::size_t first_frame,
                 std::uint16_t channels = 1U) {
    double sum = 0.0;
    const std::size_t frames = samples.size() / channels;
    std::size_t count = 0U;
    for (std::size_t frame = first_frame; frame < frames; ++frame) {
        for (std::uint16_t channel = 0U; channel < channels; ++channel) {
            const double value = samples[frame * channels + channel];
            sum += value * value;
            ++count;
        }
    }
    return count == 0U ? 0.0 : std::sqrt(sum / static_cast<double>(count));
}

void test_defaults_bounds_and_memory() {
    tabletfx::Chorus chorus;
    const auto parameters = chorus.parameters();
    require(chorus.id() == "chorus", "chorus stable ID changed");
    require(parameters.rate_hz == 0.8F && parameters.base_delay_ms == 15.0F &&
                parameters.depth_ms == 7.0F && parameters.mix == 0.3F &&
                parameters.smoothing_ms == 10.0F && !parameters.tempo_sync &&
                parameters.tempo_bpm == 120.0F && parameters.number_of_voices == 1U &&
                parameters.minimum_delay_ms == 8.0F &&
                parameters.delay_range_ms == 14.0F &&
                parameters.low_cut_hz == 0.0F &&
                parameters.high_cut_hz == 0.0F &&
                parameters.stereo_width == 1.0F &&
                chorus.effective_rate_hz() == 0.8F,
            "sane chorus defaults changed");
    require(chorus.memory_bytes() == sizeof(chorus) + 8644U * sizeof(float),
            "stereo chorus storage estimate is inaccurate at 48 kHz");

    tabletfx::ChorusParameters bounded;
    bounded.rate_hz = 100.0F;
    bounded.base_delay_ms = 30.0F;
    bounded.depth_ms = 12.0F;
    bounded.mix = -1.0F;
    bounded.smoothing_ms = 2000.0F;
    bounded.low_cut_hz = -10.0F;
    bounded.high_cut_hz = 30000.0F;
    bounded.stereo_width = 5.0F;
    chorus.set_parameters(bounded);
    const auto limits = chorus.parameters();
    require(limits.rate_hz == 8.0F && limits.base_delay_ms == 30.0F &&
                limits.depth_ms == 0.0F && limits.mix == 0.0F &&
                limits.smoothing_ms == 1000.0F &&
                limits.low_cut_hz == 0.0F &&
                limits.high_cut_hz == 20000.0F &&
                limits.stereo_width == 2.0F,
            "chorus parameter bounds were not enforced");

    bounded.rate_hz = std::numeric_limits<float>::quiet_NaN();
    bounded.base_delay_ms = std::numeric_limits<float>::infinity();
    bounded.depth_ms = std::numeric_limits<float>::quiet_NaN();
    bounded.mix = std::numeric_limits<float>::infinity();
    bounded.smoothing_ms = std::numeric_limits<float>::quiet_NaN();
    bounded.tempo_bpm = std::numeric_limits<float>::quiet_NaN();
    bounded.low_cut_hz = std::numeric_limits<float>::quiet_NaN();
    bounded.high_cut_hz = std::numeric_limits<float>::infinity();
    bounded.stereo_width = std::numeric_limits<float>::quiet_NaN();
    chorus.set_parameters(bounded);
    const auto finite = chorus.parameters();
    require(finite.rate_hz == 0.8F && finite.base_delay_ms == 15.0F &&
                finite.depth_ms <= 7.0F && finite.mix == 0.3F &&
                finite.smoothing_ms == 10.0F && finite.tempo_bpm == 120.0F &&
                finite.low_cut_hz == 0.0F && finite.high_cut_hz == 0.0F &&
                finite.stereo_width == 1.0F,
            "non-finite chorus parameters did not use safe defaults");
}

void test_project_authored_tone_master_controls() {
    constexpr std::size_t frames = 48000U;
    constexpr std::size_t steady_start = 4800U;

    tabletfx::ChorusParameters low_cut_parameters;
    low_cut_parameters.mix = 0.0F;
    low_cut_parameters.smoothing_ms = 0.0F;
    low_cut_parameters.low_cut_hz = 200.0F;
    tabletfx::Chorus low_cut(48000.0F, 1U, low_cut_parameters);
    auto low_tone = sine_tone(40.0F, frames);
    auto mid_tone = sine_tone(1000.0F, frames);
    const double low_tone_input = rms_after(low_tone, steady_start);
    const double mid_tone_input = rms_after(mid_tone, steady_start);
    low_cut.process(low_tone, 1U);
    low_cut.reset();
    low_cut.process(mid_tone, 1U);
    require(rms_after(low_tone, steady_start) < low_tone_input * 0.4,
            "project low cut did not attenuate frequencies below its setting");
    require(rms_after(mid_tone, steady_start) > mid_tone_input * 0.9,
            "project low cut altered frequencies well above its setting");

    tabletfx::ChorusParameters high_cut_parameters;
    high_cut_parameters.mix = 0.0F;
    high_cut_parameters.smoothing_ms = 0.0F;
    high_cut_parameters.high_cut_hz = 2000.0F;
    tabletfx::Chorus high_cut(48000.0F, 1U, high_cut_parameters);
    auto high_tone = sine_tone(10000.0F, frames);
    auto low_mid_tone = sine_tone(200.0F, frames);
    const double high_tone_input = rms_after(high_tone, steady_start);
    const double low_mid_tone_input = rms_after(low_mid_tone, steady_start);
    high_cut.process(high_tone, 1U);
    high_cut.reset();
    high_cut.process(low_mid_tone, 1U);
    require(rms_after(high_tone, steady_start) < high_tone_input * 0.4,
            "project high cut did not attenuate frequencies above its setting");
    require(rms_after(low_mid_tone, steady_start) > low_mid_tone_input * 0.9,
            "project high cut altered frequencies well below its setting");

    tabletfx::ChorusParameters width_parameters;
    width_parameters.mix = 0.0F;
    width_parameters.smoothing_ms = 0.0F;
    width_parameters.stereo_width = 0.0F;
    tabletfx::Chorus mono_width(48000.0F, 2U, width_parameters);
    std::array<float, 2U> collapsed{0.8F, -0.2F};
    mono_width.process(collapsed, 2U);
    require(std::abs(collapsed[0] - 0.3F) < 1.0e-6F &&
                std::abs(collapsed[1] - 0.3F) < 1.0e-6F,
            "zero stereo width did not collapse the signal to its mid channel");

    width_parameters.stereo_width = 2.0F;
    tabletfx::Chorus widened(48000.0F, 2U, width_parameters);
    std::array<float, 2U> wide{0.8F, -0.2F};
    widened.process(wide, 2U);
    require(std::abs((wide[0] - wide[1]) - 2.0F) < 1.0e-6F &&
                std::abs((wide[0] + wide[1]) - 0.6F) < 1.0e-6F,
            "stereo width must scale only the side signal");
}

void test_minimum_delay_range_bounds_and_legacy_center_depth() {
    tabletfx::Chorus chorus;
    auto bounded = chorus.parameters();
    bounded.minimum_delay_ms = 0.0F;
    bounded.delay_range_ms = 0.0F;
    chorus.set_parameters(bounded);
    require(chorus.parameters().minimum_delay_ms == 0.1F &&
                chorus.parameters().delay_range_ms == 1.0F,
            "minimum delay/range lower bounds were not enforced");
    bounded = chorus.parameters();
    bounded.minimum_delay_ms = 100.0F;
    bounded.delay_range_ms = 100.0F;
    chorus.set_parameters(bounded);
    require(chorus.parameters().minimum_delay_ms == 50.0F &&
                chorus.parameters().delay_range_ms == 40.0F,
            "minimum delay/range upper bounds were not enforced");

    tabletfx::ChorusParameters legacy;
    legacy.rate_hz = 1.25F;
    legacy.base_delay_ms = 12.0F;
    legacy.depth_ms = 5.0F;
    legacy.mix = 0.65F;
    legacy.smoothing_ms = 0.0F;
    tabletfx::ChorusParameters canonical = legacy;
    canonical.minimum_delay_ms = 7.0F;
    canonical.delay_range_ms = 10.0F;
    canonical.base_delay_ms = 15.0F;
    canonical.depth_ms = 7.0F;
    tabletfx::Chorus legacy_chorus(48000.0F, 2, legacy);
    tabletfx::Chorus canonical_chorus(48000.0F, 2, canonical);
    require(legacy_chorus.parameters().minimum_delay_ms == 7.0F &&
                legacy_chorus.parameters().delay_range_ms == 10.0F &&
                legacy_chorus.parameters().base_delay_ms == 12.0F &&
                legacy_chorus.parameters().depth_ms == 5.0F,
            "legacy center/depth did not map to minimum/range");
    std::vector<float> source(4096U);
    for (std::size_t frame = 0; frame < source.size() / 2U; ++frame) {
        const float sample = 0.3F * static_cast<float>(std::sin(
            2.0 * 3.14159265358979323846 * 330.0 *
            static_cast<double>(frame) / 48000.0));
        source[frame * 2U] = sample;
        source[frame * 2U + 1U] = -sample * 0.4F;
    }
    auto legacy_output = source;
    auto canonical_output = source;
    legacy_chorus.process(legacy_output, 2);
    canonical_chorus.process(canonical_output, 2);
    require(legacy_output == canonical_output,
            "legacy center/depth and equivalent minimum/range audio differ");
}

void test_minimum_and_maximum_sweep_timing() {
    tabletfx::ChorusParameters minimum_parameters;
    minimum_parameters.rate_hz = 0.05F;
    minimum_parameters.minimum_delay_ms = 0.1F;
    minimum_parameters.delay_range_ms = 1.0F;
    minimum_parameters.mix = 1.0F;
    minimum_parameters.smoothing_ms = 0.0F;
    tabletfx::Chorus minimum(8000.0F, 1, minimum_parameters);
    std::vector<float> to_minimum(120000U, 0.0F);
    minimum.process(to_minimum, 1);
    std::array<float, 1> impulse{1.0F};
    minimum.process(impulse, 1);
    std::array<float, 4> minimum_response{};
    minimum.process(minimum_response, 1);
    require(std::abs(minimum_response[0] - 1.0F) < 1.0e-5F,
            "0.1 ms minimum-delay sweep did not reach the one-sample floor");

    tabletfx::ChorusParameters maximum_parameters;
    maximum_parameters.rate_hz = 0.05F;
    maximum_parameters.minimum_delay_ms = 50.0F;
    maximum_parameters.delay_range_ms = 40.0F;
    maximum_parameters.mix = 1.0F;
    maximum_parameters.smoothing_ms = 0.0F;
    tabletfx::Chorus maximum(8000.0F, 1, maximum_parameters);
    std::vector<float> to_maximum(40000U, 0.0F);
    maximum.process(to_maximum, 1);
    std::array<float, 1> maximum_impulse{1.0F};
    maximum.process(maximum_impulse, 1);
    std::array<float, 730> maximum_response{};
    maximum.process(maximum_response, 1);
    const auto peak = std::max_element(
        maximum_response.begin(), maximum_response.end(),
        [](float left, float right) { return std::abs(left) < std::abs(right); });
    const std::size_t peak_frame = static_cast<std::size_t>(
        peak - maximum_response.begin()) + 1U;
    require(peak_frame >= 718U && peak_frame <= 722U && *peak > 0.7F,
            "50 ms minimum plus 40 ms range peak was at frame " +
                std::to_string(peak_frame) + " with value " +
                std::to_string(*peak));
}

void test_default_is_sample_identical_to_legacy_center_depth_path() {
    constexpr float sample_rate = 48000.0F;
    constexpr std::uint16_t channels = 2U;
    constexpr std::size_t frames = 2048U;
    constexpr std::size_t legacy_capacity_frames = 1538U;
    constexpr float pi = 3.14159265358979323846F;
    constexpr float two_pi = 2.0F * pi;
    std::vector<float> source(frames * channels);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float sample = 0.35F * static_cast<float>(std::sin(
            2.0 * 3.14159265358979323846 * 220.0 *
            static_cast<double>(frame) / sample_rate));
        source[frame * 2U] = sample;
        source[frame * 2U + 1U] = sample * -0.6F;
    }

    auto expected = source;
    std::vector<float> legacy_storage(legacy_capacity_frames * channels, 0.0F);
    float sine_increment = std::sin(two_pi * 0.8F / sample_rate);
    float cosine_increment = std::cos(two_pi * 0.8F / sample_rate);
    float sine_phase = 0.0F;
    float cosine_phase = 1.0F;
    std::size_t write_frame = 0U;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::uint16_t channel = 0U; channel < channels; ++channel) {
            const std::size_t index = frame * channels + channel;
            const float input = source[index];
            legacy_storage[write_frame * channels + channel] = input;
            const float modulation = channel == 0U ? sine_phase : -sine_phase;
            const float delay = std::clamp(
                15.0F * (sample_rate * 0.001F) +
                    7.0F * (sample_rate * 0.001F) * modulation,
                1.0F, static_cast<float>(legacy_capacity_frames - 2U));
            const std::size_t whole_delay = static_cast<std::size_t>(delay);
            const float fraction = delay - static_cast<float>(whole_delay);
            std::size_t read_frame = write_frame + legacy_capacity_frames - whole_delay;
            if (read_frame >= legacy_capacity_frames) read_frame -= legacy_capacity_frames;
            const std::size_t older_frame = read_frame == 0U
                ? legacy_capacity_frames - 1U : read_frame - 1U;
            const float newer = legacy_storage[read_frame * channels + channel];
            const float older = legacy_storage[older_frame * channels + channel];
            const float wet = newer * (1.0F - fraction) + older * fraction;
            expected[index] = input * 0.7F + wet * 0.3F;
        }
        ++write_frame;
        if (write_frame == legacy_capacity_frames) write_frame = 0U;
        const float old_sine = sine_phase;
        sine_phase = old_sine * cosine_increment +
                     cosine_phase * sine_increment;
        cosine_phase = cosine_phase * cosine_increment -
                       old_sine * sine_increment;
    }

    tabletfx::Chorus chorus(sample_rate, channels);
    auto actual = source;
    chorus.process(actual, channels);
    require(actual == expected,
            "default 8 ms minimum/14 ms range changed the legacy 15±7 ms output");
}

void test_number_of_voices_bounds_and_one_voice_identity() {
    tabletfx::ChorusParameters parameters;
    parameters.base_delay_ms = 12.0F;
    parameters.depth_ms = 5.0F;
    parameters.mix = 0.75F;
    parameters.smoothing_ms = 0.0F;

    tabletfx::Chorus one_voice_default(48000.0F, 2, parameters);
    parameters.number_of_voices = 1U;
    tabletfx::Chorus one_voice_explicit(48000.0F, 2, parameters);
    parameters.number_of_voices = 9U;
    tabletfx::Chorus eight_voice(48000.0F, 2, parameters);
    require(one_voice_default.parameters().number_of_voices == 1U &&
                one_voice_explicit.parameters().number_of_voices == 1U &&
                eight_voice.parameters().number_of_voices == 8U,
            "voice count did not default to one or clamp to the 1–8 project range");
    require(one_voice_default.memory_bytes() == eight_voice.memory_bytes(),
            "voice count allocated additional delay storage");

    parameters.number_of_voices = 0U;
    one_voice_default.set_parameters(parameters);
    require(one_voice_default.parameters().number_of_voices == 1U,
            "voice count did not clamp zero to the project minimum of one");
    parameters.number_of_voices = 255U;
    one_voice_default.set_parameters(parameters);
    require(one_voice_default.parameters().number_of_voices == 8U,
            "voice count did not clamp above eight to the project maximum");

    parameters.number_of_voices = 1U;
    one_voice_default.set_parameters(parameters);
    std::vector<float> source(8192U);
    for (std::size_t frame = 0; frame < source.size() / 2U; ++frame) {
        const float sample = 0.4F * static_cast<float>(std::sin(
            2.0 * 3.14159265358979323846 * 220.0 *
            static_cast<double>(frame) / 48000.0));
        source[frame * 2U] = sample;
        source[frame * 2U + 1U] = sample * -0.5F;
    }
    auto default_output = source;
    auto explicit_output = source;
    one_voice_default.process(default_output, 2);
    one_voice_explicit.process(explicit_output, 2);
    require(default_output == explicit_output,
            "explicit one voice changed the legacy single-voice sample path");
}

void test_multi_voice_output_and_partition_continuity() {
    tabletfx::ChorusParameters one_parameters;
    one_parameters.rate_hz = 1.3F;
    one_parameters.base_delay_ms = 11.0F;
    one_parameters.depth_ms = 5.0F;
    one_parameters.mix = 0.8F;
    one_parameters.smoothing_ms = 0.0F;
    tabletfx::ChorusParameters multi_parameters = one_parameters;
    multi_parameters.number_of_voices = 4U;

    std::vector<float> source(16000U);
    for (std::size_t frame = 0; frame < source.size() / 2U; ++frame) {
        const float sample = 0.35F * static_cast<float>(std::sin(
            2.0 * 3.14159265358979323846 * 220.0 *
            static_cast<double>(frame) / 48000.0));
        source[frame * 2U] = sample;
        source[frame * 2U + 1U] = sample * 0.7F;
    }
    auto single_output = source;
    auto multi_whole_output = source;
    tabletfx::Chorus single(48000.0F, 2, one_parameters);
    tabletfx::Chorus multi_whole(48000.0F, 2, multi_parameters);
    single.process(single_output, 2);
    multi_whole.process(multi_whole_output, 2);
    require(single_output != multi_whole_output,
            "four chorus voices did not change the processed output");
    require(std::all_of(multi_whole_output.begin(), multi_whole_output.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "multi-voice chorus emitted a non-finite sample");

    auto multi_partitioned_output = source;
    tabletfx::Chorus multi_partitioned(48000.0F, 2, multi_parameters);
    std::size_t offset = 0U;
    for (const std::size_t block_frames : {1U, 17U, 128U, 257U, 64U, 511U}) {
        const std::size_t remaining_frames =
            multi_partitioned_output.size() / 2U - offset;
        const std::size_t frames = std::min(block_frames, remaining_frames);
        multi_partitioned.process(
            std::span<float>(multi_partitioned_output.data() + offset * 2U,
                             frames * 2U),
            2);
        offset += frames;
        if (offset == multi_partitioned_output.size() / 2U) break;
    }
    if (offset < multi_partitioned_output.size() / 2U) {
        multi_partitioned.process(
            std::span<float>(multi_partitioned_output.data() + offset * 2U,
                             multi_partitioned_output.size() - offset * 2U),
            2);
    }
    require(multi_whole_output.size() == multi_partitioned_output.size(),
            "multi-voice partitioned audio buffer size changed");
    for (std::size_t i = 0; i < multi_whole_output.size(); ++i) {
        require(std::abs(multi_whole_output[i] - multi_partitioned_output[i]) < 1.0e-6F,
                "multi-voice output depends on callback partition boundaries");
    }
}

void test_quarter_note_tempo_rate() {
    tabletfx::Chorus chorus(48000.0F, 1);
    auto parameters = chorus.parameters();
    parameters.tempo_sync = true;
    parameters.rate_hz = 0.8F;
    parameters.tempo_bpm = 120.0F;
    chorus.set_parameters(parameters);
    require(chorus.effective_rate_hz() == 2.0F &&
                chorus.parameters().rate_hz == 0.8F,
            "quarter-note sync at 120 BPM should use 2 Hz and preserve manual rate");

    parameters.tempo_bpm = 60.0F;
    chorus.set_parameters(parameters);
    require(chorus.effective_rate_hz() == 1.0F,
            "quarter-note sync at 60 BPM should use 1 Hz");
    parameters.tempo_bpm = 20.0F;
    chorus.set_parameters(parameters);
    require(std::abs(chorus.effective_rate_hz() - (1.0F / 3.0F)) < 1.0e-6F,
            "quarter-note sync at 20 BPM should use one third Hz");
    parameters.tempo_bpm = 300.0F;
    chorus.set_parameters(parameters);
    require(chorus.effective_rate_hz() == 5.0F,
            "quarter-note sync at 300 BPM should use 5 Hz");
    parameters.tempo_bpm = 10.0F;
    chorus.set_parameters(parameters);
    require(chorus.parameters().tempo_bpm == 20.0F,
            "Chorus BPM should clamp to the project-authored 20 BPM minimum");
    parameters.tempo_bpm = 400.0F;
    chorus.set_parameters(parameters);
    require(chorus.parameters().tempo_bpm == 300.0F,
            "Chorus BPM should clamp to the project-authored 300 BPM maximum");
    parameters.tempo_bpm = std::numeric_limits<float>::quiet_NaN();
    chorus.set_parameters(parameters);
    require(chorus.parameters().tempo_bpm == 120.0F,
            "non-finite Chorus BPM should fall back to the project default");

    std::vector<float> source(48000);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = 0.4F * static_cast<float>(std::sin(
            2.0 * 3.14159265358979323846 * 220.0 *
            static_cast<double>(i) / 48000.0));
    }
    parameters.tempo_bpm = 120.0F;
    parameters.depth_ms = 5.0F;
    parameters.mix = 0.7F;
    tabletfx::Chorus synced(48000.0F, 1, parameters);
    parameters.tempo_sync = false;
    tabletfx::Chorus manual(48000.0F, 1, parameters);
    require(manual.effective_rate_hz() == 0.8F,
            "Free Hz mode should restore the unchanged manual rate");
    auto synced_audio = source;
    auto manual_audio = source;
    synced.process(synced_audio, 1);
    manual.process(manual_audio, 1);
    require(synced_audio != manual_audio,
            "quarter-note sync should change the chorus modulation on local audio");
}

void test_integer_delay_and_mono_stereo_isolation() {
    tabletfx::ChorusParameters parameters;
    parameters.base_delay_ms = 8.0F;
    parameters.depth_ms = 0.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::Chorus mono(8000.0F, 1, parameters);
    std::array<float, 80> impulse{};
    impulse[0] = 1.0F;
    mono.process(impulse, 1);
    require(std::abs(impulse[64] - 1.0F) < 1.0e-6F,
            "chorus wet impulse did not appear at the configured delay");
    require(std::all_of(impulse.begin(), impulse.begin() + 64,
                        [](float sample) { return std::abs(sample) < 1.0e-7F; }),
            "chorus produced wet output before its configured delay");

    parameters.base_delay_ms = 2.0F;
    tabletfx::Chorus stereo(8000.0F, 2, parameters);
    std::array<float, 64> stereo_impulse{};
    stereo_impulse[0] = 1.0F;
    stereo.process(stereo_impulse, 2);
    require(std::abs(stereo_impulse[32] - 1.0F) < 1.0e-6F,
            "stereo chorus left-channel delay is incorrect");
    for (std::size_t i = 1; i < stereo_impulse.size(); i += 2) {
        require(std::abs(stereo_impulse[i]) < 1.0e-7F,
                "stereo chorus leaked the left-channel impulse into the right channel");
    }

    std::array<float, 4> wrong_layout{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before = wrong_layout;
    stereo.process(wrong_layout, 1);
    require(wrong_layout == before,
            "chorus should leave a mismatched channel layout untouched");
}

void test_modulation_and_mix() {
    tabletfx::ChorusParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.base_delay_ms = 8.0F;
    parameters.depth_ms = 4.0F;
    parameters.mix = 1.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::Chorus chorus(8000.0F, 1, parameters);
    std::vector<float> input(8000);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = 0.5F * static_cast<float>(std::sin(
            2.0 * 3.14159265358979323846 * 300.0 *
            static_cast<double>(i) / 8000.0));
    }
    const auto dry = input;
    chorus.process(input, 1);
    double squared_difference = 0.0;
    for (std::size_t i = 256; i < input.size(); ++i) {
        const double difference = static_cast<double>(input[i] - dry[i]);
        squared_difference += difference * difference;
        require(std::isfinite(input[i]), "chorus emitted non-finite output");
    }
    require(squared_difference / static_cast<double>(input.size() - 256) > 0.01,
            "modulated wet path did not alter a steady guitar-range tone");

    parameters.mix = 0.0F;
    tabletfx::Chorus dry_chorus(8000.0F, 1, parameters);
    std::array<float, 8> dry_block{0.1F, -0.2F, 0.3F, -0.4F,
                                   0.5F, -0.6F, 0.7F, -0.8F};
    const auto original = dry_block;
    dry_chorus.process(dry_block, 1);
    require(dry_block == original,
            "zero-mix chorus should be exact dry passthrough");
}

void test_parameter_ramp_block_invariance_and_reset() {
    tabletfx::ChorusParameters initial;
    initial.smoothing_ms = 10.0F;
    tabletfx::Chorus whole(48000.0F, 1, initial);
    tabletfx::Chorus chunked(48000.0F, 1, initial);
    std::vector<float> source(5000);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = 0.4F * static_cast<float>(std::sin(
            2.0 * 3.14159265358979323846 * 220.0 *
            static_cast<double>(i) / 48000.0));
    }
    std::vector<float> a = source;
    std::vector<float> b = source;
    whole.process(std::span<float>(a.data(), 73), 1);
    chunked.process(std::span<float>(b.data(), 73), 1);

    tabletfx::ChorusParameters changed = initial;
    changed.rate_hz = 3.5F;
    changed.minimum_delay_ms = 9.0F;
    changed.delay_range_ms = 16.0F;
    changed.mix = 0.75F;
    whole.set_parameters(changed);
    chunked.set_parameters(changed);
    require(std::abs(whole.current_mix() - initial.mix) < 1.0e-7F,
            "chorus mix jumped instead of starting its smoothing ramp");
    whole.process(std::span<float>(a.data() + 73, a.size() - 73), 1);

    std::size_t offset = 73;
    for (const std::size_t block : {1U, 17U, 128U, 257U, 64U, 511U}) {
        const std::size_t count = std::min(block, b.size() - offset);
        chunked.process(std::span<float>(b.data() + offset, count), 1);
        offset += count;
        if (offset == b.size()) break;
    }
    if (offset < b.size()) {
        chunked.process(std::span<float>(b.data() + offset, b.size() - offset), 1);
    }
    require(a.size() == b.size(), "chorus block-invariance buffer size changed");
    for (std::size_t i = 0; i < a.size(); ++i) {
        require(std::abs(a[i] - b[i]) < 1.0e-6F,
                "chorus output depends on callback block boundaries");
    }
    require(std::abs(whole.current_mix() - changed.mix) < 1.0e-6F,
            "chorus parameter ramp did not reach its target");

    whole.reset();
    tabletfx::Chorus fresh(48000.0F, 1, changed);
    std::array<float, 256> after_reset{};
    std::array<float, 256> from_fresh{};
    for (std::size_t i = 0; i < after_reset.size(); ++i) {
        after_reset[i] = from_fresh[i] = source[i];
    }
    whole.process(after_reset, 1);
    fresh.process(from_fresh, 1);
    require(after_reset == from_fresh,
            "chorus reset did not match a freshly constructed processor");
}

void test_invalid_samples_and_zero_process_allocations() {
    tabletfx::Chorus mono(48000.0F, 1);
    std::array<float, 5> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.25F, -0.5F};
    mono.process(invalid, 1);
    require(invalid[0] == 0.0F && invalid[1] == 0.0F && invalid[2] == 0.0F,
            "chorus did not replace non-finite input with finite silence");

    std::array<float, 4> untouched{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before_zero_channels = untouched;
    mono.process(untouched, 0);
    require(untouched == before_zero_channels,
            "zero-channel chorus processing should leave data untouched");

    std::array<float, 512> block{};
    block.fill(0.25F);
    tabletfx::ChorusParameters synced_parameters;
    synced_parameters.tempo_sync = true;
    synced_parameters.tempo_bpm = 120.0F;
    synced_parameters.number_of_voices = 8U;
    tabletfx::Chorus stereo(48000.0F, 2, synced_parameters);
    const std::size_t before = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        stereo.process(block, 2);
    }
    const std::size_t after = allocation_count.load(std::memory_order_relaxed);
    require(after == before, "chorus allocated memory during process()");
    require(std::all_of(block.begin(), block.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "chorus emitted a non-finite sample");
}

void test_invalid_construction() {
    bool rejected_rate = false;
    try {
        tabletfx::Chorus invalid(100.0F, 1);
    } catch (const std::invalid_argument&) {
        rejected_rate = true;
    }
    require(rejected_rate, "chorus accepted an unsupported sample rate");

    bool rejected_channels = false;
    try {
        tabletfx::Chorus invalid(48000.0F, 3);
    } catch (const std::invalid_argument&) {
        rejected_channels = true;
    }
    require(rejected_channels, "chorus accepted an unsupported channel count");
}

} // namespace

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size); memory != nullptr) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_bounds_and_memory();
        test_project_authored_tone_master_controls();
        test_minimum_delay_range_bounds_and_legacy_center_depth();
        test_minimum_and_maximum_sweep_timing();
        test_default_is_sample_identical_to_legacy_center_depth_path();
        test_number_of_voices_bounds_and_one_voice_identity();
        test_multi_voice_output_and_partition_continuity();
        test_quarter_note_tempo_rate();
        test_integer_delay_and_mono_stereo_isolation();
        test_modulation_and_mix();
        test_parameter_ramp_block_invariance_and_reset();
        test_invalid_samples_and_zero_process_allocations();
        test_invalid_construction();
        std::cout << "All TabletFX chorus tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
