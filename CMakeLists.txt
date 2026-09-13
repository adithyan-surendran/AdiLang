cmake_minimum_required(VERSION 3.10)

project(AdiLang)

set(CMAKE_CXX_STANDARD 17)

# Include base src directory so "core/..." and "modules/..." resolve properly
include_directories(src)

# Collect all source files from core
file(GLOB CORE_SOURCES
    "src/core/*.cpp"
)

# Set output binary directory to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Add executable target
add_executable(adilang ${CORE_SOURCES})