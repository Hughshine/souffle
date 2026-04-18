cmake_minimum_required(VERSION 3.16)
project(approxmc_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(APPROXMC_DEMO_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_library(weighted_conversion STATIC weighted_conversion.cpp)
target_include_directories(weighted_conversion PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${APPROXMC_DEMO_REPO_ROOT}/src/include)

add_executable(weighted_conversion_demo weighted_conversion_demo.cpp)
target_link_libraries(weighted_conversion_demo PRIVATE weighted_conversion)

add_executable(problog_sampling_backends_demo problog_sampling_backends_demo.cpp)

option(APPROXMC_DEMO_BUILD_WITH_APPROXMC "Build demos that require ApproxMC library" ON)
if(APPROXMC_DEMO_BUILD_WITH_APPROXMC)
    find_package(approxmc CONFIG QUIET)
    if(approxmc_FOUND)
        add_executable(unweighted_appmc_demo unweighted_appmc_demo.cpp)
        target_link_libraries(unweighted_appmc_demo PRIVATE approxmc)
        target_include_directories(unweighted_appmc_demo PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${APPROXMC_DEMO_REPO_ROOT}/src/include)

        add_library(weighted_appmc STATIC weighted_appmc.cpp)
        target_link_libraries(weighted_appmc PUBLIC approxmc weighted_conversion)
        target_include_directories(weighted_appmc PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${APPROXMC_DEMO_REPO_ROOT}/src/include)

        add_executable(weighted_appmc_demo weighted_appmc_demo.cpp)
        target_link_libraries(weighted_appmc_demo PRIVATE weighted_appmc)
        target_include_directories(weighted_appmc_demo PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${APPROXMC_DEMO_REPO_ROOT}/src/include)
    else()
        message(WARNING "approxmc package not found. Skipping targets: unweighted_appmc_demo, weighted_appmc_demo")
    endif()
endif()
