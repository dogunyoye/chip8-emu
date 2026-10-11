cmake_minimum_required(VERSION 3.15)
project(Chip8Emulator)

set(CMAKE_CXX_STANDARD 17)

# Find SDL2 package (assumes SDL2 is installed on your system)
find_package(SDL2 REQUIRED)

# Include header directories
include_directories(include ${SDL2_INCLUDE_DIRS})

# Add executable target and its source files
add_executable(Chip8Emulator 
    src/main.cpp 
    src/chip8.cpp
)

# Link SDL2 libraries to your executable
target_link_libraries(Chip8Emulator ${SDL2_LIBRARIES})