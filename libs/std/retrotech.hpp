#pragma once
#include "../vissrt.hpp"
#include <iostream>
#include <vector>
#include <cstdint>
#include <array>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <conio.h>
#endif

namespace viss {
namespace retrotech {

    struct RGBColor {
        uint8_t r, g, b;
    };

    class ScreenBuffer {
    public:
        int width = 32;
        int height = 24;
        std::vector<uint8_t> pixels;
        bool use_palette = true;
        uint8_t mask_r = 255;
        uint8_t mask_g = 255;
        uint8_t mask_b = 255;

        // 256-color palette (default initialized to 16 classic NES / Retro colors)
        std::array<RGBColor, 256> palette;

        ScreenBuffer() : width(32), height(24), pixels(32 * 24, 0), use_palette(true) {
            init_default_palette();
        }

        void init_default_palette() {
            for (auto& c : palette) c = {0, 0, 0};

            // Classic 16-color Retro NES Palette
            palette[0]  = {92, 148, 252}; // 0: Sky Blue (NES Mario Sky)
            palette[1]  = {180, 70, 20};  // 1: Ground / Brick Brown
            palette[2]  = {236, 30, 30};  // 2: Mario Red (Cap / Shirt)
            palette[3]  = {0, 68, 220};   // 3: Mario Overalls Blue
            palette[4]  = {252, 188, 176};// 4: Peach / Skin tone
            palette[5]  = {0, 168, 0};    // 5: Pipe / Bush Green
            palette[6]  = {252, 216, 0};  // 6: Question Block / Coin Yellow
            palette[7]  = {255, 255, 255};// 7: White (Cloud / Eyes / Text)
            palette[8]  = {0, 0, 0};      // 8: Black (Mustache / Outline / Deep)
            palette[9]  = {120, 40, 10};  // 9: Goomba Dark Brown
            palette[10] = {0, 100, 0};    // 10: Pipe Dark Green / Shadow
            palette[11] = {240, 140, 0};  // 11: Orange / Brick Highlight
            palette[12] = {90, 90, 90};   // 12: Stone / Castle Gray
            palette[13] = {180, 180, 180};// 13: Light Gray
            palette[14] = {140, 20, 20};  // 14: Dark Red
            palette[15] = {15, 25, 70};   // 15: Night Sky / Dark Blue
        }

        void resize(int w, int h) {
            width = w;
            height = h;
            pixels.resize(w * h, 0);
        }

        void clear(uint8_t color_idx = 0) {
            std::fill(pixels.begin(), pixels.end(), color_idx);
        }

        void set_pixel(int x, int y, uint8_t color_idx) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                pixels[y * width + x] = color_idx;
            }
        }

        uint8_t get_pixel(int x, int y) const {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                return pixels[y * width + x];
            }
            return 0;
        }

        void draw_rect(int x, int y, int w, int h, uint8_t color_idx) {
            for (int dy = 0; dy < h; ++dy) {
                for (int dx = 0; dx < w; ++dx) {
                    set_pixel(x + dx, y + dy, color_idx);
                }
            }
        }

        void draw_sprite(const viss::Bytes& sprite, int x, int y, int w, int h, int transparent_color = 0) {
            for (int sy = 0; sy < h; ++sy) {
                for (int sx = 0; sx < w; ++sx) {
                    int sidx = sy * w + sx;
                    if (sidx < (int)sprite.size()) {
                        uint8_t col = sprite.get(sidx);
                        if (transparent_color < 0 || col != (uint8_t)transparent_color) {
                            set_pixel(x + sx, y + sy, col);
                        }
                    }
                }
            }
        }

        void draw_sprite_flipped(const viss::Bytes& sprite, int x, int y, int w, int h, bool flip_x, int transparent_color = 0) {
            for (int sy = 0; sy < h; ++sy) {
                for (int sx = 0; sx < w; ++sx) {
                    int src_x = flip_x ? (w - 1 - sx) : sx;
                    int sidx = sy * w + src_x;
                    if (sidx < (int)sprite.size()) {
                        uint8_t col = sprite.get(sidx);
                        if (transparent_color < 0 || col != (uint8_t)transparent_color) {
                            set_pixel(x + sx, y + sy, col);
                        }
                    }
                }
            }
        }

        void set_palette_color(int id, int r, int g, int b) {
            if (id >= 0 && id < 256) {
                palette[id] = {(uint8_t)r, (uint8_t)g, (uint8_t)b};
            }
        }

        void color_screen(const viss::Bytes& mask, const viss::Bytes& map) {
            if (mask.size() >= 48) {
                use_palette = true;
                for (int i = 0; i < (int)mask.size() / 3 && i < 256; ++i) {
                    palette[i] = {(uint8_t)mask.get(i * 3), (uint8_t)mask.get(i * 3 + 1), (uint8_t)mask.get(i * 3 + 2)};
                }
            } else if (mask.size() >= 3) {
                mask_r = mask.get(0);
                mask_g = mask.get(1);
                mask_b = mask.get(2);
                use_palette = false;
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

        void draw_raw(const viss::Bytes& map, int w, int h) {
            width = w;
            height = h;
            pixels.resize(w * h, 0);
            for (int i = 0; i < w * h && i < (int)map.size(); ++i) {
                pixels[i] = map.get(i);
            }
        }

        bool compact_mode = false; // When true, uses ▀ half-block rendering (2 vertical pixels per char cell)

        void set_compact(bool compact) {
            compact_mode = compact;
        }

        void set_render_mode(const std::string& mode) {
            if (mode == "compact" || mode == "halfblock" || mode == "small" || mode == "highres") {
                compact_mode = true;
            } else {
                compact_mode = false;
            }
        }

        void update() {
            #ifdef _WIN32
            static bool vt_enabled = false;
            if (!vt_enabled) {
                SetConsoleOutputCP(CP_UTF8);
                HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
                if (hOut != INVALID_HANDLE_VALUE) {
                    DWORD dwMode = 0;
                    if (GetConsoleMode(hOut, &dwMode)) {
                        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
                    }
                    CONSOLE_CURSOR_INFO cursorInfo;
                    GetConsoleCursorInfo(hOut, &cursorInfo);
                    cursorInfo.bVisible = FALSE;
                    SetConsoleCursorInfo(hOut, &cursorInfo);
                }
                vt_enabled = true;
            }
            #endif

            std::string frame = "\033[H";
            frame.reserve(width * height * 16 + 128);

            if (compact_mode) {
                // Half-block rendering: 1 terminal char = 2 vertical pixels (top = FG, bottom = BG)
                int last_fg_r = -1, last_fg_g = -1, last_fg_b = -1;
                int last_bg_r = -1, last_bg_g = -1, last_bg_b = -1;

                for (int y = 0; y < height; y += 2) {
                    for (int x = 0; x < width; ++x) {
                        int top_idx = y * width + x;
                        int bot_idx = (y + 1) * width + x;

                        uint8_t top_val = (top_idx < (int)pixels.size()) ? pixels[top_idx] : 0;
                        uint8_t bot_val = (y + 1 < height && bot_idx < (int)pixels.size()) ? pixels[bot_idx] : 0;

                        int fg_r, fg_g, fg_b;
                        int bg_r, bg_g, bg_b;

                        if (use_palette) {
                            const auto& tc = palette[top_val];
                            fg_r = tc.r; fg_g = tc.g; fg_b = tc.b;
                            const auto& bc = palette[bot_val];
                            bg_r = bc.r; bg_g = bc.g; bg_b = bc.b;
                        } else {
                            fg_r = (int)((top_val * mask_r) / 255);
                            fg_g = (int)((top_val * mask_g) / 255);
                            fg_b = (int)((top_val * mask_b) / 255);
                            bg_r = (int)((bot_val * mask_r) / 255);
                            bg_g = (int)((bot_val * mask_g) / 255);
                            bg_b = (int)((bot_val * mask_b) / 255);
                        }

                        if (fg_r != last_fg_r || fg_g != last_fg_g || fg_b != last_fg_b) {
                            frame += "\033[38;2;" + std::to_string(fg_r) + ";" + std::to_string(fg_g) + ";" + std::to_string(fg_b) + "m";
                            last_fg_r = fg_r; last_fg_g = fg_g; last_fg_b = fg_b;
                        }
                        if (bg_r != last_bg_r || bg_g != last_bg_g || bg_b != last_bg_b) {
                            frame += "\033[48;2;" + std::to_string(bg_r) + ";" + std::to_string(bg_g) + ";" + std::to_string(bg_b) + "m";
                            last_bg_r = bg_r; last_bg_g = bg_g; last_bg_b = bg_b;
                        }

                        // UTF-8 Upper Half Block: ▀ (\u2580)
                        frame += "\xE2\x96\x80";
                    }
                    frame += "\033[0m\n";
                    last_fg_r = -1; last_fg_g = -1; last_fg_b = -1;
                    last_bg_r = -1; last_bg_g = -1; last_bg_b = -1;
                }
            } else {
                int last_r = -1, last_g = -1, last_b = -1;

                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        int idx = y * width + x;
                        uint8_t raw_val = (idx < (int)pixels.size()) ? pixels[idx] : 0;
                        
                        int r, g, b;
                        if (use_palette) {
                            const auto& col = palette[raw_val];
                            r = col.r;
                            g = col.g;
                            b = col.b;
                        } else {
                            r = (int)((raw_val * mask_r) / 255);
                            g = (int)((raw_val * mask_g) / 255);
                            b = (int)((raw_val * mask_b) / 255);
                        }

                        if (r != last_r || g != last_g || b != last_b) {
                            frame += "\033[48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
                            last_r = r; last_g = g; last_b = b;
                        }
                        frame += "  ";
                    }
                    frame += "\033[0m\n";
                    last_r = -1; last_g = -1; last_b = -1;
                }
            }
            std::cout << frame << std::flush;
        }
    };

    inline ScreenBuffer& getScreen() {
        static ScreenBuffer screen;
        return screen;
    }

    inline void InitScreen(int w = 32, int h = 24) {
        getScreen().resize(w, h);
    }

    inline void DrawRawPixels(const viss::Bytes& map, int w = 32, int h = 32) {
        getScreen().draw_raw(map, w, h);
    }

    inline void SetCompact(Bool compact = true) {
        getScreen().set_compact(compact);
    }

    inline void SetRenderMode(const Str& mode) {
        getScreen().set_render_mode(mode);
    }

    inline void ColorScreen(const viss::Bytes& colormask, const viss::Bytes& map) {
        getScreen().color_screen(colormask, map);
    }

    inline void UpdateScreen() {
        getScreen().update();
    }

    inline void ClearScreen(int color = 0) {
        getScreen().clear((uint8_t)color);
    }

    inline void DrawPixel(int x, int y, int color) {
        getScreen().set_pixel(x, y, (uint8_t)color);
    }

    inline int GetPixel(int x, int y) {
        return (int)getScreen().get_pixel(x, y);
    }

    inline void DrawRect(int x, int y, int w, int h, int color) {
        getScreen().draw_rect(x, y, w, h, (uint8_t)color);
    }

    inline void DrawSprite(const viss::Bytes& sprite, int x, int y, int w, int h, int transparent_color = 0) {
        getScreen().draw_sprite(sprite, x, y, w, h, transparent_color);
    }

    inline void DrawSpriteFlipped(const viss::Bytes& sprite, int x, int y, int w, int h, bool flip_x, int transparent_color = 0) {
        getScreen().draw_sprite_flipped(sprite, x, y, w, h, flip_x, transparent_color);
    }

    inline void SetPaletteColor(int id, int r, int g, int b) {
        getScreen().set_palette_color(id, r, g, b);
    }

    inline void Beep(Int freq = 800, Int duration_ms = 100) {
        #ifdef _WIN32
        ::Beep((DWORD)freq, (DWORD)duration_ms);
        #else
        std::cout << "\a" << std::flush;
        #endif
    }

    inline Bool HasKey() {
        #ifdef _WIN32
        return _kbhit() != 0;
        #else
        return false;
        #endif
    }
    inline Bool HasKey_q() { return HasKey(); }

    inline Str GetKey() {
        if (HasKey()) {
            #ifdef _WIN32
            char ch = (char)_getch();
            return Str(1, ch);
            #endif
        }
        return "";
    }
}
}
