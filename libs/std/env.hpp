#pragma once
#include "../vissrt.hpp"
#include <cstdlib>
#include <vector>
#include <array>
#include <memory>
#include <thread>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

namespace viss {
    namespace env {
        inline Str get(const Str& key, const Str& default_val = "") {
            const char* v = std::getenv(key.c_str());
            return v ? Str(v) : default_val;
        }

        inline void set(const Str& key, const Str& val) {
            #ifdef _WIN32
            _putenv_s(key.c_str(), val.c_str());
            #else
            setenv(key.c_str(), val.c_str(), 1);
            #endif
        }

        inline Bool has(const Str& key) {
            return std::getenv(key.c_str()) != nullptr;
        }

        inline Str cwd() {
            try {
                return std::filesystem::current_path().string();
            } catch (...) {
                return "";
            }
        }

        inline void set_cwd(const Str& path) {
            try {
                std::filesystem::current_path(path);
            } catch (...) {}
        }

        inline Str os() {
            #ifdef _WIN32
            return "windows";
            #elif __APPLE__
            return "darwin";
            #else
            return "linux";
            #endif
        }

        inline Str arch() {
            #if defined(__x86_64__) || defined(_M_X64)
            return "x64";
            #elif defined(__aarch64__) || defined(_M_ARM64)
            return "arm64";
            #elif defined(__i386__) || defined(_M_IX86)
            return "x86";
            #elif defined(__arm__) || defined(_M_ARM)
            return "arm";
            #else
            return "unknown";
            #endif
        }

        inline Int cpu_count() {
            unsigned int c = std::thread::hardware_concurrency();
            return c > 0 ? (Int)c : 1;
        }

        inline Str exec(const Str& cmd) {
            std::array<char, 128> buffer;
            Str result;
            #ifdef _WIN32
            std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
            #else
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
            #endif
            if (!pipe) return "";
            while (fgets(buffer.data(), (int)buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
            return result;
        }

        inline Int run(const Str& cmd) {
            return (Int)std::system(cmd.c_str());
        }

        inline void exit(Int code = 0) {
            std::exit((int)code);
        }

        inline List<Str> args() {
            List<Str> arg_list;
            #ifdef _WIN32
            int numArgs = 0;
            LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &numArgs);
            if (argvW) {
                for (int i = 0; i < numArgs; ++i) {
                    int size_needed = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
                    std::string str(size_needed - 1, 0);
                    WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, &str[0], size_needed, NULL, NULL);
                    arg_list.add(str);
                }
                LocalFree(argvW);
            }
            #endif
            return arg_list;
        }
    }
}
