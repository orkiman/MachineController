#ifndef IO_INTERFACE_H
#define IO_INTERFACE_H

#include <string>
#include <vector>
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

    // Write outputs: accepts a vector of updated output channels.
    // Any output not provided in the vector is driven low.
    virtual bool writeOutputs(const std::vector<IOChannel>& newOutputsState) = 0;

    // Retrieve a snapshot of the current input channels.
    virtual std::vector<IOChannel> getInputChannelsSnapshot() const = 0;

    //retreve a pointer to the output channels
    virtual const std::vector<IOChannel>& getOutputChannels() const = 0;
};

#endif // IO_INTERFACE_H
