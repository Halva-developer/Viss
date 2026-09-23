#pragma once
#include "../vissrt.hpp"
#include <iostream>
#include <vector>
#include <cstdint>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace viss {
namespace retrotech {

    class ScreenBuffer {
    public:
        int width = 32;
        int height = 32;
        std::vector<uint8_t> pixels;
        uint8_t mask_r = 255;
        uint8_t mask_g = 255;
        uint8_t mask_b = 255;

        ScreenBuffer() : pixels(32 * 32, 0) {}

        void draw_raw(const viss::Bytes& map, int w, int h) {
            width = w;
            height = h;
            pixels.resize(w * h, 0);
            for (int i = 0; i < w * h && i < (int)map.size(); ++i) {
                pixels[i] = map.get(i);
            }
        }

        void color_screen(const viss::Bytes& mask, const viss::Bytes& map) {
            if (mask.size() >= 3) {
                mask_r = mask.get(0);
                mask_g = mask.get(1);
                mask_b = mask.get(2);
            }
            if (map.size() > 0) {
                if (pixels.empty() || (int)pixels.size() != (int)map.size()) {
                    pixels.resize(map.size(), 0);
                }
                for (int i = 0; i < (int)pixels.size() && i < (int)map.size(); ++i) {
                    pixels[i] = map.get(i);
                }
            }
        }

        void clear() {
            std::fill(pixels.begin(), pixels.end(), 0);
        }

        void update() {
            #ifdef _WIN32
            static bool vt_enabled = false;
            if (!vt_enabled) {
                HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
                if (hOut != INVALID_HANDLE_VALUE) {
                    DWORD dwMode = 0;
                    if (GetConsoleMode(hOut, &dwMode)) {
                        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
                    }
                }
                vt_enabled = true;
            }
            #endif

            std::cout << "\n=== [ VISS RETRO SCREEN " << width << "x" << height << " ] ===\n";
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int idx = y * width + x;
                    uint8_t raw_val = (idx < (int)pixels.size()) ? pixels[idx] : 0;
                    
                    int r = (int)((raw_val * mask_r) / 255);
                    int g = (int)((raw_val * mask_g) / 255);
                    int b = (int)((raw_val * mask_b) / 255);

                    // 24-bit ANSI color block (two spaces with background color)
                    std::cout << "\033[48;2;" << r << ";" << g << ";" << b << "m  \033[0m";
                }
                std::cout << "\n";
            }
            std::cout << "=======================================\n\n";
        }
    };

    inline ScreenBuffer& getScreen() {
        static ScreenBuffer screen;
        return screen;
    }

    inline void DrawRawPixels(const viss::Bytes& map, int w = 32, int h = 32) {
        getScreen().draw_raw(map, w, h);
    }

    inline void ColorScreen(const viss::Bytes& colormask, const viss::Bytes& map) {
        getScreen().color_screen(colormask, map);
    }

    inline void UpdateScreen() {
        getScreen().update();
    }

    inline void ClearScreen() {
        getScreen().clear();
    }

    inline void Beep(Int freq = 800, Int duration_ms = 100) {
        #ifdef _WIN32
        ::Beep((DWORD)freq, (DWORD)duration_ms);
        #else
        std::cout << "\a" << std::flush;
        #endif
    }
}
}
