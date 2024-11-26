cmake_minimum_required(VERSION 3.10)
project(ATCSystem VERSION 1.0)

# Set C++14 for QNX 7.1 compatibility
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# QNX specific settings
if(CMAKE_SYSTEM_NAME MATCHES "QNX")
    set(QNX_TARGET $ENV{QNX_TARGET})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_QNX_SOURCE")
endif()

# Source files
set(COMMON_SOURCES
    src/common/constants.cpp
    src/common/logger.cpp
    src/common/history_logger.cpp
)

set(CORE_SOURCES
    src/core/aircraft.cpp
    src/core/radar_system.cpp
    src/core/violation_detector.cpp
    src/main_system.cpp
)

set(DISPLAY_SOURCES
    src/display/display_system.cpp
)

set(OPERATOR_SOURCES
    src/operator/command.cpp
    src/operator/console.cpp
)

set(COMMUNICATION_SOURCES
    src/communication/qnx_channel.cpp
)

set(MAIN_SOURCES
    src/main.cpp
)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Create executable
add_executable(atc_system
    ${COMMON_SOURCES}
    ${CORE_SOURCES}
    ${DISPLAY_SOURCES}
    ${OPERATOR_SOURCES}
    ${COMMUNICATION_SOURCES}
    ${MAIN_SOURCES}
)

# Find and link threads
find_package(Threads REQUIRED)
target_link_libraries(atc_system PRIVATE Threads::Threads)

if(CMAKE_SYSTEM_NAME MATCHES "QNX")
    target_link_libraries(atc_system PRIVATE socket)
endif()

