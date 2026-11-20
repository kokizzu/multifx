#include "tabletfx/effects.hpp"
#include "tabletfx/effects/clean_boost.hpp"
#include "tabletfx/effects/compressor.hpp"
#include "tabletfx/effects/envelope_wah.hpp"
#include "tabletfx/effects/filters.hpp"
#include "tabletfx/effects/peak_limiter.hpp"
#include "tabletfx/effects/pedal_compressor.hpp"
#include "tabletfx/effects/tremolo.hpp"
#include "tabletfx/effects/three_band_eq.hpp"
#include "tabletfx/effects/volume_pedal.hpp"
#include "tabletfx/effects/wah.hpp"
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
    float output_gain_db = 0.0F;
    bool volume_pedal_enabled = false;
    tabletfx::VolumePedalParameters volume_pedal{};
    bool limiter_enabled = false;
    float limiter_ceiling_db = -1.0F;
    float limiter_release_ms = 80.0F;
    bool wah_enabled = false;
    tabletfx::WahParameters wah{};
    bool envelope_wah_enabled = false;
    tabletfx::EnvelopeWahParameters envelope_wah{};
    bool clean_boost_enabled = false;
    float clean_boost_db = 0.0F;
    bool tremolo_enabled = false;
    tabletfx::TremoloParameters tremolo{};
    float high_pass_hz = 0.0F;
    float low_pass_hz = 0.0F;
    bool noise_gate_enabled = false;
    tabletfx::NoiseGateParameters noise_gate{};
    float drive = 1.0F;
    float delay_ms = 0.0F;
    float delay_feedback = 0.25F;
    float delay_mix = 0.0F;
    bool compressor_enabled = false;
    tabletfx::CompressorParameters compressor{};
    bool pedal_compressor_enabled = false;
    tabletfx::PedalCompressorParameters pedal_compressor{};
    bool eq_enabled = false;
    float eq_band1_frequency_hz = 120.0F;
    float eq_band1_gain_db = 0.0F;
    float eq_band1_q = 0.707F;
    float eq_band2_frequency_hz = 800.0F;
    float eq_band2_gain_db = 0.0F;
    float eq_band2_q = 0.707F;
    float eq_band3_frequency_hz = 5000.0F;
    float eq_band3_gain_db = 0.0F;
    float eq_band3_q = 0.707F;
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
        if (key == "--volume-curve") {
            const std::string_view value(argv[++i]);
            if (value == "linear") {
                options.volume_pedal.taper = tabletfx::VolumePedalTaper::LinearAmplitude;
            } else if (value == "audio") {
                options.volume_pedal.taper = tabletfx::VolumePedalTaper::AudioTaperDb;
            } else {
                throw std::invalid_argument("Volume curve must be 'linear' or 'audio'");
            }
            options.volume_pedal_enabled = true;
            continue;
        }
        if (key == "--tremolo-waveform") {
            const std::string_view value(argv[++i]);
            if (value == "sine") {
                options.tremolo.waveform = tabletfx::TremoloWaveform::Sine;
            } else if (value == "triangle") {
                options.tremolo.waveform = tabletfx::TremoloWaveform::Triangle;
            } else if (value == "square") {
                options.tremolo.waveform = tabletfx::TremoloWaveform::Square;
            } else {
                throw std::invalid_argument("Tremolo waveform must be sine, triangle, or square");
            }
            options.tremolo_enabled = true;
            continue;
        }
        if (key == "--tremolo-sync") {
            const std::string_view value(argv[++i]);
            if (value == "on") {
                options.tremolo.tempo_sync = true;
            } else if (value == "off") {
                options.tremolo.tempo_sync = false;
            } else {
                throw std::invalid_argument("Tremolo sync must be 'on' or 'off'");
            }
            options.tremolo_enabled = true;
            continue;
        }
        if (key == "--touch-wah-direction") {
            const std::string_view value(argv[++i]);
            if (value == "up") {
                options.envelope_wah.direction = tabletfx::EnvelopeWahDirection::Up;
            } else if (value == "down") {
                options.envelope_wah.direction = tabletfx::EnvelopeWahDirection::Down;
            } else {
                throw std::invalid_argument("Touch-wah direction must be 'up' or 'down'");
            }
            options.envelope_wah_enabled = true;
            continue;
        }
        const float value = parse_float(key, argv[++i]);
        if (key == "--gain-db") options.gain_db = value;
        else if (key == "--output-gain-db") options.output_gain_db = value;
        else if (key == "--volume-position") {
            options.volume_pedal.position = value;
            options.volume_pedal_enabled = true;
        }
        else if (key == "--volume-min-db") {
            options.volume_pedal.minimum_db = value;
            options.volume_pedal_enabled = true;
        }
        else if (key == "--volume-max-db") {
            options.volume_pedal.maximum_db = value;
            options.volume_pedal_enabled = true;
        }
        else if (key == "--limiter-ceiling-db") {
            options.limiter_ceiling_db = value;
            options.limiter_enabled = true;
        }
        else if (key == "--limiter-release-ms") {
            options.limiter_release_ms = value;
            options.limiter_enabled = true;
        }
        else if (key == "--wah-position") {
            options.wah.position = value;
            options.wah_enabled = true;
        }
        else if (key == "--wah-min-hz") {
            options.wah.minimum_hz = value;
            options.wah_enabled = true;
        }
        else if (key == "--wah-max-hz") {
            options.wah.maximum_hz = value;
            options.wah_enabled = true;
        }
        else if (key == "--wah-q") {
            options.wah.resonance_q = value;
            options.wah_enabled = true;
        }
        else if (key == "--wah-level-db") {
            options.wah.level_db = value;
            options.wah_enabled = true;
        }
        else if (key == "--touch-wah-sensitivity") {
            options.envelope_wah.sensitivity = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-min-hz") {
            options.envelope_wah.minimum_hz = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-max-hz") {
            options.envelope_wah.maximum_hz = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-q") {
            options.envelope_wah.resonance_q = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-level-db") {
            options.envelope_wah.level_db = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-attack-ms") {
            options.envelope_wah.attack_ms = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-release-ms") {
            options.envelope_wah.release_ms = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--touch-wah-smoothing-ms") {
            options.envelope_wah.smoothing_ms = value;
            options.envelope_wah_enabled = true;
        }
        else if (key == "--tremolo-rate-hz") {
            options.tremolo.rate_hz = value;
            options.tremolo_enabled = true;
        }
        else if (key == "--tremolo-depth") {
            options.tremolo.depth = value;
            options.tremolo_enabled = true;
        }
        else if (key == "--tremolo-tempo-bpm") {
            options.tremolo.tempo_bpm = value;
            options.tremolo_enabled = true;
        }
        else if (key == "--tremolo-beats-per-cycle") {
            options.tremolo.beats_per_cycle = value;
            options.tremolo_enabled = true;
        }
        else if (key == "--high-pass-hz") options.high_pass_hz = value;
        else if (key == "--low-pass-hz") options.low_pass_hz = value;
        else if (key == "--gate-threshold-db") {
            options.noise_gate.threshold_db = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--gate-attenuation-db") {
            options.noise_gate.attenuation_db = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--gate-hysteresis-db") {
            options.noise_gate.hysteresis_db = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--gate-attack-ms") {
            options.noise_gate.attack_ms = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--gate-hold-ms") {
            options.noise_gate.hold_ms = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--gate-release-ms") {
            options.noise_gate.release_ms = value;
            options.noise_gate_enabled = true;
        }
        else if (key == "--drive") options.drive = value;
        else if (key == "--boost-db") {
            options.clean_boost_db = value;
            options.clean_boost_enabled = true;
        }
        else if (key == "--delay-ms") options.delay_ms = value;
        else if (key == "--delay-feedback") options.delay_feedback = value;
        else if (key == "--delay-mix") options.delay_mix = value;
        else if (key == "--compressor-threshold-db") {
            options.compressor.threshold_db = value;
            options.compressor_enabled = true;
        }
        else if (key == "--compressor-ratio") {
            options.compressor.ratio = value;
            options.compressor_enabled = true;
        }
        else if (key == "--compressor-knee-db") {
            options.compressor.knee_db = value;
            options.compressor_enabled = true;
        }
        else if (key == "--compressor-attack-ms") {
            options.compressor.attack_ms = value;
            options.compressor_enabled = true;
        }
        else if (key == "--compressor-release-ms") {
            options.compressor.release_ms = value;
            options.compressor_enabled = true;
        }
        else if (key == "--compressor-makeup-db") {
            options.compressor.makeup_db = value;
            options.compressor_enabled = true;
        }
        else if (key == "--pedal-compressor-sensitivity") {
            options.pedal_compressor.sensitivity = value;
            options.pedal_compressor_enabled = true;
        }
        else if (key == "--pedal-compressor-sustain") {
            options.pedal_compressor.sustain = value;
            options.pedal_compressor_enabled = true;
        }
        else if (key == "--pedal-compressor-attack") {
            options.pedal_compressor.attack = value;
            options.pedal_compressor_enabled = true;
        }
        else if (key == "--pedal-compressor-level-db") {
            options.pedal_compressor.level_db = value;
            options.pedal_compressor_enabled = true;
        }
        else if (key == "--eq-band1-frequency-hz") {
            options.eq_band1_frequency_hz = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band1-gain-db") {
            options.eq_band1_gain_db = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band1-q") {
            options.eq_band1_q = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band2-frequency-hz") {
            options.eq_band2_frequency_hz = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band2-gain-db") {
            options.eq_band2_gain_db = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band2-q") {
            options.eq_band2_q = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band3-frequency-hz") {
            options.eq_band3_frequency_hz = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band3-gain-db") {
            options.eq_band3_gain_db = value;
            options.eq_enabled = true;
        }
        else if (key == "--eq-band3-q") {
            options.eq_band3_q = value;
            options.eq_enabled = true;
        }
        else throw std::invalid_argument("Unknown option: " + std::string(key));
    }
    return options;
}

std::vector<std::unique_ptr<tabletfx::Effect>> make_effects(const Options& options,
                                                            std::uint32_t sample_rate,
                                                            std::uint16_t channels) {
    std::vector<std::unique_ptr<tabletfx::Effect>> effects;
    if (std::abs(options.gain_db) > 0.0001F) {
        effects.push_back(std::make_unique<tabletfx::Gain>(
            options.gain_db, static_cast<float>(sample_rate)));
    }
    if (options.volume_pedal_enabled) {
        effects.push_back(std::make_unique<tabletfx::VolumePedal>(
            static_cast<float>(sample_rate), options.volume_pedal));
    }
    if (options.high_pass_hz > 0.0F) {
        effects.push_back(std::make_unique<tabletfx::HighPassFilter>(
            static_cast<float>(sample_rate), channels, options.high_pass_hz));
    }
    if (options.low_pass_hz > 0.0F) {
        effects.push_back(std::make_unique<tabletfx::LowPassFilter>(
            static_cast<float>(sample_rate), channels, options.low_pass_hz));
    }
    if (options.noise_gate_enabled) {
        effects.push_back(std::make_unique<tabletfx::NoiseGate>(
            static_cast<float>(sample_rate), options.noise_gate));
    }
    if (options.pedal_compressor_enabled) {
        effects.push_back(std::make_unique<tabletfx::PedalCompressor>(
            static_cast<float>(sample_rate), options.pedal_compressor));
    }
    if (options.compressor_enabled) {
        effects.push_back(std::make_unique<tabletfx::Compressor>(
            static_cast<float>(sample_rate), options.compressor));
    }
    if (options.wah_enabled) {
        effects.push_back(std::make_unique<tabletfx::Wah>(
            static_cast<float>(sample_rate), channels, options.wah));
    }
    if (options.envelope_wah_enabled) {
        effects.push_back(std::make_unique<tabletfx::EnvelopeWah>(
            static_cast<float>(sample_rate), channels, options.envelope_wah));
    }
    if (options.clean_boost_enabled) {
        effects.push_back(std::make_unique<tabletfx::CleanBoost>(
            options.clean_boost_db, static_cast<float>(sample_rate)));
    }
    if (options.drive > 1.0001F) {
        effects.push_back(std::make_unique<tabletfx::SoftDrive>(options.drive));
    }
    if (options.tremolo_enabled) {
        effects.push_back(std::make_unique<tabletfx::Tremolo>(
            static_cast<float>(sample_rate), options.tremolo));
    }
    if (options.eq_enabled) {
        auto eq = std::make_unique<tabletfx::ThreeBandParametricEQ>(
            static_cast<float>(sample_rate), channels);
        eq->configure(options.eq_band1_frequency_hz, options.eq_band1_gain_db, options.eq_band1_q,
                      options.eq_band2_frequency_hz, options.eq_band2_gain_db, options.eq_band2_q,
                      options.eq_band3_frequency_hz, options.eq_band3_gain_db, options.eq_band3_q);
        effects.push_back(std::move(eq));
    }
    if (options.delay_ms > 0.0F && options.delay_mix > 0.0F) {
        effects.push_back(std::make_unique<tabletfx::Delay>(
            static_cast<float>(sample_rate), channels, 5000.0F, options.delay_ms,
            options.delay_feedback, options.delay_mix));
    }
    if (std::abs(options.output_gain_db) > 0.0001F) {
        effects.push_back(std::make_unique<tabletfx::Gain>(
            options.output_gain_db, static_cast<float>(sample_rate)));
    }
    if (options.limiter_enabled) {
        effects.push_back(std::make_unique<tabletfx::PeakLimiter>(
            static_cast<float>(sample_rate), options.limiter_ceiling_db,
            options.limiter_release_ms));
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
              << percentile(percentages, 0.999) << ","
              << *std::max_element(percentages.begin(), percentages.end()) << ","
              << effect->memory_bytes() << ","
              << signal.rms << '\n';
}

void benchmark_full_chain(std::uint32_t sample_rate, std::uint16_t channels,
                          std::size_t block_frames, std::size_t warmup_blocks,
                          std::size_t measured_blocks) {
    tabletfx::CompressorParameters compressor_parameters;
    compressor_parameters.threshold_db = -24.0F;
    compressor_parameters.ratio = 3.0F;
    auto eq = std::make_unique<tabletfx::ThreeBandParametricEQ>(sample_rate, channels);
    eq->configure(120.0F, 2.0F, 0.707F, 800.0F, -1.0F, 0.707F,
                  5000.0F, 1.5F, 0.707F);

    std::vector<std::unique_ptr<tabletfx::Effect>> effects;
    effects.push_back(std::make_unique<tabletfx::Gain>(2.0F, static_cast<float>(sample_rate)));
    tabletfx::VolumePedalParameters volume_parameters;
    volume_parameters.position = 0.82F;
    effects.push_back(std::make_unique<tabletfx::VolumePedal>(
        static_cast<float>(sample_rate), volume_parameters));
    effects.push_back(std::make_unique<tabletfx::HighPassFilter>(sample_rate, channels, 80.0F));
    effects.push_back(std::make_unique<tabletfx::LowPassFilter>(sample_rate, channels, 12000.0F));
    effects.push_back(std::make_unique<tabletfx::NoiseGate>(sample_rate, -72.0F));
    effects.push_back(std::make_unique<tabletfx::PedalCompressor>(sample_rate));
    effects.push_back(std::make_unique<tabletfx::Compressor>(sample_rate, compressor_parameters));
    tabletfx::WahParameters wah_parameters;
    wah_parameters.position = 0.62F;
    effects.push_back(std::make_unique<tabletfx::Wah>(sample_rate, channels, wah_parameters));
    tabletfx::EnvelopeWahParameters envelope_wah_parameters;
    envelope_wah_parameters.sensitivity = 0.8F;
    effects.push_back(std::make_unique<tabletfx::EnvelopeWah>(
        static_cast<float>(sample_rate), channels, envelope_wah_parameters));
    effects.push_back(std::make_unique<tabletfx::CleanBoost>(3.0F, static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::SoftDrive>(3.0F));
    tabletfx::TremoloParameters tremolo_parameters;
    tremolo_parameters.rate_hz = 4.0F;
    tremolo_parameters.depth = 0.2F;
    effects.push_back(std::make_unique<tabletfx::Tremolo>(sample_rate, tremolo_parameters));
    effects.push_back(std::move(eq));
    effects.push_back(std::make_unique<tabletfx::Delay>(sample_rate, channels, 2000.0F,
                                                        125.0F, 0.2F, 0.1F));
    effects.push_back(std::make_unique<tabletfx::Gain>(-1.0F, static_cast<float>(sample_rate)));
    effects.push_back(std::make_unique<tabletfx::PeakLimiter>(sample_rate, -1.0F, 80.0F));

    std::vector<float> buffer(block_frames * channels);
    std::uint32_t noise = 0x1234567U;
    for (float& sample : buffer) {
        noise = noise * 1664525U + 1013904223U;
        sample = static_cast<float>((noise >> 8U) & 0xffffU) / 32768.0F - 1.0F;
    }
    const std::vector<float> source_buffer = buffer;
    const auto process_chain = [&effects, channels](std::span<float> block) noexcept {
        for (const auto& effect : effects) {
            effect->process(block, channels);
        }
    };
    for (std::size_t i = 0; i < warmup_blocks; ++i) {
        std::copy(source_buffer.begin(), source_buffer.end(), buffer.begin());
        process_chain(buffer);
    }
    for (const auto& effect : effects) {
        effect->reset();
    }
    std::vector<double> percentages;
    percentages.reserve(measured_blocks);
    const double deadline_us = static_cast<double>(block_frames) * 1'000'000.0 / sample_rate;
    for (std::size_t i = 0; i < measured_blocks; ++i) {
        std::copy(source_buffer.begin(), source_buffer.end(), buffer.begin());
        const auto start = Clock::now();
        process_chain(buffer);
        const auto end = Clock::now();
        const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        percentages.push_back(100.0 * elapsed_us / deadline_us);
    }
    std::size_t memory_bytes = 0;
    for (const auto& effect : effects) {
        memory_bytes += effect->memory_bytes();
    }
    const Metrics signal = metrics(buffer);
    std::cout << "full-chain," << std::fixed << std::setprecision(4)
              << percentile(percentages, 0.50) << ","
              << percentile(percentages, 0.95) << ","
              << percentile(percentages, 0.99) << ","
              << percentile(percentages, 0.999) << ","
              << *std::max_element(percentages.begin(), percentages.end()) << ","
              << memory_bytes << "," << signal.rms << '\n';
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
    std::cout << "processor,median_pct,p95_pct,p99_pct,p99_9_pct,max_pct,owned_memory_bytes,output_rms\n";
    benchmark_one("input-gain", [=] {
        return std::make_unique<tabletfx::Gain>(6.0F, static_cast<float>(sample_rate));
    },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("output-gain", [=] {
        return std::make_unique<tabletfx::Gain>(-6.0F, static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("volume-pedal", [=] {
        tabletfx::VolumePedalParameters parameters;
        parameters.position = 0.72F;
        return std::make_unique<tabletfx::VolumePedal>(static_cast<float>(sample_rate), parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("high-pass", [=] {
        return std::make_unique<tabletfx::HighPassFilter>(sample_rate, channels, 80.0F);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("low-pass", [=] {
        return std::make_unique<tabletfx::LowPassFilter>(sample_rate, channels, 12000.0F);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("noise-gate", [=] { return std::make_unique<tabletfx::NoiseGate>(sample_rate, -48.0F); },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("soft-drive", [] { return std::make_unique<tabletfx::SoftDrive>(4.0F); },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("delay", [=] { return std::make_unique<tabletfx::Delay>(sample_rate, channels, 2000.0F); },
                  sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    tabletfx::CompressorParameters compressor_parameters;
    compressor_parameters.threshold_db = -24.0F;
    compressor_parameters.ratio = 4.0F;
    benchmark_one("compressor", [=] {
        return std::make_unique<tabletfx::Compressor>(sample_rate, compressor_parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("pedal-compressor", [=] {
        return std::make_unique<tabletfx::PedalCompressor>(sample_rate);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("three-band-parametric-eq", [=] {
        auto effect = std::make_unique<tabletfx::ThreeBandParametricEQ>(sample_rate, channels);
        effect->configure(120.0F, 3.0F, 0.707F, 800.0F, -2.0F, 0.8F,
                          5000.0F, 4.0F, 0.707F);
        return effect;
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("peak-limiter", [=] {
        return std::make_unique<tabletfx::PeakLimiter>(static_cast<float>(sample_rate), -1.0F, 80.0F);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("wah", [=] {
        tabletfx::WahParameters parameters;
        parameters.position = 0.7F;
        return std::make_unique<tabletfx::Wah>(static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("envelope-wah", [=] {
        tabletfx::EnvelopeWahParameters parameters;
        parameters.sensitivity = 0.8F;
        return std::make_unique<tabletfx::EnvelopeWah>(
            static_cast<float>(sample_rate), channels, parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("clean-boost", [=] {
        return std::make_unique<tabletfx::CleanBoost>(6.0F, static_cast<float>(sample_rate));
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_one("tremolo", [=] {
        tabletfx::TremoloParameters parameters;
        parameters.rate_hz = 5.0F;
        parameters.depth = 0.5F;
        return std::make_unique<tabletfx::Tremolo>(static_cast<float>(sample_rate), parameters);
    }, sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
    benchmark_full_chain(sample_rate, channels, block_frames, warmup_blocks, measured_blocks);
}

void print_usage() {
    std::cerr << "Usage:\n"
              << "  tabletfx_offline render INPUT.wav OUTPUT.wav [--gain-db dB] [--output-gain-db dB]"
                 " [--volume-position 0..1] [--volume-min-db dB] [--volume-max-db dB]"
                 " [--volume-curve linear|audio] [--limiter-ceiling-db dB] [--limiter-release-ms ms]"
                 " [--wah-position 0..1] [--wah-min-hz Hz] [--wah-max-hz Hz] [--wah-q Q] [--wah-level-db dB]"
                  " [--touch-wah-sensitivity 0..1] [--touch-wah-direction up|down]"
                  " [--touch-wah-min-hz Hz] [--touch-wah-max-hz Hz] [--touch-wah-q Q]"
                  " [--touch-wah-level-db dB] [--touch-wah-attack-ms ms]"
                  " [--touch-wah-release-ms ms] [--touch-wah-smoothing-ms ms]"
                 " [--tremolo-rate-hz Hz] [--tremolo-depth 0..1]"
                 " [--tremolo-waveform sine|triangle|square] [--tremolo-sync on|off]"
                 " [--tremolo-tempo-bpm BPM] [--tremolo-beats-per-cycle beats]"
                 " [--boost-db 0..24]"
                 " [--high-pass-hz Hz]"
                 " [--low-pass-hz Hz] [--gate-threshold-db dB] [--gate-attenuation-db dB]"
                 " [--gate-hysteresis-db dB] [--gate-attack-ms ms] [--gate-hold-ms ms]"
                 " [--gate-release-ms ms]"
                 " [--drive 1..20] [--delay-ms ms] [--delay-feedback 0..0.95] [--delay-mix 0..1]\n"
              << "                 [--pedal-compressor-sensitivity 0..1] [--pedal-compressor-sustain 0..1]"
                 " [--pedal-compressor-attack 0..1] [--pedal-compressor-level-db -12..12]\n"
              << "                 [--compressor-threshold-db dB] [--compressor-ratio 1..20]"
                 " [--compressor-knee-db dB] [--compressor-attack-ms ms]"
                 " [--compressor-release-ms ms] [--compressor-makeup-db dB]\n"
              << "                 [--eq-bandN-frequency-hz Hz] [--eq-bandN-gain-db dB] [--eq-bandN-q Q] (N=1..3)\n"
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
