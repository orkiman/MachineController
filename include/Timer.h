// IO.h
#ifndef IO_H
#define IO_H

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include "EventQueue.h" // Assume you have an EventQueue class defined elsewhere
#include "Config.h"     // Assume you have a Config class for parsing the configuration file

// Define a type alias for I/O state (could be bool, int, etc. depending on your hardware)
using IOState = int;

// Event structure example for an IO event
struct IOEvent {
    std::string ioName;
    IOState state;
};

class IO {
public:
    // Constructor: Accepts a pointer to the event queue and configuration settings.
    IO(EventQueue<IOEvent>* eventQueue, const Config& config);

    // Initialize the IO subsystem: configure input and output mappings based on config.
    bool initialize();

    // Method to be called periodically or on a hardware interrupt to read inputs.
    void updateInputs();

    // Write a single output: can be extended to accept a map for bulk updates.
    bool writeOutput(const std::string& ioName, IOState state);

    // Write multiple outputs at once.
    bool writeOutputs(const std::unordered_map<std::string, IOState>& outputs);

    // Optional: Method to retrieve current IO map state.
    const std::unordered_map<std::string, IOState>& getCurrentState() const;

private:
    // Reference to the event queue used for posting IO events.
    EventQueue<IOEvent>* eventQueue_;

    // Map storing current IO states; keys are names (as defined in the configuration).
    std::unordered_map<std::string, IOState> ioStateMap_;

    // Mapping for IO details (e.g., addresses, pin numbers); initialized from config.
    std::unordered_map<std::string, std::string> ioAddressMap_;

    // Helper method to load configuration data into the IO maps.
    bool loadConfig(const Config& config);

    // Method to push an IO event to the event queue.
    void pushEvent(const std::string& ioName, IOState state);
};

#endif // IO_H
