#ifndef PCI7248_IO_H
#define PCI7248_IO_H

#include "io/IOInterface.h"
#include "Config.h"
#include "EventQueue.h"
#include "IOChannel.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include "dask64.h"  // DASK API header for PCI7248
#endif

// The PCI7248IO class implements the IOInterface for the PCI7248 board.
// It manages input and output channel states, polls the hardware in a separate thread,
// and pushes consolidated input state snapshots to the event queue.
class PCI7248IO : public IOInterface {
public:
    // Constructor accepts a pointer to an event queue and a reference to a Config object.
    PCI7248IO(EventQueue<IOEvent>* eventQueue, const Config& config);
    virtual ~PCI7248IO();

    // Initialize the hardware, configure ports, and read the initial input state.
    bool initialize() override;

    // Write outputs by accepting a vector of updated output channel states.
    // Channels not provided in newOutputsState are forced low (0).
    bool writeOutputs(const std::vector<IOChannel>& newOutputsState);

    // Get a snapshot of the current input channels.
    std::vector<IOChannel> getInputChannelsSnapshot() const;

    // Stop the polling thread.
    void stopPolling();

    // For compatibility; if needed, you can later implement a proper state map.
    // const std::unordered_map<std::string, IOState>& getCurrentState() const override;

private:
    // Polling loop that continuously reads input states and pushes an event
    // if any active input channel changes.
    void pollLoop();

    // Push a consolidated event containing a copy of the current input channels vector.
    void pushStateEvent();

    // PCI7248-specific helper: returns the base offset for the given port.
    // For example, "A" -> 0, "B" -> 8, "CL" -> 16, "CH" -> 20.
    int getPortBaseOffset(const std::string &port) const;

    // Member variables.
    EventQueue<IOEvent>* eventQueue_;
    const Config& config_;
    I16 card_;  // DASK card handle.

    // Vectors storing the configuration and current state for inputs and outputs.
    std::vector<IOChannel> inputChannels_;
    std::vector<IOChannel> outputChannels_;

    // Threading members.
    std::thread pollingThread_;
    std::atomic<bool> stopPolling_;
};

#endif // PCI7248_IO_H
