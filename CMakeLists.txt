cmake_minimum_required(VERSION 3.20)
project(Tsukuyomi VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

include(cmake/CPM.cmake)
CPMAddPackage("gh:BasedInc/libhat@0.9.0")
CPMAddPackage("gh:TsudaKageyu/minhook@1.3.4")
CPMAddPackage("gh:nlohmann/json@3.11.3")
CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 0.9.9.8
)

file(GLOB_RECURSE SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(Tsukuyomi SHARED ${SOURCES})

target_compile_options(Tsukuyomi PRIVATE /utf-8)

target_link_libraries(Tsukuyomi PRIVATE
    libhat::libhat
    minhook
    nlohmann_json::nlohmann_json
    glm
)

target_include_directories(Tsukuyomi PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
