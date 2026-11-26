#include "tabletfx/effects/chorus.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tabletfx {

Chorus::Chorus(float sample_rate, std::uint16_t channels,
               ChorusParameters parameters)
    : sample_rate_(sample_rate), channels_(channels) {
    if (!std::isfinite(sample_rate_) || sample_rate_ < kMinimumSampleRate ||
        sample_rate_ > kMaximumSampleRate || channels_ == 0 || channels_ > 2) {
        throw std::invalid_argument("Chorus requires 8–384 kHz and mono or stereo channels");
    }
    const double frames = std::ceil(static_cast<double>(sample_rate_) *
                                    static_cast<double>(kStorageDelayMs) / 1000.0);
    if (frames > static_cast<double>(std::numeric_limits<std::size_t>::max() - 2)) {
        throw std::length_error("Chorus delay buffer is too large");
    }
    capacity_frames_ = static_cast<std::size_t>(frames) + 2;
    storage_.assign(capacity_frames_ * channels_, 0.0F);
    set_parameters(parameters);
    retarget(true);
}

void Chorus::set_parameters(ChorusParameters parameters) noexcept {
    parameters.rate_hz = bounded(parameters.rate_hz, 0.8F, 0.05F, 8.0F);
    parameters.tempo_bpm = bounded(parameters.tempo_bpm, 120.0F, 20.0F, 300.0F);
    parameters.number_of_voices = std::clamp<std::uint8_t>(
        parameters.number_of_voices, std::uint8_t{1U}, std::uint8_t{8U});
    if (parameters.number_of_voices == 1U) {
        voice_step_sine_ = 0.0F;
        voice_step_cosine_ = 1.0F;
    } else {
        const float voice_step = kTwoPi /
                                 static_cast<float>(parameters.number_of_voices);
        voice_step_sine_ = std::sin(voice_step);
        voice_step_cosine_ = std::cos(voice_step);
    }
    const bool use_legacy_center_depth =
        parameters.minimum_delay_ms == 8.0F &&
        parameters.delay_range_ms == 14.0F &&
        (parameters.base_delay_ms != 15.0F || parameters.depth_ms != 7.0F);
    float center_delay_ms = 0.0F;
    float modulation_depth_ms = 0.0F;
    if (use_legacy_center_depth) {
        center_delay_ms = bounded(parameters.base_delay_ms, 15.0F, 2.0F, 30.0F);
        const float maximum_depth = std::max(
            0.0F, std::min({14.0F, center_delay_ms - 1.0F,
                            30.0F - center_delay_ms}));
        modulation_depth_ms = bounded(parameters.depth_ms,
                                      std::min(7.0F, maximum_depth),
                                      0.0F, maximum_depth);
        parameters.minimum_delay_ms = center_delay_ms - modulation_depth_ms;
        parameters.delay_range_ms = modulation_depth_ms * 2.0F;
    } else {
        parameters.minimum_delay_ms = bounded(
            parameters.minimum_delay_ms, 8.0F, kMinimumDelayMs,
            kMaximumMinimumDelayMs);
        parameters.delay_range_ms = bounded(
            parameters.delay_range_ms, 14.0F, kMinimumDelayRangeMs,
            kMaximumDelayRangeMs);
        center_delay_ms = parameters.minimum_delay_ms +
                          parameters.delay_range_ms * 0.5F;
        modulation_depth_ms = parameters.delay_range_ms * 0.5F;
    }
    // Keep the former public values readable as center/depth aliases. Canonical
    // min/range controls drive new callers; legacy callers that only changed
    // these aliases are mapped above when the canonical fields are untouched.
    parameters.base_delay_ms = center_delay_ms;
    parameters.depth_ms = modulation_depth_ms;
    parameters.mix = bounded(parameters.mix, 0.3F, 0.0F, 1.0F);
    parameters.smoothing_ms = bounded(parameters.smoothing_ms, 10.0F, 0.0F,
                                      1000.0F);
    parameters.low_cut_hz = bounded(parameters.low_cut_hz, 0.0F, 0.0F,
                                    kMaximumLowCutHz);
    parameters.high_cut_hz = bounded(parameters.high_cut_hz, 0.0F, 0.0F,
                                     kMaximumHighCutHz);
    parameters.stereo_width = bounded(parameters.stereo_width, 1.0F, 0.0F,
                                      kMaximumStereoWidth);
    parameters_ = parameters;
    target_base_delay_ms_ = center_delay_ms;
    target_depth_ms_ = modulation_depth_ms;
    target_mix_ = parameters_.mix;
    const float safe_low_cut_hz = std::min(parameters_.low_cut_hz,
                                           sample_rate_ * 0.45F);
    const float safe_high_cut_hz = std::min(parameters_.high_cut_hz,
                                            sample_rate_ * 0.45F);
    target_low_cut_alpha_ = parameters_.low_cut_hz > 0.0F
        ? std::exp(-kTwoPi * safe_low_cut_hz / sample_rate_)
        : 0.0F;
    target_low_cut_mix_ = parameters_.low_cut_hz > 0.0F ? 1.0F : 0.0F;
    target_high_cut_alpha_ = parameters_.high_cut_hz > 0.0F
        ? std::exp(-kTwoPi * safe_high_cut_hz / sample_rate_)
        : 0.0F;
    target_high_cut_mix_ = parameters_.high_cut_hz > 0.0F ? 1.0F : 0.0F;
    target_stereo_width_ = parameters_.stereo_width;
    prepare_rate();
    retarget(false);
}

void Chorus::process(std::span<float> interleaved,
                     std::uint16_t channels) noexcept {
    if (channels != channels_ || channels == 0) {
        return;
    }

    const std::size_t frames = interleaved.size() / channels_;
    const float samples_per_ms = sample_rate_ * 0.001F;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (ramp_remaining_ > 0) {
            current_base_delay_ms_ += base_delay_step_;
            current_depth_ms_ += depth_step_;
            current_mix_ += mix_step_;
            low_cut_alpha_ += low_cut_alpha_step_;
            low_cut_mix_ += low_cut_mix_step_;
            high_cut_alpha_ += high_cut_alpha_step_;
            high_cut_mix_ += high_cut_mix_step_;
            stereo_width_ += stereo_width_step_;
            --ramp_remaining_;
            if (ramp_remaining_ == 0) {
                current_base_delay_ms_ = target_base_delay_ms_;
                current_depth_ms_ = target_depth_ms_;
                current_mix_ = target_mix_;
                low_cut_alpha_ = target_low_cut_alpha_;
                low_cut_mix_ = target_low_cut_mix_;
                high_cut_alpha_ = target_high_cut_alpha_;
                high_cut_mix_ = target_high_cut_mix_;
                stereo_width_ = target_stereo_width_;
                base_delay_step_ = 0.0F;
                depth_step_ = 0.0F;
                mix_step_ = 0.0F;
                low_cut_alpha_step_ = 0.0F;
                low_cut_mix_step_ = 0.0F;
                high_cut_alpha_step_ = 0.0F;
                high_cut_mix_step_ = 0.0F;
                stereo_width_step_ = 0.0F;
            }
        }

        const float base_delay = current_base_delay_ms_ * samples_per_ms;
        const float modulation_depth = current_depth_ms_ * samples_per_ms;
        const std::size_t write_offset = write_frame_ * channels_;
        const std::size_t input_offset = frame * channels_;
        std::array<float, 2U> frame_output{};
        const bool low_cut_active = low_cut_mix_ > 0.0F;
        const bool high_cut_active = high_cut_mix_ > 0.0F;
        for (std::uint16_t channel = 0; channel < channels_; ++channel) {
            const std::size_t index = input_offset + channel;
            const float input = std::isfinite(interleaved[index])
                                    ? interleaved[index]
                                    : 0.0F;
            storage_[write_offset + channel] = input;

            float wet = 0.0F;
            if (parameters_.number_of_voices == 1U) {
                const float modulation = channel == 0 ? sine_phase_ : -sine_phase_;
                const float delay = std::clamp(
                    base_delay + modulation_depth * modulation, 1.0F,
                    static_cast<float>(capacity_frames_ - 2));
                const std::size_t whole_delay = static_cast<std::size_t>(delay);
                const float fraction = delay - static_cast<float>(whole_delay);
                std::size_t read_frame = write_frame_ + capacity_frames_ - whole_delay;
                if (read_frame >= capacity_frames_) {
                    read_frame -= capacity_frames_;
                }
                const std::size_t older_frame = read_frame == 0
                                                    ? capacity_frames_ - 1
                                                    : read_frame - 1;
                const float newer = storage_[read_frame * channels_ + channel];
                const float older = storage_[older_frame * channels_ + channel];
                wet = newer * (1.0F - fraction) + older * fraction;
            } else {
                const float voice_weight =
                    1.0F / static_cast<float>(parameters_.number_of_voices);
                float voice_sine = sine_phase_;
                float voice_cosine = cosine_phase_;
                for (std::uint8_t voice = 0U;
                     voice < parameters_.number_of_voices; ++voice) {
                    const float modulation = channel == 0 ? voice_sine : -voice_sine;
                    const float delay = std::clamp(
                        base_delay + modulation_depth * modulation, 1.0F,
                        static_cast<float>(capacity_frames_ - 2));
                    const std::size_t whole_delay =
                        static_cast<std::size_t>(delay);
                    const float fraction = delay - static_cast<float>(whole_delay);
                    std::size_t read_frame = write_frame_ + capacity_frames_ - whole_delay;
                    if (read_frame >= capacity_frames_) {
                        read_frame -= capacity_frames_;
                    }
                    const std::size_t older_frame = read_frame == 0
                                                        ? capacity_frames_ - 1
                                                        : read_frame - 1;
                    const float newer = storage_[read_frame * channels_ + channel];
                    const float older = storage_[older_frame * channels_ + channel];
                    const float voice_wet =
                        newer * (1.0F - fraction) + older * fraction;
                    wet += voice_wet * voice_weight;

                    if (voice + 1U < parameters_.number_of_voices) {
                        const float previous_sine = voice_sine;
                        voice_sine = previous_sine * voice_step_cosine_ +
                                     voice_cosine * voice_step_sine_;
                        voice_cosine = voice_cosine * voice_step_cosine_ -
                                       previous_sine * voice_step_sine_;
                    }
                }
            }
            const float output = input * (1.0F - current_mix_) + wet * current_mix_;
            const float finite_output = std::isfinite(output) ? output : 0.0F;

            // The one-pole cut coefficients are prepared and ramped outside
            // this loop. Skip their multiply/add paths while the cuts are Off,
            // but keep the histories seeded for smooth activation. No buffer,
            // lookahead, or per-sample transcendental work is used.
            float stage = finite_output;
            if (low_cut_active) {
                const float high_pass = low_cut_alpha_ *
                    (low_cut_previous_output_[channel] + finite_output -
                     low_cut_previous_input_[channel]);
                low_cut_previous_input_[channel] = finite_output;
                low_cut_previous_output_[channel] = std::isfinite(high_pass)
                    ? high_pass : 0.0F;
                stage += low_cut_mix_ *
                    (low_cut_previous_output_[channel] - finite_output);
            } else {
                low_cut_previous_input_[channel] = finite_output;
                low_cut_previous_output_[channel] = finite_output;
            }

            if (high_cut_active) {
                const float low_pass = high_cut_previous_output_[channel] +
                    (1.0F - high_cut_alpha_) *
                        (stage - high_cut_previous_output_[channel]);
                high_cut_previous_output_[channel] = std::isfinite(low_pass)
                    ? low_pass : 0.0F;
                stage += high_cut_mix_ *
                    (high_cut_previous_output_[channel] - stage);
            } else {
                high_cut_previous_output_[channel] = stage;
            }
            frame_output[channel] = std::isfinite(stage) ? stage : 0.0F;
        }
        if (channels_ == 2U && stereo_width_ != 1.0F) {
            const float mid = (frame_output[0U] + frame_output[1U]) * 0.5F;
            const float side = (frame_output[0U] - frame_output[1U]) *
                               (0.5F * stereo_width_);
            frame_output[0U] = mid + side;
            frame_output[1U] = mid - side;
        }
        for (std::uint16_t channel = 0U; channel < channels_; ++channel) {
            interleaved[input_offset + channel] = frame_output[channel];
        }

        ++write_frame_;
        if (write_frame_ == capacity_frames_) {
            write_frame_ = 0;
        }
        advance_modulator();
    }
}

void Chorus::reset() noexcept {
    std::fill(storage_.begin(), storage_.end(), 0.0F);
    low_cut_previous_input_.fill(0.0F);
    low_cut_previous_output_.fill(0.0F);
    high_cut_previous_output_.fill(0.0F);
    write_frame_ = 0;
    sine_phase_ = 0.0F;
    cosine_phase_ = 1.0F;
    normalization_counter_ = 0;
    retarget(true);
}

float Chorus::bounded(float value, float fallback,
                      float minimum, float maximum) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

void Chorus::prepare_rate() noexcept {
    const float step = kTwoPi * effective_rate_hz() / sample_rate_;
    sine_increment_ = std::sin(step);
    cosine_increment_ = std::cos(step);
}

void Chorus::retarget(bool immediate) noexcept {
    if (immediate) {
        current_base_delay_ms_ = target_base_delay_ms_;
        current_depth_ms_ = target_depth_ms_;
        current_mix_ = target_mix_;
        low_cut_alpha_ = target_low_cut_alpha_;
        low_cut_mix_ = target_low_cut_mix_;
        high_cut_alpha_ = target_high_cut_alpha_;
        high_cut_mix_ = target_high_cut_mix_;
        stereo_width_ = target_stereo_width_;
        base_delay_step_ = 0.0F;
        depth_step_ = 0.0F;
        mix_step_ = 0.0F;
        low_cut_alpha_step_ = 0.0F;
        low_cut_mix_step_ = 0.0F;
        high_cut_alpha_step_ = 0.0F;
        high_cut_mix_step_ = 0.0F;
        stereo_width_step_ = 0.0F;
        ramp_remaining_ = 0;
        return;
    }
    const double requested_frames = static_cast<double>(sample_rate_) *
                                    static_cast<double>(parameters_.smoothing_ms) / 1000.0;
    ramp_remaining_ = static_cast<std::size_t>(std::llround(requested_frames));
    if (ramp_remaining_ == 0) {
        retarget(true);
        return;
    }
    const float denominator = static_cast<float>(ramp_remaining_);
    base_delay_step_ = (target_base_delay_ms_ - current_base_delay_ms_) / denominator;
    depth_step_ = (target_depth_ms_ - current_depth_ms_) / denominator;
    mix_step_ = (target_mix_ - current_mix_) / denominator;
    low_cut_alpha_step_ = (target_low_cut_alpha_ - low_cut_alpha_) / denominator;
    low_cut_mix_step_ = (target_low_cut_mix_ - low_cut_mix_) / denominator;
    high_cut_alpha_step_ = (target_high_cut_alpha_ - high_cut_alpha_) / denominator;
    high_cut_mix_step_ = (target_high_cut_mix_ - high_cut_mix_) / denominator;
    stereo_width_step_ = (target_stereo_width_ - stereo_width_) / denominator;
}

void Chorus::advance_modulator() noexcept {
    const float old_sine = sine_phase_;
    sine_phase_ = old_sine * cosine_increment_ +
                  cosine_phase_ * sine_increment_;
    cosine_phase_ = cosine_phase_ * cosine_increment_ -
                    old_sine * sine_increment_;
    if (++normalization_counter_ == 4096U) {
        const float magnitude_squared = sine_phase_ * sine_phase_ +
                                        cosine_phase_ * cosine_phase_;
        if (std::isfinite(magnitude_squared) && magnitude_squared > 0.0F) {
            const float inverse_magnitude = 1.0F / std::sqrt(magnitude_squared);
            sine_phase_ *= inverse_magnitude;
            cosine_phase_ *= inverse_magnitude;
        } else {
            sine_phase_ = 0.0F;
            cosine_phase_ = 1.0F;
        }
        normalization_counter_ = 0;
    }
}

} // namespace tabletfx
