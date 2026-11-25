#include "tabletfx/effects/fractal_chromatic_harmonizer.hpp"

#include <StftPitchShift/RFFT.h>
#include <StftPitchShift/StftPitchShiftCore.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace tabletfx::effects {
namespace {

constexpr std::size_t kDftSize = 1024;
constexpr std::size_t kAnalysisSamples = 2 * kDftSize;
constexpr std::size_t kProcessFrames = 128;
constexpr std::size_t kAnalysisHop = kProcessFrames / 2;
constexpr std::size_t kProcessorBufferSamples = kAnalysisSamples + kProcessFrames;
constexpr std::size_t kOutputQueueFrames = 2 * kProcessFrames;
constexpr std::size_t kMaximumVoices = 4;
constexpr std::size_t kMaximumChannels = 2;
constexpr float kMaximumDelayMs = 2000.0F;
constexpr double kPi = 3.141592653589793238462643383279502884;

float checked_sample_rate(float sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate < 8000.0F ||
        sample_rate > 384000.0F) {
        throw std::invalid_argument(
            "FractalChromaticHarmonizer sample rate must be 8-384 kHz");
    }
    return sample_rate;
}

std::uint16_t checked_channels(std::uint16_t channels) {
    if (channels == 0 || channels > kMaximumChannels) {
        throw std::invalid_argument(
            "FractalChromaticHarmonizer supports one or two channels");
    }
    return channels;
}

std::uint8_t checked_voices(std::uint8_t voices) {
    if (voices != 2 && voices != 4) {
        throw std::invalid_argument(
            "FractalChromaticHarmonizer supports two or four voices");
    }
    return voices;
}

FractalChromaticHarmonizer::InputMode checked_input_mode(
    FractalChromaticHarmonizer::InputMode input_mode,
    std::uint16_t channels) {
    if (input_mode != FractalChromaticHarmonizer::InputMode::MonoSum &&
        input_mode != FractalChromaticHarmonizer::InputMode::StereoRoute) {
        throw std::invalid_argument(
            "FractalChromaticHarmonizer input mode is invalid");
    }
    if (input_mode == FractalChromaticHarmonizer::InputMode::StereoRoute &&
        channels != 2) {
        throw std::invalid_argument(
            "StereoRoute requires a two-channel input/output buffer");
    }
    return input_mode;
}

double pitch_factor(float semitones, float cents) noexcept {
    const double total_semitones = static_cast<double>(semitones) +
                                   static_cast<double>(cents) / 100.0;
    return std::exp2(total_semitones / 12.0);
}

} // namespace

struct FractalChromaticHarmonizer::Impl {
    using Core = stftpitchshift::StftPitchShiftCore<double>;
    using Complex = std::complex<double>;

    struct VoiceState {
        VoiceParameters parameters{};
        std::vector<double> factor{1.0};
        std::unique_ptr<Core> core;
        std::array<double, kProcessorBufferSamples> overlap_output{};
        std::vector<float> delay_line;
        std::size_t delay_write = 0;
        std::size_t delay_samples = 0;
    };

    Impl(float sample_rate, std::uint16_t channel_count, std::uint8_t voice_count,
         FractalChromaticHarmonizer::InputMode mode)
        : channels(channel_count), voices(voice_count), input_mode(mode),
          max_delay_samples(static_cast<std::size_t>(std::llround(
              static_cast<double>(sample_rate) * kMaximumDelayMs / 1000.0))),
          fft(std::make_shared<stftpitchshift::RFFT>()) {
        initialize_windows();
        prewarm_fft();

        for (std::uint8_t voice_index = 0; voice_index < voices; ++voice_index) {
            auto& voice = voice_state[voice_index];
            voice.core = std::make_unique<Core>(
                fft, std::make_tuple(kAnalysisSamples, kProcessFrames),
                kAnalysisHop, static_cast<double>(sample_rate));
            voice.core->factors(voice.factor);

            // Useful project-authored chord defaults; these are not Fractal
            // factory settings. Callers can replace every voice independently.
            constexpr std::array<float, kMaximumVoices> default_shifts{
                7.0F, 12.0F, -5.0F, -12.0F};
            constexpr std::array<float, kMaximumVoices> default_pans{
                -0.45F, 0.45F, -0.8F, 0.8F};
            voice.parameters.shift_semitones = default_shifts[voice_index];
            voice.parameters.level = 1.0F / static_cast<float>(voices);
            voice.parameters.pan = default_pans[voice_index];
            voice.factor[0] = pitch_factor(voice.parameters.shift_semitones,
                                           voice.parameters.detune_cents);
            voice.core->factors(voice.factor);
        }

        output_count = kProcessFrames;
    }

    void initialize_windows() noexcept {
        // Mirror the pinned STFT's asymmetric analysis/synthesis windows for
        // the 2048-sample analysis frame and 128-sample synthesis hop. Using a
        // symmetric 2048-sample Hann pair here materially attenuates the output.
        constexpr std::size_t synthesis_size = kProcessFrames;
        constexpr std::size_t half_synthesis = synthesis_size / 2;
        constexpr std::size_t left_size =
            2 * kAnalysisSamples - 2 * half_synthesis;
        constexpr std::size_t right_size = 2 * half_synthesis;
        const auto symmetric = [](std::size_t size, std::size_t index) noexcept {
            return 0.5 - 0.5 * std::cos(
                (2.0 * kPi * static_cast<double>(index)) /
                static_cast<double>(size));
        };

        analysis_window.fill(0.0);
        synthesis_window.fill(0.0);
        for (std::size_t sample = 0;
             sample < kAnalysisSamples - half_synthesis; ++sample) {
            analysis_window[sample] = symmetric(left_size, sample);
        }
        for (std::size_t sample = 0; sample < half_synthesis; ++sample) {
            analysis_window[sample + kAnalysisSamples - half_synthesis] =
                symmetric(right_size, sample + half_synthesis);
        }

        double window_product = 0.0;
        for (std::size_t sample = 0; sample < half_synthesis; ++sample) {
            const std::size_t index = sample + kAnalysisSamples - 2 * half_synthesis;
            const double right = symmetric(right_size, sample);
            const double left = symmetric(left_size, index);
            synthesis_window[index] = right * right / left;
            window_product += analysis_window[index] * synthesis_window[index];
        }
        for (std::size_t sample = 0; sample < half_synthesis; ++sample) {
            const std::size_t index = sample + kAnalysisSamples - half_synthesis;
            synthesis_window[index] = symmetric(right_size, sample + half_synthesis);
            window_product += analysis_window[index] * synthesis_window[index];
        }
        const double unity_gain = static_cast<double>(kAnalysisHop) /
                                  window_product;
        for (double& value : synthesis_window) value *= unity_gain;
    }

    void prewarm_fft() {
        std::array<double, kAnalysisSamples> zero{};
        fft->fft(zero, analysis_spectrum[0]);
        fft->ifft(analysis_spectrum[0], inverse_frame);
    }

    void set_factor(VoiceState& voice) noexcept {
        voice.factor[0] = pitch_factor(voice.parameters.shift_semitones,
                                      voice.parameters.detune_cents);
        voice.core->factors(voice.factor);
    }

    float delayed_sample(VoiceState& voice, float pitched_sample) noexcept {
        auto& line = voice.delay_line;
        if (voice.delay_samples == 0 || line.empty()) {
            // A zero-millisecond delay is a direct shifted voice. In particular,
            // it does not turn feedback into an unstable same-sample loop.
            return pitched_sample;
        }

        const std::size_t read_index =
            (voice.delay_write + line.size() - voice.delay_samples) % line.size();
        const float prior_echo = line[read_index];
        const float output = pitched_sample +
            voice.parameters.feedback * prior_echo;
        line[voice.delay_write] = std::isfinite(output) ? output : 0.0F;
        return std::isfinite(output) ? output : 0.0F;
    }

    void process_block() noexcept {
        const std::uint16_t analysis_channels =
            input_mode == FractalChromaticHarmonizer::InputMode::StereoRoute ? 2 : 1;
        for (std::uint16_t channel = 0; channel < analysis_channels; ++channel) {
            auto& input = input_buffer[channel];
            std::copy(input.begin() + static_cast<std::ptrdiff_t>(kProcessFrames),
                      input.end(), input.begin());
            for (std::size_t frame = 0; frame < kProcessFrames; ++frame) {
                float value = 0.0F;
                if (input_mode == FractalChromaticHarmonizer::InputMode::StereoRoute) {
                    value = input_block[frame * 2 + channel];
                } else if (channels == 1) {
                    value = input_block[frame];
                } else {
                    // Project-authored average for stable mono-sum headroom.
                    value = 0.5F * (input_block[frame * 2] +
                                    input_block[frame * 2 + 1]);
                }
                input[kAnalysisSamples + frame] =
                    std::isfinite(value) ? static_cast<double>(value) : 0.0;
            }
        }

        std::array<bool, kMaximumVoices> voice_active{};
        bool any_active_voice = false;
        for (std::uint8_t voice_index = 0; voice_index < voices; ++voice_index) {
            const auto& voice = voice_state[voice_index];
            voice_active[voice_index] = voice.parameters.level > 0.0F ||
                                        voice.delay_samples > 0U;
            any_active_voice = any_active_voice || voice_active[voice_index];
        }

        for (std::size_t offset = 0;
             offset + kAnalysisSamples < kProcessorBufferSamples;
             offset += kAnalysisHop) {
            if (!any_active_voice) {
                break;
            }
            for (std::uint16_t channel = 0; channel < analysis_channels; ++channel) {
                const auto& input = input_buffer[channel];
                for (std::size_t sample = 0; sample < kAnalysisSamples; ++sample) {
                    analysis_frame[sample] =
                        input[offset + sample] * analysis_window[sample];
                }
                // The source frame and window/FFT analysis are shared by all
                // voices. Each voice only pays for its pitch core and inverse FFT.
                fft->fft(analysis_frame, analysis_spectrum[channel]);
            }

            for (std::uint8_t voice_index = 0; voice_index < voices; ++voice_index) {
                if (!voice_active[voice_index]) {
                    continue;
                }
                auto& voice = voice_state[voice_index];
                const std::uint16_t source_channel =
                    input_mode == FractalChromaticHarmonizer::InputMode::StereoRoute
                        ? static_cast<std::uint16_t>(voice_index % 2)
                        : 0;
                std::copy(analysis_spectrum[source_channel].begin(),
                          analysis_spectrum[source_channel].end(),
                          shifted_spectrum.begin());
                voice.core->shiftpitch(shifted_spectrum);
                // Match the pinned STFT engine's synthesis-side DC/Nyquist
                // suppression after the per-voice phase-vocoder operation.
                shifted_spectrum.front() = Complex{};
                shifted_spectrum.back() = Complex{};
                fft->ifft(shifted_spectrum, inverse_frame);

                auto& output = voice.overlap_output;
                for (std::size_t sample = 0; sample < kAnalysisSamples; ++sample) {
                    output[offset + sample] +=
                        inverse_frame[sample] * synthesis_window[sample];
                }
            }
        }

        for (std::size_t frame = 0; frame < kProcessFrames; ++frame) {
            float left_mix = 0.0F;
            float right_mix = 0.0F;
            for (std::uint8_t voice_index = 0; voice_index < voices; ++voice_index) {
                if (!voice_active[voice_index]) {
                    continue;
                }
                auto& voice = voice_state[voice_index];
                const auto output_frame = kAnalysisSamples - kProcessFrames + frame;
                const float sample = delayed_sample(
                    voice, static_cast<float>(voice.overlap_output[output_frame]));
                if (channels == 1) {
                    left_mix += sample * voice.parameters.level;
                } else {
                    const float pan = voice.parameters.pan;
                    const float left_balance = pan > 0.0F ? 1.0F - pan : 1.0F;
                    const float right_balance = pan < 0.0F ? 1.0F + pan : 1.0F;
                    left_mix += sample * voice.parameters.level * left_balance;
                    right_mix += sample * voice.parameters.level * right_balance;
                }
            }

            const std::size_t tail = (output_head + output_count) % kOutputQueueFrames;
            output_queue[tail * channels] = std::isfinite(left_mix) ? left_mix : 0.0F;
            if (channels == 2) {
                output_queue[tail * channels + 1] =
                    std::isfinite(right_mix) ? right_mix : 0.0F;
            }
            ++output_count;
            for (std::uint8_t voice_index = 0; voice_index < voices; ++voice_index) {
                auto& voice = voice_state[voice_index];
                if (!voice.delay_line.empty()) {
                    voice.delay_write =
                        (voice.delay_write + 1) % voice.delay_line.size();
                }
            }
        }

        for (std::uint8_t voice_index = 0; voice_index < voices; ++voice_index) {
            auto& voice = voice_state[voice_index];
            auto& output = voice.overlap_output;
            if (!voice_active[voice_index]) {
                output.fill(0.0);
                continue;
            }
            std::copy(output.begin() + static_cast<std::ptrdiff_t>(kProcessFrames),
                      output.end(), output.begin());
            std::fill(output.begin() + static_cast<std::ptrdiff_t>(kAnalysisSamples),
                      output.end(), 0.0);
        }
    }

    void reset() noexcept {
        for (auto& input : input_buffer) input.fill(0.0);
        input_block.fill(0.0F);
        output_queue.fill(0.0F);
        analysis_frame.fill(0.0);
        inverse_frame.fill(0.0);
        for (auto& spectrum : analysis_spectrum) spectrum.fill(Complex{});
        shifted_spectrum.fill(Complex{});
        for (std::uint8_t voice_index = 0; voice_index < voices; ++voice_index) {
            auto& voice = voice_state[voice_index];
            voice.overlap_output.fill(0.0);
            std::fill(voice.delay_line.begin(), voice.delay_line.end(), 0.0F);
            voice.core->reset();
            voice.delay_write = 0;
        }
        input_count = 0;
        output_head = 0;
        output_count = kProcessFrames;
    }

    std::size_t storage_bytes() const noexcept {
        constexpr std::size_t bins = kAnalysisSamples / 2 + 1;
        constexpr std::size_t fft_half = kAnalysisSamples / 2;
        std::size_t total = sizeof(*this) + sizeof(*fft) +
            sizeof(stftpitchshift::CRFFT<double>) +
            fft_half * (sizeof(Complex) + sizeof(std::size_t) +
                        6 * sizeof(Complex));
        for (std::uint8_t voice_index = 0; voice_index < voices; ++voice_index) {
            const auto& voice = voice_state[voice_index];
            total += voice.factor.capacity() * sizeof(double);
            total += voice.delay_line.capacity() * sizeof(float);
            total += sizeof(Core);
            // StftPitchShiftCore's pinned storage: phase-vocoder buffers, one
            // pitch resampler/output, one cepstral transform pair and envelope.
            // Allocator metadata/control blocks are excluded as elsewhere.
            total += 4 * bins * sizeof(double) +
                     2 * bins * sizeof(Complex) +
                     kAnalysisSamples * sizeof(double) +
                     2 * sizeof(double) +
                     sizeof(stftpitchshift::Resampler<double>) +
                     sizeof(std::vector<Complex>);
        }
        return total;
    }

    std::uint16_t channels;
    std::uint8_t voices;
    FractalChromaticHarmonizer::InputMode input_mode;
    std::size_t max_delay_samples;
    std::shared_ptr<stftpitchshift::FFT> fft;
    std::array<VoiceState, kMaximumVoices> voice_state{};
    std::array<std::array<double, kProcessorBufferSamples>, kMaximumChannels>
        input_buffer{};
    std::array<float, kProcessFrames * kMaximumChannels> input_block{};
    std::array<double, kAnalysisSamples> analysis_frame{};
    std::array<double, kAnalysisSamples> inverse_frame{};
    std::array<std::array<Complex, kAnalysisSamples / 2 + 1>, kMaximumChannels>
        analysis_spectrum{};
    std::array<Complex, kAnalysisSamples / 2 + 1> shifted_spectrum{};
    std::array<double, kAnalysisSamples> analysis_window{};
    std::array<double, kAnalysisSamples> synthesis_window{};
    std::array<float, kOutputQueueFrames * kMaximumChannels> output_queue{};
    std::size_t input_count = 0;
    std::size_t output_head = 0;
    std::size_t output_count = 0;
};

FractalChromaticHarmonizer::FractalChromaticHarmonizer(
    float sample_rate, std::uint16_t channels, std::uint8_t voices,
    InputMode input_mode)
    : impl_(std::make_unique<Impl>(checked_sample_rate(sample_rate),
                                  checked_channels(channels),
                                  checked_voices(voices),
                                  checked_input_mode(input_mode,
                                                     checked_channels(channels)))),
      sample_rate_(sample_rate), channels_(channels), voices_(voices),
      input_mode_(input_mode) {}

FractalChromaticHarmonizer::~FractalChromaticHarmonizer() = default;

std::size_t FractalChromaticHarmonizer::memory_bytes() const noexcept {
    return sizeof(*this) + impl_->storage_bytes();
}

float FractalChromaticHarmonizer::algorithmic_latency_ms() const noexcept {
    return 1000.0F * static_cast<float>(latency_samples()) / sample_rate_;
}

std::uint32_t FractalChromaticHarmonizer::latency_samples() const noexcept {
    return static_cast<std::uint32_t>(2 * kProcessFrames);
}

FractalChromaticHarmonizer::VoiceParameters
FractalChromaticHarmonizer::voice_parameters(std::uint8_t voice) const noexcept {
    if (voice >= voices_) return {};
    return impl_->voice_state[voice].parameters;
}

bool FractalChromaticHarmonizer::set_voice_parameters(
    std::uint8_t voice_index, const VoiceParameters& parameters) noexcept {
    if (voice_index >= voices_ ||
        !std::isfinite(parameters.shift_semitones) ||
        !std::isfinite(parameters.detune_cents) ||
        !std::isfinite(parameters.level) || !std::isfinite(parameters.pan) ||
        !std::isfinite(parameters.delay_ms) ||
        !std::isfinite(parameters.feedback)) {
        return false;
    }

    auto& voice = impl_->voice_state[voice_index];
    const VoiceParameters old = voice.parameters;
    const std::size_t old_delay_samples = voice.delay_samples;
    const float delay_ms = std::clamp(parameters.delay_ms, 0.0F, kMaximumDelayMs);
    const std::size_t delay_samples = static_cast<std::size_t>(std::llround(
        static_cast<double>(delay_ms) * sample_rate_ / 1000.0));
    if (delay_samples > 0 && voice.delay_line.empty()) {
        try {
            voice.delay_line.resize(impl_->max_delay_samples + 1, 0.0F);
        } catch (...) {
            return false;
        }
    }

    voice.parameters.shift_semitones =
        std::clamp(parameters.shift_semitones, -24.0F, 24.0F);
    voice.parameters.detune_cents =
        std::clamp(parameters.detune_cents, -50.0F, 50.0F);
    voice.parameters.level = std::clamp(parameters.level, 0.0F, 2.0F);
    voice.parameters.pan = std::clamp(parameters.pan, -1.0F, 1.0F);
    voice.parameters.delay_ms = delay_ms;
    voice.parameters.feedback = std::clamp(parameters.feedback, 0.0F, 0.9F);
    voice.delay_samples = delay_samples;
    if (old.delay_ms > 0.0F && delay_ms == 0.0F) {
        std::fill(voice.delay_line.begin(), voice.delay_line.end(), 0.0F);
        voice.delay_write = 0;
    }

    if (old.shift_semitones != voice.parameters.shift_semitones ||
        old.detune_cents != voice.parameters.detune_cents) {
        impl_->set_factor(voice);
    }
    const bool was_active = old.level > 0.0F || old_delay_samples > 0U;
    const bool is_active = voice.parameters.level > 0.0F ||
                           voice.delay_samples > 0U;
    if (was_active != is_active) {
        voice.core->reset();
        voice.overlap_output.fill(0.0);
    }
    return true;
}

void FractalChromaticHarmonizer::process(std::span<float> interleaved,
                                         std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            impl_->input_block[impl_->input_count * channels_ + channel] =
                interleaved[frame * channels_ + channel];
        }
        ++impl_->input_count;
        if (impl_->input_count == kProcessFrames) {
            impl_->process_block();
            impl_->input_count = 0;
        }

        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            interleaved[frame * channels_ + channel] =
                impl_->output_queue[impl_->output_head * channels_ + channel];
        }
        impl_->output_head = (impl_->output_head + 1) % kOutputQueueFrames;
        if (impl_->output_count > 0) --impl_->output_count;
    }
}

void FractalChromaticHarmonizer::reset() noexcept {
    impl_->reset();
}

} // namespace tabletfx::effects
