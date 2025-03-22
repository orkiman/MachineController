// Timer.h
#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>

class Timer
{
public:
    // Define a type alias for the callback function.
    using Callback = std::function<void()>;

    Timer() : cancelled_(false) {}

    // Destructor ensures that the timer is cancelled and the thread is joined.
    ~Timer()
    {
        cancel();
        if (worker_.joinable())
        {
            worker_.join();
        }
    }

    
    template <typename Duration>
    void start(Duration duration, Callback cb)
    {
        // Cancel any previous timer.
        cancel();
        // If a previous thread is still active, join it.
        if (worker_.joinable())
        {
            worker_.join();
        }
        cancelled_.store(false);
        // Launch a new thread.
        worker_ = std::thread([this, duration, cb]()
                              {
        std::unique_lock<std::mutex> lock(mutex_);
        // Wait until either the duration elapses or we are cancelled.
        if (cv_.wait_for(lock, duration, [this]() { return cancelled_.load(); })) {
            // Timer was cancelled.
            return;
        }
        // Timer elapsed normally, execute the callback.
        cb(); });
    }

    // Cancel the timer: signals the waiting thread to wake up and exit early.
    void cancel()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancelled_.store(true);
        }
        cv_.notify_all();
    }

private:
    std::atomic<bool> cancelled_; // Cancellation flag.
    std::mutex mutex_;            // Protects shared state.
    std::condition_variable cv_;  // Used to sleep without busy waiting.
    std::thread worker_;          // The timer's worker thread.
};

/*
Usage Example:

#include "Timer.h"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    Timer timer;

    // Start a timer that will call the callback after 3 seconds.
    timer.start(std::chrono::microsecond(3000), [this]() {
        std::cout << "Timer elapsed! Callback executed." << std::endl;
    });

    // Simulate doing other work.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Uncomment the next line to cancel the timer before it elapses.
    // timer.cancel();

    // Wait long enough to see if the timer fires.
    std::this_thread::sleep_for(std::chrono::seconds(4));

    return 0;
}
*/
