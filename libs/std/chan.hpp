#pragma once
#include "../vissrt.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <optional>
#include <stdexcept>

namespace viss {
namespace chan {

    template<typename T>
    class ChannelInternal {
    public:
        size_t capacity;
        std::queue<T> buffer;
        std::mutex mtx;
        std::condition_variable cv_recv;
        std::condition_variable cv_send;
        bool closed = false;

        ChannelInternal(size_t cap = 0) : capacity(cap) {}

        void send(const T& val) {
            std::unique_lock<std::mutex> lock(mtx);
            if (closed) {
                throw std::runtime_error("Cannot send to closed channel");
            }
            if (capacity > 0) {
                cv_send.wait(lock, [this]() { return buffer.size() < capacity || closed; });
                if (closed) throw std::runtime_error("Cannot send to closed channel");
                buffer.push(val);
                cv_recv.notify_one();
            } else {
                // Unbuffered channel
                cv_send.wait(lock, [this]() { return buffer.empty() || closed; });
                if (closed) throw std::runtime_error("Cannot send to closed channel");
                buffer.push(val);
                cv_recv.notify_one();
                cv_send.wait(lock, [this]() { return buffer.empty() || closed; });
            }
        }

        bool try_send(const T& val) {
            std::unique_lock<std::mutex> lock(mtx);
            if (closed) return false;
            if (capacity > 0 && buffer.size() >= capacity) return false;
            buffer.push(val);
            cv_recv.notify_one();
            return true;
        }

        T recv() {
            std::unique_lock<std::mutex> lock(mtx);
            cv_recv.wait(lock, [this]() { return !buffer.empty() || closed; });
            if (buffer.empty() && closed) {
                throw std::runtime_error("Channel closed and empty");
            }
            T val = buffer.front();
            buffer.pop();
            cv_send.notify_one();
            return val;
        }

        bool try_recv(T& out_val) {
            std::unique_lock<std::mutex> lock(mtx);
            if (buffer.empty()) return false;
            out_val = buffer.front();
            buffer.pop();
            cv_send.notify_one();
            return true;
        }

        void close() {
            std::unique_lock<std::mutex> lock(mtx);
            closed = true;
            cv_recv.notify_all();
            cv_send.notify_all();
        }

        bool is_closed() {
            std::unique_lock<std::mutex> lock(mtx);
            return closed;
        }

        size_t size() {
            std::unique_lock<std::mutex> lock(mtx);
            return buffer.size();
        }
    };

    template<typename T>
    using Chan = std::shared_ptr<ChannelInternal<T>>;

    template<typename T>
    inline Chan<T> create(Int capacity = 0) {
        return std::make_shared<ChannelInternal<T>>((size_t)(capacity > 0 ? capacity : 0));
    }

    template<typename T>
    inline void send(Chan<T> ch, const T& val) {
        if (!ch) throw std::runtime_error("Null channel");
        ch->send(val);
    }

    template<typename T>
    inline T recv(Chan<T> ch) {
        if (!ch) throw std::runtime_error("Null channel");
        return ch->recv();
    }

    template<typename T>
    inline Bool try_recv(Chan<T> ch, T& out_val) {
        if (!ch) return false;
        return ch->try_recv(out_val);
    }

    template<typename T>
    inline void close(Chan<T> ch) {
        if (ch) ch->close();
    }

    template<typename T>
    inline Bool is_closed(Chan<T> ch) {
        return ch ? ch->is_closed() : true;
    }

    template<typename T>
    inline Int len(Chan<T> ch) {
        return ch ? (Int)ch->size() : 0;
    }

    template<typename T>
    inline Int cap(Chan<T> ch) {
        return ch ? (Int)ch->capacity : 0;
    }

} // namespace chan
} // namespace viss
