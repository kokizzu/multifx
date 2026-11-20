cmake_minimum_required(VERSION 3.20)
project(TabletFX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tabletfx_core
    src/effects.cpp
    src/effects/filters.cpp
    src/effects/pedal_compressor.cpp
    src/wav.cpp
)
target_include_directories(tabletfx_core PUBLIC include)
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

    add_executable(tabletfx_gain_tests tests/test_gain.cpp)
    target_link_libraries(tabletfx_gain_tests PRIVATE tabletfx_core)
    add_test(NAME tabletfx_gain_tests COMMAND tabletfx_gain_tests)
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
