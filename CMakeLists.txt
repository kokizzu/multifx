cmake_minimum_required(VERSION 3.20)
project(TabletFX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tabletfx_core
    src/effects.cpp
    src/channel_conversion.cpp
    src/parallel_mixer.cpp
    src/tuner.cpp
    src/effects/clean_boost.cpp
    src/effects/chorus.cpp
    src/effects/triple_double_chorus.cpp
    src/effects/dimension_chorus.cpp
    src/effects/four_band_resonator.cpp
    src/effects/diatonic_harmonizer.cpp
    src/effects/pitch_arpeggiator.cpp
    src/effects/mono_guitar_synth.cpp
    src/effects/fractal_three_voice_synth.cpp
    src/effects/pitch_feedback_delay.cpp
    src/effects/feedback_generator.cpp
    src/effects/shimmer_reverb.cpp
    src/effects/convolution_reverb.cpp
    src/effects/grain_delay.cpp
    src/effects/envelope_wah.cpp
    src/effects/flanger.cpp
    src/effects/manual_thru_zero_flanger.cpp
    src/effects/octobot.cpp
    src/effects/octave_fuzz.cpp
    src/effects/rock_box_dist.cpp
    src/effects/phaser.cpp
    src/effects/overdrive.cpp
    src/effects/mid_overdrive.cpp
    src/effects/high_gain_distortion.cpp
    src/effects/round_fuzz.cpp
    src/effects/filters.cpp
    src/effects/pedal_compressor.cpp
    src/effects/optical_compressor.cpp
    src/effects/fet_compressor.cpp
    src/effects/guitar_graphic_eq.cpp
    src/effects/bass_graphic_eq.cpp
    src/effects/shelf_tilt_eq.cpp
    src/effects/peak_limiter.cpp
    src/effects/poly_pitch_shifter.cpp
    src/effects/tremolo.cpp
    src/effects/harmonic_tremolo.cpp
    src/effects/auto_swell_delay.cpp
    src/effects/shuffling_looper.cpp
    src/effects/ten_tap_delay.cpp
    src/effects/auto_pan.cpp
    src/effects/megatap_delay.cpp
    src/effects/de_esser.cpp
    src/effects/pugilist.cpp
    src/effects/mercator_distortion.cpp
    src/effects/vibe.cpp
    src/effects/multi_delay.cpp
    src/effects/plex_delay.cpp
    src/effects/celestial_reverb.cpp
    src/effects/cloud_reverb.cpp
    src/effects/porta_preamp.cpp
    src/effects/pitch_sequence.cpp
    src/effects/vibrato.cpp
    src/effects/room_reverb.cpp
    src/effects/hall_reverb.cpp
    src/effects/plate_reverb.cpp
    src/effects/spring_reverb.cpp
    src/effects/auto_swell.cpp
    src/effects/rotary_speaker.cpp
    src/effects/ring_modulator.cpp
    src/effects/am_ring_mod.cpp
    src/effects/tape_echo.cpp
    src/effects/rhythmic_slicer.cpp
    src/effects/step_filter.cpp
    src/effects/asheville_pattrn.cpp
    src/effects/notch_filter.cpp
    src/effects/formant_filter.cpp
    src/effects/harmonic_resonator.cpp
    src/effects/multiband_compressor.cpp
    src/effects/bit_crusher.cpp
    src/effects/reverse_delay.cpp
    src/effects/ducking_delay.cpp
    src/effects/modulated_delay.cpp
    src/effects/filtered_delay.cpp
    src/effects/resonant_delay.cpp
    src/effects/sweep_delay.cpp
    src/effects/stereo_crossover.cpp
    src/wav.cpp
    src/effects/volume_pedal.cpp
    src/effects/wah.cpp
    src/level_meter.cpp
    src/signal_controls.cpp
)
target_include_directories(tabletfx_core PUBLIC include)
target_include_directories(tabletfx_core SYSTEM PRIVATE
    third_party/stft-pitch-shift
)
target_compile_definitions(tabletfx_core PUBLIC TABLETFX_BUILD_TYPE="$<CONFIG>")
if(MSVC)
    target_compile_options(tabletfx_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(tabletfx_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(tabletfx_offline apps/offline_render.cpp)
target_link_libraries(tabletfx_offline PRIVATE tabletfx_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(tabletfx_tests tests/test_dsp.cpp)
    target_link_libraries(tabletfx_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_tests COMMAND tabletfx_tests)

    add_executable(tabletfx_compressor_tests tests/test_compressor.cpp)
    target_link_libraries(tabletfx_compressor_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_compressor_tests COMMAND tabletfx_compressor_tests)

    add_executable(tabletfx_three_band_eq_tests tests/test_three_band_eq.cpp)
    target_link_libraries(tabletfx_three_band_eq_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_three_band_eq_tests COMMAND tabletfx_three_band_eq_tests)

    add_executable(tabletfx_five_band_eq_tests tests/test_five_band_eq.cpp)
    target_link_libraries(tabletfx_five_band_eq_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_five_band_eq_tests COMMAND tabletfx_five_band_eq_tests)

    add_executable(tabletfx_filter_tests tests/test_filters.cpp)
    target_link_libraries(tabletfx_filter_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_filter_tests COMMAND tabletfx_filter_tests)

    add_executable(tabletfx_pedal_compressor_tests tests/test_pedal_compressor.cpp)
    target_link_libraries(tabletfx_pedal_compressor_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_pedal_compressor_tests COMMAND tabletfx_pedal_compressor_tests)

    add_executable(tabletfx_optical_compressor_tests tests/test_optical_compressor.cpp)
    target_link_libraries(tabletfx_optical_compressor_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_optical_compressor_tests COMMAND tabletfx_optical_compressor_tests)

    add_executable(tabletfx_fet_compressor_tests tests/test_fet_compressor.cpp)
    target_link_libraries(tabletfx_fet_compressor_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_fet_compressor_tests COMMAND tabletfx_fet_compressor_tests)

    add_executable(tabletfx_guitar_graphic_eq_tests tests/test_guitar_graphic_eq.cpp)
    target_link_libraries(tabletfx_guitar_graphic_eq_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_guitar_graphic_eq_tests COMMAND tabletfx_guitar_graphic_eq_tests)

    add_executable(tabletfx_bass_graphic_eq_tests tests/test_bass_graphic_eq.cpp)
    target_link_libraries(tabletfx_bass_graphic_eq_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_bass_graphic_eq_tests COMMAND tabletfx_bass_graphic_eq_tests)

    add_executable(tabletfx_shelf_tilt_eq_tests tests/test_shelf_tilt_eq.cpp)
    target_link_libraries(tabletfx_shelf_tilt_eq_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_shelf_tilt_eq_tests COMMAND tabletfx_shelf_tilt_eq_tests)

    add_executable(tabletfx_noise_gate_tests tests/test_noise_gate.cpp)
    target_link_libraries(tabletfx_noise_gate_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_noise_gate_tests COMMAND tabletfx_noise_gate_tests)

    add_executable(tabletfx_clean_boost_tests tests/test_clean_boost.cpp)
    target_link_libraries(tabletfx_clean_boost_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_clean_boost_tests COMMAND tabletfx_clean_boost_tests)

    add_executable(tabletfx_chorus_tests tests/test_chorus.cpp)
    target_link_libraries(tabletfx_chorus_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_chorus_tests COMMAND tabletfx_chorus_tests)

    add_executable(tabletfx_dimension_chorus_tests tests/test_dimension_chorus.cpp)
    target_link_libraries(tabletfx_dimension_chorus_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_dimension_chorus_tests COMMAND tabletfx_dimension_chorus_tests)

    add_executable(tabletfx_triple_double_chorus_tests tests/test_triple_double_chorus.cpp)
    target_link_libraries(tabletfx_triple_double_chorus_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_triple_double_chorus_tests COMMAND tabletfx_triple_double_chorus_tests)

    add_executable(tabletfx_four_band_resonator_tests tests/test_four_band_resonator.cpp)
    target_link_libraries(tabletfx_four_band_resonator_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_four_band_resonator_tests COMMAND tabletfx_four_band_resonator_tests)

    add_executable(tabletfx_diatonic_harmonizer_tests tests/test_diatonic_harmonizer.cpp)
    target_link_libraries(tabletfx_diatonic_harmonizer_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_diatonic_harmonizer_tests COMMAND tabletfx_diatonic_harmonizer_tests)

    add_executable(tabletfx_pitch_arpeggiator_tests tests/test_pitch_arpeggiator.cpp)
    target_link_libraries(tabletfx_pitch_arpeggiator_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_pitch_arpeggiator_tests COMMAND tabletfx_pitch_arpeggiator_tests)

    add_executable(tabletfx_mono_guitar_synth_tests tests/test_mono_guitar_synth.cpp)
    target_link_libraries(tabletfx_mono_guitar_synth_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_mono_guitar_synth_tests COMMAND tabletfx_mono_guitar_synth_tests)

    add_executable(tabletfx_note_generator_tests tests/test_note_generator.cpp)
    target_link_libraries(tabletfx_note_generator_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_note_generator_tests COMMAND tabletfx_note_generator_tests)

    add_executable(tabletfx_fractal_three_voice_synth_tests
        tests/test_fractal_three_voice_synth.cpp)
    target_link_libraries(tabletfx_fractal_three_voice_synth_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_fractal_three_voice_synth_tests
        COMMAND tabletfx_fractal_three_voice_synth_tests)

    add_executable(tabletfx_enhancer_tests tests/test_enhancer.cpp)
    target_link_libraries(tabletfx_enhancer_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_enhancer_tests COMMAND tabletfx_enhancer_tests)

    add_executable(tabletfx_looper_tests tests/test_looper.cpp)
    target_link_libraries(tabletfx_looper_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_looper_tests COMMAND tabletfx_looper_tests)

    add_executable(tabletfx_pitch_feedback_delay_tests tests/test_pitch_feedback_delay.cpp)
    target_link_libraries(tabletfx_pitch_feedback_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_pitch_feedback_delay_tests COMMAND tabletfx_pitch_feedback_delay_tests)

    add_executable(tabletfx_feedback_generator_tests tests/test_feedback_generator.cpp)
    target_link_libraries(tabletfx_feedback_generator_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_feedback_generator_tests COMMAND tabletfx_feedback_generator_tests)

    add_executable(tabletfx_shimmer_reverb_tests tests/test_shimmer_reverb.cpp)
    target_link_libraries(tabletfx_shimmer_reverb_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_shimmer_reverb_tests COMMAND tabletfx_shimmer_reverb_tests)

    add_executable(tabletfx_convolution_reverb_tests tests/test_convolution_reverb.cpp)
    target_link_libraries(tabletfx_convolution_reverb_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_convolution_reverb_tests COMMAND tabletfx_convolution_reverb_tests)

    add_executable(tabletfx_grain_delay_tests tests/test_grain_delay.cpp)
    target_link_libraries(tabletfx_grain_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_grain_delay_tests COMMAND tabletfx_grain_delay_tests)

    add_executable(tabletfx_flanger_tests tests/test_flanger.cpp)
    target_link_libraries(tabletfx_flanger_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_flanger_tests COMMAND tabletfx_flanger_tests)

    add_executable(tabletfx_manual_thru_zero_flanger_tests
        tests/test_manual_thru_zero_flanger.cpp)
    target_link_libraries(tabletfx_manual_thru_zero_flanger_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_manual_thru_zero_flanger_tests
        COMMAND tabletfx_manual_thru_zero_flanger_tests)

    add_executable(tabletfx_octobot_tests tests/test_octobot.cpp)
    target_link_libraries(tabletfx_octobot_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_octobot_tests COMMAND tabletfx_octobot_tests)

    add_executable(tabletfx_octave_fuzz_tests tests/test_octave_fuzz.cpp)
    target_link_libraries(tabletfx_octave_fuzz_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_octave_fuzz_tests COMMAND tabletfx_octave_fuzz_tests)

    add_executable(tabletfx_rock_box_dist_tests tests/test_rock_box_dist.cpp)
    target_link_libraries(tabletfx_rock_box_dist_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_rock_box_dist_tests COMMAND tabletfx_rock_box_dist_tests)

    add_executable(tabletfx_phaser_tests tests/test_phaser.cpp)
    target_link_libraries(tabletfx_phaser_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_phaser_tests COMMAND tabletfx_phaser_tests)

    add_executable(tabletfx_envelope_wah_tests tests/test_envelope_wah.cpp)
    target_link_libraries(tabletfx_envelope_wah_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_envelope_wah_tests COMMAND tabletfx_envelope_wah_tests)

    add_executable(tabletfx_signal_controls_tests tests/test_signal_controls.cpp)
    target_link_libraries(tabletfx_signal_controls_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_signal_controls_tests COMMAND tabletfx_signal_controls_tests)

    add_executable(tabletfx_level_meter_tests tests/test_level_meter.cpp)
    target_link_libraries(tabletfx_level_meter_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_level_meter_tests COMMAND tabletfx_level_meter_tests)

    add_executable(tabletfx_channel_conversion_tests tests/test_channel_conversion.cpp)
    target_link_libraries(tabletfx_channel_conversion_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_channel_conversion_tests COMMAND tabletfx_channel_conversion_tests)

    add_executable(tabletfx_parallel_mixer_tests tests/test_parallel_mixer.cpp)
    target_link_libraries(tabletfx_parallel_mixer_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_parallel_mixer_tests COMMAND tabletfx_parallel_mixer_tests)

    add_executable(tabletfx_tuner_tests tests/test_tuner.cpp)
    target_link_libraries(tabletfx_tuner_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_tuner_tests COMMAND tabletfx_tuner_tests)

    add_executable(tabletfx_poly_pitch_shifter_tests tests/test_poly_pitch_shifter.cpp)
    target_link_libraries(tabletfx_poly_pitch_shifter_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_poly_pitch_shifter_tests COMMAND tabletfx_poly_pitch_shifter_tests)

    add_executable(tabletfx_gain_tests tests/test_gain.cpp)
    target_link_libraries(tabletfx_gain_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_gain_tests COMMAND tabletfx_gain_tests)

    add_executable(tabletfx_peak_limiter_tests tests/test_peak_limiter.cpp)
    target_link_libraries(tabletfx_peak_limiter_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_peak_limiter_tests COMMAND tabletfx_peak_limiter_tests)

    add_executable(tabletfx_volume_pedal_tests tests/test_volume_pedal.cpp)
    target_link_libraries(tabletfx_volume_pedal_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_volume_pedal_tests COMMAND tabletfx_volume_pedal_tests)

    add_executable(tabletfx_tremolo_tests tests/test_tremolo.cpp)
    target_link_libraries(tabletfx_tremolo_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_tremolo_tests COMMAND tabletfx_tremolo_tests)

    add_executable(tabletfx_harmonic_tremolo_tests tests/test_harmonic_tremolo.cpp)
    target_link_libraries(tabletfx_harmonic_tremolo_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_harmonic_tremolo_tests COMMAND tabletfx_harmonic_tremolo_tests)

    add_executable(tabletfx_auto_swell_delay_tests tests/test_auto_swell_delay.cpp)
    target_link_libraries(tabletfx_auto_swell_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_auto_swell_delay_tests COMMAND tabletfx_auto_swell_delay_tests)

    add_executable(tabletfx_shuffling_looper_tests tests/test_shuffling_looper.cpp)
    target_link_libraries(tabletfx_shuffling_looper_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_shuffling_looper_tests COMMAND tabletfx_shuffling_looper_tests)

    add_executable(tabletfx_ten_tap_delay_tests tests/test_ten_tap_delay.cpp)
    target_link_libraries(tabletfx_ten_tap_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_ten_tap_delay_tests COMMAND tabletfx_ten_tap_delay_tests)

    add_executable(tabletfx_auto_pan_tests tests/test_auto_pan.cpp)
    target_link_libraries(tabletfx_auto_pan_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_auto_pan_tests COMMAND tabletfx_auto_pan_tests)

    add_executable(tabletfx_megatap_delay_tests tests/test_megatap_delay.cpp)
    target_link_libraries(tabletfx_megatap_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_megatap_delay_tests COMMAND tabletfx_megatap_delay_tests)

    add_executable(tabletfx_de_esser_tests tests/test_de_esser.cpp)
    target_link_libraries(tabletfx_de_esser_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_de_esser_tests COMMAND tabletfx_de_esser_tests)

    add_executable(tabletfx_pugilist_tests tests/test_pugilist.cpp)
    target_link_libraries(tabletfx_pugilist_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_pugilist_tests COMMAND tabletfx_pugilist_tests)

    add_executable(tabletfx_mercator_distortion_tests
        tests/test_mercator_distortion.cpp)
    target_link_libraries(tabletfx_mercator_distortion_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_mercator_distortion_tests
        COMMAND tabletfx_mercator_distortion_tests)

    add_executable(tabletfx_vibe_tests tests/test_vibe.cpp)
    target_link_libraries(tabletfx_vibe_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_vibe_tests COMMAND tabletfx_vibe_tests)

    add_executable(tabletfx_multi_delay_tests tests/test_multi_delay.cpp)
    target_link_libraries(tabletfx_multi_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_multi_delay_tests COMMAND tabletfx_multi_delay_tests)

    add_executable(tabletfx_plex_delay_tests tests/test_plex_delay.cpp)
    target_link_libraries(tabletfx_plex_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_plex_delay_tests COMMAND tabletfx_plex_delay_tests)

    add_executable(tabletfx_celestial_reverb_tests tests/test_celestial_reverb.cpp)
    target_link_libraries(tabletfx_celestial_reverb_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_celestial_reverb_tests COMMAND tabletfx_celestial_reverb_tests)

    add_executable(tabletfx_porta_preamp_tests tests/test_porta_preamp.cpp)
    target_link_libraries(tabletfx_porta_preamp_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_porta_preamp_tests COMMAND tabletfx_porta_preamp_tests)

    add_executable(tabletfx_pitch_sequence_tests tests/test_pitch_sequence.cpp)
    target_link_libraries(tabletfx_pitch_sequence_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_pitch_sequence_tests COMMAND tabletfx_pitch_sequence_tests)

    add_executable(tabletfx_vibrato_tests tests/test_vibrato.cpp)
    target_link_libraries(tabletfx_vibrato_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_vibrato_tests COMMAND tabletfx_vibrato_tests)

    add_executable(tabletfx_overdrive_tests tests/test_overdrive.cpp)
    target_link_libraries(tabletfx_overdrive_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_overdrive_tests COMMAND tabletfx_overdrive_tests)

    add_executable(tabletfx_mid_overdrive_tests tests/test_mid_overdrive.cpp)
    target_link_libraries(tabletfx_mid_overdrive_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_mid_overdrive_tests COMMAND tabletfx_mid_overdrive_tests)

    add_executable(tabletfx_high_gain_distortion_tests tests/test_high_gain_distortion.cpp)
    target_link_libraries(tabletfx_high_gain_distortion_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_high_gain_distortion_tests COMMAND tabletfx_high_gain_distortion_tests)

    add_executable(tabletfx_round_fuzz_tests tests/test_round_fuzz.cpp)
    target_link_libraries(tabletfx_round_fuzz_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_round_fuzz_tests COMMAND tabletfx_round_fuzz_tests)

    add_executable(tabletfx_delay_tests tests/test_delay.cpp)
    target_link_libraries(tabletfx_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_delay_tests COMMAND tabletfx_delay_tests)

    add_executable(tabletfx_room_reverb_tests tests/test_room_reverb.cpp)
    target_link_libraries(tabletfx_room_reverb_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_room_reverb_tests COMMAND tabletfx_room_reverb_tests)

    add_executable(tabletfx_hall_reverb_tests tests/test_hall_reverb.cpp)
    target_link_libraries(tabletfx_hall_reverb_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_hall_reverb_tests COMMAND tabletfx_hall_reverb_tests)

    add_executable(tabletfx_plate_reverb_tests tests/test_plate_reverb.cpp)
    target_link_libraries(tabletfx_plate_reverb_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_plate_reverb_tests COMMAND tabletfx_plate_reverb_tests)

    add_executable(tabletfx_spring_reverb_tests tests/test_spring_reverb.cpp)
    target_link_libraries(tabletfx_spring_reverb_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_spring_reverb_tests COMMAND tabletfx_spring_reverb_tests)

    add_executable(tabletfx_cloud_reverb_tests tests/test_cloud_reverb.cpp)
    target_link_libraries(tabletfx_cloud_reverb_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_cloud_reverb_tests COMMAND tabletfx_cloud_reverb_tests)

    add_executable(tabletfx_auto_swell_tests tests/test_auto_swell.cpp)
    target_link_libraries(tabletfx_auto_swell_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_auto_swell_tests COMMAND tabletfx_auto_swell_tests)

    add_executable(tabletfx_rotary_speaker_tests tests/test_rotary_speaker.cpp)
    target_link_libraries(tabletfx_rotary_speaker_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_rotary_speaker_tests COMMAND tabletfx_rotary_speaker_tests)

    add_executable(tabletfx_ring_modulator_tests tests/test_ring_modulator.cpp)
    target_link_libraries(tabletfx_ring_modulator_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_ring_modulator_tests COMMAND tabletfx_ring_modulator_tests)

    add_executable(tabletfx_am_ring_mod_tests tests/test_am_ring_mod.cpp)
    target_link_libraries(tabletfx_am_ring_mod_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_am_ring_mod_tests COMMAND tabletfx_am_ring_mod_tests)

    add_executable(tabletfx_tape_echo_tests tests/test_tape_echo.cpp)
    target_link_libraries(tabletfx_tape_echo_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_tape_echo_tests COMMAND tabletfx_tape_echo_tests)

    add_executable(tabletfx_rhythmic_slicer_tests tests/test_rhythmic_slicer.cpp)
    target_link_libraries(tabletfx_rhythmic_slicer_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_rhythmic_slicer_tests COMMAND tabletfx_rhythmic_slicer_tests)

    add_executable(tabletfx_step_filter_tests tests/test_step_filter.cpp)
    target_link_libraries(tabletfx_step_filter_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_step_filter_tests COMMAND tabletfx_step_filter_tests)

    add_executable(tabletfx_asheville_pattrn_tests tests/test_asheville_pattrn.cpp)
    target_link_libraries(tabletfx_asheville_pattrn_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_asheville_pattrn_tests COMMAND tabletfx_asheville_pattrn_tests)

    add_executable(tabletfx_notch_filter_tests tests/test_notch_filter.cpp)
    target_link_libraries(tabletfx_notch_filter_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_notch_filter_tests COMMAND tabletfx_notch_filter_tests)

    add_executable(tabletfx_formant_filter_tests tests/test_formant_filter.cpp)
    target_link_libraries(tabletfx_formant_filter_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_formant_filter_tests COMMAND tabletfx_formant_filter_tests)

    add_executable(tabletfx_harmonic_resonator_tests tests/test_harmonic_resonator.cpp)
    target_link_libraries(tabletfx_harmonic_resonator_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_harmonic_resonator_tests COMMAND tabletfx_harmonic_resonator_tests)

    add_executable(tabletfx_multiband_compressor_tests tests/test_multiband_compressor.cpp)
    target_link_libraries(tabletfx_multiband_compressor_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_multiband_compressor_tests COMMAND tabletfx_multiband_compressor_tests)

    add_executable(tabletfx_bit_crusher_tests tests/test_bit_crusher.cpp)
    target_link_libraries(tabletfx_bit_crusher_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_bit_crusher_tests COMMAND tabletfx_bit_crusher_tests)

    add_executable(tabletfx_reverse_delay_tests tests/test_reverse_delay.cpp)
    target_link_libraries(tabletfx_reverse_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_reverse_delay_tests COMMAND tabletfx_reverse_delay_tests)

    add_executable(tabletfx_ducking_delay_tests tests/test_ducking_delay.cpp)
    target_link_libraries(tabletfx_ducking_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_ducking_delay_tests COMMAND tabletfx_ducking_delay_tests)

    add_executable(tabletfx_modulated_delay_tests tests/test_modulated_delay.cpp)
    target_link_libraries(tabletfx_modulated_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_modulated_delay_tests COMMAND tabletfx_modulated_delay_tests)

    add_executable(tabletfx_filtered_delay_tests tests/test_filtered_delay.cpp)
    target_link_libraries(tabletfx_filtered_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_filtered_delay_tests COMMAND tabletfx_filtered_delay_tests)
    add_executable(tabletfx_resonant_delay_tests tests/test_resonant_delay.cpp)
    target_link_libraries(tabletfx_resonant_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_resonant_delay_tests COMMAND tabletfx_resonant_delay_tests)

    add_executable(tabletfx_sweep_delay_tests tests/test_sweep_delay.cpp)
    target_link_libraries(tabletfx_sweep_delay_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_sweep_delay_tests COMMAND tabletfx_sweep_delay_tests)

    add_executable(tabletfx_wah_tests tests/test_wah.cpp)
    target_link_libraries(tabletfx_wah_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_wah_tests COMMAND tabletfx_wah_tests)

    add_executable(tabletfx_stereo_crossover_tests tests/test_stereo_crossover.cpp)
    target_link_libraries(tabletfx_stereo_crossover_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_stereo_crossover_tests COMMAND tabletfx_stereo_crossover_tests)

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME tabletfx_offline_cli_test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/test_offline_cli.py
                    --executable $<TARGET_FILE:tabletfx_offline>
        )
    endif()
endif()

if(WIN32)
    add_library(tabletfx_wasapi STATIC src/audio/wasapi_backend.cpp)
    target_include_directories(tabletfx_wasapi PUBLIC include)
    if(MSVC)
        target_compile_options(tabletfx_wasapi PRIVATE /W4 /permissive-)
    else()
        target_compile_options(tabletfx_wasapi PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    target_link_libraries(tabletfx_wasapi PRIVATE ole32 avrt uuid)

    add_executable(tabletfx_wasapi_compile tests/wasapi_compile.cpp)
    target_link_libraries(tabletfx_wasapi_compile PRIVATE tabletfx_wasapi)
    if(BUILD_TESTING)
        add_test(NAME tabletfx_wasapi_compile_test COMMAND tabletfx_wasapi_compile)
    endif()
endif()
