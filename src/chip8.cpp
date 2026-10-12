#include <cstdint>
#include <array>

#include "chip8.hpp"
#include <fstream>
#include <iostream>

Chip8::Chip8() : rng(std::random_device{}()) {
    // rng seeded with random_device; nothing else to init
    // (members use default member initializers).
}

void Chip8::reset() {
    memory.fill(0);
    registers.fill(0);
    index_register = 0;
    program_counter = 0x200;
    stack.fill(0);
    stack_pointer = 0;
    delay_timer = 0;
    sound_timer = 0;
    gfx.fill(0);
    keypad.fill(0);
    draw_flag = false;
    // NOTE: quirk flags and rng state are preserved across reset.
}

void Chip8::load_fonts() {
    // Standard Chip-8 font set (Characters 0-F, each 5 bytes tall)
    uint8_t fontset[80] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };

    // Load fonts into memory starting at address 0x050 (80 in decimal)
    for (int i = 0; i < 80; ++i) {
        memory[0x050 + i] = fontset[i];
    }
}

bool Chip8::load_rom(const char* filename) {
    // 1. Open the file in binary mode and start at the end (ate) to get size
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        std::cerr << "Failed to open ROM file: " << filename << "\n";
        return false;
    }

    // 2. Get the size of the file
    std::streamsize size = file.tellg();
    
    // 3. Move the file pointer back to the beginning of the file to read content
    file.seekg(0, std::ios::beg);

    // 4. Check if the ROM fits into available RAM
    // Chip-8 RAM is 4096 bytes, and programs start at 0x200 (512 bytes).
    // Available space for ROM is 4096 - 512 = 3584 bytes.
    if (size > (4096 - 0x200)) {
        std::cerr << "Error: ROM is too large to fit in memory!\n";
        return false;
    }

    // 5. Read the file directly into a temporary buffer or straight into memory
    // Here we read directly into our memory array starting at index 0x200 (512)
    if (file.read(reinterpret_cast<char*>(&memory[0x200]), size)) {
        std::cout << "Successfully loaded ROM: " << filename << " (" << size << " bytes)\n";
        return true;
    } else {
        std::cerr << "Error: Failed to read ROM file into memory.\n";
        return false;
    }

    // file stream automatically closes when it goes out of scope
}

void Chip8::emulate_cycle() {
    // Guard against PC running off memory (each opcode is 2 bytes).
    if (static_cast<size_t>(program_counter) + 1 >= memory.size()) {
        std::cerr << "Error: PC out of bounds: 0x" << std::hex << program_counter << "\n";
        return;
    }

    // 1. Fetch Opcode (Big-endian: high byte first, shifted left by 8, ORed with low byte)
    uint16_t opcode = (memory[program_counter] << 8) | memory[program_counter + 1];

    // Increment PC for the next instruction (each opcode is 2 bytes)
    program_counter += 2;

    // 2. Decode and Execute Opcode
    // You use bitwise masking to isolate nibbles (e.g., opcode & 0xF000)
    switch (opcode & 0xF000) {
        case 0x0000:
            switch (opcode & 0x00FF) {
                case 0x00E0: // Clear screen
                    gfx.fill(0);
                    draw_flag = true;
                    break;
                case 0x00EE: // Return from subroutine
                    if (stack_pointer == 0) {
                        std::cerr << "Error: stack underflow on RET\n";
                        return;
                    }
                    program_counter = stack[--stack_pointer];
                    break;
                default:
                    // 0NNN: SYS addr — ignored on modern interpreters.
                    break;
            }
            break;

        case 0x1000: // JP addr (Jump to address NNN)
            program_counter = opcode & 0x0FFF;
            break;

        case 0x2000: // 0x2NNN: Call subroutine at NNN
            if (stack_pointer >= stack.size()) {
                std::cerr << "Error: stack overflow on CALL\n";
                return;
            }
            stack[stack_pointer] = program_counter;
            stack_pointer++;
            program_counter = opcode & 0x0FFF;
            break;

        case 0x3000: {
            // 1. Extract register index X (bits 8 to 11)
            uint8_t x = (opcode & 0x0F00) >> 8;
            
            // 2. Extract the 8-bit constant value NN (bits 0 to 7)
            uint8_t nn = opcode & 0x00FF;

            // 3. Compare register VX with NN, skip next instruction if equal
            if (registers[x] == nn) {
                program_counter += 2; // Skip the next 2-byte instruction
            }
            break;
        }

        case 0x4000: {
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t nn = opcode & 0x00FF;

            // 3. Compare register VX with NN, skip next instruction if not equal
            if (registers[x] != nn) {
                program_counter += 2;
            }
            break;
        }

        case 0x5000: {
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;

            // 3. Compare register VX with register VY, skip next instruction if equal
            if (registers[x] == registers[y]) {
                program_counter += 2;
            }
            break;
        }

        case 0x9000: {
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;

            // 3. Compare register VX with register VY, skip next instruction if not equal
            if (registers[x] != registers[y]) {
                program_counter += 2;
            }
            break;
        }

        case 0x6000: { // Set 0x6XNN
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t nn = opcode & 0x00FF;
            registers[x] = nn;
            break;
        }

        case 0x7000: { // Add Set 0x7XNN
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t nn = opcode & 0x00FF;
            registers[x] += nn;
            break;
        }

        case 0x8000: {
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;

            switch (opcode & 0x000F) {
                case 0x0: // 8XY0: Set VX = VY
                    registers[x] = registers[y];
                    break;

                case 0x1: // 8XY1: Set VX = VX | VY (Bitwise OR)
                    registers[x] |= registers[y];
                    if (reset_vf_on_logic) registers[0xF] = 0;
                    break;

                case 0x2: // 8XY2: Set VX = VX & VY (Bitwise AND)
                    registers[x] &= registers[y];
                    if (reset_vf_on_logic) registers[0xF] = 0;
                    break;

                case 0x3: // 8XY3: Set VX = VX ^ VY (Bitwise XOR)
                    registers[x] ^= registers[y];
                    if (reset_vf_on_logic) registers[0xF] = 0;
                    break;

                case 0x4: // 8XY4: Set VX = VX + VY, set VF = carry
                    // VY is added to VX. If the sum is greater than 255, VF is set to 1, otherwise 0.
                    // Only the lowest 8-bits of the result are kept in VX.
                    {
                        uint16_t sum = registers[x] + registers[y];
                        registers[0xF] = (sum > 0xFF) ? 1 : 0;
                        registers[x] = sum & 0xFF;
                    }
                    break;

                case 0x5: // 8XY5: Set VX = VX - VY, set VF = NOT borrow
                    // If VX >= VY, VF is set to 1, otherwise 0. Then VY is subtracted from VX.
                    {
                        registers[0xF] = (registers[x] >= registers[y]) ? 1 : 0;
                        registers[x] -= registers[y];
                    }
                    break;

                case 0x6: // 8XY6: SHR 1 (Shift Right)
                    // Modern (CHIP-48): shift VX. Original (COSMAC VIP): VX = VY, then shift.
                    if (use_original_shift) {
                        registers[x] = registers[y];
                    }
                    // If the least-significant bit of the source is 1, VF = 1, else 0.
                    registers[0xF] = registers[x] & 0x1;
                    registers[x] >>= 1;
                    break;

                case 0x7: // 8XY7: Set VX = VY - VX, set VF = NOT borrow
                    // If VY >= VX, VF is set to 1, otherwise 0. Then VX is subtracted from VY.
                    {
                        registers[0xF] = (registers[y] >= registers[x]) ? 1 : 0;
                        registers[x] = registers[y] - registers[x];
                    }
                    break;

                case 0xE: // 8XYE: SHL 1 (Shift Left)
                    // Modern (CHIP-48): shift VX. Original (COSMAC VIP): VX = VY, then shift.
                    if (use_original_shift) {
                        registers[x] = registers[y];
                    }
                    // If the most-significant bit of the source is 1, VF = 1, else 0.
                    registers[0xF] = (registers[x] & 0x80) >> 7;
                    registers[x] <<= 1;
                    break;

                default:
                    std::cerr << "Unknown 8000 opcode: 0x" << std::hex << opcode << "\n";
                    break;
            }
            break;
        }

        case 0xA000: { // ANNN: Set Index Register I to NNN
            uint16_t nnn = opcode & 0x0FFF;
            index_register = nnn;
            break;
        }

        case 0xB000: { // BNNN / BXNN: Jump to NNN + V0 (or NNN + VX on Super-CHIP)
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint16_t nnn = opcode & 0x0FFF;
            uint16_t base = use_vx_jump ? registers[x] : registers[0];
            program_counter = (base + nnn) & 0x0FFF;
            break;
        }

        case 0xC000: { // Random: 0xCXNN
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t nn = opcode & 0x00FF;
            registers[x] = static_cast<uint8_t>(rand_dist(rng) & nn);
            break;
        }

        case 0xD000: { // Display: 0xDXYN
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;
            uint8_t n = opcode & 0x000F;

            uint8_t x_coord = registers[x] % 64;
            uint8_t y_coord = registers[y] % 32;
            registers[0xF] = 0;

            for(int row = 0; row < n; row++) {
                if (static_cast<size_t>(index_register) + static_cast<size_t>(row) >= memory.size()) {
                    std::cerr << "Error: sprite read OOB at I=0x" << std::hex << index_register << "\n";
                    break;
                }
                uint8_t sprite_byte = memory[index_register + row];
                uint8_t col = 0;
                for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
                    if (sprite_byte & mask) {
                        int sx = x_coord + col;
                        int sy = y_coord + row;
                        if (clip_sprites) {
                            // Clip: skip pixels past the edges.
                            if (sx >= 64 || sy >= 32) { ++col; continue; }
                        } else {
                            // Wrap: coordinates wrap around the screen.
                            sx %= 64;
                            sy %= 32;
                        }
                        int pixel_index = sx + (sy * 64);

                        // Check for collision (if screen pixel is already 1)
                        if (gfx[pixel_index] == 1) {
                            registers[0xF] = 1;
                        }

                        // XOR the pixel onto the screen buffer
                        gfx[pixel_index] ^= 1;
                    }
                    ++col;
                }
            }

            draw_flag = true;
            break;
        }

        case 0xE000: { // Skip if key: 0xEX9E, 0xEXA1
            uint8_t x = (opcode & 0x0F00) >> 8;
            // VX can hold 0-255 but keypad has 16 keys; mask to 0-F to avoid OOB.
            uint8_t key = registers[x] & 0x0F;
            switch (opcode & 0x00FF) {
                case 0x9E: // Skip if key corresponding to value in VX is pressed
                    if (keypad[key] == 1) {
                        program_counter += 2;
                    }
                    break;
                case 0xA1: // Skip if key corresponding to value in VX is NOT pressed
                    if (keypad[key] == 0) {
                        program_counter += 2;
                    }
                    break;
                default:
                    std::cerr << "Unknown E000 opcode: 0x" << std::hex << opcode << "\n";
                    break;
            }
            break;
        }

        case 0xF000: { // Timers: 0xFX07, 0xFX15, 0xFX18
            uint8_t x = (opcode & 0x0F00) >> 8;
            switch (opcode & 0x00FF) {
                case 0x07:
                    registers[x] = delay_timer;
                    break;

                case 0x15:
                    delay_timer = registers[x];
                    break;

                case 0x18:
                    sound_timer = registers[x];
                    break;

                case 0x1E:
                    // I wraps within 12-bit address space.
                    index_register = (index_register + registers[x]) & 0x0FFF;
                    break;

                case 0x0A: {
                    bool key_pressed = false;
                    for (int i = 0; i < 16; ++i) {
                        if (keypad[i] == 1) {
                            registers[x] = i; // Store the key value (0-F) into VX
                            key_pressed = true;
                            break;
                        }
                    }

                    // If no key is pressed, decrement PC to repeat this instruction
                    // on the next cycle (pause execution)
                    if (!key_pressed) {
                        program_counter -= 2;
                    }
                    break;
                }
                
                case 0x29: {
                    // Each font character is 5 bytes long, starting at memory address 0x050
                    // Use the *value* in VX (the hex digit), not the register index x.
                    index_register = 0x050 + (registers[x] * 5);
                    break;
                }

                case 0x33: {
                    if (static_cast<size_t>(index_register) + 2 >= memory.size()) {
                        std::cerr << "Error: FX33 write OOB at I=0x" << std::hex << index_register << "\n";
                        break;
                    }
                    uint8_t value = registers[x];
                    memory[index_register] = value / 100;
                    memory[index_register + 1] = (value / 10) % 10;
                    memory[index_register + 2] = value % 10;
                    break;
                }

                case 0x55: {
                    if (static_cast<size_t>(index_register) + x >= memory.size()) {
                        std::cerr << "Error: FX55 write OOB at I=0x" << std::hex << index_register << "\n";
                        break;
                    }
                    for (int i = 0; i <= x; i++) {
                        memory[index_register + i] = registers[i];
                    }
                    if (increment_i_on_load_store) {
                        index_register = (index_register + x + 1) & 0x0FFF;
                    }
                    break;
                }

                case 0x65: {
                    if (static_cast<size_t>(index_register) + x >= memory.size()) {
                        std::cerr << "Error: FX65 read OOB at I=0x" << std::hex << index_register << "\n";
                        break;
                    }
                    for (int i = 0; i <= x; i++) {
                        registers[i] = memory[index_register + i];
                    }
                    if (increment_i_on_load_store) {
                        index_register = (index_register + x + 1) & 0x0FFF;
                    }
                    break;
                }


                default:
                    std::cerr << "Unknown F000 opcode: 0x" << std::hex << opcode << "\n";
                    break;
            }
            break;
        }

        default:
            std::cerr << "Unknown opcode: 0x" << std::hex << opcode << "\n";
            break;
    }

    // NOTE: timers are decremented at 60Hz in main.cpp, not here.
    // Decrementing here would tick them once per *instruction* (~600Hz).
}

