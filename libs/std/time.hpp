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

        inline Int now_us() {
            return std::chrono::duration_cast<std::chrono::microseconds>(
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
        inline void sleep_ms(Int ms) {
            sleep(ms);
        }
        inline void sleep_sec(double s) {
            sleep((Int)(s * 1000.0));
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

        class Stopwatch {
        private:
            std::chrono::high_resolution_clock::time_point start_time;
            std::chrono::high_resolution_clock::time_point stop_time;
            bool running = false;
        public:
            Stopwatch(bool autostart = true) {
                if (autostart) start();
            }

            inline void start() {
                start_time = std::chrono::high_resolution_clock::now();
                running = true;
            }

            inline void stop() {
                if (running) {
                    stop_time = std::chrono::high_resolution_clock::now();
                    running = false;
                }
            }

            inline void reset() {
                running = false;
            }

            inline Int restart() {
                Int ms = elapsed_ms();
                start();
                return ms;
            }

            inline Int elapsed_ms() const {
                auto end = running ? std::chrono::high_resolution_clock::now() : stop_time;
                return std::chrono::duration_cast<std::chrono::milliseconds>(end - start_time).count();
            }

            inline Int elapsed_us() const {
                auto end = running ? std::chrono::high_resolution_clock::now() : stop_time;
                return std::chrono::duration_cast<std::chrono::microseconds>(end - start_time).count();
            }

            inline Dec elapsed_seconds() const {
                auto end = running ? std::chrono::high_resolution_clock::now() : stop_time;
                return std::chrono::duration<Dec>(end - start_time).count();
            }

            inline Bool is_running() const {
                return running;
            }
        };
    }
}
