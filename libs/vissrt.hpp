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
#include <any>

namespace viss {
    using Str = std::string;
    using Int = long long;
    using Dec = double;
    using Bool = bool;
    
    // Viss 2.0 type aliases
    using str = Str;
    using int_t = Int;
    using dec = Dec;
    using bool_t = Bool;
    using any_t = std::any;

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
    using Exception = Error;

    template<typename T = std::string>
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
        inline void append(const T& item) {
            add(item);
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
        inline Int get_len() const {
            return size();
        }
        __declspec(property(get = get_len)) Int len;

        inline T get_first() const {
            std::lock_guard<std::mutex> lock(*mtx);
            if (!data->empty()) return (*data).front();
            return T();
        }
        __declspec(property(get = get_first)) T first;

        inline T get_last() const {
            std::lock_guard<std::mutex> lock(*mtx);
            if (!data->empty()) return (*data).back();
            return T();
        }
        __declspec(property(get = get_last)) T last;

        inline void clear() {
            std::lock_guard<std::mutex> lock(*mtx);
            data->clear();
        }

        inline T& operator[](Int index) {
            std::lock_guard<std::mutex> lock(*mtx);
            return (*data)[index];
        }

        inline const T& operator[](Int index) const {
            std::lock_guard<std::mutex> lock(*mtx);
            return (*data)[index];
        }

        // Iterator support
        inline auto begin() { return data->begin(); }
        inline auto end() { return data->end(); }
        inline auto begin() const { return data->begin(); }
        inline auto end() const { return data->end(); }
    };

    template<typename K = Str, typename V = Str>
    class Map {
    private:
        std::shared_ptr<std::unordered_map<K, V>> data;
        std::shared_ptr<std::mutex> mtx;
    public:
        Map() : data(std::make_shared<std::unordered_map<K, V>>()), mtx(std::make_shared<std::mutex>()) {}
        Map(std::initializer_list<std::pair<K, V>> init) : data(std::make_shared<std::unordered_map<K, V>>()), mtx(std::make_shared<std::mutex>()) {
            for (const auto& p : init) (*data)[p.first] = p.second;
        }
        
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
        inline Int get_len() const {
            return size();
        }
        __declspec(property(get = get_len)) Int len;

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

        inline V& operator[](const K& key) {
            std::lock_guard<std::mutex> lock(*mtx);
            return (*data)[key];
        }
    };

    // Inf: Dynamic Table / Expando Bag for infinite variables
    class Inf {
    private:
        std::shared_ptr<std::unordered_map<Str, Str>> data;
        std::shared_ptr<std::mutex> mtx;
    public:
        Inf() : data(std::make_shared<std::unordered_map<Str, Str>>()), mtx(std::make_shared<std::mutex>()) {}
        Inf(std::initializer_list<std::pair<Str, Str>> init) : data(std::make_shared<std::unordered_map<Str, Str>>()), mtx(std::make_shared<std::mutex>()) {
            for (const auto& p : init) (*data)[p.first] = p.second;
        }

        inline void write(const Str& key, const Str& val) {
            std::lock_guard<std::mutex> lock(*mtx);
            (*data)[key] = val;
        }
        inline Str read(const Str& key) const {
            std::lock_guard<std::mutex> lock(*mtx);
            auto it = data->find(key);
            if (it != data->end()) return it->second;
            return "";
        }
        inline Bool has(const Str& key) const {
            std::lock_guard<std::mutex> lock(*mtx);
            return data->find(key) != data->end();
        }
        inline List<Str> keys() const {
            std::lock_guard<std::mutex> lock(*mtx);
            List<Str> k;
            for (const auto& p : *data) k.add(p.first);
            return k;
        }
        inline Str& operator[](const Str& key) {
            std::lock_guard<std::mutex> lock(*mtx);
            return (*data)[key];
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
    inline Str toStr(const char* val) {
        return Str(val);
    }

    template<typename T>
    inline std::ostream& operator<<(std::ostream& os, const List<T>& list) {
        os << "[";
        for (Int i = 0; i < list.size(); ++i) {
            os << list.get(i);
            if (i + 1 < list.size()) os << ", ";
        }
        os << "]";
        return os;
    }

    template<typename K, typename V>
    inline std::ostream& operator<<(std::ostream& os, const Map<K, V>& map) {
        os << "{";
        auto keys = map.keys();
        for (Int i = 0; i < keys.size(); ++i) {
            auto k = keys.get(i);
            os << k << ": " << map.get(k);
            if (i + 1 < keys.size()) os << ", ";
        }
        os << "}";
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const Inf& inf) {
        os << "{";
        auto keys = inf.keys();
        for (Int i = 0; i < keys.size(); ++i) {
            auto k = keys.get(i);
            os << k << ": " << inf.read(k);
            if (i + 1 < keys.size()) os << ", ";
        }
        os << "}";
        return os;
    }

    template<typename T>
    inline Str toStr(const List<T>& list) {
        std::stringstream ss;
        ss << list;
        return ss.str();
    }
    template<typename K, typename V>
    inline Str toStr(const Map<K, V>& map) {
        std::stringstream ss;
        ss << map;
        return ss.str();
    }
    inline Str toStr(const Inf& inf) {
        std::stringstream ss;
        ss << inf;
        return ss.str();
    }

    template<typename T>
    List(std::initializer_list<T>) -> List<T>;
    List(std::initializer_list<const char*>) -> List<Str>;

    namespace async {
        inline void sleep(Int milliseconds) {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }
    }
    namespace asyncIO = async;
}

// Automatically include lightweight standard library modules
#include "std/sys.hpp"
#include "std/io.hpp"
#include "std/fs.hpp"
#include "std/math.hpp"
#include "std/time.hpp"
#include "std/str.hpp"
#include "std/thread.hpp"
