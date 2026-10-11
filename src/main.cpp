#include <SDL.h>
#include "chip8.hpp"
#include <chrono>
#include <iostream>
#include <ctime>
#include <thread>

const int SCALE = 10; // Scale 64x32 up to 640x320 pixels

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <ROM_PATH>\n";
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Chip-8 Emulator", SDL_WINDOWPOS_CENTERED, 
                                          SDL_WINDOWPOS_CENTERED, 64 * SCALE, 32 * SCALE, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
                                             SDL_TEXTUREACCESS_STREAMING, 64, 32);

    Chip8 chip8;
    chip8.load_fonts();
    chip8.load_rom(argv[1]);

    // Ensure random numbers are randomized for each emulator run
    std::srand(std::time(nullptr));

    bool running = true;
    SDL_Event event;

    // Timing variables for 60Hz loop
    const int target_fps = 60;
    const int cpu_speed = 10; // Instructions executed per frame (adjust as needed, e.g., 8-15)
    auto last_time = std::chrono::high_resolution_clock::now();

    while (running) {
        auto current_time = std::chrono::high_resolution_clock::now();
        float elapsed_time = std::chrono::duration<float, std::chrono::milliseconds::period>(current_time - last_time).count();

        // Target frame duration for ~60Hz (~16.66ms)
        if (elapsed_time >= (1000.0f / target_fps)) {
            last_time = current_time;

            // Handle input events
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }

                // Map keyboard keys to chip8.keypad here
                if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                    uint8_t value = (event.type == SDL_KEYDOWN) ? 1 : 0;

                    // Map SDL scancodes to Chip-8 hexadecimal keypad (0-F)
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_X: chip8.keypad[0x0] = value; break; // 0
                        case SDL_SCANCODE_1: chip8.keypad[0x1] = value; break; // 1
                        case SDL_SCANCODE_2: chip8.keypad[0x2] = value; break; // 2
                        case SDL_SCANCODE_3: chip8.keypad[0x3] = value; break; // 3
                        case SDL_SCANCODE_Q: chip8.keypad[0x4] = value; break; // 4
                        case SDL_SCANCODE_W: chip8.keypad[0x5] = value; break; // 5
                        case SDL_SCANCODE_E: chip8.keypad[0x6] = value; break; // 6
                        case SDL_SCANCODE_A: chip8.keypad[0x7] = value; break; // 7
                        case SDL_SCANCODE_S: chip8.keypad[0x8] = value; break; // 8
                        case SDL_SCANCODE_D: chip8.keypad[0x9] = value; break; // 9
                        case SDL_SCANCODE_Z: chip8.keypad[0xA] = value; break; // A
                        case SDL_SCANCODE_C: chip8.keypad[0xB] = value; break; // B
                        case SDL_SCANCODE_4: chip8.keypad[0xC] = value; break; // C
                        case SDL_SCANCODE_R: chip8.keypad[0xD] = value; break; // D
                        case SDL_SCANCODE_F: chip8.keypad[0xE] = value; break; // E
                        case SDL_SCANCODE_V: chip8.keypad[0xF] = value; break; // F
                        default: break;
                    }
                }
            }

            // B. Run CPU Cycles for this frame
            for (int i = 0; i < cpu_speed; ++i) {
                chip8.emulate_cycle();
            }

            // C. Update Timers at 60Hz
            if (chip8.delay_timer > 0) --chip8.delay_timer;
            if (chip8.sound_timer > 0) --chip8.sound_timer;

            if (chip8.draw_flag) {
                // Convert 1-bit pixel buffer to 32-bit ARGB pixels for SDL
                uint32_t pixels[64 * 32];
                for (int i = 0; i < 64 * 32; ++i) {
                    pixels[i] = chip8.gfx[i] ? 0xFFFFFFFF : 0xFF000000; // White on Black
                }

                SDL_UpdateTexture(texture, nullptr, pixels, 64 * sizeof(uint32_t));
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);

                chip8.draw_flag = false;
            }
        } else {
            // Sleep briefly to prevent burning 100% CPU core usage
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}