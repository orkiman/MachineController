#include <iostream>
#include <chrono>
#include <thread>
#include "io/PCI7248IO.h"
#include "EventQueue.h"
#include "Config.h"

int main() {
    Config config;    
    EventQueue<IOEvent> eventQueue;
    
    PCI7248IO io(&eventQueue, config);
    if (!io.initialize()) {
        std::cerr << "Failed to initialize PCI7248IO." << std::endl;
        return 1;
    }
    
    std::cout << "Starting IO update loop. Press Ctrl+C to exit." << std::endl;
    while (true) {
        io.updateInputs();
        
        IOEvent event;
        while (eventQueue.try_pop(event)) {
            std::cout << "Event: " << event.ioName << " state: " << event.state << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}
