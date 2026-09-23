#pragma once
#include "../vissrt.hpp"
#include <iostream>
#include <vector>
#include <cstdint>
#include <array>
#include <string>
#include <cmath>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdlib>
#include <algorithm>

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

    struct CharCell {
        std::string text = "";
        uint8_t fg = 7;
        uint8_t bg = 0;
        bool has_char = false;
    };

    // =========================================================================
    // 8-BIT RETRO AUDIO ENGINE (Square, Triangle, Noise, Chiptune Music)
    // =========================================================================
    class RetroAudioEngine {
    private:
        #ifdef _WIN32
        HMODULE hWinmm = nullptr;
        typedef UINT (WINAPI *waveOutOpen_t)(LPHWAVEOUT, UINT, LPCWAVEFORMATEX, DWORD_PTR, DWORD_PTR, DWORD);
        typedef UINT (WINAPI *waveOutPrepareHeader_t)(HWAVEOUT, LPWAVEHDR, UINT);
        typedef UINT (WINAPI *waveOutWrite_t)(HWAVEOUT, LPWAVEHDR, UINT);
        typedef UINT (WINAPI *waveOutUnprepareHeader_t)(HWAVEOUT, LPWAVEHDR, UINT);
        typedef UINT (WINAPI *waveOutClose_t)(HWAVEOUT);

        waveOutOpen_t pWaveOutOpen = nullptr;
        waveOutPrepareHeader_t pWaveOutPrepareHeader = nullptr;
        waveOutWrite_t pWaveOutWrite = nullptr;
        waveOutUnprepareHeader_t pWaveOutUnprepareHeader = nullptr;
        waveOutClose_t pWaveOutClose = nullptr;

        HWAVEOUT hWaveOut = nullptr;
        bool pcm_ready = false;
        #endif

        std::mutex audio_mtx;
        bool music_playing = false;

    public:
        RetroAudioEngine() {
            #ifdef _WIN32
            init_winmm();
            #endif
        }

        ~RetroAudioEngine() {
            #ifdef _WIN32
            music_playing = false;
            if (hWaveOut && pWaveOutClose) {
                pWaveOutClose(hWaveOut);
            }
            if (hWinmm) {
                FreeLibrary(hWinmm);
            }
            #endif
        }

        void init_winmm() {
            #ifdef _WIN32
            if (pcm_ready) return;
            hWinmm = LoadLibraryA("winmm.dll");
            if (!hWinmm) return;

            pWaveOutOpen = (waveOutOpen_t)GetProcAddress(hWinmm, "waveOutOpen");
            pWaveOutPrepareHeader = (waveOutPrepareHeader_t)GetProcAddress(hWinmm, "waveOutPrepareHeader");
            pWaveOutWrite = (waveOutWrite_t)GetProcAddress(hWinmm, "waveOutWrite");
            pWaveOutUnprepareHeader = (waveOutUnprepareHeader_t)GetProcAddress(hWinmm, "waveOutUnprepareHeader");
            pWaveOutClose = (waveOutClose_t)GetProcAddress(hWinmm, "waveOutClose");

            if (pWaveOutOpen && pWaveOutPrepareHeader && pWaveOutWrite) {
                WAVEFORMATEX wfx;
                wfx.wFormatTag = 1; // WAVE_FORMAT_PCM
                wfx.nChannels = 1;  // Mono
                wfx.nSamplesPerSec = 22050;
                wfx.nAvgBytesPerSec = 22050;
                wfx.nBlockAlign = 1;
                wfx.wBitsPerSample = 8;
                wfx.cbSize = 0;

                if (pWaveOutOpen(&hWaveOut, (UINT)-1 /* WAVE_MAPPER */, &wfx, 0, 0, 0) == 0) {
                    pcm_ready = true;
                }
            }
            #endif
        }

        // Generate synthetic wave: square (pulse), triangle, noise, sine
        std::vector<uint8_t> generate_wave(double start_freq, double end_freq, int duration_ms, 
                                           const std::string& wave_type = "square", double duty = 0.5, int volume = 90) {
            int sample_rate = 22050;
            int total_samples = (sample_rate * duration_ms) / 1000;
            if (total_samples < 1) total_samples = 1;
            std::vector<uint8_t> buf(total_samples, 128);

            int amp = (volume * 110) / 100;
            if (amp > 120) amp = 120;
            double current_phase = 0.0;

            for (int i = 0; i < total_samples; ++i) {
                double t = (double)i / total_samples;
                double freq = start_freq + (end_freq - start_freq) * t;
                if (freq < 15.0) freq = 15.0;

                current_phase += freq / sample_rate;
                if (current_phase >= 1.0) current_phase -= (int)current_phase;

                int sample = 128;
                if (wave_type == "square" || wave_type == "pulse") {
                    sample = (current_phase < duty) ? (128 + amp) : (128 - amp);
                } else if (wave_type == "triangle") {
                    double val = (current_phase < 0.5) ? (4.0 * current_phase - 1.0) : (3.0 - 4.0 * current_phase);
                    sample = 128 + (int)(val * amp);
                } else if (wave_type == "noise") {
                    sample = 128 + (rand() % (amp * 2 + 1)) - amp;
                } else if (wave_type == "sine") {
                    sample = 128 + (int)(sin(current_phase * 6.28318530718) * amp);
                } else {
                    sample = (current_phase < 0.5) ? (128 + amp) : (128 - amp);
                }

                // Envelope attack/decay smoothing
                if (i < 60) sample = 128 + ((sample - 128) * i) / 60;
                else if (i > total_samples - 60) sample = 128 + ((sample - 128) * (total_samples - i)) / 60;

                if (sample < 0) sample = 0;
                if (sample > 255) sample = 255;
                buf[i] = (uint8_t)sample;
            }
            return buf;
        }

        // Fire-and-forget non-blocking audio output
        void play_async(const std::vector<uint8_t>& samples, int duration_ms, int fallback_freq = 800) {
            #ifdef _WIN32
            if (!pcm_ready) {
                // Fallback to Beep
                std::thread([fallback_freq, duration_ms]() {
                    ::Beep((DWORD)fallback_freq, (DWORD)duration_ms);
                }).detach();
                return;
            }

            std::thread([this, samples, duration_ms]() {
                std::lock_guard<std::mutex> lock(audio_mtx);
                WAVEHDR header;
                memset(&header, 0, sizeof(header));
                header.lpData = (LPSTR)samples.data();
                header.dwBufferLength = (DWORD)samples.size();

                pWaveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
                pWaveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
                std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms + 10));
                pWaveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            }).detach();
            #else
            std::cout << "\a" << std::flush;
            #endif
        }

        void tone(double freq, int ms, const std::string& wave_type = "square", double duty = 0.5, int vol = 90) {
            auto samples = generate_wave(freq, freq, ms, wave_type, duty, vol);
            play_async(samples, ms, (int)freq);
        }

        void sweep(double start_freq, double end_freq, int ms, const std::string& wave_type = "square", double duty = 0.5) {
            auto samples = generate_wave(start_freq, end_freq, ms, wave_type, duty, 90);
            play_async(samples, ms, (int)start_freq);
        }

        // Sound effect presets
        void play_sfx(const std::string& name) {
            if (name == "jump") {
                // Fast rising pitch square wave (NES jump)
                sweep(150, 620, 130, "square", 0.25);
            } else if (name == "coin") {
                // Two-tone bell (B5 -> E6)
                std::thread([this]() {
                    tone(987, 70, "square", 0.5, 95);
                    std::this_thread::sleep_for(std::chrono::milliseconds(70));
                    tone(1319, 220, "square", 0.5, 95);
                }).detach();
            } else if (name == "stomp") {
                // Quick downward pitch drop
                sweep(360, 60, 90, "square", 0.5);
            } else if (name == "powerup") {
                // Fast rising 7-note arpeggio
                std::thread([this]() {
                    int notes[] = {330, 392, 659, 523, 587, 784, 988};
                    for (int n : notes) {
                        tone(n, 45, "triangle", 0.5, 95);
                        std::this_thread::sleep_for(std::chrono::milliseconds(45));
                    }
                }).detach();
            } else if (name == "bump") {
                sweep(160, 90, 70, "square", 0.5);
            } else if (name == "hurt" || name == "die") {
                sweep(240, 50, 250, "noise", 0.5);
            } else if (name == "fireball") {
                sweep(800, 200, 70, "noise", 0.5);
            } else if (name == "flag") {
                std::thread([this]() {
                    int fanfares[] = {784, 988, 1319, 1175, 1319};
                    for (int n : fanfares) {
                        tone(n, 120, "square", 0.5, 90);
                        std::this_thread::sleep_for(std::chrono::milliseconds(130));
                    }
                }).detach();
            } else if (name == "gameover") {
                std::thread([this]() {
                    int notes[] = {523, 440, 392, 330, 294, 261};
                    for (int n : notes) {
                        tone(n, 150, "square", 0.25, 90);
                        std::this_thread::sleep_for(std::chrono::milliseconds(160));
                    }
                }).detach();
            }
        }

        // Note parser: C4, D#5, Bb3, etc. -> Hz
        double note_to_freq(const std::string& note_str) {
            if (note_str.empty() || note_str == ".") return 0.0;
            char n = toupper(note_str[0]);
            int semitone = 0;
            switch (n) {
                case 'C': semitone = 0; break;
                case 'D': semitone = 2; break;
                case 'E': semitone = 4; break;
                case 'F': semitone = 5; break;
                case 'G': semitone = 7; break;
                case 'A': semitone = 9; break;
                case 'B': semitone = 11; break;
                default: return 0.0;
            }
            size_t idx = 1;
            if (idx < note_str.size() && note_str[idx] == '#') {
                semitone += 1;
                idx++;
            } else if (idx < note_str.size() && note_str[idx] == 'b') {
                semitone -= 1;
                idx++;
            }
            int octave = 4;
            if (idx < note_str.size() && isdigit(note_str[idx])) {
                octave = note_str[idx] - '0';
            }
            int note_num = (octave + 1) * 12 + semitone; // MIDI note number
            return 440.0 * pow(2.0, (note_num - 69) / 12.0);
        }

        // Background music player
        void play_music(const std::string& melody, int bpm = 120) {
            music_playing = true;
            std::thread([this, melody, bpm]() {
                std::stringstream ss(melody);
                std::string item;
                int quarter_ms = 60000 / bpm;

                while (music_playing && (ss >> item)) {
                    int duration_ms = quarter_ms / 2; // Default eighth note
                    std::string note_part = item;
                    size_t colon = item.find(':');
                    if (colon != std::string::npos) {
                        note_part = item.substr(0, colon);
                        int div = atoi(item.substr(colon + 1).c_str());
                        if (div > 0) duration_ms = (quarter_ms * 4) / div;
                    }

                    if (note_part == "." || note_part == "R" || note_part == "r") {
                        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
                    } else {
                        double f = note_to_freq(note_part);
                        if (f > 20.0) {
                            tone(f, duration_ms - 15, "square", 0.5, 80);
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
                    }
                }
                music_playing = false;
            }).detach();
        }

        void stop_music() {
            music_playing = false;
        }
    };

    inline RetroAudioEngine& getAudio() {
        static RetroAudioEngine engine;
        return engine;
    }

    // =========================================================================
    // RETRO VIDEO DISPLAY BUFFER
    // =========================================================================
    class ScreenBuffer {
    public:
        int width = 32;
        int height = 24;
        std::vector<uint8_t> pixels;
        std::vector<CharCell> text_overlay;
        bool use_palette = true;
        uint8_t mask_r = 255;
        uint8_t mask_g = 255;
        uint8_t mask_b = 255;

        bool compact_mode = false;
        int camera_x = 0;
        int camera_y = 0;
        int shake_amount = 0;
        int shake_frames = 0;

        // 256-color palette (default initialized to 16 classic NES / Retro colors)
        std::array<RGBColor, 256> palette;

        ScreenBuffer() : width(32), height(24), pixels(32 * 24, 0), text_overlay(32 * 24), use_palette(true) {
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
            text_overlay.resize(w * h);
        }

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

        void clear(uint8_t color_idx = 0) {
            std::fill(pixels.begin(), pixels.end(), color_idx);
            for (auto& cell : text_overlay) {
                cell.has_char = false;
                cell.text.clear();
            }
        }

        void set_pixel(int x, int y, uint8_t color_idx) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                pixels[y * width + x] = color_idx;
                text_overlay[y * width + x].has_char = false;
            }
        }

        uint8_t get_pixel(int x, int y) const {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                return pixels[y * width + x];
            }
            return 0;
        }

        // Draw character/symbol with custom foreground and background colors
        void draw_char(int x, int y, const std::string& symbol, uint8_t fg, uint8_t bg) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                int idx = y * width + x;
                pixels[idx] = bg;
                text_overlay[idx].text = symbol;
                text_overlay[idx].fg = fg;
                text_overlay[idx].bg = bg;
                text_overlay[idx].has_char = true;
            }
        }

        // Draw text string across consecutive cells
        void draw_text(int x, int y, const std::string& text, uint8_t fg, uint8_t bg) {
            for (size_t i = 0; i < text.size(); ++i) {
                draw_char(x + (int)i, y, std::string(1, text[i]), fg, bg);
            }
        }

        // Draw filled block of characters (e.g. 2x2 Question Block with '?')
        void draw_block(int x, int y, int w, int h, const std::string& symbol, uint8_t fg, uint8_t bg) {
            for (int dy = 0; dy < h; ++dy) {
                for (int dx = 0; dx < w; ++dx) {
                    draw_char(x + dx, y + dy, symbol, fg, bg);
                }
            }
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

        void shake(int amount = 2, int frames = 4) {
            shake_amount = amount;
            shake_frames = frames;
        }

        void set_camera(int cx, int cy = 0) {
            camera_x = cx;
            camera_y = cy;
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
                    text_overlay.resize(map.size());
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
            text_overlay.resize(w * h);
            for (int i = 0; i < w * h && i < (int)map.size(); ++i) {
                pixels[i] = map.get(i);
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

            // Apply screen shake if active
            int cur_shake_x = 0;
            int cur_shake_y = 0;
            if (shake_frames > 0) {
                cur_shake_x = (rand() % (shake_amount * 2 + 1)) - shake_amount;
                cur_shake_y = (rand() % (shake_amount * 2 + 1)) - shake_amount;
                shake_frames--;
            }

            std::string frame = "\033[H";
            frame.reserve(width * height * 16 + 256);

            if (compact_mode) {
                // High-Res Half-Block rendering: 1 terminal char = 2 vertical pixels
                int last_fg_r = -1, last_fg_g = -1, last_fg_b = -1;
                int last_bg_r = -1, last_bg_g = -1, last_bg_b = -1;

                for (int y = 0; y < height; y += 2) {
                    for (int x = 0; x < width; ++x) {
                        int sx = x + cur_shake_x;
                        int sy_top = y + cur_shake_y;
                        int sy_bot = y + 1 + cur_shake_y;

                        int top_idx = (sx >= 0 && sx < width && sy_top >= 0 && sy_top < height) ? (sy_top * width + sx) : -1;
                        int bot_idx = (sx >= 0 && sx < width && sy_bot >= 0 && sy_bot < height) ? (sy_bot * width + sx) : -1;

                        // Check text symbol overlay
                        if (top_idx >= 0 && text_overlay[top_idx].has_char) {
                            const auto& cell = text_overlay[top_idx];
                            const auto& fc = palette[cell.fg];
                            const auto& bc = palette[cell.bg];
                            frame += "\033[38;2;" + std::to_string(fc.r) + ";" + std::to_string(fc.g) + ";" + std::to_string(fc.b) + "m";
                            frame += "\033[48;2;" + std::to_string(bc.r) + ";" + std::to_string(bc.g) + ";" + std::to_string(bc.b) + "m";
                            frame += cell.text;
                            last_fg_r = fc.r; last_fg_g = fc.g; last_fg_b = fc.b;
                            last_bg_r = bc.r; last_bg_g = bc.g; last_bg_b = bc.b;
                            continue;
                        }

                        uint8_t top_val = (top_idx >= 0 && top_idx < (int)pixels.size()) ? pixels[top_idx] : 0;
                        uint8_t bot_val = (bot_idx >= 0 && bot_idx < (int)pixels.size()) ? pixels[bot_idx] : 0;

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

                        // UTF-8 Upper Half Block: ▀
                        frame += "\xE2\x96\x80";
                    }
                    frame += "\033[0m\n";
                    last_fg_r = -1; last_fg_g = -1; last_fg_b = -1;
                    last_bg_r = -1; last_bg_g = -1; last_bg_b = -1;
                }
            } else {
                // Classic block mode (2 spaces per pixel)
                int last_r = -1, last_g = -1, last_b = -1;
                int last_fg_r = -1, last_fg_g = -1, last_fg_b = -1;

                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        int sx = x + cur_shake_x;
                        int sy = y + cur_shake_y;
                        int idx = (sx >= 0 && sx < width && sy >= 0 && sy < height) ? (sy * width + sx) : -1;

                        // Check symbol overlay (e.g. "?" inside Question block!)
                        if (idx >= 0 && text_overlay[idx].has_char) {
                            const auto& cell = text_overlay[idx];
                            const auto& fc = palette[cell.fg];
                            const auto& bc = palette[cell.bg];
                            frame += "\033[38;2;" + std::to_string(fc.r) + ";" + std::to_string(fc.g) + ";" + std::to_string(fc.b) + "m";
                            frame += "\033[48;2;" + std::to_string(bc.r) + ";" + std::to_string(bc.g) + ";" + std::to_string(bc.b) + "m";
                            if (cell.text.size() == 1) {
                                frame += " " + cell.text; // Centered 1-char symbol
                            } else {
                                frame += cell.text;
                            }
                            last_r = bc.r; last_g = bc.g; last_b = bc.b;
                            last_fg_r = fc.r; last_fg_g = fc.g; last_fg_b = fc.b;
                            continue;
                        }

                        uint8_t raw_val = (idx >= 0 && idx < (int)pixels.size()) ? pixels[idx] : 0;
                        int r, g, b;
                        if (use_palette) {
                            const auto& col = palette[raw_val];
                            r = col.r; g = col.g; b = col.b;
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
                    last_fg_r = -1; last_fg_g = -1; last_fg_b = -1;
                }
            }
            std::cout << frame << std::flush;
        }
    };

    inline ScreenBuffer& getScreen() {
        static ScreenBuffer screen;
        return screen;
    }

    // =========================================================================
    // RETROTECH API FUNCTIONS
    // =========================================================================
    inline void InitScreen(int w = 32, int h = 24) {
        getScreen().resize(w, h);
    }

    inline void SetCompact(Bool compact = true) {
        getScreen().set_compact(compact);
    }

    inline void SetRenderMode(const Str& mode) {
        getScreen().set_render_mode(mode);
    }

    inline void SetPixelSize(const Str& mode) {
        getScreen().set_render_mode(mode);
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

    inline void DrawChar(int x, int y, const Str& symbol, int fg, int bg) {
        getScreen().draw_char(x, y, symbol, (uint8_t)fg, (uint8_t)bg);
    }

    inline void DrawText(int x, int y, const Str& text, int fg, int bg) {
        getScreen().draw_text(x, y, text, (uint8_t)fg, (uint8_t)bg);
    }

    inline void DrawBlock(int x, int y, int w, int h, const Str& symbol, int fg, int bg) {
        getScreen().draw_block(x, y, w, h, symbol, (uint8_t)fg, (uint8_t)bg);
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

    inline void SetCamera(int cx, int cy = 0) {
        getScreen().set_camera(cx, cy);
    }

    inline int GetCameraX() {
        return getScreen().camera_x;
    }

    inline int GetCameraY() {
        return getScreen().camera_y;
    }

    inline void Shake(int amount = 2, int frames = 4) {
        getScreen().shake(amount, frames);
    }

    // Bounding-box AABB collision detection helper
    inline Bool Collides(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
        return (x1 < x2 + w2) && (x1 + w1 > x2) && (y1 < y2 + h2) && (y1 + h1 > y2);
    }
    inline Bool Collides_q(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
        return Collides(x1, y1, w1, h1, x2, y2, w2, h2);
    }

    // Built-in Retro Tile Drawer
    inline void DrawTile(const Str& tile_type, int x, int y, int size = 4) {
        if (tile_type == "question") {
            getScreen().draw_rect(x, y, size, size, 6); // Yellow block
            getScreen().draw_char(x + size / 2, y + size / 2, "?", 8, 6);
        } else if (tile_type == "empty_block") {
            getScreen().draw_rect(x, y, size, size, 12); // Gray block
        } else if (tile_type == "brick") {
            getScreen().draw_rect(x, y, size, size, 1);  // Brown brick
            getScreen().draw_char(x + size / 2, y + size / 2, "#", 14, 1);
        } else if (tile_type == "pipe_top") {
            getScreen().draw_rect(x, y, size * 2, size, 5); // Green pipe lip
            getScreen().draw_rect(x, y, size * 2, 1, 10);
        } else if (tile_type == "pipe_body") {
            getScreen().draw_rect(x + 1, y, size * 2 - 2, size, 5); // Green pipe body
        } else if (tile_type == "ground") {
            getScreen().draw_rect(x, y, size, 1, 5);         // Grass top
            getScreen().draw_rect(x, y + 1, size, size - 1, 1); // Brown earth
        }
    }

    // =========================================================================
    // AUDIO API FUNCTIONS
    // =========================================================================
    inline void Beep(Int freq = 800, Int duration_ms = 100) {
        #ifdef _WIN32
        ::Beep((DWORD)freq, (DWORD)duration_ms);
        #else
        std::cout << "\a" << std::flush;
        #endif
    }

    inline void Tone(Dec freq, Int duration_ms, const Str& wave_type = "square", Dec duty = 0.5, Int volume = 90) {
        getAudio().tone(freq, (int)duration_ms, wave_type, duty, (int)volume);
    }

    inline void Sweep(Dec start_freq, Dec end_freq, Int duration_ms, const Str& wave_type = "square", Dec duty = 0.5) {
        getAudio().sweep(start_freq, end_freq, (int)duration_ms, wave_type, duty);
    }

    inline void PlaySfx(const Str& name) {
        getAudio().play_sfx(name);
    }

    inline void SoundJump() {
        getAudio().play_sfx("jump");
    }

    inline void SoundCoin() {
        getAudio().play_sfx("coin");
    }

    inline void SoundStomp() {
        getAudio().play_sfx("stomp");
    }

    inline void SoundPowerup() {
        getAudio().play_sfx("powerup");
    }

    inline void SoundHurt() {
        getAudio().play_sfx("hurt");
    }

    inline void SoundBump() {
        getAudio().play_sfx("bump");
    }

    inline void SoundFlag() {
        getAudio().play_sfx("flag");
    }

    inline void SoundGameOver() {
        getAudio().play_sfx("gameover");
    }

    inline void PlayMusic(const Str& melody, Int bpm = 120) {
        getAudio().play_music(melody, (int)bpm);
    }

    inline void StopMusic() {
        getAudio().stop_music();
    }

    // =========================================================================
    // INPUT API FUNCTIONS
    // =========================================================================
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
