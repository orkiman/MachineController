#pragma once

#include <unordered_map>
#include <string>
#include <thread>

#include "Config.h"      // Provides full definition for Config.
#include "Event.h"       // Provides full definitions for EventVariant and (if defined there) IOEventType.
#include "IOChannel.h"   // Provides full definition for IOChannel and possibly IOEventType if not in Event.h.
#include "EventQueue.h"


// Forward declaration for the event queue template if its definition is not in "EventQueue.h"
template <typename T>
class EventQueue;

class PCI7248IO {
public:
    PCI7248IO(EventQueue<EventVariant>* eventQueue, const Config& config);
    ~PCI7248IO();

    bool initialize();
    const std::unordered_map<std::string, IOChannel>& getOutputChannels() const;
    std::unordered_map<std::string, IOChannel> getInputChannelsSnapshot() const;
    bool writeOutputs(const std::unordered_map<std::string, IOChannel>& newOutputsState);
    void stopPolling();

private:
    // Helper functions
    void assignPortNames(std::unordered_map<std::string, IOChannel>& channels);
    void readInitialInputStates(const std::unordered_map<std::string, int>& portToChannel);
    void logConfiguredChannels();
    void pollLoop();
    bool updateInputStates(const std::unordered_map<std::string, int>& portToChannel);
    void pushStateEvent();
    int getDaskChannel(const std::string &port) const;
    int getPortBaseOffset(const std::string &port) const;
    void preciseSleep(int microseconds);
    bool resetConfiguredOutputPorts();

    // Member variables
    EventQueue<EventVariant>* eventQueue_;
    Config config_;
    int card_;
    std::unordered_map<std::string, IOChannel> inputChannels_;
    std::unordered_map<std::string, IOChannel> outputChannels_;
    std::unordered_map<std::string, std::string> portsConfig_;
    std::thread pollingThread_;
    volatile bool stopPolling_;
};
