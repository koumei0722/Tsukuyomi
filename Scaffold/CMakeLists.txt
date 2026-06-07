cmake_minimum_required(VERSION 4.1)
project(Scaffold)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
include(cmake/CPM.cmake)
CPMAddPackage("gh:BasedInc/libhat@0.9.0")
CPMAddPackage("gh:TsudaKageyu/minhook@1.3.4")
CPMAddPackage("gh:nlohmann/json@3.11.3")
add_library(Scaffold SHARED dllmain.cpp)
target_link_libraries(Scaffold PRIVATE
        libhat::libhat
        minhook
        nlohmann_json::nlohmann_json)
target_include_directories(Scaffold PRIVATE
        ${CMAKE_SOURCE_DIR}/glm
)