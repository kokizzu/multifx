#include "tabletfx/effects.hpp"
#include "tabletfx/wav.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    float gain_db = 0.0F;
    float gate_threshold_db = -120.0F;
    float drive = 1.0F;
    float delay_ms = 0.0F;
    float delay_feedback = 0.25F;
    float delay_mix = 0.0F;
};

float parse_float(std::string_view name, const char* raw) {
    std::size_t parsed = 0;
    const std::string text(raw);
    const float value = std::stof(text, &parsed);
    if (parsed != text.size() || !std::isfinite(value)) {
        throw std::invalid_argument("Invalid numeric argument for " + std::string(name) + ": " + text);
    }
    return value;
}

Options parse_options(int argc, char** argv, int first) {
    Options options;
    for (int i = first; i < argc; ++i) {
        const std::string_view key(argv[i]);
        if (i + 1 >= argc) {
            throw std::invalid_argument("Missing value for " + std::string(key));
        }
        const float value = parse_float(key, argv[++i]);
        if (key == "--gain-db") options.gain_db = value;
        else if (key == "--gate-threshold-db") options.gate_threshold_db = value;
        else if (key == "--drive") options.drive = value;
        else if (key == "--delay-ms") options.delay_ms = value;
        else if (key == "--delay-feedback") options.delay_feedback = value;
        else if (key == "--delay-mix") options.delay_mix = value;
        else throw std::invalid_argument("Unknown option: " + std::string(key));
    }
    return options;
}

std::vector<std::unique_ptr<tabletfx::Effect>> make_effects(const Options& options,
                                                            std::uint32_t sample_rate,
                                                            std::uint16_t channels) {
    std::vector<std::unique_ptr<tabletfx::Effect>> effects;
    if (std::abs(options.gain_db) > 0.0001F) {
        effects.push_back(std::make_unique<tabletfx::Gain>(options.gain_db));
    }
    if (options.gate_threshold_db > -119.0F) {
        effects.push_back(std::make_unique<tabletfx::NoiseGate>(
            static_cast<float>(sample_rate), options.gate_threshold_db));
    }
    if (options.drive > 1.0001F) {
        effects.push_back(std::make_unique<tabletfx::SoftDrive>(options.drive));
    }
    if (options.delay_ms > 0.0F && options.delay_mix > 0.0F) {
        effects.push_back(std::make_unique<tabletfx::Delay>(
            static_cast<float>(sample_rate), channels, 5000.0F, options.delay_ms,
            options.delay_feedback, options.delay_mix));
    }
    return effects;
}

void process_in_blocks(tabletfx::AudioBuffer& audio,
                       std::vector<std::unique_ptr<tabletfx::Effect>>& effects,
                       std::size_t block_frames) {
    const std::size_t channels = audio.channels;
    for (std::size_t offset = 0; offset < audio.frames(); offset += block_frames) {
        const std::size_t frames = std::min(block_frames, audio.frames() - offset);
        std::span<float> block(audio.interleaved.data() + offset * channels, frames * channels);
        for (const auto& effect : effects) {
            effect->process(block, audio.channels);
        }
    }
}

struct Metrics {
    double peak = 0.0;
    double rms = 0.0;
};

Metrics metrics(std::span<const float> samples) {
    Metrics result;
    double sum_squares = 0.0;
    for (float sample : samples) {
        const double value = static_cast<double>(sample);
        result.peak = std::max(result.peak, std::abs(value));
        sum_squares += value * value;
    }
    if (!samples.empty()) {
        result.rms = std::sqrt(sum_squares / static_cast<double>(samples.size()));
    }
    return result;
}

double percentile(std::vector<double>& values, double fraction) {
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(values.size() - 1)));
    return values[index];
}

template <typename Factory>
void benchmark_one(std::string_view name, Factory make_effect, std::uint32_t sample_rate,
                   std::uint16_t channels, std::size_t block_frames,
                   std::size_t warmup_blocks, std::size_t measured_blocks) {
    auto effect = make_effect();
    std::vector<float> buffer(block_frames * channels);
    std::uint32_t noise = 0x1234567U;
    for (float& sample : buffer) {
        noise = noise * 1664525U + 1013904223U;
        sample = static_cast<float>((noise >> 8U) & 0xffffU) / 32768.0F - 1.0F;
    }
    const std::vector<float> source_buffer = buffer;
    for (std::size_t i = 0; i < warmup_blocks; ++i) {
        std::copy(source_buffer.begin(), source_buffer.end(), buffer.begin());
        effect->process(buffer, channels);
    }
    effect->reset();
    std::vector<double> percentages;
    percentages.reserve(measured_blocks);
    const double deadline_us = static_cast<double>(block_frames) * 1'000'000.0 / sample_rate;
    for (std::size_t i = 0; i < measured_blocks; ++i) {
        std::copy(source_buffer.begin(), source_buffer.end(), buffer.begin());
        const auto start = Clock::now();
        effect->process(buffer, channels);
        const auto end = Clock::now();
        const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        percentages.push_back(100.0 * elapsed_us / deadline_us);
    }
    const Metrics signal = metrics(buffer);
    std::cout << name << "," << std::fixed << std::setprecision(4)
              << percentile(percentages, 0.50) << ","
              << percentile(percentages, 0.95) << ","
              << percentile(percentages, 0.99) << ","
              << *std::max_element(percentages.begin(), percentages.end()) << ","
              << signal.rms << '\n';
}

void run_bench() {
    constexpr std::uint32_t sample_rate = 48000;
    constexpr std::uint16_t channels = 2;
    constexpr std::size_t block_frames = 128;
    constexpr std::size_t warmup_blocks = 2000;
    constexpr std::size_t measured_blocks = 5000;
    std::cout << "# build=" << TABLETFX_BUILD_TYPE << " sample_rate_hz=" << sample_rate
              << " block_frames=" << block_frames << " channels=" << channels
              << " warmup_blocks=" << warmup_blocks << " measured_blocks=" << measured_blocks
              << " metric=elapsed_block_us/callback_deadline_us*100\n";
    std::cout << "processor,median_pct,p95_pct,p99_pct,max_pct,output_rms\n";
    benchmark_one("gain", [] { return std::make_unique<tabletfx::Gain>(6.0F); },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("noise-gate", [=] { return std::make_unique<tabletfx::NoiseGate>(sample_rate, -48.0F); },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("soft-drive", [] { return std::make_unique<tabletfx::SoftDrive>(4.0F); },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay", [=] { return std::make_unique<tabletfx::Delay>(sample_rate, channels, 2000.0F); },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
}

void print_usage() {
    std::cerr << "Usage:\n"
              << "  tabletfx_offline render INPUT.wav OUTPUT.wav [--gain-db dB] [--gate-threshold-db dB]"
                 " [--drive 1..20] [--delay-ms ms] [--delay-feedback 0..0.95] [--delay-mix 0..1]\n"
              << "  tabletfx_offline --bench\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--bench") {
            run_bench();
            return 0;
        }
        if (argc < 4 || std::string_view(argv[1]) != "render") {
            print_usage();
            return 2;
        }
        const Options options = parse_options(argc, argv, 4);
        tabletfx::AudioBuffer input = tabletfx::read_wav(argv[2]);
        const Metrics input_metrics = metrics(input.interleaved);
        auto effects = make_effects(options, input.sample_rate, input.channels);
        process_in_blocks(input, effects, 256);
        const Metrics output_metrics = metrics(input.interleaved);
        tabletfx::write_wav_float32(argv[3], input);
        std::cout << "rendered frames=" << input.frames() << " sample_rate_hz=" << input.sample_rate
                  << " channels=" << input.channels << " processors=" << effects.size()
                  << " input_peak=" << input_metrics.peak << " input_rms=" << input_metrics.rms
                  << " output_peak=" << output_metrics.peak << " output_rms=" << output_metrics.rms
                  << " output=" << argv[3] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "tabletfx_offline: " << error.what() << '\n';
        return 1;
    }
}
