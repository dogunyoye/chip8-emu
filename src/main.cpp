#include <SDL.h>
#include "chip8.hpp"
#include <chrono>
#include <iostream>
#include <cstring>
#include <string>

namespace {
constexpr int kWidth = 64;
constexpr int kHeight = 32;
constexpr int kFps = 60;
}

// Simple 440Hz square-wave beep for sound_timer. Runs on the SDL audio thread.
void audio_callback(void* /*userdata*/, Uint8* stream, int len) {
    Sint16* buf = reinterpret_cast<Sint16*>(stream);
    int samples = len / static_cast<int>(sizeof(Sint16));
    static uint32_t phase = 0;
    // 44100Hz / 440Hz ~= 100 samples per square-wave period.
    const uint32_t period = 100;
    for (int i = 0; i < samples; ++i) {
        phase = (phase + 1) % period;
        buf[i] = (phase < period / 2) ? 3000 : -3000;
    }
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <ROM_PATH> [options]\n"
              << "Options:\n"
              << "  --original-shift / --shift-vy  COSMAC VIP shift quirk (VX=VY then shift)\n"
              << "  --no-vf-reset                  Keep VF on 8XY1/2/3 (VIP); default resets VF (CHIP-48)\n"
              << "  --increment-i                  FX55/FX65 increment I (VIP); default leaves I\n"
              << "  --jump-vx                      BXNN jump with VX (Super-CHIP); default BNNN with V0\n"
              << "  --clip                         Clip sprites at edges; default wraps\n"
              << "  --ips <n>                      Instructions per second (default 600)\n"
              << "  --scale <n>                    Window scale factor (default 10)\n"
              << "  -h, --help                     Show this help\n"
              << "Keys: Esc quit, P pause, Ctrl+R reset (reload ROM)\n";
}

int main(int argc, char* argv[]) {
    const char* rom_path = nullptr;
    bool original_shift = false;
    bool vf_reset = true;
    bool increment_i = false;
    bool jump_vx = false;
    bool clip = false;
    int ips = 600;
    int scale = 10;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--original-shift" || arg == "--shift-vy") {
            original_shift = true;
        } else if (arg == "--no-vf-reset") {
            vf_reset = false;
        } else if (arg == "--increment-i") {
            increment_i = true;
        } else if (arg == "--jump-vx") {
            jump_vx = true;
        } else if (arg == "--clip") {
            clip = true;
        } else if ((arg == "--ips" || arg == "--scale") && i + 1 < argc) {
            int v = std::stoi(argv[++i]);
            if (arg == "--ips" && v > 0 && v <= 10000) ips = v;
            if (arg == "--scale" && v > 0 && v <= 40) scale = v;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            if (!rom_path) rom_path = argv[i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    if (!rom_path) {
        print_usage(argv[0]);
        return 1;
    }
    const int cpu_per_frame = (ips + kFps - 1) / kFps; // round up, >=1
    const int instructions_per_frame = cpu_per_frame > 0 ? cpu_per_frame : 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Chip-8 Emulator", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, kWidth * scale, kHeight * scale,
                                          SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }
    // Prefer vsync; fall back to plain accelerated.
    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    SDL_Texture* texture = nullptr;
    if (renderer) {
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);
    }
    if (!renderer || !texture) {
        std::cerr << "SDL video init failed: " << SDL_GetError() << "\n";
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Open audio for the sound_timer beep (graceful fallback to silent if it fails).
    SDL_AudioSpec want{}, have{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = audio_callback;
    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audio_dev == 0) {
        std::cerr << "Audio disabled: " << SDL_GetError() << "\n";
    } else {
        SDL_PauseAudioDevice(audio_dev, 1); // start paused; unpause while sound_timer > 0
    }

    auto apply_quirks = [&](Chip8& c) {
        c.use_original_shift = original_shift;
        c.reset_vf_on_logic = vf_reset;
        c.increment_i_on_load_store = increment_i;
        c.use_vx_jump = jump_vx;
        c.clip_sprites = clip;
    };

    Chip8 chip8;
    apply_quirks(chip8);
    chip8.load_fonts();
    if (!chip8.load_rom(rom_path)) {
        if (audio_dev != 0) SDL_CloseAudioDevice(audio_dev);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    auto reload_rom = [&]() {
        chip8.reset();
        apply_quirks(chip8);
        chip8.load_fonts();
        if (!chip8.load_rom(rom_path)) {
            std::cerr << "Failed to reload ROM.\n";
        }
    };

    auto set_key = [&](SDL_Scancode sc, uint8_t value) {
        switch (sc) {
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
    };

    bool running = true;
    bool paused = false;
    SDL_Event event;

    auto last_time = std::chrono::high_resolution_clock::now();

    while (running) {
        auto current_time = std::chrono::high_resolution_clock::now();
        float elapsed_ms =
            std::chrono::duration<float, std::chrono::milliseconds::period>(current_time - last_time)
                .count();

        if (elapsed_ms >= (1000.0f / kFps)) {
            last_time = current_time;

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                        running = false;
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_P) {
                        paused = !paused;
                        if (audio_dev != 0) SDL_PauseAudioDevice(audio_dev, 1);
                    } else if (event.key.keysym.scancode == SDL_SCANCODE_R &&
                               (event.key.keysym.mod & KMOD_CTRL)) {
                        // Ctrl+R reloads ROM (plain R is a CHIP-8 key).
                        reload_rom();
                    } else {
                        set_key(event.key.keysym.scancode, 1);
                    }
                } else if (event.type == SDL_KEYDOWN) {
                    // Key repeat: keep key held without toggling pause/reload.
                    if (event.key.keysym.scancode != SDL_SCANCODE_P &&
                        event.key.keysym.scancode != SDL_SCANCODE_ESCAPE) {
                        set_key(event.key.keysym.scancode, 1);
                    }
                } else if (event.type == SDL_KEYUP) {
                    set_key(event.key.keysym.scancode, 0);
                }
            }

            if (!paused) {
                for (int i = 0; i < instructions_per_frame; ++i) {
                    chip8.emulate_cycle();
                    // FX0A blocks by rewinding PC; don't burn the whole frame on it.
                    // If PC didn't advance, the CPU is waiting for input.
                    // (Heuristic: still cheap to just continue; break avoids 10x repeats.)
                    // We detect the stall by checking whether the last op was a blocking FX0A.
                    // Simplest: continue — stalled cycles are cheap. No break needed.
                }

                if (chip8.delay_timer > 0) --chip8.delay_timer;
                if (chip8.sound_timer > 0) --chip8.sound_timer;

                if (audio_dev != 0) {
                    SDL_PauseAudioDevice(audio_dev, chip8.sound_timer > 0 ? 0 : 1);
                }
            }

            if (chip8.draw_flag) {
                static uint32_t pixels[kWidth * kHeight];
                for (int i = 0; i < kWidth * kHeight; ++i) {
                    pixels[i] = chip8.gfx[i] ? 0xFFFFFFFF : 0xFF000000; // White on Black
                }

                SDL_UpdateTexture(texture, nullptr, pixels, kWidth * sizeof(uint32_t));
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);

                chip8.draw_flag = false;
            } else if (paused) {
                // Ensure overlay still presents while paused (cheap, keeps window alive).
                SDL_RenderPresent(renderer);
            }
        } else {
            SDL_Delay(1);
        }
    }

    if (audio_dev != 0) SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
