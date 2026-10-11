#pragma once
#include <cstdint>
#include <array>

class Chip8 {
public:
    Chip8();
    ~Chip8() = default;

    // Core components
    std::array<uint8_t, 4096> memory{};     // 4KB RAM
    std::array<uint8_t, 16> registers{};    // V0 - VF registers
    uint16_t index_register = 0;            // I register
    uint16_t program_counter = 0x200;       // Program counter (starts at 0x200)
    
    std::array<uint16_t, 16> stack{};       // Subroutine stack
    uint8_t stack_pointer = 0;

    uint8_t delay_timer = 0;
    uint8_t sound_timer = 0;

    std::array<uint8_t, 64 * 32> gfx{};     // Monochrome display buffer (64x32)
    std::array<uint8_t, 16> keypad{};       // 16-key hex keypad
    
    bool draw_flag = false;                 // Set when screen needs rendering

    void load_fonts();
    void load_rom(const char* filename);
    void emulate_cycle();
};