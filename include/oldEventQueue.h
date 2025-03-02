#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class EventQueue {
public:
    EventQueue() = default;
    ~EventQueue() = default;

    // Disable copying
    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;

    void push(const T& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(event);
        m_cond.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this] { return !m_queue.empty(); });
        T event = m_queue.front();
        m_queue.pop();
        return event;
    }

    bool try_pop(T& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty())
            return false;
        event = m_queue.front();
        m_queue.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    mutable std::mutex m_mutex;
    std::queue<T> m_queue;
    std::condition_variable m_cond;
};

#endif // EVENT_QUEUE_H
