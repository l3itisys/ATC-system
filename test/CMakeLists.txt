# In test/CMakeLists.txt
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

set(TEST_SRC_FILES
    sim/sim_test.cpp
)

add_executable(runTests ${TEST_SRC_FILES})
target_link_libraries(runTests ${GTEST_LIBRARIES} pthread)
add_test(NAME AircraftTest COMMAND runTests)

