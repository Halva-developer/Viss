#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <cmath>
#include <chrono>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <exception>
#include <memory>
#include <future>
#include <queue>
#include <condition_variable>

namespace viss {
    using Str = std::string;
    using Int = long long;
    using Dec = double;
    using Bool = bool;

    class ThreadPool {
    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queueMutex;
        std::condition_variable cv;
        bool stop = false;
    public:
        ThreadPool(size_t threads = std::thread::hardware_concurrency()) {
            if (threads == 0) threads = 2;
            for (size_t i = 0; i < threads; ++i) {
                workers.emplace_back([this]() {
                    while (true) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->queueMutex);
                            this->cv.wait(lock, [this]() {
                                return this->stop || !this->tasks.empty();
                            });
                            if (this->stop && this->tasks.empty()) return;
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        task();
                    }
                });
            }
        }

        template<typename F, typename... Args>
        auto enqueue(F&& f, Args&&... args) 
            -> std::future<typename std::invoke_result<F, Args...>::type> {
            using return_type = typename std::invoke_result<F, Args...>::type;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );
            
            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");
                tasks.emplace([task]() { (*task)(); });
            }
            cv.notify_one();
            return res;
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                stop = true;
            }
            cv.notify_all();
            for (std::thread &worker: workers) {
                if (worker.joinable()) worker.join();
            }
        }
    };

    inline ThreadPool& getGlobalThreadPool() {
        static ThreadPool pool;
        return pool;
    }

    struct Point {
        Int x = 0;
        Int y = 0;
    };

    class Error : public std::exception {
    private:
        Str message;
    public:
        Error(const Str& msg) : message(msg) {}
        virtual const char* what() const noexcept override {
            return message.c_str();
        }
    };

    template<typename T>
    class List {
    private:
        std::shared_ptr<std::vector<T>> data;
        std::shared_ptr<std::mutex> mtx;
    public:
        List() : data(std::make_shared<std::vector<T>>()), mtx(std::make_shared<std::mutex>()) {}
        List(std::initializer_list<T> init) : data(std::make_shared<std::vector<T>>(init)), mtx(std::make_shared<std::mutex>()) {}
        
        inline void add(const T& item) {
            std::lock_guard<std::mutex> lock(*mtx);
            data->push_back(item);
        }
        inline void insert(Int index, const T& item) {
            std::lock_guard<std::mutex> lock(*mtx);
            if (index >= 0 && index <= (Int)data->size()) {
                data->insert(data->begin() + index, item);
            }
        }
        inline void removeAt(Int index) {
            std::lock_guard<std::mutex> lock(*mtx);
            if (index >= 0 && index < (Int)data->size()) {
                data->erase(data->begin() + index);
            }
        }
        inline void removeLast() {
            std::lock_guard<std::mutex> lock(*mtx);
            if (!data->empty()) {
                data->pop_back();
            }
        }
        inline T get(Int index) const {
            std::lock_guard<std::mutex> lock(*mtx);
            if (index >= 0 && index < (Int)data->size()) {
                return (*data)[index];
            }
            return T();
        }
        inline Int size() const {
            std::lock_guard<std::mutex> lock(*mtx);
            return (Int)data->size();
        }
        inline void clear() {
            std::lock_guard<std::mutex> lock(*mtx);
            data->clear();
        }

        // Iterator support
        inline auto begin() { return data->begin(); }
        inline auto end() { return data->end(); }
        inline auto begin() const { return data->begin(); }
        inline auto end() const { return data->end(); }
    };

    template<typename K, typename V>
    class Map {
    private:
        std::shared_ptr<std::unordered_map<K, V>> data;
        std::shared_ptr<std::mutex> mtx;
    public:
        Map() : data(std::make_shared<std::unordered_map<K, V>>()), mtx(std::make_shared<std::mutex>()) {}
        
        inline void set(const K& key, const V& val) {
            std::lock_guard<std::mutex> lock(*mtx);
            (*data)[key] = val;
        }
        inline V get(const K& key) const {
            std::lock_guard<std::mutex> lock(*mtx);
            auto it = data->find(key);
            if (it != data->end()) {
                return it->second;
            }
            return V();
        }
        inline Bool has(const K& key) const {
            std::lock_guard<std::mutex> lock(*mtx);
            return data->find(key) != data->end();
        }
        inline void remove(const K& key) {
            std::lock_guard<std::mutex> lock(*mtx);
            data->erase(key);
        }
        inline Int size() const {
            std::lock_guard<std::mutex> lock(*mtx);
            return (Int)data->size();
        }
        inline void clear() {
            std::lock_guard<std::mutex> lock(*mtx);
            data->clear();
        }
        inline List<K> keys() const {
            std::lock_guard<std::mutex> lock(*mtx);
            List<K> kList;
            for (const auto& pair : *data) {
                kList.add(pair.first);
            }
            return kList;
        }
    };

    inline Int toInt(const Str& s) {
        try {
            return std::stoll(s);
        } catch (...) {
            return 0;
        }
    }
    inline Dec toDec(const Str& s) {
        try {
            return std::stod(s);
        } catch (...) {
            return 0.0;
        }
    }
    template<typename T>
    inline Str toStr(const T& val) {
        return std::to_string(val);
    }
    inline Str toStr(const Str& val) {
        return val;
    }
}

// Automatically include lightweight standard library modules
#include "std/sys.hpp"
#include "std/io.hpp"
#include "std/fs.hpp"
#include "std/math.hpp"
#include "std/time.hpp"
#include "std/str.hpp"
#include "std/thread.hpp"
