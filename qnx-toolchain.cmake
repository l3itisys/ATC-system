# qnx-toolchain.cmake

# Specify the cross-compilation toolchain
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 7.0)
set(QNX_TARGET_CPU x86_64)

# Specify the paths to the QNX tools
set(QNX_HOST "/home/tycia/qnx710/host/linux/x86_64" CACHE PATH "QNX host")
set(QNX_TARGET "/home/tycia/qnx710/target/qnx7" CACHE PATH "QNX target")

set(CMAKE_SYSROOT "${QNX_TARGET}" CACHE PATH "QNX sysroot")

set(CMAKE_C_COMPILER "${QNX_HOST}/usr/bin/qcc")
set(CMAKE_CXX_COMPILER "${QNX_HOST}/usr/bin/q++")

# Specify the compiler flags
set(CMAKE_C_FLAGS "-Vgcc_nto${QNX_TARGET_CPU} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "-Vgcc_nto${QNX_TARGET_CPU}_gpp ${CMAKE_CXX_FLAGS}")

# Ensure that CMake uses the sysroot paths
set(CMAKE_FIND_ROOT_PATH "${QNX_TARGET}" CACHE PATH "QNX find root path")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

