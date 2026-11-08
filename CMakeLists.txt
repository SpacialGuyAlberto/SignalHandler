cmake_minimum_required(VERSION 3.20)
project(gpr3d
    VERSION 2.0.0
    DESCRIPTION "GPR/GDR Radar Signal Processing, 3D Semantic Object Reconstruction, Graphical Fluctuations & Georeferenced Point Clouds"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GPR3D_USE_FFTW3 "Use FFTW3 for accelerated spectral/Hilbert transforms if available" ON)
option(GPR3D_USE_CAIRO "Use Cairo for PNG and vector SVG fluctuation charts if available" ON)
option(GPR3D_USE_OPENGL "Use GLFW3 + OpenGL for native interactive 3D/2D desktop viewer if available" ON)
option(GPR3D_BUILD_CLI "Build the gpr3d command-line interface" ON)
option(GPR3D_BUILD_TESTS "Build unit and integration tests" ON)

# Detect optional FFTW3
set(GPR3D_HAS_FFTW3 OFF)
if(GPR3D_USE_FFTW3)
    find_path(FFTW3_INCLUDE_DIR NAMES fftw3.h)
    find_library(FFTW3_LIBRARY NAMES fftw3)
    if(FFTW3_INCLUDE_DIR AND FFTW3_LIBRARY)
        message(STATUS "Found FFTW3: ${FFTW3_LIBRARY} (include: ${FFTW3_INCLUDE_DIR})")
        set(GPR3D_HAS_FFTW3 ON)
    else()
        message(STATUS "FFTW3 not found; falling back to built-in C++20 Cooley-Tukey FFT")
    endif()
endif()

# Detect optional Cairo (PNG + SVG plotting)
set(GPR3D_HAS_CAIRO OFF)
if(GPR3D_USE_CAIRO)
    find_path(CAIRO_INCLUDE_DIR NAMES cairo/cairo.h)
    find_library(CAIRO_LIBRARY NAMES cairo)
    if(CAIRO_INCLUDE_DIR AND CAIRO_LIBRARY)
        message(STATUS "Found Cairo: ${CAIRO_LIBRARY} (include: ${CAIRO_INCLUDE_DIR})")
        set(GPR3D_HAS_CAIRO ON)
    else()
        message(STATUS "Cairo not found; PNG/SVG static chart rendering disabled")
    endif()
endif()

# Detect optional GLFW3 + OpenGL (Native interactive viewer)
set(GPR3D_HAS_OPENGL OFF)
if(GPR3D_USE_OPENGL)
    find_path(GLFW3_INCLUDE_DIR NAMES GLFW/glfw3.h)
    find_library(GLFW3_LIBRARY NAMES glfw glfw3)
    find_path(GL_INCLUDE_DIR NAMES GL/gl.h)
    find_library(GL_LIBRARY NAMES GL OpenGL)
    if(GLFW3_INCLUDE_DIR AND GLFW3_LIBRARY AND GL_INCLUDE_DIR AND GL_LIBRARY)
        message(STATUS "Found GLFW3 + OpenGL: ${GLFW3_LIBRARY}, ${GL_LIBRARY}")
        set(GPR3D_HAS_OPENGL ON)
    else()
        message(STATUS "GLFW3/OpenGL not found; native interactive window disabled")
    endif()
endif()

# Core library
add_library(gpr3d
    src/geo/crs_transform.cpp
    src/geo/material_db.cpp
    src/geo/trajectory_sync.cpp
    src/io/gps_reader.cpp
    src/io/reader.cpp
    src/io/segy_reader.cpp
    src/io/dzt_reader.cpp
    src/io/rd3_reader.cpp
    src/io/exporter_3d.cpp
    src/io/scene_exporter.cpp
    src/dsp/fft.cpp
    src/dsp/signal_processor.cpp
    src/dsp/interpreter.cpp
    src/dsp/soil_inversion.cpp
    src/dsp/object_transformer.cpp
    src/viz/fluctuation_plotter.cpp
    src/viz/gl_viewer.cpp
    src/sim/synthetic_generator.cpp
)

add_library(gpr3d::gpr3d ALIAS gpr3d)

target_include_directories(gpr3d
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(GPR3D_HAS_FFTW3)
    target_compile_definitions(gpr3d PUBLIC GPR3D_USE_FFTW3=1)
    target_include_directories(gpr3d PRIVATE ${FFTW3_INCLUDE_DIR})
    target_link_libraries(gpr3d PRIVATE ${FFTW3_LIBRARY})
endif()

if(GPR3D_HAS_CAIRO)
    target_compile_definitions(gpr3d PUBLIC GPR3D_USE_CAIRO=1)
    target_include_directories(gpr3d PRIVATE ${CAIRO_INCLUDE_DIR})
    target_link_libraries(gpr3d PRIVATE ${CAIRO_LIBRARY})
endif()

if(GPR3D_HAS_OPENGL)
    target_compile_definitions(gpr3d PUBLIC GPR3D_USE_OPENGL=1)
    target_include_directories(gpr3d PRIVATE ${GLFW3_INCLUDE_DIR} ${GL_INCLUDE_DIR})
    target_link_libraries(gpr3d PRIVATE ${GLFW3_LIBRARY} ${GL_LIBRARY})
endif()

if(MSVC)
    target_compile_options(gpr3d PRIVATE /W4)
else()
    target_compile_options(gpr3d PRIVATE -Wall -Wextra -Wpedantic)
endif()

# CLI application
if(GPR3D_BUILD_CLI)
    add_executable(gpr3d_cli apps/gpr3d_cli.cpp)
    set_target_properties(gpr3d_cli PROPERTIES OUTPUT_NAME gpr3d)
    target_link_libraries(gpr3d_cli PRIVATE gpr3d::gpr3d)
    if(NOT MSVC)
        target_compile_options(gpr3d_cli PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# Unit and integration tests
if(GPR3D_BUILD_TESTS)
    enable_testing()
    add_executable(test_gpr3d tests/test_gpr3d.cpp)
    target_link_libraries(test_gpr3d PRIVATE gpr3d::gpr3d)
    if(NOT MSVC)
        target_compile_options(test_gpr3d PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME gpr3d_all_tests COMMAND test_gpr3d)
endif()
