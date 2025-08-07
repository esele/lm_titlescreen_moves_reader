cmake_minimum_required(VERSION 3.10.0)
project(state_reader VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

include(FetchContent)

FetchContent_Declare(tinf GIT_REPOSITORY https://github.com/jibsen/tinf GIT_TAG 57ffa1f1d5e3dde19011b2127bd26d01689b694b)
FetchContent_MakeAvailable(tinf)

add_executable(state_reader main.cpp)

target_link_libraries(state_reader PRIVATE tinf)
