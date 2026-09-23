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
#include <cstdint>
#include <iomanip>
#include <functional>

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
    inline Str toStr(const std::exception& e) {
        return e.what();
    }

    template<typename T = std::string>
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
        inline T get(Int index) const {
            std::lock_guard<std::mutex> lock(*mtx);
            if (index >= 0 && index < (Int)data->size()) {
                return (*data)[index];
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

        inline T& operator[](Int index) {
            std::lock_guard<std::mutex> lock(*mtx);
            return (*data)[index];
        }

        inline const T& operator[](Int index) const {
            std::lock_guard<std::mutex> lock(*mtx);
            return (*data)[index];
        }

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

    // Bytes: High-Performance Hardware-Level Memory Buffer
    class Bytes {
    private:
        std::shared_ptr<std::vector<uint8_t>> data;
    public:
        static const size_t DEFAULT_MAX_SIZE = 1024; // 1 KB maximum default

        Bytes(size_t size = DEFAULT_MAX_SIZE, uint8_t fill = 0)
            : data(std::make_shared<std::vector<uint8_t>>(size, fill)) {}
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

        inline size_t size() const { return data ? data->size() : 0; }
        inline Int get_len() const { return (Int)size(); }

        inline uint8_t& operator[](size_t index) {
            if (index >= data->size()) data->resize(index + 1, 0);
            return (*data)[index];
        }
        inline const uint8_t& operator[](size_t index) const {
            return (*data)[index];
        }
        inline uint8_t get(size_t index) const {
            if (index < data->size()) return (*data)[index];
            return 0;
        }
        inline void set(size_t index, uint8_t val) {
            if (index >= data->size()) data->resize(index + 1, 0);
            (*data)[index] = val;
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
        inline uint8_t* raw() { return data->data(); }
        inline const uint8_t* raw() const { return data->data(); }
    };

    // Bits: Compact Bitfield Buffer
    class Bits : public Bytes {
    public:
        static const size_t DEFAULT_MAX_BITS = 8192; // 8192 bits = 1024 bytes
        size_t total_bits;

        Bits(size_t num_bits = DEFAULT_MAX_BITS) 
            : Bytes((num_bits + 7) / 8), total_bits(num_bits) {}

        inline bool get(size_t bit_idx) const {
            return get_bit(bit_idx / 8, bit_idx % 8);
        }
        inline void set(size_t bit_idx, bool val) {
            set_bit(bit_idx / 8, bit_idx % 8, val);
        }
        inline Int count_ones() const {
            Int count = 0;
            for (size_t i = 0; i < total_bits; ++i) {
                if (get(i)) count++;
            }
            return count;
        }
        inline size_t size() const { return total_bits; }
        inline Int get_len() const { return (Int)total_bits; }
    };

    using bytes_t = Bytes;
    using bits_t = Bits;

    inline Int toInt(const Str& s) {
        try {
            return std::stoll(s, nullptr, 0);
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
        os << "[Bytes: " << b.size() << " B]";
        return os;
    }
    inline Str toStr(const Bytes& b) {
        return "[Bytes: " + std::to_string(b.size()) + " B]";
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
#include "std/retrotech.hpp"
