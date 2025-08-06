cmake_minimum_required(VERSION 3.10.0)
project(state_reader VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

include(FetchContent)

FetchContent_Declare(miniz GIT_REPOSITORY https://github.com/richgel999/miniz GIT_TAG c883286f1a6443720e7705450f59e579a4bbb8e2)
FetchContent_MakeAvailable(miniz)

add_executable(state_reader main.cpp)

target_link_libraries(state_reader PRIVATE miniz)
