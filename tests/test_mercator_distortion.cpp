#include "tabletfx/effects/mercator_distortion.hpp"

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

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> guitar_signal(std::size_t frames) {
    std::vector<float> samples(frames);
    constexpr float sample_rate = 48000.0F;
    constexpr float two_pi = 6.28318530717958647692F;
    for (std::size_t i = 0; i < frames; ++i) {
        const float time = static_cast<float>(i) / sample_rate;
        samples[i] = 0.14F * std::sin(two_pi * 110.0F * time) +
                     0.10F * std::sin(two_pi * 660.0F * time) +
                     0.06F * std::sin(two_pi * 4200.0F * time);
    }
    return samples;
}

double difference(const std::vector<float>& left,
                  const std::vector<float>& right) {
    require(left.size() == right.size(), "test vectors have different sizes");
    double sum = 0.0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        sum += std::abs(static_cast<double>(left[i] - right[i]));
    }
    return sum;
}

double rms(const std::vector<float>& samples, std::size_t skip = 0U) {
    double sum = 0.0;
    for (std::size_t i = skip; i < samples.size(); ++i) {
        const double value = samples[i];
        sum += value * value;
    }
    return std::sqrt(sum / static_cast<double>(samples.size() - skip));
}

std::vector<float> render(tabletfx::MercatorDistParameters parameters,
                          std::vector<float> signal) {
    parameters.smoothing_ms = 0.0F;
    tabletfx::MercatorDistortion effect(48000.0F, parameters);
    effect.process(signal, 1U);
    return signal;
}

void test_defaults_bounds_and_memory() {
    tabletfx::MercatorDistortion effect;
    const auto defaults = effect.parameters();
    require(effect.id() == "mercator-distortion",
            "Mercator distortion stable ID changed");
    require(defaults.drive == 0.45F && defaults.drive_2 == 0.45F &&
                defaults.bass_db == 0.0F && defaults.mid_db == 0.0F &&
                defaults.treble_db == 0.0F && !defaults.bright_1 &&
                !defaults.bright_2 && defaults.level_db == 0.0F &&
                defaults.smoothing_ms == 10.0F,
            "project-authored Mercator defaults changed");
    require(effect.memory_bytes() == sizeof(effect),
            "Mercator memory report is inaccurate");

    tabletfx::MercatorDistParameters p;
    p.drive = -1.0F;
    p.drive_2 = 2.0F;
    p.bass_db = -24.0F;
    p.mid_db = 24.0F;
    p.treble_db = std::numeric_limits<float>::quiet_NaN();
    p.level_db = 100.0F;
    p.smoothing_ms = -1.0F;
    effect.set_parameters(p);
    const auto bounded = effect.parameters();
    require(bounded.drive == 0.0F && bounded.drive_2 == 1.0F &&
                bounded.bass_db == -12.0F && bounded.mid_db == 12.0F &&
                bounded.treble_db == 0.0F && bounded.level_db == 12.0F &&
                bounded.smoothing_ms == 0.0F,
            "project-authored bounds or non-finite fallbacks failed");
}

void test_drive_bright_and_tone_controls_change_signal() {
    const auto source = guitar_signal(16000U);
    tabletfx::MercatorDistParameters base;
    base.smoothing_ms = 0.0F;
    const auto baseline = render(base, source);

    auto drive = base;
    drive.drive = 0.0F;
    require(difference(baseline, render(drive, source)) > 2.0,
            "Drive did not change the interstage distortion");

    auto drive_2 = base;
    drive_2.drive_2 = 0.0F;
    require(difference(baseline, render(drive_2, source)) > 2.0,
            "Drive 2 did not change the interstage distortion");

    auto bright_1 = base;
    bright_1.bright_1 = true;
    require(difference(baseline, render(bright_1, source)) > 0.5,
            "Bright 1 did not affect the signal after stage one");

    auto bright_2 = base;
    bright_2.bright_2 = true;
    require(difference(baseline, render(bright_2, source)) > 0.5,
            "Bright 2 did not affect the signal after stage two");

    auto bass = base;
    bass.bass_db = 6.0F;
    require(difference(baseline, render(bass, source)) > 1.0,
            "Bass control did not affect the low band");

    auto mid = base;
    mid.mid_db = 6.0F;
    require(difference(baseline, render(mid, source)) > 1.0,
            "Mid control did not affect the middle band");

    auto treble = base;
    treble.treble_db = 6.0F;
    require(difference(baseline, render(treble, source)) > 1.0,
            "Treble control did not affect the high band");

    auto level = base;
    level.level_db = 6.0F;
    const auto leveled = render(level, source);
    require(difference(baseline, leveled) > 1.0,
            "Level control did not change output level");
    require(rms(leveled, 1000U) > rms(baseline, 1000U),
            "positive Level did not increase output RMS");
}

void test_stereo_isolation_partial_frame_and_invalid_channels() {
    tabletfx::MercatorDistParameters p;
    p.bright_1 = true;
    p.mid_db = 3.0F;
    p.smoothing_ms = 0.0F;

    tabletfx::MercatorDistortion stereo(48000.0F, p);
    tabletfx::MercatorDistortion left_effect(48000.0F, p);
    tabletfx::MercatorDistortion right_effect(48000.0F, p);
    auto left = guitar_signal(1024U);
    auto right = guitar_signal(1024U);
    for (std::size_t i = 0; i < right.size(); ++i) right[i] *= -0.5F;
    auto interleaved = std::vector<float>(left.size() * 2U);
    for (std::size_t i = 0; i < left.size(); ++i) {
        interleaved[2U * i] = left[i];
        interleaved[2U * i + 1U] = right[i];
    }
    left_effect.process(left, 1U);
    right_effect.process(right, 1U);
    stereo.process(interleaved, 2U);
    for (std::size_t i = 0; i < left.size(); ++i) {
        require(std::abs(interleaved[2U * i] - left[i]) < 1.0e-6F &&
                    std::abs(interleaved[2U * i + 1U] - right[i]) < 1.0e-6F,
                "stereo channels did not maintain independent histories");
    }

    std::array<float, 3> partial{0.1F, -0.2F, 0.3F};
    stereo.process(partial, 2U);
    for (float value : partial) {
        require(std::isfinite(value), "partial final frame was not processed safely");
    }
    const auto unchanged = partial;
    stereo.process(partial, 3U);
    require(partial == unchanged, "unsupported channel count modified audio");
}

void test_smoothing_reset_finite_recovery_and_no_allocations() {
    tabletfx::MercatorDistParameters start;
    start.drive = 0.0F;
    start.drive_2 = 0.0F;
    start.smoothing_ms = 1.0F;
    tabletfx::MercatorDistortion smoothed(8000.0F, start);

    auto target = start;
    target.drive = 1.0F;
    target.drive_2 = 1.0F;
    auto immediate_parameters = target;
    immediate_parameters.smoothing_ms = 0.0F;
    tabletfx::MercatorDistortion immediate(8000.0F, immediate_parameters);
    tabletfx::MercatorDistortion baseline(8000.0F, start);
    smoothed.set_parameters(target);

    std::array<float, 1> first{0.4F};
    std::array<float, 1> immediate_sample{0.4F};
    std::array<float, 1> baseline_sample{0.4F};
    smoothed.process(first, 1U);
    immediate.process(immediate_sample, 1U);
    baseline.process(baseline_sample, 1U);
    require(std::abs(first[0] - baseline_sample[0]) > 1.0e-6F &&
                std::abs(first[0] - baseline_sample[0]) <
                    std::abs(immediate_sample[0] - baseline_sample[0]),
            "parameter update did not ramp toward its target");

    std::array<float, 8> audio{0.2F, -0.2F, 0.4F, -0.4F,
                               0.6F, -0.6F, 0.8F, -0.8F};
    audio[3] = std::numeric_limits<float>::quiet_NaN();
    allocation_count = 0U;
    count_allocations = true;
    smoothed.process(audio, 2U);
    count_allocations = false;
    require(allocation_count == 0U,
            "Mercator processing allocated memory");
    for (float sample : audio) {
        require(std::isfinite(sample),
                "Mercator output contains a non-finite sample");
    }

    auto source = guitar_signal(2048U);
    auto after = source;
    tabletfx::MercatorDistortion configured(48000.0F, target);
    configured.process(source, 1U);
    configured.reset();
    auto fresh = guitar_signal(2048U);
    tabletfx::MercatorDistortion fresh_effect(48000.0F, target);
    fresh_effect.process(fresh, 1U);
    require(source == fresh,
            "reset did not restore fresh filter state and targets");

    configured.configure(0.0F, start);
    configured.process(after, 1U);
    for (float sample : after) {
        require(std::isfinite(sample),
                "invalid sample rate produced non-finite audio");
    }
}

} // namespace

void* operator new(std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (count_allocations.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1U, std::memory_order_relaxed);
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
        test_defaults_bounds_and_memory();
        test_drive_bright_and_tone_controls_change_signal();
        test_stereo_isolation_partial_frame_and_invalid_channels();
        test_smoothing_reset_finite_recovery_and_no_allocations();
        std::cout << "Mercator distortion tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        count_allocations = false;
        std::cerr << "Mercator distortion test failure: "
                  << exception.what() << '\n';
        return 1;
    }
}
