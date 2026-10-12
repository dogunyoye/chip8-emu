cmake_minimum_required(VERSION 3.15)
project(Chip8Emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find SDL2 package (assumes SDL2 is installed on your system)
find_package(SDL2 REQUIRED)

# Add executable target and its source files
add_executable(Chip8Emulator
    src/main.cpp
    src/chip8.cpp
)

target_include_directories(Chip8Emulator PRIVATE include)

# Prefer the modern imported target; fall back to legacy variables.
if(TARGET SDL2::SDL2)
    target_link_libraries(Chip8Emulator PRIVATE SDL2::SDL2)
else()
    target_include_directories(Chip8Emulator PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(Chip8Emulator PRIVATE ${SDL2_LIBRARIES})
endif()

if(MSVC)
    target_compile_options(Chip8Emulator PRIVATE /W4)
else()
    target_compile_options(Chip8Emulator PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Tests (no external deps; plain assert-based driver)
enable_testing()
add_executable(chip8_tests
    tests/test_chip8.cpp
    src/chip8.cpp
)
target_include_directories(chip8_tests PRIVATE include)
if(MSVC)
    target_compile_options(chip8_tests PRIVATE /W4)
else()
    target_compile_options(chip8_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME chip8_tests COMMAND chip8_tests)
