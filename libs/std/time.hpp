#pragma once
#include "../vissrt.hpp"
#include <chrono>
#include <thread>

namespace viss {
    namespace time {
        inline Int now() {
            return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }
        inline Int ms() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }
        inline void sleep(Int milliseconds) {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }
    }
}
