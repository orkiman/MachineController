#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <unordered_map>
#include <unordered_map>
#include "json.hpp"  // Include the nlohmann JSON header
#include "io/IOChannel.h"  // This header should define IOChannel, IOType, and IOEventType

// Config class for the project settings.
class Config {
public:
    // Constructor that loads the configuration from a file.
    Config(const std::string& filePath);

    // Accessors for different sections.
    std::string getIODevice() const;
    std::unordered_map<std::string, std::string> getPci7248IoPortsConfiguration() const;
    const std::unordered_map<std::string, IOChannel>& getInputs() const;
    const std::unordered_map<std::string, IOChannel>& getOutputs() const;
    
    // Other getters for communication and timers.
    nlohmann::json getCommunicationSettings() const;
    nlohmann::json getTimerSettings() const;

private:
    nlohmann::json configJson_;
};

#endif // CONFIG_H
