cmake_minimum_required(VERSION 3.20)
project(TabletFX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tabletfx_core
    src/effects.cpp
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
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME tabletfx_offline_cli_test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/test_offline_cli.py
                    --executable $<TARGET_FILE:tabletfx_offline>
        )
    endif()
endif()
