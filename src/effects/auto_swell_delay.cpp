#include "tabletfx/effects/auto_swell_delay.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTwoPi = 6.28318530717958647692F;
constexpr double kMaximumFloat =
    static_cast<double>(std::numeric_limits<float>::max());

float bounded_sample(double value) noexcept {
    if (!std::isfinite(value)) return 0.0F;
    return static_cast<float>(std::clamp(value, -kMaximumFloat, kMaximumFloat));
}

} // namespace

AutoSwellDelay::AutoSwellDelay(float sample_rate, std::uint16_t channels,
                               float max_delay_ms,
                               AutoSwellDelayParameters parameters)
    : sample_rate_(sample_rate), channels_(channels),
      max_delay_ms_(max_delay_ms) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2 ||
        !std::isfinite(max_delay_ms_) || max_delay_ms_ < kMinimumTimeMs ||
        max_delay_ms_ > kMaximumTimeMs) {
        throw std::invalid_argument(
            "Auto-Swell Delay requires 8-192 kHz, mono/stereo, and a maximum time in [1, 2000] ms");
    }

    const double delay_frames = std::ceil(
        static_cast<double>(sample_rate_) * static_cast<double>(max_delay_ms_) /
        1000.0);
    const double modulation_frames = std::ceil(
        static_cast<double>(sample_rate_) * kMaximumModMs / 1000.0);
    const double maximum_frames = static_cast<double>(
        storage_.max_size() / channels_ - 3U);
    if (!std::isfinite(delay_frames) || !std::isfinite(modulation_frames) ||
        delay_frames < 1.0 || modulation_frames < 1.0 ||
        delay_frames + modulation_frames + 3.0 > maximum_frames) {
        throw std::length_error("Auto-Swell Delay buffer size is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(delay_frames +
                                                 modulation_frames + 3.0);
    storage_.assign(capacity_frames_ * channels_, 0.0F);

    ramp_total_ = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::ceil(
                static_cast<double>(sample_rate_) * kControlRampMs / 1000.0)));
    delay_transition_total_ = ramp_total_;
    mod_phase_step_ = kTwoPi * kModRateHz / sample_rate_;
    detector_attack_coefficient_ = one_pole_coefficient(
        sample_rate_, kDetectorAttackMs);
    detector_release_coefficient_ = one_pole_coefficient(
        sample_rate_, kDetectorReleaseMs);

    configure(parameters);
    feedback_ = parameters_.feedback;
    mix_ = parameters_.mix;
    swell_step_ = 1000.0F / (sample_rate_ * parameters_.swell_ms);
    mod_depth_samples_ = parameters_.mod * kMaximumModMs * sample_rate_ / 1000.0F;
    tone_alpha_ = target_tone_alpha_;
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    swell_step_step_ = 0.0F;
    mod_depth_step_ = 0.0F;
    tone_alpha_step_ = 0.0F;
    ramp_remaining_ = 0;
    delay_transition_from_samples_ = target_delay_samples_;
    delay_transition_remaining_ = 0;
    reset();
}

void AutoSwellDelay::configure(AutoSwellDelayParameters parameters) noexcept {
    parameters.time_ms = bounded(parameters.time_ms, 350.0F,
                                 kMinimumTimeMs,
                                 std::min(kMaximumTimeMs, max_delay_ms_));
    if (parameters.time_mode != DelayTimeMode::milliseconds &&
        parameters.time_mode != DelayTimeMode::tempo_sync) {
        parameters.time_mode = DelayTimeMode::milliseconds;
    }
    parameters.tap_division = static_cast<std::uint8_t>(parameters.tap_division) <=
                                      static_cast<std::uint8_t>(
                                          DelayDivision::sixteenth_triplet)
                                  ? parameters.tap_division
                                  : DelayDivision::quarter;
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, 120.0F, 20.0F, 300.0F);
    const float requested_time = parameters.time_mode == DelayTimeMode::tempo_sync
        ? 60000.0F * beats_for_division(parameters.tap_division) /
              parameters.tempo_bpm
        : parameters.time_ms;
    const float effective_time = bounded(requested_time, 350.0F,
                                         kMinimumTimeMs,
                                         std::min(kMaximumTimeMs, max_delay_ms_));
    const float new_delay = static_cast<float>(samples_for_ms(effective_time));
    if (target_delay_samples_ == 0.0F) {
        target_delay_samples_ = new_delay;
        delay_transition_from_samples_ = new_delay;
    } else if (std::abs(new_delay - target_delay_samples_) > 1.0e-5F) {
        delay_transition_from_samples_ = current_delay_samples();
        target_delay_samples_ = new_delay;
        delay_transition_remaining_ = delay_transition_total_;
    }

    parameters.feedback = bounded(parameters.feedback, 0.25F,
                                  0.0F, kMaximumFeedback);
    parameters.mix = bounded(parameters.mix, 0.25F, 0.0F, 1.0F);
    parameters.swell_ms = bounded(parameters.swell_ms, 300.0F,
                                  1.0F, kMaximumSwellMs);
    parameters.mod = bounded(parameters.mod, 0.15F,
                             0.0F, kMaximumMod);
    parameters.tone = bounded(parameters.tone, 0.7F,
                              kMinimumTone, kMaximumTone);
    parameters.sense_db = bounded(parameters.sense_db, -36.0F,
                                   kMinimumSenseDb, kMaximumSenseDb);
    parameters.taper = valid_taper(parameters.taper);
    parameters_ = parameters;

    sense_threshold_ = std::pow(10.0F, parameters_.sense_db / 20.0F);
    const float min_cut_hz = 300.0F;
    const float max_cut_hz = std::max(
        min_cut_hz, std::min(20000.0F, sample_rate_ * 0.45F));
    const float cutoff_hz = min_cut_hz *
        std::pow(max_cut_hz / min_cut_hz, parameters_.tone);
    target_tone_alpha_ = std::exp(-kTwoPi * cutoff_hz / sample_rate_);

    feedback_step_ = (parameters_.feedback - feedback_) /
                     static_cast<float>(ramp_total_);
    mix_step_ = (parameters_.mix - mix_) / static_cast<float>(ramp_total_);
    const float target_swell_step =
        1000.0F / (sample_rate_ * parameters_.swell_ms);
    swell_step_step_ = (target_swell_step - swell_step_) /
                       static_cast<float>(ramp_total_);
    const float target_mod_depth = parameters_.mod * kMaximumModMs *
                                   sample_rate_ / 1000.0F;
    mod_depth_step_ = (target_mod_depth - mod_depth_samples_) /
                      static_cast<float>(ramp_total_);
    tone_alpha_step_ = (target_tone_alpha_ - tone_alpha_) /
                       static_cast<float>(ramp_total_);
    ramp_remaining_ = ramp_total_;
    build_taper_table();
}

void AutoSwellDelay::set_enabled(bool enabled) noexcept {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    detector_envelope_ = 0.0F;
    swell_phase_ = 0.0F;
    triggered_ = false;
    tail_quiet_frames_ = 0;
    if (!enabled_ && !parameters_.spillover) {
        // Logical invalidation avoids a large clear on the audio thread.
        valid_frames_ = 0;
        tone_state_ = {};
    }
}

void AutoSwellDelay::process(std::span<float> interleaved,
                             std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) return;

    const std::size_t frames = interleaved.size() / channels_;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels_;
        std::array<float, 2> original{};
        float input_peak = 0.0F;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const float value = interleaved[offset + channel];
            original[channel] = std::isfinite(value)
                ? std::clamp(value, -kMaximumSignal, kMaximumSignal)
                : 0.0F;
            input_peak = std::max(input_peak, std::abs(original[channel]));
        }

        if (!enabled_ && !parameters_.spillover) {
            valid_frames_ = 0;
            tone_state_ = {};
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                interleaved[offset + channel] = original[channel];
            }
            tail_quiet_frames_ = 0;
            continue;
        }

        float gain = 0.0F;
        if (enabled_) {
            const float detector_coefficient = input_peak > detector_envelope_
                ? detector_attack_coefficient_
                : detector_release_coefficient_;
            detector_envelope_ += detector_coefficient *
                                  (input_peak - detector_envelope_);
            const float reset_threshold = sense_threshold_ * 0.5011872F;
            if (!triggered_ && detector_envelope_ >= sense_threshold_) {
                triggered_ = true;
                swell_phase_ = 0.0F;
            } else if (triggered_ && detector_envelope_ <= reset_threshold) {
                triggered_ = false;
            }
            if (triggered_) {
                swell_phase_ = std::min(1.0F, swell_phase_ + swell_step_);
            } else {
                swell_phase_ = std::max(
                    0.0F, swell_phase_ - 1000.0F /
                        (sample_rate_ * kSwellReleaseMs));
            }
            gain = swell_gain();
        }

        const float delay_samples = current_delay_samples();
        const std::size_t write_offset = write_frame_ * channels_;
        float delayed_peak = 0.0F;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const float phase_offset = channel == 1U ? kPi * 0.5F : 0.0F;
            const float modulation = std::sin(mod_phase_ + phase_offset) *
                                     mod_depth_samples_;
            const float modulated_delay = std::clamp(
                delay_samples + modulation, 1.0F,
                static_cast<float>(capacity_frames_ - 2U));
            const float delayed = read_fractional_delay(modulated_delay, channel);
            const float filtered = (1.0F - tone_alpha_) * delayed +
                                   tone_alpha_ * tone_state_[channel];
            tone_state_[channel] = std::isfinite(filtered) ? filtered : 0.0F;
            delayed_peak = std::max(delayed_peak, std::abs(tone_state_[channel]));

            const float swollen_input = original[channel] * gain;
            if (enabled_) {
                const float direct = parameters_.dry ? swollen_input
                                                     : original[channel];
                const double output = static_cast<double>(direct) *
                                          static_cast<double>(1.0F - mix_) +
                                      static_cast<double>(tone_state_[channel]) *
                                          static_cast<double>(mix_);
                interleaved[offset + channel] = bounded_sample(output);
                const double recorded = static_cast<double>(swollen_input) +
                    static_cast<double>(tone_state_[channel]) * feedback_;
                storage_[write_offset + channel] = bounded_sample(recorded);
            } else {
                const double output = static_cast<double>(original[channel]) +
                    static_cast<double>(tone_state_[channel]) * mix_;
                interleaved[offset + channel] = bounded_sample(output);
                storage_[write_offset + channel] = bounded_sample(
                    static_cast<double>(tone_state_[channel]) * feedback_);
            }
        }

        write_frame_ = (write_frame_ + 1U) % capacity_frames_;
        valid_frames_ = std::min(valid_frames_ + 1U, capacity_frames_ - 1U);

        if (!enabled_ && delayed_peak < kTailSilenceThreshold) {
            ++tail_quiet_frames_;
            const std::size_t quiet_limit = std::max<std::size_t>(
                1U, static_cast<std::size_t>(std::ceil(delay_samples)));
            if (tail_quiet_frames_ >= quiet_limit) {
                valid_frames_ = 0;
                tone_state_ = {};
                tail_quiet_frames_ = 0;
            }
        } else {
            tail_quiet_frames_ = 0;
        }

        mod_phase_ += mod_phase_step_;
        if (mod_phase_ >= kTwoPi) mod_phase_ -= kTwoPi;
        advance_ramps();
    }
}

void AutoSwellDelay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
    valid_frames_ = 0;
    feedback_ = parameters_.feedback;
    mix_ = parameters_.mix;
    swell_step_ = 1000.0F / (sample_rate_ * parameters_.swell_ms);
    mod_depth_samples_ = parameters_.mod * kMaximumModMs *
                         sample_rate_ / 1000.0F;
    tone_alpha_ = target_tone_alpha_;
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    swell_step_step_ = 0.0F;
    mod_depth_step_ = 0.0F;
    tone_alpha_step_ = 0.0F;
    ramp_remaining_ = 0;
    delay_transition_from_samples_ = target_delay_samples_;
    delay_transition_remaining_ = 0;
    detector_envelope_ = 0.0F;
    swell_phase_ = 0.0F;
    triggered_ = false;
    tone_state_ = {};
    mod_phase_ = 0.0F;
    tail_quiet_frames_ = 0;
}

float AutoSwellDelay::bounded(float value, float fallback,
                              float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

AutoSwellDelayTaper AutoSwellDelay::valid_taper(
    AutoSwellDelayTaper taper) noexcept {
    switch (taper) {
    case AutoSwellDelayTaper::slow:
    case AutoSwellDelayTaper::normal:
    case AutoSwellDelayTaper::fast:
        return taper;
    }
    return AutoSwellDelayTaper::normal;
}

float AutoSwellDelay::beats_for_division(DelayDivision division) noexcept {
    switch (division) {
    case DelayDivision::whole: return 4.0F;
    case DelayDivision::dotted_half: return 3.0F;
    case DelayDivision::half: return 2.0F;
    case DelayDivision::dotted_quarter: return 1.5F;
    case DelayDivision::quarter: return 1.0F;
    case DelayDivision::quarter_triplet: return 2.0F / 3.0F;
    case DelayDivision::dotted_eighth: return 0.75F;
    case DelayDivision::eighth: return 0.5F;
    case DelayDivision::eighth_triplet: return 1.0F / 3.0F;
    case DelayDivision::dotted_sixteenth: return 0.375F;
    case DelayDivision::sixteenth: return 0.25F;
    case DelayDivision::sixteenth_triplet: return 1.0F / 6.0F;
    case DelayDivision::thirty_second: return 0.125F;
    case DelayDivision::sixty_fourth: return 0.0625F;
    }
    return 1.0F;
}

float AutoSwellDelay::one_pole_coefficient(float sample_rate,
                                           float milliseconds) noexcept {
    const double samples = std::max(
        1.0, static_cast<double>(sample_rate) * milliseconds / 1000.0);
    return static_cast<float>(-std::expm1(-1.0 / samples));
}

std::size_t AutoSwellDelay::samples_for_ms(float time_ms) const noexcept {
    const double samples = std::round(
        static_cast<double>(sample_rate_) * static_cast<double>(time_ms) / 1000.0);
    const double maximum = static_cast<double>(capacity_frames_ - 2U);
    return static_cast<std::size_t>(std::clamp(samples, 1.0, maximum));
}

float AutoSwellDelay::current_delay_samples() const noexcept {
    if (delay_transition_remaining_ == 0U) return target_delay_samples_;
    const float progress = 1.0F -
        static_cast<float>(delay_transition_remaining_) /
            static_cast<float>(delay_transition_total_);
    return delay_transition_from_samples_ +
        (target_delay_samples_ - delay_transition_from_samples_) * progress;
}

float AutoSwellDelay::read_fractional_delay(float delay_samples,
                                            std::uint16_t channel) const noexcept {
    const float bounded_delay = std::clamp(
        delay_samples, 1.0F, static_cast<float>(capacity_frames_ - 2U));
    const auto whole = static_cast<std::size_t>(std::floor(bounded_delay));
    const float fraction = bounded_delay - static_cast<float>(whole);
    const float recent = read_age(whole, channel);
    const float older = read_age(whole + 1U, channel);
    return recent + (older - recent) * fraction;
}

float AutoSwellDelay::read_age(std::size_t age,
                               std::uint16_t channel) const noexcept {
    if (age == 0U || age >= capacity_frames_ || age > valid_frames_ ||
        channel >= channels_) {
        return 0.0F;
    }
    const std::size_t read_frame =
        (write_frame_ + capacity_frames_ - age) % capacity_frames_;
    return storage_[read_frame * channels_ + channel];
}

float AutoSwellDelay::swell_gain() const noexcept {
    return lookup_taper(swell_phase_);
}

float AutoSwellDelay::lookup_taper(float position) const noexcept {
    const float table_position = std::clamp(position, 0.0F, 1.0F) *
                                 static_cast<float>(kTaperSteps);
    const std::size_t index = static_cast<std::size_t>(table_position);
    if (index >= kTaperSteps) return taper_table_[kTaperSteps];
    const float fraction = table_position - static_cast<float>(index);
    return taper_table_[index] +
           (taper_table_[index + 1U] - taper_table_[index]) * fraction;
}

void AutoSwellDelay::build_taper_table() noexcept {
    for (std::size_t index = 0; index <= kTaperSteps; ++index) {
        const float position = static_cast<float>(index) /
                               static_cast<float>(kTaperSteps);
        switch (parameters_.taper) {
        case AutoSwellDelayTaper::slow:
            taper_table_[index] = position * position;
            break;
        case AutoSwellDelayTaper::normal:
            taper_table_[index] = position;
            break;
        case AutoSwellDelayTaper::fast:
            taper_table_[index] = std::sqrt(position);
            break;
        }
    }
}

void AutoSwellDelay::advance_ramps() noexcept {
    if (ramp_remaining_ != 0U) {
        --ramp_remaining_;
        if (ramp_remaining_ == 0U) {
            feedback_ = parameters_.feedback;
            mix_ = parameters_.mix;
            swell_step_ = 1000.0F / (sample_rate_ * parameters_.swell_ms);
            mod_depth_samples_ = parameters_.mod * kMaximumModMs *
                                 sample_rate_ / 1000.0F;
            tone_alpha_ = target_tone_alpha_;
        } else {
            feedback_ += feedback_step_;
            mix_ += mix_step_;
            swell_step_ += swell_step_step_;
            mod_depth_samples_ += mod_depth_step_;
            tone_alpha_ += tone_alpha_step_;
        }
    }
    if (delay_transition_remaining_ != 0U) {
        --delay_transition_remaining_;
        if (delay_transition_remaining_ == 0U) {
            delay_transition_from_samples_ = target_delay_samples_;
        }
    }
}

} // namespace tabletfx
