cmake_minimum_required(VERSION 3.10)
project(unikey_cpp_telex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(unikey_telex_cpp
    vietnamese.cpp
    rime_table.cpp
    canonicalize.cpp
    render_utf8.cpp
    engine.cpp
)

target_include_directories(unikey_telex_cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(unikey_telex_cpp_tests
    tests.cpp
)

target_link_libraries(unikey_telex_cpp_tests PRIVATE unikey_telex_cpp)

