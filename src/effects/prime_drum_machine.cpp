#include "tabletfx/effects/prime_drum_machine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tabletfx::effects {

DrumMachine::DrumMachine(float sample_rate, std::uint16_t channels,
                         DrumMachineParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate) {
        throw std::invalid_argument("Drum Machine sample rate is unsupported");
    }
    if (channels_ == 0U || channels_ > 2U) {
        throw std::invalid_argument("Drum Machine supports mono or stereo");
    }
    kick_.random_state = 0xA341316CU;
    snare_.random_state = 0xC8013EA4U;
    hat_.random_state = 0xAD90777DU;
    tom_.random_state = 0x7E95761EU;
    crash_.random_state = 0x9E3779B9U;
    configure(parameters);
    reset();
}

void DrumMachine::configure(DrumMachineParameters parameters) noexcept {
    parameters.tempo_bpm = bounded(parameters.tempo_bpm,
                                   parameters_.tempo_bpm, 40.0, 300.0);
    parameters.mix = bounded(parameters.mix, parameters_.mix, 0.0F, 1.0F);
    parameters.volume = bounded(parameters.volume, parameters_.volume,
                                 0.0F, 1.0F);
    parameters.intensity = bounded(parameters.intensity,
                                    parameters_.intensity, 0.0F, 1.0F);
    parameters.groove = bounded(parameters.groove, parameters_.groove,
                                0.0F, 1.0F);
    parameters.kick_mix = bounded(parameters.kick_mix,
                                   parameters_.kick_mix, 0.0F, 1.0F);
    parameters.snare_mix = bounded(parameters.snare_mix,
                                    parameters_.snare_mix, 0.0F, 1.0F);
    parameters.cymbal_mix = bounded(parameters.cymbal_mix,
                                    parameters_.cymbal_mix, 0.0F, 1.0F);
    parameters.tom_mix = bounded(parameters.tom_mix,
                                 parameters_.tom_mix, 0.0F, 1.0F);
    parameters.eq_db = bounded(parameters.eq_db, parameters_.eq_db,
                                -12.0F, 12.0F);
    parameters.timbre = bounded(parameters.timbre, parameters_.timbre,
                                0.0F, 1.0F);
    parameters.tuning_semitones = bounded(
        parameters.tuning_semitones, parameters_.tuning_semitones,
        -12.0F, 12.0F);
    parameters.reverb = bounded(parameters.reverb, parameters_.reverb,
                                 0.0F, 1.0F);
    parameters.drive = bounded(parameters.drive, parameters_.drive,
                               0.0F, 1.0F);
    parameters.length_bars = std::clamp<std::uint8_t>(
        parameters.length_bars, static_cast<std::uint8_t>(1U),
        static_cast<std::uint8_t>(4U));
    if (!valid_style(parameters.style)) {
        parameters.style = parameters_.style;
    }
    parameters_ = parameters;

    const double frames_per_step = static_cast<double>(sample_rate_) * 60.0 /
                                  (parameters_.tempo_bpm * 4.0);
    step_phase_increment_ = 1.0 / frames_per_step;
    tuning_ratio_ = std::pow(2.0F, parameters_.tuning_semitones / 12.0F);
    high_shelf_gain_ = std::pow(10.0F, parameters_.eq_db / 20.0F);
    const float cutoff_hz = 350.0F + parameters_.timbre * 7'500.0F;
    tone_coefficient_ = 1.0F - std::exp(-kTwoPi * cutoff_hz / sample_rate_);

    const auto phase_increment = [this](const float frequency_hz) {
        return kTwoPi * frequency_hz * tuning_ratio_ / sample_rate_;
    };
    kick_.phase_increment = phase_increment(44.0F + parameters_.timbre * 22.0F);
    snare_.phase_increment = phase_increment(150.0F + parameters_.timbre * 90.0F);
    tom_.phase_increment = phase_increment(90.0F + parameters_.timbre * 90.0F);

    decay_kick_ = std::exp(-1.0F / (sample_rate_ * 0.16F));
    decay_snare_ = std::exp(-1.0F / (sample_rate_ * 0.20F));
    decay_hat_ = std::exp(-1.0F / (sample_rate_ * 0.12F));
    decay_tom_ = std::exp(-1.0F / (sample_rate_ * 0.24F));
    decay_crash_ = std::exp(-1.0F / (sample_rate_ * 0.85F));

    const std::array<float, 4U> delay_ms{17.3F, 21.7F, 25.1F, 29.9F};
    for (std::size_t line = 0U; line < reverb_delays_.size(); ++line) {
        const auto requested = static_cast<std::size_t>(std::lround(
            sample_rate_ * delay_ms[line] * 0.001F));
        reverb_delays_[line] = std::clamp<std::size_t>(
            requested, 1U, kReverbLineLength - 1U);
    }
}

void DrumMachine::reset() noexcept {
    step_phase_ = 0.0;
    current_step_ = 0U;
    current_bar_ = 0U;
    started_ = false;
    accent_pending_ = false;
    fill_pending_ = false;
    swing_hat_delay_frames_ = 0U;
    swing_hat_amplitude_ = 0.0F;
    kick_.envelope = 0.0F;
    snare_.envelope = 0.0F;
    hat_.envelope = 0.0F;
    tom_.envelope = 0.0F;
    crash_.envelope = 0.0F;
    kick_.phase = snare_.phase = tom_.phase = 0.0F;
    kick_.random_state = 0xA341316CU;
    snare_.random_state = 0xC8013EA4U;
    hat_.random_state = 0xAD90777DU;
    tom_.random_state = 0x7E95761EU;
    crash_.random_state = 0x9E3779B9U;
    kick_.previous_noise = kick_.highpass_state = 0.0F;
    snare_.previous_noise = snare_.highpass_state = 0.0F;
    hat_.previous_noise = hat_.highpass_state = 0.0F;
    tom_.previous_noise = tom_.highpass_state = 0.0F;
    crash_.previous_noise = crash_.highpass_state = 0.0F;
    tone_state_[0] = tone_state_[1] = 0.0F;
    for (auto& line : reverb_lines_) {
        line.fill(0.0F);
    }
    reverb_write_ = 0U;
}

void DrumMachine::process(std::span<float> interleaved,
                          std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0U || channels > 2U) {
        return;
    }
    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0U; frame < frames; ++frame) {
        if (running_ && !started_) {
            trigger_step();
            started_ = true;
        }
        if (swing_hat_delay_frames_ > 0U) {
            --swing_hat_delay_frames_;
            if (swing_hat_delay_frames_ == 0U) {
                trigger_voice(hat_, swing_hat_amplitude_);
            }
        }

        const float drum = saturate(
            process_kick() * parameters_.kick_mix +
            process_snare() * parameters_.snare_mix +
            process_hat() * parameters_.cymbal_mix +
            process_tom() * parameters_.tom_mix +
            process_crash() * parameters_.cymbal_mix);

        float wet_left = 0.0F;
        float wet_right = 0.0F;
        reverb_sample(drum, wet_left, wet_right);
        const float dry_amount = 1.0F - parameters_.reverb * 0.4F;
        const float drum_left = muted_ ? 0.0F :
            (drum * dry_amount + wet_left * parameters_.reverb) *
                parameters_.mix * parameters_.volume;
        const float drum_right = muted_ ? 0.0F :
            (drum * dry_amount + wet_right * parameters_.reverb) *
                parameters_.mix * parameters_.volume;

        const std::size_t offset = frame * channels_;
        for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
            const float input = finite_input(interleaved[offset + channel]);
            const float channel_drum = channel == 0U ? drum_left : drum_right;
            tone_state_[channel] += tone_coefficient_ *
                                    (channel_drum - tone_state_[channel]);
            const float low = tone_state_[channel];
            const float high = channel_drum - low;
            const float eq_drum = std::clamp(
                low + high * high_shelf_gain_, -kMaximumGeneratedSample,
                kMaximumGeneratedSample);
            const float output = input + eq_drum;
            interleaved[offset + channel] =
                std::isfinite(output) ? output : input;
        }

        ++reverb_write_;
        if (reverb_write_ == kReverbLineLength) {
            reverb_write_ = 0U;
        }
        if (running_) {
            step_phase_ += step_phase_increment_;
            if (step_phase_ >= 1.0) {
                step_phase_ -= 1.0;
                current_step_ = static_cast<std::uint8_t>(current_step_ + 1U);
                if (current_step_ >= kStepCount) {
                    current_step_ = 0U;
                    current_bar_ = static_cast<std::uint8_t>(current_bar_ + 1U);
                    if (current_bar_ >= parameters_.length_bars) {
                        current_bar_ = 0U;
                    }
                }
                trigger_step();
            }
        }
    }
}

float DrumMachine::bounded(float value, float fallback,
                           float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

double DrumMachine::bounded(double value, double fallback,
                            double minimum, double maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

bool DrumMachine::valid_style(DrumMachineStyle style) noexcept {
    switch (style) {
        case DrumMachineStyle::rock:
        case DrumMachineStyle::pop:
        case DrumMachineStyle::shuffle:
        case DrumMachineStyle::half_time:
            return true;
    }
    return false;
}

float DrumMachine::next_noise(Voice& voice) noexcept {
    std::uint32_t value = voice.random_state;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    voice.random_state = value == 0U ? 0xA341316CU : value;
    return static_cast<float>(voice.random_state >> 8U) *
               (2.0F / 16777215.0F) -
           1.0F;
}

float DrumMachine::process_kick() noexcept {
    if (kick_.envelope <= 1.0e-5F) {
        kick_.envelope = 0.0F;
        return 0.0F;
    }
    kick_.phase += kick_.phase_increment;
    if (kick_.phase >= kTwoPi) {
        kick_.phase -= kTwoPi;
    }
    const float output = std::sin(kick_.phase) * kick_.envelope *
                         kick_.amplitude;
    kick_.envelope *= decay_kick_;
    return output;
}

float DrumMachine::process_snare() noexcept {
    if (snare_.envelope <= 1.0e-5F) {
        snare_.envelope = 0.0F;
        return 0.0F;
    }
    snare_.phase += snare_.phase_increment;
    if (snare_.phase >= kTwoPi) {
        snare_.phase -= kTwoPi;
    }
    const float noise = next_noise(snare_);
    const float filtered = noise - snare_.previous_noise +
                           0.78F * snare_.highpass_state;
    snare_.previous_noise = noise;
    snare_.highpass_state = filtered;
    const float body = std::sin(snare_.phase);
    const float output = (0.72F * filtered + 0.28F * body) *
                         snare_.envelope * snare_.amplitude;
    snare_.envelope *= decay_snare_;
    return output;
}

float DrumMachine::process_hat() noexcept {
    if (hat_.envelope <= 1.0e-5F) {
        hat_.envelope = 0.0F;
        return 0.0F;
    }
    const float noise = next_noise(hat_);
    const float filtered = noise - hat_.previous_noise +
                           0.42F * hat_.highpass_state;
    hat_.previous_noise = noise;
    hat_.highpass_state = filtered;
    const float output = filtered * hat_.envelope * hat_.amplitude;
    hat_.envelope *= decay_hat_;
    return output;
}

float DrumMachine::process_tom() noexcept {
    if (tom_.envelope <= 1.0e-5F) {
        tom_.envelope = 0.0F;
        return 0.0F;
    }
    tom_.phase += tom_.phase_increment;
    if (tom_.phase >= kTwoPi) {
        tom_.phase -= kTwoPi;
    }
    const float output = std::sin(tom_.phase) * tom_.envelope * tom_.amplitude;
    tom_.envelope *= decay_tom_;
    return output;
}

float DrumMachine::process_crash() noexcept {
    if (crash_.envelope <= 1.0e-5F) {
        crash_.envelope = 0.0F;
        return 0.0F;
    }
    const float noise = next_noise(crash_);
    const float filtered = noise - crash_.previous_noise +
                           0.72F * crash_.highpass_state;
    crash_.previous_noise = noise;
    crash_.highpass_state = filtered;
    const float output = filtered * crash_.envelope * crash_.amplitude;
    crash_.envelope *= decay_crash_;
    return output;
}

void DrumMachine::trigger_step() noexcept {
    const std::uint8_t step = current_step_;
    float accent = (step % 4U == 0U) ? 1.0F : 0.72F;
    if (accent_pending_) {
        trigger_voice(crash_, 0.9F * parameters_.intensity);
        accent = 1.0F;
        accent_pending_ = false;
    }

    bool kick = false;
    bool snare = false;
    bool hat = false;
    switch (parameters_.style) {
        case DrumMachineStyle::rock:
            kick = step == 0U || step == 8U || step == 11U ||
                   (variation_ && step == 14U);
            snare = step == 4U || step == 12U;
            hat = step % 2U == 0U;
            break;
        case DrumMachineStyle::pop:
            kick = step == 0U || step == 6U || step == 8U || step == 14U ||
                   (variation_ && step == 10U);
            snare = step == 4U || step == 12U;
            hat = step % 2U == 0U;
            break;
        case DrumMachineStyle::shuffle:
            kick = step == 0U || step == 8U || (variation_ && step == 10U);
            snare = step == 4U || step == 12U;
            hat = step % 4U == 0U || step % 4U == 3U;
            break;
        case DrumMachineStyle::half_time:
            kick = step == 0U || step == 10U || (variation_ && step == 6U);
            snare = step == 8U;
            hat = step % 2U == 0U;
            break;
    }

    const float velocity = accent * parameters_.intensity;
    if (kick) {
        trigger_voice(kick_, velocity);
    }
    if (snare) {
        trigger_voice(snare_, velocity);
    }
    if (hat) {
        if (step % 4U == 2U && parameters_.groove > 0.0F) {
            trigger_swing_hat(velocity);
        } else {
            trigger_voice(hat_, velocity);
        }
    }

    const bool phrase_last_bar =
        static_cast<std::uint8_t>(current_bar_ + 1U) >= parameters_.length_bars;
    if (fill_pending_ && phrase_last_bar && step >= 12U &&
        (step == 12U || step == 14U || step == 15U)) {
        trigger_voice(tom_, velocity);
    }
    if (step == 15U && fill_pending_ && phrase_last_bar) {
        trigger_voice(crash_, 0.75F * parameters_.intensity);
        fill_pending_ = false;
    } else if (step == 15U && phrase_last_bar) {
        // The phrase-length setting now changes audible content: the last
        // bar receives a light turnaround tom, unless a fill supplies it.
        trigger_voice(tom_, 0.42F * parameters_.intensity);
    }
}

void DrumMachine::trigger_voice(Voice& voice, float amplitude) noexcept {
    voice.envelope = 1.0F;
    voice.amplitude = amplitude;
    voice.phase = 0.0F;
}

void DrumMachine::trigger_swing_hat(float amplitude) noexcept {
    const double frames_per_step = 1.0 / step_phase_increment_;
    // Swing moves the offbeat eighth by up to 16% of one quarter-note.
    const double delay_frames = frames_per_step * 0.64 * parameters_.groove;
    swing_hat_delay_frames_ = static_cast<std::uint32_t>(std::max(
        1.0, std::floor(delay_frames + 0.5)));
    swing_hat_amplitude_ = amplitude;
}

void DrumMachine::reverb_sample(float input, float& left,
                                float& right) noexcept {
    std::array<float, 4U> delayed{};
    for (std::size_t line = 0U; line < reverb_lines_.size(); ++line) {
        const std::size_t delay = reverb_delays_[line];
        const std::size_t read_index = reverb_write_ >= delay
            ? reverb_write_ - delay
            : reverb_write_ + kReverbLineLength - delay;
        delayed[line] = reverb_lines_[line][read_index];
        reverb_lines_[line][reverb_write_] =
            input + delayed[line] * reverb_feedback_[line];
    }
    left = 0.5F * (delayed[0U] + delayed[2U]);
    right = 0.5F * (delayed[1U] + delayed[3U]);
}

float DrumMachine::saturate(float sample) const noexcept {
    if (parameters_.drive <= 0.0F) {
        return sample;
    }
    const float gain = 1.0F + parameters_.drive * 6.0F;
    const float driven = sample * gain;
    const float square = driven * driven;
    const float normalized = driven * (27.0F + square) /
                             (27.0F + 9.0F * square);
    return sample + (normalized - sample) * parameters_.drive;
}

float DrumMachine::finite_input(float sample) noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return std::clamp(sample, -1.0e6F, 1.0e6F);
}

}  // namespace tabletfx::effects
