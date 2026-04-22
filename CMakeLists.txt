# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PACKAGE_VERSION "artifact-ae")

project(souffle VERSION 0.0.0
                DESCRIPTION "Probabilistic Souffle artifact"
                LANGUAGES CXX)
include(CTest)

# Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()


if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build" FORCE)
endif()
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# Tells us whether we're building souffle as a main project
# or a subprobject
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(SOUFFLE_MASTER_PROJECT On)
else()
    set(SOUFFLE_MASTER_PROJECT Off)
endif()

set(SOUFFLE_ENABLE_TESTING_DEFAULT ON)

# --------------------------------------------------
# User options available from the command line/cache
# --------------------------------------------------
option(SOUFFLE_DOMAIN_64BIT "Enable/Disable 64-bit number values in Datalog tuples" OFF)
option(SOUFFLE_USE_ZLIB "Enable/Disable use of libz file compression" ON)
option(SOUFFLE_USE_SQLITE "Enable/Disable use sqlite IO" ON)
option(SOUFFLE_USE_OPENMP "Enable/Disable use of openmp if available" ON)
option(SOUFFLE_SANITISE_MEMORY "Enable/Disable memory sanitiser" OFF)
option(SOUFFLE_SANITISE_THREAD "Enable/Disable thread sanitiser" OFF)
# SOUFFLE_NDEBUG = ON means -DNDEBUG on the compiler command line = no cassert
# Therefor SOUFFLE_NDEBUG = OFF means keep asserts
option(SOUFFLE_NDEBUG "Enable/Disable runtime checks even in release mode" OFF)
option(SOUFFLE_ENABLE_TESTING "Enable/Disable testing" ${SOUFFLE_ENABLE_TESTING_DEFAULT})
option(SOUFFLE_ENABLE_REGRESSION_TESTS "Enable/Disable the maintained regression test suite" ON)
option(SOUFFLE_USE_LIBFFI "Enable/Disable use of libffi" ON)
option(SOUFFLE_CUSTOM_GETOPTLONG "Enable/Disable custom getopt_long implementation" OFF)

# Using Clang? Likely want to use `lld` too.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
endif()

# Add additional modules to CMake
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# --------------------------------------------------
# flex and bison
# --------------------------------------------------
find_package(FLEX REQUIRED)
find_package(BISON "3.2" REQUIRED)

# --------------------------------------------------
# mcpp
# --------------------------------------------------
find_program(MCPP mcpp)

# --------------------------------------------------
# libz
# --------------------------------------------------
if (SOUFFLE_USE_ZLIB)
    find_package(ZLIB REQUIRED)
endif()

# --------------------------------------------------
# sqlite
# --------------------------------------------------
if (SOUFFLE_USE_SQLITE)
    find_package(SQLite3 REQUIRED)
endif()

# --------------------------------------------------
# libffi
# --------------------------------------------------
if (SOUFFLE_USE_LIBFFI)
  find_package(libffi CONFIG QUIET)
  if (NOT libffi_FOUND)
    find_package(LibFFI REQUIRED)
  endif()
endif()

# --------------------------------------------------
# pthreads
# --------------------------------------------------
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# --------------------------------------------------
# OpenMP
# --------------------------------------------------
if (SOUFFLE_USE_OPENMP)
  find_package(OpenMP)
endif()

# --------------------------------------------------
# Memory Sanitiser
# --------------------------------------------------
if (SOUFFLE_SANITISE_MEMORY)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,leak,undefined")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,leak,undefined")
    else()
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,leak")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,leak")
    endif()
endif()

# --------------------------------------------------
# Thread Sanitiser
# --------------------------------------------------
if (SOUFFLE_SANITISE_THREAD)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

# --------------------------------------------------
# Generate the config file
# --------------------------------------------------
configure_file("${PROJECT_SOURCE_DIR}/cmake/config.h.in"
               "${PROJECT_BINARY_DIR}/src/config.h")

# --------------------------------------------------
# Change compile flags
# --------------------------------------------------
if (NOT SOUFFLE_NDEBUG)
    foreach(FLAG_VAR
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL
            CMAKE_CXX_FLAGS_RELWITHDEBINFO)

            # Remove/keep NDEBUG in Release builds
            string(REGEX REPLACE "-DNDEBUG" "" ${FLAG_VAR} "${${FLAG_VAR}}")
    endforeach()
endif()

add_subdirectory(src)

if (SOUFFLE_ENABLE_TESTING AND BUILD_TESTING)
    find_package(Python3 3.10 REQUIRED)
    if (SOUFFLE_ENABLE_REGRESSION_TESTS)
        add_subdirectory(tests/regression)
    endif()
endif()
