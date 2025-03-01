#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include "io/PCI7248IO.h"
#include "EventQueue.h"
#include "Config.h"
#include "io/IOChannel.h"

using namespace std::chrono;

int main() {
    timeBeginPeriod(1);  // Ensure 1ms sleep accuracy

    // Create a Config instance from your settings file.
    Config config("config/settings.json");    
    EventQueue<IOEvent> eventQueue;
    
    // Instantiate the PCI7248IO object.
    PCI7248IO io(&eventQueue, config);
    if (!io.initialize()) {
        std::cerr << "Failed to initialize PCI7248IO." << std::endl;
        return 1;
    }
    
    std::cout << "PCI7248IO polling thread started. Press Ctrl+C to exit." << std::endl;
    
    // Retrieve output channels from the configuration.
    std::vector<IOChannel> outputs = io.getOutputChannels();
    // print configured output channels
    for (const auto& ch : outputs) {
        std::cout << "Configured output: " << ch.name << " (port " 
                  << ch.ioPort << ", pin " << ch.pin << ")" << std::endl;
    }
    // For demonstration, assign each output channel a different blink interval in milliseconds.
    std::vector<int> blinkIntervals;
    for (size_t i = 0; i < outputs.size(); i++) {
        blinkIntervals.push_back(500 + static_cast<int>(i) * 250); // e.g., 500ms, 750ms, 1000ms, etc.
    }
    // Record the last toggle time for each output channel.
    std::vector<steady_clock::time_point> lastToggle(outputs.size(), steady_clock::now());
    
    // Launch a thread to blink outputs.
    std::thread outputBlinkThread([&]() {
        while (true) {
            auto now = steady_clock::now();
            bool needUpdate = false;
            // Iterate through each output channel.
            for (size_t i = 0; i < outputs.size(); i++) {
                auto elapsed = duration_cast<milliseconds>(now - lastToggle[i]).count();
                if (elapsed >= blinkIntervals[i]) {
                    // Toggle the state: if low, set high; if high, set low.
                    outputs[i].state = (outputs[i].state == 0) ? 1 : 0;
                    lastToggle[i] = now;
                    needUpdate = true;
                }
            }
            if (needUpdate) {
                // Write the new output states.
                if (!io.writeOutputs(outputs)) {
                    std::cerr << "Failed to update outputs." << std::endl;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Main loop: process input events from the event queue.
    while (true) {
        IOEvent event;
        while (eventQueue.try_pop(event)) {
            std::cout << "Input Snapshot:" << std::endl;
            for (const auto &channel : event.channels) {
                std::cout << "  " << channel.name 
                          << " (port " << channel.ioPort 
                          << ", pin " << channel.pin 
                          << ") state: " << channel.state 
                          << " event: ";
                switch(channel.eventType) {
                    case IOEventType::Rising: std::cout << "Rising"; break;
                    case IOEventType::Falling: std::cout << "Falling"; break;
                    default: std::cout << "None"; break;
                }
                std::cout << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Optionally join the outputBlinkThread (if you ever exit the loop).
    outputBlinkThread.join();
    timeEndPeriod(1);  // Restore normal timer resolution
    return 0;
}
