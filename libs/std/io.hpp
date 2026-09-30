#pragma once
#include "../vissrt.hpp"
#include <iostream>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace viss {
    namespace io {
        template<typename T>
        inline void print_single(const T& value) {
            std::cout << value;
        }
        inline void print_single(uint8_t value) {
            std::cout << (int)value;
        }
        inline void print_single(int8_t value) {
            std::cout << (int)value;
        }
        inline void print_single(Bool value) {
            std::cout << (value ? "true" : "false");
        }
        inline void print_single(const std::exception& e) {
            std::cout << e.what();
        }

        inline void println() { std::cout << "\n"; }

        template<typename First, typename... Rest>
        inline void println(const First& first, const Rest&... rest) {
            print_single(first);
            if constexpr (sizeof...(rest) > 0) {
                std::cout << " ";
                println(rest...);
            } else {
                std::cout << "\n";
            }
        }

        template<typename First, typename... Rest>
        inline void print(const First& first, const Rest&... rest) {
            print_single(first);
            if constexpr (sizeof...(rest) > 0) {
                std::cout << " ";
                print(rest...);
            }
        }

        inline void eprint(const Str& value) { std::cerr << value; }
        inline void eprintln(const Str& value) { std::cerr << value << "\n"; }

        inline Str readln() {
            Str s;
            std::getline(std::cin, s);
            return s;
        }

        inline Str read_char() {
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
            #else
            struct termios oldt, newt;
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            char ch = getchar();
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return Str(1, ch);
            #endif
        }

        inline Bool has_key() {
            #ifdef _WIN32
            return _kbhit() != 0;
            #else
            return false;
            #endif
        }
        inline Bool has_key_q() { return has_key(); }

        inline Str get_key() {
            if (has_key()) {
                return read_char();
            }
            return "";
        }

        inline Bool is_key_down(int vkey) {
            #ifdef _WIN32
            return (GetAsyncKeyState(vkey) & 0x8000) != 0;
            #else
            return false;
            #endif
        }

        inline Bool is_down(const Str& key_name) {
            #ifdef _WIN32
            if (key_name == "LEFT" || key_name == "a" || key_name == "A") return is_key_down(VK_LEFT) || is_key_down('A');
            if (key_name == "RIGHT" || key_name == "d" || key_name == "D") return is_key_down(VK_RIGHT) || is_key_down('D');
            if (key_name == "UP" || key_name == "w" || key_name == "W") return is_key_down(VK_UP) || is_key_down('W');
            if (key_name == "DOWN" || key_name == "s" || key_name == "S") return is_key_down(VK_DOWN) || is_key_down('S');
            if (key_name == "SPACE" || key_name == " ") return is_key_down(VK_SPACE);
            if (key_name == "ENTER" || key_name == "\n") return is_key_down(VK_RETURN);
            if (key_name == "ESCAPE" || key_name == "ESC") return is_key_down(VK_ESCAPE);
            if (key_name.size() == 1) {
                char c = (char)toupper((unsigned char)key_name[0]);
                return is_key_down(c);
            }
            #endif
            return false;
        }
        inline Bool is_down_q(const Str& key_name) { return is_down(key_name); }

        inline void write_file(const Str& path, const Str& text) {
            std::ofstream f(path);
            if (f.is_open()) f << text;
        }

        inline Str read_file(const Str& path) {
            std::ifstream f(path);
            if (!f.is_open()) return "";
            return Str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        }

        inline void color(Int colorCode) {
            #ifdef _WIN32
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)colorCode);
            #else
            int ansiCode = 37;
            switch (colorCode) {
                case 0: ansiCode = 30; break;
                case 1: ansiCode = 34; break;
                case 2: ansiCode = 32; break;
                case 3: ansiCode = 36; break;
                case 4: ansiCode = 31; break;
                case 5: ansiCode = 35; break;
                case 6: ansiCode = 33; break;
                case 7: ansiCode = 37; break;
                case 8: ansiCode = 90; break;
                case 9: ansiCode = 94; break;
                case 10: ansiCode = 92; break;
                case 11: ansiCode = 96; break;
                case 12: ansiCode = 91; break;
                case 13: ansiCode = 95; break;
                case 14: ansiCode = 93; break;
                case 15: ansiCode = 97; break;
            }
            std::cout << "\033[" << ansiCode << "m";
            #endif
        }

        inline void clear() {
            #ifdef _WIN32
            COORD topLeft  = { 0, 0 };
            HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO screen;
            DWORD written;
            GetConsoleScreenBufferInfo(console, &screen);
            FillConsoleOutputCharacterA(console, ' ', screen.dwSize.X * screen.dwSize.Y, topLeft, &written);
            FillConsoleOutputAttribute(console, screen.wAttributes, screen.dwSize.X * screen.dwSize.Y, topLeft, &written);
            SetConsoleCursorPosition(console, topLeft);
            #else
            std::cout << "\033[2J\033[1;1H";
            #endif
        }

        inline void cursor(Int x, Int y) {
            #ifdef _WIN32
            COORD pos = { (SHORT)x, (SHORT)y };
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
            #else
            std::cout << "\033[" << (y + 1) << ";" << (x + 1) << "H";
            #endif
        }
    }
}
