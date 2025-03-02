#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include "Event.h"

template <typename T>
class EventQueue {
public:
    void push(const T& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(event);
        condition_.notify_one();
    }

    bool try_pop(T& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        event = queue_.front();
        queue_.pop();
        return true;
    }

    void wait_and_pop(T& event) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !queue_.empty(); });
        event = queue_.front();
        queue_.pop();
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
};

#endif // EVENT_QUEUE_H
