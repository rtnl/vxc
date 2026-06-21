#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <stdexcept>

// totally not vibe coded channel class
template <typename T>
class Channel {
private:
    std::queue<T> buffer;
    std::mutex mu;
    std::condition_variable cv;
    bool closed = false;
    size_t capacity;

public:
    explicit Channel(size_t cap = 0) : capacity(cap) {}

    ~Channel() = default;

    // Send (blocks if full, like Go's channel <-)
    void send(T value) {
        std::unique_lock<std::mutex> lock(mu);

        // Wait if buffer is full (unbuffered if capacity == 0)
        cv.wait(lock, [this]() {
            return buffer.size() < capacity || closed;
        });

        if (closed) throw std::runtime_error("send on closed channel");

        buffer.push(value);
        cv.notify_all();  // Wake up receivers
    }

    // Receive (blocks if empty, like Go's <-channel)
    T recv() {
        std::unique_lock<std::mutex> lock(mu);

        // Wait if buffer is empty
        cv.wait(lock, [this]() {
            return !buffer.empty() || closed;
        });

        if (buffer.empty()) throw std::runtime_error("recv on closed channel");

        T value = buffer.front();
        buffer.pop();
        cv.notify_all();  // Wake up senders
        return value;
    }

    void close() {
        std::unique_lock<std::mutex> lock(mu);
        closed = true;
        cv.notify_all();
    }
};
