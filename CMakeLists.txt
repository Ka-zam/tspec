cmake_minimum_required(VERSION 3.16)
project(tspec VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED jack)
pkg_check_modules(FFTW3 REQUIRED fftw3)
pkg_check_modules(NCURSES REQUIRED ncursesw)

add_executable(tspec
    src/main.c
    src/audio.c
    src/spectrum.c
    src/display.c
)

target_include_directories(tspec PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${JACK_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
    ${NCURSES_INCLUDE_DIRS}
)

target_link_libraries(tspec PRIVATE
    ${JACK_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ${NCURSES_LIBRARIES}
    m
    pthread
)

target_compile_options(tspec PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Debug>:-g -O0>
)
