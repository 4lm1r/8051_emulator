cmake_minimum_required(VERSION 3.15)
project(8051_emulator VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler Warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core library
add_library(8051_core STATIC
    src/memory.cpp
    src/cpu.cpp
    src/disassembler.cpp
    src/debugger.cpp
)

target_include_directories(8051_core PUBLIC include)

# Main Emulator Executable
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE 8051_core)

# Tests
enable_testing()
add_executable(test_cpu tests/test_cpu.cpp)
target_link_libraries(test_cpu PRIVATE 8051_core)
add_test(NAME cpu_tests COMMAND test_cpu)
