#ifndef PCI7248_IO_H
#define PCI7248_IO_H

#include "io/IOInterface.h"
#include "Config.h"
#include "EventQueue.h"
#include "IOChannel.h"
#include "Logger.h"
#include <string>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <Windows.h>
#include "dask64.h" // ✅ added missing header for I16

class PCI7248IO : public IOInterface {
public:
    PCI7248IO(EventQueue<EventVariant>* eventQueue, const Config &config);
    ~PCI7248IO();

    bool initialize();
    bool writeOutputs(const std::unordered_map<std::string, IOChannel>& newOutputsState);
    std::unordered_map<std::string, IOChannel> getInputChannelsSnapshot() const;
    const std::unordered_map<std::string, IOChannel>& getOutputChannels() const;
    void stopPolling();

private:
    void pollLoop();
    void pushStateEvent();
    bool resetConfiguredOutputPorts();
    void preciseSleep(int microseconds);
    int getPortBaseOffset(const std::string& port) const;
    int getDaskChannel(const std::string &port) const;
    void assignPortNames(std::unordered_map<std::string, IOChannel>& channels);
    void readInitialInputStates();
    bool updateInputStates();
    void logConfiguredChannels();

    EventQueue<EventVariant>* eventQueue_;
    const Config& config_;
    I16 card_;
    std::unordered_map<std::string, std::string> portsConfig_;
    std::unordered_map<std::string, IOChannel> inputChannels_;
    std::unordered_map<std::string, IOChannel> outputChannels_;
    std::thread pollingThread_;
    std::atomic<bool> stopPolling_;
};

#endif // PCI7248_IO_H
