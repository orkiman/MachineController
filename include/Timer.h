// Timer.h
#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include "io/IOChannel.h"

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
        cancelled_.store(true);
        cv_.notify_all();
    }
    
    // State management methods
    void setName(const std::string& name) { name_ = name; }
    std::string getName() const { return name_; }
    
    void setDescription(const std::string& description) { description_ = description; }
    std::string getDescription() const { return description_; }
    
    void setDuration(int duration) { duration_ = duration; }
    int getDuration() const { return duration_; }
    
    void setState(int state) { state_ = state; }
    int getState() const { return state_; }
    
    void setEventType(IOEventType eventType) { eventType_ = eventType; }
    IOEventType getEventType() const { return eventType_; }
    
    int state_;                   // Current state: 0 (inactive) or 1 (active)
    IOEventType eventType_;       // Event trigger type: Rising, Falling, or None

    

private:
    std::atomic<bool> cancelled_; // Flag to indicate if the timer is cancelled.
    std::mutex mutex_;            // Mutex for the condition variable.
    std::condition_variable cv_;  // Condition variable for waiting.
    std::thread worker_;          // The timer's worker thread.
    
    // State management variables
    std::string name_;            // Timer name (e.g., "timer1", "timer2")
    std::string description_;     // Human-readable description
    int duration_;                // Duration in milliseconds
    
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
