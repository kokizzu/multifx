#include "tabletfx/effects.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {
namespace {

constexpr float kMinGainDb = -96.0F;
constexpr float kMaxGainDb = 24.0F;

float decibels_to_linear(float db) noexcept {
    return std::pow(10.0F, db / 20.0F);
}

float time_coefficient(float milliseconds, float sample_rate) noexcept {
    if (milliseconds <= 0.0F || sample_rate <= 0.0F) {
        return 0.0F;
    }
    const float seconds = milliseconds * 0.001F;
    return std::exp(-1.0F / (seconds * sample_rate));
}

} // namespace

Gain::Gain(float gain_db, float sample_rate, float smoothing_ms) noexcept
    : gain_db_(0.0F), gain_(1.0F), target_gain_(1.0F), ramp_step_(0.0F),
      sample_rate_(48000.0F), smoothing_ms_(10.0F) {
    configure_smoothing(sample_rate, smoothing_ms);
    set_gain_db_immediate(gain_db);
}

void Gain::set_gain_db(float gain_db) noexcept {
    if (std::isfinite(gain_db)) {
        gain_db_ = std::clamp(gain_db, kMinGainDb, kMaxGainDb);
    }
    target_gain_ = decibels_to_linear(gain_db_);
    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(smoothing_ms_) / 1000.0;
    const std::size_t ramp_frames = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_frames == 0) {
        gain_ = target_gain_;
        ramp_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }
    ramp_step_ = (target_gain_ - gain_) / static_cast<float>(ramp_frames);
    ramp_remaining_ = ramp_frames;
}

void Gain::set_gain_db_immediate(float gain_db) noexcept {
    if (std::isfinite(gain_db)) {
        gain_db_ = std::clamp(gain_db, kMinGainDb, kMaxGainDb);
    }
    target_gain_ = decibels_to_linear(gain_db_);
    gain_ = target_gain_;
    ramp_step_ = 0.0F;
    ramp_remaining_ = 0;
}

void Gain::process(std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels == 0) {
        return;
    }
    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (ramp_remaining_ > 0) {
            gain_ += ramp_step_;
            --ramp_remaining_;
            if (ramp_remaining_ == 0) {
                gain_ = target_gain_;
                ramp_step_ = 0.0F;
            }
        }
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            interleaved[offset + channel] *= gain_;
        }
    }
}

void Gain::reset() noexcept {
    gain_ = target_gain_;
    ramp_step_ = 0.0F;
    ramp_remaining_ = 0;
}

void Gain::configure_smoothing(float sample_rate, float smoothing_ms) noexcept {
    sample_rate_ = std::isfinite(sample_rate) && sample_rate >= 8000.0F &&
                           sample_rate <= 384000.0F
                       ? sample_rate
                       : 48000.0F;
    smoothing_ms_ = std::isfinite(smoothing_ms)
                        ? std::clamp(smoothing_ms, 0.0F, 1000.0F)
                        : 10.0F;
}

NoiseGate::NoiseGate(float sample_rate, float threshold_db, float attenuation_db,
                     float attack_ms, float release_ms) noexcept
    : sample_rate_(sample_rate), threshold_(0.0F), close_threshold_(0.0F),
      closed_gain_(0.0F),
      attack_coefficient_(0.0F), release_coefficient_(0.0F) {
    configure(sample_rate, threshold_db, attenuation_db, attack_ms, release_ms);
}

NoiseGate::NoiseGate(float sample_rate, NoiseGateParameters parameters) noexcept
    : sample_rate_(sample_rate), threshold_(0.0F), close_threshold_(0.0F),
      closed_gain_(0.0F), attack_coefficient_(0.0F), release_coefficient_(0.0F) {
    configure(sample_rate, parameters);
}

void NoiseGate::configure(float sample_rate, float threshold_db, float attenuation_db,
                          float attack_ms, float release_ms) noexcept {
    NoiseGateParameters parameters;
    parameters.threshold_db = threshold_db;
    parameters.attenuation_db = attenuation_db;
    parameters.attack_ms = attack_ms;
    parameters.release_ms = release_ms;
    configure(sample_rate, parameters);
}

void NoiseGate::configure(float sample_rate, NoiseGateParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) ? std::clamp(sample_rate, 1.0F, 384000.0F)
                                             : 48000.0F;
    const float threshold_db = std::isfinite(parameters.threshold_db)
                                   ? std::clamp(parameters.threshold_db, -120.0F, 0.0F)
                                   : -60.0F;
    const float attenuation_db = std::isfinite(parameters.attenuation_db)
                                     ? std::clamp(parameters.attenuation_db, -96.0F, 0.0F)
                                     : -72.0F;
    const float hysteresis_db = std::isfinite(parameters.hysteresis_db)
                                    ? std::clamp(parameters.hysteresis_db, 0.0F, 24.0F)
                                    : 3.0F;
    const float attack_ms = std::isfinite(parameters.attack_ms)
                                ? std::clamp(parameters.attack_ms, 0.0F, 2000.0F)
                                : 2.0F;
    const float hold_ms = std::isfinite(parameters.hold_ms)
                              ? std::clamp(parameters.hold_ms, 0.0F, 5000.0F)
                              : 20.0F;
    const float release_ms = std::isfinite(parameters.release_ms)
                                 ? std::clamp(parameters.release_ms, 0.0F, 5000.0F)
                                 : 80.0F;

    threshold_ = decibels_to_linear(threshold_db);
    close_threshold_ = decibels_to_linear(threshold_db - hysteresis_db);
    closed_gain_ = decibels_to_linear(attenuation_db);
    attack_coefficient_ = time_coefficient(attack_ms, sample_rate_);
    release_coefficient_ = time_coefficient(release_ms, sample_rate_);
    const double hold_samples = static_cast<double>(sample_rate_) *
                                static_cast<double>(hold_ms) / 1000.0;
    hold_duration_samples_ = static_cast<std::size_t>(std::llround(hold_samples));
    hold_remaining_samples_ = std::min(hold_remaining_samples_, hold_duration_samples_);
}

void NoiseGate::process(std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels == 0) {
        return;
    }
    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        float peak = 0.0F;
        const std::size_t offset = frame * channels;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const float sample = interleaved[offset + channel];
            if (std::isfinite(sample)) {
                peak = std::max(peak, std::abs(sample));
            }
        }
        const float envelope_coefficient = peak > envelope_ ? attack_coefficient_ : release_coefficient_;
        envelope_ = envelope_coefficient * envelope_ + (1.0F - envelope_coefficient) * peak;
        if (!gate_open_) {
            if (envelope_ >= threshold_) {
                gate_open_ = true;
                hold_remaining_samples_ = hold_duration_samples_;
            }
        } else if (envelope_ >= close_threshold_) {
            hold_remaining_samples_ = hold_duration_samples_;
        } else if (hold_remaining_samples_ > 0) {
            --hold_remaining_samples_;
        } else {
            gate_open_ = false;
        }

        const float target_gain = gate_open_ ? 1.0F : closed_gain_;
        const float gain_coefficient = target_gain > gain_ ? attack_coefficient_ : release_coefficient_;
        gain_ = gain_coefficient * gain_ + (1.0F - gain_coefficient) * target_gain;
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const std::size_t index = offset + channel;
            interleaved[index] = std::isfinite(interleaved[index])
                                     ? interleaved[index] * gain_
                                     : 0.0F;
        }
    }
}

void NoiseGate::reset() noexcept {
    envelope_ = 0.0F;
    gain_ = closed_gain_;
    hold_remaining_samples_ = 0;
    gate_open_ = false;
}

SoftDrive::SoftDrive(float drive) noexcept : drive_(1.0F), normalization_(1.0F) {
    set_drive(drive);
}

void SoftDrive::set_drive(float drive) noexcept {
    drive_ = std::clamp(drive, 1.0F, 20.0F);
    normalization_ = std::tanh(drive_);
}

void SoftDrive::process(std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels == 0 || drive_ <= 1.0001F) {
        return;
    }
    for (float& sample : interleaved) {
        sample = std::tanh(sample * drive_) / normalization_;
    }
}

Delay::Delay(float sample_rate, std::uint16_t channels, float max_delay_ms,
             float delay_ms, float feedback, float mix, bool reserve_dual_line)
    : sample_rate_(sample_rate), channels_(channels),
      storage_channels_(channels == 1 && reserve_dual_line ? 2 : channels),
      dual_capable_(reserve_dual_line), delay_samples_(1), feedback_(0.0F),
      target_feedback_(0.0F), mix_(0.0F), target_mix_(0.0F), write_frame_(0),
      capacity_frames_(0) {
    if (!std::isfinite(sample_rate_) || sample_rate_ <= 0.0F ||
        sample_rate_ > 384000.0F || channels_ == 0 || channels_ > 2 ||
        !std::isfinite(max_delay_ms) || max_delay_ms <= 0.0F) {
        throw std::invalid_argument(
            "Delay requires a finite supported sample rate, mono/stereo layout, and positive maximum delay");
    }
    const double requested_frames = std::ceil(static_cast<double>(sample_rate_) *
                                              static_cast<double>(max_delay_ms) / 1000.0);
    if (!std::isfinite(requested_frames) || requested_frames < 1.0 ||
        requested_frames >= static_cast<double>(std::numeric_limits<std::size_t>::max() - 1)) {
        throw std::length_error("Delay buffer size is too large");
    }
    const std::size_t maximum_frames = static_cast<std::size_t>(requested_frames);
    capacity_frames_ = maximum_frames + 1;
    if (capacity_frames_ > storage_.max_size() / storage_channels_) {
        throw std::length_error("Delay buffer size is too large");
    }
    storage_.assign(capacity_frames_ * storage_channels_, 0.0F);
    const double ramp_frames = std::ceil(static_cast<double>(sample_rate_) * 0.010);
    const std::size_t ramp_samples = ramp_frames >= 1.0
                                         ? static_cast<std::size_t>(ramp_frames)
                                         : 0;
    parameter_ramp_total_ = std::max<std::size_t>(ramp_samples, 1);
    delay_transition_total_ = parameter_ramp_total_;
    const float bbd_cutoff_hz = std::min(4500.0F, sample_rate_ * 0.45F);
    bbd_filter_alpha_ = 1.0F - std::exp(
        -6.2831853071795864769F * bbd_cutoff_hz / sample_rate_);
    target_tape_wow_depth_samples_ = 0.35F * sample_rate_ / 1000.0F;
    tape_wow_depth_samples_ = target_tape_wow_depth_samples_;
    target_tape_flutter_depth_samples_ = 0.08F * sample_rate_ / 1000.0F;
    tape_flutter_depth_samples_ = target_tape_flutter_depth_samples_;
    target_tape_wow_phase_increment_ =
        6.2831853071795864769F * 0.3F / sample_rate_;
    tape_wow_phase_increment_ = target_tape_wow_phase_increment_;
    target_tape_flutter_phase_increment_ =
        6.2831853071795864769F * 6.0F / sample_rate_;
    tape_flutter_phase_increment_ = target_tape_flutter_phase_increment_;
    configure(delay_ms, feedback, mix);
    dual_delay_samples_ = {delay_samples_, delay_samples_};
    dual_transition_from_delay_samples_ = {
        static_cast<float>(delay_samples_), static_cast<float>(delay_samples_)};
    target_dual_feedback_ = {target_feedback_, target_feedback_};
    dual_feedback_ = {target_feedback_, target_feedback_};
    target_dual_mix_ = {target_mix_, target_mix_};
    dual_mix_ = {target_mix_, target_mix_};
    constexpr std::array<float, maximum_delay_taps> default_times{250.0F, 500.0F,
                                                                  750.0F, 1000.0F};
    constexpr std::array<float, maximum_delay_taps> default_levels{0.65F, 0.45F,
                                                                   0.30F, 0.20F};
    constexpr std::array<float, maximum_delay_taps> default_pans{-0.55F, 0.55F,
                                                                  -0.25F, 0.25F};
    for (std::size_t tap = 0; tap < maximum_delay_taps; ++tap) {
        configure_multitap_tap(tap, default_times[tap], default_levels[tap],
                               default_pans[tap]);
    }
    reset();
}

void Delay::configure(float delay_ms, float feedback, float mix) noexcept {
    if (std::isfinite(delay_ms)) set_delay_samples(samples_for_ms(delay_ms));
    if (std::isfinite(feedback)) target_feedback_ = std::clamp(feedback, 0.0F, 0.95F);
    if (std::isfinite(mix)) target_mix_ = std::clamp(mix, 0.0F, 1.0F);
    time_mode_ = DelayTimeMode::milliseconds;
    retarget_parameters();
}

void Delay::configure_bbd_style(bool enabled) noexcept {
    if (enabled) {
        target_tape_mix_ = 0.0F;
        tape_mix_step_ = (target_tape_mix_ - tape_mix_) /
                         static_cast<float>(std::max<std::size_t>(parameter_ramp_total_, 1));
        tape_ramp_remaining_ = parameter_ramp_total_;
    }
    target_bbd_mix_ = enabled ? 1.0F : 0.0F;
    if (parameter_ramp_total_ <= 1) {
        bbd_mix_ = target_bbd_mix_;
        bbd_mix_step_ = 0.0F;
        bbd_ramp_remaining_ = 0;
        return;
    }
    bbd_mix_step_ = (target_bbd_mix_ - bbd_mix_) /
                    static_cast<float>(parameter_ramp_total_);
    bbd_ramp_remaining_ = parameter_ramp_total_;
}

void Delay::configure_tape_echo(bool enabled, float saturation, float wow_depth_ms,
                                float wow_rate_hz, float flutter_depth_ms,
                                float flutter_rate_hz) noexcept {
    if (enabled) configure_bbd_style(false);
    if (std::isfinite(saturation)) {
        target_tape_saturation_ = std::clamp(saturation, 0.0F, 1.0F);
    }
    if (std::isfinite(wow_depth_ms)) {
        target_tape_wow_depth_samples_ = std::clamp(wow_depth_ms, 0.0F, 5.0F) *
                                         sample_rate_ / 1000.0F;
    }
    if (std::isfinite(flutter_depth_ms)) {
        target_tape_flutter_depth_samples_ = std::clamp(flutter_depth_ms, 0.0F, 1.0F) *
                                              sample_rate_ / 1000.0F;
    }
    if (std::isfinite(wow_rate_hz)) {
        const float rate = std::clamp(wow_rate_hz, 0.05F, 2.0F);
        target_tape_wow_phase_increment_ = 6.2831853071795864769F *
            std::min(rate, sample_rate_ * 0.45F) / sample_rate_;
    }
    if (std::isfinite(flutter_rate_hz)) {
        const float rate = std::clamp(flutter_rate_hz, 1.0F, 20.0F);
        target_tape_flutter_phase_increment_ = 6.2831853071795864769F *
            std::min(rate, sample_rate_ * 0.45F) / sample_rate_;
    }
    target_tape_mix_ = enabled ? 1.0F : 0.0F;
    if (!enabled && tape_mix_ <= 0.0F) {
        tape_mix_ = 0.0F;
        tape_mix_step_ = 0.0F;
        tape_ramp_remaining_ = 0;
        tape_saturation_ = target_tape_saturation_;
        tape_wow_depth_samples_ = target_tape_wow_depth_samples_;
        tape_flutter_depth_samples_ = target_tape_flutter_depth_samples_;
        tape_wow_phase_increment_ = target_tape_wow_phase_increment_;
        tape_flutter_phase_increment_ = target_tape_flutter_phase_increment_;
        tape_parameter_ramp_remaining_ = 0;
        tape_saturation_step_ = 0.0F;
        tape_wow_depth_step_ = 0.0F;
        tape_flutter_depth_step_ = 0.0F;
        tape_wow_phase_step_ = 0.0F;
        tape_flutter_phase_step_ = 0.0F;
        return;
    }

    const float ramp = static_cast<float>(std::max<std::size_t>(parameter_ramp_total_, 1));
    tape_mix_step_ = (target_tape_mix_ - tape_mix_) / ramp;
    tape_ramp_remaining_ = parameter_ramp_total_;
    tape_saturation_step_ = (target_tape_saturation_ - tape_saturation_) / ramp;
    tape_wow_depth_step_ =
        (target_tape_wow_depth_samples_ - tape_wow_depth_samples_) / ramp;
    tape_flutter_depth_step_ =
        (target_tape_flutter_depth_samples_ - tape_flutter_depth_samples_) / ramp;
    tape_wow_phase_step_ =
        (target_tape_wow_phase_increment_ - tape_wow_phase_increment_) / ramp;
    tape_flutter_phase_step_ =
        (target_tape_flutter_phase_increment_ - tape_flutter_phase_increment_) / ramp;
    tape_parameter_ramp_remaining_ = parameter_ramp_total_;
    if (parameter_ramp_total_ <= 1) {
        tape_mix_ = target_tape_mix_;
        tape_mix_step_ = 0.0F;
        tape_ramp_remaining_ = 0;
        tape_saturation_ = target_tape_saturation_;
        tape_wow_depth_samples_ = target_tape_wow_depth_samples_;
        tape_flutter_depth_samples_ = target_tape_flutter_depth_samples_;
        tape_wow_phase_increment_ = target_tape_wow_phase_increment_;
        tape_flutter_phase_increment_ = target_tape_flutter_phase_increment_;
        tape_saturation_step_ = 0.0F;
        tape_wow_depth_step_ = 0.0F;
        tape_flutter_depth_step_ = 0.0F;
        tape_wow_phase_step_ = 0.0F;
        tape_flutter_phase_step_ = 0.0F;
        tape_parameter_ramp_remaining_ = 0;
    }
}

void Delay::configure_repeat_filters(bool enabled, float low_cut_hz,
                                     float high_cut_hz) noexcept {
    const float low_max = std::max(20.0F, std::min(1000.0F, sample_rate_ * 0.15F));
    const float high_max = std::max(20.0F, std::min(20000.0F, sample_rate_ * 0.45F));
    const float high_min = std::min(2000.0F, high_max);
    const float safe_low = std::isfinite(low_cut_hz)
        ? std::clamp(low_cut_hz, 20.0F, low_max)
        : 120.0F;
    const float safe_high = std::isfinite(high_cut_hz)
        ? std::clamp(high_cut_hz, high_min, high_max)
        : std::clamp(6000.0F, high_min, high_max);
    constexpr float two_pi = 6.2831853071795864769F;
    target_repeat_highpass_alpha_ = std::exp(-two_pi * safe_low / sample_rate_);
    target_repeat_lowpass_alpha_ = 1.0F -
        std::exp(-two_pi * safe_high / sample_rate_);
    target_repeat_filter_mix_ = enabled ? 1.0F : 0.0F;
    if (parameter_ramp_total_ <= 1) {
        repeat_filter_mix_ = target_repeat_filter_mix_;
        repeat_highpass_alpha_ = target_repeat_highpass_alpha_;
        repeat_lowpass_alpha_ = target_repeat_lowpass_alpha_;
        repeat_filter_mix_step_ = 0.0F;
        repeat_highpass_alpha_step_ = 0.0F;
        repeat_lowpass_alpha_step_ = 0.0F;
        repeat_filter_ramp_remaining_ = 0;
        return;
    }
    const float count = static_cast<float>(parameter_ramp_total_);
    repeat_filter_mix_step_ = (target_repeat_filter_mix_ - repeat_filter_mix_) / count;
    repeat_highpass_alpha_step_ =
        (target_repeat_highpass_alpha_ - repeat_highpass_alpha_) / count;
    repeat_lowpass_alpha_step_ =
        (target_repeat_lowpass_alpha_ - repeat_lowpass_alpha_) / count;
    repeat_filter_ramp_remaining_ = parameter_ramp_total_;
}

void Delay::configure_ping_pong(bool enabled) noexcept {
    if (target_ping_pong_ == enabled && !target_dual_ && !target_multitap_) return;
    target_ping_pong_ = enabled;
    target_dual_ = false;
    target_multitap_ = false;
    const float ping_target = enabled ? 1.0F : 0.0F;
    if (parameter_ramp_total_ <= 1) {
        ping_pong_mix_ = ping_target;
        ping_pong_step_ = 0.0F;
        ping_pong_ramp_remaining_ = 0;
        dual_routing_mix_ = 0.0F;
        dual_routing_step_ = 0.0F;
        dual_routing_ramp_remaining_ = 0;
        multitap_route_mix_ = 0.0F;
        multitap_route_step_ = 0.0F;
        multitap_route_ramp_remaining_ = 0;
        return;
    }
    ping_pong_step_ = (ping_target - ping_pong_mix_) /
                      static_cast<float>(parameter_ramp_total_);
    ping_pong_ramp_remaining_ = parameter_ramp_total_;
    dual_routing_step_ = (0.0F - dual_routing_mix_) /
                         static_cast<float>(parameter_ramp_total_);
    dual_routing_ramp_remaining_ = parameter_ramp_total_;
    multitap_route_step_ = (0.0F - multitap_route_mix_) /
                           static_cast<float>(parameter_ramp_total_);
    multitap_route_ramp_remaining_ = parameter_ramp_total_;
}

void Delay::configure_dual(float delay_a_ms, float feedback_a, float mix_a,
                           float delay_b_ms, float feedback_b, float mix_b) noexcept {
    if (!dual_capable_) return;
    if (std::isfinite(delay_a_ms)) {
        set_dual_delay_samples(0, samples_for_ms(delay_a_ms));
    }
    if (std::isfinite(delay_b_ms)) {
        set_dual_delay_samples(1, samples_for_ms(delay_b_ms));
    }
    if (std::isfinite(feedback_a)) target_dual_feedback_[0] = std::clamp(feedback_a, 0.0F, 0.95F);
    if (std::isfinite(feedback_b)) target_dual_feedback_[1] = std::clamp(feedback_b, 0.0F, 0.95F);
    if (std::isfinite(mix_a)) target_dual_mix_[0] = std::clamp(mix_a, 0.0F, 1.0F);
    if (std::isfinite(mix_b)) target_dual_mix_[1] = std::clamp(mix_b, 0.0F, 1.0F);
    dual_parameter_ramp_remaining_ = parameter_ramp_total_;
    const float ramp = static_cast<float>(parameter_ramp_total_);
    for (std::size_t line = 0; line < 2; ++line) {
        dual_feedback_step_[line] =
            (target_dual_feedback_[line] - dual_feedback_[line]) / ramp;
        dual_mix_step_[line] = (target_dual_mix_[line] - dual_mix_[line]) / ramp;
    }

    target_ping_pong_ = false;
    target_dual_ = true;
    target_multitap_ = false;
    if (parameter_ramp_total_ <= 1) {
        ping_pong_mix_ = 0.0F;
        ping_pong_step_ = 0.0F;
        ping_pong_ramp_remaining_ = 0;
        dual_routing_mix_ = 1.0F;
        dual_routing_step_ = 0.0F;
        dual_routing_ramp_remaining_ = 0;
        multitap_route_mix_ = 0.0F;
        multitap_route_step_ = 0.0F;
        multitap_route_ramp_remaining_ = 0;
        return;
    }
    ping_pong_step_ = (0.0F - ping_pong_mix_) / ramp;
    ping_pong_ramp_remaining_ = parameter_ramp_total_;
    dual_routing_step_ = (1.0F - dual_routing_mix_) / ramp;
    dual_routing_ramp_remaining_ = parameter_ramp_total_;
    multitap_route_step_ = (0.0F - multitap_route_mix_) / ramp;
    multitap_route_ramp_remaining_ = parameter_ramp_total_;
}

void Delay::configure_multitap(bool enabled, float feedback, float mix) noexcept {
    if (std::isfinite(feedback)) target_feedback_ = std::clamp(feedback, 0.0F, 0.95F);
    if (std::isfinite(mix)) target_mix_ = std::clamp(mix, 0.0F, 1.0F);
    retarget_parameters();

    target_multitap_ = enabled;
    target_ping_pong_ = false;
    target_dual_ = false;
    const float ramp = static_cast<float>(std::max<std::size_t>(parameter_ramp_total_, 1));
    if (parameter_ramp_total_ <= 1) {
        ping_pong_mix_ = 0.0F;
        ping_pong_step_ = 0.0F;
        ping_pong_ramp_remaining_ = 0;
        dual_routing_mix_ = 0.0F;
        dual_routing_step_ = 0.0F;
        dual_routing_ramp_remaining_ = 0;
        multitap_route_mix_ = enabled ? 1.0F : 0.0F;
        multitap_route_step_ = 0.0F;
        multitap_route_ramp_remaining_ = 0;
        return;
    }
    ping_pong_step_ = (0.0F - ping_pong_mix_) / ramp;
    ping_pong_ramp_remaining_ = parameter_ramp_total_;
    dual_routing_step_ = (0.0F - dual_routing_mix_) / ramp;
    dual_routing_ramp_remaining_ = parameter_ramp_total_;
    multitap_route_step_ = ((enabled ? 1.0F : 0.0F) - multitap_route_mix_) / ramp;
    multitap_route_ramp_remaining_ = parameter_ramp_total_;
}

void Delay::configure_multitap_tap(std::size_t tap, float delay_ms,
                                   float level, float pan) noexcept {
    if (tap >= maximum_delay_taps) return;
    if (std::isfinite(delay_ms)) {
        const std::size_t samples = samples_for_ms(delay_ms);
        if (samples != multitap_delay_samples_[tap]) {
            multitap_transition_from_delay_samples_[tap] =
                static_cast<float>(multitap_delay_samples_[tap]);
            multitap_delay_samples_[tap] = samples;
            multitap_delay_transition_remaining_[tap] = parameter_ramp_total_;
        }
    }
    if (std::isfinite(level)) target_multitap_level_[tap] = std::clamp(level, 0.0F, 1.0F);
    if (std::isfinite(pan)) {
        const float clamped_pan = std::clamp(pan, -1.0F, 1.0F);
        target_multitap_pan_left_[tap] = std::sqrt(0.5F * (1.0F - clamped_pan));
        target_multitap_pan_right_[tap] = std::sqrt(0.5F * (1.0F + clamped_pan));
    }
    multitap_parameter_ramp_remaining_[tap] = parameter_ramp_total_;
    const float ramp = static_cast<float>(std::max<std::size_t>(parameter_ramp_total_, 1));
    multitap_level_step_[tap] =
        (target_multitap_level_[tap] - multitap_level_[tap]) / ramp;
    multitap_pan_left_step_[tap] =
        (target_multitap_pan_left_[tap] - multitap_pan_left_[tap]) / ramp;
    multitap_pan_right_step_[tap] =
        (target_multitap_pan_right_[tap] - multitap_pan_right_[tap]) / ramp;
    if (parameter_ramp_total_ <= 1) {
        multitap_level_[tap] = target_multitap_level_[tap];
        multitap_pan_left_[tap] = target_multitap_pan_left_[tap];
        multitap_pan_right_[tap] = target_multitap_pan_right_[tap];
        multitap_level_step_[tap] = 0.0F;
        multitap_pan_left_step_[tap] = 0.0F;
        multitap_pan_right_step_[tap] = 0.0F;
        multitap_parameter_ramp_remaining_[tap] = 0;
        multitap_delay_transition_remaining_[tap] = 0;
    }
}

void Delay::configure_tape_heads(bool enabled, float head_spacing_ms,
                                 std::uint8_t selected_heads) noexcept {
    constexpr std::uint8_t head_mask = 0x0FU;
    constexpr std::array<float, maximum_delay_taps> head_pans{
        -0.55F, 0.55F, -0.55F, 0.55F};
    const std::uint8_t selected = static_cast<std::uint8_t>(selected_heads & head_mask);
    std::size_t selected_count = 0;
    for (std::size_t head = 0; head < maximum_delay_taps; ++head) {
        if ((selected & static_cast<std::uint8_t>(1U << head)) != 0) ++selected_count;
    }

    const bool active = enabled && selected_count != 0;
    configure_multitap(active, target_feedback_, target_mix_);

    const float requested_spacing_ms = std::isfinite(head_spacing_ms)
                                           ? head_spacing_ms
                                           : 110.0F;
    const float maximum_delay_ms = static_cast<float>(capacity_frames_ - 1) *
                                  1000.0F / sample_rate_;
    const float maximum_spacing_ms = std::max(maximum_delay_ms /
                                                  static_cast<float>(maximum_delay_taps),
                                              1000.0F / sample_rate_);
    const float spacing_ms = std::clamp(requested_spacing_ms, 1000.0F / sample_rate_,
                                        maximum_spacing_ms);
    const float level = selected_count == 0
                            ? 0.0F
                            : 1.0F / static_cast<float>(selected_count);
    for (std::size_t head = 0; head < maximum_delay_taps; ++head) {
        const bool selected_head =
            (selected & static_cast<std::uint8_t>(1U << head)) != 0;
        const float head_delay_ms = spacing_ms * static_cast<float>(head + 1);
        configure_multitap_tap(head, head_delay_ms,
                               active && selected_head ? level : 0.0F,
                               head_pans[head]);
    }
}

void Delay::configure_tempo(float bpm, DelayDivision division, float feedback,
                            float mix) noexcept {
    tempo_bpm_ = std::isfinite(bpm) ? std::clamp(bpm, 20.0F, 300.0F) : 120.0F;
    switch (division) {
    case DelayDivision::whole:
    case DelayDivision::dotted_half:
    case DelayDivision::half:
    case DelayDivision::dotted_quarter:
    case DelayDivision::quarter:
    case DelayDivision::quarter_triplet:
    case DelayDivision::dotted_eighth:
    case DelayDivision::eighth:
    case DelayDivision::eighth_triplet:
    case DelayDivision::dotted_sixteenth:
    case DelayDivision::sixteenth:
    case DelayDivision::sixteenth_triplet:
        division_ = division;
        break;
    default:
        division_ = DelayDivision::quarter;
        break;
    }

    const float beats = beats_for_division(division_);
    const float delay_ms = (60000.0F / tempo_bpm_) * beats;
    set_delay_samples(samples_for_ms(delay_ms));
    if (std::isfinite(feedback)) target_feedback_ = std::clamp(feedback, 0.0F, 0.95F);
    if (std::isfinite(mix)) target_mix_ = std::clamp(mix, 0.0F, 1.0F);
    time_mode_ = DelayTimeMode::tempo_sync;
    retarget_parameters();
}

std::size_t Delay::samples_for_ms(float delay_ms) const noexcept {
    const std::size_t maximum_samples = capacity_frames_ - 1;
    const double minimum_ms_samples = std::ceil(static_cast<double>(sample_rate_) / 1000.0);
    const std::size_t minimum_samples = std::min(
        maximum_samples,
        std::max<std::size_t>(1, static_cast<std::size_t>(minimum_ms_samples)));
    const double requested = static_cast<double>(delay_ms) *
                             static_cast<double>(sample_rate_) / 1000.0;
    if (!std::isfinite(requested) || requested >= static_cast<double>(maximum_samples)) {
        return maximum_samples;
    }
    if (requested <= static_cast<double>(minimum_samples)) return minimum_samples;
    return std::clamp(static_cast<std::size_t>(std::llround(requested)),
                      minimum_samples, maximum_samples);
}

float Delay::beats_for_division(DelayDivision division) noexcept {
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
    }
    return 1.0F;
}

void Delay::set_delay_samples(std::size_t samples) noexcept {
    samples = std::clamp(samples, std::size_t{1}, capacity_frames_ - 1);
    if (samples == delay_samples_) return;

    if (delay_transition_remaining_ > 0) {
        const float progress = static_cast<float>(delay_transition_total_ -
                                                  delay_transition_remaining_) /
                              static_cast<float>(delay_transition_total_);
        transition_from_delay_samples_ +=
            (static_cast<float>(delay_samples_) - transition_from_delay_samples_) * progress;
    } else {
        transition_from_delay_samples_ = static_cast<float>(delay_samples_);
    }
    delay_samples_ = samples;
    delay_transition_remaining_ = delay_transition_total_;
}

void Delay::set_dual_delay_samples(std::uint16_t line, std::size_t samples) noexcept {
    if (line >= dual_delay_samples_.size()) return;
    samples = std::clamp(samples, std::size_t{1}, capacity_frames_ - 1);
    if (samples == dual_delay_samples_[line]) return;

    if (dual_delay_transition_remaining_[line] > 0) {
        const float progress = static_cast<float>(delay_transition_total_ -
                                                  dual_delay_transition_remaining_[line]) /
                              static_cast<float>(delay_transition_total_);
        dual_transition_from_delay_samples_[line] +=
            (static_cast<float>(dual_delay_samples_[line]) -
             dual_transition_from_delay_samples_[line]) * progress;
    } else {
        dual_transition_from_delay_samples_[line] =
            static_cast<float>(dual_delay_samples_[line]);
    }
    dual_delay_samples_[line] = samples;
    dual_delay_transition_remaining_[line] = delay_transition_total_;
}

float Delay::read_delay(std::size_t delay_samples, std::uint16_t channel) const noexcept {
    if (tape_modulation_samples_ != 0.0F) {
        return read_fractional_delay(static_cast<float>(delay_samples), channel);
    }
    const std::size_t read_frame = write_frame_ >= delay_samples
                                       ? write_frame_ - delay_samples
                                       : write_frame_ + capacity_frames_ - delay_samples;
    return storage_[read_frame * storage_channels_ + channel];
}

float Delay::read_fractional_delay(float delay_samples, std::uint16_t channel) const noexcept {
    const double maximum_delay = static_cast<double>(capacity_frames_ - 1);
    const double effective_delay = std::clamp(
        static_cast<double>(delay_samples) + static_cast<double>(tape_modulation_samples_),
        1.0, maximum_delay);
    double read_position = static_cast<double>(write_frame_) -
                           effective_delay;
    if (read_position < 0.0) read_position += static_cast<double>(capacity_frames_);
    const std::size_t first_frame = static_cast<std::size_t>(read_position);
    const std::size_t second_frame = first_frame + 1 == capacity_frames_ ? 0 : first_frame + 1;
    const double fraction = read_position - static_cast<double>(first_frame);
    const std::size_t first_index = first_frame * storage_channels_ + channel;
    const std::size_t second_index = second_frame * storage_channels_ + channel;
    const double first = storage_[first_index];
    const double second = storage_[second_index];
    return static_cast<float>(first + (second - first) * fraction);
}

float Delay::shape_delay_feedback(float value, std::uint16_t channel) noexcept {
    if (channel >= bbd_filter_state_.size() || !std::isfinite(value)) {
        return std::isfinite(value) ? value : 0.0F;
    }
    const double input = static_cast<double>(value);
    double shaped = input;
    if (bbd_mix_ > 0.0F || bbd_ramp_remaining_ > 0) {
        const double prior = static_cast<double>(bbd_filter_state_[channel]);
        const double filtered_value = prior + static_cast<double>(bbd_filter_alpha_) *
                                                     (input - prior);
        const double bounded_filter = std::clamp(
            filtered_value,
            -static_cast<double>(std::numeric_limits<float>::max()),
            static_cast<double>(std::numeric_limits<float>::max()));
        const float filtered = static_cast<float>(bounded_filter);
        bbd_filter_state_[channel] = filtered;
        const double saturated = static_cast<double>(filtered) /
            (1.0 + 0.08 * std::abs(static_cast<double>(filtered)));
        const double blend = static_cast<double>(std::clamp(bbd_mix_, 0.0F, 1.0F));
        shaped = input + (saturated - input) * blend;
    } else {
        bbd_filter_state_[channel] = value;
    }
    if (tape_mix_ > 0.0F || tape_ramp_remaining_ > 0) {
        const double saturated = shaped /
            (1.0 + 0.65 * static_cast<double>(tape_saturation_) * std::abs(shaped));
        shaped += (saturated - shaped) *
                  static_cast<double>(std::clamp(tape_mix_, 0.0F, 1.0F));
    }
    if (repeat_filter_mix_ > 0.0F || repeat_filter_ramp_remaining_ > 0) {
        const double high_passed = static_cast<double>(repeat_highpass_alpha_) *
            (static_cast<double>(repeat_highpass_previous_output_[channel]) + shaped -
             static_cast<double>(repeat_highpass_previous_input_[channel]));
        const double maximum = static_cast<double>(std::numeric_limits<float>::max());
        const float high_pass = static_cast<float>(std::clamp(high_passed, -maximum, maximum));
        repeat_highpass_previous_input_[channel] = static_cast<float>(
            std::clamp(shaped, -maximum, maximum));
        repeat_highpass_previous_output_[channel] = high_pass;
        const double low_passed = static_cast<double>(repeat_lowpass_state_[channel]) +
            static_cast<double>(repeat_lowpass_alpha_) *
                (static_cast<double>(high_pass) - repeat_lowpass_state_[channel]);
        const float low_pass = static_cast<float>(std::clamp(low_passed, -maximum, maximum));
        repeat_lowpass_state_[channel] = low_pass;
        const double blend = static_cast<double>(std::clamp(repeat_filter_mix_, 0.0F, 1.0F));
        shaped += (static_cast<double>(low_pass) - shaped) * blend;
    }
    return static_cast<float>(shaped);
}

void Delay::retarget_parameters() noexcept {
    if (parameter_ramp_total_ <= 1) {
        feedback_ = target_feedback_;
        mix_ = target_mix_;
        feedback_step_ = 0.0F;
        mix_step_ = 0.0F;
        parameter_ramp_remaining_ = 0;
        return;
    }
    parameter_ramp_remaining_ = parameter_ramp_total_;
    const float count = static_cast<float>(parameter_ramp_total_);
    feedback_step_ = (target_feedback_ - feedback_) / count;
    mix_step_ = (target_mix_ - mix_) / count;
}

void Delay::advance_ramps() noexcept {
    if (parameter_ramp_remaining_ > 0) {
        feedback_ += feedback_step_;
        mix_ += mix_step_;
        --parameter_ramp_remaining_;
        if (parameter_ramp_remaining_ == 0) {
            feedback_ = target_feedback_;
            mix_ = target_mix_;
            feedback_step_ = 0.0F;
            mix_step_ = 0.0F;
        }
    }
    if (bbd_ramp_remaining_ > 0) {
        bbd_mix_ += bbd_mix_step_;
        --bbd_ramp_remaining_;
        if (bbd_ramp_remaining_ == 0) {
            bbd_mix_ = target_bbd_mix_;
            bbd_mix_step_ = 0.0F;
        }
    }
    if (tape_ramp_remaining_ > 0) {
        tape_mix_ += tape_mix_step_;
        --tape_ramp_remaining_;
        if (tape_ramp_remaining_ == 0) {
            tape_mix_ = target_tape_mix_;
            tape_mix_step_ = 0.0F;
        }
    }
    if (tape_parameter_ramp_remaining_ > 0) {
        tape_saturation_ += tape_saturation_step_;
        tape_wow_depth_samples_ += tape_wow_depth_step_;
        tape_flutter_depth_samples_ += tape_flutter_depth_step_;
        tape_wow_phase_increment_ += tape_wow_phase_step_;
        tape_flutter_phase_increment_ += tape_flutter_phase_step_;
        --tape_parameter_ramp_remaining_;
        if (tape_parameter_ramp_remaining_ == 0) {
            tape_saturation_ = target_tape_saturation_;
            tape_wow_depth_samples_ = target_tape_wow_depth_samples_;
            tape_flutter_depth_samples_ = target_tape_flutter_depth_samples_;
            tape_wow_phase_increment_ = target_tape_wow_phase_increment_;
            tape_flutter_phase_increment_ = target_tape_flutter_phase_increment_;
            tape_saturation_step_ = 0.0F;
            tape_wow_depth_step_ = 0.0F;
            tape_flutter_depth_step_ = 0.0F;
            tape_wow_phase_step_ = 0.0F;
            tape_flutter_phase_step_ = 0.0F;
        }
    }
    if (repeat_filter_ramp_remaining_ > 0) {
        repeat_filter_mix_ += repeat_filter_mix_step_;
        repeat_highpass_alpha_ += repeat_highpass_alpha_step_;
        repeat_lowpass_alpha_ += repeat_lowpass_alpha_step_;
        --repeat_filter_ramp_remaining_;
        if (repeat_filter_ramp_remaining_ == 0) {
            repeat_filter_mix_ = target_repeat_filter_mix_;
            repeat_highpass_alpha_ = target_repeat_highpass_alpha_;
            repeat_lowpass_alpha_ = target_repeat_lowpass_alpha_;
            repeat_filter_mix_step_ = 0.0F;
            repeat_highpass_alpha_step_ = 0.0F;
            repeat_lowpass_alpha_step_ = 0.0F;
        }
    }
}

void Delay::process(std::span<float> interleaved, std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) {
        return;
    }
    const std::size_t frames = interleaved.size() / channels;
    const float maximum = std::numeric_limits<float>::max();
    const bool routed_path = ping_pong_mix_ != 0.0F || dual_routing_mix_ != 0.0F ||
        multitap_route_mix_ != 0.0F || ping_pong_ramp_remaining_ > 0 ||
        dual_routing_ramp_remaining_ > 0 || multitap_route_ramp_remaining_ > 0;
    const auto bound = [maximum](double value) noexcept {
        return static_cast<float>(std::clamp(
            value, -static_cast<double>(maximum), static_cast<double>(maximum)));
    };
    for (std::size_t frame = 0; frame < frames; ++frame) {
        tape_modulation_samples_ = 0.0F;
        if (tape_mix_ > 0.0F || tape_ramp_remaining_ > 0) {
            constexpr float two_pi = 6.2831853071795864769F;
            const float wow = tape_wow_depth_samples_ * std::sin(tape_wow_phase_);
            const float flutter = tape_flutter_depth_samples_ *
                                  std::sin(tape_flutter_phase_);
            tape_modulation_samples_ =
                (wow + flutter) * std::clamp(tape_mix_, 0.0F, 1.0F);
            tape_wow_phase_ += tape_wow_phase_increment_;
            tape_flutter_phase_ += tape_flutter_phase_increment_;
            if (tape_wow_phase_ >= two_pi) tape_wow_phase_ -= two_pi;
            if (tape_flutter_phase_ >= two_pi) tape_flutter_phase_ -= two_pi;
        }
        const float transition_progress = delay_transition_remaining_ == 0
            ? 1.0F
            : static_cast<float>(delay_transition_total_ - delay_transition_remaining_) /
                  static_cast<float>(delay_transition_total_);
        std::array<float, 2> multitap_feedback_taps{};
        std::array<float, 2> multitap_output{};
        const float multitap_weight = std::clamp(multitap_route_mix_, 0.0F, 1.0F);
        if (routed_path && (multitap_weight > 0.0F || multitap_route_ramp_remaining_ > 0)) {
            float feedback_level_sum = 0.0F;
            for (std::size_t tap = 0; tap < maximum_delay_taps; ++tap) {
                const float level = multitap_level_[tap];
                feedback_level_sum += level;
                std::array<float, 2> delayed{};
                for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                    const float new_tap = read_delay(multitap_delay_samples_[tap], channel);
                    delayed[channel] = new_tap;
                    const std::size_t remaining = multitap_delay_transition_remaining_[tap];
                    if (remaining > 0) {
                        const float progress = static_cast<float>(delay_transition_total_ - remaining) /
                                               static_cast<float>(delay_transition_total_);
                        const float old_tap = read_fractional_delay(
                            multitap_transition_from_delay_samples_[tap], channel);
                        delayed[channel] = old_tap + (new_tap - old_tap) * progress;
                    }
                    multitap_feedback_taps[channel] += delayed[channel] * level;
                }
                if (channels_ == 2) {
                    const float mono_tap = 0.5F * (delayed[0] + delayed[1]) * level;
                    multitap_output[0] += mono_tap * multitap_pan_left_[tap];
                    multitap_output[1] += mono_tap * multitap_pan_right_[tap];
                } else {
                    multitap_output[0] += delayed[0] * level;
                }
            }
            if (feedback_level_sum > 1.0e-6F) {
                for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                    multitap_feedback_taps[channel] /= feedback_level_sum;
                }
            }
        }
        if (!routed_path) {
            for (std::uint16_t channel = 0; channel < channels_; ++channel) {
                const std::size_t input_index = frame * channels_ + channel;
                const std::size_t write_index = write_frame_ * storage_channels_ + channel;
                const float input = std::isfinite(interleaved[input_index])
                                        ? interleaved[input_index]
                                        : 0.0F;
                const float new_delay = read_delay(delay_samples_, channel);
                float delayed = new_delay;
                if (delay_transition_remaining_ > 0) {
                    const float old_delay = read_fractional_delay(
                        transition_from_delay_samples_, channel);
                    delayed = old_delay + (new_delay - old_delay) * transition_progress;
                }

                const double output_value = static_cast<double>(input) * (1.0 - mix_) +
                                            static_cast<double>(delayed) * mix_;
                interleaved[input_index] = bound(output_value);
                const double feedback_value = static_cast<double>(input) +
                                              static_cast<double>(delayed) * feedback_;
                const float shaped_feedback = shape_delay_feedback(
                    bound(feedback_value), channel);
                storage_[write_index] = shaped_feedback;
                if (channels_ == 1 && dual_capable_) {
                    storage_[write_frame_ * storage_channels_ + 1] = shaped_feedback;
                }
            }
        } else if (channels_ == 2) {
            std::array<float, 2> inputs{};
            std::array<float, 2> normal_taps{};
            std::array<float, 2> dual_taps{};
            std::array<float, 2> ping_pong_taps{};
            const float ping_weight = std::clamp(ping_pong_mix_, 0.0F, 1.0F);
            const float dual_weight = std::clamp(dual_routing_mix_, 0.0F, 1.0F);
            const float normal_weight = std::clamp(1.0F - ping_weight - dual_weight -
                                                   multitap_weight,
                                                   0.0F, 1.0F);
            for (std::uint16_t channel = 0; channel < 2; ++channel) {
                const std::size_t input_index = frame * 2 + channel;
                inputs[channel] = std::isfinite(interleaved[input_index])
                                      ? interleaved[input_index]
                                      : 0.0F;
                const float normal_new = read_delay(delay_samples_, channel);
                normal_taps[channel] = normal_new;
                if (delay_transition_remaining_ > 0) {
                    const float normal_old = read_fractional_delay(
                        transition_from_delay_samples_, channel);
                    normal_taps[channel] = normal_old +
                        (normal_new - normal_old) * transition_progress;
                }

                const float dual_new = read_delay(dual_delay_samples_[channel], channel);
                dual_taps[channel] = dual_new;
                if (dual_delay_transition_remaining_[channel] > 0) {
                    const float dual_progress = static_cast<float>(delay_transition_total_ -
                        dual_delay_transition_remaining_[channel]) /
                        static_cast<float>(delay_transition_total_);
                    const float dual_old = read_fractional_delay(
                        dual_transition_from_delay_samples_[channel], channel);
                    dual_taps[channel] = dual_old + (dual_new - dual_old) * dual_progress;
                }
            }
            for (std::uint16_t channel = 0; channel < 2; ++channel) {
                const std::uint16_t opposite = static_cast<std::uint16_t>(1U - channel);
                ping_pong_taps[channel] = normal_taps[opposite];
                const float normal_output = inputs[channel] * (1.0F - mix_) +
                                            normal_taps[channel] * mix_;
                const float ping_output = inputs[channel] * (1.0F - mix_) +
                                          ping_pong_taps[channel] * mix_;
                const float dual_output = inputs[channel] * (1.0F - dual_mix_[channel]) +
                                          dual_taps[channel] * dual_mix_[channel];
                const float multitap_output_channel =
                    inputs[channel] * (1.0F - mix_) + multitap_output[channel] * mix_;
                const double output_value =
                    static_cast<double>(normal_weight) * normal_output +
                    static_cast<double>(ping_weight) * ping_output +
                    static_cast<double>(dual_weight) * dual_output +
                    static_cast<double>(multitap_weight) * multitap_output_channel;
                interleaved[frame * 2 + channel] = bound(output_value);

                const float normal_feedback = inputs[channel] +
                                              normal_taps[channel] * feedback_;
                const float ping_feedback = inputs[channel] +
                                            ping_pong_taps[channel] * feedback_;
                const float dual_feedback = inputs[channel] +
                                            dual_taps[channel] * dual_feedback_[channel];
                const float multitap_feedback = inputs[channel] +
                    multitap_feedback_taps[channel] * feedback_;
                const double feedback_value =
                    static_cast<double>(normal_weight) * normal_feedback +
                    static_cast<double>(ping_weight) * ping_feedback +
                    static_cast<double>(dual_weight) * dual_feedback +
                    static_cast<double>(multitap_weight) * multitap_feedback;
                storage_[write_frame_ * storage_channels_ + channel] =
                    shape_delay_feedback(bound(feedback_value), channel);
            }
        } else {
            const float input = std::isfinite(interleaved[frame]) ? interleaved[frame] : 0.0F;
            const float normal_tap = read_delay(delay_samples_, 0);
            const float normal_delayed = delay_transition_remaining_ > 0
                ? read_fractional_delay(transition_from_delay_samples_, 0) +
                      (normal_tap - read_fractional_delay(transition_from_delay_samples_, 0)) *
                          transition_progress
                : normal_tap;
            std::array<float, 2> dual_taps{};
            for (std::uint16_t line = 0; line < 2; ++line) {
                const float dual_new = read_delay(dual_delay_samples_[line], line);
                dual_taps[line] = dual_new;
                if (dual_delay_transition_remaining_[line] > 0) {
                    const float dual_progress = static_cast<float>(delay_transition_total_ -
                        dual_delay_transition_remaining_[line]) /
                        static_cast<float>(delay_transition_total_);
                    const float dual_old = read_fractional_delay(
                        dual_transition_from_delay_samples_[line], line);
                    dual_taps[line] = dual_old + (dual_new - dual_old) * dual_progress;
                }
            }
            const float ping_weight = std::clamp(ping_pong_mix_, 0.0F, 1.0F);
            const float dual_weight = std::clamp(dual_routing_mix_, 0.0F, 1.0F);
            const float normal_weight = std::clamp(1.0F - ping_weight - dual_weight -
                                                   multitap_weight,
                                                   0.0F, 1.0F);
            const float normal_output = input * (1.0F - mix_) + normal_delayed * mix_;
            const float average_dual_mix = 0.5F * (dual_mix_[0] + dual_mix_[1]);
            const float dual_wet = 0.5F *
                (dual_taps[0] * dual_mix_[0] + dual_taps[1] * dual_mix_[1]);
            const float dual_output = input * (1.0F - average_dual_mix) + dual_wet;
            const float multitap_output_mono = input * (1.0F - mix_) +
                                                multitap_output[0] * mix_;
            interleaved[frame] = bound(
                static_cast<double>(normal_weight + ping_weight) * normal_output +
                static_cast<double>(dual_weight) * dual_output +
                static_cast<double>(multitap_weight) * multitap_output_mono);

            const float normal_feedback = input + normal_delayed * feedback_;
            for (std::uint16_t line = 0; line < storage_channels_; ++line) {
                const float dual_feedback = input + dual_taps[line] * dual_feedback_[line];
                const float multitap_feedback = input + multitap_feedback_taps[0] * feedback_;
                const double feedback_value =
                    static_cast<double>(normal_weight + ping_weight) * normal_feedback +
                    static_cast<double>(dual_weight) * dual_feedback +
                    static_cast<double>(multitap_weight) * multitap_feedback;
                storage_[write_frame_ * storage_channels_ + line] =
                    shape_delay_feedback(bound(feedback_value), line);
            }
        }
        write_frame_ = write_frame_ + 1 == capacity_frames_ ? 0 : write_frame_ + 1;
        advance_ramps();
        if (delay_transition_remaining_ > 0) {
            --delay_transition_remaining_;
            if (delay_transition_remaining_ == 0) {
                transition_from_delay_samples_ = static_cast<float>(delay_samples_);
            }
        }
        for (std::uint16_t line = 0; line < 2; ++line) {
            if (dual_delay_transition_remaining_[line] > 0) {
                --dual_delay_transition_remaining_[line];
                if (dual_delay_transition_remaining_[line] == 0) {
                    dual_transition_from_delay_samples_[line] =
                        static_cast<float>(dual_delay_samples_[line]);
                }
            }
        }
        for (std::size_t tap = 0; tap < maximum_delay_taps; ++tap) {
            if (multitap_delay_transition_remaining_[tap] > 0) {
                --multitap_delay_transition_remaining_[tap];
                if (multitap_delay_transition_remaining_[tap] == 0) {
                    multitap_transition_from_delay_samples_[tap] =
                        static_cast<float>(multitap_delay_samples_[tap]);
                }
            }
            if (multitap_parameter_ramp_remaining_[tap] > 0) {
                multitap_level_[tap] += multitap_level_step_[tap];
                multitap_pan_left_[tap] += multitap_pan_left_step_[tap];
                multitap_pan_right_[tap] += multitap_pan_right_step_[tap];
                --multitap_parameter_ramp_remaining_[tap];
                if (multitap_parameter_ramp_remaining_[tap] == 0) {
                    multitap_level_[tap] = target_multitap_level_[tap];
                    multitap_pan_left_[tap] = target_multitap_pan_left_[tap];
                    multitap_pan_right_[tap] = target_multitap_pan_right_[tap];
                    multitap_level_step_[tap] = 0.0F;
                    multitap_pan_left_step_[tap] = 0.0F;
                    multitap_pan_right_step_[tap] = 0.0F;
                }
            }
        }
        if (ping_pong_ramp_remaining_ > 0) {
            ping_pong_mix_ += ping_pong_step_;
            --ping_pong_ramp_remaining_;
            if (ping_pong_ramp_remaining_ == 0) {
                ping_pong_mix_ = target_ping_pong_ ? 1.0F : 0.0F;
                ping_pong_step_ = 0.0F;
            }
        }
        if (dual_parameter_ramp_remaining_ > 0) {
            for (std::size_t line = 0; line < 2; ++line) {
                dual_feedback_[line] += dual_feedback_step_[line];
                dual_mix_[line] += dual_mix_step_[line];
            }
            --dual_parameter_ramp_remaining_;
            if (dual_parameter_ramp_remaining_ == 0) {
                dual_feedback_ = target_dual_feedback_;
                dual_mix_ = target_dual_mix_;
                dual_feedback_step_ = {};
                dual_mix_step_ = {};
            }
        }
        if (dual_routing_ramp_remaining_ > 0) {
            dual_routing_mix_ += dual_routing_step_;
            --dual_routing_ramp_remaining_;
            if (dual_routing_ramp_remaining_ == 0) {
                dual_routing_mix_ = target_dual_ ? 1.0F : 0.0F;
                dual_routing_step_ = 0.0F;
            }
        }
        if (multitap_route_ramp_remaining_ > 0) {
            multitap_route_mix_ += multitap_route_step_;
            --multitap_route_ramp_remaining_;
            if (multitap_route_ramp_remaining_ == 0) {
                multitap_route_mix_ = target_multitap_ ? 1.0F : 0.0F;
                multitap_route_step_ = 0.0F;
            }
        }
    }
}

void Delay::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    write_frame_ = 0;
    feedback_ = target_feedback_;
    mix_ = target_mix_;
    feedback_step_ = 0.0F;
    mix_step_ = 0.0F;
    parameter_ramp_remaining_ = 0;
    transition_from_delay_samples_ = static_cast<float>(delay_samples_);
    delay_transition_remaining_ = 0;
    ping_pong_mix_ = target_ping_pong_ ? 1.0F : 0.0F;
    ping_pong_step_ = 0.0F;
    ping_pong_ramp_remaining_ = 0;
    dual_transition_from_delay_samples_ = {
        static_cast<float>(dual_delay_samples_[0]),
        static_cast<float>(dual_delay_samples_[1])};
    dual_delay_transition_remaining_ = {};
    dual_feedback_ = target_dual_feedback_;
    dual_feedback_step_ = {};
    dual_mix_ = target_dual_mix_;
    dual_mix_step_ = {};
    dual_parameter_ramp_remaining_ = 0;
    for (std::size_t tap = 0; tap < maximum_delay_taps; ++tap) {
        multitap_transition_from_delay_samples_[tap] =
            static_cast<float>(multitap_delay_samples_[tap]);
        multitap_delay_transition_remaining_[tap] = 0;
        multitap_level_[tap] = target_multitap_level_[tap];
        multitap_level_step_[tap] = 0.0F;
        multitap_pan_left_[tap] = target_multitap_pan_left_[tap];
        multitap_pan_left_step_[tap] = 0.0F;
        multitap_pan_right_[tap] = target_multitap_pan_right_[tap];
        multitap_pan_right_step_[tap] = 0.0F;
        multitap_parameter_ramp_remaining_[tap] = 0;
    }
    multitap_route_mix_ = target_multitap_ ? 1.0F : 0.0F;
    multitap_route_step_ = 0.0F;
    multitap_route_ramp_remaining_ = 0;
    dual_routing_mix_ = target_dual_ ? 1.0F : 0.0F;
    dual_routing_step_ = 0.0F;
    dual_routing_ramp_remaining_ = 0;
    bbd_mix_ = target_bbd_mix_;
    bbd_mix_step_ = 0.0F;
    bbd_ramp_remaining_ = 0;
    bbd_filter_state_ = {};
    tape_mix_ = target_tape_mix_;
    tape_mix_step_ = 0.0F;
    tape_ramp_remaining_ = 0;
    tape_saturation_ = target_tape_saturation_;
    tape_saturation_step_ = 0.0F;
    tape_wow_depth_samples_ = target_tape_wow_depth_samples_;
    tape_wow_depth_step_ = 0.0F;
    tape_flutter_depth_samples_ = target_tape_flutter_depth_samples_;
    tape_flutter_depth_step_ = 0.0F;
    tape_wow_phase_increment_ = target_tape_wow_phase_increment_;
    tape_wow_phase_step_ = 0.0F;
    tape_flutter_phase_increment_ = target_tape_flutter_phase_increment_;
    tape_flutter_phase_step_ = 0.0F;
    tape_wow_phase_ = 0.0F;
    tape_flutter_phase_ = 0.0F;
    tape_modulation_samples_ = 0.0F;
    tape_parameter_ramp_remaining_ = 0;
    repeat_filter_mix_ = target_repeat_filter_mix_;
    repeat_filter_mix_step_ = 0.0F;
    repeat_filter_ramp_remaining_ = 0;
    repeat_highpass_alpha_ = target_repeat_highpass_alpha_;
    repeat_highpass_alpha_step_ = 0.0F;
    repeat_lowpass_alpha_ = target_repeat_lowpass_alpha_;
    repeat_lowpass_alpha_step_ = 0.0F;
    repeat_highpass_previous_input_ = {};
    repeat_highpass_previous_output_ = {};
    repeat_lowpass_state_ = {};
}

} // namespace tabletfx
