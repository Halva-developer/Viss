#pragma once
#include "../vissrt.hpp"
#include "mask.hpp"
#include <iostream>
#include <vector>
#include <queue>
#include <condition_variable>
#include <atomic>
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
#include <mmsystem.h>
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

    struct AudioJob {
        std::vector<uint8_t> samples;
        int duration_ms = 0;
        int fallback_freq = 0;
    };

    // =========================================================================
    // 8-BIT RETRO AUDIO ENGINE (Queue-based, Single Worker Thread)
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

        std::queue<AudioJob> job_queue;
        std::mutex q_mtx;
        std::condition_variable q_cv;
        std::thread worker;
        std::atomic<bool> audio_running{false};
        std::atomic<bool> music_playing{false};

    public:
        RetroAudioEngine() {
            #ifdef _WIN32
            init_winmm();
            #endif
            audio_running = true;
            worker = std::thread([this]() {
                while (audio_running) {
                    AudioJob job;
                    {
                        std::unique_lock<std::mutex> lock(q_mtx);
                        q_cv.wait(lock, [this]() { return !audio_running || !job_queue.empty(); });
                        if (!audio_running) break;
                        job = std::move(job_queue.front());
                        job_queue.pop();
                    }
                    #ifdef _WIN32
                    if (pcm_ready && hWaveOut && pWaveOutWrite && !job.samples.empty()) {
                        WAVEHDR header;
                        memset(&header, 0, sizeof(header));
                        header.lpData = (LPSTR)job.samples.data();
                        header.dwBufferLength = (DWORD)job.samples.size();
                        pWaveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
                        pWaveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
                        std::this_thread::sleep_for(std::chrono::milliseconds(job.duration_ms + 5));
                        pWaveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
                    } else if (job.fallback_freq > 0) {
                        ::Beep((DWORD)job.fallback_freq, (DWORD)job.duration_ms);
                    }
                    #endif
                }
            });
        }

        ~RetroAudioEngine() {
            stop_music();
            audio_running = false;
            q_cv.notify_all();
            if (worker.joinable()) {
                worker.join();
            }
            #ifdef _WIN32
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

                if (i < 60) sample = 128 + ((sample - 128) * i) / 60;
                else if (i > total_samples - 60) sample = 128 + ((sample - 128) * (total_samples - i)) / 60;

                if (sample < 0) sample = 0;
                if (sample > 255) sample = 255;
                buf[i] = (uint8_t)sample;
            }
            return buf;
        }

        void play_async(const std::vector<uint8_t>& samples, int duration_ms, int fallback_freq = 800) {
            {
                std::lock_guard<std::mutex> lock(q_mtx);
                if (job_queue.size() > 6) {
                    job_queue.pop();
                }
                job_queue.push({samples, duration_ms, fallback_freq});
            }
            q_cv.notify_one();
        }

        void tone(double freq, int ms, const std::string& wave_type = "square", double duty = 0.5, int vol = 90) {
            auto samples = generate_wave(freq, freq, ms, wave_type, duty, vol);
            play_async(samples, ms, (int)freq);
        }

        void sweep(double start_freq, double end_freq, int ms, const std::string& wave_type = "square", double duty = 0.5) {
            auto samples = generate_wave(start_freq, end_freq, ms, wave_type, duty, 90);
            play_async(samples, ms, (int)start_freq);
        }

        void play_sfx(const std::string& name) {
            if (name == "jump") {
                sweep(150, 620, 120, "square", 0.25);
            } else if (name == "coin") {
                tone(987, 60, "square", 0.5, 95);
                tone(1319, 180, "square", 0.5, 95);
            } else if (name == "stomp") {
                sweep(360, 60, 80, "square", 0.5);
            } else if (name == "powerup") {
                int notes[] = {330, 392, 659, 523, 587, 784, 988};
                for (int n : notes) tone(n, 40, "triangle", 0.5, 95);
            } else if (name == "bump") {
                sweep(160, 90, 60, "square", 0.5);
            } else if (name == "hurt" || name == "die") {
                sweep(240, 50, 200, "noise", 0.5);
            } else if (name == "fireball") {
                sweep(800, 200, 60, "noise", 0.5);
            } else if (name == "flag") {
                int fanfares[] = {784, 988, 1319, 1175, 1319};
                for (int n : fanfares) tone(n, 100, "square", 0.5, 90);
            } else if (name == "gameover") {
                int notes[] = {523, 440, 392, 330, 294, 261};
                for (int n : notes) tone(n, 120, "square", 0.25, 90);
            }
        }

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
            int note_num = (octave + 1) * 12 + semitone;
            return 440.0 * pow(2.0, (note_num - 69) / 12.0);
        }

        void play_music(const std::string& melody, int bpm = 120) {
            stop_music();
            music_playing = true;
            std::thread([this, melody, bpm]() {
                std::stringstream ss(melody);
                std::string item;
                int quarter_ms = 60000 / bpm;

                while (music_playing && (ss >> item)) {
                    int duration_ms = quarter_ms / 2;
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
                            tone(f, duration_ms - 15, "square", 0.5, 75);
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

        struct UIElement {
            int col;
            int row;
            std::string text;
            uint8_t fg;
            uint8_t bg;
            bool is_terminal_col;
        };
        std::vector<UIElement> ui_elements;

#ifdef _WIN32
        bool window_mode = false;
        HWND hwnd = NULL;
        int window_scale = 4;
        std::vector<uint32_t> dib_pixels;
        BITMAPINFO bmi = {};

        static LRESULT CALLBACK VissWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
            if (uMsg == WM_CLOSE || uMsg == WM_DESTROY) {
                PostQuitMessage(0);
                return 0;
            }
            return DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }

        bool open_window(const std::string& title = "Viss RetroTech Engine", int scale = 4) {
            window_scale = scale > 0 ? scale : 4;
            WNDCLASSEXA wc = { sizeof(WNDCLASSEXA) };
            wc.lpfnWndProc = VissWindowProc;
            wc.hInstance = GetModuleHandle(NULL);
            wc.lpszClassName = "VissRetroWindowClass";
            wc.hCursor = LoadCursor(NULL, IDC_ARROW);
            RegisterClassExA(&wc);

            RECT rc = { 0, 0, width * window_scale, height * window_scale };
            AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);
            int win_w = rc.right - rc.left;
            int win_h = rc.bottom - rc.top;

            hwnd = CreateWindowExA(
                0, "VissRetroWindowClass", title.c_str(),
                (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX) | WS_VISIBLE,
                CW_USEDEFAULT, CW_USEDEFAULT, win_w, win_h,
                NULL, NULL, GetModuleHandle(NULL), NULL
            );

            if (!hwnd) return false;

            memset(&bmi, 0, sizeof(bmi));
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = width;
            bmi.bmiHeader.biHeight = -height; // top-down DIB
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            dib_pixels.resize(width * height, 0);
            window_mode = true;
            return true;
        }

        bool is_window_open() const { return window_mode && hwnd != NULL; }
#endif

        void print_ui(int col, int row, const std::string& text, uint8_t fg = 10, uint8_t bg = 13, bool is_terminal_col = false) {
            ui_elements.push_back({col, row, text, fg, bg, is_terminal_col});
        }

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

        void set_colormask(const viss::Bytes& mask) {
            use_palette = true;
            size_t num_colors = mask.size() / 3;
            if (num_colors > 256) num_colors = 256;
            for (size_t i = 0; i < num_colors; ++i) {
                palette[i] = {(uint8_t)mask.get(i * 3), (uint8_t)mask.get(i * 3 + 1), (uint8_t)mask.get(i * 3 + 2)};
            }
        }

        void color_screen(const viss::Bytes& mask) {
            set_colormask(mask);
        }

        void color_screen(const viss::Bytes& mask, const viss::Bytes& map) {
            set_colormask(mask);
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

        void draw_tiles(const viss::Bytes& map, int map_w, int map_h, int x = 0, int y = 0, int tile_size = 1) {
            for (int ty = 0; ty < map_h; ++ty) {
                for (int tx = 0; tx < map_w; ++tx) {
                    int idx = ty * map_w + tx;
                    if (idx < (int)map.size()) {
                        uint8_t tile_id = map.get(idx);
                        if (tile_size <= 1) {
                            set_pixel(x + tx, y + ty, tile_id);
                        } else {
                            draw_rect(x + tx * tile_size, y + ty * tile_size, tile_size, tile_size, tile_id);
                        }
                    }
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

        void draw_screen_bits(const viss::Bits& bits, int on_color = 7, int off_color = 8) {
            int w = width;
            int h = height;
            if (w * h != (int)bits.size()) {
                int side = (int)std::round(std::sqrt((double)bits.size()));
                if (side * side == (int)bits.size()) {
                    w = side;
                    h = side;
                } else {
                    w = 32;
                    h = (int)(bits.size() + 31) / 32;
                }
                resize(w, h);
            }
            for (int i = 0; i < w * h && i < (int)bits.size(); ++i) {
                pixels[i] = bits.get(i) ? (uint8_t)on_color : (uint8_t)off_color;
                text_overlay[i].has_char = false;
            }
            update();
        }

        void draw_screen_bytes(const viss::Bytes& bytes, int on_color = 7, int off_color = 8) {
            int w = width;
            int h = height;
            if (w * h != (int)bytes.size()) {
                int side = (int)std::round(std::sqrt((double)bytes.size()));
                if (side * side == (int)bytes.size()) {
                    w = side;
                    h = side;
                } else {
                    w = 32;
                    h = (int)(bytes.size() + 31) / 32;
                }
                resize(w, h);
            }
            for (int i = 0; i < w * h && i < (int)bytes.size(); ++i) {
                pixels[i] = bytes.get(i);
                text_overlay[i].has_char = false;
            }
            update();
        }

        void draw_bits(const viss::Bits& bits, int w = 0, int h = 0, int on_color = 7, int off_color = 8, int x = 0, int y = 0) {
            if (w <= 0 || h <= 0) {
                int side = (int)std::round(std::sqrt((double)bits.size()));
                if (side * side == (int)bits.size()) {
                    w = side;
                    h = side;
                } else {
                    w = (int)bits.size();
                    h = 1;
                }
            }
            for (int dy = 0; dy < h; ++dy) {
                for (int dx = 0; dx < w; ++dx) {
                    size_t idx = dy * w + dx;
                    if (idx < bits.size()) {
                        set_pixel(x + dx, y + dy, bits.get(idx) ? (uint8_t)on_color : (uint8_t)off_color);
                    }
                }
            }
        }

        void draw_bit_sprite(const viss::Bits& bits, int x, int y, int w, int h, int on_color = 7, bool transparent_off = true, int off_color = 8) {
            for (int dy = 0; dy < h; ++dy) {
                for (int dx = 0; dx < w; ++dx) {
                    size_t idx = dy * w + dx;
                    if (idx < bits.size()) {
                        bool b = bits.get(idx);
                        if (b) {
                            set_pixel(x + dx, y + dy, (uint8_t)on_color);
                        } else if (!transparent_off) {
                            set_pixel(x + dx, y + dy, (uint8_t)off_color);
                        }
                    }
                }
            }
        }

        void draw_bit_tilemap(const viss::Bits& map, int map_w, int map_h, int tile_size = 4, const std::string& solid_sym = "#", int fg = 6, int bg = 1) {
            for (int ty = 0; ty < map_h; ++ty) {
                for (int tx = 0; tx < map_w; ++tx) {
                    size_t tidx = ty * map_w + tx;
                    if (tidx < map.size() && map.get(tidx)) {
                        draw_block(tx * tile_size, ty * tile_size, tile_size, tile_size, solid_sym, (uint8_t)fg, (uint8_t)bg);
                    }
                }
            }
        }

        void update() {
#ifdef _WIN32
            if (window_mode && hwnd) {
                MSG msg;
                while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) {
                        hwnd = NULL;
                        window_mode = false;
                        return;
                    }
                    TranslateMessage(&msg);
                    DispatchMessageA(&msg);
                }

                if (!hwnd) return;

                dib_pixels.resize(width * height);
                for (size_t i = 0; i < pixels.size() && i < dib_pixels.size(); ++i) {
                    uint8_t raw = pixels[i];
                    RGBColor col = palette[raw];
                    dib_pixels[i] = (col.r << 16) | (col.g << 8) | col.b;
                }

                HDC hdc = GetDC(hwnd);
                if (hdc) {
                    StretchDIBits(
                        hdc,
                        0, 0, width * window_scale, height * window_scale,
                        0, 0, width, height,
                        dib_pixels.data(),
                        &bmi,
                        DIB_RGB_COLORS,
                        SRCCOPY
                    );
                    ReleaseDC(hwnd, hdc);
                }
                return;
            }

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
            if (!ui_elements.empty()) {
                for (const auto& el : ui_elements) {
                    int term_col = el.is_terminal_col ? (el.col + 1) : (compact_mode ? (el.col + 1) : (el.col * 2 + 1));
                    int term_row = el.row + 1;
                    frame += "\033[" + std::to_string(term_row) + ";" + std::to_string(term_col) + "H";
                    if (use_palette) {
                        const auto& fc = palette[el.fg];
                        const auto& bc = palette[el.bg];
                        frame += "\033[38;2;" + std::to_string(fc.r) + ";" + std::to_string(fc.g) + ";" + std::to_string(fc.b) + "m";
                        frame += "\033[48;2;" + std::to_string(bc.r) + ";" + std::to_string(bc.g) + ";" + std::to_string(bc.b) + "m";
                    }
                    frame += el.text;
                }
                frame += "\033[0m";
                ui_elements.clear();
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

    // --- Screen Setup & Configuration (10-byte Hardware Config) ---
    // [0] Width (e.g. 32, 64)
    // [1] Height (e.g. 24, 36)
    // [2] Render Mode (0: Standard block, 1: Compact Half-Block ▀)
    // [3] Palette Preset (0: NES Mario, 1: GameBoy, 2: CGA, 3: Monochrome)
    // [4] Target FPS (30, 60, 120)
    // [5] Flags (Bit 0: cursor, Bit 1: vsync, Bit 2: auto-clear)
    // [6] Default Background Color index
    // [7] Default Foreground Color index
    // [8] Scale (1x, 2x)
    // [9] Audio Volume (0-100)
    namespace screen {
        inline void create(const viss::Bytes& cfg) {
            int w = (cfg.size() >= 1 && cfg[0] > 0) ? cfg[0] : 32;
            int h = (cfg.size() >= 2 && cfg[1] > 0) ? cfg[1] : 24;
            getScreen().resize(w, h);

            if (cfg.size() >= 3) {
                getScreen().set_compact(cfg[2] == 1);
            }
            if (cfg.size() >= 4) {
                uint8_t pal = cfg[3];
                if (pal == 1) { // GameBoy
                    getScreen().set_palette_color(0, 155, 188, 15);
                    getScreen().set_palette_color(1, 139, 172, 15);
                    getScreen().set_palette_color(2, 48, 98, 48);
                    getScreen().set_palette_color(3, 15, 56, 15);
                } else if (pal == 2) { // CGA
                    getScreen().set_palette_color(0, 0, 0, 0);
                    getScreen().set_palette_color(1, 0, 170, 170);
                    getScreen().set_palette_color(2, 170, 0, 170);
                    getScreen().set_palette_color(3, 255, 255, 255);
                } else if (pal == 3) { // Monochrome
                    getScreen().set_palette_color(0, 0, 0, 0);
                    getScreen().set_palette_color(1, 255, 255, 255);
                }
            }
            if (cfg.size() >= 7) {
                getScreen().clear(cfg[6]);
            }
        }

        inline void create(int w, int h, int render_mode = 1, int palette_id = 0, int fps_cap = 60) {
            viss::Bytes cfg(10, 0);
            cfg[0] = (uint8_t)w;
            cfg[1] = (uint8_t)h;
            cfg[2] = (uint8_t)render_mode;
            cfg[3] = (uint8_t)palette_id;
            cfg[4] = (uint8_t)fps_cap;
            cfg[5] = 0x02; // vsync
            cfg[6] = 0;    // default bg
            cfg[7] = 7;    // default fg
            cfg[8] = 1;    // scale
            cfg[9] = 90;   // audio volume
            create(cfg);
        }

        inline void create(std::initializer_list<int> init) {
            viss::Bytes cfg(10, 0);
            size_t idx = 0;
            for (auto v : init) {
                if (idx < 10) cfg[idx++] = (uint8_t)v;
            }
            create(cfg);
        }

        inline viss::Bytes default_config(int w = 32, int h = 24) {
            viss::Bytes cfg(10, 0);
            cfg[0] = (uint8_t)w;
            cfg[1] = (uint8_t)h;
            cfg[2] = 1;
            cfg[3] = 0;
            cfg[4] = 60;
            cfg[5] = 0x02;
            cfg[6] = 0;
            cfg[7] = 7;
            cfg[8] = 1;
            cfg[9] = 90;
            return cfg;
        }

        inline void set(int x, int y, int id) {
            getScreen().set_pixel(x, y, (uint8_t)id);
        }

        inline int get(int x, int y) {
            return (int)getScreen().get_pixel(x, y);
        }

        inline void rect(int x, int y, int w, int h, int id) {
            getScreen().draw_rect(x, y, w, h, (uint8_t)id);
        }

        inline void clear(int id = 0) {
            getScreen().clear((uint8_t)id);
        }

        inline void blit(const viss::Bytes& grid, int gw, int gh, int x = 0, int y = 0) {
            getScreen().draw_tiles(grid, gw, gh, x, y, 1);
        }

        inline void update() {
            getScreen().update();
        }

        inline void print_ui(int col, int row, const std::string& text, int fg_id = 10, int bg_id = 13) {
            getScreen().print_ui(col, row, text, fg_id, bg_id);
        }

        inline void apply(const viss::Bytes& m) {
            getScreen().set_colormask(m);
        }

        inline bool window(const std::string& title = "Viss RetroTech Engine", int scale = 4) {
#ifdef _WIN32
            return getScreen().open_window(title, scale);
#else
            return false;
#endif
        }

        inline bool open_window(const std::string& title = "Viss RetroTech Engine", int scale = 4) {
            return window(title, scale);
        }

        inline bool is_window_open() {
#ifdef _WIN32
            return getScreen().is_window_open();
#else
            return false;
#endif
        }
    }

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

    inline void ColorScreen(const viss::Bytes& colormask) {
        getScreen().color_screen(colormask);
    }

    inline void ColorScreen(const viss::Bytes& colormask, const viss::Bytes& map) {
        getScreen().color_screen(colormask, map);
    }

    inline void SetColorMask(const viss::Bytes& mask) {
        getScreen().set_colormask(mask);
    }

    inline void DrawTiles(const viss::Bytes& map, int map_w, int map_h, int x = 0, int y = 0, int tile_size = 1) {
        getScreen().draw_tiles(map, map_w, map_h, x, y, tile_size);
    }

    inline void DrawTilemap(const viss::Bytes& map, int map_w, int map_h, int x = 0, int y = 0, int tile_size = 1) {
        getScreen().draw_tiles(map, map_w, map_h, x, y, tile_size);
    }

    // --- Screen Bit / Buffer Drawing ---
    inline void DrawScreen(const viss::Bits& bits, Int on_color = 7, Int off_color = 8) {
        getScreen().draw_screen_bits(bits, (int)on_color, (int)off_color);
    }
    inline void draw_screen(const viss::Bits& bits, Int on_color = 7, Int off_color = 8) {
        DrawScreen(bits, on_color, off_color);
    }

    inline void DrawScreen(const viss::Bytes& bytes, Int on_color = 7, Int off_color = 8) {
        getScreen().draw_screen_bytes(bytes, (int)on_color, (int)off_color);
    }
    inline void draw_screen(const viss::Bytes& bytes, Int on_color = 7, Int off_color = 8) {
        DrawScreen(bytes, on_color, off_color);
    }

    inline void DrawBits(const viss::Bits& bits, Int w = 0, Int h = 0, Int on_color = 7, Int off_color = 8, Int x = 0, Int y = 0) {
        getScreen().draw_bits(bits, (int)w, (int)h, (int)on_color, (int)off_color, (int)x, (int)y);
    }
    inline void draw_bits(const viss::Bits& bits, Int w = 0, Int h = 0, Int on_color = 7, Int off_color = 8, Int x = 0, Int y = 0) {
        DrawBits(bits, w, h, on_color, off_color, x, y);
    }

    inline void DrawBitSprite(const viss::Bits& bits, Int x, Int y, Int w, Int h, Int on_color = 7, Bool transparent_off = true, Int off_color = 8) {
        getScreen().draw_bit_sprite(bits, (int)x, (int)y, (int)w, (int)h, (int)on_color, transparent_off, (int)off_color);
    }
    inline void draw_bit_sprite(const viss::Bits& bits, Int x, Int y, Int w, Int h, Int on_color = 7, Bool transparent_off = true, Int off_color = 8) {
        DrawBitSprite(bits, x, y, w, h, on_color, transparent_off, off_color);
    }

    inline void DrawBitTilemap(const viss::Bits& map, Int map_w, Int map_h, Int tile_size = 4, const Str& solid_sym = "#", Int fg = 6, Int bg = 1) {
        getScreen().draw_bit_tilemap(map, (int)map_w, (int)map_h, (int)tile_size, solid_sym, (int)fg, (int)bg);
    }
    inline void draw_bit_tilemap(const viss::Bits& map, Int map_w, Int map_h, Int tile_size = 4, const Str& solid_sym = "#", Int fg = 6, Int bg = 1) {
        DrawBitTilemap(map, map_w, map_h, tile_size, solid_sym, fg, bg);
    }

    inline Bool BitCollides(const viss::Bits& a, Int ax, Int ay, Int aw, Int ah,
                            const viss::Bits& b, Int bx, Int by, Int bw, Int bh) {
        Int x1 = std::max(ax, bx);
        Int y1 = std::max(ay, by);
        Int x2 = std::min(ax + aw, bx + bw);
        Int y2 = std::min(ay + ah, by + bh);

        if (x1 >= x2 || y1 >= y2) return false;

        for (Int y = y1; y < y2; ++y) {
            for (Int x = x1; x < x2; ++x) {
                Int a_idx = (y - ay) * aw + (x - ax);
                Int b_idx = (y - by) * bw + (x - bx);
                if (a.get(a_idx) && b.get(b_idx)) {
                    return true;
                }
            }
        }
        return false;
    }
    inline Bool bit_collides(const viss::Bits& a, Int ax, Int ay, Int aw, Int ah,
                            const viss::Bits& b, Int bx, Int by, Int bw, Int bh) {
        return BitCollides(a, ax, ay, aw, ah, b, bx, by, bw, bh);
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

    // --- Self-Contained 3-Byte Header Sprites ---
    // [0] Width (e.g. 8, 16)
    // [1] Height (e.g. 8, 16)
    // [2] Flags (Bits 0..3: transparent color 0-15; Bit 4: 1-bit monochrome; Bit 5: flip_x; Bit 6: flip_y; Bit 7: opaque)
    // [3..N] Pixel or Bitfield data
    namespace draw {
        inline void sprite(const viss::Bytes& spr, int x, int y) {
            if (spr.size() < 3) return;
            int w = spr[0];
            int h = spr[1];
            uint8_t flags = spr[2];

            int transparent_color = (flags & 0x80) ? -1 : (flags & 0x0F);
            bool is_1bit = (flags & 0x10) != 0;
            bool flip_x = (flags & 0x20) != 0;
            bool flip_y = (flags & 0x40) != 0;

            if (is_1bit) {
                size_t bit_offset = 24; // 3 bytes * 8 bits
                for (int sy = 0; sy < h; ++sy) {
                    for (int sx = 0; sx < w; ++sx) {
                        int src_x = flip_x ? (w - 1 - sx) : sx;
                        int src_y = flip_y ? (h - 1 - sy) : sy;
                        size_t b_idx = bit_offset + src_y * w + src_x;
                        bool bit = spr.get_bit(b_idx / 8, (uint8_t)(b_idx % 8));
                        if (bit) {
                            getScreen().set_pixel(x + sx, y + sy, 7); // white
                        } else if (transparent_color >= 0) {
                            getScreen().set_pixel(x + sx, y + sy, (uint8_t)transparent_color);
                        }
                    }
                }
            } else {
                size_t data_offset = 3;
                for (int sy = 0; sy < h; ++sy) {
                    for (int sx = 0; sx < w; ++sx) {
                        int src_x = flip_x ? (w - 1 - sx) : sx;
                        int src_y = flip_y ? (h - 1 - sy) : sy;
                        size_t p_idx = data_offset + src_y * w + src_x;
                        if (p_idx < spr.size()) {
                            uint8_t col = spr[p_idx];
                            if (transparent_color < 0 || col != (uint8_t)transparent_color) {
                                getScreen().set_pixel(x + sx, y + sy, col);
                            }
                        }
                    }
                }
            }
        }

        inline void sprite(const viss::Bytes& spr, int x, int y, int w, int h, int transparent_color = 0) {
            getScreen().draw_sprite(spr, x, y, w, h, transparent_color);
        }
    }

    namespace mask {
        using namespace viss::bytemask;
        inline void apply(const viss::Bytes& m) {
            getScreen().set_colormask(m);
        }
    }
    namespace bytemask = mask;
    namespace colormask = mask;

    inline struct _ColormaskBridgeInit {
        _ColormaskBridgeInit() {
            retrotech_bridge::apply_colormask_fn = [](const uint8_t* ptr, size_t sz) {
                viss::Bytes b(sz, 0);
                std::memcpy(b.raw(), ptr, sz);
                getScreen().set_colormask(b);
            };
        }
    } _colormask_bridge_init_instance;


    namespace sprite {
        inline viss::Bytes create(int w, int h, int transparent_id = 0) {
            viss::Bytes spr(3 + w * h, 0);
            spr[0] = (uint8_t)w;
            spr[1] = (uint8_t)h;
            spr[2] = (transparent_id < 0) ? 0x80 : (uint8_t)(transparent_id & 0x7F);
            return spr;
        }

        inline void draw(const viss::Bytes& spr, int x, int y, bool flip_x = false) {
            if (spr.size() < 3) return;
            int w = spr[0];
            int h = spr[1];
            uint8_t flags = spr[2];
            int trans = (flags & 0x80) ? -1 : (int)(flags & 0x7F);

            for (int sy = 0; sy < h; ++sy) {
                for (int sx = 0; sx < w; ++sx) {
                    int src_x = flip_x ? (w - 1 - sx) : sx;
                    size_t idx = 3 + sy * w + src_x;
                    if (idx < spr.size()) {
                        uint8_t id = spr[idx];
                        if (trans < 0 || id != (uint8_t)trans) {
                            getScreen().set_pixel(x + sx, y + sy, id);
                        }
                    }
                }
            }
        }

        inline void draw_scaled(const viss::Bytes& spr, int x, int y, int scale = 1, bool flip_x = false) {
            if (spr.size() < 3 || scale <= 0) return;
            if (scale == 1) {
                draw(spr, x, y, flip_x);
                return;
            }
            int w = spr[0];
            int h = spr[1];
            uint8_t flags = spr[2];
            int trans = (flags & 0x80) ? -1 : (int)(flags & 0x7F);

            for (int sy = 0; sy < h; ++sy) {
                for (int sx = 0; sx < w; ++sx) {
                    int src_x = flip_x ? (w - 1 - sx) : sx;
                    size_t idx = 3 + sy * w + src_x;
                    if (idx < spr.size()) {
                        uint8_t id = spr[idx];
                        if (trans < 0 || id != (uint8_t)trans) {
                            getScreen().draw_rect(x + sx * scale, y + sy * scale, scale, scale, id);
                        }
                    }
                }
            }
        }

        inline void set(viss::Bytes& spr, int sx, int sy, int id) {
            if (spr.size() < 3) return;
            int w = spr[0];
            int h = spr[1];
            if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
                size_t idx = 3 + sy * w + sx;
                if (idx < spr.size()) spr[idx] = (uint8_t)id;
            }
        }

        inline int get(const viss::Bytes& spr, int sx, int sy) {
            if (spr.size() < 3) return 0;
            int w = spr[0];
            int h = spr[1];
            if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
                size_t idx = 3 + sy * w + sx;
                if (idx < spr.size()) return (int)spr[idx];
            }
            return 0;
        }

        inline void fill(viss::Bytes& spr, int id) {
            if (spr.size() < 3) return;
            for (size_t i = 3; i < spr.size(); ++i) {
                spr[i] = (uint8_t)id;
            }
        }

        inline void replace_color(viss::Bytes& spr, int old_id, int new_id) {
            if (spr.size() < 3) return;
            for (size_t i = 3; i < spr.size(); ++i) {
                if (spr[i] == (uint8_t)old_id) {
                    spr[i] = (uint8_t)new_id;
                }
            }
        }
    }

    namespace ui {
        inline void text(int col, int row, const std::string& str, int fg_id = 10, int bg_id = 13) {
            getScreen().print_ui(col, row, str, fg_id, bg_id);
        }
    }

    inline viss::Bytes make_sprite(int w, int h, int transparent_color, const viss::Bytes& pixels, bool flip_x = false) {
        viss::Bytes spr(3 + pixels.size(), 0);
        spr[0] = (uint8_t)w;
        spr[1] = (uint8_t)h;
        uint8_t flags = (transparent_color < 0) ? 0x80 : (transparent_color & 0x0F);
        if (flip_x) flags |= 0x20;
        spr[2] = flags;
        for (size_t i = 0; i < pixels.size(); ++i) {
            spr[3 + i] = pixels[i];
        }
        return spr;
    }

    inline void DrawSprite(const viss::Bytes& sprite, int x, int y) {
        draw::sprite(sprite, x, y);
    }
    inline void draw_sprite(const viss::Bytes& sprite, int x, int y) {
        draw::sprite(sprite, x, y);
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

    inline void Tone(Dec freq, Int duration_ms, Int volume) {
        getAudio().tone(freq, (int)duration_ms, "square", 0.5, (int)volume);
    }

    inline void Tone(Dec freq, Int duration_ms, Dec duty) {
        getAudio().tone(freq, (int)duration_ms, "square", duty, 90);
    }

    inline void ToneSync(Dec freq, Int duration_ms, const Str& wave_type = "square", Dec duty = 0.5, Int volume = 90) {
        Tone(freq, duration_ms, wave_type, duty, volume);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
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
            int ch = _getch();
            if (ch == 0 || ch == 224) {
                int ch2 = _getch();
                if (ch2 == 72) return "UP";
                if (ch2 == 80) return "DOWN";
                if (ch2 == 75) return "LEFT";
                if (ch2 == 77) return "RIGHT";
                return Str(1, (char)ch2);
            }
            return Str(1, (char)ch);
            #endif
        }
        return "";
    }

    // --- Snake-Case API Aliases ---
    inline void screen_create(const viss::Bytes& cfg) { screen::create(cfg); }
    inline void screen_create(int w, int h, int render_mode = 1, int palette_id = 0, int fps_cap = 60) { screen::create(w, h, render_mode, palette_id, fps_cap); }
    inline void init_screen(int w = 32, int h = 24) { InitScreen(w, h); }
    inline void set_compact(Bool compact = true) { SetCompact(compact); }
    inline void set_render_mode(const Str& mode) { SetRenderMode(mode); }
    inline void set_pixel_size(const Str& mode) { SetPixelSize(mode); }
    inline void draw_raw_pixels(const viss::Bytes& map, int w = 32, int h = 32) { DrawRawPixels(map, w, h); }
    inline void color_screen(const viss::Bytes& colormask) { ColorScreen(colormask); }
    inline void color_screen(const viss::Bytes& colormask, const viss::Bytes& map) { ColorScreen(colormask, map); }
    inline void set_colormask(const viss::Bytes& colormask) { SetColorMask(colormask); }
    inline void draw_tiles(const viss::Bytes& map, int map_w, int map_h, int x = 0, int y = 0, int tile_size = 1) { DrawTiles(map, map_w, map_h, x, y, tile_size); }
    inline void draw_tilemap(const viss::Bytes& map, int map_w, int map_h, int x = 0, int y = 0, int tile_size = 1) { DrawTilemap(map, map_w, map_h, x, y, tile_size); }
    inline void update_screen() { UpdateScreen(); }
    inline void clear_screen(int color = 0) { ClearScreen(color); }
    inline void draw_pixel(int x, int y, int color) { DrawPixel(x, y, color); }
    inline int get_pixel(int x, int y) { return GetPixel(x, y); }
    inline void draw_rect(int x, int y, int w, int h, int color) { DrawRect(x, y, w, h, color); }
    inline void draw_char(int x, int y, const Str& symbol, int fg, int bg) { DrawChar(x, y, symbol, fg, bg); }
    inline void draw_text(int x, int y, const Str& text, int fg, int bg) { DrawText(x, y, text, fg, bg); }
    inline void draw_block(int x, int y, int w, int h, const Str& symbol, int fg, int bg) { DrawBlock(x, y, w, h, symbol, fg, bg); }
    inline void draw_sprite(const viss::Bytes& sprite, int x, int y, int w, int h, int transparent_color = 0) { DrawSprite(sprite, x, y, w, h, transparent_color); }
    inline void draw_sprite_flipped(const viss::Bytes& sprite, int x, int y, int w, int h, bool flip_x, int transparent_color = 0) { DrawSpriteFlipped(sprite, x, y, w, h, flip_x, transparent_color); }
    inline void set_palette_color(int id, int r, int g, int b) { SetPaletteColor(id, r, g, b); }
    inline void set_camera(int cx, int cy = 0) { SetCamera(cx, cy); }
    inline int get_camera_x() { return GetCameraX(); }
    inline int get_camera_y() { return GetCameraY(); }
    inline void shake(int amount = 2, int frames = 4) { Shake(amount, frames); }
    inline Bool collides(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) { return Collides(x1, y1, w1, h1, x2, y2, w2, h2); }
    inline void draw_tile(const Str& tile_type, int x, int y, int size = 4) { DrawTile(tile_type, x, y, size); }
    inline void beep(Int freq = 800, Int duration_ms = 100) { Beep(freq, duration_ms); }
    inline void tone(Dec freq, Int duration_ms, const Str& wave_type = "square", Dec duty = 0.5, Int volume = 90) { Tone(freq, duration_ms, wave_type, duty, volume); }
    inline void tone(Dec freq, Int duration_ms, Int volume) { Tone(freq, duration_ms, volume); }
    inline void tone(Dec freq, Int duration_ms, Dec duty) { Tone(freq, duration_ms, duty); }
    inline void tone_sync(Dec freq, Int duration_ms, const Str& wave_type = "square", Dec duty = 0.5, Int volume = 90) { ToneSync(freq, duration_ms, wave_type, duty, volume); }
    inline void sweep(Dec start_freq, Dec end_freq, Int duration_ms, const Str& wave_type = "square", Dec duty = 0.5) { Sweep(start_freq, end_freq, duration_ms, wave_type, duty); }
    inline void play_sfx(const Str& name) { PlaySfx(name); }
    inline void sound_jump() { SoundJump(); }
    inline void sound_coin() { SoundCoin(); }
    inline void sound_stomp() { SoundStomp(); }
    inline void sound_powerup() { SoundPowerup(); }
    inline void sound_hurt() { SoundHurt(); }
    inline void sound_bump() { SoundBump(); }
    inline void sound_flag() { SoundFlag(); }
    inline void sound_game_over() { SoundGameOver(); }
    inline void play_music(const Str& melody, Int bpm = 120) { PlayMusic(melody, bpm); }
    inline void stop_music() { StopMusic(); }
    inline Bool has_key() { return HasKey(); }
    inline Str get_key() { return GetKey(); }
}
}
