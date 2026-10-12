# Chip-8 Emulator in C++ & SDL2

A robust, modular, and cross-platform Chip-8 emulator built from scratch using Modern C++ (C++17) and SDL2. Designed for standard compliance, clean architecture, and reliable performance.

---

## Features
* **Complete Instruction Set**: Full implementation of standard Chip-8 opcodes (`0x0000` through `0xF000`), including math, bitwise operations, jump routines, BCD conversion (`FX33`), and sprite rendering with collision detection (`0xDXYN`).
* **Accurate Memory Map**: 4KB RAM with proper reservations for the built-in fontset starting at `0x050` and ROM loading starting at `0x200`. Out-of-bounds sprite / load-store accesses are guarded.
* **Compatibility Quirks**: Configurable COSMAC VIP vs CHIP-48 / Super-CHIP behavior: shift (`8XY6/8XYE`), `VF` reset on logic ops, `I` increment on `FX55/FX65`, `BNNN` vs `BXNN` jump base, sprite wrap vs clip.
* **Layout-Agnostic Input**: Scancode-based keyboard mapping supporting a standard 16-key hex keypad layout mapped to modern keyboards. Key repeat is ignored; `Esc` quits, `P` pauses, `Ctrl+R` reloads the ROM.
* **SDL2 Graphics, Audio & Timing**: 60Hz frame loop with vsync, scalable rendering, 440Hz square-wave beep driven by `sound_timer`, and timers ticked at 60Hz.
* **Tests**: Assert-based regression suite run via CTest (`chip8_tests`).

---

## Project Structure
```text
Chip8Emulator/
├── include/
│   └── chip8.hpp       # Chip-8 CPU state, memory, and register declarations
├── src/
│   ├── main.cpp        # SDL2 window management, input polling, and main loop
│   └── chip8.cpp       # Core fetch-decode-execute cycle and instruction logic
├── tests/
│   └── test_chip8.cpp  # Regression tests (ctest)
├── .clang-format       # Formatting rules
└── CMakeLists.txt      # CMake build configuration
```

---

## Prerequisites & Dependencies
Before building, ensure you have the following installed on your system:
* A modern **C++17** compatible compiler (GCC, Clang, or MSVC)
* **CMake** (version 3.15 or higher)
* **SDL2 Development Libraries**

### Installing Dependencies
* **macOS (Homebrew):**
  ```bash
  brew install sdl2 cmake
  ```
* **Linux (Ubuntu/Debian):**
  ```bash
  sudo apt-get install build-essential cmake libsdl2-dev
  ```
* **Windows:**
  Install SDL2 via `vcpkg`, MSYS2, or download the development libraries directly from the [SDL website](https://www.libsdl.org/).

---

## Build Instructions

1. Clone or download the repository into your local workspace.
2. Create a build directory and generate the build files using CMake:
   ```bash
   mkdir build
   cd build
   cmake ..
   ```
3. Compile the project:
   ```bash
   cmake --build .
   ```
4. Run the tests:
   ```bash
   ctest --output-on-failure
   ```

---

## Usage

Launch the executable from your terminal by passing the path to a valid Chip-8 ROM file:

```bash
./Chip8Emulator path/to/rom.ch8 [--ips 600] [--scale 10] [quirk flags]
```

Quirk / run options (`./Chip8Emulator --help` for full list):
```text
--original-shift / --shift-vy   COSMAC VIP shift quirk (VX=VY then shift)
--no-vf-reset                   Keep VF on 8XY1/2/3 (VIP); default resets VF (CHIP-48)
--increment-i                   FX55/FX65 increment I (VIP); default leaves I
--jump-vx                       BXNN jump with VX (Super-CHIP); default BNNN with V0
--clip                          Clip sprites at edges; default wraps
--ips <n>                       Instructions per second (default 600)
--scale <n>                     Window scale factor (default 10)
```

### Controls Mapping
The 16-key hexadecimal Chip-8 keypad is mapped to your keyboard as follows:

| Chip-8 Keypad | Keyboard Key |
| :---: | :---: |
| `1` `2` `3` `C` | `1` `2` `3` `4` |
| `4` `5` `6` `D` | `Q` `W` `E` `R` |
| `7` `8` `9` `E` | `A` `S` `D` `F` |
| `A` `0` `B` `F` | `Z` `X` `C` `V` |

Emulator keys: `Esc` quit, `P` pause, `Ctrl+R` reload ROM (`R` alone is CHIP-8 key `D`).

---

## License
This project is open-source and available under the MIT License.
