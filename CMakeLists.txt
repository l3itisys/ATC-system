cmake_minimum_required(VERSION 3.10)
project(ATCSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(include)

# Source files
set(SRC_FILES
    src/sim/aircraft.cpp
    src/sim/channel_manager.cpp
    # Add other source files here
)

# Executable
add_executable(aircraft_sim ${SRC_FILES})

# Link libraries
target_link_libraries(aircraft_sim pthread)

# Tests
enable_testing()
add_subdirectory(test)

