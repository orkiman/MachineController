// PCI7248IO.cpp
#include "io/PCI7248IO.h"
#include "Logger.h"
#include <thread>
#include <unordered_map>

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
    if (card_ >= 0)
        Release_Card(card_);
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

    // Configure ports
    for (const auto &[port, config] : portsConfig_) {
        int type = (config == "output") ? OUTPUT_PORT : INPUT_PORT;
        if (DIO_PortConfig(card_, portToChannel.at(port), type) != 0) {
            getLogger()->error("Failed to configure Port {} as {}", port, config);
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

    pollingThread_ = std::thread(&PCI7248IO::pollLoop, this);
    return true;
}

void PCI7248IO::assignPortNames(std::unordered_map<std::string, IOChannel> &channels)
{
    for (auto &[_, channel] : channels) {
        if (channel.pin < 8) channel.ioPort = "A";
        else if (channel.pin < 16) channel.ioPort = "B";
        else if (channel.pin < 20) channel.ioPort = "CL";
        else channel.ioPort = "CH";
    }
}

void PCI7248IO::readInitialInputStates(const std::unordered_map<std::string, int> &portToChannel)
{
    for (const auto &[port, type] : portsConfig_) {
        if (type != "input") continue;
        U32 portValue = 0;
        if (DI_ReadPort(card_, portToChannel.at(port), &portValue) != 0) {
            getLogger()->error("Failed to read initial state from Port {}", port);
            continue;
        }
        portValue = ~portValue;
        int baseOffset = getPortBaseOffset(port);
        for (auto &[_, channel] : inputChannels_) {
            if (channel.ioPort == port)
                channel.state = (portValue >> (channel.pin - baseOffset)) & 0x1;
        }
    }
}

void PCI7248IO::logConfiguredChannels()
{
    for (const auto &[_, ch] : inputChannels_)
        getLogger()->info("Configured input: {} (port {}, pin {})", ch.name, ch.ioPort, ch.pin);

    for (const auto &[_, ch] : outputChannels_)
        getLogger()->info("Configured output: {} (port {}, pin {})", ch.name, ch.ioPort, ch.pin);
}

void PCI7248IO::pollLoop()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    getLogger()->debug("THREAD_PRIORITY_TIME_CRITICAL");

    static const std::unordered_map<std::string, int> portToChannel = {
        {"A", Channel_P1A}, {"B", Channel_P1B}, {"CL", Channel_P1CL}, {"CH", Channel_P1CH}
    };

    while (!stopPolling_) {
        bool anyChange = updateInputStates(portToChannel);
        if (anyChange) pushStateEvent();
        preciseSleep(1000);
    }
    getLogger()->flush();
}

bool PCI7248IO::updateInputStates(const std::unordered_map<std::string, int> &portToChannel)
{
    bool anyChange = false;
    for (const auto &[port, type] : portsConfig_) {
        if (type != "input") continue;
        U32 portValue = 0;
        if (DI_ReadPort(card_, portToChannel.at(port), &portValue) != 0) {
            getLogger()->error("Failed to read Port {}", port);
            continue;
        }
        portValue = ~portValue;
        int baseOffset = getPortBaseOffset(port);
        for (auto &[_, channel] : inputChannels_) {
            if (channel.ioPort != port) continue;
            int newState = (portValue >> (channel.pin - baseOffset)) & 0x1;
            if (channel.state != newState) {
                channel.eventType = (channel.state == 0 && newState == 1) ? IOEventType::Rising : IOEventType::Falling;
                channel.state = newState;
                anyChange = true;
            } else {
                channel.eventType = IOEventType::None;
            }
        }
    }
    return anyChange;
}

void PCI7248IO::pushStateEvent()
{
    if (eventQueue_)
        eventQueue_->push(IOEvent{inputChannels_});
}

bool PCI7248IO::resetConfiguredOutputPorts()
{
    return writeOutputs({});
}

bool PCI7248IO::writeOutputs(const std::unordered_map<std::string, IOChannel> &newOutputsState)
{
    std::unordered_map<std::string, U32> portAggregates;
    aggregateOutputs(newOutputsState, portAggregates);

    bool success = true;
    for (const auto &[port, value] : portAggregates) {
        if (portsConfig_[port] != "output") continue;
        int channel = getDaskChannel(port);
        if (DO_WritePort(card_, channel, (~value & 0xFF)) != 0) {
            getLogger()->error("Failed to write Port {}", port);
            success = false;
        }
    }
    return success;
}

void PCI7248IO::aggregateOutputs(const std::unordered_map<std::string, IOChannel> &channels, std::unordered_map<std::string, U32> &aggregates)
{
    for (const auto &[_, ch] : channels) {
        if (ch.type != IOType::Output) continue;
        aggregates[ch.ioPort] |= (ch.state ? 1 : 0) << (ch.pin - getPortBaseOffset(ch.ioPort));
    }
}

int PCI7248IO::getDaskChannel(const std::string &port) const {
    static const std::unordered_map<std::string, int> channels = {
        {"A", Channel_P1A}, {"B", Channel_P1B}, {"CL", Channel_P1CL}, {"CH", Channel_P1CH}
    };
    return channels.at(port);
}
