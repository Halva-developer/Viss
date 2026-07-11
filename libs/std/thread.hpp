#pragma once
#include "../vissrt.hpp"
#include <thread>

namespace viss {
    namespace thread {
        inline void sleep(Int ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }

        template<typename F>
        inline void run(F&& func) {
            std::thread t(std::forward<F>(func));
            t.detach();
        }
    }
}
