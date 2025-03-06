// PCI7248IO.cpp

#include "io/PCI7248IO.h"
#include "Logger.h"
#include <thread>
#include <unordered_map>
#include <limits>
#include <chrono>
#include <windows.h>         // For GetCurrentThread, SetThreadPriority, etc.
#include "dask64.h"

using namespace std::chrono;

PCI7248IO::PCI7248IO(EventQueue<EventVariant> *eventQueue, const Config &config)
    : eventQueue_(eventQueue), config_(config), card_(-1), stopPolling_(false)
{
    inputChannels_ = config_.getInputs();
    outputChannels_ = config_.getOutputs();
}

PCI7248IO::~PCI7248IO()
{
    resetConfiguredOutputPorts();
    stopPolling();
    if (card_ >= 0) {
        Release_Card(card_);
    }
    getLogger()->flush();
}

bool PCI7248IO::initialize()
{
    if (!config_.isPci7248ConfigurationValid()) {
        getLogger()->error("Invalid PCI7248 configuration.");
        return false;
    }

    card_ = Register_Card(PCI_7248, 0);
    if (card_ < 0) {
        getLogger()->error("Failed to register PCI-7248! Error Code: {}", card_);
        return false;
    }

    portsConfig_ = config_.getPci7248IoPortsConfiguration();

    static const std::unordered_map<std::string, int> portToChannel = {
        {"A", Channel_P1A}, {"B", Channel_P1B}, {"CL", Channel_P1CL}, {"CH", Channel_P1CH}
    };

    // Configure each port as input or output.
    for (const auto &[port, portTypeAsString] : portsConfig_) {
        int portType = (portTypeAsString == "output") ? OUTPUT_PORT : INPUT_PORT;
        if (DIO_PortConfig(card_, portToChannel.at(port), portType) != 0) {
            getLogger()->error("Failed to configure Port {} as {}", port, portTypeAsString);
            return false;
        }
    }

    assignPortNames(inputChannels_);
    assignPortNames(outputChannels_);

    readInitialInputStates(portToChannel);
    logConfiguredChannels();

    if (!resetConfiguredOutputPorts()) {
        getLogger()->error("Failed to reset configured output ports.");
        return false;
    }

    // Start polling in a separate thread.
    bool printLoopStatistics = false;
    pollingThread_ = std::thread(&PCI7248IO::pollLoop, this,printLoopStatistics);
    return true;
}

// Assign the ioPort field based on pin number.
void PCI7248IO::assignPortNames(std::unordered_map<std::string, IOChannel> &channels)
{
    for (auto &[_, channel] : channels) {
        if (channel.pin < 8) channel.ioPort = "A";
        else if (channel.pin < 16) channel.ioPort = "B";
        else if (channel.pin < 20) channel.ioPort = "CL";
        else channel.ioPort = "CH";
    }
}

// Read initial input states for all ports configured as input.
void PCI7248IO::readInitialInputStates(const std::unordered_map<std::string, int> &portToChannel)
{
    for (const auto &[port, portTypeAsString] : portsConfig_) {
        if (portTypeAsString != "input") continue;

        U32 portValue = 0;
        if (DI_ReadPort(card_, portToChannel.at(port), &portValue) != 0) {
            getLogger()->error("Failed to read initial state from Port {}", port);
            continue;
        }
        portValue = ~portValue; // Active-low
        int baseOffset = getPortBaseOffset(port);

        for (auto &[_, channel] : inputChannels_) {
            if (channel.ioPort == port) {
                channel.state = (portValue >> (channel.pin - baseOffset)) & 0x1;
            }
        }
    }
}

// Log which channels are configured as input vs. output.
void PCI7248IO::logConfiguredChannels()
{
    for (const auto &[_, ch] : inputChannels_) {
        getLogger()->info("Configured input: {} (port {}, pin {})", ch.name, ch.ioPort, ch.pin);
    }
    for (const auto &[_, ch] : outputChannels_) {
        getLogger()->info("Configured output: {} (port {}, pin {})", ch.name, ch.ioPort, ch.pin);
    }
}

// The main polling loop with timing statistics.
void PCI7248IO::pollLoop(bool debugStatistics)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    getLogger()->debug("THREAD_PRIORITY_TIME_CRITICAL");

    static const std::unordered_map<std::string, int> portToChannel = {
        {"A", Channel_P1A}, {"B", Channel_P1B}, {"CL", Channel_P1CL}, {"CH", Channel_P1CH}
    };

    using clock = std::chrono::steady_clock;
    auto statStart = clock::now();

    // Only used if debugStatistics is true.
    long long totalTime = 0;
    long long minTime = std::numeric_limits<long long>::max();
    long long maxTime = 0;
    size_t iterations = 0;
    size_t longRuns = 0;

    while (!stopPolling_) {
        auto loopStart = clock::now();

        // Read inputs and detect changes.
        bool anyChange = updateInputStates(portToChannel);
        if (anyChange) {
            pushStateEvent();
        }

        // Always measure loop duration for timing control.
        auto loopEnd = clock::now();
        auto execTime = std::chrono::duration_cast<std::chrono::microseconds>(loopEnd - loopStart).count();

        // Ensure the loop runs at least 1ms (or use your desired target).
        if (execTime < 1000) {
            preciseSleep(static_cast<int>(1000 - execTime));
            auto loopEndAfterSleep = clock::now();
            execTime = std::chrono::duration_cast<std::chrono::microseconds>(loopEndAfterSleep - loopStart).count();
        }

        // If debug statistics are enabled, accumulate measurements.
        if (debugStatistics) {
            totalTime += execTime;
            minTime = std::min(minTime, execTime);
            maxTime = std::max(maxTime, execTime);
            if (execTime > 5000) {
                longRuns++;
            }
            iterations++;

            // Log every second.
            auto now = clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - statStart).count() >= 1) {
                double avgTime = static_cast<double>(totalTime) / iterations;
                getLogger()->debug(
                    "Poll Loop Execution Time (µs) -- Min: {}, Max: {}, Avg: {:.2f}, long runs: {}",
                    minTime, maxTime, avgTime, longRuns
                );
                statStart = now;
                totalTime = 0;
                minTime = std::numeric_limits<long long>::max();
                maxTime = 0;
                iterations = 0;
                longRuns = 0;
            }
        }
    }

    getLogger()->flush();
}

// Reads input ports, checks for state changes, and updates `inputChannels_`.
bool PCI7248IO::updateInputStates(const std::unordered_map<std::string, int> &portToChannel)
{
    bool anyChange = false;

    for (const auto &[port, portTypeAsString] : portsConfig_) {
        if (portTypeAsString != "input") continue;

        U32 portValue = 0;
        if (DI_ReadPort(card_, portToChannel.at(port), &portValue) != 0) {
            getLogger()->error("Failed to read Port {}", port);
            continue;
        }

        portValue = ~portValue; // Active-low
        int baseOffset = getPortBaseOffset(port);

        for (auto &[_, channel] : inputChannels_) {
            if (channel.ioPort != port) continue;

            int newState = (portValue >> (channel.pin - baseOffset)) & 0x1;
            if (channel.state != newState) {
                if (channel.state == 0 && newState == 1) {
                    channel.eventType = IOEventType::Rising;
                } else if (channel.state == 1 && newState == 0) {
                    channel.eventType = IOEventType::Falling;
                }
                channel.state = newState;
                anyChange = true;
            } else {
                channel.eventType = IOEventType::None;
            }
        }
    }
    return anyChange;
}

// Push an IOEvent containing the updated inputChannels_.
void PCI7248IO::pushStateEvent()
{
    if (eventQueue_) {
        IOEvent event;
        event.channels = inputChannels_;
        eventQueue_->push(event);
    }
}

bool PCI7248IO::resetConfiguredOutputPorts()
{
    // Passing an empty map effectively turns all output pins off.
    return writeOutputs({});
}

// Write outputs using active-low logic.
bool PCI7248IO::writeOutputs(const std::unordered_map<std::string, IOChannel> &newOutputsState)
{
    // Prepare an aggregate of bits for each port.
    std::unordered_map<std::string, U32> portAggregates;
    // Initialize aggregates for all ports in the config so we don't miss any.
    for (const auto &entry : portsConfig_) {
        portAggregates[entry.first] = 0;
    }

    // Set bits for each channel in newOutputsState.
    for (const auto &[_, ch] : newOutputsState) {
        if (ch.type != IOType::Output) continue;
        int baseOffset = getPortBaseOffset(ch.ioPort);
        int bitPos = ch.pin - baseOffset;
        if (ch.state != 0) {
            portAggregates[ch.ioPort] |= (1u << bitPos);
        }
    }

    // Write each port's aggregated value (inverted for active-low).
    bool success = true;
    for (const auto &[port, value] : portAggregates) {
        if (portsConfig_[port] != "output") {
            continue;
        }
        int daskChannel = getDaskChannel(port);
        U32 outValue = (~value) & 0xFF;  // invert and mask to 8 bits
        if (DO_WritePort(card_, daskChannel, outValue) != 0) {
            getLogger()->error("Failed to write Port {}", port);
            success = false;
        }
    }
    return success;
}

// Stop polling gracefully.
void PCI7248IO::stopPolling()
{
    stopPolling_ = true;
    if (pollingThread_.joinable()) {
        pollingThread_.join();
    }
}

// Return a snapshot of the current input channels.
std::unordered_map<std::string, IOChannel> PCI7248IO::getInputChannelsSnapshot() const
{
    return inputChannels_;
}

// Return a reference to the output channels map.
const std::unordered_map<std::string, IOChannel> &PCI7248IO::getOutputChannels() const
{
    return outputChannels_;
}

// Utility to map port name to the DASK channel constant.
int PCI7248IO::getDaskChannel(const std::string &port) const
{
    static const std::unordered_map<std::string, int> channels = {
        {"A", Channel_P1A}, {"B", Channel_P1B}, {"CL", Channel_P1CL}, {"CH", Channel_P1CH}
    };
    return channels.at(port);
}

// Return the base offset for a given port name.
int PCI7248IO::getPortBaseOffset(const std::string &port) const
{
    if (port == "A")  return 0;
    if (port == "B")  return 8;
    if (port == "CL") return 16;
    if (port == "CH") return 20;
    return 0; // fallback
}

// Sleep for the specified number of microseconds.
void PCI7248IO::preciseSleep(int microseconds)
{
    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (!timer) return;

    LARGE_INTEGER dueTime;
    // Convert microseconds to 100-nanosecond intervals.
    dueTime.QuadPart = -static_cast<LONGLONG>(microseconds) * 10LL;
    SetWaitableTimer(timer, &dueTime, 0, NULL, NULL, FALSE);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}
