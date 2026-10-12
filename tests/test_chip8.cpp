// Plain assert-based regression tests (run via ctest). No external deps.
#include "chip8.hpp"
#include <cassert>
#include <iostream>

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n";      \
            return 1;                                                      \
        }                                                                  \
    } while (0)

static void load_op(Chip8& c, uint16_t op) {
    c.memory[0x200] = static_cast<uint8_t>(op >> 8);
    c.memory[0x201] = static_cast<uint8_t>(op & 0xFF);
    c.program_counter = 0x200;
}

int main() {
    // FX29 uses value in VX.
    {
        Chip8 c;
        c.load_fonts();
        c.registers[3] = 0xA;
        load_op(c, 0xF329);
        c.emulate_cycle();
        CHECK(c.index_register == 0x050 + 0xA * 5);
    }
    // Timers tick in main, not per-cycle.
    {
        Chip8 c;
        c.delay_timer = 5;
        c.sound_timer = 5;
        load_op(c, 0x6000);
        c.emulate_cycle();
        CHECK(c.delay_timer == 5 && c.sound_timer == 5);
    }
    // EX9E with VX=0xFF must not crash (masked to 0xF).
    {
        Chip8 c;
        c.registers[0] = 0xFF;
        c.keypad[0xF] = 1;
        load_op(c, 0xE09E);
        c.emulate_cycle();
        CHECK(c.program_counter == 0x204);
    }
    // load_rom failure returns false.
    {
        Chip8 c;
        CHECK(c.load_rom("/nonexistent.rom") == false);
    }
    // Stack underflow guard.
    {
        Chip8 c;
        load_op(c, 0x00EE);
        c.stack_pointer = 0;
        c.emulate_cycle(); // must not crash
    }
    // Stack overflow guard.
    {
        Chip8 c;
        load_op(c, 0x2206);
        c.stack_pointer = 16;
        c.emulate_cycle();
        CHECK(c.program_counter == 0x202); // unchanged past fetch
    }
    // Modern vs original shift.
    {
        Chip8 c;
        c.use_original_shift = false;
        c.registers[1] = 0b1011;
        c.registers[2] = 0xFF;
        load_op(c, 0x8126);
        c.emulate_cycle();
        CHECK(c.registers[1] == 0b101 && c.registers[0xF] == 1 && c.registers[2] == 0xFF);
    }
    {
        Chip8 c;
        c.use_original_shift = true;
        c.registers[1] = 0x00;
        c.registers[2] = 0b1011;
        load_op(c, 0x8126);
        c.emulate_cycle();
        CHECK(c.registers[1] == 0b101 && c.registers[0xF] == 1);
    }
    // VF reset quirk on 8XY1.
    {
        Chip8 c;
        c.reset_vf_on_logic = true;
        c.registers[0xF] = 5;
        c.registers[1] = 0xF0;
        c.registers[2] = 0x0F;
        load_op(c, 0x8121);
        c.emulate_cycle();
        CHECK(c.registers[1] == 0xFF && c.registers[0xF] == 0);
    }
    {
        Chip8 c;
        c.reset_vf_on_logic = false;
        c.registers[0xF] = 5;
        c.registers[1] = 0xF0;
        c.registers[2] = 0x0F;
        load_op(c, 0x8121);
        c.emulate_cycle();
        CHECK(c.registers[1] == 0xFF && c.registers[0xF] == 5);
    }
    // FX55/FX65 I-increment quirk + bounds.
    {
        Chip8 c;
        c.increment_i_on_load_store = true;
        c.registers[0] = 11;
        c.registers[1] = 22;
        c.index_register = 0x300;
        load_op(c, 0xF155);
        c.emulate_cycle();
        CHECK(c.memory[0x300] == 11 && c.memory[0x301] == 22);
        CHECK(c.index_register == 0x302);
    }
    {
        Chip8 c;
        c.increment_i_on_load_store = false;
        c.index_register = 0x300;
        c.registers[0] = 11;
        load_op(c, 0xF055);
        c.emulate_cycle();
        CHECK(c.index_register == 0x300);
    }
    // FX55 OOB guard (I near end of RAM).
    {
        Chip8 c;
        c.index_register = 0xFFF;
        c.registers[0] = 1;
        c.registers[1] = 2;
        load_op(c, 0xF155);
        c.emulate_cycle(); // must not crash
    }
    // BNNN vs BXNN.
    {
        Chip8 c;
        c.use_vx_jump = false;
        c.registers[0] = 0x10;
        c.registers[1] = 0x50;
        load_op(c, 0xB300);
        c.emulate_cycle();
        CHECK(c.program_counter == 0x310);
    }
    {
        Chip8 c;
        c.use_vx_jump = true;
        c.registers[0] = 0x10;
        c.registers[3] = 0x50;
        load_op(c, 0xB300);
        c.emulate_cycle();
        CHECK(c.program_counter == 0x350);
    }
    // Sprite clip vs wrap.
    {
        Chip8 c;
        c.clip_sprites = true;
        c.registers[0] = 63;
        c.registers[1] = 0;
        c.index_register = 0x300;
        c.memory[0x300] = 0xFF; // 8 pixels starting at x=63
        load_op(c, 0xD011);
        c.emulate_cycle();
        int lit = 0;
        for (auto p : c.gfx) lit += p;
        CHECK(lit == 1); // only x=63 visible, rest clipped
    }
    {
        Chip8 c;
        c.clip_sprites = false;
        c.registers[0] = 63;
        c.registers[1] = 0;
        c.index_register = 0x300;
        c.memory[0x300] = 0xFF;
        load_op(c, 0xD011);
        c.emulate_cycle();
        int lit = 0;
        for (auto p : c.gfx) lit += p;
        CHECK(lit == 8); // wraps around
    }
    // CXNN result always respects mask.
    {
        Chip8 c;
        for (int i = 0; i < 200; ++i) {
            load_op(c, 0xC0F0);
            c.emulate_cycle();
            CHECK((c.registers[0] & ~0xF0) == 0);
        }
    }
    // reset() restores power-on state but preserves quirks.
    {
        Chip8 c;
        c.use_original_shift = true;
        c.registers[0] = 9;
        c.program_counter = 0x400;
        c.reset();
        CHECK(c.program_counter == 0x200 && c.registers[0] == 0);
        CHECK(c.use_original_shift == true);
    }

    std::cout << "All chip8_tests passed.\n";
    return 0;
}
