#include "tabletfx/effects/asheville_pattrn.hpp"

#include <algorithm>
#include <cmath>

namespace tabletfx {
namespace {

constexpr float kTwoPi = 6.28318530717958647692F;
constexpr float kMinimumSampleRate = 8000.0F;
constexpr float kMaximumSampleRate = 384000.0F;
constexpr float kInputLimit = 16.0F;
constexpr float kOutputLimit = 8.0F;
constexpr float kBandNormalization = 0.35355339059327376220F;

// Frequency centers and filter implementation are project choices informed by
// the MF-105M manual's BASS/MIDS voicing table. BASS band 0 is a low-pass at
// the documented 110 Hz cutoff; its other bands and all MIDS bands are
// resonant band-pass filters. This does not claim the original circuit.
constexpr std::array<float, project_asheville_band_count> kBassCentersHz{
    110.0F, 160.0F, 240.0F, 350.0F, 525.0F, 775.0F, 1200.0F, 1800.0F};
constexpr std::array<float, project_asheville_band_count> kMidsCentersHz{
    200.0F, 300.0F, 450.0F, 675.0F, 1000.0F, 1500.0F, 2200.0F, 3400.0F};

// Eight project-authored 8-step masks. Each bit selects one band's envelope
// for that step; the tables are original project patterns, not copies of the
// MF-105M factory sequences.
constexpr std::array<
    std::array<std::uint8_t, project_asheville_pattern_count>,
    project_asheville_pattern_count>
    kProjectPatternMasks{{
        {{0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U}},
        {{0x55U, 0xAAU, 0x55U, 0xAAU, 0x55U, 0xAAU, 0x55U, 0xAAU}},
        {{0x01U, 0x03U, 0x07U, 0x0FU, 0x1FU, 0x3FU, 0x7FU, 0xFFU}},
        {{0x80U, 0xC0U, 0xE0U, 0xF0U, 0xF8U, 0xFCU, 0xFEU, 0xFFU}},
        {{0x11U, 0x22U, 0x44U, 0x88U, 0x88U, 0x44U, 0x22U, 0x11U}},
        {{0x81U, 0x42U, 0x24U, 0x18U, 0x18U, 0x24U, 0x42U, 0x81U}},
        {{0x0FU, 0xF0U, 0x0FU, 0xF0U, 0x0FU, 0xF0U, 0x0FU, 0xF0U}},
        {{0xFFU, 0x00U, 0x33U, 0x00U, 0xCCU, 0x00U, 0xFFU, 0x00U}},
    }};

AshevilleFrequencyBank sanitize_bank(AshevilleFrequencyBank bank) noexcept {
    switch (bank) {
    case AshevilleFrequencyBank::Bass:
    case AshevilleFrequencyBank::Mids:
        return bank;
    }
    return AshevilleFrequencyBank::Bass;
}

} // namespace

ProjectAshevillePattrn::ProjectAshevillePattrn(
    float sample_rate,
    ProjectAshevillePattrnParameters parameters) noexcept {
    configure(sample_rate, parameters);
}

void ProjectAshevillePattrn::configure(
    float sample_rate,
    ProjectAshevillePattrnParameters parameters) noexcept {
    sample_rate_ = std::isfinite(sample_rate) &&
                           sample_rate >= kMinimumSampleRate &&
                           sample_rate <= kMaximumSampleRate
                       ? sample_rate
                       : 48000.0F;
    parameters_ = sanitize(parameters);
    update_filter_coefficients();
    update_envelope_coefficients();
    drive_gain_target_ = db_to_gain(parameters_.drive_db);
    output_gain_target_ = db_to_gain(parameters_.output_db);
    mix_target_ = parameters_.mix;
    pattern_rate_target_hz_ = parameters_.pattern_rate_hz;
    band_level_target_ = parameters_.band_level;
    snap_to_targets();
    reset();
}

void ProjectAshevillePattrn::set_parameters(
    ProjectAshevillePattrnParameters parameters) noexcept {
    parameters = sanitize(parameters);
    const bool filter_change = parameters.bank != parameters_.bank ||
                               parameters.resonance_q !=
                                   parameters_.resonance_q;
    const bool pattern_change = parameters.pattern != parameters_.pattern;
    parameters_ = parameters;

    if (filter_change) {
        update_filter_coefficients();
        filter_state_.fill({});
    }
    update_envelope_coefficients();

    drive_gain_target_ = db_to_gain(parameters_.drive_db);
    output_gain_target_ = db_to_gain(parameters_.output_db);
    mix_target_ = parameters_.mix;
    pattern_rate_target_hz_ = parameters_.pattern_rate_hz;
    band_level_target_ = parameters_.band_level;

    if (pattern_change) {
        pattern_phase_ = 0.0F;
        step_index_ = 0U;
    }
    update_envelope_targets();
    retarget();
}

void ProjectAshevillePattrn::process(std::span<float> interleaved,
                                     std::uint16_t channels) noexcept {
    if (channels == 0U || channels > filter_state_.size()) return;

    const std::size_t channel_count = channels;
    for (std::size_t offset = 0U; offset < interleaved.size();
         offset += channel_count) {
        advance_ramp();
        advance_envelopes();

        const std::size_t frame_channels =
            std::min(channel_count, interleaved.size() - offset);
        for (std::size_t channel = 0U; channel < frame_channels; ++channel) {
            float& sample = interleaved[offset + channel];
            if (!std::isfinite(sample)) {
                sample = 0.0F;
                filter_state_[channel].fill({});
                continue;
            }

            const float dry = std::clamp(sample, -kInputLimit, kInputLimit);
            const float driven = std::clamp(dry * drive_gain_,
                                            -kInputLimit, kInputLimit);
            float wet = 0.0F;
            for (std::size_t band = 0U;
                 band < project_asheville_band_count; ++band) {
                const float filtered = process_band(driven,
                                                    static_cast<std::uint16_t>(channel),
                                                    band);
                wet += filtered * envelope_[band] * band_level_[band];
            }
            wet *= kBandNormalization;
            const float mixed = dry + (wet - dry) * mix_;
            sample = std::clamp(mixed * output_gain_,
                                -kOutputLimit, kOutputLimit);
        }
        advance_sequence();
    }
}

void ProjectAshevillePattrn::reset() noexcept {
    filter_state_.fill({});
    envelope_.fill(0.0F);
    pattern_phase_ = 0.0F;
    step_index_ = 0U;
    update_envelope_targets();
    snap_to_targets();
    ramp_remaining_ = 0U;
}

ProjectAshevillePattrnParameters ProjectAshevillePattrn::sanitize(
    ProjectAshevillePattrnParameters parameters) noexcept {
    parameters.bank = sanitize_bank(parameters.bank);
    const std::uint8_t max_pattern = static_cast<std::uint8_t>(
        project_asheville_pattern_count - 1U);
    parameters.pattern = std::min(parameters.pattern, max_pattern);
    for (float& level : parameters.band_level) {
        level = std::isfinite(level) ? std::clamp(level, 0.0F, 1.0F) : 1.0F;
    }
    parameters.drive_db = std::isfinite(parameters.drive_db)
                              ? std::clamp(parameters.drive_db, 0.0F, 18.0F)
                              : 0.0F;
    parameters.output_db = std::isfinite(parameters.output_db)
                               ? std::clamp(parameters.output_db, -24.0F,
                                            12.0F)
                               : 0.0F;
    parameters.mix = std::isfinite(parameters.mix)
                         ? std::clamp(parameters.mix, 0.0F, 1.0F)
                         : 0.5F;
    parameters.pattern_rate_hz =
        std::isfinite(parameters.pattern_rate_hz)
            ? std::clamp(parameters.pattern_rate_hz, 0.05F, 10.0F)
            : 0.75F;
    parameters.resonance_q = std::isfinite(parameters.resonance_q)
                                 ? std::clamp(parameters.resonance_q, 0.5F,
                                              12.0F)
                                 : 3.0F;
    parameters.envelope_shape =
        std::isfinite(parameters.envelope_shape)
            ? std::clamp(parameters.envelope_shape, 0.0F, 1.0F)
            : 0.5F;
    parameters.smoothing_ms = std::isfinite(parameters.smoothing_ms)
                                  ? std::clamp(parameters.smoothing_ms, 0.0F,
                                               1000.0F)
                                  : 10.0F;
    return parameters;
}

float ProjectAshevillePattrn::db_to_gain(float decibels) noexcept {
    return std::pow(10.0F, decibels / 20.0F);
}

float ProjectAshevillePattrn::alpha_for_ms(float sample_rate,
                                           float milliseconds) noexcept {
    const float seconds = std::max(milliseconds, 0.001F) / 1000.0F;
    return 1.0F - std::exp(-1.0F / (sample_rate * seconds));
}

void ProjectAshevillePattrn::update_filter_coefficients() noexcept {
    const auto& centers = parameters_.bank == AshevilleFrequencyBank::Bass
                              ? kBassCentersHz
                              : kMidsCentersHz;
    for (std::size_t band = 0U; band < project_asheville_band_count; ++band) {
        const bool bass_lowpass =
            parameters_.bank == AshevilleFrequencyBank::Bass && band == 0U;
        const float frequency = std::clamp(
            centers[band], 20.0F,
            std::min(18000.0F, sample_rate_ * 0.45F));
        const float omega = kTwoPi * frequency / sample_rate_;
        const float sine = std::sin(omega);
        const float cosine = std::cos(omega);
        // The first BASS band has no project control for resonance. Use a
        // second-order Butterworth low-pass; other bands use the Q control.
        constexpr float kButterworthQ = 0.70710678118654752440F;
        const float q = bass_lowpass ? kButterworthQ : parameters_.resonance_q;
        const float alpha = sine / (2.0F * q);
        const float a0 = 1.0F + alpha;
        if (bass_lowpass) {
            const float one_minus_cosine = 1.0F - cosine;
            coefficients_[band] = {
                0.5F * one_minus_cosine / a0,
                one_minus_cosine / a0,
                0.5F * one_minus_cosine / a0,
                -2.0F * cosine / a0,
                (1.0F - alpha) / a0,
            };
        } else {
            coefficients_[band] = {
                alpha / a0,
                0.0F,
                -alpha / a0,
                -2.0F * cosine / a0,
                (1.0F - alpha) / a0,
            };
        }
    }
}

void ProjectAshevillePattrn::update_envelope_targets() noexcept {
    const std::uint8_t mask =
        kProjectPatternMasks[parameters_.pattern][step_index_];
    for (std::size_t band = 0U; band < project_asheville_band_count; ++band) {
        const std::uint8_t band_bit =
            static_cast<std::uint8_t>(1U << static_cast<unsigned>(band));
        envelope_target_[band] = (mask & band_bit) != 0U ? 1.0F : 0.0F;
    }
}

void ProjectAshevillePattrn::update_envelope_coefficients() noexcept {
    const float shape = parameters_.envelope_shape;
    const float attack_ms = 1.0F + 39.0F * shape;
    const float release_ms = 10.0F + 590.0F * shape;
    attack_alpha_ = alpha_for_ms(sample_rate_, attack_ms);
    release_alpha_ = alpha_for_ms(sample_rate_, release_ms);
}

void ProjectAshevillePattrn::retarget() noexcept {
    const double requested_frames = std::ceil(
        static_cast<double>(sample_rate_) *
        static_cast<double>(parameters_.smoothing_ms) / 1000.0);
    if (requested_frames < 1.0) {
        snap_to_targets();
        ramp_remaining_ = 0U;
        return;
    }

    ramp_remaining_ = static_cast<std::size_t>(requested_frames);
    const float divisor = static_cast<float>(ramp_remaining_);
    drive_gain_step_ = (drive_gain_target_ - drive_gain_) / divisor;
    output_gain_step_ = (output_gain_target_ - output_gain_) / divisor;
    mix_step_ = (mix_target_ - mix_) / divisor;
    pattern_rate_step_ = (pattern_rate_target_hz_ - pattern_rate_hz_) / divisor;
    for (std::size_t band = 0U; band < project_asheville_band_count; ++band) {
        band_level_step_[band] =
            (band_level_target_[band] - band_level_[band]) / divisor;
    }
}

void ProjectAshevillePattrn::advance_ramp() noexcept {
    if (ramp_remaining_ == 0U) return;
    drive_gain_ += drive_gain_step_;
    output_gain_ += output_gain_step_;
    mix_ += mix_step_;
    pattern_rate_hz_ += pattern_rate_step_;
    for (std::size_t band = 0U; band < project_asheville_band_count; ++band) {
        band_level_[band] += band_level_step_[band];
    }
    --ramp_remaining_;
    if (ramp_remaining_ == 0U) snap_to_targets();
}

void ProjectAshevillePattrn::advance_sequence() noexcept {
    pattern_phase_ += pattern_rate_hz_ / sample_rate_;
    if (pattern_phase_ >= 1.0F) pattern_phase_ -= 1.0F;
    const auto next_step = static_cast<std::uint8_t>(
        std::min(static_cast<unsigned>(pattern_phase_ *
                                      static_cast<float>(project_asheville_pattern_count)),
                 static_cast<unsigned>(project_asheville_pattern_count - 1U)));
    if (next_step == step_index_) return;
    step_index_ = next_step;
    update_envelope_targets();
}

void ProjectAshevillePattrn::advance_envelopes() noexcept {
    for (std::size_t band = 0U; band < project_asheville_band_count; ++band) {
        const float coefficient = envelope_target_[band] > envelope_[band]
                                      ? attack_alpha_
                                      : release_alpha_;
        envelope_[band] += coefficient *
                           (envelope_target_[band] - envelope_[band]);
    }
}

void ProjectAshevillePattrn::snap_to_targets() noexcept {
    drive_gain_ = drive_gain_target_;
    output_gain_ = output_gain_target_;
    mix_ = mix_target_;
    pattern_rate_hz_ = pattern_rate_target_hz_;
    band_level_ = band_level_target_;
    drive_gain_step_ = 0.0F;
    output_gain_step_ = 0.0F;
    mix_step_ = 0.0F;
    pattern_rate_step_ = 0.0F;
    band_level_step_.fill(0.0F);
}

float ProjectAshevillePattrn::process_band(float input,
                                            std::uint16_t channel,
                                            std::size_t band) noexcept {
    const auto& coefficient = coefficients_[band];
    BiquadState& state = filter_state_[channel][band];
    const float output = coefficient.b0 * input + state.z1;
    state.z1 = coefficient.b1 * input - coefficient.a1 * output + state.z2;
    state.z2 = coefficient.b2 * input - coefficient.a2 * output;
    return output;
}

} // namespace tabletfx
