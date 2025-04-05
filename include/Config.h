// Config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <unordered_map>
#include <mutex>
#include "json.hpp"  // Include the nlohmann JSON header
#include "io/IOChannel.h"  // This header should define IOChannel, IOType, and IOEventType
// #include "communication/RS232Communication.h" // Include RS232Communication.h

// Config class for the project settings.
class Config {
public:
    // Constructor that loads the configuration from a file.
    Config(const std::string& filePath);

    // Accessors for different sections.
    std::string getIODevice() const;
    std::unordered_map<std::string, std::string> getPci7248IoPortsConfiguration() const;
    std::unordered_map<std::string, IOChannel> getInputs() const;
    std::unordered_map<std::string, IOChannel> getOutputs() const;
    bool isPci7248ConfigurationValid() const;
    
    // Other getters for communication and timers.
    nlohmann::json getCommunicationSettings() const;
    nlohmann::json getTimerSettings() const;
    
    // Ensures default communication settings exist in the config
    void ensureDefaultCommunicationSettings();

private:
    nlohmann::json configJson_;
    mutable std::mutex configMutex_; // Mutex to protect concurrent access
};

#endif // CONFIG_H
