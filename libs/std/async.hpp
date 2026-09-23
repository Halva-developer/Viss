#pragma once
#include "../vissrt.hpp"
#include <future>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <functional>
#include <memory>

namespace viss {
    namespace async {

        // --- Task<T> wrapper ---
        template<typename T>
        class Task {
        private:
            std::shared_ptr<std::future<T>> fut;
        public:
            Task() : fut(nullptr) {}
            Task(std::future<T>&& f) : fut(std::make_shared<std::future<T>>(std::move(f))) {}

            inline Bool is_ready() const {
                if (!fut || !fut->valid()) return false;
                return fut->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
            }
            inline Bool is_done() const { return is_ready(); }

            inline void wait() const {
                if (fut && fut->valid()) fut->wait();
            }

            inline T get() {
                if (!fut || !fut->valid()) throw Error("Task is empty or already consumed");
                return fut->get();
            }

            inline Bool valid() const {
                return fut && fut->valid();
            }
        };

        // Specialization for void
        template<>
        class Task<void> {
        private:
            std::shared_ptr<std::future<void>> fut;
        public:
            Task() : fut(nullptr) {}
            Task(std::future<void>&& f) : fut(std::make_shared<std::future<void>>(std::move(f))) {}

            inline Bool is_ready() const {
                if (!fut || !fut->valid()) return false;
                return fut->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
            }
            inline Bool is_done() const { return is_ready(); }

            inline void wait() const {
                if (fut && fut->valid()) fut->wait();
            }

            inline void get() {
                if (!fut || !fut->valid()) throw Error("Task is empty or already consumed");
                fut->get();
            }

            inline Bool valid() const {
                return fut && fut->valid();
            }
        };

        // --- Spawn & Await ---
        template<typename F, typename... Args>
        inline auto spawn(F&& f, Args&&... args) {
            using RetType = std::invoke_result_t<F, Args...>;
            auto task_future = getGlobalThreadPool().enqueue(std::forward<F>(f), std::forward<Args>(args)...);
            return Task<RetType>(std::move(task_future));
        }

        template<typename T>
        inline T await(Task<T>& task) {
            return task.get();
        }

        inline void await(Task<void>& task) {
            task.get();
        }

        inline void sleep(Int ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }

        // --- Channel<T> (Thread-Safe MPSC Queue) ---
        template<typename T>
        class Channel {
        private:
            mutable std::mutex mtx;
            std::queue<T> queue;
            std::condition_variable cv;
            std::atomic<bool> closed{false};
        public:
            Channel() = default;

            inline void send(const T& value) {
                if (closed.load()) throw Error("Cannot send to a closed Channel");
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    queue.push(value);
                }
                cv.notify_one();
            }

            inline T recv() {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]() {
                    return !queue.empty() || closed.load();
                });
                if (queue.empty() && closed.load()) {
                    throw Error("Channel closed and empty");
                }
                T val = queue.front();
                queue.pop();
                return val;
            }

            inline Bool try_recv(T& out_val) {
                std::lock_guard<std::mutex> lock(mtx);
                if (queue.empty()) return false;
                out_val = queue.front();
                queue.pop();
                return true;
            }

            inline void close() {
                closed.store(true);
                cv.notify_all();
            }

            inline Bool is_closed() const {
                return closed.load();
            }

            inline Int size() const {
                std::lock_guard<std::mutex> lock(mtx);
                return (Int)queue.size();
            }

            inline Bool is_empty() const {
                std::lock_guard<std::mutex> lock(mtx);
                return queue.empty();
            }
        };

        // --- Mutex Wrapper ---
        class Mutex {
        private:
            std::mutex mtx;
        public:
            inline void lock() { mtx.lock(); }
            inline void unlock() { mtx.unlock(); }
            inline Bool try_lock() { return mtx.try_lock(); }
        };

        // --- Timer / Delayed Execution ---
        class TimerHandle {
        private:
            std::shared_ptr<std::atomic<bool>> active;
        public:
            TimerHandle(std::shared_ptr<std::atomic<bool>> a) : active(a) {}
            inline void stop() {
                if (active) active->store(false);
            }
            inline Bool is_running() const {
                return active ? active->load() : false;
            }
        };

        template<typename F>
        inline void delay(Int ms, F&& func) {
            std::thread([ms, f = std::forward<F>(func)]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                f();
            }).detach();
        }

        template<typename F>
        inline TimerHandle interval(Int ms, F&& func) {
            auto active = std::make_shared<std::atomic<bool>>(true);
            std::thread([ms, active, f = std::forward<F>(func)]() {
                while (active->load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                    if (active->load()) {
                        f();
                    }
                }
            }).detach();
            return TimerHandle(active);
        }

        // --- Parallel For ---
        template<typename F>
        inline void parallel_for(Int start, Int end, F&& func) {
            if (start >= end) return;
            Int count = end - start;
            Int threads = (Int)std::thread::hardware_concurrency();
            if (threads <= 1 || count < 4) {
                for (Int i = start; i < end; ++i) func(i);
                return;
            }
            if (threads > count) threads = count;

            Int chunk_size = (count + threads - 1) / threads;
            std::vector<std::future<void>> futures;

            for (Int t = 0; t < threads; ++t) {
                Int c_start = start + t * chunk_size;
                Int c_end = std::min(end, c_start + chunk_size);
                if (c_start >= end) break;
                futures.push_back(getGlobalThreadPool().enqueue([c_start, c_end, &func]() {
                    for (Int i = c_start; i < c_end; ++i) {
                        func(i);
                    }
                }));
            }

            for (auto& f : futures) {
                f.get();
            }
        }
    }
}
