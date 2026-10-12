#pragma once
#include <cstdint>
#include <array>
#include <random>

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

    // Quirks (defaults = modern CHIP-48 behavior, most ROMs expect this):
    //  shift: false = shift VX in place; true = VX = VY then shift (COSMAC VIP).
    bool use_original_shift = false;
    //  logic VF reset: true = 8XY1/2/3 reset VF to 0 (CHIP-48); false = leave VF (VIP).
    bool reset_vf_on_logic = true;
    //  load/store: true = FX55/FX65 increment I by X+1 (VIP); false = leave I (CHIP-48).
    bool increment_i_on_load_store = false;
    //  jump: false = BNNN jumps to NNN + V0; true = BXNN jumps to NNN + VX (Super-CHIP).
    bool use_vx_jump = false;
    //  sprites: false = wrap around edges; true = clip at edges.
    bool clip_sprites = false;

    void load_fonts();
    void reset();
    bool load_rom(const char* filename);
    void emulate_cycle();

private:
    std::mt19937 rng;
    std::uniform_int_distribution<uint16_t> rand_dist{0, 255};
};