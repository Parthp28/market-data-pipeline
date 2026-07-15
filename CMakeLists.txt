cmake_minimum_required(VERSION 3.20)
project(market_data_pipeline LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MDP_BUILD_TESTS "Build unit tests" ON)
option(MDP_BUILD_BENCH "Build Google Benchmarks" ON)
option(MDP_ENABLE_COVERAGE "Enable coverage flags" OFF)
option(MDP_ENABLE_SANITIZERS "Enable ASan and UBSan on tests" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(mdp_warnings INTERFACE)
target_compile_options(mdp_warnings INTERFACE
  -Wall -Wextra -Wpedantic -Wconversion -Wshadow
)

if(MDP_ENABLE_COVERAGE)
  target_compile_options(mdp_warnings INTERFACE --coverage -O0 -g -fno-inline -fno-inline-functions)
  target_link_options(mdp_warnings INTERFACE --coverage)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  hdrhistogram
  GIT_REPOSITORY https://github.com/HdrHistogram/HdrHistogram_c.git
  GIT_TAG        0.11.8
)

if(MDP_BUILD_TESTS)
  FetchContent_MakeAvailable(googletest)
endif()
if(MDP_BUILD_BENCH)
  FetchContent_MakeAvailable(googlebenchmark)
endif()
FetchContent_MakeAvailable(hdrhistogram)

add_library(mdp_core
  src/parser.cpp
  src/sequence_tracker.cpp
  src/book.cpp
  src/consumers.cpp
  src/instrumentation.cpp
  src/feed_handler.cpp
  src/simulator_lib.cpp
)
target_include_directories(mdp_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(mdp_core PUBLIC hdr_histogram mdp_warnings)
target_compile_definitions(mdp_core PUBLIC MDP_VERSION=1)

add_executable(mdp_simulator src/simulator_main.cpp)
target_link_libraries(mdp_simulator PRIVATE mdp_core)

add_executable(mdp_handler src/handler_main.cpp)
target_link_libraries(mdp_handler PRIVATE mdp_core)

if(MDP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(MDP_BUILD_BENCH)
  add_subdirectory(benchmarks)
endif()
