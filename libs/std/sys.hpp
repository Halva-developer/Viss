#pragma once
#include "../vissrt.hpp"
#include <chrono>
#include <cstdlib>

namespace viss {
    namespace sys {
        inline void seed() {
            srand((unsigned int)std::chrono::system_clock::now().time_since_epoch().count());
        }
        inline Int random(Int min, Int max) {
            if (max <= min) return min;
            return min + (rand() % (max - min));
        }
        inline void command(const Str& cmd) {
            std::system(cmd.c_str());
        }
    }
}
