cmake_minimum_required(VERSION 3.16)
project(sat_key_cegar CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wall -Wextra")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include
)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib
)

add_executable(run_cegar_synthesis
    src/run_cegar_synthesis.cpp
)
target_link_libraries(run_cegar_synthesis PRIVATE cadical)

add_executable(verify_invariant
    src/verify_invariant.cpp
)
target_link_libraries(verify_invariant PRIVATE cadical)
