# Souffle - A Datalog Compiler
# Copyright (c) 2021 The Souffle Developers. All rights reserved
# Licensed under the Universal Permissive License v 1.0 as shown at:
# - https://opensource.org/licenses/UPL
# - <souffle root>/licenses/SOUFFLE-UPL.txt

cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CMakeDependentOption)

option(SOUFFLE_GIT "Enable/Disable git completion" ON)

if (SOUFFLE_GIT) 
    find_package(Git REQUIRED)

    # PACKAGE_VERSION is the full tag with git hash
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --always
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    RESULT_VARIABLE GIT_RESULT
                    OUTPUT_VARIABLE GIT_PACKAGE_VERSION)
                    # FIXME: Use in cmake 3.19 or later
                    # COMMAND_ERROR_IS_FATAL ANY)

    # Figure out the version number, depends on whether building from the git repo
    if (NOT GIT_RESULT EQUAL 0)
        # Not building from a git clone
        message(WARNING "Unable to find git repository: version number will be incomplete")
        set(PACKAGE_VERSION "UNKNOWN")
        set(SOUFFLE_VERSION "")
    else()
        string(REGEX REPLACE "\n$" "" PACKAGE_VERSION "${GIT_PACKAGE_VERSION}")
        message(STATUS "Building souffle version ${PACKAGE_VERSION}")

        # SOUFFLE_VERSION only includes the major/minor triplet
        string(REGEX REPLACE "-.*$" "" SOUFFLE_VERSION "${PACKAGE_VERSION}")

        # If building from a shallow clone where tag is not available.
        if (NOT ${SOUFFLE_VERSION} MATCHES "^[0-9.]+$")
            # message(WARNING "Cannot find a valid tag: cmake project version will be incomplete")
            set (SOUFFLE_VERSION "")
        endif()
    endif()
endif()

project(souffle VERSION "${SOUFFLE_VERSION}"
                DESCRIPTION "A datalog compiler"
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

cmake_dependent_option(SOUFFLE_USE_LIBCPP "Link to libc++ instead of libstdc++" ON
    "CMAKE_CXX_COMPILER_ID STREQUAL Clang" OFF)

# Using Clang? Likely want to use `lld` too.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
endif()

# Add aditional modules to CMake
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

if (SOUFFLE_USE_LIBCPP)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -lc++abi")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lc++abi")
endif()

if (WIN32)
  # Search libraries with and without 'lib' prefix.
  set(CMAKE_FIND_LIBRARY_PREFIXES ";lib")

  # Prefix all shared libraries with 'lib'.
  set(CMAKE_SHARED_LIBRARY_PREFIX "lib")

  # Prefix all static libraries with 'lib'.
  set(CMAKE_STATIC_LIBRARY_PREFIX "lib")

  SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
  SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")
  SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
  SET( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")
  SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
  SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")
endif ()


if(EMSCRIPTEN)
  set(SOUFFLE_USE_SQLITE off)
  set(SOUFFLE_USE_OPENMP off)
  set(SOUFFLE_USE_ZLIB off)
  set(SOUFFLE_USE_LIBFFI off)
  set(SOUFFLE_ENABLE_TESTING off)
endif ()

# --------------------------------------------------
# flex and bison
# --------------------------------------------------
# See issue #2143.
if (CMAKE_HOST_SYSTEM_NAME MATCHES "Darwin")
    execute_process(
            COMMAND brew --prefix bison
            RESULT_VARIABLE BREW_BISON
            OUTPUT_VARIABLE BREW_BISON_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (BREW_BISON EQUAL 0 AND EXISTS "${BREW_BISON_PREFIX}")
        message(STATUS "Found Bison keg installed by Homebrew at ${BREW_BISON_PREFIX}")
        set(BISON_EXECUTABLE "${BREW_BISON_PREFIX}/bin/bison")
    endif()

    execute_process(
            COMMAND brew --prefix flex
            RESULT_VARIABLE BREW_FLEX
            OUTPUT_VARIABLE BREW_FLEX_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (BREW_FLEX EQUAL 0 AND EXISTS "${BREW_FLEX_PREFIX}")
        message(STATUS "Found Flex keg installed by Homebrew at ${BREW_FLEX_PREFIX}")
        set(FLEX_EXECUTABLE "${BREW_FLEX_PREFIX}/bin/flex")
    endif()
endif()
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
