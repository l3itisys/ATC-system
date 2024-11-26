# QNX 7.1 toolchain file
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 7.1)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# QNX paths
set(QNX_HOST "$ENV{QNX_HOST}")
set(QNX_TARGET "$ENV{QNX_TARGET}")

# Specify compiler
set(CMAKE_C_COMPILER ${QNX_HOST}/usr/bin/qcc)
set(CMAKE_CXX_COMPILER ${QNX_HOST}/usr/bin/q++)

# Set compiler flags
set(CMAKE_C_FLAGS "-Vgcc_ntoaarch64le" CACHE STRING "C compiler flags")
set(CMAKE_CXX_FLAGS "-Vgcc_ntoaarch64le -Y_cxx" CACHE STRING "C++ compiler flags")

# Set root paths
set(CMAKE_FIND_ROOT_PATH ${QNX_TARGET})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
