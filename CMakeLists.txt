cmake_minimum_required(VERSION 3.14)
project(VBlockSort VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library
add_library(vblock INTERFACE)
target_include_directories(vblock INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Enable optimization flags for benchmarks
if(MSVC)
    target_compile_options(vblock INTERFACE /O2 /W4)
else()
    target_compile_options(vblock INTERFACE -O3 -Wall -Wextra -pedantic)
endif()

# Unit tests
enable_testing()
add_executable(test_correctness tests/test_correctness.cpp)
target_link_libraries(test_correctness PRIVATE vblock)
add_test(NAME test_correctness COMMAND test_correctness)

# Benchmarks
add_executable(bench_vs_gnu bench_vs_gnu.cpp)
target_link_libraries(bench_vs_gnu PRIVATE vblock)

add_executable(bench_struct_sizes bench_struct_sizes.cpp)
target_link_libraries(bench_struct_sizes PRIVATE vblock)

add_executable(master_benchmark benchmark.cpp)
target_link_libraries(master_benchmark PRIVATE vblock)

# Install rules
include(GNUInstallDirs)
install(DIRECTORY include/vblock DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS vblock EXPORT VBlockSortTargets)
install(EXPORT VBlockSortTargets
    FILE VBlockSortConfig.cmake
    NAMESPACE vblock::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VBlockSort
)
