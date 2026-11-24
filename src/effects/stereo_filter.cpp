#include "tabletfx/effects/stereo_filter.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kTwoPi = 2.0F * kPi;
constexpr float kDefaultSampleRate = 48000.0F;
constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kMaximumInput = 8.0F;
constexpr float kMaximumFeedback = 0.95F;
constexpr float kMaximumDelayMs = 50.0F;
constexpr float kTouchWahMixSmoothingMs = 10.0F;

float safe_sample(float sample) noexcept {
    if (!std::isfinite(sample)) return 0.0F;
    return std::clamp(sample, -kMaximumInput, kMaximumInput);
}

std::uint32_t hash_cycle(std::uint32_t value) noexcept {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

} // namespace

StereoFilter::StereoFilter(float sample_rate,
                           StereoFilterParameters parameters) {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : kDefaultSampleRate;
    delay_capacity_frames_ = std::max<std::size_t>(
        2U, static_cast<std::size_t>(std::ceil(
                static_cast<double>(sample_rate_) * kMaximumDelayMs / 1000.0)) +
                2U);
    delayed_input_.assign(delay_capacity_frames_ * 2U, 0.0F);
    delayed_output_.assign(delay_capacity_frames_ * 2U, 0.0F);
    touch_wah_mix_slew_per_sample_ = 1.0F /
        (sample_rate_ * kTouchWahMixSmoothingMs * 0.001F);
    set_parameters(parameters);
    reset();
}

std::size_t StereoFilter::memory_bytes() const noexcept {
    return sizeof(*this) + delayed_input_.capacity() * sizeof(float) +
           delayed_output_.capacity() * sizeof(float);
}

void StereoFilter::set_parameters(StereoFilterParameters parameters) noexcept {
    const StereoFilterType previous_type = parameters_.type;
    const float saved_lfo_phase = lfo_phase_;
    const std::uint32_t saved_lfo_cycle = lfo_cycle_;
    parameters.type = sanitize_type(parameters.type);
    const bool swept_type = parameters.type == StereoFilterType::AutoWah ||
                            parameters.type == StereoFilterType::EnvelopeFilter ||
                            parameters.type == StereoFilterType::TouchLadderFilter ||
                            parameters.type == StereoFilterType::TouchWahFilter;
    const float minimum_frequency = swept_type ? 100.0F : 20.0F;
    const float maximum_frequency = swept_type
        ? std::min(10000.0F, nyquist_limit())
        : nyquist_limit();
    parameters.frequency_hz = bounded(parameters.frequency_hz, 1000.0F,
                                       minimum_frequency, maximum_frequency);
    parameters.order = parameters.order == 4U ? 4U : 2U;
    parameters.q = bounded(parameters.q, 0.707F, 0.1F, 12.0F);
    parameters.ladder_resonance = bounded(parameters.ladder_resonance, 0.25F,
                                          0.0F, 1.0F);
    parameters.ladder_drive = bounded(parameters.ladder_drive, 0.0F,
                                      0.0F, 1.0F);
    parameters.ladder_bass_comp = bounded(parameters.ladder_bass_comp, 0.0F,
                                          0.0F, 1.0F);
    parameters.ladder_order = parameters.ladder_order == 2U ? 2U : 4U;
    parameters.gain_db = bounded(parameters.gain_db, 0.0F, -24.0F, 24.0F);
    parameters.low_cut_hz = parameters.low_cut_hz <= 0.0F
        ? 0.0F
        : bounded(parameters.low_cut_hz, 20.0F, 20.0F, nyquist_limit());
    parameters.high_cut_hz = parameters.high_cut_hz <= 0.0F
        ? 0.0F
        : bounded(parameters.high_cut_hz, nyquist_limit(), 20.0F,
                  nyquist_limit());
    parameters.pan_left_percent = bounded(parameters.pan_left_percent, -100.0F,
                                           -100.0F, 100.0F);
    parameters.pan_right_percent = bounded(parameters.pan_right_percent, 100.0F,
                                            -100.0F, 100.0F);
    parameters.phase_invert = sanitize_phase(parameters.phase_invert);
    parameters.delay_time_ms = bounded(parameters.delay_time_ms, 5.0F,
                                       0.1F, kMaximumDelayMs);
    parameters.depth = bounded(parameters.depth, 0.5F, 0.0F, 1.0F);
    parameters.feedback = bounded(parameters.feedback, 0.5F,
                                   -kMaximumFeedback, kMaximumFeedback);
    parameters.level_db = bounded(parameters.level_db, 0.0F, -24.0F, 12.0F);
    parameters.balance_percent = bounded(parameters.balance_percent, 0.0F,
                                         -100.0F, 100.0F);
    parameters.lfo_shape = sanitize_shape(parameters.lfo_shape);
    parameters.lfo_rate_hz = bounded(parameters.lfo_rate_hz, 0.5F, 0.05F,
                                      std::min(20.0F, sample_rate_ * 0.25F));
    parameters.lfo_duty_percent = bounded(parameters.lfo_duty_percent, 50.0F,
                                          1.0F, 99.0F);
    parameters.mod_frequency_hz = bounded(parameters.mod_frequency_hz, 4000.0F,
                                          minimum_frequency, maximum_frequency);
    parameters.mode = sanitize_mode(parameters.mode);
    if ((parameters.type == StereoFilterType::LadderFilter ||
         parameters.type == StereoFilterType::TouchLadderFilter) &&
        parameters.mode == StereoFilterMode::Highpass) {
        parameters.mode = StereoFilterMode::Lowpass;
    }
    parameters.sweep_shape = bounded(parameters.sweep_shape, 1.0F,
                                     0.25F, 4.0F);
    parameters.lfo_tempo_bpm = bounded(parameters.lfo_tempo_bpm, 120.0F,
                                       20.0F, 300.0F);
    parameters.lfo_beats_per_cycle = bounded(
        parameters.lfo_beats_per_cycle, 1.0F, 0.0625F, 4.0F);
    parameters.sensitivity_db = bounded(parameters.sensitivity_db, -36.0F,
                                         -60.0F, 0.0F);
    parameters.attack_time_ms = bounded(parameters.attack_time_ms, 5.0F,
                                        1.0F, 1000.0F);
    parameters.release_time_ms = bounded(parameters.release_time_ms, 80.0F,
                                          1.0F, 2000.0F);
    parameters.emphasis = bounded(parameters.emphasis, 0.0F, 0.0F, 1.0F);
    parameters.mix_percent = bounded(parameters.mix_percent, 50.0F,
                                      0.0F, 100.0F);
    if (parameters.mode == StereoFilterMode::Mix &&
        parameters.type != StereoFilterType::TouchWahFilter) {
        parameters.mode = StereoFilterMode::Lowpass;
    }
    if (parameters.lfo_quantize_steps == 1U) {
        parameters.lfo_quantize_steps = 2U;
    } else if (parameters.lfo_quantize_steps > 32U) {
        parameters.lfo_quantize_steps = 32U;
    }

    parameters_ = parameters;
    attack_alpha_ = std::exp(-1.0F /
        (parameters_.attack_time_ms * 0.001F * sample_rate_));
    release_alpha_ = std::exp(-1.0F /
        (parameters_.release_time_ms * 0.001F * sample_rate_));
    emphasis_alpha_ = std::exp(-kTwoPi * 1800.0F / sample_rate_);
    delay_frames_ = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::lround(
            static_cast<double>(parameters_.delay_time_ms) * sample_rate_ /
            1000.0)),
        1U, delay_capacity_frames_ - 1U);
    level_gain_ = std::pow(10.0F, parameters_.level_db / 20.0F);
    const float balance = parameters_.balance_percent / 100.0F;
    balance_left_gain_ = balance > 0.0F ? 1.0F - balance : 1.0F;
    balance_right_gain_ = balance < 0.0F ? 1.0F + balance : 1.0F;
    low_cut_alpha_ = parameters_.low_cut_hz <= 0.0F
        ? 1.0F
        : std::exp(-kTwoPi * parameters_.low_cut_hz / sample_rate_);
    high_cut_alpha_ = parameters_.high_cut_hz <= 0.0F
        ? 0.0F
        : std::exp(-kTwoPi * parameters_.high_cut_hz / sample_rate_);
    ladder_bass_comp_alpha_ = 1.0F - std::exp(
        -kTwoPi * std::min(90.0F, sample_rate_ * 0.05F) / sample_rate_);
    if (parameters_.type != previous_type) {
        reset();
    } else {
        lfo_phase_ = saved_lfo_phase;
        lfo_cycle_ = saved_lfo_cycle;
    }
    rebuild_coefficients(parameters_.frequency_hz);
}

void StereoFilter::process(std::span<float> interleaved,
                           std::uint16_t channels) noexcept {
    if (channels == 0U || channels > 2U) return;
    const std::size_t frames = interleaved.size() / channels;
    const bool is_auto_wah = parameters_.type == StereoFilterType::AutoWah;
    const bool is_envelope_filter =
        parameters_.type == StereoFilterType::EnvelopeFilter;
    const bool is_touch_ladder =
        parameters_.type == StereoFilterType::TouchLadderFilter;
    const bool is_touch_wah = parameters_.type == StereoFilterType::TouchWahFilter;
    const bool is_touch_wah_mix = is_touch_wah &&
                                  parameters_.mode == StereoFilterMode::Mix;
    const bool has_envelope_sweep =
        is_envelope_filter || is_touch_ladder || is_touch_wah;
    const bool has_lfo = (parameters_.lfo_enabled || is_auto_wah) &&
                         !has_envelope_sweep &&
                         parameters_.type != StereoFilterType::FeedForwardComb &&
                         parameters_.type != StereoFilterType::FeedbackComb &&
                         parameters_.type != StereoFilterType::Allpass &&
                         parameters_.type != StereoFilterType::Null;

    for (std::size_t frame = 0; frame < frames; ++frame) {
        const bool update_coefficients = is_touch_wah_mix
            ? coefficient_update_countdown_ == 0U
            : frame % kCoefficientUpdateFrames == 0U;
        float frame_inputs[2] = {0.0F, 0.0F};
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            frame_inputs[channel] = safe_sample(
                interleaved[frame * channels + channel]);
        }

        if (has_envelope_sweep) {
            float detector_level = 0.0F;
            for (std::uint16_t channel = 0; channel < channels; ++channel) {
                const float input = frame_inputs[channel];
                const float previous_lowpass =
                    envelope_emphasis_lowpass_[channel];
                const float lowpass = emphasis_alpha_ * previous_lowpass +
                    (1.0F - emphasis_alpha_) * input;
                envelope_emphasis_lowpass_[channel] = lowpass;
                const float highpass = input - lowpass;
                const float emphasized = std::abs(input) +
                    parameters_.emphasis * 2.0F * std::abs(highpass);
                detector_level = std::max(detector_level, emphasized);
            }

            const float previous_level = envelope_level_;
            const float alpha = detector_level > previous_level
                ? attack_alpha_
                : release_alpha_;
            envelope_level_ = alpha * previous_level +
                (1.0F - alpha) * detector_level;
            float sweep_level = envelope_level_;
            if (is_envelope_filter) {
                // Project-authored attack trigger: a rising level starts a
                // short quacky sweep that releases during a held note. This
                // keeps Envelope Filter distinct from Touch-Wah's continuous
                // volume-following response.
                const float trigger_threshold =
                    std::max(1.0e-4F, previous_level * 0.65F);
                if (detector_level > previous_level + trigger_threshold) {
                    envelope_trigger_level_ = attack_alpha_ *
                        envelope_trigger_level_ +
                        (1.0F - attack_alpha_) * detector_level;
                } else {
                    envelope_trigger_level_ *= release_alpha_;
                }
                sweep_level = envelope_trigger_level_;
            }
            const float envelope_db = 20.0F * std::log10(
                std::max(sweep_level, 1.0e-6F));
            const float normalized = parameters_.sensitivity_db >= -1.0e-6F
                ? (envelope_db >= 0.0F ? 1.0F : 0.0F)
                : std::clamp(
                    (envelope_db - parameters_.sensitivity_db) /
                        -parameters_.sensitivity_db,
                    0.0F, 1.0F);
            const float position = std::pow(normalized,
                                             parameters_.sweep_shape);
            if (update_coefficients) {
                const float lower = std::log(parameters_.frequency_hz);
                const float upper = std::log(parameters_.mod_frequency_hz);
                const float frequency = std::exp(
                    lower + (upper - lower) * position);
                rebuild_coefficients(frequency);
            }
        }

        if (has_lfo && update_coefficients) {
            const float raw_position = lfo_position();
            const float position = is_auto_wah
                ? std::pow(raw_position, parameters_.sweep_shape)
                : raw_position;
            const float lower = std::log(parameters_.frequency_hz);
            const float upper = std::log(parameters_.mod_frequency_hz);
            const float frequency = std::exp(lower + (upper - lower) * position);
            rebuild_coefficients(frequency);
        }

        if (is_touch_wah_mix) {
            const float target = parameters_.mix_percent * 0.01F;
            const float difference = target - touch_wah_mix_fraction_;
            if (difference > touch_wah_mix_slew_per_sample_) {
                touch_wah_mix_fraction_ += touch_wah_mix_slew_per_sample_;
            } else if (difference < -touch_wah_mix_slew_per_sample_) {
                touch_wah_mix_fraction_ -= touch_wah_mix_slew_per_sample_;
            } else {
                touch_wah_mix_fraction_ = target;
            }
        }

        float filtered[2] = {0.0F, 0.0F};
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const float input = frame_inputs[channel];
            float output = input;
            switch (parameters_.type) {
            case StereoFilterType::FeedForwardComb:
                output = process_comb(input, channel, false);
                break;
            case StereoFilterType::FeedbackComb:
                output = process_comb(input, channel, true);
                break;
            case StereoFilterType::Allpass:
                output = process_allpass(input, channel);
                break;
            case StereoFilterType::LadderFilter:
            case StereoFilterType::TouchLadderFilter:
                output = process_ladder(input, channel);
                break;
            case StereoFilterType::Null:
                break;
            default:
                for (std::size_t pass = 0; pass < pass_count_; ++pass) {
                    output = process_biquad(output, channel, pass);
                }
                break;
            }
            output = process_first_order(output, channel);
            if (is_touch_wah_mix) {
                // TabletFX defines MIX as a linear dry/band-pass crossfade.
                // The Fractal guide names the mode but does not publish its
                // topology or blend amount.
                output = input + touch_wah_mix_fraction_ * (output - input);
            }
            const bool invert = parameters_.phase_invert ==
                                    StereoFilterPhaseInvert::Both ||
                                (channel == 0U && parameters_.phase_invert ==
                                    StereoFilterPhaseInvert::Left) ||
                                (channel == 1U && parameters_.phase_invert ==
                                    StereoFilterPhaseInvert::Right);
            if (invert) output = -output;
            filtered[channel] = safe_sample(output * level_gain_);
        }

        if (channels == 1U) {
            interleaved[frame] = filtered[0];
        } else {
            const float left = filtered[0] * pan_left_gain(
                parameters_.pan_left_percent);
            const float right = filtered[0] * pan_right_gain(
                parameters_.pan_left_percent);
            const float other_left = filtered[1] * pan_left_gain(
                parameters_.pan_right_percent);
            const float other_right = filtered[1] * pan_right_gain(
                parameters_.pan_right_percent);
            interleaved[frame * 2U] = safe_sample(
                (left + other_left) * balance_left_gain_);
            interleaved[frame * 2U + 1U] = safe_sample(
                (right + other_right) * balance_right_gain_);
        }

        ++write_frame_;
        if (write_frame_ == delay_capacity_frames_) write_frame_ = 0U;
        if (coefficient_update_countdown_ == 0U) {
            coefficient_update_countdown_ = kCoefficientUpdateFrames - 1U;
        } else {
            --coefficient_update_countdown_;
        }
        if (has_lfo) advance_lfo();
    }
}

void StereoFilter::reset() noexcept {
    for (auto& channel : biquad_states_) {
        channel.fill(BiquadState{});
    }
    for (auto& channel : ladder_states_) {
        channel.fill(0.0F);
    }
    ladder_bass_comp_states_.fill(0.0F);
    low_cut_states_.fill(FirstOrderState{});
    high_cut_states_.fill(FirstOrderState{});
    std::fill(delayed_input_.begin(), delayed_input_.end(), 0.0F);
    std::fill(delayed_output_.begin(), delayed_output_.end(), 0.0F);
    write_frame_ = 0U;
    lfo_phase_ = 0.0F;
    lfo_cycle_ = 0U;
    coefficient_update_countdown_ = 0U;
    envelope_emphasis_lowpass_.fill(0.0F);
    envelope_level_ = 0.0F;
    envelope_trigger_level_ = 0.0F;
    touch_wah_mix_fraction_ = parameters_.mix_percent * 0.01F;
}

float StereoFilter::bounded(float value, float fallback, float minimum,
                            float maximum) noexcept {
    if (!std::isfinite(value)) value = fallback;
    return std::clamp(value, minimum, maximum);
}

StereoFilterType StereoFilter::sanitize_type(StereoFilterType type) noexcept {
    switch (type) {
    case StereoFilterType::Null:
    case StereoFilterType::Lowpass:
    case StereoFilterType::Bandpass:
    case StereoFilterType::Highpass:
    case StereoFilterType::LowShelf:
    case StereoFilterType::HighShelf:
    case StereoFilterType::Peaking:
    case StereoFilterType::Notch:
    case StereoFilterType::TiltEq:
    case StereoFilterType::FeedForwardComb:
    case StereoFilterType::FeedbackComb:
    case StereoFilterType::Allpass:
    case StereoFilterType::LowShelf2:
    case StereoFilterType::HighShelf2:
    case StereoFilterType::Peaking2:
    case StereoFilterType::AutoWah:
    case StereoFilterType::EnvelopeFilter:
    case StereoFilterType::LadderFilter:
    case StereoFilterType::TouchLadderFilter:
    case StereoFilterType::TouchWahFilter:
        return type;
    }
    return StereoFilterType::Lowpass;
}

StereoFilterLfoShape StereoFilter::sanitize_shape(
    StereoFilterLfoShape shape) noexcept {
    switch (shape) {
    case StereoFilterLfoShape::Sine:
    case StereoFilterLfoShape::Triangle:
    case StereoFilterLfoShape::Square:
    case StereoFilterLfoShape::SawUp:
    case StereoFilterLfoShape::SawDown:
    case StereoFilterLfoShape::Random:
        return shape;
    }
    return StereoFilterLfoShape::Sine;
}

StereoFilterPhaseInvert StereoFilter::sanitize_phase(
    StereoFilterPhaseInvert phase) noexcept {
    switch (phase) {
    case StereoFilterPhaseInvert::None:
    case StereoFilterPhaseInvert::Left:
    case StereoFilterPhaseInvert::Right:
    case StereoFilterPhaseInvert::Both:
        return phase;
    }
    return StereoFilterPhaseInvert::None;
}

StereoFilterMode StereoFilter::sanitize_mode(StereoFilterMode mode) noexcept {
    switch (mode) {
    case StereoFilterMode::Lowpass:
    case StereoFilterMode::Bandpass:
    case StereoFilterMode::Highpass:
    case StereoFilterMode::Mix:
        return mode;
    }
    return StereoFilterMode::Lowpass;
}

float StereoFilter::nyquist_limit() const noexcept {
    return std::max(20.0F, std::min(20000.0F, sample_rate_ * 0.45F));
}

void StereoFilter::rebuild_coefficients(float frequency_hz) noexcept {
    if (parameters_.type == StereoFilterType::LadderFilter ||
        parameters_.type == StereoFilterType::TouchLadderFilter) {
        ladder_cutoff_hz_ = bounded(frequency_hz, parameters_.frequency_hz,
                                    20.0F, nyquist_limit());
        ladder_cutoff_alpha_ = 1.0F - std::exp(
            -kTwoPi * ladder_cutoff_hz_ / sample_rate_);
        pass_count_ = 0U;
        coefficients_.fill(BiquadCoefficients{});
        return;
    }
    pass_count_ = make_coefficients(frequency_hz, coefficients_);
    if (parameters_.order == 4U && pass_count_ > 0U &&
        pass_count_ <= kMaximumPasses / 2U) {
        const std::size_t first_count = pass_count_;
        for (std::size_t pass = 0; pass < first_count; ++pass) {
            coefficients_[pass + first_count] = coefficients_[pass];
        }
        pass_count_ += first_count;
    }
    if (pass_count_ == 0U) {
        coefficients_.fill(BiquadCoefficients{});
    }
}

std::size_t StereoFilter::make_coefficients(
    float frequency_hz,
    std::array<BiquadCoefficients, kMaximumPasses>& coefficients) const noexcept {
    coefficients.fill(BiquadCoefficients{});
    StereoFilterType effective_type = parameters_.type;
    if (parameters_.type == StereoFilterType::AutoWah) {
        switch (parameters_.mode) {
        case StereoFilterMode::Lowpass:
            effective_type = StereoFilterType::Lowpass;
            break;
        case StereoFilterMode::Bandpass:
            effective_type = StereoFilterType::Bandpass;
            break;
        case StereoFilterMode::Highpass:
            effective_type = StereoFilterType::Highpass;
            break;
        case StereoFilterMode::Mix:
            effective_type = StereoFilterType::Bandpass;
            break;
        }
    } else if (parameters_.type == StereoFilterType::EnvelopeFilter ||
               parameters_.type == StereoFilterType::TouchWahFilter) {
        switch (parameters_.mode) {
        case StereoFilterMode::Lowpass:
            effective_type = StereoFilterType::Lowpass;
            break;
        case StereoFilterMode::Bandpass:
            effective_type = StereoFilterType::Bandpass;
            break;
        case StereoFilterMode::Highpass:
            effective_type = StereoFilterType::Highpass;
            break;
        case StereoFilterMode::Mix:
            effective_type = StereoFilterType::Bandpass;
            break;
        }
    }
    if (effective_type == StereoFilterType::Null ||
        effective_type == StereoFilterType::FeedForwardComb ||
        effective_type == StereoFilterType::FeedbackComb ||
        effective_type == StereoFilterType::Allpass ||
        effective_type == StereoFilterType::LadderFilter ||
        effective_type == StereoFilterType::TouchLadderFilter) {
        return 0U;
    }

    const float frequency = bounded(frequency_hz, parameters_.frequency_hz,
                                    20.0F, nyquist_limit());
    const float omega = kTwoPi * frequency / sample_rate_;
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    const float q = parameters_.q;
    const float alpha = sine / (2.0F * q);
    const auto normalize = [](float b0, float b1, float b2,
                              float a0, float a1, float a2) noexcept {
        BiquadCoefficients result;
        if (!std::isfinite(a0) || std::abs(a0) < 1.0e-12F) return result;
        result.b0 = b0 / a0;
        result.b1 = b1 / a0;
        result.b2 = b2 / a0;
        result.a1 = a1 / a0;
        result.a2 = a2 / a0;
        return result;
    };

    const auto peaking = [&](float gain_db) noexcept {
        const float a = std::pow(10.0F, gain_db / 40.0F);
        return normalize(1.0F + alpha * a, -2.0F * cosine,
                         1.0F - alpha * a, 1.0F + alpha / a,
                         -2.0F * cosine, 1.0F - alpha / a);
    };
    const auto shelf = [&](bool high, float gain_db) noexcept {
        const float a = std::pow(10.0F, gain_db / 40.0F);
        const float slope = std::clamp(q, 0.1F, 2.0F);
        const float shelf_alpha = sine * 0.5F * std::sqrt(
            std::max(0.0F, (a + 1.0F / a) * (1.0F / slope - 1.0F) + 2.0F));
        const float beta = 2.0F * std::sqrt(a) * shelf_alpha;
        if (!high) {
            return normalize(
                a * ((a + 1.0F) - (a - 1.0F) * cosine + beta),
                2.0F * a * ((a - 1.0F) - (a + 1.0F) * cosine),
                a * ((a + 1.0F) - (a - 1.0F) * cosine - beta),
                (a + 1.0F) + (a - 1.0F) * cosine + beta,
                -2.0F * ((a - 1.0F) + (a + 1.0F) * cosine),
                (a + 1.0F) + (a - 1.0F) * cosine - beta);
        }
        return normalize(
            a * ((a + 1.0F) + (a - 1.0F) * cosine + beta),
            -2.0F * a * ((a - 1.0F) + (a + 1.0F) * cosine),
            a * ((a + 1.0F) + (a - 1.0F) * cosine - beta),
            (a + 1.0F) - (a - 1.0F) * cosine + beta,
            2.0F * ((a - 1.0F) - (a + 1.0F) * cosine),
            (a + 1.0F) - (a - 1.0F) * cosine - beta);
    };

    switch (effective_type) {
    case StereoFilterType::Lowpass:
        coefficients[0] = normalize((1.0F - cosine) * 0.5F,
                                    1.0F - cosine,
                                    (1.0F - cosine) * 0.5F,
                                    1.0F + alpha, -2.0F * cosine,
                                    1.0F - alpha);
        return 1U;
    case StereoFilterType::Highpass:
        coefficients[0] = normalize((1.0F + cosine) * 0.5F,
                                    -(1.0F + cosine),
                                    (1.0F + cosine) * 0.5F,
                                    1.0F + alpha, -2.0F * cosine,
                                    1.0F - alpha);
        return 1U;
    case StereoFilterType::Bandpass:
        coefficients[0] = normalize(alpha, 0.0F, -alpha,
                                    1.0F + alpha, -2.0F * cosine,
                                    1.0F - alpha);
        return 1U;
    case StereoFilterType::LowShelf:
    case StereoFilterType::LowShelf2:
        coefficients[0] = shelf(false, parameters_.gain_db);
        return 1U;
    case StereoFilterType::HighShelf:
    case StereoFilterType::HighShelf2:
        coefficients[0] = shelf(true, parameters_.gain_db);
        return 1U;
    case StereoFilterType::Peaking:
    case StereoFilterType::Peaking2:
        coefficients[0] = peaking(parameters_.gain_db);
        return 1U;
    case StereoFilterType::Notch:
        coefficients[0] = normalize(1.0F, -2.0F * cosine, 1.0F,
                                    1.0F + alpha, -2.0F * cosine,
                                    1.0F - alpha);
        return 1U;
    case StereoFilterType::TiltEq:
        coefficients[0] = shelf(false, -parameters_.gain_db * 0.5F);
        coefficients[1] = shelf(true, parameters_.gain_db * 0.5F);
        return 2U;
    case StereoFilterType::Allpass:
    case StereoFilterType::FeedForwardComb:
    case StereoFilterType::FeedbackComb:
    case StereoFilterType::Null:
    case StereoFilterType::AutoWah:
    case StereoFilterType::EnvelopeFilter:
    case StereoFilterType::LadderFilter:
    case StereoFilterType::TouchLadderFilter:
    case StereoFilterType::TouchWahFilter:
        break;
    }
    return 0U;
}

float StereoFilter::process_biquad(float input, std::uint16_t channel,
                                   std::size_t pass) noexcept {
    auto& state = biquad_states_[channel][pass];
    const auto& coefficient = coefficients_[pass];
    const float output = coefficient.b0 * input + state.s1;
    state.s1 = coefficient.b1 * input - coefficient.a1 * output + state.s2;
    state.s2 = coefficient.b2 * input - coefficient.a2 * output;
    if (!std::isfinite(state.s1) || !std::isfinite(state.s2)) {
        state = BiquadState{};
        return 0.0F;
    }
    return safe_sample(output);
}

float StereoFilter::process_ladder(float input,
                                  std::uint16_t channel) noexcept {
    auto& states = ladder_states_[channel];
    const std::size_t stage_count = parameters_.ladder_order == 2U ? 2U : 4U;
    const std::size_t last_stage = stage_count - 1U;

    // This bounded four-stage feedback cascade is a project-authored filter,
    // not an emulation of Fractal's proprietary Ladder Filter DSP. Resonance
    // follows the documented role; its numeric mapping here is a local choice.
    const float resonance = 3.6F * parameters_.ladder_resonance;
    float driven = safe_sample(input - resonance * states[last_stage]);
    if (parameters_.ladder_drive > 0.0F) {
        const float drive_gain = 1.0F + 6.0F * parameters_.ladder_drive;
        driven = std::tanh(driven * drive_gain) / drive_gain;
    }

    float stage_input = driven;
    for (std::size_t stage = 0U; stage < stage_count; ++stage) {
        float& state = states[stage];
        state += ladder_cutoff_alpha_ * (stage_input - state);
        if (!std::isfinite(state)) {
            states.fill(0.0F);
            return 0.0F;
        }
        state = safe_sample(state);
        stage_input = state;
    }

    float output = states[last_stage];
    if (parameters_.mode == StereoFilterMode::Bandpass) {
        // Adjacent low-pass stages form a project-defined band-pass response.
        output = states[stage_count / 2U - 1U] - states[last_stage];
    } else {
        // Bass compensation is a separate low-frequency feed-forward shelf,
        // enabled only in low-pass mode as specified by the Blocks Guide.
        float& bass_state = ladder_bass_comp_states_[channel];
        bass_state += ladder_bass_comp_alpha_ * (input - bass_state);
        if (!std::isfinite(bass_state)) {
            bass_state = 0.0F;
        }
        output += 0.8F * parameters_.ladder_bass_comp *
                  parameters_.ladder_resonance * bass_state;
    }
    return safe_sample(output);
}

float StereoFilter::process_comb(float input, std::uint16_t channel,
                                 bool feedback) noexcept {
    const std::size_t offset = static_cast<std::size_t>(channel) *
                               delay_capacity_frames_;
    const std::size_t read_frame =
        (write_frame_ + delay_capacity_frames_ - delay_frames_) %
        delay_capacity_frames_;
    const std::size_t delayed_index = offset + read_frame;
    const std::size_t write_index = offset + write_frame_;
    const float delayed = feedback ? delayed_output_[delayed_index]
                                   : delayed_input_[delayed_index];
    // For the feedback form, the documented Depth control is interpreted as
    // loop intensity. Both variants use the same project-authored 0..1 control
    // scale; the stable feedback loop is bounded by that maximum.
    const float output = input + parameters_.depth * delayed;
    delayed_input_[write_index] = input;
    delayed_output_[write_index] = safe_sample(output);
    return safe_sample(output);
}

float StereoFilter::process_allpass(float input,
                                    std::uint16_t channel) noexcept {
    const std::size_t offset = static_cast<std::size_t>(channel) *
                               delay_capacity_frames_;
    const std::size_t read_frame =
        (write_frame_ + delay_capacity_frames_ - delay_frames_) %
        delay_capacity_frames_;
    const std::size_t delayed_index = offset + read_frame;
    const std::size_t write_index = offset + write_frame_;
    const float delayed_input = delayed_input_[delayed_index];
    const float delayed_output = delayed_output_[delayed_index];
    const float feedback = parameters_.feedback;
    const float output = -feedback * input + delayed_input +
                         feedback * delayed_output;
    delayed_input_[write_index] = input;
    delayed_output_[write_index] = safe_sample(output);
    return safe_sample(output);
}

float StereoFilter::process_first_order(float input,
                                        std::uint16_t channel) noexcept {
    float output = input;
    if (parameters_.low_cut_hz > 0.0F) {
        auto& state = low_cut_states_[channel];
        const float highpass = low_cut_alpha_ *
            (state.previous_output + output - state.previous_input);
        state.previous_input = output;
        state.previous_output = safe_sample(highpass);
        output = state.previous_output;
    }
    if (parameters_.high_cut_hz > 0.0F) {
        auto& state = high_cut_states_[channel];
        const float lowpass = (1.0F - high_cut_alpha_) * output +
                              high_cut_alpha_ * state.previous_output;
        state.previous_output = safe_sample(lowpass);
        output = state.previous_output;
    }
    return safe_sample(output);
}

float StereoFilter::lfo_position() const noexcept {
    const float phase = lfo_phase_ - std::floor(lfo_phase_);
    float position = 0.0F;
    switch (parameters_.lfo_shape) {
    case StereoFilterLfoShape::Sine:
        position = 0.5F - 0.5F * std::cos(kTwoPi * phase);
        break;
    case StereoFilterLfoShape::Triangle: {
        const float duty = parameters_.lfo_duty_percent / 100.0F;
        position = phase < duty ? phase / duty
                                : 1.0F - (phase - duty) / (1.0F - duty);
        break;
    }
    case StereoFilterLfoShape::Square:
        position = phase < parameters_.lfo_duty_percent / 100.0F ? 1.0F : 0.0F;
        break;
    case StereoFilterLfoShape::SawUp:
        position = phase;
        break;
    case StereoFilterLfoShape::SawDown:
        position = 1.0F - phase;
        break;
    case StereoFilterLfoShape::Random:
        position = static_cast<float>(hash_cycle(lfo_cycle_) & 0xffffU) /
                   65535.0F;
        break;
    }
    if (parameters_.lfo_quantize_steps >= 2U) {
        const float steps = static_cast<float>(parameters_.lfo_quantize_steps - 1U);
        position = std::floor(std::clamp(position, 0.0F, 1.0F) * steps) / steps;
    }
    return std::clamp(position, 0.0F, 1.0F);
}

void StereoFilter::advance_lfo() noexcept {
    const float rate_hz = parameters_.lfo_tempo_sync
        ? parameters_.lfo_tempo_bpm /
              (60.0F * parameters_.lfo_beats_per_cycle)
        : parameters_.lfo_rate_hz;
    lfo_phase_ += std::clamp(rate_hz, 0.05F, 20.0F) / sample_rate_;
    if (lfo_phase_ >= 1.0F) {
        lfo_phase_ -= 1.0F;
        ++lfo_cycle_;
    }
}

float StereoFilter::pan_left_gain(float percent) noexcept {
    const float position = (bounded(percent, 0.0F, -100.0F, 100.0F) + 100.0F) /
                           200.0F;
    return std::cos(position * kPi * 0.5F);
}

float StereoFilter::pan_right_gain(float percent) noexcept {
    const float position = (bounded(percent, 0.0F, -100.0F, 100.0F) + 100.0F) /
                           200.0F;
    return std::sin(position * kPi * 0.5F);
}

} // namespace tabletfx
