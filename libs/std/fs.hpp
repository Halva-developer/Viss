#pragma once
#include "../vissrt.hpp"
#include <fstream>
#include <cstdio>
#include <filesystem>

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
            return std::filesystem::exists(path);
        }

        inline Bool is_file(const Str& path) {
            return std::filesystem::is_regular_file(path);
        }

        inline Bool is_dir(const Str& path) {
            return std::filesystem::is_directory(path);
        }

        inline void append(const Str& path, const Str& content) {
            std::ofstream f(path, std::ios::app);
            if (f.is_open()) {
                f << content;
            }
        }

        inline void remove(const Str& path) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }

        inline void remove_all(const Str& path) {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        inline void mkdir(const Str& path) {
            std::error_code ec;
            std::filesystem::create_directory(path, ec);
        }

        inline void mkdir_p(const Str& path) {
            std::error_code ec;
            std::filesystem::create_directories(path, ec);
        }

        inline void copy(const Str& src, const Str& dst) {
            std::error_code ec;
            std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
        }

        inline void move(const Str& src, const Str& dst) {
            std::error_code ec;
            std::filesystem::rename(src, dst, ec);
        }

        inline Int size(const Str& path) {
            std::error_code ec;
            auto sz = std::filesystem::file_size(path, ec);
            if (ec) return 0;
            return (Int)sz;
        }

        inline Str extension(const Str& path) {
            return std::filesystem::path(path).extension().string();
        }

        inline Str filename(const Str& path) {
            return std::filesystem::path(path).filename().string();
        }

        inline Str stem(const Str& path) {
            return std::filesystem::path(path).stem().string();
        }

        inline Str parent(const Str& path) {
            return std::filesystem::path(path).parent_path().string();
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

        inline List<Str> list(const Str& path) {
            return list_dir(path);
        }

        inline List<Str> list_recursive(const Str& path) {
            List<Str> files;
            try {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    files.add(entry.path().string());
                }
            } catch (...) {}
            return files;
        }
    }
}
