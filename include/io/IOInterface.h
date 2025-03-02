#ifndef IO_INTERFACE_H
#define IO_INTERFACE_H

#include <string>
#include <unordered_map>
#include "IOChannel.h"  // Defines the IOChannel structure

// Define a type for IO state (can be adapted as needed)
typedef int IOState;


// Abstract interface for IO modules reflecting the new design.
class IOInterface {
public:
    virtual ~IOInterface() {}

    // Initialize hardware and configuration.
    virtual bool initialize() = 0;

    // Write outputs: accepts a unordered_map of updated output channels.
    // Any output not provided in the unordered_map is driven low.
    virtual bool writeOutputs(const std::unordered_map<std::string, IOChannel>& newOutputsState) = 0;

    // Retrieve a snapshot of the current input channels.
    virtual std::unordered_map<std::string, IOChannel> getInputChannelsSnapshot() const = 0;

    //retreve a pointer to the output channels
    virtual const std::unordered_map<std::string, IOChannel>& getOutputChannels() const = 0;
};

#endif // IO_INTERFACE_H
