#include "tabletfx/effects/stereo_panner.hpp"

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

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float actual, float expected, float tolerance = 1.0e-4F) {
    return std::abs(actual - expected) <= tolerance;
}

void test_defaults_and_bounds() {
    tabletfx::StereoPanner panner;
    const auto p = panner.parameters();
    require(panner.id() == "stereo-panner", "stereo panner ID changed");
    require(p.rate_hz == 0.5F && p.width_percent == 100.0F &&
                p.waveform == tabletfx::StereoPannerWaveform::Sine &&
                !p.tempo_sync && p.tempo_bpm == 120.0F &&
                p.division == tabletfx::AutoPanDivision::Quarter &&
                p.duty_cycle_percent == 50.0F && p.shape_percent == 50.0F &&
                p.phase_degrees == 180.0F && p.pan_center_percent == 0.0F &&
                p.ducking_percent == 0.0F && p.level_db == 0.0F &&
                p.balance_percent == 0.0F && p.smoothing_ms == 10.0F,
            "project-authored stereo panner defaults changed");
    require(panner.memory_bytes() == sizeof(panner),
            "stereo panner memory report is inaccurate");

    auto bounded = p;
    bounded.rate_hz = 100.0F;
    bounded.width_percent = 400.0F;
    bounded.duty_cycle_percent = -10.0F;
    bounded.shape_percent = 110.0F;
    bounded.phase_degrees = 720.0F;
    bounded.pan_center_percent = -200.0F;
    bounded.ducking_percent = 200.0F;
    bounded.level_db = 20.0F;
    bounded.balance_percent = 150.0F;
    bounded.smoothing_ms = -5.0F;
    bounded.division = static_cast<tabletfx::AutoPanDivision>(255);
    bounded.waveform = static_cast<tabletfx::StereoPannerWaveform>(255);
    panner.set_parameters(bounded);
    const auto q = panner.parameters();
    require(q.rate_hz == 20.0F && q.width_percent == 200.0F &&
                q.duty_cycle_percent == 0.0F && q.shape_percent == 100.0F &&
                q.phase_degrees == 360.0F && q.pan_center_percent == -100.0F &&
                q.ducking_percent == 100.0F && q.level_db == 12.0F &&
                q.balance_percent == 100.0F && q.smoothing_ms == 0.0F &&
                q.division == tabletfx::AutoPanDivision::Quarter &&
                q.waveform == tabletfx::StereoPannerWaveform::Sine,
            "project-authored controls were not bounded or sanitized");

    bounded.rate_hz = std::numeric_limits<float>::quiet_NaN();
    bounded.width_percent = std::numeric_limits<float>::infinity();
    panner.set_parameters(bounded);
    require(panner.parameters().rate_hz == 0.5F &&
                panner.parameters().width_percent == 100.0F,
            "non-finite controls did not fall back to project defaults");
}

void test_stereo_motion_and_zero_width() {
    tabletfx::StereoPannerParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.width_percent = 100.0F;
    parameters.phase_degrees = 180.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::StereoPanner panner(8000.0F, parameters);
    std::vector<float> audio(8000U * 2U, 0.5F);
    panner.process(audio, 2);
    require(std::abs(audio[0] - 0.5F) < 1.0e-5F &&
                std::abs(audio[1] - 0.5F) < 1.0e-5F,
            "sine pan should start centered");
    const std::size_t right_sweep = 2000U * 2U;
    const std::size_t left_sweep = 6000U * 2U;
    require(audio[right_sweep + 1U] > 0.65F &&
                std::abs(audio[right_sweep]) < 1.0e-4F,
            "180-degree phase did not sweep the centered input right");
    require(audio[left_sweep] > 0.65F &&
                std::abs(audio[left_sweep + 1U]) < 1.0e-4F,
            "180-degree phase did not sweep the centered input left");

    parameters.width_percent = 0.0F;
    tabletfx::StereoPanner neutral(8000.0F, parameters);
    std::array<float, 8> unchanged{0.25F, -0.5F, 0.75F, -1.0F,
                                   0.125F, -0.25F, 0.625F, 0.375F};
    const auto original = unchanged;
    neutral.process(unchanged, 2);
    require(unchanged == original,
            "zero width should preserve stereo input exactly");
}

void test_tempo_duty_waveforms_and_overwidth() {
    tabletfx::StereoPannerParameters parameters;
    parameters.tempo_sync = true;
    parameters.tempo_bpm = 120.0F;
    parameters.division = tabletfx::AutoPanDivision::DottedQuarter;
    parameters.width_percent = 200.0F;
    parameters.waveform = tabletfx::StereoPannerWaveform::Trapezoid;
    parameters.duty_cycle_percent = 25.0F;
    parameters.shape_percent = 75.0F;
    parameters.phase_degrees = 90.0F;
    parameters.pan_center_percent = 25.0F;
    parameters.smoothing_ms = 0.0F;
    tabletfx::StereoPanner panner(48000.0F, parameters);
    require(near(panner.effective_rate_hz(), 120.0F / 60.0F / 1.5F),
            "tempo division conversion was incorrect");

    for (std::uint8_t type = 0; type <= 9; ++type) {
        parameters.waveform = static_cast<tabletfx::StereoPannerWaveform>(type);
        panner.set_parameters(parameters);
        std::array<float, 2048> audio{};
        audio.fill(0.4F);
        panner.process(audio, 2);
        for (float sample : audio) {
            require(std::isfinite(sample) && std::abs(sample) <= 8.0F,
                    "waveform or over-width processing produced invalid output");
        }
    }
}

void test_ducking_and_smoothing() {
    auto run = [](float ducking) {
        tabletfx::StereoPannerParameters parameters;
        parameters.rate_hz = 1.0F;
        parameters.width_percent = 100.0F;
        parameters.ducking_percent = ducking;
        parameters.phase_degrees = 180.0F;
        parameters.smoothing_ms = 0.0F;
        tabletfx::StereoPanner panner(8000.0F, parameters);
        std::vector<float> audio(8000U * 2U, 0.5F);
        panner.process(audio, 2);
        return std::array<float, 2>{audio[6000U * 2U],
                                    audio[6000U * 2U + 1U]};
    };
    const auto unducked = run(0.0F);
    const auto ducked = run(100.0F);
    require(std::abs(ducked[0] - ducked[1]) <
                std::abs(unducked[0] - unducked[1]),
            "strong input did not reduce LFO panning with ducking enabled");

    tabletfx::StereoPannerParameters parameters;
    parameters.width_percent = 0.0F;
    parameters.smoothing_ms = 10.0F;
    tabletfx::StereoPanner panner(8000.0F, parameters);
    parameters.width_percent = 100.0F;
    panner.set_parameters(parameters);
    std::array<float, 8> audio{};
    audio.fill(0.5F);
    panner.process(audio, 2);
    require(audio[0] > 0.49F && audio[0] < 0.51F,
            "width update did not start with a smoothed transition");

    allocation_count = 0U;
    count_allocations = true;
    panner.process(audio, 2);
    count_allocations = false;
    require(allocation_count == 0U,
            "stereo panner allocated memory in process()");
}

void test_envelope_mode_widens_quieter_input() {
    constexpr std::size_t frames = 8000U;
    constexpr std::size_t measurement_start = 4000U;
    tabletfx::StereoPannerParameters parameters;
    parameters.rate_hz = 1.0F;
    parameters.width_percent = 100.0F;
    parameters.waveform = tabletfx::StereoPannerWaveform::Envelope;
    parameters.phase_degrees = 180.0F;
    parameters.ducking_percent = 0.0F;
    parameters.smoothing_ms = 0.0F;

    std::vector<float> quiet(frames * 2U, 0.01F);
    std::vector<float> loud(frames * 2U, 0.5F);
    tabletfx::StereoPanner quiet_panner(8000.0F, parameters);
    tabletfx::StereoPanner loud_panner(8000.0F, parameters);
    quiet_panner.process(quiet, 2U);
    loud_panner.process(loud, 2U);

    const auto normalized_side_rms = [](const std::vector<float>& audio,
                                        float input_level) {
        double sum_squares = 0.0;
        for (std::size_t frame = measurement_start; frame < frames; ++frame) {
            const double side = static_cast<double>(audio[frame * 2U]) -
                                static_cast<double>(audio[frame * 2U + 1U]);
            sum_squares += side * side;
        }
        return std::sqrt(sum_squares /
                         static_cast<double>(frames - measurement_start)) /
               static_cast<double>(input_level);
    };
    const double quiet_motion = normalized_side_rms(quiet, 0.01F);
    const double loud_motion = normalized_side_rms(loud, 0.5F);
    require(quiet_motion > loud_motion * 2.0,
            "Envelope mode should pan quiet input wider than loud input");
    require(quiet_panner.memory_bytes() == sizeof(quiet_panner) &&
                loud_panner.memory_bytes() == sizeof(loud_panner),
            "Envelope mode should keep fixed, accurately reported state");
    require(std::all_of(quiet.begin(), quiet.end(), [](float sample) {
                return std::isfinite(sample);
            }) && std::all_of(loud.begin(), loud.end(), [](float sample) {
                return std::isfinite(sample);
            }),
            "Envelope mode produced non-finite output");

    std::array<float, 256> callback{};
    callback.fill(0.1F);
    allocation_count = 0U;
    count_allocations = true;
    quiet_panner.process(callback, 2U);
    count_allocations = false;
    require(allocation_count == 0U,
            "Envelope mode allocated memory in process()");
}

void test_mono_passthrough_and_finite_sanitization() {
    tabletfx::StereoPanner panner;
    std::array<float, 5> mono{0.25F, -0.5F,
                              std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(), 12.0F};
    panner.process(mono, 1);
    require(mono[0] == 0.25F && mono[1] == -0.5F && mono[2] == 0.0F &&
                mono[3] == 0.0F && mono[4] == 8.0F,
            "mono pass-through or non-finite sanitization failed");
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load()) ++allocation_count;
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    try {
        test_defaults_and_bounds();
        test_stereo_motion_and_zero_width();
        test_tempo_duty_waveforms_and_overwidth();
        test_ducking_and_smoothing();
        test_envelope_mode_widens_quieter_input();
        test_mono_passthrough_and_finite_sanitization();
        std::cout << "Stereo Panner tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        count_allocations = false;
        std::cerr << "Stereo Panner test failure: " << exception.what() << '\n';
        return 1;
    }
}
