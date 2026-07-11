#pragma once
#include "../vissrt.hpp"
#include <fstream>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace viss {
    namespace fs {
        inline void write(const Str& path, const Str& content) {
            std::ofstream f(path);
            if (f.is_open()) {
                f << content;
            }
        }
        inline Str read(const Str& path) {
            std::ifstream f(path);
            if (!f.is_open()) return "";
            Str content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            return content;
        }
        inline Bool exists(const Str& path) {
            std::ifstream f(path);
            return f.good();
        }
        inline void append(const Str& path, const Str& content) {
            std::ofstream f(path, std::ios::app);
            if (f.is_open()) {
                f << content;
            }
        }
        inline void remove(const Str& path) {
            std::remove(path.c_str());
        }
        inline void mkdir(const Str& path) {
            #ifdef _WIN32
            CreateDirectoryA(path.c_str(), NULL);
            #else
            ::mkdir(path.c_str(), 0777);
            #endif
        }
    }
}
