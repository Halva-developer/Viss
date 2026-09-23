#pragma once
#include "../vissrt.hpp"
#include <chrono>
#include <thread>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace viss {
    namespace time {
        inline Int now_ms() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }
        inline Int now_secs() {
            return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }
        inline void sleep(Int ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        inline Str format_now(const Str& fmt = "%Y-%m-%d %H:%M:%S") {
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            #ifdef _WIN32
            struct tm buf;
            localtime_s(&buf, &in_time_t);
            ss << std::put_time(&buf, fmt.c_str());
            #else
            struct tm* buf = std::localtime(&in_time_t);
            ss << std::put_time(buf, fmt.c_str());
            #endif
            return ss.str();
        }
    }
}
