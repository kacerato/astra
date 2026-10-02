cmake_minimum_required(VERSION 3.28)
project(Astra LANGUAGES C CXX)

set(ASTRA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${ASTRA_ROOT}/cmake")

include(TheForge)
include(Glslang)

option(ASTRA_BUILD_SPIKES "Compila os spikes da F0 (spikes/)" ON)
if(ASTRA_BUILD_SPIKES)
    add_subdirectory(spikes/s01_forge_triangle)
    add_subdirectory(spikes/s02_gameactivity)
endif()
