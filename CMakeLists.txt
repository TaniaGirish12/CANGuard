cmake_minimum_required(VERSION 3.20)
project(CANGuard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release or Debug)" FORCE)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

option(CANGUARD_TSAN "Build every target with -fsanitize=thread" OFF)

find_package(Threads REQUIRED)

# Flags shared by every target. Linking this interface target propagates them.
add_library(canguard_build_flags INTERFACE)
target_compile_options(canguard_build_flags INTERFACE -Wall -Wextra)
if(CANGUARD_TSAN)
    target_compile_options(canguard_build_flags INTERFACE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(canguard_build_flags INTERFACE -fsanitize=thread)
endif()

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
list(FILTER CORE_SOURCES EXCLUDE REGEX "/main\\.cpp$")

add_library(canguard_core STATIC ${CORE_SOURCES})
target_include_directories(canguard_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(canguard_core PUBLIC Threads::Threads canguard_build_flags)

add_executable(canguard src/main.cpp)
target_link_libraries(canguard PRIVATE canguard_core)

enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/*.cpp)

add_executable(canguard_tests ${TEST_SOURCES})
target_link_libraries(canguard_tests PRIVATE canguard_core)
# The end-to-end tests read the real dataset from data/raw (see the README).
target_compile_definitions(canguard_tests PRIVATE CANGUARD_DATA_DIR="${CMAKE_SOURCE_DIR}/data/raw")
add_test(NAME canguard_tests COMMAND canguard_tests)
# A broken BoundedBuffer would hang the threaded tests, so never wait forever. The end-to-end tests read about
# 700 MB of data, which needs more time under ThreadSanitizer.
set_tests_properties(canguard_tests PROPERTIES TIMEOUT 600)
