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
    
    // Get duration for a specific timer (in milliseconds)
    int getTimerDuration(const std::string& timerName) const;
    
    // Ensures default communication settings exist in the config
    void ensureDefaultCommunicationSettings();
    
    // Ensures default timer settings exist in the config
    void ensureDefaultTimerSettings();
    
    // Updates communication settings with new values
    void updateCommunicationSettings(const nlohmann::json& commSettings);
    
    // Updates timer settings with new values
    void updateTimerSettings(const nlohmann::json& timerSettings);
    
    // Saves the current configuration to a file
    bool saveToFile(const std::string& filePath = "") const;

private:
    nlohmann::json configJson_;
    mutable std::mutex configMutex_; // Mutex to protect concurrent access
    std::string filePath_; // Store the original file path
};

#endif // CONFIG_H
