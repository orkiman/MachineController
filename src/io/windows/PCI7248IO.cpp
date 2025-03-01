#include "io/PCI7248IO.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std::chrono;

PCI7248IO::PCI7248IO(EventQueue<IOEvent>* eventQueue, const Config& config)
    : eventQueue_(eventQueue), config_(config), card_(-1)
{
    // Copy IO channel settings from configuration.
    inputChannels_ = config_.getInputs();    // active input channels from settings.json
    // outputChannels_ = config_.getOutputs();    // active output channels

    // Print configured channels.
    for (const auto& ch : inputChannels_) {
        std::cout << "Configured input: " << ch.name << " (port " 
                  << ch.ioPort << ", pin " << ch.pin << ")" << std::endl;
    }
    for (const auto& ch : outputChannels_) {
        std::cout << "Configured output: " << ch.name << " (port " 
                  << ch.ioPort << ", pin " << ch.pin << ")" << std::endl;
    }
}

PCI7248IO::~PCI7248IO() {
    stopPolling_ = true;
    if (pollingThread_.joinable()) {
        pollingThread_.join();
    }
    if (card_ >= 0) {
        Release_Card(card_);
    }
}

bool PCI7248IO::initialize() {
    // Register the PCI-7248 card.
    card_ = Register_Card(PCI_7248, 0);
    if (card_ < 0) {
        std::cerr << "Failed to register PCI-7248! Error Code: " << card_ << std::endl;
        return false;
    }
    
    // Retrieve port configuration from the config.
    auto portsConfig = config_.getPci7248IoPortsConfiguration();

    // Configure each port based on the configuration.
    if (portsConfig.find("A") != portsConfig.end()) {
        int type = (portsConfig["A"] == "output") ? OUTPUT_PORT : INPUT_PORT;
        if (DIO_PortConfig(card_, Channel_P1A, type) != 0) {
            std::cerr << "Failed to configure Port A as " << portsConfig["A"] << "." << std::endl;
            return false;
        }
    } else {
        std::cerr << "No configuration found for Port A." << std::endl;
        return false;
    }
    
    if (portsConfig.find("B") != portsConfig.end()) {
        int type = (portsConfig["B"] == "output") ? OUTPUT_PORT : INPUT_PORT;
        if (DIO_PortConfig(card_, Channel_P1B, type) != 0) {
            std::cerr << "Failed to configure Port B as " << portsConfig["B"] << "." << std::endl;
            return false;
        }
    } else {
        std::cerr << "No configuration found for Port B." << std::endl;
        return false;
    }
    
    if (portsConfig.find("CL") != portsConfig.end()) {
        int type = (portsConfig["CL"] == "output") ? OUTPUT_PORT : INPUT_PORT;
        if (DIO_PortConfig(card_, Channel_P1CL, type) != 0) {
            std::cerr << "Failed to configure Port CL as " << portsConfig["CL"] << "." << std::endl;
            return false;
        }
    } else {
        std::cerr << "No configuration found for Port CL." << std::endl;
        return false;
    }
    
    if (portsConfig.find("CH") != portsConfig.end()) {
        int type = (portsConfig["CH"] == "output") ? OUTPUT_PORT : INPUT_PORT;
        if (DIO_PortConfig(card_, Channel_P1CH, type) != 0) {
            std::cerr << "Failed to configure Port CH as " << portsConfig["CH"] << "." << std::endl;
            return false;
        }
    } else {
        std::cerr << "No configuration found for Port CH." << std::endl;
        return false;
    }
    
    std::cout << "PCI7248IO initialized successfully." << std::endl;
    
    // Read initial input state for each active input channel dynamically.
    std::unordered_map<std::string, int> portToChannel = {
        {"A", Channel_P1A},
        {"B", Channel_P1B},
        {"CL", Channel_P1CL},
        {"CH", Channel_P1CH}
    };

    for (const auto &portEntry : portsConfig) {
        const std::string &port = portEntry.first;
        const std::string &portType = portEntry.second;
        if (portType != "input")
            continue; // Only process input ports

        auto chIt = portToChannel.find(port);
        if (chIt == portToChannel.end()) {
            std::cerr << "Port " << port << " is not recognized for initial state read." << std::endl;
            continue;
        }
        int daskChannel = chIt->second;
        int baseOffset = getPortBaseOffset(port);

        U32 portValue = 0;
        if (DI_ReadPort(card_, daskChannel, &portValue) == 0) {
            for (auto &channel : inputChannels_) {
                if (channel.ioPort == port) {
                    int state = (portValue >> (channel.pin - baseOffset)) & 0x1;
                    channel.state = state;                    
                }
            }
        } else {
            std::cerr << "Failed to read initial state from Port " << port << "." << std::endl;
        }
    }
    // set inputChannels ioport field by pin number
    for (auto &channel : inputChannels_) {
        if (channel.pin < 8) {
            channel.ioPort = "A";
        } else if (channel.pin < 16) {
            channel.ioPort = "B";
        } else if (channel.pin < 20) {
            channel.ioPort = "CL";
        } else {
            channel.ioPort = "CH";
        }
    }

    // construct the outputChannels_ vector
    outputChannels_ = config_.getOutputs();
    // set outputChannels ioport field by pin number
    for (auto &ch : outputChannels_) {
        if (ch.pin < 8) {
            ch.ioPort = "A";
        } else if (ch.pin < 16) {
            ch.ioPort = "B";
        } else if (ch.pin < 20) {
            ch.ioPort = "CL";
        } else {
            ch.ioPort = "CH";
        }
    }

    
    // Start the polling thread to continuously update inputs.
    stopPolling_ = false;
    pollingThread_ = std::thread(&PCI7248IO::pollLoop, this);
    
    return true;
}

//get a pointer to the output channels
const std::vector<IOChannel>& PCI7248IO::getOutputChannels() const {
    return outputChannels_;
}

void PCI7248IO::pollLoop() {
    // Use the stored ports configuration.
    auto portsConfig = config_.getPci7248IoPortsConfiguration();

    std::unordered_map<std::string, int> portToChannel = {
        {"A",  Channel_P1A},
        {"B",  Channel_P1B},
        {"CL", Channel_P1CL},
        {"CH", Channel_P1CH}
    };

    // Statistics variables
    using clock = std::chrono::steady_clock;
    auto statStart = clock::now();
    long long totalTime = 0;
    long long minTime = std::numeric_limits<long long>::max();
    long long maxTime = 0;
    size_t iterations = 0;
    auto longRuns = 0;

    while (!stopPolling_) {
        auto loopStart = clock::now();

        bool anyChange = false;
        for (const auto &portEntry : portsConfig) {
            const std::string &port = portEntry.first;
            const std::string &portType = portEntry.second;
            if (portType != "input")
                continue;
            
            auto channelIt = portToChannel.find(port);
            if (channelIt == portToChannel.end()) {
                std::cerr << "Port " << port << " is not recognized." << std::endl;
                continue;
            }
            int channelConstant = channelIt->second;
            U32 portValue = 0;
            if (DI_ReadPort(card_, channelConstant, &portValue) == 0) {
                int baseOffset = getPortBaseOffset(port);
                for (auto &channel : inputChannels_) {
                    if (channel.ioPort == port) {
                        int newState = (portValue >> (channel.pin - baseOffset)) & 0x1;
                        if (newState != channel.state) {
                            if (channel.state == 0 && newState == 1) {
                                channel.eventType = IOEventType::Rising;
                            } else if (channel.state == 1 && newState == 0) {
                                channel.eventType = IOEventType::Falling;
                            } else {
                                channel.eventType = IOEventType::None;
                            }
                            anyChange = true;
                            channel.state = newState;
                        }
                    }
                }
            } else {
                std::cerr << "Failed to read Port " << port << " in pollLoop." << std::endl;
            }
        }
        if (anyChange) {
            pushStateEvent();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        // Measure the time taken for this loop iteration
        auto loopEnd = clock::now();
        auto execTime = std::chrono::duration_cast<std::chrono::microseconds>(loopEnd - loopStart).count();
        totalTime += execTime;
        minTime = std::min(minTime, execTime);
        maxTime = std::max(maxTime, execTime);
        if(execTime > 2000) {
            longRuns++;
        }
        iterations++;

        // Every second, print the statistics
        auto now = clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - statStart).count() >= 1) {
            double avgTime = static_cast<double>(totalTime) / iterations;
            std::cout << "Poll Loop Execution Time (microseconds) -- Min: " 
                      << minTime << ", Max: " << maxTime 
                      << ", Avg: " << avgTime << ", long runs: "<< longRuns<< std::endl;
            // Reset the stats for the next interval
            statStart = now;
            totalTime = 0;
            minTime = std::numeric_limits<long long>::max();
            maxTime = 0;
            iterations = 0;
            longRuns = 0;
        }
    }
}

// original loop - without time statistics:
// void PCI7248IO::pollLoop() {
//     // Use the stored ports configuration.
//     auto portsConfig = config_.getPci7248IoPortsConfiguration();

//     std::unordered_map<std::string, int> portToChannel = {
//         {"A", Channel_P1A},
//         {"B", Channel_P1B},
//         {"CL", Channel_P1CL},
//         {"CH", Channel_P1CH}
//     };

//     while (!stopPolling_) {
//         bool anyChange = false;
//         for (const auto& portEntry : portsConfig) {
//             const std::string& port = portEntry.first;
//             const std::string& portType = portEntry.second;
//             if (portType != "input")
//                 continue;
            
//             auto channelIt = portToChannel.find(port);
//             if (channelIt == portToChannel.end()) {
//                 std::cerr << "Port " << port << " is not recognized." << std::endl;
//                 continue;
//             }
//             int channelConstant = channelIt->second;
//             U32 portValue = 0;
//             if (DI_ReadPort(card_, channelConstant, &portValue) == 0) {
//                 int baseOffset = getPortBaseOffset(port);
//                 for (auto &channel : inputChannels_) {
//                     if (channel.ioPort == port) {
//                         int newState = (portValue >> (channel.pin - baseOffset)) & 0x1;
//                         if (newState != channel.state) {
//                             // Determine transition type.
//                             if (channel.state == 0 && newState == 1) {
//                                 channel.eventType = IOEventType::Rising;
//                             } else if (channel.state == 1 && newState == 0) {
//                                 channel.eventType = IOEventType::Falling;
//                             } else {
//                                 channel.eventType = IOEventType::None;
//                             }
//                             anyChange = true;
//                             // Update the stored state.
//                             channel.state = newState;
//                         }                        
//                     }
//                 }
//             } else {
//                 std::cerr << "Failed to read Port " << port << " in pollLoop." << std::endl;
//             }
//         }
//         if (anyChange) {
//             pushStateEvent();
//         }
//         std::this_thread::sleep_for(std::chrono::milliseconds(1));
//     }
// }

void PCI7248IO::pushStateEvent() {
    if (eventQueue_) {
        IOEvent event;
        event.channels = inputChannels_; // Copy the inputChannels vector.
        eventQueue_->push(event);
    }
}

std::vector<IOChannel> PCI7248IO::getInputChannelsSnapshot() const {
    return inputChannels_;
}



bool PCI7248IO::writeOutputs(const std::vector<IOChannel>& newOutputsState) {
    // Mapping from port identifier to DASK channel constant. eg. "A" -> Channel_P1A
    std::unordered_map<std::string, int> portToChannel = {
        {"A", Channel_P1A},
        {"B", Channel_P1B},
        {"CL", Channel_P1CL},
        {"CH", Channel_P1CH}
    };

    // Prepare an aggregated output value for each port.
    // All ports are initialized to 0 (channels not mentioned will be driven low).
    std::unordered_map<std::string, U32> portAggregates;
    for (const auto& entry : portToChannel) {
        portAggregates[entry.first] = 0;
    }

    // Process each channel in newOutputsState.
    for (const auto &channel : newOutputsState) {
        // Only process channels marked as output.
        if (channel.type != IOType::Output)
            continue;
        // Get the base offset for the port (e.g., 0 for "A", 8 for "B", etc.).
        int baseOffset = getPortBaseOffset(channel.ioPort);
        int bitPos = channel.pin - baseOffset;
        // If state is 1, set the corresponding bit in the aggregated value.
        if (channel.state != 0) {
            portAggregates[channel.ioPort] |= (1 << bitPos);
        }
        // If state is 0, do nothing since the aggregate is already initialized to 0.
    }

    bool overallSuccess = true;
    // Write each aggregated value to the corresponding port.
    for (const auto &p : portAggregates) {
        // std::cout << "Port: " << p.first << std::endl;
        const std::string &port = p.first;
        U32 aggregatedValue = p.second;
        auto it = portToChannel.find(port);
        if (it != portToChannel.end()) {
            int daskChannel = it->second;
            if (DO_WritePort(card_, daskChannel, aggregatedValue) != 0) {
                std::cerr << "Failed to write to Port " << port << std::endl;
                overallSuccess = false;
            }
        } else {
            std::cerr << "No DASK channel defined for Port " << port << std::endl;
            overallSuccess = false;
        }
    }

    return overallSuccess;
}


int PCI7248IO::getPortBaseOffset(const std::string &port) const {
    if (port == "A") return 0;
    if (port == "B") return 8;
    if (port == "CL") return 16;
    if (port == "CH") return 20;
    return 0;
}



void PCI7248IO::stopPolling() {
    stopPolling_ = true;
    if (pollingThread_.joinable()) {
        pollingThread_.join();
    }
}
