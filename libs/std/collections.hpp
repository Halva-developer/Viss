#pragma once
#include "../vissrt.hpp"
#include <vector>
#include <deque>
#include <unordered_set>
#include <stdexcept>

namespace viss {
    namespace collections {

        // --- Stack<T> ---
        template<typename T>
        class Stack {
        private:
            std::vector<T> data;
        public:
            Stack() = default;
            inline void push(const T& val) { data.push_back(val); }
            inline T pop() {
                if (data.empty()) throw Error("Stack is empty");
                T v = data.back();
                data.pop_back();
                return v;
            }
            inline T peek() const {
                if (data.empty()) throw Error("Stack is empty");
                return data.back();
            }
            inline Int size() const { return (Int)data.size(); }
            inline Bool is_empty() const { return data.empty(); }
            inline void clear() { data.clear(); }
        };

        // --- Queue<T> ---
        template<typename T>
        class Queue {
        private:
            std::deque<T> data;
        public:
            Queue() = default;
            inline void push(const T& val) { data.push_back(val); }
            inline T pop() {
                if (data.empty()) throw Error("Queue is empty");
                T v = data.front();
                data.pop_front();
                return v;
            }
            inline T front() const {
                if (data.empty()) throw Error("Queue is empty");
                return data.front();
            }
            inline T back() const {
                if (data.empty()) throw Error("Queue is empty");
                return data.back();
            }
            inline Int size() const { return (Int)data.size(); }
            inline Bool is_empty() const { return data.empty(); }
            inline void clear() { data.clear(); }
        };

        // --- Set<T> ---
        template<typename T>
        class Set {
        private:
            std::unordered_set<T> data;
        public:
            Set() = default;
            Set(std::initializer_list<T> init) : data(init) {}

            inline void add(const T& val) { data.insert(val); }
            inline void remove(const T& val) { data.erase(val); }
            inline Bool has(const T& val) const { return data.find(val) != data.end(); }
            inline Int size() const { return (Int)data.size(); }
            inline Bool is_empty() const { return data.empty(); }
            inline void clear() { data.clear(); }
            inline List<T> to_list() const {
                List<T> l;
                for (const auto& item : data) l.add(item);
                return l;
            }
        };

        // --- RingBuffer<T> (Circular Buffer) ---
        template<typename T>
        class RingBuffer {
        private:
            std::vector<T> buf;
            size_t head = 0;
            size_t tail = 0;
            size_t count = 0;
            size_t cap = 0;
        public:
            RingBuffer(size_t capacity = 16) : buf(capacity), cap(capacity) {}

            inline void push(const T& val) {
                if (cap == 0) return;
                buf[head] = val;
                head = (head + 1) % cap;
                if (count < cap) count++;
                else tail = (tail + 1) % cap;
            }

            inline T pop() {
                if (count == 0) throw Error("RingBuffer is empty");
                T v = buf[tail];
                tail = (tail + 1) % cap;
                count--;
                return v;
            }

            inline T get(size_t index) const {
                if (index >= count) throw Error("Index out of range");
                return buf[(tail + index) % cap];
            }

            inline Int size() const { return (Int)count; }
            inline Int capacity() const { return (Int)cap; }
            inline Bool is_full() const { return count == cap; }
            inline Bool is_empty() const { return count == 0; }
            inline void clear() { head = tail = count = 0; }
        };
    }
}
