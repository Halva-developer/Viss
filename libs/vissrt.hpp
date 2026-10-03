#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#endif
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
#include <cstdint>
#include <iomanip>
#include <functional>
#include <cstring>
#include <cstdio>
#include <random>

namespace viss {
    using Str = std::string;
    using Int = long long;
    using Dec = double;
    using Bool = bool;
    
    // Viss 2.0 type aliases
    using int_t = Int;
    using dec_t = Dec;
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

    struct Vec2 {
        Dec x = 0.0;
        Dec y = 0.0;

        Vec2() = default;
        Vec2(Dec _x, Dec _y) : x(_x), y(_y) {}

        inline Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
        inline Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
        inline Vec2 operator*(Dec s) const { return Vec2(x * s, y * s); }
        inline Vec2 operator/(Dec s) const { return Vec2(x / s, y / s); }
        inline Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
        inline Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
        inline Vec2& operator*=(Dec s) { x *= s; y *= s; return *this; }
        inline Vec2& operator/=(Dec s) { x /= s; y /= s; return *this; }
        inline bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
        inline bool operator!=(const Vec2& o) const { return !(*this == o); }

        inline Dec len() const { return std::sqrt(x * x + y * y); }
        inline Dec len_sq() const { return x * x + y * y; }
        inline Dec dist(const Vec2& o) const { return (*this - o).len(); }
        inline Dec dot(const Vec2& o) const { return x * o.x + y * o.y; }
        inline Dec size() const { return len(); }
        inline Dec length() const { return len(); }
        inline Vec2 normalize() const {
            Dec l = len();
            return l > 0.00001 ? Vec2(x / l, y / l) : Vec2(0.0, 0.0);
        }
    };

    inline Vec2 vec2(Dec x = 0.0, Dec y = 0.0) { return Vec2(x, y); }

    struct Vec3 {
        Dec x = 0.0;
        Dec y = 0.0;
        Dec z = 0.0;

        Vec3() = default;
        Vec3(Dec _x, Dec _y, Dec _z) : x(_x), y(_y), z(_z) {}

        inline Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
        inline Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
        inline Vec3 operator*(Dec s) const { return Vec3(x * s, y * s, z * s); }
        inline Vec3 operator/(Dec s) const { return Vec3(x / s, y / s, z / s); }
        inline Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
        inline Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
        inline Vec3& operator*=(Dec s) { x *= s; y *= s; z *= s; return *this; }
        inline Vec3& operator/=(Dec s) { x /= s; y /= s; z /= s; return *this; }
        inline bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
        inline bool operator!=(const Vec3& o) const { return !(*this == o); }

        inline Dec len() const { return std::sqrt(x * x + y * y + z * z); }
        inline Dec len_sq() const { return x * x + y * y + z * z; }
        inline Dec dist(const Vec3& o) const { return (*this - o).len(); }
        inline Dec dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
        inline Dec size() const { return len(); }
        inline Dec length() const { return len(); }
        inline Vec3 cross(const Vec3& o) const {
            return Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
        }
        inline Vec3 normalize() const {
            Dec l = len();
            return l > 0.00001 ? Vec3(x / l, y / l, z / l) : Vec3(0.0, 0.0, 0.0);
        }
    };

    inline Vec3 vec3(Dec x = 0.0, Dec y = 0.0, Dec z = 0.0) { return Vec3(x, y, z); }

    inline Str toStr(const Vec2& v) {
        return "Vec2(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
    }
    inline Str toStr(const Vec3& v) {
        return "Vec3(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
    }

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

    inline Str toStr(const Error& e) {
        return e.what();
    }
    struct Var {
        enum class Type { Null, Int, Dec, Str, Bool } type = Type::Null;
        Int i_val = 0;
        Dec d_val = 0.0;
        Str s_val = "";
        Bool b_val = false;

        Var() : type(Type::Null) {}
        Var(const Var& o) = default;
        Var& operator=(const Var& o) = default;

        Var(Int v) : type(Type::Int), i_val(v), d_val((Dec)v), s_val(std::to_string(v)), b_val(v != 0) {}
        Var(int v) : Var((Int)v) {}
        Var(long v) : Var((Int)v) {}
        Var(unsigned int v) : Var((Int)v) {}
        Var(unsigned long long v) : Var((Int)v) {}
        Var(Dec v) : type(Type::Dec), d_val(v), i_val((Int)v), s_val(std::to_string(v)), b_val(v != 0.0) {}
        Var(float v) : Var((Dec)v) {}
        Var(const Str& v) : type(Type::Str), s_val(v), b_val(!v.empty()) {
            try { i_val = std::stoll(v); } catch(...) { i_val = 0; }
            try { d_val = std::stod(v); } catch(...) { d_val = 0.0; }
        }
        Var(const char* v) : Var(Str(v ? v : "")) {}
        Var(Bool v) : type(Type::Bool), b_val(v), i_val(v ? 1 : 0), d_val(v ? 1.0 : 0.0), s_val(v ? "true" : "false") {}

        // Implicit conversions
        operator Str() const { return s_val; }
        operator Int() const { return i_val; }
        operator int() const { return (int)i_val; }
        operator Dec() const { return d_val; }
        operator float() const { return (float)d_val; }
        operator Bool() const { return b_val; }
        operator const char*() const { return s_val.c_str(); }

        inline Str to_str() const { return s_val; }
        inline Int to_int() const { return i_val; }
        inline Dec to_dec() const { return d_val; }
        inline Bool to_bool() const { return b_val; }

        inline bool operator==(const Var& o) const {
            if (type == Type::Str || o.type == Type::Str) return s_val == o.s_val;
            if (type == Type::Dec || o.type == Type::Dec) return d_val == o.d_val;
            return i_val == o.i_val;
        }
        inline bool operator!=(const Var& o) const { return !(*this == o); }
        inline bool operator<(const Var& o) const {
            if (type == Type::Str && o.type == Type::Str) return s_val < o.s_val;
            if (type == Type::Dec || o.type == Type::Dec) return d_val < o.d_val;
            return i_val < o.i_val;
        }

        inline Var operator+(const Var& o) const {
            if (type == Type::Str || o.type == Type::Str) return Var(s_val + o.s_val);
            if (type == Type::Dec || o.type == Type::Dec) return Var(d_val + o.d_val);
            return Var(i_val + o.i_val);
        }
        inline Var operator-(const Var& o) const {
            if (type == Type::Dec || o.type == Type::Dec) return Var(d_val - o.d_val);
            return Var(i_val - o.i_val);
        }
        inline Var operator*(const Var& o) const {
            if (type == Type::Dec || o.type == Type::Dec) return Var(d_val * o.d_val);
            return Var(i_val * o.i_val);
        }
        inline Var operator/(const Var& o) const {
            if (type == Type::Dec || o.type == Type::Dec) return Var(o.d_val != 0.0 ? d_val / o.d_val : 0.0);
            return Var(o.i_val != 0 ? i_val / o.i_val : 0);
        }

        friend std::ostream& operator<<(std::ostream& os, const Var& v) {
            os << v.s_val;
            return os;
        }
    };

    inline Str toStr(const Var& v) {
        return v.s_val;
    }

    template<typename T = Var>
    class List {
    private:
        std::shared_ptr<std::vector<T>> data;
        std::shared_ptr<std::mutex> mtx;
    public:
        List() : data(std::make_shared<std::vector<T>>()), mtx(std::make_shared<std::mutex>()) {}
        List(std::initializer_list<T> init) : data(std::make_shared<std::vector<T>>(init)), mtx(std::make_shared<std::mutex>()) {}

        template<typename U, typename = std::enable_if_t<std::is_constructible_v<T, U>>>
        List(std::initializer_list<U> init) : data(std::make_shared<std::vector<T>>()), mtx(std::make_shared<std::mutex>()) {
            for (const auto& item : init) {
                data->push_back(item);
            }
        }

        template<typename> friend class List;

        template<typename U, typename = std::enable_if_t<std::is_constructible_v<T, U>>>
        List(const List<U>& other) : data(std::make_shared<std::vector<T>>()), mtx(std::make_shared<std::mutex>()) {
            if (other.data) {
                for (const auto& item : *other.data) {
                    data->push_back((T)item);
                }
            }
        }
        
        inline void add(const T& item) {
            std::lock_guard<std::mutex> lock(*mtx);
            data->push_back(item);
        }
        inline void append(const T& item) {
            add(item);
        }
        inline T pop() {
            std::lock_guard<std::mutex> lock(*mtx);
            if (!data->empty()) {
                T val = data->back();
                data->pop_back();
                return val;
            }
            return T();
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
        inline void remove(const T& item) {
            std::lock_guard<std::mutex> lock(*mtx);
            auto it = std::find(data->begin(), data->end(), item);
            if (it != data->end()) {
                data->erase(it);
            }
        }
        inline void removeLast() {
            std::lock_guard<std::mutex> lock(*mtx);
            if (!data->empty()) {
                data->pop_back();
            }
        }
        template<typename IndexT, typename = std::enable_if_t<std::is_integral_v<IndexT>>>
        inline T get(IndexT index) const {
            std::lock_guard<std::mutex> lock(*mtx);
            Int idx = (Int)index;
            if (idx < 0) idx += (Int)data->size();
            if (idx >= 0 && (size_t)idx < data->size()) {
                return (*data)[(size_t)idx];
            }
            return T();
        }
        inline Bool contains(const T& item) const {
            std::lock_guard<std::mutex> lock(*mtx);
            return std::find(data->begin(), data->end(), item) != data->end();
        }
        inline Int index_of(const T& item) const {
            std::lock_guard<std::mutex> lock(*mtx);
            auto it = std::find(data->begin(), data->end(), item);
            if (it != data->end()) return (Int)(it - data->begin());
            return -1;
        }
        inline List<T> slice(Int start, Int count) const {
            std::lock_guard<std::mutex> lock(*mtx);
            List<T> res;
            if (start < 0) start += (Int)data->size();
            if (start < 0) start = 0;
            for (Int i = start; i < start + count && i < (Int)data->size(); ++i) {
                res.add((*data)[i]);
            }
            return res;
        }
        inline void reverse() {
            std::lock_guard<std::mutex> lock(*mtx);
            std::reverse(data->begin(), data->end());
        }
        inline void sort() {
            std::lock_guard<std::mutex> lock(*mtx);
            std::sort(data->begin(), data->end());
        }
        inline Str join(const Str& sep = ", ") const {
            std::lock_guard<std::mutex> lock(*mtx);
            std::stringstream ss;
            for (size_t i = 0; i < data->size(); ++i) {
                ss << (*data)[i];
                if (i + 1 < data->size()) ss << sep;
            }
            return ss.str();
        }
        inline Int size() const {
            std::lock_guard<std::mutex> lock(*mtx);
            return (Int)data->size();
        }
        inline Int get_len() const {
            return size();
        }

        inline T get_first() const {
            std::lock_guard<std::mutex> lock(*mtx);
            if (!data->empty()) return (*data).front();
            return T();
        }

        inline T get_last() const {
            std::lock_guard<std::mutex> lock(*mtx);
            if (!data->empty()) return (*data).back();
            return T();
        }

        inline void clear() {
            std::lock_guard<std::mutex> lock(*mtx);
            data->clear();
        }

        template<typename Fn>
        inline auto map(Fn fn) const {
            std::lock_guard<std::mutex> lock(*mtx);
            using R = std::decay_t<decltype(fn(std::declval<T>()))>;
            List<R> res;
            for (const auto& item : *data) {
                res.add(fn(item));
            }
            return res;
        }

        template<typename Fn>
        inline List<T> filter(Fn fn) const {
            std::lock_guard<std::mutex> lock(*mtx);
            List<T> res;
            for (const auto& item : *data) {
                if (fn(item)) {
                    res.add(item);
                }
            }
            return res;
        }

        template<typename Acc, typename Fn>
        inline Acc reduce(Acc init, Fn fn) const {
            std::lock_guard<std::mutex> lock(*mtx);
            Acc acc = init;
            for (const auto& item : *data) {
                acc = fn(acc, item);
            }
            return acc;
        }

        template<typename Fn>
        inline void each(Fn fn) const {
            std::lock_guard<std::mutex> lock(*mtx);
            for (const auto& item : *data) {
                fn(item);
            }
        }

        template<typename Fn>
        inline T find_first(Fn fn, T default_val = T()) const {
            std::lock_guard<std::mutex> lock(*mtx);
            for (const auto& item : *data) {
                if (fn(item)) return item;
            }
            return default_val;
        }

        template<typename Fn>
        inline void retain(Fn fn) {
            std::lock_guard<std::mutex> lock(*mtx);
            data->erase(std::remove_if(data->begin(), data->end(), [&](const T& item) {
                return !fn(item);
            }), data->end());
        }

        template<typename Fn>
        inline void remove_if(Fn fn) {
            std::lock_guard<std::mutex> lock(*mtx);
            data->erase(std::remove_if(data->begin(), data->end(), [&](const T& item) {
                return fn(item);
            }), data->end());
        }

        template<typename Fn>
        inline Bool all(Fn fn) const {
            std::lock_guard<std::mutex> lock(*mtx);
            for (const auto& item : *data) {
                if (!fn(item)) return false;
            }
            return true;
        }

        template<typename Fn>
        inline Bool any(Fn fn) const {
            std::lock_guard<std::mutex> lock(*mtx);
            for (const auto& item : *data) {
                if (fn(item)) return true;
            }
            return false;
        }

        inline void shuffle() {
            std::lock_guard<std::mutex> lock(*mtx);
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(data->begin(), data->end(), g);
        }

        inline T choice() const {
            std::lock_guard<std::mutex> lock(*mtx);
            if (data->empty()) return T();
            static std::random_device rd;
            static std::mt19937_64 g(rd());
            std::uniform_int_distribution<size_t> dis(0, data->size() - 1);
            return (*data)[dis(g)];
        }

        template<typename IndexT, typename = std::enable_if_t<std::is_integral_v<IndexT>>>
        inline typename std::vector<T>::reference operator[](IndexT index) {
            std::lock_guard<std::mutex> lock(*mtx);
            Int idx = (Int)index;
            if (idx < 0) idx += (Int)data->size();
            return (*data)[(size_t)idx];
        }

        template<typename IndexT, typename = std::enable_if_t<std::is_integral_v<IndexT>>>
        inline typename std::vector<T>::const_reference operator[](IndexT index) const {
            std::lock_guard<std::mutex> lock(*mtx);
            Int idx = (Int)index;
            if (idx < 0) idx += (Int)data->size();
            return (*data)[(size_t)idx];
        }

        template<typename IndexT, typename = std::enable_if_t<std::is_integral_v<IndexT>>>
        inline void set(IndexT index, const T& val) {
            std::lock_guard<std::mutex> lock(*mtx);
            Int idx = (Int)index;
            if (idx < 0) idx += (Int)data->size();
            if (idx >= 0 && (size_t)idx < data->size()) {
                (*data)[(size_t)idx] = val;
            }
        }

        inline auto begin() { return data->begin(); }
        inline auto end() { return data->end(); }
        inline auto begin() const { return data->begin(); }
        inline auto end() const { return data->end(); }
    };

    template<typename K = Str, typename V = Var>
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
        inline V get(const K& key, const V& default_val = V()) const {
            std::lock_guard<std::mutex> lock(*mtx);
            auto it = data->find(key);
            if (it != data->end()) {
                return it->second;
            }
            return default_val;
        }
        inline Bool contains(const K& key) const {
            return has(key);
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
        inline List<V> values() const {
            std::lock_guard<std::mutex> lock(*mtx);
            List<V> vList;
            for (const auto& pair : *data) {
                vList.add(pair.second);
            }
            return vList;
        }

        inline V& operator[](const K& key) {
            std::lock_guard<std::mutex> lock(*mtx);
            return (*data)[key];
        }
        inline auto begin() { return data->begin(); }
        inline auto end() { return data->end(); }
        inline auto begin() const { return data->begin(); }
        inline auto end() const { return data->end(); }
    };

    template<typename K = Str, typename V = Var>
    using Dict = Map<K, V>;

    // Slicing utilities for range operator [start..end]
    template<typename T>
    inline List<T> slice(const List<T>& list, Int start, Int end) {
        Int n = list.size();
        if (start < 0) start = n + start;
        if (end < 0) end = n + end;
        if (start < 0) start = 0;
        if (end > n) end = n;
        if (start >= end) return List<T>{};
        return list.slice(start, end - start);
    }

    inline Str slice(const Str& str, Int start, Int end) {
        Int n = (Int)str.size();
        if (start < 0) start = n + start;
        if (end < 0) end = n + end;
        if (start < 0) start = 0;
        if (end > n) end = n;
        if (start >= end) return "";
        return str.substr((size_t)start, (size_t)(end - start));
    }


    // Membership testing: in / !in
    template<typename T, typename U>
    inline bool contains(const List<T>& list, const U& item) {
        return list.contains(item);
    }
    template<typename K, typename V, typename U>
    inline bool contains(const Map<K, V>& map, const U& key) {
        return map.has(key);
    }
    inline bool contains(const Str& str, const Str& sub) {
        return str.find(sub) != std::string::npos;
    }
    inline bool contains(const Str& str, const char* sub) {
        return str.find(sub ? sub : "") != std::string::npos;
    }
    inline bool contains(const Str& str, char c) {
        return str.find(c) != std::string::npos;
    }
    inline bool contains(const Var& var, const Var& item) {
        return var.to_str().find(item.to_str()) != std::string::npos;
    }

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

    namespace retrotech_bridge {
        inline void (*apply_colormask_fn)(const uint8_t*, size_t) = nullptr;
    }

    // Grid: Multi-Layer Volumetric / Depth Grid with Custom Layer Dimensions & Matrix Editing
    class GridLayer {
    public:
        int width;
        int height;
        int thickness;
        std::vector<uint8_t> voxels; // size: width * height * thickness

        GridLayer(int w = 32, int h = 24, int thick = 1)
            : width(w > 0 ? w : 1), height(h > 0 ? h : 1), thickness(thick > 0 ? thick : 1),
              voxels((w > 0 ? w : 1) * (h > 0 ? h : 1) * (thick > 0 ? thick : 1), 0) {}

        inline int get(int x, int y, int z = 0) const {
            if (x >= 0 && x < width && y >= 0 && y < height && z >= 0 && z < thickness) {
                size_t idx = (size_t)((z * height + y) * width + x);
                return (idx < voxels.size()) ? voxels[idx] : 0;
            }
            return 0;
        }

        inline void set(int x, int y, int z, int val) {
            if (x < 0 || y < 0 || z < 0) return;
            if (x >= width) width = x + 1;
            if (y >= height) height = y + 1;
            if (z >= thickness) thickness = z + 1;
            size_t needed = (size_t)(width * height * thickness);
            if (needed > voxels.size()) voxels.resize(needed, 0);
            voxels[(z * height + y) * width + x] = (uint8_t)(val & 0xFF);
        }

        inline void set(int x, int y, int val) {
            set(x, y, 0, val);
        }

        inline uint8_t& operator[](size_t idx) {
            if (idx >= voxels.size()) voxels.resize(idx + 1, 0);
            return voxels[idx];
        }

        inline uint8_t operator[](size_t idx) const {
            return (idx < voxels.size()) ? voxels[idx] : 0;
        }

        inline size_t size() const { return voxels.size(); }
        inline Int get_len() const { return (Int)voxels.size(); }
    };

    class Grid {
    public:
        std::vector<GridLayer> layers;
        int default_w = 32;
        int default_h = 24;

        Grid(int w = 32, int h = 24)
            : default_w(w), default_h(h) {
            layers.emplace_back(w, h, 1);
        }

        Grid(std::initializer_list<std::pair<int, int>> specs, int w = 32, int h = 24)
            : default_w(w), default_h(h) {
            set_specs(specs, w, h);
        }

        Grid(const std::vector<std::pair<int, int>>& specs, int w = 32, int h = 24)
            : default_w(w), default_h(h) {
            layers.clear();
            for (const auto& sp : specs) {
                int lw = sp.first > 0 ? sp.first : default_w;
                int lh = sp.second > 0 ? sp.second : default_h;
                layers.emplace_back(lw, lh, 1);
            }
            if (layers.empty()) layers.emplace_back(default_w, default_h, 1);
        }

        inline void set_specs(std::initializer_list<std::pair<int, int>> specs, int w = 0, int h = 0) {
            layers.clear();
            for (const auto& sp : specs) {
                int lw = sp.first > 0 ? sp.first : (w > 0 ? w : default_w);
                int lh = sp.second > 0 ? sp.second : (h > 0 ? h : default_h);
                layers.emplace_back(lw, lh, 1);
            }
            if (layers.empty()) layers.emplace_back(w > 0 ? w : default_w, h > 0 ? h : default_h, 1);
        }

        inline size_t layer_count() const { return layers.size(); }
        inline Int total_layers() const { return (Int)layers.size(); }
        inline size_t size() const { return layers.size(); }
        inline Int get_len() const { return (Int)layers.size(); }

        inline int width(size_t layer = 0) const {
            return (layer < layers.size()) ? layers[layer].width : default_w;
        }
        inline int height(size_t layer = 0) const {
            return (layer < layers.size()) ? layers[layer].height : default_h;
        }

        inline int layer_thickness(size_t layer_idx) const {
            if (layer_idx < layers.size()) return layers[layer_idx].thickness;
            return 0;
        }

        inline int total_depth() const {
            int d = 0;
            for (const auto& l : layers) d += l.thickness;
            return d;
        }

        inline GridLayer& operator[](size_t layer_idx) {
            if (layer_idx >= layers.size()) {
                layers.resize(layer_idx + 1, GridLayer(default_w, default_h, 1));
            }
            return layers[layer_idx];
        }

        inline const GridLayer& operator[](size_t layer_idx) const {
            return layers[layer_idx];
        }

        inline int get(size_t layer, int x, int y, int z = 0) const {
            if (layer < layers.size()) {
                return layers[layer].get(x, y, z);
            }
            return 0;
        }

        inline void set(size_t layer, int x, int y, int z, int val) {
            if (layer >= layers.size()) {
                layers.resize(layer + 1, GridLayer(default_w, default_h, 1));
            }
            layers[layer].set(x, y, z, val);
        }

        inline void set(size_t layer, int x, int y, int val) {
            set(layer, x, y, 0, val);
        }

        inline void resize_grid(int w, int h) {
            default_w = w;
            default_h = h;
            for (auto& l : layers) {
                l.width = w;
                l.height = h;
                l.voxels.assign(w * h * l.thickness, 0);
            }
        }

        inline void grid_edit(const std::vector<std::vector<int>>& rows) {
            if (layers.empty()) {
                int max_w = 0;
                for (const auto& r : rows) max_w = std::max(max_w, (int)r.size());
                if (max_w == 0) max_w = 1;
                layers.emplace_back(max_w, (int)rows.size());
            }
            size_t cur_l = 0;
            int cur_y = 0;
            for (const auto& r : rows) {
                if (cur_l >= layers.size()) {
                    int w = (int)r.size() > 0 ? (int)r.size() : 1;
                    layers.emplace_back(w, 1);
                }
                auto& layer = layers[cur_l];
                for (size_t c = 0; c < r.size(); ++c) {
                    layer.set((int)c, cur_y, r[c]);
                }
                cur_y++;
                if (cur_y >= layer.height && cur_l + 1 < layers.size()) {
                    cur_l++;
                    cur_y = 0;
                }
            }
        }

        inline void grid_print() const {
            std::cout << "[Grid: " << layers.size() << " layers]" << std::endl;
            for (size_t l = 0; l < layers.size(); ++l) {
                const auto& layer = layers[l];
                std::cout << "  Layer " << l << " (" << layer.width << "x" << layer.height << "):" << std::endl;
                for (int y = 0; y < layer.height; ++y) {
                    std::cout << "    ";
                    for (int x = 0; x < layer.width; ++x) {
                        std::cout << (int)layer.get(x, y) << "\t";
                    }
                    std::cout << std::endl;
                }
            }
        }

        inline void grid_clear() {
            for (auto& l : layers) {
                std::fill(l.voxels.begin(), l.voxels.end(), 0);
            }
        }

        inline void grid_fill(int val) {
            for (auto& l : layers) {
                std::fill(l.voxels.begin(), l.voxels.end(), (uint8_t)(val & 0xFF));
            }
        }

        inline void grid_invert() {
            for (auto& l : layers) {
                for (auto& v : l.voxels) v = (uint8_t)(~v);
            }
        }

        inline void flip_h(size_t layer = 0) {
            if (layer < layers.size()) {
                auto& l = layers[layer];
                for (int y = 0; y < l.height; ++y) {
                    for (int x = 0; x < l.width / 2; ++x) {
                        std::swap(l.voxels[y * l.width + x], l.voxels[y * l.width + (l.width - 1 - x)]);
                    }
                }
            }
        }

        inline void flip_v(size_t layer = 0) {
            if (layer < layers.size()) {
                auto& l = layers[layer];
                for (int y = 0; y < l.height / 2; ++y) {
                    for (int x = 0; x < l.width; ++x) {
                        std::swap(l.voxels[y * l.width + x], l.voxels[(l.height - 1 - y) * l.width + x]);
                    }
                }
            }
        }

        inline Str to_string() const {
            std::stringstream ss;
            ss << "[Grid: " << layers.size() << " Layers (";
            for (size_t i = 0; i < layers.size(); ++i) {
                ss << "L" << i << ": " << layers[i].width << "x" << layers[i].height;
                if (i + 1 < layers.size()) ss << ", ";
            }
            ss << ")]";
            return ss.str();
        }
    };

    enum class MaskView {
        HLV, // Human Like Vision (decimal: 255, 0, 128)
        RAW  // Raw binary (8 zeros and ones: 11111111, 00000000)
    };

    // Bytes: High-Performance Hardware-Level Memory Buffer
    class Bytes {
    protected:
        std::shared_ptr<std::vector<uint8_t>> data;
    public:
        Grid grid;
        size_t cursor = 0;
        MaskView view_mode = MaskView::HLV;
        static const size_t DEFAULT_MAX_SIZE = 1024; // 1 KB maximum default

        inline void set_view(MaskView v) { view_mode = v; }
        inline void set_view(const Str& v) {
            std::string s = v;
            for (auto& c : s) c = (char)std::tolower((unsigned char)c);
            if (s == "raw") view_mode = MaskView::RAW;
            else view_mode = MaskView::HLV;
        }
        inline void view(const Str& v) { set_view(v); }
        inline Str get_view() const {
            return view_mode == MaskView::RAW ? "raw" : "hlv";
        }

        inline Str to_string() const {
            std::stringstream ss;
            ss << "[";
            for (size_t i = 0; i < data->size(); ++i) {
                if (view_mode == MaskView::RAW) {
                    uint8_t b = (*data)[i];
                    for (int bit = 7; bit >= 0; --bit) {
                        ss << ((b >> bit) & 1);
                    }
                } else {
                    ss << (int)(*data)[i];
                }
                if (i + 1 < data->size()) ss << ", ";
            }
            ss << "]";
            return ss.str();
        }

        Bytes()
            : data(std::make_shared<std::vector<uint8_t>>(DEFAULT_MAX_SIZE, 0)) {}
        Bytes(int size)
            : data(std::make_shared<std::vector<uint8_t>>(size > 0 ? size : 0, 0)) {}
        Bytes(size_t size, uint8_t fill = 0)
            : data(std::make_shared<std::vector<uint8_t>>(size, fill)) {}
        Bytes(const Str& str)
            : data(std::make_shared<std::vector<uint8_t>>(str.begin(), str.end())) {}
        Bytes(const char* str)
            : Bytes(Str(str ? str : "")) {}
        Bytes(std::initializer_list<uint8_t> init)
            : data(std::make_shared<std::vector<uint8_t>>(init)) {}
        template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
        Bytes(std::initializer_list<T> init)
            : data(std::make_shared<std::vector<uint8_t>>()) {
            data->reserve(init.size());
            for (auto v : init) data->push_back((uint8_t)v);
        }

        inline Bytes& operator=(std::initializer_list<uint8_t> init) {
            data = std::make_shared<std::vector<uint8_t>>(init);
            return *this;
        }
        template<typename T>
        inline Bytes& operator=(std::initializer_list<T> init) {
            data = std::make_shared<std::vector<uint8_t>>();
            data->reserve(init.size());
            for (auto v : init) data->push_back((uint8_t)v);
            return *this;
        }

        template<typename T>
        Bytes(const List<T>& list)
            : data(std::make_shared<std::vector<uint8_t>>()) {
            Int sz = list.size();
            data->reserve(sz > 0 ? (size_t)sz : 0);
            for (Int i = 0; i < sz; ++i) {
                data->push_back((uint8_t)(int64_t)list.get(i));
            }
        }
        template<typename T>
        inline Bytes& operator=(const List<T>& list) {
            data = std::make_shared<std::vector<uint8_t>>();
            Int sz = list.size();
            data->reserve(sz > 0 ? (size_t)sz : 0);
            for (Int i = 0; i < sz; ++i) {
                data->push_back((uint8_t)(int64_t)list.get(i));
            }
            return *this;
        }
        template<typename T>
        Bytes(const std::vector<T>& vec)
            : data(std::make_shared<std::vector<uint8_t>>()) {
            data->reserve(vec.size());
            for (const auto& v : vec) data->push_back((uint8_t)(int64_t)v);
        }
        template<typename T>
        inline Bytes& operator=(const std::vector<T>& vec) {
            data = std::make_shared<std::vector<uint8_t>>();
            data->reserve(vec.size());
            for (const auto& v : vec) data->push_back((uint8_t)(int64_t)v);
            return *this;
        }

        inline size_t size() const { return data ? data->size() : 0; }
        inline Int get_len() const { return (Int)size(); }

        template<typename IndexT, typename = std::enable_if_t<std::is_integral_v<IndexT>>>
        inline uint8_t& operator[](IndexT index) {
            Int idx = (Int)index;
            if (idx < 0) idx += (Int)data->size();
            if (idx < 0) idx = 0;
            if ((size_t)idx >= data->size()) data->resize((size_t)idx + 1, 0);
            return (*data)[(size_t)idx];
        }
        template<typename IndexT, typename = std::enable_if_t<std::is_integral_v<IndexT>>>
        inline const uint8_t& operator[](IndexT index) const {
            Int idx = (Int)index;
            if (idx < 0) idx += (Int)data->size();
            if (idx < 0 || (size_t)idx >= data->size()) {
                static uint8_t dummy = 0;
                return dummy;
            }
            return (*data)[(size_t)idx];
        }
        template<typename IndexT, typename = std::enable_if_t<std::is_integral_v<IndexT>>>
        inline uint8_t get(IndexT index) const {
            Int idx = (Int)index;
            if (idx < 0) idx += (Int)data->size();
            if (idx >= 0 && (size_t)idx < data->size()) return (*data)[(size_t)idx];
            return 0;
        }
        template<typename IndexT, typename = std::enable_if_t<std::is_integral_v<IndexT>>>
        inline void set(IndexT index, uint8_t val) {
            Int idx = (Int)index;
            if (idx < 0) idx += (Int)data->size();
            if (idx < 0) idx = 0;
            if ((size_t)idx >= data->size()) data->resize((size_t)idx + 1, 0);
            (*data)[(size_t)idx] = val;
        }
        inline void set_bit(size_t byte_idx, uint8_t bit_idx, bool val) {
            if (byte_idx >= data->size()) data->resize(byte_idx + 1, 0);
            if (bit_idx < 8) {
                if (val) (*data)[byte_idx] |= (1 << bit_idx);
                else (*data)[byte_idx] &= ~(1 << bit_idx);
            }
        }
        inline bool get_bit(size_t byte_idx, uint8_t bit_idx) const {
            if (byte_idx < data->size() && bit_idx < 8) {
                return ((*data)[byte_idx] >> bit_idx) & 1;
            }
            return false;
        }
        inline bool has_bit(size_t byte_idx, uint8_t bit_idx) const {
            return get_bit(byte_idx, bit_idx);
        }
        inline void fill(uint8_t val) {
            std::fill(data->begin(), data->end(), val);
        }
        inline void clear() {
            std::fill(data->begin(), data->end(), 0);
        }
        inline void resize(size_t new_size, uint8_t fill_val = 0) {
            data->resize(new_size, fill_val);
        }
        inline Bytes slice(size_t start, size_t count) const {
            Bytes res(count, 0);
            for (size_t i = 0; i < count && start + i < data->size(); ++i) {
                res.set(i, (*data)[start + i]);
            }
            return res;
        }
        inline void copy_to(Bytes& dest, size_t dest_offset = 0) const {
            for (size_t i = 0; i < data->size(); ++i) {
                dest.set(dest_offset + i, (*data)[i]);
            }
        }
        inline uint16_t get_u16(size_t idx) const {
            if (idx + 1 < data->size()) {
                return (uint16_t)((*data)[idx] | ((*data)[idx + 1] << 8));
            }
            return 0;
        }
        inline void set_u16(size_t idx, uint16_t val) {
            set(idx, (uint8_t)(val & 0xFF));
            set(idx + 1, (uint8_t)((val >> 8) & 0xFF));
        }
        inline uint32_t get_u32(size_t idx) const {
            if (idx + 3 < data->size()) {
                return (uint32_t)((*data)[idx] | ((*data)[idx + 1] << 8) | ((*data)[idx + 2] << 16) | ((*data)[idx + 3] << 24));
            }
            return 0;
        }
        inline void set_u32(size_t idx, uint32_t val) {
            set(idx, (uint8_t)(val & 0xFF));
            set(idx + 1, (uint8_t)((val >> 8) & 0xFF));
            set(idx + 2, (uint8_t)((val >> 16) & 0xFF));
            set(idx + 3, (uint8_t)((val >> 24) & 0xFF));
        }
        inline Str to_hex() const {
            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            for (size_t i = 0; i < data->size(); ++i) {
                ss << std::setw(2) << (int)(*data)[i] << " ";
            }
            return ss.str();
        }

        // =====================================================================
        // RAW REPRESENTATIONS (BINARY BITS & HEX)
        // =====================================================================
        inline Str to_bin() const {
            std::stringstream ss;
            for (size_t i = 0; i < data->size(); ++i) {
                uint8_t b = (*data)[i];
                for (int bit = 7; bit >= 0; --bit) {
                    ss << ((b >> bit) & 1);
                }
                if (i + 1 < data->size()) ss << " ";
            }
            return ss.str();
        }

        inline Str to_bin_raw() const {
            std::stringstream ss;
            for (size_t i = 0; i < data->size(); ++i) {
                uint8_t b = (*data)[i];
                for (int bit = 7; bit >= 0; --bit) {
                    ss << ((b >> bit) & 1);
                }
            }
            return ss.str();
        }

        inline Str get_bin(size_t idx) const {
            if (idx >= data->size()) return "00000000";
            uint8_t b = (*data)[idx];
            std::string s = "";
            for (int bit = 7; bit >= 0; --bit) {
                s += ((b >> bit) & 1) ? '1' : '0';
            }
            return s;
        }

        inline void set_bin(size_t idx, const Str& bin_str) {
            if (idx >= data->size()) data->resize(idx + 1, 0);
            uint8_t val = 0;
            int bit_count = 0;
            for (char ch : bin_str) {
                if (ch == '0' || ch == '1') {
                    val = (val << 1) | (ch - '0');
                    bit_count++;
                    if (bit_count == 8) break;
                }
            }
            (*data)[idx] = val;
        }

        inline Str get_hex(size_t idx) const {
            if (idx >= data->size()) return "00";
            std::stringstream ss;
            ss << std::hex << std::setfill('0') << std::setw(2) << (int)(*data)[idx];
            return ss.str();
        }

        inline void set_hex(size_t idx, const Str& hex_str) {
            if (idx >= data->size()) data->resize(idx + 1, 0);
            std::string clean = "";
            for (char ch : hex_str) {
                if (std::isxdigit(ch)) clean += ch;
            }
            if (clean.empty()) clean = "00";
            try {
                (*data)[idx] = (uint8_t)std::strtol(clean.c_str(), nullptr, 16);
            } catch (...) {}
        }

        static inline Bytes from_bin(const Str& bin_str) {
            std::vector<uint8_t> bytes;
            uint8_t cur_byte = 0;
            int bit_count = 0;
            for (char ch : bin_str) {
                if (ch == '0' || ch == '1') {
                    cur_byte = (cur_byte << 1) | (ch - '0');
                    bit_count++;
                    if (bit_count == 8) {
                        bytes.push_back(cur_byte);
                        cur_byte = 0;
                        bit_count = 0;
                    }
                }
            }
            if (bit_count > 0) {
                cur_byte <<= (8 - bit_count);
                bytes.push_back(cur_byte);
            }
            Bytes res(bytes.size(), 0);
            for (size_t i = 0; i < bytes.size(); ++i) res.set(i, bytes[i]);
            return res;
        }

        static inline Bytes from_hex(const Str& hex_str) {
            std::vector<uint8_t> bytes;
            std::string cur = "";
            for (char ch : hex_str) {
                if (std::isxdigit(ch)) {
                    cur += ch;
                    if (cur.size() == 2) {
                        bytes.push_back((uint8_t)std::strtol(cur.c_str(), nullptr, 16));
                        cur.clear();
                    }
                }
            }
            if (!cur.empty()) {
                bytes.push_back((uint8_t)(std::strtol(cur.c_str(), nullptr, 16) << 4));
            }
            Bytes res(bytes.size(), 0);
            for (size_t i = 0; i < bytes.size(); ++i) res.set(i, bytes[i]);
            return res;
        }

        static inline Bytes from_raw(const Str& raw_str) {
            std::string s = raw_str;
            if (s.rfind("0b", 0) == 0 || s.rfind("0B", 0) == 0) {
                return from_bin(s.substr(2));
            }
            if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
                return from_hex(s.substr(2));
            }
            bool only_binary = true;
            int bit_count = 0;
            for (char ch : s) {
                if (ch == '0' || ch == '1') bit_count++;
                else if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
                    only_binary = false;
                    break;
                }
            }
            if (only_binary && bit_count > 0) {
                return from_bin(s);
            }
            return from_hex(s);
        }

        inline void dump_raw() const {
            std::cout << "--- Raw Buffer Dump (" << data->size() << " Bytes, cursor=" << cursor << ") ---" << std::endl;
            for (size_t i = 0; i < data->size(); ++i) {
                uint8_t b = (*data)[i];
                std::printf("[%04zx] HEX: 0x%02X | BIN: ", i, b);
                for (int bit = 7; bit >= 0; --bit) {
                    std::putchar(((b >> bit) & 1) ? '1' : '0');
                }
                std::printf(" | DEC: %3d | CHAR: %c\n", (int)b, (b >= 32 && b <= 126) ? (char)b : '.');
            }
        }

        // =====================================================================
        // BINARY STREAM CURSOR, WRITE & READ (TEXT, INTEGERS, DECIMALS, BYTES)
        // =====================================================================
        inline void seek(size_t pos) {
            if (pos > data->size()) data->resize(pos, 0);
            cursor = pos;
        }
        inline size_t tell() const { return cursor; }
        inline void rewind() { cursor = 0; }
        inline bool eof() const { return cursor >= data->size(); }
        inline size_t remaining() const { return (cursor < data->size()) ? (data->size() - cursor) : 0; }

        inline void write_u8(uint8_t val, int offset = -1) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            if (pos >= data->size()) data->resize(pos + 1, 0);
            (*data)[pos] = val;
            if (offset < 0) cursor = pos + 1;
        }
        inline void write_i8(int8_t val, int offset = -1) { write_u8((uint8_t)val, offset); }
        inline void write_byte(uint8_t val, int offset = -1) { write_u8(val, offset); }

        inline void write_u16(uint16_t val, int offset = -1) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            if (pos + 2 > data->size()) data->resize(pos + 2, 0);
            (*data)[pos] = (uint8_t)(val & 0xFF);
            (*data)[pos + 1] = (uint8_t)((val >> 8) & 0xFF);
            if (offset < 0) cursor = pos + 2;
        }
        inline void write_i16(int16_t val, int offset = -1) { write_u16((uint16_t)val, offset); }

        inline void write_u32(uint32_t val, int offset = -1) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            if (pos + 4 > data->size()) data->resize(pos + 4, 0);
            for (int i = 0; i < 4; ++i) (*data)[pos + i] = (uint8_t)((val >> (i * 8)) & 0xFF);
            if (offset < 0) cursor = pos + 4;
        }
        inline void write_i32(int32_t val, int offset = -1) { write_u32((uint32_t)val, offset); }

        inline void write_u64(uint64_t val, int offset = -1) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            if (pos + 8 > data->size()) data->resize(pos + 8, 0);
            for (int i = 0; i < 8; ++i) (*data)[pos + i] = (uint8_t)((val >> (i * 8)) & 0xFF);
            if (offset < 0) cursor = pos + 8;
        }
        inline void write_i64(int64_t val, int offset = -1) { write_u64((uint64_t)val, offset); }
        inline void write_int(Int val, int offset = -1) { write_i64((int64_t)val, offset); }

        inline void write_dec(Dec val, int offset = -1) {
            double d = val;
            uint64_t raw = 0;
            std::memcpy(&raw, &d, sizeof(double));
            write_u64(raw, offset);
        }
        inline void write_float(float val, int offset = -1) {
            uint32_t raw = 0;
            std::memcpy(&raw, &val, sizeof(float));
            write_u32(raw, offset);
        }
        inline void write_double(double val, int offset = -1) { write_dec(val, offset); }
        inline void write_bool(bool val, int offset = -1) { write_u8(val ? 1 : 0, offset); }
        inline void write_bit(bool val, int offset = -1) { write_bool(val, offset); }

        inline void write_str(const Str& s, int offset = -1, bool null_term = true) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            size_t needed = pos + s.size() + (null_term ? 1 : 0);
            if (needed > data->size()) data->resize(needed, 0);
            for (size_t i = 0; i < s.size(); ++i) {
                (*data)[pos + i] = (uint8_t)s[i];
            }
            if (null_term) {
                (*data)[pos + s.size()] = 0;
                if (offset < 0) cursor = pos + s.size() + 1;
            } else {
                if (offset < 0) cursor = pos + s.size();
            }
        }
        inline void write_string(const Str& s, int offset = -1, bool null_term = true) {
            write_str(s, offset, null_term);
        }
        inline void write_bytes(const Bytes& other, int offset = -1) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            size_t needed = pos + other.size();
            if (needed > data->size()) data->resize(needed, 0);
            for (size_t i = 0; i < other.size(); ++i) {
                (*data)[pos + i] = other.get(i);
            }
            if (offset < 0) cursor = pos + other.size();
        }

        inline uint8_t read_u8(int offset = -1) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            uint8_t val = (pos < data->size()) ? (*data)[pos] : 0;
            if (offset < 0) cursor = pos + 1;
            return val;
        }
        inline int8_t read_i8(int offset = -1) { return (int8_t)read_u8(offset); }
        inline uint8_t read_byte(int offset = -1) { return read_u8(offset); }

        inline uint16_t read_u16(int offset = -1) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            uint16_t b0 = (pos < data->size()) ? (*data)[pos] : 0;
            uint16_t b1 = (pos + 1 < data->size()) ? (*data)[pos + 1] : 0;
            if (offset < 0) cursor = pos + 2;
            return b0 | (b1 << 8);
        }
        inline int16_t read_i16(int offset = -1) { return (int16_t)read_u16(offset); }

        inline uint32_t read_u32(int offset = -1) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            uint32_t val = 0;
            for (int i = 0; i < 4; ++i) {
                uint32_t b = (pos + i < data->size()) ? (*data)[pos + i] : 0;
                val |= (b << (i * 8));
            }
            if (offset < 0) cursor = pos + 4;
            return val;
        }
        inline int32_t read_i32(int offset = -1) { return (int32_t)read_u32(offset); }

        inline uint64_t read_u64(int offset = -1) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            uint64_t val = 0;
            for (int i = 0; i < 8; ++i) {
                uint64_t b = (pos + i < data->size()) ? (*data)[pos + i] : 0;
                val |= (b << (i * 8));
            }
            if (offset < 0) cursor = pos + 8;
            return val;
        }
        inline int64_t read_i64(int offset = -1) { return (int64_t)read_u64(offset); }
        inline Int read_int(int offset = -1) { return (Int)read_i64(offset); }

        inline Dec read_dec(int offset = -1) {
            uint64_t raw = read_u64(offset);
            double d = 0.0;
            std::memcpy(&d, &raw, sizeof(double));
            return d;
        }
        inline float read_float(int offset = -1) {
            uint32_t raw = read_u32(offset);
            float f = 0.0f;
            std::memcpy(&f, &raw, sizeof(float));
            return f;
        }
        inline double read_double(int offset = -1) { return (double)read_dec(offset); }
        inline bool read_bool(int offset = -1) { return read_u8(offset) != 0; }
        inline bool read_bit(int offset = -1) { return read_bool(offset); }

        inline Str read_str(int len = -1, int offset = -1) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            std::string s;
            if (len >= 0) {
                for (int i = 0; i < len && pos + i < data->size(); ++i) {
                    s.push_back((char)(*data)[pos + i]);
                }
                if (offset < 0) cursor = pos + len;
            } else {
                while (pos < data->size() && (*data)[pos] != 0) {
                    s.push_back((char)(*data)[pos++]);
                }
                if (pos < data->size() && (*data)[pos] == 0) pos++;
                if (offset < 0) cursor = pos;
            }
            return s;
        }
        inline Str read_string(int len = -1, int offset = -1) { return read_str(len, offset); }

        inline Bytes read_bytes(size_t len, int offset = -1) {
            size_t pos = (offset >= 0) ? (size_t)offset : cursor;
            Bytes res(len, 0);
            for (size_t i = 0; i < len && pos + i < data->size(); ++i) {
                res.set(i, (*data)[pos + i]);
            }
            if (offset < 0) cursor = pos + len;
            return res;
        }

        // =====================================================================
        // ARBITRARY BITFIELDS & FLAG OPERATIONS
        // =====================================================================
        inline uint64_t get_bitfield(size_t start_bit, uint8_t bit_count) const {
            if (bit_count == 0 || bit_count > 64) return 0;
            uint64_t result = 0;
            for (uint8_t i = 0; i < bit_count; ++i) {
                size_t bit_idx = start_bit + i;
                size_t b_idx = bit_idx / 8;
                uint8_t bit_in_byte = (uint8_t)(bit_idx % 8);
                if (b_idx < data->size()) {
                    if (((*data)[b_idx] >> bit_in_byte) & 1) {
                        result |= ((uint64_t)1 << i);
                    }
                }
            }
            return result;
        }
        inline void set_bitfield(size_t start_bit, uint8_t bit_count, uint64_t val) {
            if (bit_count == 0 || bit_count > 64) return;
            size_t max_bit = start_bit + bit_count;
            size_t needed_bytes = (max_bit + 7) / 8;
            if (needed_bytes > data->size()) data->resize(needed_bytes, 0);
            for (uint8_t i = 0; i < bit_count; ++i) {
                size_t bit_idx = start_bit + i;
                size_t b_idx = bit_idx / 8;
                uint8_t bit_in_byte = (uint8_t)(bit_idx % 8);
                bool b = (val >> i) & 1;
                if (b) (*data)[b_idx] |= (1 << bit_in_byte);
                else (*data)[b_idx] &= ~(1 << bit_in_byte);
            }
        }
        inline bool test_bit(size_t bit_idx) const {
            size_t b_idx = bit_idx / 8;
            if (b_idx >= data->size()) return false;
            return (((*data)[b_idx] >> (bit_idx % 8)) & 1) != 0;
        }
        inline void toggle_bit(size_t bit_idx) {
            set_bit(bit_idx / 8, (uint8_t)(bit_idx % 8), !test_bit(bit_idx));
        }
        inline size_t popcount() const {
            size_t total = 0;
            for (uint8_t b : *data) {
                total += (size_t)__builtin_popcount((unsigned int)b);
            }
            return total;
        }
        inline size_t count_ones() const { return popcount(); }
        inline size_t count_zeros() const { return (data->size() * 8) - popcount(); }
        inline int find_first_bit(bool val) const {
            for (size_t i = 0; i < data->size() * 8; ++i) {
                if (test_bit(i) == val) return (int)i;
            }
            return -1;
        }
        inline bool has_flag(uint8_t byte_idx, uint8_t flag_mask) const {
            return (get(byte_idx) & flag_mask) == flag_mask;
        }
        inline void set_flag(uint8_t byte_idx, uint8_t flag_mask) {
            set(byte_idx, get(byte_idx) | flag_mask);
        }
        inline void clear_flag(uint8_t byte_idx, uint8_t flag_mask) {
            set(byte_idx, get(byte_idx) & ~flag_mask);
        }
        inline void toggle_flag(uint8_t byte_idx, uint8_t flag_mask) {
            set(byte_idx, get(byte_idx) ^ flag_mask);
        }

        // =====================================================================
        // MEMORY MASKING, DIFF & DISTANCE
        // =====================================================================
        inline Bytes diff(const Bytes& other) const {
            size_t max_sz = std::max(data->size(), other.data->size());
            Bytes res(max_sz, 0);
            for (size_t i = 0; i < max_sz; ++i) {
                uint8_t a = (i < data->size()) ? (*data)[i] : 0;
                uint8_t b = (i < other.data->size()) ? (*other.data)[i] : 0;
                res.set(i, a ^ b);
            }
            return res;
        }
        inline size_t diff_count(const Bytes& other) const {
            size_t cnt = 0;
            size_t max_sz = std::max(data->size(), other.data->size());
            for (size_t i = 0; i < max_sz; ++i) {
                uint8_t a = (i < data->size()) ? (*data)[i] : 0;
                uint8_t b = (i < other.data->size()) ? (*other.data)[i] : 0;
                if (a != b) cnt++;
            }
            return cnt;
        }
        inline size_t hamming_distance(const Bytes& other) const {
            return diff(other).popcount();
        }

        // =====================================================================
        // CHECKSUMS & HASHES
        // =====================================================================
        inline uint8_t xor_sum() const {
            uint8_t x = 0;
            for (uint8_t b : *data) x ^= b;
            return x;
        }
        inline uint64_t sum() const {
            uint64_t s = 0;
            for (uint8_t b : *data) s += b;
            return s;
        }
        inline uint8_t crc8(uint8_t poly = 0x07) const {
            uint8_t crc = 0x00;
            for (uint8_t b : *data) {
                crc ^= b;
                for (int i = 0; i < 8; ++i) {
                    if (crc & 0x80) crc = (crc << 1) ^ poly;
                    else crc <<= 1;
                }
            }
            return crc;
        }
        inline uint16_t crc16(uint16_t poly = 0xA001) const {
            uint16_t crc = 0xFFFF;
            for (uint8_t b : *data) {
                crc ^= b;
                for (int i = 0; i < 8; ++i) {
                    if (crc & 1) crc = (crc >> 1) ^ poly;
                    else crc >>= 1;
                }
            }
            return crc;
        }
        inline uint32_t crc32(uint32_t poly = 0xEDB88320) const {
            uint32_t crc = 0xFFFFFFFF;
            for (uint8_t b : *data) {
                crc ^= b;
                for (int i = 0; i < 8; ++i) {
                    if (crc & 1) crc = (crc >> 1) ^ poly;
                    else crc >>= 1;
                }
            }
            return ~crc;
        }

        // =====================================================================
        // INSPECTION & HEX DUMP
        // =====================================================================
        inline void dump() const {
            if (view_mode == MaskView::RAW) {
                dump_raw();
                return;
            }
            std::cout << "--- Buffer Dump (" << data->size() << " Bytes, cursor=" << cursor << ", view=hlv) ---" << std::endl;
            for (size_t i = 0; i < data->size(); i += 16) {
                std::printf("%08zx: ", i);
                for (size_t j = 0; j < 16; ++j) {
                    if (i + j < data->size()) {
                        std::printf("%02x ", (*data)[i + j]);
                    } else {
                        std::printf("   ");
                    }
                    if (j == 7) std::printf(" ");
                }
                std::printf(" |");
                for (size_t j = 0; j < 16 && i + j < data->size(); ++j) {
                    uint8_t c = (*data)[i + j];
                    std::printf("%c", (c >= 32 && c <= 126) ? (char)c : '.');
                }
                std::printf("|\n");
            }
        }
        inline void hexdump() const { dump(); }

        // =====================================================================
        // PATTERN SEARCH
        // =====================================================================
        inline int find_pattern(const std::vector<uint8_t>& pattern, size_t start_pos = 0) const {
            if (pattern.empty() || start_pos >= data->size()) return -1;
            for (size_t i = start_pos; i + pattern.size() <= data->size(); ++i) {
                bool match = true;
                for (size_t j = 0; j < pattern.size(); ++j) {
                    if ((*data)[i + j] != pattern[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) return (int)i;
            }
            return -1;
        }
        inline bool contains_pattern(const std::vector<uint8_t>& pattern) const {
            return find_pattern(pattern) != -1;
        }
        // =====================================================================
        // BYTE-MASK & PALETTE TOOLS (RGB, HSV, Presets, Color Tuning)
        // =====================================================================

        inline void set_rgb(int id, int r, int g, int b) {
            if (id >= 0 && id < 256) {
                size_t offset = (size_t)id * 3;
                if (offset + 2 >= data->size()) {
                    data->resize(offset + 3, 0);
                }
                (*data)[offset]     = (uint8_t)std::clamp(r, 0, 255);
                (*data)[offset + 1] = (uint8_t)std::clamp(g, 0, 255);
                (*data)[offset + 2] = (uint8_t)std::clamp(b, 0, 255);
            }
        }

        inline int get_r(int id) const {
            size_t offset = (size_t)id * 3;
            return (offset < data->size()) ? (int)(*data)[offset] : 0;
        }

        inline int get_g(int id) const {
            size_t offset = (size_t)id * 3 + 1;
            return (offset < data->size()) ? (int)(*data)[offset] : 0;
        }

        inline int get_b(int id) const {
            size_t offset = (size_t)id * 3 + 2;
            return (offset < data->size()) ? (int)(*data)[offset] : 0;
        }

        inline void set_hsv(int id, double h, double s, double v) {
            h = std::fmod(h, 360.0);
            if (h < 0) h += 360.0;
            if (s > 1.0) s /= 100.0;
            if (v > 1.0) v /= 100.0;
            s = std::clamp(s, 0.0, 1.0);
            v = std::clamp(v, 0.0, 1.0);

            double c = v * s;
            double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
            double m_val = v - c;

            double r1 = 0, g1 = 0, b1 = 0;
            if (h < 60)       { r1 = c; g1 = x; b1 = 0; }
            else if (h < 120) { r1 = x; g1 = c; b1 = 0; }
            else if (h < 180) { r1 = 0; g1 = c; b1 = x; }
            else if (h < 240) { r1 = 0; g1 = x; b1 = c; }
            else if (h < 300) { r1 = x; g1 = 0; b1 = c; }
            else              { r1 = c; g1 = 0; b1 = x; }

            int r = (int)std::round((r1 + m_val) * 255.0);
            int g = (int)std::round((g1 + m_val) * 255.0);
            int b = (int)std::round((b1 + m_val) * 255.0);
            set_rgb(id, r, g, b);
        }

        inline void gradient(int start_id, int end_id, int r1, int g1, int b1, int r2, int g2, int b2) {
            if (start_id > end_id) std::swap(start_id, end_id);
            int count = end_id - start_id;
            if (count <= 0) {
                set_rgb(start_id, r1, g1, b1);
                return;
            }
            for (int i = 0; i <= count; ++i) {
                double t = (double)i / (double)count;
                int r = (int)(r1 + (r2 - r1) * t);
                int g = (int)(g1 + (g2 - g1) * t);
                int b = (int)(b1 + (b2 - b1) * t);
                set_rgb(start_id + i, r, g, b);
            }
        }

        inline void gradient_hsv(int start_id, int end_id, double h1, double s1, double v1, double h2, double s2, double v2) {
            if (start_id > end_id) std::swap(start_id, end_id);
            int count = end_id - start_id;
            if (count <= 0) {
                set_hsv(start_id, h1, s1, v1);
                return;
            }
            for (int i = 0; i <= count; ++i) {
                double t = (double)i / (double)count;
                double h = h1 + (h2 - h1) * t;
                double s = s1 + (s2 - s1) * t;
                double v = v1 + (v2 - v1) * t;
                set_hsv(start_id + i, h, s, v);
            }
        }

        inline void preset(const std::string& name) {
            if (name == "tetris") {
                if (data->size() < 48) data->resize(48, 0);
                set_rgb(0,  18,  20,  28);  // 0: Board bg
                set_rgb(1,  0,   240, 240); // 1: I (Cyan)
                set_rgb(2,  33,  66,  230); // 2: J (Blue)
                set_rgb(3,  255, 140, 0);   // 3: L (Orange)
                set_rgb(4,  255, 220, 0);   // 4: O (Yellow)
                set_rgb(5,  30,  220, 30);  // 5: S (Green)
                set_rgb(6,  170, 0,   255); // 6: T (Purple)
                set_rgb(7,  240, 30,  30);  // 7: Z (Red)
                set_rgb(8,  95,  105, 125); // 8: Wall / Border (Steel)
                set_rgb(9,  40,  50,  68);  // 9: Ghost shadow
                set_rgb(10, 255, 255, 255); // 10: White text
                set_rgb(11, 255, 205, 45);  // 11: Gold accent
                set_rgb(12, 255, 255, 220); // 12: Flash clear
                set_rgb(13, 10,  12,  18);  // 13: UI dark slate
                set_rgb(14, 0,   210, 255); // 14: Sky cyan
                set_rgb(15, 225, 25,  45);  // 15: Game over red
            } else if (name == "gameboy") {
                if (data->size() < 12) data->resize(12, 0);
                set_rgb(0, 155, 188, 15);
                set_rgb(1, 139, 172, 15);
                set_rgb(2, 48,  98,  48);
                set_rgb(3, 15,  56,  15);
            } else if (name == "pico8") {
                if (data->size() < 48) data->resize(48, 0);
                set_rgb(0, 0,0,0);        set_rgb(1, 29,43,83);    set_rgb(2, 126,37,83);   set_rgb(3, 0,135,81);
                set_rgb(4, 171,82,54);    set_rgb(5, 95,87,79);    set_rgb(6, 194,195,199); set_rgb(7, 255,241,232);
                set_rgb(8, 255,0,77);     set_rgb(9, 255,163,0);   set_rgb(10, 255,236,39); set_rgb(11, 0,228,54);
                set_rgb(12, 41,173,255);  set_rgb(13, 131,118,156);set_rgb(14, 255,119,168);set_rgb(15, 255,204,170);
            } else if (name == "nes") {
                if (data->size() < 48) data->resize(48, 0);
                set_rgb(0,  92,  148, 252); set_rgb(1,  180, 70,  20);  set_rgb(2,  236, 30,  30);  set_rgb(3,  0,   68,  220);
                set_rgb(4,  252, 188, 176); set_rgb(5,  0,   168, 0);   set_rgb(6,  252, 216, 0);   set_rgb(7,  255, 255, 255);
                set_rgb(8,  0,   0,   0);   set_rgb(9,  120, 40,  10);  set_rgb(10, 0,   100, 0);   set_rgb(11, 240, 140, 0);
                set_rgb(12, 90,  90,  90);  set_rgb(13, 180, 180, 180); set_rgb(14, 140, 20,  20);  set_rgb(15, 15,  25,  70);
            } else if (name == "fire") {
                if (data->size() < 48) data->resize(48, 0);
                gradient(0, 4, 0, 0, 0, 180, 0, 0);
                gradient(4, 8, 180, 0, 0, 255, 120, 0);
                gradient(8, 12, 255, 120, 0, 255, 240, 0);
                gradient(12, 15, 255, 240, 0, 255, 255, 255);
            } else if (name == "cyberpunk") {
                if (data->size() < 48) data->resize(48, 0);
                set_rgb(0, 10, 10, 20);
                gradient(1, 5, 0, 240, 255, 255, 0, 128);
                gradient(6, 10, 255, 0, 128, 255, 230, 0);
                gradient(11, 15, 128, 0, 255, 255, 255, 255);
            } else if (name == "monochrome") {
                if (data->size() < 6) data->resize(6, 0);
                set_rgb(0, 0, 0, 0);
                set_rgb(1, 255, 255, 255);
            } else if (name == "neon") {
                if (data->size() < 48) data->resize(48, 0);
                set_rgb(0, 10, 5, 20);
                set_rgb(1, 255, 0, 128);   // Neon Pink
                set_rgb(2, 0, 255, 240);   // Neon Cyan
                set_rgb(3, 50, 255, 50);   // Neon Green
                set_rgb(4, 255, 240, 0);   // Neon Yellow
                set_rgb(5, 170, 0, 255);   // Neon Purple
                set_rgb(6, 255, 100, 0);   // Neon Orange
                set_rgb(7, 0, 150, 255);   // Deep Neon Sky
                set_rgb(8, 80, 80, 100);   // Gray
                set_rgb(9, 30, 20, 50);    // Shadow
                set_rgb(10, 255, 255, 255);// White
            } else if (name == "c64") {
                if (data->size() < 48) data->resize(48, 0);
                set_rgb(0, 0,0,0);       set_rgb(1, 255,255,255); set_rgb(2, 136,0,0);     set_rgb(3, 170,255,238);
                set_rgb(4, 204,68,204);  set_rgb(5, 0,204,85);    set_rgb(6, 0,0,170);     set_rgb(7, 238,238,119);
                set_rgb(8, 221,136,85);  set_rgb(9, 102,68,0);    set_rgb(10, 255,119,119);set_rgb(11, 51,51,51);
                set_rgb(12, 119,119,119);set_rgb(13, 170,255,102);set_rgb(14, 0,136,255); set_rgb(15, 187,187,187);
            } else if (name == "matrix") {
                if (data->size() < 48) data->resize(48, 0);
                set_rgb(0, 0, 0, 0);
                gradient(1, 14, 0, 30, 0, 0, 255, 70);
                set_rgb(15, 220, 255, 220);
            } else if (name == "lava") {
                if (data->size() < 48) data->resize(48, 0);
                gradient(0, 5, 20, 0, 0, 180, 20, 0);
                gradient(6, 11, 180, 20, 0, 255, 180, 0);
                gradient(12, 15, 255, 180, 0, 255, 255, 200);
            } else if (name == "pastel") {
                if (data->size() < 48) data->resize(48, 0);
                set_rgb(0, 250, 245, 240);
                set_rgb(1, 255, 179, 186); set_rgb(2, 255, 223, 186);
                set_rgb(3, 255, 255, 186); set_rgb(4, 186, 255, 201);
                set_rgb(5, 186, 225, 255); set_rgb(6, 218, 186, 255);
                set_rgb(7, 255, 186, 243); set_rgb(8, 180, 180, 190);
                set_rgb(9, 60, 60, 75);    set_rgb(10, 255, 255, 255);
            }
        }

        inline void fade(double factor) {
            factor = std::clamp(factor, 0.0, 1.0);
            for (size_t i = 0; i < data->size(); ++i) {
                (*data)[i] = (uint8_t)((*data)[i] * factor);
            }
        }

        inline void brightness(int delta) {
            for (size_t i = 0; i < data->size(); ++i) {
                (*data)[i] = (uint8_t)std::clamp((int)(*data)[i] + delta, 0, 255);
            }
        }

        inline void contrast(double factor) {
            for (size_t i = 0; i < data->size(); ++i) {
                double val = (double)(*data)[i];
                double adjusted = 128.0 + (val - 128.0) * factor;
                (*data)[i] = (uint8_t)std::clamp((int)adjusted, 0, 255);
            }
        }

        inline void grayscale() {
            for (size_t i = 0; i + 2 < data->size(); i += 3) {
                int r = (*data)[i];
                int g = (*data)[i + 1];
                int b = (*data)[i + 2];
                uint8_t gray = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
                (*data)[i] = gray;
                (*data)[i + 1] = gray;
                (*data)[i + 2] = gray;
            }
        }

        inline int nearest(int r, int g, int b) const {
            int best_id = 0;
            double min_dist = 1e9;
            size_t total_colors = data->size() / 3;
            for (size_t id = 0; id < total_colors; ++id) {
                size_t offset = id * 3;
                int cr = (*data)[offset];
                int cg = (*data)[offset + 1];
                int cb = (*data)[offset + 2];
                double dr = (cr - r) * 0.30;
                double dg = (cg - g) * 0.59;
                double db = (cb - b) * 0.11;
                double dist = dr * dr + dg * dg + db * db;
                if (dist < min_dist) {
                    min_dist = dist;
                    best_id = (int)id;
                }
            }
            return best_id;
        }

        inline void shift(int dr, int dg, int db) {
            for (size_t i = 0; i + 2 < data->size(); i += 3) {
                (*data)[i]     = (uint8_t)std::clamp((int)(*data)[i] + dr, 0, 255);
                (*data)[i + 1] = (uint8_t)std::clamp((int)(*data)[i + 1] + dg, 0, 255);
                (*data)[i + 2] = (uint8_t)std::clamp((int)(*data)[i + 2] + db, 0, 255);
            }
        }

        inline void lerp(const Bytes& target, double t) {
            t = std::clamp(t, 0.0, 1.0);
            size_t max_sz = std::max(data->size(), target.data->size());
            if (data->size() < max_sz) data->resize(max_sz, 0);
            for (size_t i = 0; i < max_sz; ++i) {
                uint8_t va = (*data)[i];
                uint8_t vb = (i < target.data->size()) ? (*target.data)[i] : 0;
                (*data)[i] = (uint8_t)(va + (vb - va) * t);
            }
        }

        inline int color_count() const {
            return (int)(data->size() / 3);
        }
        inline int colors() const {
            return color_count();
        }

        inline void apply() const {
            if (retrotech_bridge::apply_colormask_fn) {
                retrotech_bridge::apply_colormask_fn(data->data(), data->size());
            }
        }

        // =====================================================================
        // BOUND GRID & MATRIX EDITING
        // =====================================================================
        inline void grid_set(std::initializer_list<std::pair<int, int>> specs, int w = 0, int h = 0) {
            grid.set_specs(specs, w, h);
        }
        inline void grid_edit(const std::vector<std::vector<int>>& rows) {
            size_t total_items = 0;
            for (const auto& r : rows) total_items += r.size();
            if (data->size() < total_items) data->resize(total_items, 0);
            size_t offset = 0;
            for (const auto& r : rows) {
                for (int val : r) {
                    if (offset < data->size()) (*data)[offset++] = (uint8_t)(val & 0xFF);
                }
            }
            grid.grid_edit(rows);
        }
        inline void grid_print() const { grid.grid_print(); }
        inline void grid_clear() {
            for (auto& b : *data) b = 0;
            grid.grid_clear();
        }
        inline void grid_fill(int val) {
            for (auto& b : *data) b = (uint8_t)(val & 0xFF);
            grid.grid_fill(val);
        }
        inline void grid_invert() {
            for (auto& b : *data) b = (uint8_t)(~b);
            grid.grid_invert();
        }
        inline void grid_resize(int w, int h) { grid.resize_grid(w, h); }
        inline int grid_get(int x, int y) const { return grid.get(0, x, y); }
        inline int grid_get(size_t layer, int x, int y) const { return grid.get(layer, x, y); }
        inline void grid_set_cell(int x, int y, int val) { grid.set(0, x, y, val); }
        inline void grid_set_cell(size_t layer, int x, int y, int val) { grid.set(layer, x, y, val); }
        inline int operator()(int x, int y) const { return grid.get(0, x, y); }
        inline int operator()(size_t layer, int x, int y) const { return grid.get(layer, x, y); }

        // =====================================================================
        // SPATIAL 2D MASKS, SHAPES & STENCILS
        // =====================================================================

        inline uint8_t get_2d(int x, int y, int pitch) const {
            if (x < 0 || y < 0 || pitch <= 0) return 0;
            size_t idx = (size_t)(y * pitch + x);
            return (idx < data->size()) ? (*data)[idx] : 0;
        }

        inline void set_2d(int x, int y, int pitch, uint8_t val) {
            if (x < 0 || y < 0 || pitch <= 0) return;
            size_t idx = (size_t)(y * pitch + x);
            if (idx >= data->size()) data->resize(idx + 1, 0);
            (*data)[idx] = val;
        }

        inline void rect_2d(int w, int h, int x, int y, int rw, int rh, uint8_t val) {
            for (int r = 0; r < rh; ++r) {
                int py = y + r;
                if (py < 0 || py >= h) continue;
                for (int c = 0; c < rw; ++c) {
                    int px = x + c;
                    if (px < 0 || px >= w) continue;
                    set_2d(px, py, w, val);
                }
            }
        }
        inline void rect(int x, int y, int rw, int rh, uint8_t val, int pitch) {
            int h = (int)(data->size() / (size_t)pitch);
            rect_2d(pitch, h, x, y, rw, rh, val);
        }

        inline void circle_2d(int w, int h, int cx, int cy, int radius, uint8_t val) {
            int r2 = radius * radius;
            for (int dy = -radius; dy <= radius; ++dy) {
                int py = cy + dy;
                if (py < 0 || py >= h) continue;
                for (int dx = -radius; dx <= radius; ++dx) {
                    int px = cx + dx;
                    if (px < 0 || px >= w) continue;
                    if (dx * dx + dy * dy <= r2) {
                        set_2d(px, py, w, val);
                    }
                }
            }
        }
        inline void circle(int cx, int cy, int radius, uint8_t val, int pitch) {
            int h = (int)(data->size() / (size_t)pitch);
            circle_2d(pitch, h, cx, cy, radius, val);
        }

        inline void line_2d(int w, int h, int x0, int y0, int x1, int y1, uint8_t val) {
            int dx = std::abs(x1 - x0);
            int sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0);
            int sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            while (true) {
                if (x0 >= 0 && x0 < w && y0 >= 0 && y0 < h) {
                    set_2d(x0, y0, w, val);
                }
                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
        }
        inline void line(int x0, int y0, int x1, int y1, uint8_t val, int pitch) {
            int h = (int)(data->size() / (size_t)pitch);
            line_2d(pitch, h, x0, y0, x1, y1, val);
        }

        inline void threshold(int thresh, uint8_t high_val = 1, uint8_t low_val = 0) {
            for (size_t i = 0; i < data->size(); ++i) {
                (*data)[i] = ((*data)[i] >= (uint8_t)thresh) ? high_val : low_val;
            }
        }

        inline void apply_stencil(const Bytes& stencil, int pass_id = 1) {
            size_t limit = std::min(data->size(), stencil.data->size());
            for (size_t i = 0; i < limit; ++i) {
                if ((*stencil.data)[i] != (uint8_t)pass_id) {
                    (*data)[i] = 0;
                }
            }
        }

        inline void blend(const Bytes& other, const Bytes& alpha_mask) {
            size_t limit = std::min({data->size(), other.data->size(), alpha_mask.data->size()});
            for (size_t i = 0; i < limit; ++i) {
                int a = (int)(*alpha_mask.data)[i];
                int v1 = (int)(*data)[i];
                int v2 = (int)(*other.data)[i];
                (*data)[i] = (uint8_t)((v1 * (255 - a) + v2 * a) / 255);
            }
        }

        inline void scroll_2d(int dx, int dy, int w, int h) {
            if (w <= 0 || h <= 0 || data->size() < (size_t)(w * h)) return;
            std::vector<uint8_t> temp = *data;
            dx = ((dx % w) + w) % w;
            dy = ((dy % h) + h) % h;
            for (int y = 0; y < h; ++y) {
                int ny = (y + dy) % h;
                for (int x = 0; x < w; ++x) {
                    int nx = (x + dx) % w;
                    (*data)[ny * w + nx] = temp[y * w + x];
                }
            }
        }

        inline void flip_h(int w, int h) {
            if (w <= 0 || h <= 0) return;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w / 2; ++x) {
                    std::swap((*data)[y * w + x], (*data)[y * w + (w - 1 - x)]);
                }
            }
        }

        inline void flip_v(int w, int h) {
            if (w <= 0 || h <= 0) return;
            for (int y = 0; y < h / 2; ++y) {
                for (int x = 0; x < w; ++x) {
                    std::swap((*data)[y * w + x], (*data)[(h - 1 - y) * w + x]);
                }
            }
        }

        // =====================================================================
        // COLLISION & RAYCASTING
        // =====================================================================

        inline bool collides_2d(int ax, int ay, int aw, int ah,
                                const Bytes& b, int bx, int by, int bw, int bh,
                                int transparent_id = 0) const {
            int ix1 = std::max(ax, bx);
            int iy1 = std::max(ay, by);
            int ix2 = std::min(ax + aw, bx + bw);
            int iy2 = std::min(ay + ah, by + bh);
            if (ix1 >= ix2 || iy1 >= iy2) return false;
            for (int y = iy1; y < iy2; ++y) {
                for (int x = ix1; x < ix2; ++x) {
                    size_t a_idx = (size_t)((y - ay) * aw + (x - ax));
                    size_t b_idx = (size_t)((y - by) * bw + (x - bx));
                    if (a_idx < data->size() && b_idx < b.data->size()) {
                        uint8_t val_a = (*data)[a_idx];
                        uint8_t val_b = (*b.data)[b_idx];
                        if (val_a != (uint8_t)transparent_id && val_b != (uint8_t)transparent_id) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        inline double raycast_2d(int w, int h, double x0, double y0, double dir_x, double dir_y, double max_dist, int solid_id = 1) const {
            double len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
            if (len == 0.0) return -1.0;
            dir_x /= len;
            dir_y /= len;
            double step = 0.5;
            for (double d = 0.0; d <= max_dist; d += step) {
                int px = (int)std::floor(x0 + dir_x * d);
                int py = (int)std::floor(y0 + dir_y * d);
                if (px < 0 || px >= w || py < 0 || py >= h) return -1.0;
                size_t idx = (size_t)(py * w + px);
                if (idx < data->size() && (*data)[idx] == (uint8_t)solid_id) {
                    return d;
                }
            }
            return -1.0;
        }

        // =====================================================================
        // BITWISE & BUFFER MANIPULATION
        // =====================================================================

        inline void invert() {
            for (size_t i = 0; i < data->size(); ++i) {
                (*data)[i] = ~(*data)[i];
            }
        }

        inline void and_with(const Bytes& other) {
            size_t limit = std::min(data->size(), other.data->size());
            for (size_t i = 0; i < limit; ++i) (*data)[i] &= (*other.data)[i];
        }

        inline void or_with(const Bytes& other) {
            size_t limit = std::min(data->size(), other.data->size());
            for (size_t i = 0; i < limit; ++i) (*data)[i] |= (*other.data)[i];
        }

        inline void xor_with(const Bytes& other) {
            size_t limit = std::min(data->size(), other.data->size());
            for (size_t i = 0; i < limit; ++i) (*data)[i] ^= (*other.data)[i];
        }

        inline Bytes operator&(const Bytes& other) const {
            size_t limit = std::min(data->size(), other.data->size());
            Bytes res(limit, 0);
            for (size_t i = 0; i < limit; ++i) res[i] = (*data)[i] & (*other.data)[i];
            return res;
        }

        inline Bytes operator|(const Bytes& other) const {
            size_t limit = std::max(data->size(), other.data->size());
            Bytes res(limit, 0);
            for (size_t i = 0; i < limit; ++i) {
                uint8_t a = (i < data->size()) ? (*data)[i] : 0;
                uint8_t b = (i < other.data->size()) ? (*other.data)[i] : 0;
                res[i] = a | b;
            }
            return res;
        }

        inline Bytes operator^(const Bytes& other) const {
            size_t limit = std::max(data->size(), other.data->size());
            Bytes res(limit, 0);
            for (size_t i = 0; i < limit; ++i) {
                uint8_t a = (i < data->size()) ? (*data)[i] : 0;
                uint8_t b = (i < other.data->size()) ? (*other.data)[i] : 0;
                res[i] = a ^ b;
            }
            return res;
        }

        inline Bytes operator~() const {
            Bytes res(data->size(), 0);
            for (size_t i = 0; i < data->size(); ++i) res[i] = ~(*data)[i];
            return res;
        }

        inline int count_matching(uint8_t val) const {
            int cnt = 0;
            for (size_t i = 0; i < data->size(); ++i) {
                if ((*data)[i] == val) cnt++;
            }
            return cnt;
        }
        inline int count(uint8_t val) const {
            return count_matching(val);
        }

        inline int find_first(uint8_t val) const {
            for (size_t i = 0; i < data->size(); ++i) {
                if ((*data)[i] == val) return (int)i;
            }
            return -1;
        }
        inline int find(uint8_t val) const {
            return find_first(val);
        }

        inline void replace(uint8_t old_val, uint8_t new_val) {
            for (size_t i = 0; i < data->size(); ++i) {
                if ((*data)[i] == old_val) (*data)[i] = new_val;
            }
        }

        inline void clamp_all(uint8_t min_v, uint8_t max_v) {
            for (size_t i = 0; i < data->size(); ++i) {
                (*data)[i] = std::clamp((*data)[i], min_v, max_v);
            }
        }

        inline void reverse() {
            std::reverse(data->begin(), data->end());
        }

        // =====================================================================
        // SERIALIZATION & FILE I/O
        // =====================================================================

        inline void load_hex_str(const std::string& hex_str) {
            std::vector<uint8_t> bytes;
            std::string cur = "";
            for (char ch : hex_str) {
                if (std::isxdigit((unsigned char)ch)) {
                    cur += ch;
                    if (cur.size() == 2) {
                        bytes.push_back((uint8_t)std::strtol(cur.c_str(), nullptr, 16));
                        cur.clear();
                    }
                } else if (!cur.empty()) {
                    bytes.push_back((uint8_t)std::strtol(cur.c_str(), nullptr, 16));
                    cur.clear();
                }
            }
            if (!cur.empty()) bytes.push_back((uint8_t)std::strtol(cur.c_str(), nullptr, 16));
            *data = bytes;
        }

        inline bool save_hex(const std::string& path) const {
            std::ofstream out(path);
            if (!out.is_open()) return false;
            out << to_hex() << "\n";
            return true;
        }

        inline bool load_hex(const std::string& path) {
            std::ifstream in(path);
            if (!in.is_open()) return false;
            std::stringstream buffer;
            buffer << in.rdbuf();
            from_hex(buffer.str());
            return true;
        }

        inline bool save_bin(const std::string& path) const {
            std::ofstream out(path, std::ios::binary);
            if (!out.is_open()) return false;
            out.write((const char*)data->data(), data->size());
            return true;
        }

        inline bool load_bin(const std::string& path) {
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in.is_open()) return false;
            std::streamsize sz = in.tellg();
            in.seekg(0, std::ios::beg);
            data->resize((size_t)sz);
            in.read((char*)data->data(), sz);
            return true;
        }

        inline uint8_t* raw() { return data->data(); }
        inline const uint8_t* raw() const { return data->data(); }
        inline std::shared_ptr<std::vector<uint8_t>> get_data() const { return data; }
    };

    using Bytemask = Bytes;
    using Colormask = Bytes;
    using ByteMask = Bytes;
    using ColorMask = Bytes;

    inline Bytes slice(const Bytes& b, Int start, Int end) {
        Int n = (Int)b.size();
        if (start < 0) start = n + start;
        if (end < 0) end = n + end;
        if (start < 0) start = 0;
        if (end > n) end = n;
        if (start >= end) return Bytes(0);
        return b.slice((size_t)start, (size_t)(end - start));
    }

    // Bits: Compact Bitfield Buffer (Real Physical Hardware Bit Storage)
    class Bits : public Bytes {
    public:
        struct BitRef {
            Bits& owner;
            size_t idx;

            BitRef(Bits& o, size_t i) : owner(o), idx(i) {}

            inline operator bool() const;
            inline operator Int() const;

            inline BitRef& operator=(bool val);
            inline BitRef& operator=(Int val);
            inline BitRef& operator=(int val);
            inline BitRef& operator=(const BitRef& other);

            friend inline std::ostream& operator<<(std::ostream& os, const BitRef& br) {
                os << (bool(br) ? 1 : 0);
                return os;
            }
        };

        static const size_t DEFAULT_MAX_BITS = 8192; // 8192 bits = 1024 bytes (1 KB)
        size_t total_bits;

        Bits(size_t num_bits = DEFAULT_MAX_BITS) 
            : Bytes((num_bits + 7) / 8, 0), total_bits(num_bits) {}

        Bits(std::initializer_list<int> init)
            : Bytes((init.size() + 7) / 8, 0), total_bits(init.size()) {
            size_t idx = 0;
            for (auto v : init) {
                set(idx++, v != 0);
            }
        }

        Bits(const Str& bit_str)
            : Bytes((bit_str.size() + 7) / 8, 0), total_bits(bit_str.size()) {
            for (size_t i = 0; i < bit_str.size(); ++i) {
                set(i, bit_str[i] == '1');
            }
        }

        inline BitRef operator[](size_t bit_idx) {
            return BitRef(*this, bit_idx);
        }
        inline bool operator[](size_t bit_idx) const {
            return get(bit_idx);
        }

        inline bool get(size_t bit_idx) const {
            if (bit_idx >= total_bits) return false;
            return get_bit(bit_idx / 8, (uint8_t)(bit_idx % 8));
        }

        inline void set(size_t bit_idx, bool val) {
            if (bit_idx >= total_bits) {
                total_bits = bit_idx + 1;
                resize((total_bits + 7) / 8, 0);
            }
            set_bit(bit_idx / 8, (uint8_t)(bit_idx % 8), val);
        }

        inline void toggle(size_t bit_idx) {
            set(bit_idx, !get(bit_idx));
        }

        inline void fill_all(bool val) {
            fill(val ? 0xFF : 0x00);
        }

        inline void invert() {
            uint8_t* p = raw();
            size_t b_count = (total_bits + 7) / 8;
            for (size_t i = 0; i < b_count; ++i) {
                p[i] = ~p[i];
            }
        }

        inline Int count_ones() const {
            Int count = 0;
            const uint8_t* p = raw();
            size_t full_bytes = total_bits / 8;
            for (size_t i = 0; i < full_bytes; ++i) {
                #if defined(__GNUC__) || defined(__clang__)
                count += __builtin_popcount(p[i]);
                #else
                uint8_t b = p[i];
                while (b) { count += (b & 1); b >>= 1; }
                #endif
            }
            for (size_t i = full_bytes * 8; i < total_bits; ++i) {
                if (get(i)) count++;
            }
            return count;
        }

        inline Int count_zeros() const {
            return (Int)total_bits - count_ones();
        }

        inline size_t size() const { return total_bits; }
        inline Int get_len() const { return (Int)total_bits; }
        inline size_t bit_size() const { return total_bits; }
        inline size_t byte_size() const { return (total_bits + 7) / 8; }
        inline size_t memory_size() const { return byte_size(); }

        // 2D Bitfield helper
        inline bool get_2d(size_t x, size_t y, size_t pitch) const {
            return get(y * pitch + x);
        }
        inline void set_2d(size_t x, size_t y, size_t pitch, bool val) {
            set(y * pitch + x, val);
        }

        // Bitwise operations
        inline Bits operator&(const Bits& other) const {
            size_t res_bits = std::min(total_bits, other.total_bits);
            Bits res(res_bits);
            size_t bytes_to_copy = (res_bits + 7) / 8;
            const uint8_t* a = raw();
            const uint8_t* b = other.raw();
            uint8_t* r = res.raw();
            for (size_t i = 0; i < bytes_to_copy; ++i) {
                r[i] = a[i] & b[i];
            }
            return res;
        }

        inline Bits operator|(const Bits& other) const {
            size_t res_bits = std::max(total_bits, other.total_bits);
            Bits res(res_bits);
            size_t a_bytes = byte_size();
            size_t b_bytes = other.byte_size();
            const uint8_t* a = raw();
            const uint8_t* b = other.raw();
            uint8_t* r = res.raw();
            for (size_t i = 0; i < res.byte_size(); ++i) {
                uint8_t val_a = (i < a_bytes) ? a[i] : 0;
                uint8_t val_b = (i < b_bytes) ? b[i] : 0;
                r[i] = val_a | val_b;
            }
            return res;
        }

        inline Bits operator^(const Bits& other) const {
            size_t res_bits = std::max(total_bits, other.total_bits);
            Bits res(res_bits);
            size_t a_bytes = byte_size();
            size_t b_bytes = other.byte_size();
            const uint8_t* a = raw();
            const uint8_t* b = other.raw();
            uint8_t* r = res.raw();
            for (size_t i = 0; i < res.byte_size(); ++i) {
                uint8_t val_a = (i < a_bytes) ? a[i] : 0;
                uint8_t val_b = (i < b_bytes) ? b[i] : 0;
                r[i] = val_a ^ val_b;
            }
            return res;
        }

        inline Bits operator~() const {
            Bits res = *this;
            res.invert();
            return res;
        }

        inline void bit_and(const Bits& other) { *this = *this & other; }
        inline void bit_or(const Bits& other) { *this = *this | other; }
        inline void bit_xor(const Bits& other) { *this = *this ^ other; }
        inline void bit_not() { invert(); }

        // Fast 1-bit shifting / scrolling
        inline void shift_left(size_t shift) {
            if (shift >= total_bits) { fill_all(false); return; }
            for (size_t i = 0; i + shift < total_bits; ++i) {
                set(i, get(i + shift));
            }
            for (size_t i = total_bits - shift; i < total_bits; ++i) {
                set(i, false);
            }
        }

        inline void shift_right(size_t shift) {
            if (shift >= total_bits) { fill_all(false); return; }
            for (size_t i = total_bits; i > shift; --i) {
                set(i - 1, get(i - 1 - shift));
            }
            for (size_t i = 0; i < shift; ++i) {
                set(i, false);
            }
        }

        // 2D Scroll / Parallax
        inline void scroll_x(int dx, int width, int height) {
            if (width <= 0 || height <= 0) return;
            Bits copy = *this;
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int src_x = (x - dx) % width;
                    if (src_x < 0) src_x += width;
                    set(y * width + x, copy.get(y * width + src_x));
                }
            }
        }

        inline void scroll_y(int dy, int width, int height) {
            if (width <= 0 || height <= 0) return;
            Bits copy = *this;
            for (int y = 0; y < height; ++y) {
                int src_y = (y - dy) % height;
                if (src_y < 0) src_y += height;
                for (int x = 0; x < width; ++x) {
                    set(y * width + x, copy.get(src_y * width + x));
                }
            }
        }

        inline Str to_bit_string() const {
            std::string s;
            s.reserve(total_bits);
            for (size_t i = 0; i < total_bits; ++i) {
                s.push_back(get(i) ? '1' : '0');
            }
            return s;
        }
    };

    inline Bits::BitRef::operator bool() const {
        return owner.get(idx);
    }
    inline Bits::BitRef::operator Int() const {
        return owner.get(idx) ? 1 : 0;
    }
    inline Bits::BitRef& Bits::BitRef::operator=(bool val) {
        owner.set(idx, val);
        return *this;
    }
    inline Bits::BitRef& Bits::BitRef::operator=(Int val) {
        owner.set(idx, val != 0);
        return *this;
    }
    inline Bits::BitRef& Bits::BitRef::operator=(int val) {
        owner.set(idx, val != 0);
        return *this;
    }
    inline Bits::BitRef& Bits::BitRef::operator=(const BitRef& other) {
        owner.set(idx, (bool)other);
        return *this;
    }

    // Hybrid: Hardware-Level Hybrid Memory Buffer (N Bytes + M Bits)
    // Example: bytes, 3, bits, 4 -> 7 indexable cells:
    // cells 0..2 are bytes (0..255)
    // cells 3..6 are bits (0..1)
    // Physical RAM footprint: 3 bytes + 1 byte for bits = 4 bytes total!
    class Hybrid {
    public:
        struct HybridRef {
            Hybrid& owner;
            size_t idx;

            HybridRef(Hybrid& o, size_t i) : owner(o), idx(i) {}

            inline operator Int() const;
            inline operator bool() const;
            inline HybridRef& operator=(Int val);
            inline HybridRef& operator=(int val);
            inline HybridRef& operator=(bool val);
            inline HybridRef& operator=(const HybridRef& other);

            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            inline bool operator==(T val) const { return (Int)*this == (Int)val; }
            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            inline bool operator!=(T val) const { return (Int)*this != (Int)val; }
            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            inline bool operator<(T val) const { return (Int)*this < (Int)val; }
            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            inline bool operator<=(T val) const { return (Int)*this <= (Int)val; }
            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            inline bool operator>(T val) const { return (Int)*this > (Int)val; }
            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            inline bool operator>=(T val) const { return (Int)*this >= (Int)val; }

            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            friend inline bool operator==(T val, const HybridRef& hr) { return (Int)val == (Int)hr; }
            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            friend inline bool operator!=(T val, const HybridRef& hr) { return (Int)val != (Int)hr; }
            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            friend inline bool operator<(T val, const HybridRef& hr) { return (Int)val < (Int)hr; }
            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            friend inline bool operator<=(T val, const HybridRef& hr) { return (Int)val <= (Int)hr; }
            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            friend inline bool operator>(T val, const HybridRef& hr) { return (Int)val > (Int)hr; }
            template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
            friend inline bool operator>=(T val, const HybridRef& hr) { return (Int)val >= (Int)hr; }

            friend inline std::ostream& operator<<(std::ostream& os, const HybridRef& hr) {
                os << (Int)hr;
                return os;
            }
        };

        Grid grid;
        size_t num_bytes;
        size_t num_bits;
        Bytes byte_data;
        Bits bit_data;

        Hybrid(size_t n_bytes = 0, size_t n_bits = 0)
            : num_bytes(n_bytes), num_bits(n_bits),
              byte_data(n_bytes, 0),
              bit_data(n_bits) {}

        inline size_t size() const { return num_bytes + num_bits; }
        inline Int get_len() const { return (Int)size(); }
        inline size_t byte_count() const { return num_bytes; }
        inline size_t bit_count() const { return num_bits; }
        inline size_t memory_size() const {
            return num_bytes + bit_data.byte_size();
        }
        inline size_t byte_size() const { return memory_size(); }

        inline bool is_bit(size_t index) const {
            return index >= num_bytes;
        }

        inline Int get(size_t index) const {
            if (index < num_bytes) {
                return (Int)byte_data.get(index);
            } else if (index < num_bytes + num_bits) {
                return bit_data.get(index - num_bytes) ? 1 : 0;
            }
            return 0;
        }

        inline void set(size_t index, Int val) {
            if (index < num_bytes) {
                byte_data.set(index, (uint8_t)(val & 0xFF));
            } else if (index < num_bytes + num_bits) {
                bit_data.set(index - num_bytes, val != 0);
            } else {
                size_t needed_bytes = index + 1 - num_bits;
                if (needed_bytes > num_bytes) {
                    num_bytes = needed_bytes;
                    byte_data.resize(num_bytes);
                }
                byte_data.set(index - num_bits, (uint8_t)(val & 0xFF));
            }
        }

        // =====================================================================
        // BOUND GRID & MATRIX EDITING
        // =====================================================================
        inline void grid_set(std::initializer_list<std::pair<int, int>> specs, int w = 0, int h = 0) {
            grid.set_specs(specs, w, h);
        }
        inline void grid_edit(const std::vector<std::vector<int>>& rows) {
            size_t idx = 0;
            for (const auto& r : rows) {
                for (int val : r) {
                    set(idx++, val);
                }
            }
            grid.grid_edit(rows);
        }
        inline void grid_print() const { grid.grid_print(); }
        inline void grid_clear() {
            for (size_t i = 0; i < size(); ++i) set(i, 0);
            grid.grid_clear();
        }
        inline void grid_fill(int val) {
            for (size_t i = 0; i < size(); ++i) set(i, val);
            grid.grid_fill(val);
        }
        inline void grid_invert() {
            for (size_t i = 0; i < size(); ++i) {
                if (is_bit(i)) set(i, get(i) == 0 ? 1 : 0);
                else set(i, (Int)((uint8_t)~((uint8_t)get(i))));
            }
            grid.grid_invert();
        }
        inline void grid_resize(int w, int h) { grid.resize_grid(w, h); }
        inline int grid_get(int x, int y) const { return grid.get(0, x, y); }
        inline int grid_get(size_t layer, int x, int y) const { return grid.get(layer, x, y); }
        inline void grid_set_cell(int x, int y, int val) { grid.set(0, x, y, val); }
        inline void grid_set_cell(size_t layer, int x, int y, int val) { grid.set(layer, x, y, val); }
        inline int operator()(int x, int y) const { return grid.get(0, x, y); }
        inline int operator()(size_t layer, int x, int y) const { return grid.get(layer, x, y); }

        inline HybridRef operator[](size_t index) {
            return HybridRef(*this, index);
        }
        inline Int operator[](size_t index) const {
            return get(index);
        }

        // =====================================================================
        // BINARY STREAM CURSOR, WRITE & READ (TEXT, INTEGERS, DECIMALS, BYTES)
        // =====================================================================
        inline void seek(size_t pos) { byte_data.seek(pos); }
        inline size_t tell() const { return byte_data.tell(); }
        inline void rewind() { byte_data.rewind(); }
        inline bool eof() const { return byte_data.eof(); }
        inline size_t remaining() const { return byte_data.remaining(); }

        inline void write_u8(uint8_t val, int offset = -1) { byte_data.write_u8(val, offset); }
        inline void write_i8(int8_t val, int offset = -1) { byte_data.write_i8(val, offset); }
        inline void write_byte(uint8_t val, int offset = -1) { byte_data.write_byte(val, offset); }
        inline void write_u16(uint16_t val, int offset = -1) { byte_data.write_u16(val, offset); }
        inline void write_i16(int16_t val, int offset = -1) { byte_data.write_i16(val, offset); }
        inline void write_u32(uint32_t val, int offset = -1) { byte_data.write_u32(val, offset); }
        inline void write_i32(int32_t val, int offset = -1) { byte_data.write_i32(val, offset); }
        inline void write_u64(uint64_t val, int offset = -1) { byte_data.write_u64(val, offset); }
        inline void write_i64(int64_t val, int offset = -1) { byte_data.write_i64(val, offset); }
        inline void write_int(Int val, int offset = -1) { byte_data.write_int(val, offset); }
        inline void write_dec(Dec val, int offset = -1) { byte_data.write_dec(val, offset); }
        inline void write_float(float val, int offset = -1) { byte_data.write_float(val, offset); }
        inline void write_double(double val, int offset = -1) { byte_data.write_double(val, offset); }
        inline void write_bool(bool val, int offset = -1) { byte_data.write_bool(val, offset); }
        inline void write_str(const Str& s, int offset = -1, bool null_term = true) { byte_data.write_str(s, offset, null_term); }
        inline void write_string(const Str& s, int offset = -1, bool null_term = true) { byte_data.write_string(s, offset, null_term); }
        inline void write_bytes(const Bytes& other, int offset = -1) { byte_data.write_bytes(other, offset); }

        inline uint8_t read_u8(int offset = -1) { return byte_data.read_u8(offset); }
        inline int8_t read_i8(int offset = -1) { return byte_data.read_i8(offset); }
        inline uint8_t read_byte(int offset = -1) { return byte_data.read_byte(offset); }
        inline uint16_t read_u16(int offset = -1) { return byte_data.read_u16(offset); }
        inline int16_t read_i16(int offset = -1) { return byte_data.read_i16(offset); }
        inline uint32_t read_u32(int offset = -1) { return byte_data.read_u32(offset); }
        inline int32_t read_i32(int offset = -1) { return byte_data.read_i32(offset); }
        inline uint64_t read_u64(int offset = -1) { return byte_data.read_u64(offset); }
        inline int64_t read_i64(int offset = -1) { return byte_data.read_i64(offset); }
        inline Int read_int(int offset = -1) { return byte_data.read_int(offset); }
        inline Dec read_dec(int offset = -1) { return byte_data.read_dec(offset); }
        inline float read_float(int offset = -1) { return byte_data.read_float(offset); }
        inline double read_double(int offset = -1) { return byte_data.read_double(offset); }
        inline bool read_bool(int offset = -1) { return byte_data.read_bool(offset); }
        inline Str read_str(int len = -1, int offset = -1) { return byte_data.read_str(len, offset); }
        inline Str read_string(int len = -1, int offset = -1) { return byte_data.read_string(len, offset); }
        inline Bytes read_bytes(size_t len, int offset = -1) { return byte_data.read_bytes(len, offset); }

        inline void dump() const { byte_data.dump(); }
        inline void hexdump() const { byte_data.hexdump(); }
        inline uint32_t crc32() const { return byte_data.crc32(); }
        inline uint16_t crc16() const { return byte_data.crc16(); }
        inline uint8_t crc8() const { return byte_data.crc8(); }
        inline uint64_t get_bitfield(size_t start, uint8_t count) const { return byte_data.get_bitfield(start, count); }
        inline void set_bitfield(size_t start, uint8_t count, uint64_t val) { byte_data.set_bitfield(start, count, val); }
        inline void fill(uint8_t val) { byte_data.fill(val); }
        inline void clear() { byte_data.clear(); bit_data.clear(); }
        inline void invert() { byte_data.invert(); bit_data.invert(); }

        inline Str to_string() const {
            std::stringstream ss;
            ss << "[Hybrid: " << num_bytes << " Bytes, " << num_bits << " Bits (RAM: " << memory_size() << " B)]";
            return ss.str();
        }
    };

    inline Hybrid::HybridRef::operator Int() const {
        return owner.get(idx);
    }
    inline Hybrid::HybridRef::operator bool() const {
        return owner.get(idx) != 0;
    }
    inline Hybrid::HybridRef& Hybrid::HybridRef::operator=(Int val) {
        owner.set(idx, val);
        return *this;
    }
    inline Hybrid::HybridRef& Hybrid::HybridRef::operator=(int val) {
        owner.set(idx, (Int)val);
        return *this;
    }
    inline Hybrid::HybridRef& Hybrid::HybridRef::operator=(bool val) {
        owner.set(idx, val ? 1 : 0);
        return *this;
    }
    inline Hybrid::HybridRef& Hybrid::HybridRef::operator=(const HybridRef& other) {
        owner.set(idx, (Int)other);
        return *this;
    }

    using bytes_t = Bytes;
    using bits_t = Bits;
    using hybrid_t = Hybrid;
    using grid_t = Grid;

    template<typename A, typename B>
    inline auto add(A a, B b) -> decltype(a + b) {
        return a + b;
    }

    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    inline Int toInt(T val) {
        return static_cast<Int>(val);
    }
    inline Int toInt(const Str& s) {
        try {
            return std::stoll(s, nullptr, 0);
        } catch (...) {
            return 0;
        }
    }
    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    inline Dec toDec(T val) {
        return static_cast<Dec>(val);
    }
    inline Dec toDec(const Str& s) {
        try {
            return std::stod(s);
        } catch (...) {
            return 0.0;
        }
    }
    inline Bool toBool(Bool b) { return b; }
    inline Bool toBool(Int i) { return i != 0; }
    inline Bool toBool(Dec d) { return d != 0.0; }
    inline Bool toBool(const Str& s) { return s == "true" || s == "1"; }
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
    inline Str toStr(Bool val) {
        return val ? "true" : "false";
    }
    inline Str toHex(Int val) {
        std::stringstream ss;
        ss << "0x" << std::hex << std::uppercase << val;
        return ss.str();
    }
    inline Str toBin(Int val) {
        if (val == 0) return "0b0";
        std::string s = "";
        long long v = val;
        while (v > 0) {
            s = ((v & 1) ? "1" : "0") + s;
            v >>= 1;
        }
        return "0b" + s;
    }

    template<typename T, typename U>
    inline auto elvis(const T& a, const U& b) {
        if constexpr (std::is_same_v<T, Str> || std::is_same_v<T, std::string>) {
            return a.empty() ? Str(b) : a;
        } else if constexpr (std::is_constructible_v<bool, T>) {
            return bool(a) ? a : T(b);
        } else {
            return a;
        }
    }

    template<typename F>
    struct ScopeGuard {
        F f;
        ScopeGuard(F&& func) : f(std::forward<F>(func)) {}
        ~ScopeGuard() { f(); }
    };
    template<typename F>
    inline ScopeGuard<std::decay_t<F>> make_defer(F&& f) {
        return ScopeGuard<std::decay_t<F>>(std::forward<F>(f));
    }
    #define VISS_DEFER_CONCAT_IMPL(x, y) x##y
    #define VISS_DEFER_CONCAT(x, y) VISS_DEFER_CONCAT_IMPL(x, y)
    #define VISS_DEFER auto VISS_DEFER_CONCAT(_viss_defer_, __LINE__) = ::viss::make_defer

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

    inline std::ostream& operator<<(std::ostream& os, const Bytes& b) {
        os << b.to_string();
        return os;
    }
    inline Str toStr(const Bytes& b) {
        return b.to_string();
    }

    inline std::ostream& operator<<(std::ostream& os, const Bits& b) {
        os << "[Bits: " << b.size() << " b (" << b.byte_size() << " B)]";
        return os;
    }
    inline Str toStr(const Bits& b) {
        return "[Bits: " + std::to_string(b.size()) + " b (" + std::to_string(b.byte_size()) + " B)]";
    }

    inline Str toStr(const Bits::BitRef& br) {
        return bool(br) ? "1" : "0";
    }

    inline std::ostream& operator<<(std::ostream& os, const Hybrid& h) {
        os << h.to_string();
        return os;
    }
    inline Str toStr(const Hybrid& h) {
        return h.to_string();
    }
    inline Str toStr(const Hybrid::HybridRef& hr) {
        return std::to_string((Int)hr);
    }

    inline std::ostream& operator<<(std::ostream& os, const Grid& g) {
        os << g.to_string();
        return os;
    }
    inline Str toStr(const Grid& g) {
        return g.to_string();
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

    template<typename T>
    inline Str operator+(const Str& s, const List<T>& l) {
        return s + toStr(l);
    }
    template<typename T>
    inline Str operator+(const char* s, const List<T>& l) {
        return Str(s) + toStr(l);
    }
    template<typename T>
    inline Str operator+(const List<T>& l, const Str& s) {
        return toStr(l) + s;
    }
    template<typename T>
    inline Str operator+(const List<T>& l, const char* s) {
        return toStr(l) + Str(s);
    }

    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, char>>>
    inline Str operator+(const Str& s, T val) {
        if constexpr (std::is_same_v<T, bool>) {
            return s + (val ? "true" : "false");
        } else if constexpr (std::is_floating_point_v<T>) {
            std::string res = std::to_string(val);
            while (res.size() > 1 && res.back() == '0' && res[res.size() - 2] != '.') res.pop_back();
            return s + res;
        } else {
            return s + std::to_string(val);
        }
    }

    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, char>>>
    inline Str operator+(T val, const Str& s) {
        if constexpr (std::is_same_v<T, bool>) {
            return (val ? "true" : "false") + s;
        } else if constexpr (std::is_floating_point_v<T>) {
            std::string res = std::to_string(val);
            while (res.size() > 1 && res.back() == '0' && res[res.size() - 2] != '.') res.pop_back();
            return res + s;
        } else {
            return std::to_string(val) + s;
        }
    }

}

// Automatically include standard library modules
#include "std/sys.hpp"
#include "std/io.hpp"
#include "std/fs.hpp"
#include "std/math.hpp"
#include "std/time.hpp"
#include "std/str.hpp"
#include "std/thread.hpp"
#include "std/async.hpp"
#include "std/json.hpp"
#include "std/crypto.hpp"
#include "std/collections.hpp"
#include "std/env.hpp"
#include "std/net.hpp"
#include "std/retrotech.hpp"
#include "std/audio.hpp"
#include "std/media.hpp"
#include "std/gui.hpp"

namespace viss {
    namespace asyncIO = async;
}


