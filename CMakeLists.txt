# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(ATCSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${QNX_TARGET}/usr/include)

# Source files by component
set(CORE_SOURCES
    src/core/aircraft.cpp
    src/core/violation_detector.cpp
    src/display/display_system.cpp
    src/common/logger.cpp
    src/common/constants.cpp
    src/common/history_logger.cpp
    src/core/radar_system.cpp
    src/operator/console.cpp
    src/operator/command.cpp
)

set(COMMUNICATION_SOURCES
    src/communication/qnx_channel.cpp
)

# Main executable
add_executable(atc_system
    ${CORE_SOURCES}
    ${COMMUNICATION_SOURCES}
    ${DISPLAY_SOURCES}
    ${COMMON_SOURCES}
    ${OPERATOR_SOURCES}
    src/main.cpp
)

# Link libraries
if(NOT CMAKE_CROSSCOMPILING)
    target_link_libraries(atc_system pthread rt)
endif()

# Testing
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()

    # GTest dependency
    find_package(GTest REQUIRED)
    include_directories(${GTEST_INCLUDE_DIRS})

    # Test executable
    add_executable(run_tests
        test/core/aircraft_test.cpp
        test/display/display_test.cpp
        test/operator/console_test.cpp
    )

    target_link_libraries(run_tests
        ${GTEST_LIBRARIES}
        pthread

    )

    add_test(NAME CoreTests COMMAND run_tests)
endif()
