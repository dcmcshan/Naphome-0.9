cmake_minimum_required(VERSION 3.5)

# Include only the specific components we need, not the entire components directory
# This avoids pulling in perf_tester which requires esp-sr
# Paths are relative to project root
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS
    ${PROJECT_ROOT}/esp-skainet/components/hardware_driver
    ${PROJECT_ROOT}/esp-skainet/components/player
    ${PROJECT_ROOT}/esp-skainet/components/sr_ringbuf
    )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(naphome_phase09_test)
