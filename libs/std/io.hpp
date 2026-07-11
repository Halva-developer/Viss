#pragma once
#include "../vissrt.hpp"
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace viss {
    namespace io {
        inline void println(const Str& value) { std::cout << value << "\n"; }
        inline void println(Int value) { std::cout << value << "\n"; }
        inline void println(Dec value) { std::cout << value << "\n"; }
        inline void println(Bool value) { std::cout << (value ? "true" : "false") << "\n"; }

        inline void print(const Str& value) { std::cout << value; }
        inline void print(Int value) { std::cout << value; }
        inline void print(Dec value) { std::cout << value; }
        inline void print(Bool value) { std::cout << (value ? "true" : "false"); }

        inline void eprint(const Str& value) { std::cerr << value; }
        inline void eprintln(const Str& value) { std::cerr << value << "\n"; }

        inline Str readln() {
            Str s;
            std::getline(std::cin, s);
            return s;
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
