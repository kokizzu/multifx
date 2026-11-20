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
    src/effects/envelope_wah.cpp
    src/effects/filters.cpp
    src/effects/pedal_compressor.cpp
    src/effects/peak_limiter.cpp
    src/effects/poly_pitch_shifter.cpp
    src/effects/tremolo.cpp
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

    add_executable(tabletfx_filter_tests tests/test_filters.cpp)
    target_link_libraries(tabletfx_filter_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_filter_tests COMMAND tabletfx_filter_tests)

    add_executable(tabletfx_pedal_compressor_tests tests/test_pedal_compressor.cpp)
    target_link_libraries(tabletfx_pedal_compressor_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_pedal_compressor_tests COMMAND tabletfx_pedal_compressor_tests)

    add_executable(tabletfx_noise_gate_tests tests/test_noise_gate.cpp)
    target_link_libraries(tabletfx_noise_gate_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_noise_gate_tests COMMAND tabletfx_noise_gate_tests)

    add_executable(tabletfx_clean_boost_tests tests/test_clean_boost.cpp)
    target_link_libraries(tabletfx_clean_boost_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_clean_boost_tests COMMAND tabletfx_clean_boost_tests)

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

    add_executable(tabletfx_wah_tests tests/test_wah.cpp)
    target_link_libraries(tabletfx_wah_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_wah_tests COMMAND tabletfx_wah_tests)
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
