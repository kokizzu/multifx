#include "tabletfx/effects/pitch_arpeggiator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tabletfx::effects {
namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr float kTriggerDisabledDbfs = -90.0F;
constexpr double kTriggerAttackSeconds = 0.005;
constexpr double kTriggerReleaseSeconds = 0.080;

}  // namespace

PitchArpeggiator::PitchArpeggiator(
    float sample_rate, PitchArpeggiatorParameters parameters)
    : sample_rate_(bounded(sample_rate, 48000.0F,
                           kMinimumSampleRate, kMaximumSampleRate)),
      tracker_(sample_rate_) {
    trigger_attack_coefficient_ = static_cast<float>(
        1.0 - std::exp(-1.0 /
                       (static_cast<double>(sample_rate_) *
                        kTriggerAttackSeconds)));
    trigger_release_coefficient_ = static_cast<float>(
        1.0 - std::exp(-1.0 /
                       (static_cast<double>(sample_rate_) *
                        kTriggerReleaseSeconds)));
    configure(parameters);
}

std::size_t PitchArpeggiator::memory_bytes() const noexcept {
    return sizeof(*this) + tracker_.memory_bytes() - sizeof(tracker_);
}

float PitchArpeggiator::bounded(float value, float fallback,
                                float minimum, float maximum) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minimum, maximum);
}

bool PitchArpeggiator::valid_division(
    PitchArpeggiatorDivision division) noexcept {
    return static_cast<unsigned>(division) <=
           static_cast<unsigned>(PitchArpeggiatorDivision::ThirtySecond);
}

bool PitchArpeggiator::valid_amplitude_shape(
    PitchArpeggiatorAmplitudeShape shape) noexcept {
    return static_cast<unsigned>(shape) <=
           static_cast<unsigned>(PitchArpeggiatorAmplitudeShape::Triangle);
}

bool PitchArpeggiator::valid_pan_shape(PitchArpeggiatorPanShape shape) noexcept {
    return static_cast<unsigned>(shape) <=
           static_cast<unsigned>(PitchArpeggiatorPanShape::PingPong);
}

double PitchArpeggiator::division_multiplier(
    PitchArpeggiatorDivision division) noexcept {
    switch (division) {
        case PitchArpeggiatorDivision::Free: return 0.0;
        case PitchArpeggiatorDivision::Whole: return 4.0;
        case PitchArpeggiatorDivision::Half: return 2.0;
        case PitchArpeggiatorDivision::DottedHalf: return 3.0;
        case PitchArpeggiatorDivision::Quarter: return 1.0;
        case PitchArpeggiatorDivision::DottedQuarter: return 1.5;
        case PitchArpeggiatorDivision::QuarterTriplet: return 2.0 / 3.0;
        case PitchArpeggiatorDivision::Eighth: return 0.5;
        case PitchArpeggiatorDivision::DottedEighth: return 0.75;
        case PitchArpeggiatorDivision::EighthTriplet: return 1.0 / 3.0;
        case PitchArpeggiatorDivision::Sixteenth: return 0.25;
        case PitchArpeggiatorDivision::DottedSixteenth: return 0.375;
        case PitchArpeggiatorDivision::SixteenthTriplet: return 1.0 / 6.0;
        case PitchArpeggiatorDivision::ThirtySecond: return 0.125;
    }
    return 0.5;
}

float PitchArpeggiator::eased(float phase, float alpha) noexcept {
    phase = std::clamp(phase, 0.0F, 1.0F);
    alpha = std::clamp(alpha, 0.0F, 1.0F);
    const float smooth = phase * phase * (3.0F - 2.0F * phase);
    return phase + alpha * (smooth - phase);
}

float PitchArpeggiator::finite_output(double sample) noexcept {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return static_cast<float>(std::clamp(
        sample, -static_cast<double>(kMaximumOutputMagnitude),
        static_cast<double>(kMaximumOutputMagnitude)));
}

void PitchArpeggiator::configure(
    const PitchArpeggiatorParameters& requested) noexcept {
    const PitchArpeggiatorParameters previous = parameters_;
    const bool was_auto_trigger =
        previous.trigger_threshold_dbfs > kTriggerDisabledDbfs;

    parameters_ = requested;
    parameters_.key = std::clamp(parameters_.key, 0, 11);
    if (static_cast<unsigned>(parameters_.scale) >
        static_cast<unsigned>(DiatonicScale::Chromatic)) {
        parameters_.scale = DiatonicScale::Major;
    }
    parameters_.steps = std::clamp<std::uint8_t>(
        parameters_.steps, std::uint8_t{1}, std::uint8_t{16});
    parameters_.repeats = std::min<std::uint8_t>(
        parameters_.repeats, std::uint8_t{64});
    parameters_.trigger_threshold_dbfs = bounded(
        parameters_.trigger_threshold_dbfs, kTriggerDisabledDbfs,
        kTriggerDisabledDbfs, 0.0F);
    const bool auto_trigger =
        parameters_.trigger_threshold_dbfs > kTriggerDisabledDbfs;
    trigger_threshold_linear_ = auto_trigger
        ? std::pow(10.0F, parameters_.trigger_threshold_dbfs / 20.0F)
        : 0.0F;
    for (int& shift : parameters_.stage_shift_scale_degrees) {
        // The shared monophonic tracker limits one harmonizer interval to
        // seven scale degrees. Fractal's Pitch type table p. 76 lists ±24 for
        // its Arpeggiator; that wider range remains a known project gap.
        shift = std::clamp(shift, -7, 7);
    }
    if (!valid_division(parameters_.step_division)) {
        parameters_.step_division = PitchArpeggiatorDivision::Eighth;
    }
    parameters_.tempo_bpm = bounded(parameters_.tempo_bpm, 120.0F,
                                    kMinimumTempoBpm, kMaximumTempoBpm);
    parameters_.free_step_ms = bounded(parameters_.free_step_ms, 250.0F,
                                       kMinimumStepMs, kMaximumStepMs);
    parameters_.mix = bounded(parameters_.mix, 1.0F, 0.0F, 1.0F);
    parameters_.output_level = bounded(parameters_.output_level, 1.0F,
                                       0.0F, 2.0F);
    parameters_.amplitude_alpha = bounded(parameters_.amplitude_alpha,
                                          0.0F, 0.0F, 1.0F);
    parameters_.pan_alpha = bounded(parameters_.pan_alpha,
                                    0.0F, 0.0F, 1.0F);
    if (!valid_amplitude_shape(parameters_.amplitude_shape)) {
        parameters_.amplitude_shape =
            PitchArpeggiatorAmplitudeShape::Constant;
    }
    if (!valid_pan_shape(parameters_.pan_shape)) {
        parameters_.pan_shape = PitchArpeggiatorPanShape::Center;
    }

    const bool pattern_changed =
        parameters_.steps != previous.steps ||
        parameters_.stage_shift_scale_degrees !=
            previous.stage_shift_scale_degrees;
    const bool trigger_mode_changed = auto_trigger != was_auto_trigger;
    const bool threshold_changed =
        parameters_.trigger_threshold_dbfs !=
        previous.trigger_threshold_dbfs;
    const bool manual_run_changed =
        !auto_trigger && parameters_.run != previous.run;
    const bool restart = pattern_changed || trigger_mode_changed ||
        (auto_trigger && threshold_changed) || manual_run_changed;

    if (auto_trigger) {
        if (restart) {
            running_ = false;
            trigger_envelope_ = 0.0F;
            trigger_gate_open_ = false;
        }
    } else {
        trigger_envelope_ = 0.0F;
        trigger_gate_open_ = false;
        running_ = parameters_.run;
    }
    update_step_duration(!restart && running_);
    if (restart || !running_) {
        current_step_ = 0;
        completed_repeats_ = 0;
        step_elapsed_samples_ = 0.0;
    }
    update_tracker();
    update_shape_controls();
}

void PitchArpeggiator::update_step_duration(bool preserve_fraction) noexcept {
    const double old_duration = step_duration_samples_;
    double new_duration = 0.0;
    if (parameters_.step_division == PitchArpeggiatorDivision::Free) {
        new_duration = static_cast<double>(sample_rate_) *
                       static_cast<double>(parameters_.free_step_ms) / 1000.0;
    } else {
        const double quarter_note_ms = 60000.0 /
                                       static_cast<double>(parameters_.tempo_bpm);
        const double duration_ms = quarter_note_ms *
            division_multiplier(parameters_.step_division);
        new_duration = static_cast<double>(sample_rate_) * duration_ms / 1000.0;
    }
    step_duration_samples_ = std::max(1.0, new_duration);
    if (preserve_fraction && old_duration > 0.0) {
        step_elapsed_samples_ = std::clamp(
            step_elapsed_samples_ / old_duration, 0.0, 1.0) *
            step_duration_samples_;
        if (step_elapsed_samples_ >= step_duration_samples_) {
            step_elapsed_samples_ = std::nextafter(step_duration_samples_, 0.0);
        }
    } else if (!preserve_fraction) {
        step_elapsed_samples_ = 0.0;
    }
}

void PitchArpeggiator::update_tracker() noexcept {
    tracker_config_.key = parameters_.key;
    tracker_config_.scale = parameters_.scale;
    tracker_config_.pitch_interval = current_scale_shift();
    tracker_config_.mix = parameters_.mix;
    tracker_config_.tone_hz = 12000.0F;
    tracker_config_.delay_ms = 0.0F;
    tracker_config_.feedback = 0.0F;
    tracker_config_.tap_division = TapDivision::Free;
    tracker_config_.spillover = false;
    tracker_.configure(tracker_config_);
}

void PitchArpeggiator::update_trigger_gate(
    std::span<const float> frame) noexcept {
    if (parameters_.trigger_threshold_dbfs <= kTriggerDisabledDbfs) {
        trigger_envelope_ = 0.0F;
        trigger_gate_open_ = false;
        return;
    }

    float peak = 0.0F;
    for (const float sample : frame) {
        if (std::isfinite(sample)) {
            peak = std::max(peak, std::abs(sample));
        }
    }
    const float coefficient = peak > trigger_envelope_
        ? trigger_attack_coefficient_
        : trigger_release_coefficient_;
    trigger_envelope_ += coefficient * (peak - trigger_envelope_);

    if (!trigger_gate_open_ && trigger_envelope_ >= trigger_threshold_linear_) {
        trigger_gate_open_ = true;
        running_ = true;
        current_step_ = 0;
        completed_repeats_ = 0;
        step_elapsed_samples_ = 0.0;
        update_tracker();
    } else if (trigger_gate_open_ &&
               trigger_envelope_ < trigger_threshold_linear_) {
        trigger_gate_open_ = false;
        running_ = false;
        current_step_ = 0;
        completed_repeats_ = 0;
        step_elapsed_samples_ = 0.0;
        update_tracker();
    }
}

void PitchArpeggiator::update_shape_controls() noexcept {
    const float phase = static_cast<float>(normalized_cycle_phase());
    const float amp_phase = eased(phase, parameters_.amplitude_alpha);
    const float pan_phase = eased(phase, parameters_.pan_alpha);

    switch (parameters_.amplitude_shape) {
        case PitchArpeggiatorAmplitudeShape::Constant:
            current_amplitude_ = 1.0F;
            break;
        case PitchArpeggiatorAmplitudeShape::RampUp:
            current_amplitude_ = amp_phase;
            break;
        case PitchArpeggiatorAmplitudeShape::RampDown:
            current_amplitude_ = 1.0F - amp_phase;
            break;
        case PitchArpeggiatorAmplitudeShape::Triangle:
            current_amplitude_ = 1.0F - std::abs(2.0F * amp_phase - 1.0F);
            break;
    }
    current_amplitude_ = std::clamp(current_amplitude_, 0.0F, 1.0F);

    switch (parameters_.pan_shape) {
        case PitchArpeggiatorPanShape::Center:
            current_pan_ = 0.0F;
            break;
        case PitchArpeggiatorPanShape::LeftToRight:
            current_pan_ = -1.0F + 2.0F * pan_phase;
            break;
        case PitchArpeggiatorPanShape::RightToLeft:
            current_pan_ = 1.0F - 2.0F * pan_phase;
            break;
        case PitchArpeggiatorPanShape::PingPong: {
            const float triangle = 1.0F - std::abs(2.0F * pan_phase - 1.0F);
            current_pan_ = -1.0F + 2.0F * triangle;
            break;
        }
    }
    current_pan_ = std::clamp(current_pan_, -1.0F, 1.0F);
}

void PitchArpeggiator::advance_sequence() noexcept {
    if (!running_) {
        return;
    }
    step_elapsed_samples_ += 1.0;
    if (step_elapsed_samples_ + 1.0e-9 < step_duration_samples_) {
        return;
    }

    step_elapsed_samples_ -= step_duration_samples_;
    if (step_elapsed_samples_ < 0.0) {
        step_elapsed_samples_ = 0.0;
    }
    ++current_step_;
    if (current_step_ >= parameters_.steps) {
        current_step_ = 0;
        if (completed_repeats_ < std::numeric_limits<std::uint32_t>::max()) {
            ++completed_repeats_;
        }
        if (parameters_.repeats != 0U &&
            completed_repeats_ >= parameters_.repeats) {
            running_ = false;
            if (parameters_.trigger_threshold_dbfs <= kTriggerDisabledDbfs) {
                parameters_.run = false;
            }
            current_step_ = 0;
            step_elapsed_samples_ = 0.0;
            update_tracker();
            return;
        }
    }
    update_tracker();
}

double PitchArpeggiator::normalized_cycle_phase() const noexcept {
    if (!running_ || parameters_.steps == 0U || step_duration_samples_ <= 0.0) {
        return 0.0;
    }
    const double fraction = std::clamp(
        step_elapsed_samples_ / step_duration_samples_, 0.0, 1.0);
    const double phase = (static_cast<double>(current_step_) + fraction) /
                         static_cast<double>(parameters_.steps);
    return std::clamp(phase, 0.0, 1.0);
}

int PitchArpeggiator::current_scale_shift() const noexcept {
    if (!running_ || current_step_ >= parameters_.steps) {
        return 0;
    }
    return parameters_.stage_shift_scale_degrees[current_step_];
}

int PitchArpeggiator::sequence_midi_note(int input_midi_note,
                                         int key,
                                         DiatonicScale scale,
                                         int scale_degrees) noexcept {
    const int note = std::clamp(input_midi_note, 0, 127);
    const int bounded_degrees = std::clamp(scale_degrees, -7, 7);
    return DiatonicHarmonizer::harmony_midi_note(
        note, key, scale, bounded_degrees);
}

void PitchArpeggiator::trigger() noexcept {
    parameters_.run = true;
    running_ = true;
    trigger_envelope_ = 0.0F;
    trigger_gate_open_ =
        parameters_.trigger_threshold_dbfs > kTriggerDisabledDbfs;
    current_step_ = 0;
    completed_repeats_ = 0;
    step_elapsed_samples_ = 0.0;
    update_tracker();
    update_shape_controls();
}

void PitchArpeggiator::stop() noexcept {
    parameters_.run = false;
    running_ = false;
    trigger_gate_open_ =
        parameters_.trigger_threshold_dbfs > kTriggerDisabledDbfs;
    current_step_ = 0;
    completed_repeats_ = 0;
    step_elapsed_samples_ = 0.0;
    update_tracker();
    update_shape_controls();
}

void PitchArpeggiator::reset() noexcept {
    tracker_.reset();
    const bool auto_trigger =
        parameters_.trigger_threshold_dbfs > kTriggerDisabledDbfs;
    trigger_envelope_ = 0.0F;
    trigger_gate_open_ = false;
    running_ = auto_trigger ? false : parameters_.run;
    current_step_ = 0;
    completed_repeats_ = 0;
    step_elapsed_samples_ = 0.0;
    update_tracker();
    update_shape_controls();
}

void PitchArpeggiator::process(std::span<float> interleaved,
                               std::uint16_t channels) noexcept {
    if (channels == 0U || channels > 2U) {
        return;
    }
    const std::size_t frames = interleaved.size() / channels;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const std::size_t offset = frame * channels;
        update_trigger_gate(
            std::span<const float>(interleaved.data() + offset, channels));
        update_shape_controls();
        tracker_.process(interleaved.subspan(offset, channels), channels);

        if (channels == 1U) {
            const double gain = static_cast<double>(current_amplitude_) *
                                parameters_.output_level;
            interleaved[offset] = finite_output(
                static_cast<double>(interleaved[offset]) * gain);
        } else {
            // The pitch detector is linked and monophonic. Sum its stereo
            // result to one tracked voice, then apply the project's equal-power
            // pan shape so left/right output remains deterministic.
            const double mono = 0.5 *
                (static_cast<double>(interleaved[offset]) +
                 static_cast<double>(interleaved[offset + 1U]));
            const double angle = (static_cast<double>(current_pan_) + 1.0) *
                                 (kPi / 4.0);
            const double gain = static_cast<double>(current_amplitude_) *
                                parameters_.output_level;
            interleaved[offset] = finite_output(
                mono * gain * std::cos(angle));
            interleaved[offset + 1U] = finite_output(
                mono * gain * std::sin(angle));
        }
        advance_sequence();
    }
}

}  // namespace tabletfx::effects
