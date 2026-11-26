#include "tabletfx/effects/manual_thru_zero_flanger.hpp"

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
    if (!condition) throw std::runtime_error(message);
}

void test_documented_controls_and_project_bounds() {
    tabletfx::ManualThruZeroFlanger flanger;
    const auto defaults = flanger.parameters();
    require(flanger.id() == "manual-thru-zero-flanger",
            "manual through-zero flanger ID changed");
    require(defaults.minimum_time_ms == 0.34F && defaults.maximum_time_ms == 8.0F &&
                defaults.manual == 5.0F && defaults.dry_delay_position == 0.5F &&
                defaults.feedback_percent == 0.0F &&
                defaults.phase_reverse == tabletfx::FlangerPhaseReverse::Both &&
                defaults.mode == tabletfx::ThruZeroMode::Manual &&
                defaults.lfo_rate_hz == 0.25F && defaults.lfo_depth == 5.0F &&
                defaults.lfo_waveform == tabletfx::ThruZeroLfoWaveform::Sine &&
                defaults.lfo_phase_degrees == 180.0F &&
                defaults.vco_response == tabletfx::FlangerVcoResponse::Linear &&
                defaults.vco_exponent == 1.0F &&
                defaults.mix == 0.5F && defaults.smoothing_ms == 10.0F,
            "project-authored through-zero defaults changed");
    require(flanger.current_wet_time_ms() == flanger.current_dry_time_ms(),
            "default Manual and Dry Delay positions should meet at the zero point");
    require(flanger.maximum_path_latency_frames() == 384U,
            "reported 8 ms maximum path delay is inaccurate at 48 kHz");

    tabletfx::ManualThruZeroFlangerParameters parameters;
    parameters.minimum_time_ms = -1.0F;
    parameters.maximum_time_ms = 200.0F;
    parameters.manual = 100.0F;
    parameters.dry_delay_position = -2.0F;
    parameters.feedback_percent = 200.0F;
    parameters.phase_reverse = static_cast<tabletfx::FlangerPhaseReverse>(99);
    parameters.mode = static_cast<tabletfx::ThruZeroMode>(99);
    parameters.lfo_waveform = static_cast<tabletfx::ThruZeroLfoWaveform>(99);
    parameters.lfo_rate_hz = 99.0F;
    parameters.lfo_depth = -1.0F;
    parameters.lfo_phase_degrees = 400.0F;
    parameters.vco_response = static_cast<tabletfx::FlangerVcoResponse>(99);
    parameters.vco_exponent = 99.0F;
    parameters.mix = -1.0F;
    parameters.smoothing_ms = 2000.0F;
    flanger.set_parameters(parameters);
    const auto bounded = flanger.parameters();
    require(bounded.minimum_time_ms == 0.34F && bounded.maximum_time_ms == 20.0F &&
                bounded.manual == 10.0F && bounded.dry_delay_position == 0.0F &&
                bounded.feedback_percent == 100.0F &&
                bounded.phase_reverse == tabletfx::FlangerPhaseReverse::Both &&
                bounded.mode == tabletfx::ThruZeroMode::Manual &&
                bounded.lfo_waveform == tabletfx::ThruZeroLfoWaveform::Sine &&
                bounded.lfo_rate_hz == 8.0F && bounded.lfo_depth == 0.0F &&
                bounded.lfo_phase_degrees == 360.0F &&
                bounded.vco_response == tabletfx::FlangerVcoResponse::Linear &&
                bounded.vco_exponent == 4.0F &&
                bounded.mix == 0.0F && bounded.smoothing_ms == 1000.0F,
            "through-zero parameter bounds or invalid enum fallback changed");

    parameters = {};
    parameters.minimum_time_ms = 19.999F;
    parameters.maximum_time_ms = 0.34F;
    parameters.manual = std::numeric_limits<float>::quiet_NaN();
    parameters.dry_delay_position = std::numeric_limits<float>::infinity();
    parameters.feedback_percent = -std::numeric_limits<float>::infinity();
    parameters.mode = tabletfx::ThruZeroMode::Auto;
    parameters.lfo_rate_hz = std::numeric_limits<float>::quiet_NaN();
    parameters.lfo_depth = std::numeric_limits<float>::infinity();
    parameters.lfo_phase_degrees = -std::numeric_limits<float>::infinity();
    parameters.vco_response = tabletfx::FlangerVcoResponse::Exponential;
    parameters.vco_exponent = std::numeric_limits<float>::quiet_NaN();
    parameters.mix = std::numeric_limits<float>::quiet_NaN();
    parameters.smoothing_ms = std::numeric_limits<float>::quiet_NaN();
    flanger.set_parameters(parameters);
    const auto finite = flanger.parameters();
    require(finite.minimum_time_ms == 19.99F && finite.maximum_time_ms == 20.0F &&
                finite.manual == 5.0F && finite.dry_delay_position == 0.5F &&
                finite.feedback_percent == 0.0F &&
                finite.mode == tabletfx::ThruZeroMode::Auto &&
                finite.lfo_rate_hz == 0.25F && finite.lfo_depth == 5.0F &&
                finite.lfo_phase_degrees == 180.0F && finite.mix == 0.5F &&
                finite.vco_response == tabletfx::FlangerVcoResponse::Exponential &&
                finite.vco_exponent == 1.0F &&
                finite.smoothing_ms == 10.0F,
            "invalid through-zero parameters did not fall back to bounded values");

    parameters.vco_exponent = 0.0F;
    flanger.set_parameters(parameters);
    require(flanger.parameters().vco_exponent == 0.25F,
            "VCO exponent lower project bound changed");
}

void test_vco_response_curves_and_neutral_exponent() {
    const auto sweep_time = [](tabletfx::FlangerVcoResponse response,
                               float exponent) {
        tabletfx::ManualThruZeroFlangerParameters parameters;
        parameters.minimum_time_ms = 2.0F;
        parameters.maximum_time_ms = 6.0F;
        parameters.manual = 5.0F;
        parameters.mode = tabletfx::ThruZeroMode::Auto;
        parameters.lfo_rate_hz = 1.0F;
        parameters.lfo_depth = 10.0F;
        parameters.lfo_waveform = tabletfx::ThruZeroLfoWaveform::Sine;
        parameters.vco_response = response;
        parameters.vco_exponent = exponent;
        parameters.mix = 0.0F;
        parameters.smoothing_ms = 0.0F;
        tabletfx::ManualThruZeroFlanger flanger(8000.0F, 1, parameters);
        std::array<float, 1000> phase_advance{};
        flanger.process(phase_advance, 1);
        return flanger.current_wet_time_ms();
    };

    constexpr float kPi = 3.14159265358979323846F;
    const float linear_position = 0.5F + 0.5F * std::sin(kPi * 0.25F);
    const float linear_time = 2.0F + 4.0F * linear_position;
    const float square_time = 2.0F + 4.0F * linear_position * linear_position;
    const float exponential_lower_time = 2.0F + 4.0F * std::sqrt(linear_position);
    require(std::abs(sweep_time(tabletfx::FlangerVcoResponse::Linear, 1.0F) -
                     linear_time) < 1.0e-4F,
            "Linear VCO response changed the normalized sweep mapping");
    require(std::abs(sweep_time(tabletfx::FlangerVcoResponse::Exponential, 2.0F) -
                     square_time) < 1.0e-4F,
            "Exponential VCO response does not apply the project exponent");
    require(std::abs(sweep_time(tabletfx::FlangerVcoResponse::Exponential, 0.5F) -
                     exponential_lower_time) < 1.0e-4F,
            "Exponent below one should favor longer-delay sweep positions");
    require(std::abs(sweep_time(tabletfx::FlangerVcoResponse::SquareLaw, 1.0F) -
                     square_time) < 1.0e-4F &&
                std::abs(sweep_time(tabletfx::FlangerVcoResponse::SquareLaw, 4.0F) -
                         square_time) < 1.0e-4F,
            "Square-Law must square the sweep position and ignore Exponent");

    tabletfx::ManualThruZeroFlangerParameters linear_parameters;
    linear_parameters.mode = tabletfx::ThruZeroMode::Auto;
    linear_parameters.lfo_rate_hz = 1.3F;
    linear_parameters.lfo_depth = 8.0F;
    linear_parameters.vco_response = tabletfx::FlangerVcoResponse::Linear;
    tabletfx::ManualThruZeroFlangerParameters neutral_parameters = linear_parameters;
    neutral_parameters.vco_response = tabletfx::FlangerVcoResponse::Exponential;
    neutral_parameters.vco_exponent = 1.0F;
    tabletfx::ManualThruZeroFlanger linear(48000.0F, 1, linear_parameters);
    tabletfx::ManualThruZeroFlanger neutral(48000.0F, 1, neutral_parameters);
    std::array<float, 2048> source{};
    for (std::size_t frame = 0; frame < source.size(); ++frame) {
        source[frame] = 0.3F * std::sin(
            6.28318530717958647692F * 317.0F * static_cast<float>(frame) / 48000.0F);
    }
    auto linear_output = source;
    auto neutral_output = source;
    linear.process(linear_output, 1);
    neutral.process(neutral_output, 1);
    require(linear_output == neutral_output,
            "Exponential Exponent=1.0 must preserve the linear samples exactly");
}

void test_off_auto_and_lfo_waveforms() {
    tabletfx::ManualThruZeroFlangerParameters parameters;
    parameters.minimum_time_ms = 2.0F;
    parameters.maximum_time_ms = 6.0F;
    parameters.manual = 5.0F;
    parameters.dry_delay_position = 0.5F;
    parameters.phase_reverse = tabletfx::FlangerPhaseReverse::None;
    parameters.mix = 0.5F;
    parameters.smoothing_ms = 0.0F;

    parameters.mode = tabletfx::ThruZeroMode::Off;
    tabletfx::ManualThruZeroFlanger normal_flanger(8000.0F, 1, parameters);
    std::array<float, 96> immediate_dry{};
    immediate_dry[0] = 1.0F;
    normal_flanger.process(immediate_dry, 1);
    require(std::abs(immediate_dry[0] - 0.5F) < 1.0e-6F &&
                std::abs(immediate_dry[16]) < 1.0e-6F &&
                std::abs(immediate_dry[32] - 0.5F) < 0.03F,
            "Off mode should use an immediate dry path and retain the wet flanger delay; got " +
                std::to_string(immediate_dry[0]) + "/" +
                std::to_string(immediate_dry[16]) + "/" +
                std::to_string(immediate_dry[32]));

    parameters.mode = tabletfx::ThruZeroMode::Auto;
    parameters.lfo_rate_hz = 1.0F;
    parameters.lfo_depth = 10.0F;
    parameters.mix = 1.0F;
    parameters.phase_reverse = tabletfx::FlangerPhaseReverse::Both;
    tabletfx::ManualThruZeroFlanger auto_sine(8000.0F, 1, parameters);
    parameters.mode = tabletfx::ThruZeroMode::Manual;
    tabletfx::ManualThruZeroFlanger manual(8000.0F, 1, parameters);
    std::array<float, 8192> source{};
    for (std::size_t frame = 0; frame < source.size(); ++frame) {
        source[frame] = 0.4F * std::sin(
            6.28318530717958647692F * 713.0F * static_cast<float>(frame) / 8000.0F);
    }
    auto auto_output = source;
    auto manual_output = source;
    auto_sine.process(auto_output, 1);
    manual.process(manual_output, 1);
    const auto sine_output = auto_output;
    double auto_manual_difference = 0.0;
    for (std::size_t frame = 0; frame < source.size(); ++frame) {
        const double difference = auto_output[frame] - manual_output[frame];
        auto_manual_difference += difference * difference;
    }
    require(auto_manual_difference > 1.0,
            "Auto mode must sweep the wet delay across the manual position");

    parameters.lfo_waveform = tabletfx::ThruZeroLfoWaveform::Triangle;
    parameters.mode = tabletfx::ThruZeroMode::Auto;
    tabletfx::ManualThruZeroFlanger auto_triangle(8000.0F, 1, parameters);
    auto_output = source;
    auto_triangle.process(auto_output, 1);
    double waveform_difference = 0.0;
    for (std::size_t frame = 0; frame < source.size(); ++frame) {
        const double difference = auto_output[frame] - sine_output[frame];
        waveform_difference += difference * difference;
    }
    require(waveform_difference > 1.0,
            "Sine and Triangle waveforms should produce distinct Auto sweeps");

    tabletfx::ManualThruZeroFlangerParameters stereo_parameters = parameters;
    stereo_parameters.lfo_waveform = tabletfx::ThruZeroLfoWaveform::Sine;
    stereo_parameters.lfo_phase_degrees = 0.0F;
    stereo_parameters.vco_response = tabletfx::FlangerVcoResponse::Exponential;
    stereo_parameters.vco_exponent = 2.0F;
    tabletfx::ManualThruZeroFlanger in_phase(8000.0F, 2, stereo_parameters);
    stereo_parameters.lfo_phase_degrees = 180.0F;
    tabletfx::ManualThruZeroFlanger opposite_phase(8000.0F, 2, stereo_parameters);
    std::array<float, 8192> stereo_source{};
    for (std::size_t frame = 0; frame < stereo_source.size() / 2; ++frame) {
        const float sample = 0.4F * std::sin(
            6.28318530717958647692F * 713.0F * static_cast<float>(frame) / 8000.0F);
        stereo_source[frame * 2] = sample;
        stereo_source[frame * 2 + 1] = sample;
    }
    auto in_phase_output = stereo_source;
    auto opposite_phase_output = stereo_source;
    in_phase.process(in_phase_output, 2);
    opposite_phase.process(opposite_phase_output, 2);
    double stereo_phase_difference = 0.0;
    for (std::size_t sample = 0; sample < stereo_source.size(); sample += 2) {
        const double difference =
            (in_phase_output[sample] - in_phase_output[sample + 1]) -
            (opposite_phase_output[sample] - opposite_phase_output[sample + 1]);
        stereo_phase_difference += difference * difference;
    }
    require(stereo_phase_difference > 0.1,
            "LFO phase should alter the relative stereo sweep");
}

void test_manual_sweep_crosses_delayed_dry_path() {
    tabletfx::ManualThruZeroFlangerParameters parameters;
    parameters.minimum_time_ms = 2.0F;
    parameters.maximum_time_ms = 6.0F;
    parameters.manual = 0.0F;
    parameters.dry_delay_position = 0.5F;
    parameters.feedback_percent = 0.0F;
    parameters.phase_reverse = tabletfx::FlangerPhaseReverse::Both;
    parameters.mix = 0.5F;
    parameters.smoothing_ms = 0.0F;

    tabletfx::ManualThruZeroFlanger wet_leads(8000.0F, 1, parameters);
    std::array<float, 96> lead_impulse{};
    lead_impulse[0] = 1.0F;
    wet_leads.process(lead_impulse, 1);
    require(std::abs(lead_impulse[16] + 0.5F) < 1.0e-6F &&
                std::abs(lead_impulse[32] - 0.5F) < 1.0e-6F,
            "Manual endpoint should put the wet tap before the delayed dry tap");

    parameters.manual = 10.0F;
    tabletfx::ManualThruZeroFlanger wet_trails(8000.0F, 1, parameters);
    std::array<float, 96> trail_impulse{};
    trail_impulse[0] = 1.0F;
    wet_trails.process(trail_impulse, 1);
    require(std::abs(trail_impulse[32] - 0.5F) < 1.0e-6F &&
                std::abs(trail_impulse[48] + 0.5F) < 1.0e-6F,
            "Manual endpoint should put the wet tap after the delayed dry tap");
}

void test_zero_point_cancellation_and_phase_choices() {
    tabletfx::ManualThruZeroFlangerParameters parameters;
    parameters.minimum_time_ms = 2.0F;
    parameters.maximum_time_ms = 6.0F;
    parameters.manual = 5.0F;
    parameters.dry_delay_position = 0.5F;
    parameters.feedback_percent = 0.0F;
    parameters.phase_reverse = tabletfx::FlangerPhaseReverse::Both;
    parameters.mix = 0.5F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::ManualThruZeroFlanger cancelling(8000.0F, 1, parameters);
    std::array<float, 256> steady{};
    steady.fill(0.25F);
    cancelling.process(steady, 1);
    require(std::all_of(steady.begin() + 64, steady.end(),
                        [](float value) { return std::abs(value) < 1.0e-7F; }),
            "matched delayed paths with opposite phase should cancel at the zero point");

    parameters.phase_reverse = tabletfx::FlangerPhaseReverse::None;
    tabletfx::ManualThruZeroFlanger reinforcing(8000.0F, 1, parameters);
    steady.fill(0.25F);
    reinforcing.process(steady, 1);
    require(std::all_of(steady.begin() + 64, steady.end(),
                        [](float value) { return std::abs(value - 0.25F) < 1.0e-7F; }),
            "matched delayed paths without phase reversal should reinforce");
}

void test_feedback_is_bounded_and_polarity_controlled() {
    for (const float polarity : {100.0F, -100.0F}) {
        tabletfx::ManualThruZeroFlangerParameters parameters;
        parameters.minimum_time_ms = 2.0F;
        parameters.maximum_time_ms = 6.0F;
        parameters.manual = 0.0F;
        parameters.dry_delay_position = 0.5F;
        parameters.feedback_percent = polarity;
        parameters.phase_reverse = tabletfx::FlangerPhaseReverse::None;
        parameters.mix = 1.0F;
        parameters.smoothing_ms = 0.0F;
        tabletfx::ManualThruZeroFlanger flanger(8000.0F, 1, parameters);
        std::array<float, 80> impulse{};
        impulse[0] = 1.0F;
        flanger.process(impulse, 1);
        const float expected_repeat = polarity > 0.0F ? 0.9F : -0.9F;
        require(std::abs(impulse[16] - 1.0F) < 1.0e-6F &&
                    std::abs(impulse[32] - expected_repeat) < 1.0e-6F,
                "feedback polarity or project stability cap changed");
    }
}

void test_stereo_isolation_smoothing_and_reset() {
    tabletfx::ManualThruZeroFlanger whole(48000.0F, 2);
    tabletfx::ManualThruZeroFlanger chunks(48000.0F, 2);
    std::vector<float> input(8192);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = 0.3F * std::sin(2.0F * 3.14159265358979323846F *
                                   330.0F * static_cast<float>(i / 2) / 48000.0F);
    }
    auto whole_output = input;
    auto chunks_output = input;
    whole.process(whole_output, 2);
    for (std::size_t offset = 0; offset < chunks_output.size();) {
        const std::size_t count = std::min<std::size_t>(318, chunks_output.size() - offset);
        chunks.process(std::span<float>(chunks_output.data() + offset, count), 2);
        offset += count;
    }
    require(whole_output == chunks_output,
            "manual through-zero output changed with block partitioning");

    tabletfx::ManualThruZeroFlangerParameters changed;
    changed.minimum_time_ms = 1.0F;
    changed.maximum_time_ms = 12.0F;
    changed.manual = 2.0F;
    changed.dry_delay_position = 0.8F;
    changed.feedback_percent = -55.0F;
    changed.phase_reverse = tabletfx::FlangerPhaseReverse::Right;
    changed.mode = tabletfx::ThruZeroMode::Auto;
    changed.lfo_rate_hz = 2.0F;
    changed.lfo_depth = 7.0F;
    changed.lfo_waveform = tabletfx::ThruZeroLfoWaveform::Triangle;
    changed.lfo_phase_degrees = 95.0F;
    changed.mix = 0.8F;
    whole.set_parameters(changed);
    chunks.set_parameters(changed);
    std::array<float, 128> change_a{};
    change_a.fill(0.2F);
    auto change_b = change_a;
    whole.process(change_a, 2);
    chunks.process(change_b, 2);
    require(change_a == change_b,
            "smoothed parameter changes differ across block partitions");

    whole.reset();
    tabletfx::ManualThruZeroFlanger fresh(48000.0F, 2, changed);
    std::array<float, 512> after_reset{};
    after_reset[0] = 1.0F;
    auto from_fresh = after_reset;
    whole.process(after_reset, 2);
    fresh.process(from_fresh, 2);
    require(after_reset == from_fresh,
            "reset manual through-zero state differs from a fresh instance");

    tabletfx::ManualThruZeroFlanger stereo(8000.0F, 2);
    std::array<float, 96> left_only{};
    left_only[0] = 1.0F;
    stereo.process(left_only, 2);
    for (std::size_t i = 1; i < left_only.size(); i += 2) {
        require(std::abs(left_only[i]) < 1.0e-7F,
                "manual through-zero left input leaked to the right channel");
    }
}

void test_finite_recovery_layout_and_no_process_allocations() {
    tabletfx::ManualThruZeroFlanger mono(48000.0F, 1);
    std::array<float, 5> invalid{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.25F, -0.5F};
    mono.process(invalid, 1);
    require(std::all_of(invalid.begin(), invalid.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "manual through-zero emitted non-finite output");

    std::array<float, 4> wrong_layout{0.1F, -0.2F, 0.3F, -0.4F};
    const auto before = wrong_layout;
    mono.process(wrong_layout, 2);
    require(wrong_layout == before,
            "manual through-zero modified a mismatched channel layout");

    std::array<float, 512> block{};
    block.fill(0.25F);
    tabletfx::ManualThruZeroFlangerParameters auto_parameters;
    auto_parameters.mode = tabletfx::ThruZeroMode::Auto;
    auto_parameters.vco_response = tabletfx::FlangerVcoResponse::Exponential;
    auto_parameters.vco_exponent = 2.0F;
    tabletfx::ManualThruZeroFlanger auto_flanger(48000.0F, 1, auto_parameters);
    const std::size_t before_allocations = allocation_count.load(std::memory_order_relaxed);
    for (int iteration = 0; iteration < 100; ++iteration) {
        mono.process(block, 1);
    }
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto_flanger.process(block, 1);
    }
    const std::size_t after_allocations = allocation_count.load(std::memory_order_relaxed);
    require(before_allocations == after_allocations,
            "manual through-zero process() allocated memory");
}

void test_invalid_construction() {
    bool bad_rate = false;
    try { tabletfx::ManualThruZeroFlanger invalid(100.0F, 1); }
    catch (const std::invalid_argument&) { bad_rate = true; }
    require(bad_rate, "manual through-zero accepted an unsupported sample rate");

    bool bad_channels = false;
    try { tabletfx::ManualThruZeroFlanger invalid(48000.0F, 3); }
    catch (const std::invalid_argument&) { bad_channels = true; }
    require(bad_channels, "manual through-zero accepted an unsupported channel count");
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
        test_documented_controls_and_project_bounds();
        test_vco_response_curves_and_neutral_exponent();
        test_off_auto_and_lfo_waveforms();
        test_manual_sweep_crosses_delayed_dry_path();
        test_zero_point_cancellation_and_phase_choices();
        test_feedback_is_bounded_and_polarity_controlled();
        test_stereo_isolation_smoothing_and_reset();
        test_finite_recovery_layout_and_no_process_allocations();
        test_invalid_construction();
        std::cout << "All TabletFX manual through-zero flanger tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
