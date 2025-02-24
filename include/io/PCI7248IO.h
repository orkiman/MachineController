#ifndef PCI7248_IO_H
#define PCI7248_IO_H

#include "io/IOInterface.h"
#include "Config.h"
#include "EventQueue.h"
#include <unordered_map>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include "dask64.h"  // DASK API header for PCI7248
#endif

class PCI7248IO : public IOInterface {
public:
    PCI7248IO(EventQueue<IOEvent>* eventQueue, const Config& config);
    virtual ~PCI7248IO();

    bool initialize() override;
    void updateInputs() override;
    bool writeOutput(const std::string& ioName, IOState state) override;
    bool writeOutputs(const std::unordered_map<std::string, IOState>& outputs) override;
    const std::unordered_map<std::string, IOState>& getCurrentState() const override;

private:
    EventQueue<IOEvent>* eventQueue_;
    const Config& config_;
    std::unordered_map<std::string, IOState> ioStateMap_;
    // Map of IO name to pin number (for both inputs and outputs)
    std::unordered_map<std::string, int> ioPinMap_;
    I16 card_;  // DASK card handle

    // Hardware-specific functions
    IOState readInput(const std::string& ioName);
    bool writeHardware(int pinNumber, IOState state);
    void pushEvent(const std::string& ioName, IOState state);
};

#endif // PCI7248_IO_H
