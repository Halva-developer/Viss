#pragma once
#include "../vissrt.hpp"
#include <fstream>
#include <cstdio>
#include <filesystem>
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
        inline Int size(const Str& path) {
            std::ifstream f(path, std::ifstream::ate | std::ifstream::binary);
            if (f.is_open()) {
                return (Int)f.tellg();
            }
            return 0;
        }
        inline Bytes read_bytes(const Str& path) {
            std::ifstream f(path, std::ios::binary | std::ios::ate);
            if (!f.is_open()) return Bytes(0);
            std::streamsize sz = f.tellg();
            f.seekg(0, std::ios::beg);
            Bytes b((size_t)sz);
            if (sz > 0) {
                f.read(reinterpret_cast<char*>(b.raw()), sz);
            }
            return b;
        }
        inline void write_bytes(const Str& path, const Bytes& b) {
            std::ofstream f(path, std::ios::binary);
            if (f.is_open() && b.size() > 0) {
                f.write(reinterpret_cast<const char*>(b.raw()), b.size());
            }
        }
        inline List<Str> list_dir(const Str& path) {
            List<Str> files;
            try {
                for (const auto& entry : std::filesystem::directory_iterator(path)) {
                    files.add(entry.path().string());
                }
            } catch (...) {}
            return files;
        }
    }
}
