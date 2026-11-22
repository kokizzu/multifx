#include "tabletfx/effects/auto_pan.hpp"

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

std::atomic<bool> count_allocations{false};
std::atomic<std::size_t> allocation_count{0};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float actual, float expected, float tolerance = 2.0e-5F) {
    return std::abs(actual - expected) <= tolerance;
}

void test_defaults_and_project_bounds() {
    tabletfx::AutoPan panner;
    const auto p = panner.parameters();
    require(panner.id() == "auto-pan", "auto-pan stable ID changed");
    require(p.rate_hz == 0.5F && p.depth == 0.75F &&
                p.waveform == tabletfx::AutoPanWaveform::Sine &&
                !p.tempo_sync && p.tempo_bpm == 120.0F &&
                p.division == tabletfx::AutoPanDivision::Quarter &&
                p.depth_smoothing_ms == 10.0F &&
                near(panner.effective_rate_hz(), 0.5F),
            "project auto-pan defaults changed");
    require(panner.memory_bytes() == sizeof(panner),
            "auto-pan memory report is inaccurate");

    panner.set_rate_hz(-1.0F);
    require(panner.parameters().rate_hz == 0.05F,
            "minimum project-authored rate bound failed");
    panner.set_rate_hz(100.0F);
    require(panner.parameters().rate_hz == 20.0F,
            "maximum project-authored rate bound failed");
    panner.set_rate_hz(std::numeric_limits<float>::quiet_NaN());
    require(panner.parameters().rate_hz == 20.0F,
            "non-finite rate changed the last valid value");

    panner.set_depth(-1.0F);
    require(panner.parameters().depth == 0.0F,
            "negative depth was not bounded");
    panner.set_depth(2.0F);
    require(panner.parameters().depth == 1.0F,
            "depth above unity was not bounded");
    panner.set_depth(std::numeric_limits<float>::infinity());
    require(panner.parameters().depth == 1.0F,
            "non-finite depth changed the last valid value");
}

void test_stereo_pan_shapes_and_depth_endpoints() {
    tabletfx::AutoPanParameters p;
    p.rate_hz = 1.0F;
    p.depth = 1.0F;
    p.depth_smoothing_ms = 0.0F;
    p.waveform = tabletfx::AutoPanWaveform::Square;
    tabletfx::AutoPan square(8000.0F, p);
    std::vector<float> square_audio(8000U * 2U, 0.5F);
    square.process(square_audio, 2);
    require(square_audio[0] > 0.70F && std::abs(square_audio[1]) < 1.0e-5F,
            "square panner did not begin hard-left at full depth");
    const std::size_t half_cycle = 4100U * 2U;
    require(std::abs(square_audio[half_cycle]) < 1.0e-5F &&
                square_audio[half_cycle + 1U] > 0.70F,
            "square panner did not switch hard-right halfway through its cycle");

    p.waveform = tabletfx::AutoPanWaveform::Triangle;
    tabletfx::AutoPan triangle(8000.0F, p);
    std::array<float, 8> tri{0.5F, 0.5F, 0.5F, 0.5F,
                             0.5F, 0.5F, 0.5F, 0.5F};
    triangle.process(tri, 2);
    require(tri[0] > 0.70F && std::abs(tri[1]) < 1.0e-5F,
            "triangle panner did not reach its initial left extreme");
    require(tri[2] < tri[0] && tri[3] > 0.0F,
            "triangle panner did not move smoothly toward center");

    p.waveform = tabletfx::AutoPanWaveform::Sine;
    tabletfx::AutoPan sine(8000.0F, p);
    std::vector<float> sine_audio(8000U * 2U, 0.5F);
    sine.process(sine_audio, 2);
    require(near(sine_audio[0], 0.5F) && near(sine_audio[1], 0.5F),
            "sine panner should start at the centered phase");
    require(std::abs(sine_audio[4000U]) < 1.0e-5F &&
                sine_audio[4001U] > 0.70F,
            "sine panner did not reach the right extreme after a quarter cycle");
    require(near(sine_audio[8000U], 0.5F) && near(sine_audio[8001U], 0.5F),
            "sine panner did not return through center after a half cycle");

    p.depth = 0.0F;
    tabletfx::AutoPan dry(8000.0F, p);
    std::array<float, 8> unchanged{0.25F, -0.5F, 0.75F, -1.0F,
                                   0.125F, -0.25F, 0.625F, 0.375F};
    const auto original = unchanged;
    dry.process(unchanged, 2);
    require(unchanged == original,
            "zero depth should preserve the stereo input exactly");
}

void test_mono_passthrough_and_finite_sanitization() {
    tabletfx::AutoPan panner;
    std::array<float, 5> mono{0.25F, -0.5F,
                              std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(), 12.0F};
    panner.process(mono, 1);
    require(mono[0] == 0.25F && mono[1] == -0.5F && mono[2] == 0.0F &&
                mono[3] == 0.0F && mono[4] == 8.0F,
            "mono pass-through or non-finite sample sanitization failed");
    panner.process(mono, 0);
}

void test_tempo_divisions_and_parameter_sanitization() {
    tabletfx::AutoPan panner;
    panner.set_tempo_sync(true, 120.0F,
                          tabletfx::AutoPanDivision::DottedQuarter);
    require(near(panner.effective_rate_hz(), 120.0F / 60.0F / 1.5F),
            "dotted-quarter tempo conversion was incorrect");
    panner.set_tempo_sync(true, 60.0F,
                          tabletfx::AutoPanDivision::SixteenthTriplet);
    require(near(panner.effective_rate_hz(), 6.0F),
            "sixteenth-triplet tempo conversion was incorrect");
    panner.set_tempo_sync(true, 0.0F,
                          tabletfx::AutoPanDivision::Quarter);
    require(panner.parameters().tempo_bpm == 30.0F &&
                near(panner.effective_rate_hz(), 0.5F),
            "tempo minimum bound or quarter-note conversion failed");
    panner.set_tempo_sync(true, 1000.0F,
                          static_cast<tabletfx::AutoPanDivision>(255));
    const auto bounded = panner.parameters();
    require(bounded.tempo_bpm == 300.0F &&
                bounded.division == tabletfx::AutoPanDivision::Quarter &&
                near(panner.effective_rate_hz(), 5.0F),
            "tempo maximum or invalid division fallback failed");

    panner.set_waveform(static_cast<tabletfx::AutoPanWaveform>(255));
    require(panner.parameters().waveform == tabletfx::AutoPanWaveform::Sine,
            "invalid waveform did not fall back to sine");
}

void test_depth_ramp_reset_and_no_process_allocations() {
    tabletfx::AutoPanParameters p;
    p.rate_hz = 0.5F;
    p.depth = 0.0F;
    p.depth_smoothing_ms = 1.0F;
    tabletfx::AutoPan panner(8000.0F, p);
    panner.set_depth(1.0F);
    require(panner.current_depth() == 0.0F,
            "depth update should start at the current value");
    std::array<float, 8> audio{};
    audio.fill(0.5F);
    allocation_count = 0U;
    count_allocations = true;
    panner.process(audio, 2);
    count_allocations = false;
    require(allocation_count == 0U,
            "auto-pan allocated memory in process()");
    require(panner.current_depth() > 0.0F && panner.current_depth() < 1.0F,
            "depth control ramp did not advance over the processed frames");

    panner.reset();
    require(panner.current_depth() == panner.parameters().depth,
            "reset did not restore configured depth");
    std::array<float, 8> first{};
    first.fill(0.5F);
    panner.process(first, 2);
    panner.reset();
    std::array<float, 8> second{};
    second.fill(0.5F);
    panner.process(second, 2);
    require(first == second,
            "reset did not restore deterministic modulation phase");
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
        test_defaults_and_project_bounds();
        test_stereo_pan_shapes_and_depth_endpoints();
        test_mono_passthrough_and_finite_sanitization();
        test_tempo_divisions_and_parameter_sanitization();
        test_depth_ramp_reset_and_no_process_allocations();
        std::cout << "Auto-Pan tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        count_allocations = false;
        std::cerr << "Auto-Pan test failure: " << exception.what() << '\n';
        return 1;
    }
}
