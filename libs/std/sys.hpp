#pragma once
#include "../vissrt.hpp"
#include <chrono>
#include <cstdlib>
#include <thread>
#include <sstream>
#include <array>
#include <memory>

namespace viss {
    namespace sys {
        inline void seed() {
            srand((unsigned int)std::chrono::system_clock::now().time_since_epoch().count());
        }
        inline Int random(Int min_v, Int max_v) {
            if (max_v <= min_v) return min_v;
            return min_v + (rand() % (max_v - min_v));
        }
        inline void command(const Str& cmd) {
            std::system(cmd.c_str());
        }
        inline Str env(const Str& key) {
            const char* val = std::getenv(key.c_str());
            return val ? Str(val) : "";
        }
        inline void exit(Int code = 0) {
            std::exit((int)code);
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
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
            return result;
        }
        inline void sleep(Int ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
    }
}
