#ifndef IO_INTERFACE_H
#define IO_INTERFACE_H

#include <string>
#include <unordered_map>

// Define a type for IO state (can be adapted as needed)
typedef int IOState;

// Event structure for IO changes
struct IOEvent {
    std::string ioName;
    IOState state;
};

// Abstract interface for IO modules
class IOInterface {
public:
    virtual ~IOInterface() {}

    // Initialize hardware and configuration (IO mappings, etc.)
    virtual bool initialize() = 0;

    // Read inputs from hardware and generate events for state changes
    virtual void updateInputs() = 0;

    // Write a single output
    virtual bool writeOutput(const std::string& ioName, IOState state) = 0;

    // Write multiple outputs at once
    virtual bool writeOutputs(const std::unordered_map<std::string, IOState>& outputs) = 0;

    // Retrieve the current IO state mapping
    virtual const std::unordered_map<std::string, IOState>& getCurrentState() const = 0;
};

#endif // IO_INTERFACE_H
